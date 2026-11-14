#include "DeskTheme.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int MENU_BUTTON_SIZE = 44;  // touch minimum
constexpr int BAR_HEIGHT = 3;
constexpr int BAR_GAP = 6;
constexpr int BAR_WIDTH = 22;
constexpr int PANEL_WIDTH = 280;
constexpr int PANEL_GAP = 4;      // between button and panel
constexpr int PANEL_PADDING = 8;  // inside the frame
}  // namespace

void DeskTheme::drawRecentBookCover(GfxRenderer& renderer, const Rect rect, const std::vector<RecentBook>& recentBooks,
                                    const BookReadingSummary* readingSummaries, const int selectorIndex,
                                    bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                                    std::function<bool()> storeCoverBuffer) const {
  if (!recentBooks.empty()) {
    Lyra3CoversTheme::drawRecentBookCover(renderer, rect, recentBooks, readingSummaries, selectorIndex, coverRendered,
                                          coverBufferStored, bufferRestored, std::move(storeCoverBuffer));
    return;
  }
  // Lyra's empty text points "below", where Desk has no list.
  constexpr int padding = 48;
  renderer.drawText(UI_12_FONT_ID, rect.x + padding,
                    rect.y + rect.height / 2 - renderer.getLineHeight(UI_12_FONT_ID) - 2, tr(STR_NO_OPEN_BOOK), true,
                    EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, rect.x + padding, rect.y + rect.height / 2 + 2, tr(STR_DESK_START_READING), true);
}

Rect DeskTheme::homeMenuButtonRect(const GfxRenderer& renderer) {
  const auto& metrics = DeskMetrics::values;
  const Rect content = UITheme::getContentArea(renderer);
  const int y = metrics.homeTopPadding - metrics.verticalSpacing - MENU_BUTTON_SIZE;
  return Rect{content.x + content.width - metrics.contentSidePadding - MENU_BUTTON_SIZE, y, MENU_BUTTON_SIZE,
              MENU_BUTTON_SIZE};
}

void DeskTheme::drawHomeMenuRow(const GfxRenderer& renderer, const Rect rect, const bool selected,
                                const bool hasBooks) {
  const auto& metrics = DeskMetrics::values;
  if (hasBooks) {
    const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    renderer.drawText(UI_12_FONT_ID, UITheme::getContentArea(renderer).x + metrics.contentSidePadding, textY,
                      tr(STR_CONTINUE_READING), true, EpdFontFamily::BOLD);
  }
  if (selected) {
    renderer.fillRoundedRect(rect.x, rect.y, rect.width, rect.height, metrics.listRowRadius, Color::LightGray);
  }
  const int x = rect.x + (rect.width - BAR_WIDTH) / 2;
  const int top = rect.y + (rect.height - (3 * BAR_HEIGHT + 2 * BAR_GAP)) / 2;
  for (int i = 0; i < 3; ++i) {
    renderer.fillRect(x, top + i * (BAR_HEIGHT + BAR_GAP), BAR_WIDTH, BAR_HEIGHT, true);
  }
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
  GUI.drawButtonMenu(renderer, homeMenuRowsRect(panel), rowCount, selectedIndex, label, icon);
}
