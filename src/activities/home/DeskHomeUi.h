#pragma once

#include "MappedInputManager.h"
#include "activities/ActivityManager.h"

class ButtonNavigator;
class GfxRenderer;

// Desk theme's Home chrome: the menu button row and the app menu panel drawn
// over Home. HomeActivity keeps the covers, their snapshot and app launching.
class DeskHomeUi final {
 public:
  // What a pass of input asked Home to do.
  enum class Request : uint8_t { None, Redraw, Open };

  DeskHomeUi(GfxRenderer& renderer, MappedInputManager& input) : renderer(renderer), input(input) {}
  // slot: Plugins, OPDS or NONE, as Home's slotItem().
  void begin(HomeMenuItem slot);

  bool menuOpen() const { return open; }
  void openMenu();
  // While the panel is open it owns the input. On Open, chosen() is the item
  // and the panel is already closed.
  Request loopMenu(ButtonNavigator& navigator);
  HomeMenuItem chosen() const { return chosenItem; }

  // Touch on the menu button while the panel is closed.
  MappedInputManager::RowTouch buttonTouch() const;

  // Drawn after the covers, so the panel lies over them and never enters the
  // cover snapshot.
  void draw(bool buttonSelected, bool hasBooks) const;
  // Back closes an open panel instead of resuming.
  const char* backHint(bool hasBooks) const;

 private:
  int itemCount() const;
  void close();

  GfxRenderer& renderer;
  MappedInputManager& input;
  HomeMenuItem slot = HomeMenuItem::NONE;
  HomeMenuItem chosenItem = HomeMenuItem::NONE;
  bool open = false;
  int selected = 0;
};
