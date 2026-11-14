#include "WeatherActivity.h"

#include <FontCacheManager.h>
#include <HalDisplay.h>
#include <I18n.h>
#include <Memory.h>
#include <TrustedTime.h>
#include <WiFi.h>
#ifdef SIMULATOR
#include <SimTransferInput.h>
#endif
#include <algorithm>
#include <cstdio>

#include "ManualSleep.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/UiScreenHeader.h"
#include "components/WeatherPresentation.h"
#include "fontIds.h"
namespace fui = freeink::ui;
void WeatherActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  {
    RenderLock lock(*this);
    store = makeUniqueNoThrow<weather::Store>();
    if (!store) {
      LOG_ERR("WEATHER", "OOM: weather store (%u bytes)", unsigned(sizeof(weather::Store)));
      notice = tr(STR_WEATHER_MEMORY);
    } else {
      store->load();
      if (!store->available()) notice = tr(STR_WEATHER_CACHE_ERROR);
    }
    selection = store && store->model().configured ? 0 : 1;
    updateText();
  }
  for (auto id : {Back, Refresh, Location, Units, Pick, Cancel}) app.on(id, action, this);
  app.setScreen(screenFn, this);
  requestUpdate();
}
void WeatherActivity::stopNetwork() {
  if (networkUsed) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    networkUsed = false;
    LOG_DBG("WEATHER", "Wi-Fi stopped");
  }
}
void WeatherActivity::onExit() {
  closeRouting();
  stopNetwork();
  store.reset();
  Activity::onExit();
}
bool WeatherActivity::preventAutoSleep() { return mode == Mode::Transferring; }
void WeatherActivity::screenFn(UiScreen& screen, void* context) {
  static_cast<WeatherActivity*>(context)->buildScreen(screen);
}
void WeatherActivity::action(const fui::ActionEvent& event, void* context) {
  static_cast<WeatherActivity*>(context)->activate(event.action, event.value);
}
void WeatherActivity::updateText() {
  now = std::max<int64_t>(0, trustedtime::trustedNow());
  title[0] = 0;
  if (store && store->available() && store->model().configured) {
    const auto text = renderer.truncatedText(
        UI_12_FONT_ID, store->model().location.name,
        UITheme::getContentArea(renderer).width - 2 * UITheme::getInstance().getMetrics().contentSidePadding,
        EpdFontFamily::BOLD);
    snprintf(title, sizeof(title), "%s", text.c_str());
  }
  for (auto& line : noticeLines) line[0] = 0;
  shortNotice[0] = 0;
  const char* message = notice;
  if (!message && (!store || !store->model().cached))
    message = store && store->model().configured ? tr(STR_WEATHER_REFRESH_HINT) : tr(STR_WEATHER_LOCATION_HINT);
  if (message) {
    const int width =
        UITheme::getContentArea(renderer).width - 2 * UITheme::getInstance().getMetrics().contentSidePadding;
    const auto lines = renderer.wrappedText(UI_10_FONT_ID, message, width, 3);
    for (size_t i = 0; i < lines.size() && i < 3; ++i)
      snprintf(noticeLines[i], sizeof(noticeLines[i]), "%s", lines[i].c_str());
    const auto shortText = renderer.truncatedText(SMALL_FONT_ID, message, width);
    snprintf(shortNotice, sizeof(shortNotice), "%s", shortText.c_str());
  }
  for (unsigned i = 0; i < placeCount; ++i) {
    snprintf(subtitles[i], sizeof(subtitles[i]), places[i].region[0] ? "%s · %s" : "%s%s", places[i].region,
             places[i].country);
    rows[i].label = places[i].name;
    rows[i].subtitle = subtitles[i];
    rows[i].actionValue = i;
  }
}
void WeatherActivity::openLocation() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_WEATHER_CITY), query, 64);
  if (!keyboard) {
    LOG_ERR("WEATHER", "OOM: location keyboard (%u bytes)", unsigned(sizeof(KeyboardEntryActivity)));
    RenderLock lock(*this);
    notice = tr(STR_WEATHER_MEMORY);
    updateText();
    requestUpdate();
    return;
  }
  closeRouting();
  app.clearTapFlash();
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      requestUpdate();
      return;
    }
    const auto* entry = std::get_if<KeyboardResult>(&result.data);
    if (!entry) return;
    if (!weather::validQuery(entry->text)) {
      RenderLock lock(*this);
      notice = tr(STR_WEATHER_QUERY_INVALID);
      updateText();
      requestUpdate();
      return;
    }
    {
      RenderLock lock(*this);
      snprintf(query, sizeof(query), "%s", entry->text.c_str());
    }
    beginNetwork(Purpose::Search);
  });
}
void WeatherActivity::beginNetwork(Purpose operation) {
  if (!store || !store->canSave()) return;
  auto wifi = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput);
  if (!wifi) {
    LOG_ERR("WEATHER", "OOM: Wi-Fi picker (%u bytes)", unsigned(sizeof(WifiSelectionActivity)));
    RenderLock lock(*this);
    notice = tr(STR_WEATHER_MEMORY);
    updateText();
    requestUpdate();
    return;
  }
  {
    RenderLock lock(*this);
    purpose = operation;
    mode = Mode::SelectingWifi;
    notice = nullptr;
    closeRouting();
    app.clearTapFlash();
    networkUsed = true;
  }
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::move(wifi), [this](const ActivityResult& result) {
    const auto* wifi = std::get_if<WifiResult>(&result.data);
    if (result.isCancelled || !wifi || !wifi->connected) {
      stopNetwork();
      RenderLock lock(*this);
      mode = Mode::Overview;
      notice = tr(STR_WEATHER_CANCELLED);
      updateText();
      closeRouting();
      requestUpdate();
      return;
    }
    RenderLock lock(*this);
    mode = Mode::Ready;
    closeRouting();
    requestUpdate();
  });
}
void WeatherActivity::activate(fui::ActionId id, int value) {
  app.clearTapFlash();
  if (mode == Mode::Transferring) {
    if (id == Back || id == Cancel) cancelRequested = true;
    return;
  }
  if (mode == Mode::SelectingWifi || mode == Mode::Ready || mode == Mode::AwaitingSleep) return;
  if (id == Back) {
    if (mode == Mode::Results) {
      RenderLock lock(*this);
      mode = Mode::Overview;
      selection = 1;
      notice = nullptr;
      updateText();
      closeRouting();
      requestUpdate();
    } else
      activityManager.goHome(HomeMenuItem::WEATHER);
    return;
  }
  if (!store || !store->canSave()) return;
  if (mode == Mode::Results) {
    if (id != Pick || value < 0 || unsigned(value) >= placeCount) return;
    RenderLock lock(*this);
    const auto previous = store->model();
    store->model().location = places[value];
    store->model().configured = true;
    store->model().cached = false;
    if (store->save()) {
      mode = Mode::Overview;
      selection = 0;
      notice = tr(STR_WEATHER_REFRESH_HINT);
    } else {
      store->model() = previous;
      notice = tr(STR_WEATHER_SAVE_ERROR);
    }
    updateText();
    closeRouting();
    requestUpdate();
    return;
  }
  if (id == Location) {
    openLocation();
    return;
  }
  if (id == Refresh && store->model().configured) {
    beginNetwork(Purpose::Forecast);
    return;
  }
  if (id == Units) {
    RenderLock lock(*this);
    const bool old = store->model().fahrenheit;
    store->model().fahrenheit = !old;
    if (store->save())
      notice = nullptr;
    else {
      store->model().fahrenheit = old;
      notice = tr(STR_WEATHER_SAVE_ERROR);
    }
    updateText();
    closeRouting();
    requestUpdate();
  }
}
bool WeatherActivity::pollCancel() {
#ifdef SIMULATOR
  simBeginTransferFrame();
#endif
  mappedInput.update(true);
  if (requestManualSleep()) return true;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) || mappedInput.wasHomeGesture()) cancelRequested = true;
  routeTouch(mappedInput);
  return cancelRequested;
}
void WeatherActivity::transfer() {
  {
    RenderLock lock(*this);
    mode = Mode::Transferring;
    cancelRequested = false;
    closeRouting();
    if (auto* cache = renderer.getFontCacheManager()) cache->releaseSdFontCaches();
  }
  requestUpdateAndWait();
  weather::Forecast forecast;
  auto result = purpose == Purpose::Search
                    ? weather::search(query, places, placeCount, [this] { return pollCancel(); })
                    : weather::refresh(store->model().location, forecast, [this] { return pollCancel(); });
  stopNetwork();
  if (requestManualSleep()) {
    RenderLock lock(*this);
    mode = Mode::AwaitingSleep;
    closeRouting();
    return;
  }
  mappedInput.resetHomeButtonInput();
  if (result != weather::Fetch::Ok) LOG_ERR("WEATHER", "Weather request failed (%u)", unsigned(result));
  {
    RenderLock lock(*this);
    mode = Mode::Overview;
    notice = nullptr;
    switch (result) {
      case weather::Fetch::Ok:
        if (purpose == Purpose::Search) {
          if (placeCount) {
            mode = Mode::Results;
            selection = 0;
          } else
            notice = tr(STR_WEATHER_NO_RESULTS);
        } else {
          const auto previous = store->model().forecast;
          const bool cached = store->model().cached;
          store->model().forecast = forecast;
          store->model().cached = true;
          if (!store->save()) {
            store->model().forecast = previous;
            store->model().cached = cached;
            notice = tr(STR_WEATHER_SAVE_ERROR);
          }
        }
        break;
      case weather::Fetch::Cancelled:
        notice = tr(STR_WEATHER_CANCELLED);
        break;
      case weather::Fetch::Network:
        notice = tr(STR_WEATHER_NETWORK_ERROR);
        break;
      case weather::Fetch::InvalidData:
        notice = tr(STR_WEATHER_DATA_ERROR);
        break;
      case weather::Fetch::OutOfMemory:
        notice = tr(STR_WEATHER_MEMORY);
        break;
    }
    updateText();
    closeRouting();
  }
  LOG_DBG("WEATHER", "Transfer finished (request %u)", unsigned(result));
  requestUpdate();
}
void WeatherActivity::moveSelection(int delta) {
  RenderLock lock(*this);
  const int count = mode == Mode::Results ? int(placeCount) : 3;
  if (!count) return;
  for (int tries = 0; tries < count; ++tries) {
    selection =
        delta > 0 ? ButtonNavigator::nextIndex(selection, count) : ButtonNavigator::previousIndex(selection, count);
    if (mode == Mode::Results || selection != 0 || (store && store->model().configured)) break;
  }
  requestUpdate();
}
void WeatherActivity::loop() {
  if (mode == Mode::Ready) {
    transfer();
    return;
  }
  if (mode == Mode::SelectingWifi || mode == Mode::Transferring || mode == Mode::AwaitingSleep) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activate(Back);
    return;
  }
  const auto touch = routeTouch(mappedInput);
  if (touch.event) return;
  if (touch.routed && app.invalidated()) requestUpdate();
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (mode == Mode::Results)
      activate(Pick, selection);
    else {
      static constexpr Action ITEMS[] = {Refresh, Location, Units};
      activate(ITEMS[selection]);
    }
    return;
  }
  ButtonNavigator::onNextPress([this] { moveSelection(1); });
  ButtonNavigator::onPreviousPress([this] { moveSelection(-1); });
}
void WeatherActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
void WeatherActivity::drawForecast() {
  const auto& m = UITheme::getInstance().getMetrics();
  const auto content = UITheme::getContentArea(renderer);
  const int x = content.x + m.contentSidePadding, w = content.width - 2 * m.contentSidePadding;
  const auto& data = store->model();
  const auto& f = data.forecast;
  const int small = renderer.getLineHeight(SMALL_FONT_ID), body = renderer.getLineHeight(UI_10_FONT_ID),
            titleH = renderer.getLineHeight(UI_12_FONT_ID);
  int y = m.topPadding + m.headerHeight + m.verticalSpacing;
  renderer.drawText(UI_12_FONT_ID, x, y, title, true, EpdFontFamily::BOLD);
  y += titleH + 4;
  char text[96];
  unsigned year, month, dom;
  weather::civilDate(f.currentDate, year, month, dom);
  snprintf(text, sizeof(text), tr(STR_WEATHER_DATE), year, month, dom);
  renderer.drawText(SMALL_FONT_ID, x, y, text, true);
  y += small + m.verticalSpacing;
  const int currentX = x + w / 4 + m.verticalSpacing, humidityX = x + w * 2 / 3;
  constexpr int ICON_SIZE = 64;
  const int valueFont = NOTOSANS_14_FONT_ID;
  const int valueY = y + (ICON_SIZE - renderer.getLineHeight(valueFont)) / 2;
  renderer.drawIcon(weatherui::icon(f.code, f.isDay, true), x, y, ICON_SIZE);
  weatherui::temperature(text, sizeof(text), f.temperature, data.fahrenheit, true);
  renderer.drawText(valueFont, currentX, valueY, text, true, EpdFontFamily::BOLD);
  weatherui::percent(text, sizeof(text), f.humidity);
  renderer.drawText(valueFont, humidityX, valueY, text, true, EpdFontFamily::BOLD);
  y += std::max(ICON_SIZE, renderer.getLineHeight(valueFont)) + 8;
  renderer.drawText(SMALL_FONT_ID, currentX, y, weatherui::condition(f.code), true);
  renderer.drawText(SMALL_FONT_ID, humidityX, y, tr(STR_WEATHER_HUMIDITY), true);
  y += body + m.verticalSpacing;
  renderer.drawLine(x, y, x + w - 1, y, true);
  y += m.verticalSpacing;
  renderer.drawText(UI_10_FONT_ID, x, y, tr(STR_WEATHER_PRECIPITATION), true, EpdFontFamily::BOLD);
  y += body + m.verticalSpacing;
  weatherui::percent(text, sizeof(text), f.days[0].chance);
  renderer.drawText(UI_12_FONT_ID, x, y, text, true, EpdFontFamily::BOLD);
  weatherui::precipitation(text, sizeof(text), f.days[0].rain, data.fahrenheit);
  renderer.drawText(UI_12_FONT_ID, x + w / 2 + 24, y, text, true, EpdFontFamily::BOLD);
  y += titleH + 8;
  renderer.drawText(SMALL_FONT_ID, x, y, tr(STR_WEATHER_CHANCE), true);
  renderer.drawText(SMALL_FONT_ID, x + w / 2 + 24, y, tr(STR_WEATHER_AMOUNT), true);
  y += small + 24;
  renderer.drawText(UI_10_FONT_ID, x, y, tr(STR_WEATHER_FORECAST), true, EpdFontFamily::BOLD);
  y += body + 4;
  renderer.drawLine(x, y, x + w - 1, y, true);
  y += 12;
  renderer.drawText(SMALL_FONT_ID, x, y, tr(STR_WEATHER_DAY), true);
  const int tempsX = x + w * 52 / 100, chanceX = x + w * 75 / 100, right = x + w;
  auto rightText = [&](int edge, int yy, const char* label, int font) {
    renderer.drawText(font, edge - renderer.getTextWidth(font, label), yy, label, true);
  };
  rightText(tempsX, y, tr(STR_WEATHER_HIGH_LOW), SMALL_FONT_ID);
  rightText(chanceX, y, tr(STR_WEATHER_CHANCE), SMALL_FONT_ID);
  rightText(right, y, tr(STR_WEATHER_AMOUNT), SMALL_FONT_ID);
  y += small + 12;
  for (unsigned i = 0; i < weather::DAYS; ++i) {
    const auto& d = f.days[i];
    weatherui::day(text, sizeof(text), d.date, f, now);
    renderer.drawText(UI_10_FONT_ID, x, y, text, true);
    weatherui::range(text, sizeof(text), d, data.fahrenheit);
    rightText(tempsX, y, text, UI_10_FONT_ID);
    weatherui::percent(text, sizeof(text), d.chance);
    rightText(chanceX, y, text, UI_10_FONT_ID);
    weatherui::precipitation(text, sizeof(text), d.rain, data.fahrenheit);
    rightText(right, y, text, UI_10_FONT_ID);
    y += std::max(44, body + 16);
  }
}
void WeatherActivity::buildScreen(UiScreen& screen) {
  const auto& m = UITheme::getInstance().getMetrics();
  const auto content = UITheme::getContentArea(renderer);
  const int x = content.x + m.contentSidePadding, w = content.width - 2 * m.contentSidePadding;
  const int small = renderer.getLineHeight(SMALL_FONT_ID), body = renderer.getLineHeight(UI_10_FONT_ID);
  const int top = m.topPadding + m.headerHeight + m.verticalSpacing;
  const int footer = m.verticalSpacing + (mappedInput.hasTouch() ? 0 : m.buttonHintsHeight);
  const int buttonH = std::max<int>(44, screen.theme().minTouchSize);
  const int controlsY = renderer.getScreenHeight() - footer - buttonH - 24;
  const bool editable = store && store->canSave();
  {
    fui::HeaderProps header;
    const auto band = prepareUiHeader(screen, renderer, header,
                                      mode == Mode::Results ? tr(STR_WEATHER_CHOOSE) : tr(STR_WEATHER), Back);
    if (mode == Mode::Overview) {
      header.trailingLabel = tr(STR_WEATHER_REFRESH);
      header.trailingText = screen.theme().smallText;
      header.trailingText.bold = true;
      header.trailingAction = Refresh;
      header.trailingEnabled = editable && store->model().configured;
      header.trailingStyles.normal.border = fui::Paint::solid(fui::Color::Black);
      header.trailingStyles.normal.borderWidth = 1;
      header.trailingRadius = screen.theme().listRowRadius;  // match the Change/Units buttons
      if (selection == 0) header.trailingStyles.normal.background = fui::Paint::dither(fui::Color::LightGray);
    }
    fui::header(screen.frame(), band, header);
  }
  if (mode == Mode::Results) {
    screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(top), 0, static_cast<int16_t>(footer), 0});
    fui::ListProps props;
    props.items = rows;
    props.count = placeCount;
    props.selectedIndex = selection;
    props.action = Pick;
    props.labelText = screen.theme().bodyText;
    props.labelText.maxLines = 2;
    props.subtitleText = screen.theme().smallText;
    props.subtitleText.maxLines = 1;
    props.rowHeight = 72;
    props.inputMask = fui::InputTouch;
    screen.list(props);
    renderer.drawText(SMALL_FONT_ID, x, renderer.getScreenHeight() - footer - small, tr(STR_WEATHER_GEO_CREDIT), true);
    return;
  }
  if (mode != Mode::Overview) {
    renderer.drawText(UI_12_FONT_ID, x, top + body,
                      purpose == Purpose::Search ? tr(STR_WEATHER_SEARCHING) : tr(STR_WEATHER_FETCHING), true,
                      EpdFontFamily::BOLD);
    fui::ButtonProps button;
    button.label = tr(STR_CANCEL);
    button.action = Cancel;
    screen.button(button, fui::Rect{static_cast<int16_t>(x), static_cast<int16_t>(controlsY), static_cast<int16_t>(w),
                                    static_cast<int16_t>(buttonH)});
    return;
  }
  if (store && store->available() && store->model().cached)
    drawForecast();
  else {
    renderer.drawText(UI_12_FONT_ID, x, top,
                      title[0]                      ? title
                      : store && store->available() ? tr(STR_WEATHER_CHOOSE)
                                                    : tr(STR_WEATHER_UNAVAILABLE),
                      true, EpdFontFamily::BOLD);
    for (int i = 0; i < 3; ++i)
      if (noticeLines[i][0])
        renderer.drawText(UI_10_FONT_ID, x, top + renderer.getLineHeight(UI_12_FONT_ID) + 24 + i * body, noticeLines[i],
                          true);
  }
  char stamp[96], attribution[128];
  if (store && store->available() && store->model().cached) {
    weatherui::updated(stamp, sizeof(stamp), store->model().forecast);
    snprintf(attribution, sizeof(attribution), tr(STR_WEATHER_ATTRIBUTED), stamp);
    renderer.drawText(SMALL_FONT_ID, x, controlsY - 2 * small - 8, attribution, true);
    const char* status = shortNotice[0]                                   ? shortNotice
                         : weatherui::stale(store->model().forecast, now) ? tr(STR_WEATHER_SAVED)
                                                                          : "";
    if (status[0]) renderer.drawText(SMALL_FONT_ID, x, controlsY - small - 4, status, true);
  } else
    renderer.drawText(SMALL_FONT_ID, x, controlsY - small - 8, tr(STR_WEATHER_PROVIDER), true);
  {
    fui::ButtonProps button;
    button.styles = themedSelectionStyles(screen.theme());
    for (auto* style :
         {&button.styles.normal, &button.styles.selected, &button.styles.focused, &button.styles.active}) {
      style->border = fui::Paint::solid(fui::Color::Black);
      style->borderWidth = 1;
    }
    button.text = screen.theme().smallText;
    button.text.bold = true;
    button.text.align = fui::TextAlign::Center;
    button.label = tr(STR_WEATHER_CHANGE);
    button.action = Location;
    button.enabled = editable;
    button.state = selection == 1 ? fui::StateSelected : fui::StateNormal;
    const int half = (w - m.verticalSpacing) / 2;
    screen.button(button, fui::Rect{static_cast<int16_t>(x), static_cast<int16_t>(controlsY),
                                    static_cast<int16_t>(half), static_cast<int16_t>(buttonH)});
    button.label = store && store->model().fahrenheit ? tr(STR_WEATHER_UNITS_F) : tr(STR_WEATHER_UNITS_C);
    button.action = Units;
    button.state = selection == 2 ? fui::StateSelected : fui::StateNormal;
    screen.button(button, fui::Rect{static_cast<int16_t>(x + w - half), static_cast<int16_t>(controlsY),
                                    static_cast<int16_t>(half), static_cast<int16_t>(buttonH)});
  }
}
