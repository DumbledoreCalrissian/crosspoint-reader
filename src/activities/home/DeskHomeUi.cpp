#include "DeskHomeUi.h"

#include <FreeInkUIGfxRenderer.h>
#include <FreeInkUIIcon.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <TodoStore.h>
#include <TrustedTime.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "HapticFeedback.h"
#include "HomeMenuOrder.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/WeatherPresentation.h"
#include "components/icons/clock.h"
#include "components/icons/listIcons.h"
#include "components/icons/pages.h"
#include "components/themes/desk/DeskTheme.h"
#include "fontIds.h"
#include "util/BookReadingSummary.h"
#include "util/ButtonNavigator.h"
#include "util/ReadingCalendarClock.h"
#include "util/ReadingCalendarStore.h"
#include "util/WeatherStore.h"

namespace {
// Book layout below the menu button row (portrait 480 px content width).
constexpr int CURRENT_W = 172;
constexpr int CURRENT_H = 258;
constexpr int SMALL_GAP = 16;  // between covers, and current to strip
constexpr int STRIP_PAGE = 2;  // small covers per strip page
constexpr int SMALL_H = 177;   // 2:3 of the strip's 118 px cover width
constexpr int SELECT_OUTSET = 5;
constexpr int SELECT_LINE = 3;
constexpr int TEXT_GAP = 8;  // cover to title, title to bar
constexpr int ROW_GAP = 8;   // between the current book's rows
constexpr int BAR_HEIGHT = 6;
constexpr int SMALL_BAR_HEIGHT = 5;
constexpr int STAT_ICON = 24;
constexpr int ICON_GAP = 4;
constexpr int STAT_MIN_GAP = 8;  // between the time and the pages-turned group
constexpr int DOT_SIZE = 8;
constexpr int TODO_ICON = 24;
constexpr int TODO_GAP = 4;

struct BookLayout {
  Rect current;
  int stripX;
  int smallW;
  int top;
};

BookLayout bookLayout(const GfxRenderer& renderer) {
  const auto& metrics = DeskMetrics::values;
  const Rect content = UITheme::getContentArea(renderer);
  const int left = content.x + metrics.contentSidePadding;
  const int right = content.x + content.width - metrics.contentSidePadding;
  const int top = metrics.homeTopPadding;
  const int stripX = left + CURRENT_W + SMALL_GAP;
  return BookLayout{Rect{left, top, CURRENT_W, CURRENT_H}, stripX,
                    (right - stripX - (STRIP_PAGE - 1) * SMALL_GAP) / STRIP_PAGE, top};
}

Rect smallRect(const BookLayout& layout, const int slot) {
  return Rect{layout.stripX + slot * (layout.smallW + SMALL_GAP), layout.top, layout.smallW, SMALL_H};
}

int stripBarY(const GfxRenderer& renderer, const BookLayout& layout) {
  return layout.top + SMALL_H + TEXT_GAP + 2 * renderer.getLineHeight(SMALL_FONT_ID) + 4;
}

// Left-aligned, wrapped and ellipsized; returns the height used.
int drawTitle(const GfxRenderer& renderer, const int fontId, const EpdFontFamily::Style style, const int x, const int y,
              const int width, const int maxLines, const std::string& title) {
  const int lineH = renderer.getLineHeight(fontId);
  const auto lines = renderer.wrappedText(fontId, title.c_str(), width, maxLines, style);
  for (size_t i = 0; i < lines.size(); ++i) {
    renderer.drawText(fontId, x, y + static_cast<int>(i) * lineH, lines[i].c_str(), true, style);
  }
  return static_cast<int>(lines.size()) * lineH;
}

// Percentage at the right end of a bar that fills the rest of the width.
void drawProgress(const GfxRenderer& renderer, const int fontId, const EpdFontFamily::Style style, const int x,
                  const int y, const int width, const int barHeight, const int percent, const bool percentFirst) {
  char text[8];
  snprintf(text, sizeof(text), "%d%%", std::clamp(percent, 0, 100));
  const int textW = renderer.getTextWidth(fontId, text, style);
  const int lineH = renderer.getLineHeight(fontId);
  const int barW = width - textW - 8;
  const int barX = percentFirst ? x + textW + 8 : x;
  const int barY = y + (lineH - barHeight) / 2;
  renderer.drawText(fontId, percentFirst ? x : x + width - textW, y, text, true, style);
  renderer.drawRect(barX, barY, barW, barHeight, true);
  const int filled = barW * std::clamp(percent, 0, 100) / 100;
  if (filled > 0) renderer.fillRect(barX, barY, filled, barHeight, true);
}

HomeMenuItem deskMenuItemAt(int index, HomeMenuItem slot) {
  int count = 0;
  while (homeMenuItemAt(count, slot) != HomeMenuItem::NONE) ++count;
  if (index == count) return HomeMenuItem::WEATHER;
  return homeMenuItemAt(index, slot);
}

const char* itemLabel(const HomeMenuItem item) {
  switch (item) {
    case HomeMenuItem::WEATHER:
      return tr(STR_WEATHER);
    case HomeMenuItem::FILE_BROWSER:
      return tr(STR_BROWSE_FILES);
    case HomeMenuItem::LIBRARY:
      return tr(STR_LIBRARY);
    case HomeMenuItem::PLUGINS:
      return tr(STR_PLUGINS);
    case HomeMenuItem::OPDS_BROWSER:
      return tr(STR_OPDS_BROWSER);
    case HomeMenuItem::SETTINGS_MENU:
      return tr(STR_SETTINGS_TITLE);
    case HomeMenuItem::TODO:
      return tr(STR_TODO);
    case HomeMenuItem::POMODORO:
      return tr(STR_POMODORO);
    case HomeMenuItem::SUDOKU:
      return tr(STR_SUDOKU);
    default:
      return "";
  }
}

UIIcon itemIcon(const HomeMenuItem item) {
  switch (item) {
    case HomeMenuItem::FILE_BROWSER:
      return Folder;
    case HomeMenuItem::LIBRARY:
      return Library;
    case HomeMenuItem::PLUGINS:
      return Plugins;
    case HomeMenuItem::OPDS_BROWSER:
      return Book;
    case HomeMenuItem::SETTINGS_MENU:
      return Settings;
    case HomeMenuItem::TODO:
      return Todo;
    case HomeMenuItem::POMODORO:
      return Pomodoro;
    case HomeMenuItem::SUDOKU:
      return Sudoku;
    default:
      return None;
  }
}

bool contains(const Rect& r, const int x, const int y) {
  return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}
}  // namespace

void DeskHomeUi::begin(const HomeMenuItem slotItem, const std::vector<RecentBook>* recentBooks,
                       const BookReadingSummary* readingSummaries) {
  slot = slotItem;
  books = recentBooks;
  summaries = readingSummaries;
  stripFirst = 1;
  if (bookCount() > 0) coverCache.begin();
  open = false;
  selected = 0;
  chosenItem = HomeMenuItem::NONE;
  loadTodo();
  loadCalendar();
  loadWeather();
}

Rect DeskHomeUi::todoRect() const {
  const auto layout = bookLayout(renderer);
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int rowH = std::max(lineH, TODO_ICON);
  return Rect{layout.stripX, stripBarY(renderer, layout) + lineH + 12 + DOT_SIZE + TEXT_GAP,
              STRIP_PAGE * layout.smallW + (STRIP_PAGE - 1) * SMALL_GAP,
              renderer.getLineHeight(UI_10_FONT_ID) + TODO_GAP + 1 + TODO_GAP + static_cast<int>(TODO_ROWS) * rowH};
}

void DeskHomeUi::loadTodo() {
  todoCount = 0;
  auto store = makeUniqueNoThrow<TodoStore>();
  if (!store) {
    LOG_ERR("HOME", "OOM: Todo snapshot (%u bytes)", static_cast<unsigned>(sizeof(TodoStore)));
    return;
  }
  // load() only reads snapshots; Todo owns edits and persistence.
  if (store->load() != TodoStore::Status::Ok) {
    LOG_ERR("HOME", "Todo snapshot load failed; widget hidden");
    return;
  }
  const int textWidth = todoRect().width - TODO_ICON - TODO_GAP;
  for (size_t i = 0; i < store->model().size() && todoCount < TODO_ROWS; ++i) {
    const auto& task = *store->model().at(i);
    if (task.completed) continue;
    const auto text = renderer.truncatedText(SMALL_FONT_ID, task.text, textWidth);
    snprintf(todoTexts[todoCount], sizeof(todoTexts[todoCount]), "%s", text.c_str());
    ++todoCount;
  }
}

bool DeskHomeUi::todoTapped() const {
  if (!todoCount) return false;
  const auto rect = todoRect();
  int row = -1;
  return input.rowTouch(row, rect.y, rect.height, 1, rect.x, rect.x + rect.width, rect.height) ==
         MappedInputManager::RowTouch::Tap;
}

void DeskHomeUi::drawTodo() const {
  if (!todoCount) return;
  const auto rect = todoRect();
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int rowH = std::max(lineH, TODO_ICON);
  renderer.drawText(UI_10_FONT_ID, rect.x, rect.y, tr(STR_TODO), true, EpdFontFamily::BOLD);
  const int ruleY = rect.y + renderer.getLineHeight(UI_10_FONT_ID) + TODO_GAP;
  renderer.drawLine(rect.x, ruleY, rect.x + rect.width - 1, ruleY, true);
  freeink::ui::GfxRendererTarget target(renderer);
  for (size_t i = 0; i < todoCount; ++i) {
    const int y = ruleY + 1 + TODO_GAP + static_cast<int>(i) * rowH;
    target.bitmap(freeink::ui::Rect{static_cast<int16_t>(rect.x), static_cast<int16_t>(y + (rowH - TODO_ICON) / 2),
                                    TODO_ICON, TODO_ICON},
                  freeink::ui::bitmapFromIcon(icon_checkbox_off_24), freeink::ui::BitmapMode::Contain);
    renderer.drawText(SMALL_FONT_ID, rect.x + TODO_ICON + TODO_GAP, y + (rowH - lineH) / 2, todoTexts[i], true);
  }
}

int DeskHomeUi::itemCount() const {
  int count = 0;
  while (deskMenuItemAt(count, slot) != HomeMenuItem::NONE) ++count;
  return count;
}

void DeskHomeUi::openMenu() {
  open = true;
  selected = 0;
}

void DeskHomeUi::close() {
  open = false;
  selected = 0;
}

// Buttons and swipes move the selection, Confirm or a row tap opens the item,
// Back or a tap outside the panel closes it.
DeskHomeUi::Request DeskHomeUi::loopMenu(ButtonNavigator& navigator) {
  const int count = itemCount();
  Request request = Request::None;
  const auto step = [this, count, &request](const bool next) {
    selected = next ? ButtonNavigator::nextIndex(selected, count) : ButtonNavigator::previousIndex(selected, count);
    request = Request::Redraw;
  };
  const auto choose = [this](const int index) {
    chosenItem = deskMenuItemAt(index, slot);
    close();
    return Request::Open;
  };
  navigator.onNext([&step] { step(true); });
  navigator.onPrevious([&step] { step(false); });

  const auto swipe = input.wasSwipe();
  if (swipe != MappedInputManager::SwipeDir::None) {
    if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
      step(swipe == MappedInputManager::SwipeDir::Up);
    }
    return request;
  }
  if (input.wasReleased(MappedInputManager::Button::Back)) {
    close();
    return Request::Redraw;
  }
  if (input.wasReleased(MappedInputManager::Button::Confirm)) return choose(selected);

  const Rect panel = DeskTheme::homeMenuPanelRect(renderer, count);
  const Rect rows = DeskTheme::homeMenuRowsRect(panel);
  int row = -1;
  const auto touch = input.rowTouch(row, rows.y, DeskTheme::menuRowStep(renderer), count, rows.x, rows.x + rows.width,
                                    GUI.getMenuRowHeight(renderer));
  if (touch == MappedInputManager::RowTouch::Down) {
    if (selected == row) return request;
    selected = row;
    return Request::Redraw;
  }
  if (touch == MappedInputManager::RowTouch::Tap) {
    haptic_feedback::touchAction();
    return choose(row);
  }
  int tx = 0;
  int ty = 0;
  if (input.wasScreenTapped(tx, ty) && !contains(panel, tx, ty)) {
    close();
    return Request::Redraw;
  }
  return request;
}

MappedInputManager::RowTouch DeskHomeUi::buttonTouch() const {
  const Rect button = DeskTheme::homeMenuButtonRect(renderer);
  int row = -1;
  return input.rowTouch(row, button.y, button.height, 1, button.x, button.x + button.width, button.height);
}

void DeskHomeUi::draw(const bool buttonSelected, const bool hasBooks) const {
  DeskTheme::drawHomeMenuRow(renderer, DeskTheme::homeMenuButtonRect(renderer), buttonSelected, hasBooks);
  drawTodo();
  drawCalendar();
  drawWeather();
  if (!open) return;
  const int count = itemCount();
  const HomeMenuItem slotItem = slot;
  DeskTheme::drawHomeMenuPanel(
      renderer, DeskTheme::homeMenuPanelRect(renderer, count), count, selected,
      [slotItem](const int i) { return std::string(itemLabel(deskMenuItemAt(i, slotItem))); },
      [slotItem](const int i) { return itemIcon(deskMenuItemAt(i, slotItem)); });
}

const char* DeskHomeUi::backHint(const bool hasBooks) const {
  if (open) return tr(STR_BACK);
  return hasBooks ? tr(STR_RESUME) : "";
}

int DeskHomeUi::thumbHeightFor(const int bookIndex) { return bookIndex == 0 ? CURRENT_H : SMALL_H; }

int DeskHomeUi::bookCount() const { return books ? std::min(static_cast<int>(books->size()), MAX_BOOKS) : 0; }

int DeskHomeUi::stripPageCount() const {
  const int strip = std::max(0, bookCount() - 1);
  return (strip + STRIP_PAGE - 1) / STRIP_PAGE;
}

void DeskHomeUi::follow(const int selectorIndex) {
  if (selectorIndex < 1 || selectorIndex >= bookCount()) return;
  stripFirst = 1 + (selectorIndex - 1) / STRIP_PAGE * STRIP_PAGE;
}

bool DeskHomeUi::pageStrip(const bool next, int& selectorIndex) {
  const int page = (stripFirst - 1) / STRIP_PAGE;
  const int target = next ? page + 1 : page - 1;
  if (target < 0 || target >= stripPageCount()) return false;
  stripFirst = 1 + target * STRIP_PAGE;
  if (selectorIndex >= 1 && selectorIndex < bookCount()) selectorIndex = stripFirst;
  return true;
}

MappedInputManager::RowTouch DeskHomeUi::bookTouch(int& index) const {
  const int count = bookCount();
  if (count == 0) return MappedInputManager::RowTouch::None;
  const BookLayout layout = bookLayout(renderer);
  int col = -1;
  auto touch = input.colTouch(col, layout.current.x, layout.current.width, 1, layout.current.y,
                              layout.current.y + layout.current.height);
  if (touch != MappedInputManager::RowTouch::None) {
    index = 0;
    return touch;
  }
  const int visible = std::min(STRIP_PAGE, count - stripFirst);
  if (visible <= 0) return MappedInputManager::RowTouch::None;
  touch = input.colTouch(col, layout.stripX, layout.smallW + SMALL_GAP, visible, layout.top, layout.top + SMALL_H,
                         layout.smallW);
  if (touch != MappedInputManager::RowTouch::None) index = stripFirst + col;
  return touch;
}

void DeskHomeUi::drawCover(const Rect rect, const int bookIndex, const bool selected) {
  const RecentBook& book = (*books)[bookIndex];
  const std::string path =
      book.coverBmpPath.empty() ? std::string() : UITheme::getCoverThumbPath(book.coverBmpPath, rect.height);
  coverCache.paint(freeink::ui::Rect{static_cast<int16_t>(rect.x), static_cast<int16_t>(rect.y),
                                     static_cast<int16_t>(rect.width), static_cast<int16_t>(rect.height)},
                   static_cast<size_t>(bookIndex), path);
  if (selected) {
    renderer.drawRoundedRect(rect.x - SELECT_OUTSET, rect.y - SELECT_OUTSET, rect.width + 2 * SELECT_OUTSET,
                             rect.height + 2 * SELECT_OUTSET, SELECT_LINE, DeskMetrics::values.listRowRadius, true);
  }
}

// Title, progress row, then time (left) and pages turned (right-aligned).
// Unknown values are omitted, as elsewhere on Home.
void DeskHomeUi::drawCurrentStats(const Rect rect) const {
  const BookReadingSummary& summary = summaries[0];
  int y = rect.y +
          drawTitle(renderer, UI_10_FONT_ID, EpdFontFamily::BOLD, rect.x, rect.y, rect.width, 2, (*books)[0].title);
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  if (summary.percent >= 0) {
    y += ROW_GAP;
    drawProgress(renderer, UI_10_FONT_ID, EpdFontFamily::BOLD, rect.x, y, rect.width, BAR_HEIGHT, summary.percent,
                 true);
    y += lineH;
  }
  if (!summary.timeKnown) return;
  y += ROW_GAP;
  const int iconY = y + (lineH - STAT_ICON) / 2;
  char pages[24];
  snprintf(pages, sizeof(pages), "%llu", static_cast<unsigned long long>(summary.forwardTurns));
  const int pagesW = renderer.getTextWidth(UI_10_FONT_ID, pages);
  // Room for the time between its icon and the pages group; a long time drops
  // its minutes, and if it still does not fit the pages group is left out.
  const int timeRoom = rect.width - 2 * (STAT_ICON + ICON_GAP) - pagesW - STAT_MIN_GAP;
  char time[24];
  formatShortReadingTime(summary, time, sizeof(time));
  if (renderer.getTextWidth(UI_10_FONT_ID, time) > timeRoom) {
    formatShortReadingTime(summary, time, sizeof(time), true);
  }
  renderer.drawIcon(ClockIcon, rect.x, iconY, STAT_ICON);
  renderer.drawText(UI_10_FONT_ID, rect.x + STAT_ICON + ICON_GAP, y, time, true);
  if (renderer.getTextWidth(UI_10_FONT_ID, time) > timeRoom) return;
  const int right = rect.x + rect.width;
  renderer.drawText(UI_10_FONT_ID, right - pagesW, y, pages, true);
  renderer.drawIcon(PagesIcon, right - pagesW - ICON_GAP - STAT_ICON, iconY, STAT_ICON);
}

void DeskHomeUi::drawBooks(const int selectorIndex) {
  const BookLayout layout = bookLayout(renderer);
  const int count = bookCount();
  if (count == 0) {
    // Lyra's empty text points "below", where Desk has no list.
    const Rect area{layout.current.x, layout.top, layout.current.width * 2, layout.current.height};
    renderer.drawText(UI_12_FONT_ID, area.x, area.y + area.height / 2 - renderer.getLineHeight(UI_12_FONT_ID) - 2,
                      tr(STR_NO_OPEN_BOOK), true, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, area.x, area.y + area.height / 2 + 2, tr(STR_DESK_START_READING), true);
    return;
  }
  coverCache.prepare();
  drawCover(layout.current, 0, selectorIndex == 0);
  drawCurrentStats(
      Rect{layout.current.x, layout.current.y + layout.current.height + TEXT_GAP, layout.current.width, 0});

  // Strip: title (two lines reserved, so bars line up) and a progress bar.
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int barY = stripBarY(renderer, layout);
  for (int slot = 0; slot < STRIP_PAGE && stripFirst + slot < count; ++slot) {
    const int index = stripFirst + slot;
    const Rect rect = smallRect(layout, slot);
    drawCover(rect, index, selectorIndex == index);
    drawTitle(renderer, SMALL_FONT_ID, EpdFontFamily::REGULAR, rect.x, rect.y + rect.height + TEXT_GAP, rect.width, 2,
              (*books)[index].title);
    if (summaries[index].percent >= 0) {
      drawProgress(renderer, SMALL_FONT_ID, EpdFontFamily::REGULAR, rect.x, barY, rect.width, SMALL_BAR_HEIGHT,
                   summaries[index].percent, false);
    }
  }
  const int pages = stripPageCount();
  if (pages > 1) {
    const int dotsY = barY + lineH + 12;
    const int stripW = STRIP_PAGE * layout.smallW + (STRIP_PAGE - 1) * SMALL_GAP;
    const int dotsW = pages * DOT_SIZE + (pages - 1) * DOT_SIZE;
    int x = layout.stripX + (stripW - dotsW) / 2;
    const int current = (stripFirst - 1) / STRIP_PAGE;
    for (int page = 0; page < pages; ++page, x += 2 * DOT_SIZE) {
      if (page == current) {
        renderer.fillRoundedRect(x, dotsY, DOT_SIZE, DOT_SIZE, DOT_SIZE / 2, Color::Black);
      } else {
        renderer.drawRoundedRect(x, dotsY, DOT_SIZE, DOT_SIZE, 1, DOT_SIZE / 2, true);
      }
    }
  }
}

void DeskHomeUi::loadCalendar() {
  calendarVisible = false;
  tm date{};
  const auto local = readingcalendar::localMilliseconds(&date);
  if (!local) return;
  auto ledger = makeUniqueNoThrow<readingcalendar::Store>();
  if (!ledger) {
    LOG_ERR("HOME", "OOM: calendar (%u bytes)", unsigned(sizeof(readingcalendar::Store)));
    return;
  }
  ledger->load();
  const auto today = int32_t(local / readingcalendar::Model::MAX_DAY_MS);
  if (!ledger->canSave() || today < ledger->model().latestDay()) return;
  // Only the temporary display model changes here. Home never saves a ledger.
  ledger->model().changeGoal(today, readingcalendar::GOALS[ledger->goalIndex()]);
  const int year = date.tm_year + 1900;
  const int month = date.tm_mon + 1;
  const auto first = readingcalendar::civilDay(year, month, 1);
  const auto next = readingcalendar::civilDay(year + (month == 12), month == 12 ? 1 : month + 1, 1);
  calendarDays = next - first;
  calendarToday = date.tm_mday;
  calendarFirstWeekday = (first + 3) % 7;
  calendarMarks = 0;
  for (int day = 1; day <= calendarDays; ++day) {
    if (ledger->model().achieved(first + day - 1)) calendarMarks |= uint32_t(1) << (day - 1);
  }
  calendarMinutes = ledger->model().minutes(first, next - 1);
  calendarStreak = ledger->model().streak(today);
  static constexpr StrId MONTHS[] = {StrId::STR_CAL_JAN, StrId::STR_CAL_FEB, StrId::STR_CAL_MAR, StrId::STR_CAL_APR,
                                     StrId::STR_CAL_MAY, StrId::STR_CAL_JUN, StrId::STR_CAL_JUL, StrId::STR_CAL_AUG,
                                     StrId::STR_CAL_SEP, StrId::STR_CAL_OCT, StrId::STR_CAL_NOV, StrId::STR_CAL_DEC};
  snprintf(calendarHeading, sizeof(calendarHeading), tr(STR_CAL_MONTH_YEAR), I18N.get(MONTHS[date.tm_mon]), year);
  const auto layout = bookLayout(renderer);
  const auto heading =
      renderer.truncatedText(UI_10_FONT_ID, calendarHeading, layout.stripX - layout.current.x, EpdFontFamily::BOLD);
  snprintf(calendarHeading, sizeof(calendarHeading), "%s", heading.c_str());
  calendarVisible = true;
}

void DeskHomeUi::drawCalendar() const {
  if (!calendarVisible) return;
  const auto layout = bookLayout(renderer);
  const int x = layout.current.x;
  const int width =
      (UITheme::getContentArea(renderer).width - 2 * DeskMetrics::values.contentSidePadding - SMALL_GAP) / 2;
  const int y = weatherRect().y;
  const int line = renderer.getLineHeight(SMALL_FONT_ID);
  const int headingH = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawText(UI_10_FONT_ID, x, y, calendarHeading, true, EpdFontFamily::BOLD);
  const int ruleY = y + headingH + 4;
  renderer.drawLine(x, ruleY, x + width - 1, ruleY, true);
  const int cell = width / 7;
  static constexpr StrId WEEK[] = {StrId::STR_CAL_MON, StrId::STR_CAL_TUE, StrId::STR_CAL_WED, StrId::STR_CAL_THU,
                                   StrId::STR_CAL_FRI, StrId::STR_CAL_SAT, StrId::STR_CAL_SUN};
  for (int col = 0; col < 7; ++col) {
    const auto text = I18N.get(WEEK[col]);
    renderer.drawText(SMALL_FONT_ID, x + col * cell + (cell - renderer.getTextWidth(SMALL_FONT_ID, text)) / 2,
                      ruleY + 4, text, true);
  }
  const int gridY = ruleY + 4 + line + 4;
  const int rowH = std::max(24, line + 4);
  for (int day = 1; day <= calendarDays; ++day) {
    const int pos = calendarFirstWeekday + day - 1;
    const int cx = x + (pos % 7) * cell;
    const int cy = gridY + (pos / 7) * rowH;
    const bool marked = calendarMarks & (uint32_t(1) << (day - 1));
    if (marked) renderer.fillRoundedRect(cx + 2, cy, cell - 4, rowH - 2, (rowH - 2) / 2, Color::Black);
    if (day == calendarToday) renderer.drawRect(cx, cy - 1, cell, rowH, true);
    char number[4];
    snprintf(number, sizeof(number), "%d", day);
    renderer.drawText(SMALL_FONT_ID, cx + (cell - renderer.getTextWidth(SMALL_FONT_ID, number)) / 2,
                      cy + (rowH - line) / 2, number, !marked);
  }
  const int footerY = widgetsRuleY();
  renderer.drawLine(x, footerY, x + width - 1, footerY, true);
  char value[40];
  snprintf(value, sizeof(value), tr(STR_CAL_MINUTES), static_cast<unsigned long long>(calendarMinutes));
  renderer.drawText(UI_10_FONT_ID, x, footerY + 8, value, true, EpdFontFamily::BOLD);
  snprintf(value, sizeof(value), (calendarStreak == 1 ? tr(STR_CAL_DAY) : tr(STR_CAL_DAYS)), unsigned(calendarStreak));
  renderer.drawText(UI_10_FONT_ID, x + width / 2, footerY + 8, value, true, EpdFontFamily::BOLD);
  renderer.drawText(SMALL_FONT_ID, x, footerY + 8 + headingH, tr(STR_CAL_THIS_MONTH), true);
  renderer.drawText(SMALL_FONT_ID, x + width / 2, footerY + 8 + headingH, tr(STR_CAL_STREAK), true);
}

Rect DeskHomeUi::weatherRect() const {
  const auto content = UITheme::getContentArea(renderer);
  const auto todo = todoRect();
  const int padding = DeskMetrics::values.contentSidePadding;
  const int width = (content.width - 2 * padding - SMALL_GAP) / 2;
  const int top = todo.y + todo.height + 24;
  return Rect{content.x + content.width - padding - width, top, width, renderer.getScreenHeight() - top - 16};
}
int DeskHomeUi::widgetsRuleY() const {
  const int line = renderer.getLineHeight(SMALL_FONT_ID);
  const int gridY = weatherRect().y + renderer.getLineHeight(UI_10_FONT_ID) + 4 + 4 + line + 4;
  return gridY + 6 * std::max(24, line + 4) + 6;
}
void DeskHomeUi::loadWeather() {
  weatherData.cached = false;
  auto store = makeUniqueNoThrow<weather::Store>();
  if (!store) {
    LOG_ERR("HOME", "OOM: weather cache (%u bytes)", unsigned(sizeof(weather::Store)));
    return;
  }
  store->load();
  if (!store->available() || !store->model().cached) return;
  weatherData = store->model();
  weatherNow = std::max<int64_t>(0, trustedtime::trustedNow());
  const auto label =
      renderer.truncatedText(UI_10_FONT_ID, weatherData.location.name, weatherRect().width, EpdFontFamily::BOLD);
  snprintf(weatherTitle, sizeof(weatherTitle), "%s", label.c_str());
}
bool DeskHomeUi::weatherTapped() const {
  if (!weatherData.cached) return false;
  const auto area = weatherRect();
  int row = -1;
  return input.rowTouch(row, area.y, area.height, 1, area.x, area.x + area.width, area.height) ==
         MappedInputManager::RowTouch::Tap;
}
void DeskHomeUi::drawWeather() const {
  if (!weatherData.cached) return;
  const auto r = weatherRect();
  const auto& f = weatherData.forecast;
  const int small = renderer.getLineHeight(SMALL_FONT_ID), heading = renderer.getLineHeight(UI_10_FONT_ID);
  int y = r.y;
  char value[96], chance[20];
  renderer.drawText(UI_10_FONT_ID, r.x, y, weatherTitle, true, EpdFontFamily::BOLD);
  y += heading + 4;
  renderer.drawLine(r.x, y, r.x + r.width - 1, y, true);
  y += 10;
  renderer.drawIcon(weatherui::icon(f.code, f.isDay), r.x, y, 48);
  weatherui::temperature(value, sizeof(value), f.temperature, weatherData.fahrenheit, true);
  renderer.drawText(UI_12_FONT_ID, r.x + 60, y, value, true, EpdFontFamily::BOLD);
  renderer.drawText(SMALL_FONT_ID, r.x + 60, y + renderer.getLineHeight(UI_12_FONT_ID) + 4,
                    weatherui::condition(f.code), true);
  y += 48 + 12;
  weatherui::percent(chance, sizeof(chance), f.humidity);
  snprintf(value, sizeof(value), tr(STR_WEATHER_HUMIDITY_VALUE), chance);
  renderer.drawText(SMALL_FONT_ID, r.x, y, value, true);
  weatherui::percent(chance, sizeof(chance), f.days[0].chance);
  snprintf(value, sizeof(value), tr(STR_WEATHER_PRECIP_VALUE), chance);
  renderer.drawText(SMALL_FONT_ID, r.x + (r.width + SMALL_GAP) / 2, y, value, true);
  y += small + 8;
  for (unsigned i = 0; i < weather::DAYS; ++i) {
    const auto& d = f.days[i];
    weatherui::day(value, sizeof(value), d.date, f, weatherNow, true);
    if (renderer.getTextWidth(SMALL_FONT_ID, value) > 54) {
      unsigned year, month, dom;
      weather::civilDate(d.date, year, month, dom);
      snprintf(value, sizeof(value), "%u", dom);
    }
    renderer.drawText(SMALL_FONT_ID, r.x, y, value, true);
    weatherui::percent(chance, sizeof(chance), d.chance);
    const int chanceW = renderer.getTextWidth(SMALL_FONT_ID, chance);
    weatherui::range(value, sizeof(value), d, weatherData.fahrenheit);
    if (renderer.getTextWidth(SMALL_FONT_ID, value) > r.width - 58 - chanceW - 8)
      snprintf(value, sizeof(value), tr(STR_WEATHER_COMPACT_RANGE),
               weather::temperature(d.high, weatherData.fahrenheit),
               weather::temperature(d.low, weatherData.fahrenheit));
    renderer.drawText(SMALL_FONT_ID, r.x + 58, y, value, true);
    renderer.drawText(SMALL_FONT_ID, r.x + r.width - chanceW, y, chance, true);
    y += small + 4;
  }
  y = widgetsRuleY();
  renderer.drawLine(r.x, y, r.x + r.width - 1, y, true);
  y += 6;
  weatherui::updated(value, sizeof(value), f);
  renderer.drawText(SMALL_FONT_ID, r.x, y, value, true);
  y += small;
  renderer.drawText(SMALL_FONT_ID, r.x, y,
                    weatherui::stale(f, weatherNow) ? tr(STR_WEATHER_CACHED_PROVIDER) : tr(STR_WEATHER_PROVIDER), true);
}
