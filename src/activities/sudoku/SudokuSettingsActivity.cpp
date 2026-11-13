#include "SudokuSettingsActivity.h"

#include <FontCacheManager.h>
#include <I18n.h>
#include <Memory.h>
#include <WiFi.h>
#ifdef SIMULATOR
#include <SimTransferInput.h>
#endif
#include <cstdio>
#include <cstdlib>

#include "CrossPointSettings.h"
#include "SilentRestart.h"
#include "SudokuActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/CatalogScreens.h"
#include "components/UITheme.h"
#include "network/HttpDownloader.h"

namespace fui = freeink::ui;
namespace {
struct Row {
  StrId name;
  uint8_t CrossPointSettings::* field;
};
constexpr Row ROWS[SudokuSettingsActivity::ITEM_COUNT] = {
    {StrId::STR_SUDOKU_SHOW_EASY, &CrossPointSettings::sudokuShowEasy},
    {StrId::STR_SUDOKU_MARK_CONFLICTS, &CrossPointSettings::sudokuMarkConflicts},
    {StrId::STR_SUDOKU_CHECK_BUTTON, &CrossPointSettings::sudokuCheckButton},
    {StrId::STR_SUDOKU_REVEAL_BUTTON, &CrossPointSettings::sudokuRevealButton},
};
constexpr const char* BANK_URL =
    "https://github.com/DumbledoreCalrissian/crosspoint-reader/releases/download/sudoku-bank-v2/";
}  // namespace

void SudokuSettingsActivity::onEnter() {
  UiListActivity::onEnter();
  app.on(ACTION_CANCEL, cancelAction, this);
  refresh();
}

void SudokuSettingsActivity::onExit() {
  Activity::onExit();
  if (networkUsed) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    silentRestartToSudokuSettings();
  }
}

const char* SudokuSettingsActivity::headerTitle() const { return tr(STR_SUDOKU_SETTINGS); }
int SudokuSettingsActivity::listCount() const { return state == State::Browsing ? ROW_COUNT : 1; }

void SudokuSettingsActivity::refresh() {
  RenderLock lock(*this);
  closeRouting();
  for (int i = 0; i < ITEM_COUNT; ++i) {
    rowItems[i].label = I18N.get(ROWS[i].name);
    rowItems[i].value = SETTINGS.*(ROWS[i].field) ? tr(STR_STATE_ON) : tr(STR_STATE_OFF);
  }
  rowItems[ITEM_COUNT].label = tr(STR_SUDOKU_DOWNLOAD_ALL);
  for (int d = 0; d < sudoku::DIFFICULTIES; ++d) {
    const auto difficulty = static_cast<sudoku::Difficulty>(d);
    sudoku::PackInfo info;
    const auto status = sudoku::readPackInfo(difficulty, info);
    const auto& pack = sudoku::publishedPack(difficulty);
    const char* name = SudokuActivity::difficultyLabel(difficulty);
    const int index = ITEM_COUNT + 1 + 2 * d;
    std::snprintf(labels[2 * d], sizeof(labels[0]), tr(STR_SUDOKU_DOWNLOAD_PACK), name);
    std::snprintf(labels[2 * d + 1], sizeof(labels[0]), tr(STR_SUDOKU_DELETE_PACK), name);
    const char* statusText = status == sudoku::PackStatus::Valid     ? tr(STR_SUDOKU_INSTALLED)
                             : status == sudoku::PackStatus::Missing ? tr(STR_SUDOKU_NOT_INSTALLED)
                                                                     : tr(STR_SUDOKU_DAMAGED);
    std::snprintf(subtitles[d], sizeof(subtitles[0]), tr(STR_SUDOKU_PACK_SIZE), statusText,
                  static_cast<unsigned long>(status == sudoku::PackStatus::Valid ? info.count : pack.count),
                  static_cast<unsigned long>(pack.bytes));
    rowItems[index].label = labels[2 * d];
    rowItems[index].subtitle = subtitles[d];
    rowItems[index + 1].label = labels[2 * d + 1];
    rowItems[index + 1].enabled = status != sudoku::PackStatus::Missing || Storage.exists(sudoku::partPath(difficulty));
  }
  for (int i = 0; i < ROW_COUNT; ++i) rowItems[i].actionValue = static_cast<int16_t>(i);
  requestUpdate();
}

void SudokuSettingsActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (state == State::Result) {
    finish();
    return;
  }
  if (state != State::Browsing || index < 0 || index >= ROW_COUNT) return;
  if (!rowItems[index].enabled) return;
  nav.selected = index;
  if (index < ITEM_COUNT) {
    uint8_t& value = SETTINGS.*(ROWS[index].field);
    value = value ? 0 : 1;
    SETTINGS.saveToFile();
    refresh();
    return;
  }
  if (index == ITEM_COUNT) {
    beginDownload(-1);
    return;
  }
  const int d = (index - ITEM_COUNT - 1) / 2;
  if ((index - ITEM_COUNT - 1) % 2 == 0)
    beginDownload(d);
  else
    confirmDelete(static_cast<sudoku::Difficulty>(d));
}

void SudokuSettingsActivity::confirmDelete(sudoku::Difficulty difficulty) {
  auto confirmation = makeUniqueNoThrow<ConfirmationActivity>(
      renderer, mappedInput, tr(STR_SUDOKU_DELETE_TITLE), tr(STR_SUDOKU_DELETE_BODY), tr(STR_CANCEL), tr(STR_DELETE));
  if (!confirmation) {
    LOG_ERR("SUDOKU", "Allocation failed: %u bytes", static_cast<unsigned>(sizeof(ConfirmationActivity)));
    notice = tr(STR_SUDOKU_MEMORY_ERROR);
    refresh();
    return;
  }
  startActivityForResult(std::move(confirmation), [this, difficulty](const ActivityResult& result) {
    if (!result.isCancelled)
      notice = sudoku::deletePack(difficulty) ? tr(STR_SUDOKU_PACK_DELETED) : tr(STR_SUDOKU_STORAGE_ERROR);
    refresh();
  });
}

void SudokuSettingsActivity::beginDownload(int difficulty) {
  if (!canDownload) {
    notice = tr(STR_SUDOKU_DOWNLOAD_UNSAVED);
    requestUpdate();
    return;
  }
  auto wifi = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput);
  if (!wifi) {
    LOG_ERR("SUDOKU", "Allocation failed: %u bytes", static_cast<unsigned>(sizeof(WifiSelectionActivity)));
    notice = tr(STR_SUDOKU_MEMORY_ERROR);
    requestUpdate();
    return;
  }
  requestedDifficulty = difficulty;
  networkUsed = true;
  {
    RenderLock lock(*this);
    state = State::SelectingWifi;
    closeRouting();
  }
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::move(wifi), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      finish();
      return;
    }
    RenderLock lock(*this);
    state = State::Ready;
  });
}

void SudokuSettingsActivity::cancelAction(const fui::ActionEvent&, void* context) {
  static_cast<SudokuSettingsActivity*>(context)->cancelRequested = true;
}

bool SudokuSettingsActivity::pollCancel() {
  // The synchronous transfer owns this input pump until it returns to loop().
#ifdef SIMULATOR
  simBeginTransferFrame();
#endif
  mappedInput.update(true);
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) || mappedInput.wasHomeGesture()) cancelRequested = true;
  routeTouch(mappedInput);
  return cancelRequested;
}

void SudokuSettingsActivity::transfer() {
  {
    RenderLock lock(*this);
    received = total = 0;
    completed = 0;
    closeRouting();
    if (auto* cache = renderer.getFontCacheManager()) cache->releaseSdFontCaches();
  }
  cancelRequested = false;
  const char* message = tr(STR_SUDOKU_DOWNLOAD_DONE);
  for (int d = 0; d < sudoku::DIFFICULTIES; ++d) {
    if (requestedDifficulty >= 0 && d != requestedDifficulty) continue;
    const auto difficulty = static_cast<sudoku::Difficulty>(d);
    const auto& pack = sudoku::publishedPack(difficulty);
    {
      RenderLock lock(*this);
      currentDifficulty = d;
      received = 0;
      total = pack.bytes;
    }
    requestUpdateAndWait();
    if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP ||
        ESP.getMaxAllocHeap() < HttpDownloader::MIN_TLS_MAX_ALLOC) {
      message = tr(STR_SUDOKU_MEMORY_ERROR);
      break;
    }
    if (!sudoku::recoverPack(difficulty) || !Storage.ensureDirectoryExists("/games/sudoku")) {
      message = tr(STR_SUDOKU_STORAGE_ERROR);
      break;
    }
    const char* base = BANK_URL;
#ifdef SIMULATOR
    if (const char* testUrl = std::getenv("CROSSPOINT_SIM_SUDOKU_BANK_URL")) base = testUrl;
#endif
    const std::string url = std::string(base) + sudoku::difficultyFileName(difficulty);
    const auto result = HttpDownloader::downloadVerified(
        url, sudoku::partPath(difficulty), pack.bytes, pack.sha256,
        [this](size_t bytes, size_t expected) {
          {
            RenderLock lock(*this);
            received = bytes;
            total = expected;
          }
          const auto now = millis();
          if (now - lastPaint >= 500 || bytes == expected) {
            lastPaint = now;
            requestUpdate(true);
          }
        },
        [this] { return pollCancel(); });
    if (result != HttpDownloader::OK) {
      switch (result) {
        case HttpDownloader::ABORTED:
          message = tr(STR_SUDOKU_DOWNLOAD_CANCELLED);
          break;
        case HttpDownloader::LOW_MEMORY:
          message = tr(STR_SUDOKU_MEMORY_ERROR);
          break;
        case HttpDownloader::HASH_ERROR:
          message = tr(STR_SUDOKU_HASH_ERROR);
          break;
        case HttpDownloader::FILE_ERROR:
          message = tr(STR_SUDOKU_STORAGE_ERROR);
          break;
        case HttpDownloader::HTTP_ERROR:
        case HttpDownloader::UNAUTHORIZED:
          message = tr(STR_SUDOKU_NETWORK_ERROR);
          break;
        case HttpDownloader::OK:
          break;
      }
      break;
    }
    if (!sudoku::installPack(difficulty)) {
      message = tr(STR_SUDOKU_INSTALL_ERROR);
      break;
    }
    ++completed;
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  mappedInput.resetHomeButtonInput();
  {
    RenderLock lock(*this);
    std::snprintf(resultText, sizeof(resultText), tr(STR_SUDOKU_DOWNLOAD_RESULT), message, completed);
    state = State::Result;
    nav.reset();
    closeRouting();
  }
  LOG_DBG("SUDOKU", "Download result: %s (installed %d)", message, completed);
  requestUpdate();
}

bool SudokuSettingsActivity::handleCustomInput() {
  if (state == State::Ready) {
    RenderLock lock(*this);
    state = State::Transferring;
    requestUpdate();
    return true;
  }
  if (state == State::Transferring) {
    transfer();
    return true;
  }
  return state == State::SelectingWifi;
}
void SudokuSettingsActivity::onBackButton() {
  if (state == State::Transferring)
    cancelRequested = true;
  else
    finish();
}

void SudokuSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (state == State::Transferring) {
    catalogDownloadScreen(screen, SudokuActivity::difficultyLabel(static_cast<sudoku::Difficulty>(currentDifficulty)),
                          received, total, ACTION_CANCEL);
    return;
  }
  if (state == State::Result) {
    fui::TextAreaProps text;
    text.text = resultText;
    text.showCaret = false;
    text.style = screen.theme().bodyText;
    text.style.maxLines = 6;
    screen.textArea(text, static_cast<int16_t>(6 * uiTarget.lineHeight(text.style.font)));
    fui::ListItem done;
    done.label = tr(STR_DONE);
    fui::ListProps props;
    props.items = &done;
    props.count = 1;
    props.action = ACTION_ROW;
    screen.list(props);
    return;
  }
  if (notice) {
    fui::TextAreaProps text;
    text.text = notice;
    text.showCaret = false;
    text.style = screen.theme().smallText;
    text.style.maxLines = 3;
    screen.textArea(text, static_cast<int16_t>(3 * uiTarget.lineHeight(text.style.font)));
  }
  fui::ListProps props;
  props.items = rowItems;
  props.count = ROW_COUNT;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.subtitleText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
