#include "EpubBookStatisticsActivity.h"

#include <I18n.h>

#include <cstdio>

void EpubBookStatisticsActivity::onEnter() {
  Activity::onEnter();
  showSummary();
}

void EpubBookStatisticsActivity::showSummary() {
  RenderLock lock;
  confirmingReset = false;
  std::string message;
  if (statistics && statistics->available()) {
    const auto totals = statistics->totals();
    const auto seconds = totals.readingMilliseconds / 1000;
    char summary[128];
    char progress[48];
    if (percent >= 0)
      snprintf(progress, sizeof(progress), tr(STR_STATS_PROGRESS), percent);
    else
      snprintf(progress, sizeof(progress), "%s", tr(STR_STATS_PROGRESS_UNAVAILABLE));
    snprintf(summary, sizeof(summary), tr(STR_STATS_SUMMARY), progress, static_cast<unsigned long long>(seconds / 3600),
             static_cast<unsigned long long>((seconds / 60) % 60), static_cast<unsigned long long>(seconds % 60),
             static_cast<unsigned long long>(totals.forwardTurns));
    message = summary;
    if (statistics->unsaved()) {
      message += ' ';
      message += tr(STR_STATS_UNSAVED);
    } else if (statistics->writeFailed()) {
      message += ' ';
      message += tr(STR_STATS_RESET_FAILED);
    } else if (statistics->status() == EpubReadingStats::Status::Recovered) {
      message += ' ';
      message += tr(STR_STATS_RECOVERED);
    }
  } else {
    char progress[48];
    if (percent >= 0)
      snprintf(progress, sizeof(progress), tr(STR_STATS_PROGRESS), percent);
    else
      snprintf(progress, sizeof(progress), "%s", tr(STR_STATS_PROGRESS_UNAVAILABLE));
    message = progress;
    message += ' ';
    if (statistics && statistics->writeFailed()) {
      message += tr(STR_STATS_RESET_FAILED);
      message += ' ';
    }
    if (!statistics) {
      message += tr(STR_STATS_UNAVAILABLE);
    } else {
      using Status = EpubReadingStats::Status;
      switch (statistics->status()) {
        case Status::Corrupt:
          message += tr(STR_STATS_CORRUPT);
          break;
        case Status::Unsupported:
          message += tr(STR_STATS_UNSUPPORTED);
          break;
        case Status::PathConflict:
          message += tr(STR_STATS_CONFLICT);
          break;
        case Status::Ok:
        case Status::Missing:
        case Status::Recovered:
        case Status::InvalidPath:
        case Status::StorageError:
        case Status::Exhausted:
        case Status::OutOfMemory:
          message += tr(STR_STATS_UNAVAILABLE);
          break;
      }
    }
  }
  const bool resettable = statistics && statistics->canReset();
  const bool retry = statistics && statistics->unsaved();
  const char* options[] = {tr(STR_BACK), tr(STR_STATS_RESET), tr(STR_STATS_RETRY)};
  if (!resettable && retry) options[1] = options[2];
  popup.showMessage(
      tr(STR_BOOK_STATISTICS), message.c_str(), options, 1 + resettable + retry, 0, [this, resettable](int index) {
        pending = index == 0 ? Action::Close : (index == 1 && resettable ? Action::ConfirmReset : Action::Retry);
      });
  requestUpdate();
}

void EpubBookStatisticsActivity::loop() {
  if (sleeping) return;
  if (pending == Action::None) {
    popup.handleInput(mappedInput, [this] { requestUpdate(); });
    if (!popup.isActive() && pending == Action::None) {
      if (confirmingReset)
        pending = Action::Summary;
      else
        finish();
    }
    return;
  }
  const auto action = pending;
  pending = Action::None;
  switch (action) {
    case Action::None:
      break;
    case Action::Close:
      finish();
      break;
    case Action::ConfirmReset: {
      RenderLock lock;
      confirmingReset = true;
      const char* options[] = {tr(STR_CANCEL), tr(STR_STATS_RESET)};
      popup.showMessage(tr(STR_STATS_RESET), tr(STR_STATS_RESET_CONFIRM), options, 2, 0,
                        [this](int index) { pending = index == 1 ? Action::Reset : Action::Summary; });
      requestUpdate();
      break;
    }
    case Action::Reset: {
      {
        RenderLock lock;
        if (statistics) statistics->reset();
      }
      showSummary();
      break;
    }
    case Action::Retry: {
      {
        RenderLock lock;
        if (statistics) statistics->pause(millis());
      }
      showSummary();
      break;
    }
    case Action::Summary:
      showSummary();
      break;
  }
}

void EpubBookStatisticsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  popup.processRender(renderer, mappedInput);
}
