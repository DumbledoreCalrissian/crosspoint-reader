#pragma once

#include "components/themes/lyra/Lyra3CoversTheme.h"

struct Rect;

namespace DeskMetrics {
// Lyra Extended with a taller top band: the menu button row sits below the
// status strip, whose taps open the frontlight panel (ActivityManager).
constexpr ThemeMetrics values = [] {
  ThemeMetrics v = Lyra3CoversMetrics::values;
  v.homeTopPadding = 108;
  return v;
}();
}  // namespace DeskMetrics

// Desk: Home drawn by DeskHomeUi, with the apps behind a menu button at the
// top right. Other screens keep Lyra styling.
class DeskTheme : public Lyra3CoversTheme {
 public:
  // Label-sized hit/draw area for the Home menu button, right of the header band.
  static Rect homeMenuButtonRect(const GfxRenderer& renderer);
  // Draws the "Continue reading" label left of the button when there are books.
  static void drawHomeMenuRow(const GfxRenderer& renderer, Rect button, bool selected, bool hasBooks);
  // Menu panel hanging from the button over Home; rows use menuRowStep().
  static Rect homeMenuPanelRect(const GfxRenderer& renderer, int rowCount);
  // Row tiles inside the panel (drawn and touched), inset equally on all sides.
  static Rect homeMenuRowsRect(Rect panel);
  static int menuRowStep(const GfxRenderer& renderer);
  static void drawHomeMenuPanel(GfxRenderer& renderer, Rect panel, int rowCount, int selectedIndex,
                                const std::function<std::string(int)>& label, const std::function<UIIcon(int)>& icon);
};
