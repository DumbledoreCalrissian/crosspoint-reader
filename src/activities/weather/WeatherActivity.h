#pragma once
#include <memory>

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "util/WeatherClient.h"
#include "util/WeatherStore.h"
class WeatherActivity final : public Activity, private UiAppHost {
 public:
  WeatherActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("Weather", renderer, input), UiAppHost(renderer) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override;

 private:
  enum Action : freeink::ui::ActionId { Back = 1, Refresh, Location, Units, Pick, Cancel };
  enum class Mode : uint8_t { Overview, Results, SelectingWifi, Ready, Transferring, AwaitingSleep };
  enum class Purpose : uint8_t { Search, Forecast };
  Mode mode = Mode::Overview;
  Purpose purpose = Purpose::Forecast;
  std::unique_ptr<weather::Store> store;
  weather::Location places[weather::MAX_PLACES]{};
  freeink::ui::ListItem rows[weather::MAX_PLACES]{};
  char subtitles[weather::MAX_PLACES][72]{};
  char query[65]{};
  char title[84]{};
  char noticeLines[3][128]{};
  char shortNotice[128]{};
  unsigned placeCount = 0;
  int selection = 0;
  const char* notice = nullptr;
  bool cancelRequested = false;
  bool networkUsed = false;
  uint64_t now = 0;
  static void screenFn(UiScreen& screen, void* context);
  static void action(const freeink::ui::ActionEvent& event, void* context);
  void buildScreen(UiScreen& screen);
  void activate(freeink::ui::ActionId id, int value = 0);
  void openLocation();
  void beginNetwork(Purpose operation);
  void transfer();
  bool pollCancel();
  void stopNetwork();
  void updateText();
  void moveSelection(int delta);
  void drawForecast();
};
