#include "DeskHomeUi.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <string>

#include "HapticFeedback.h"
#include "HomeMenuOrder.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "components/themes/desk/DeskTheme.h"
#include "util/ButtonNavigator.h"

namespace {
const char* itemLabel(const HomeMenuItem item) {
  switch (item) {
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

void DeskHomeUi::begin(const HomeMenuItem slotItem) {
  slot = slotItem;
  open = false;
  selected = 0;
  chosenItem = HomeMenuItem::NONE;
}

int DeskHomeUi::itemCount() const {
  int count = 0;
  while (homeMenuItemAt(count, slot) != HomeMenuItem::NONE) ++count;
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
    chosenItem = homeMenuItemAt(index, slot);
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
  if (!open) return;
  const int count = itemCount();
  const HomeMenuItem slotItem = slot;
  DeskTheme::drawHomeMenuPanel(
      renderer, DeskTheme::homeMenuPanelRect(renderer, count), count, selected,
      [slotItem](const int i) { return std::string(itemLabel(homeMenuItemAt(i, slotItem))); },
      [slotItem](const int i) { return itemIcon(homeMenuItemAt(i, slotItem)); });
}

const char* DeskHomeUi::backHint(const bool hasBooks) const {
  if (open) return tr(STR_BACK);
  return hasBooks ? tr(STR_RESUME) : "";
}
