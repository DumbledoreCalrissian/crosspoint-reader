#include "DeskTheme.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int MENU_BUTTON_HEIGHT = 44;
constexpr int MENU_BUTTON_MIN_WIDTH = 81;
constexpr int MENU_BUTTON_PADDING = 16;
constexpr int PANEL_WIDTH = 280;
constexpr int PANEL_GAP = 4;       // between button and panel
constexpr int PANEL_PADDING = 10;  // frame to row highlight, equal on all sides
// drawHomeMenuPanel cancels LyraTheme::drawButtonMenu's side inset, which
// reads LyraMetrics; Desk must keep the same value.
static_assert(DeskMetrics::values.contentSidePadding == LyraMetrics::values.contentSidePadding);
}  // namespace

Rect DeskTheme::homeMenuButtonRect(const GfxRenderer& renderer) {
  const auto& metrics = DeskMetrics::values;
  const Rect content = UITheme::getContentArea(renderer);
  const int width =
      std::max(MENU_BUTTON_MIN_WIDTH,
               renderer.getTextWidth(UI_10_FONT_ID, tr(STR_DESK_APPS), EpdFontFamily::BOLD) + 2 * MENU_BUTTON_PADDING);
  const int y = metrics.homeTopPadding - metrics.verticalSpacing - MENU_BUTTON_HEIGHT;
  return Rect{content.x + content.width - metrics.contentSidePadding - width, y, width, MENU_BUTTON_HEIGHT};
}

void DeskTheme::drawHomeMenuRow(const GfxRenderer& renderer, const Rect rect, const bool selected,
                                const bool hasBooks) {
  const auto& metrics = DeskMetrics::values;
  if (hasBooks) {
    const int x = UITheme::getContentArea(renderer).x + metrics.contentSidePadding;
    const int room = rect.x - x - metrics.verticalSpacing;
    const int font = renderer.getTextWidth(UI_12_FONT_ID, tr(STR_CONTINUE_READING), EpdFontFamily::BOLD) <= room
                         ? UI_12_FONT_ID
                         : UI_10_FONT_ID;
    const int textY = rect.y + (rect.height - renderer.getLineHeight(font)) / 2;
    renderer.drawText(font, x, textY, tr(STR_CONTINUE_READING), true, EpdFontFamily::BOLD);
  }
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, selected ? 3 : 1, metrics.listRowRadius, true);
  const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, tr(STR_DESK_APPS), EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, rect.x + (rect.width - labelWidth) / 2,
                    rect.y + (rect.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2, tr(STR_DESK_APPS), true,
                    EpdFontFamily::BOLD);
}

int DeskTheme::menuRowStep(const GfxRenderer& renderer) {
  return GUI.getMenuRowHeight(renderer) + DeskMetrics::values.menuSpacing;
}

Rect DeskTheme::homeMenuPanelRect(const GfxRenderer& renderer, const int rowCount) {
  const Rect button = homeMenuButtonRect(renderer);
  const int rowsHeight = rowCount * menuRowStep(renderer) - DeskMetrics::values.menuSpacing;
  return Rect{button.x + button.width - PANEL_WIDTH, button.y + button.height + PANEL_GAP, PANEL_WIDTH,
              rowsHeight + 2 * PANEL_PADDING};
}

Rect DeskTheme::homeMenuRowsRect(const Rect panel) {
  return Rect{panel.x + PANEL_PADDING, panel.y + PANEL_PADDING, panel.width - 2 * PANEL_PADDING,
              panel.height - 2 * PANEL_PADDING};
}

void DeskTheme::drawHomeMenuPanel(GfxRenderer& renderer, const Rect panel, const int rowCount, const int selectedIndex,
                                  const std::function<std::string(int)>& label,
                                  const std::function<UIIcon(int)>& icon) {
  const auto& metrics = DeskMetrics::values;
  // Opaque panel with the theme's popup frame; Home stays visible around it.
  renderer.fillRoundedRect(panel.x, panel.y, panel.width, panel.height, metrics.listRowRadius, Color::White);
  renderer.drawRoundedRect(panel.x, panel.y, panel.width, panel.height, metrics.popupFrameThickness,
                           metrics.listRowRadius, true);
  // drawButtonMenu insets rows by contentSidePadding on each side only; widen
  // its rect by that much so the rows fill homeMenuRowsRect exactly.
  const Rect rows = homeMenuRowsRect(panel);
  GUI.drawButtonMenu(
      renderer,
      Rect{rows.x - metrics.contentSidePadding, rows.y, rows.width + 2 * metrics.contentSidePadding, rows.height},
      rowCount, selectedIndex, label, icon);
}
