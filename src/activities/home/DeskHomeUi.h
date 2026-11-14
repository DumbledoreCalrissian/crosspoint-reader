#pragma once

#include <TodoModel.h>

#include <vector>

#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "components/HomeCoverCache.h"

class ButtonNavigator;
class GfxRenderer;
struct BookReadingSummary;
struct Rect;
struct RecentBook;

// Desk theme's Home: the current book large with its stats, the other recent
// books as a paged strip of small covers, the menu button and the app menu
// panel drawn over Home. HomeActivity keeps the book list and app launching.
class DeskHomeUi final {
 public:
  // What a pass of input asked Home to do.
  enum class Request : uint8_t { None, Redraw, Open };

  // Current book plus the strip; matches Home's summaries array.
  static constexpr int MAX_BOOKS = HomeCoverCache::MAX_COVERS;

  DeskHomeUi(GfxRenderer& renderer, MappedInputManager& input)
      : renderer(renderer), input(input), coverCache(renderer) {}
  // slot: Plugins, OPDS or NONE, as Home's slotItem(). books and summaries
  // (one per book) stay owned by Home for the visit.
  void begin(HomeMenuItem slot, const std::vector<RecentBook>* books, const BookReadingSummary* summaries);
  // Thumbs are generated at the drawn height; rescaling dithered art aliases.
  static int thumbHeightFor(int bookIndex);

  // Keeps a selected strip book on the visible page.
  void follow(int selectorIndex);
  // Horizontal swipe: next (true) or previous strip page. A selection on the
  // strip moves to the new page's first book. False when the page is unchanged.
  bool pageStrip(bool next, int& selectorIndex);
  // Touch on a visible cover; index is the book index.
  MappedInputManager::RowTouch bookTouch(int& index) const;
  // Covers, current-book stats, strip percentages and page dots.
  void drawBooks(int selectorIndex);

  bool menuOpen() const { return open; }
  void openMenu();
  // While the panel is open it owns the input. On Open, chosen() is the item
  // and the panel is already closed.
  Request loopMenu(ButtonNavigator& navigator);
  HomeMenuItem chosen() const { return chosenItem; }

  // Touch on the menu button while the panel is closed.
  MappedInputManager::RowTouch buttonTouch() const;
  bool todoTapped() const;

  // Drawn after the covers, so the panel lies over them and never enters the
  // cover snapshot.
  void draw(bool buttonSelected, bool hasBooks) const;
  // Back closes an open panel instead of resuming.
  const char* backHint(bool hasBooks) const;

 private:
  int itemCount() const;
  void close();
  int bookCount() const;
  int stripPageCount() const;
  void drawCover(Rect rect, int bookIndex, bool selected);
  void drawCurrentStats(Rect rect) const;
  Rect todoRect() const;
  void loadTodo();
  void drawTodo() const;

  static constexpr size_t TODO_ROWS = 3;
  // Full UTF-8 task plus a possible three-byte ellipsis and terminator.
  char todoTexts[TODO_ROWS][TodoModel::MAX_TEXT_BYTES + 4]{};
  size_t todoCount = 0;

  GfxRenderer& renderer;
  MappedInputManager& input;
  HomeMenuItem slot = HomeMenuItem::NONE;
  HomeMenuItem chosenItem = HomeMenuItem::NONE;
  bool open = false;
  int selected = 0;
  const std::vector<RecentBook>* books = nullptr;
  const BookReadingSummary* summaries = nullptr;
  int stripFirst = 1;  // book index of the first visible strip cover
  HomeCoverCache coverCache;
};
