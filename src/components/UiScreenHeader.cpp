#include "UiScreenHeader.h"

#include <FreeInkUIIcon.h>

#include "UITheme.h"
#include "icons/headerIcons.h"

namespace fui = freeink::ui;

fui::Rect prepareUiHeader(UiAppHost::UiScreen& screen, const GfxRenderer& renderer, fui::HeaderProps& header,
                          const char* title, const fui::ActionId backAction) {
  const auto& theme = screen.theme();
  header.title = title;
  header.titleText = theme.titleText;
  header.titleText.align = theme.headerTitleAlign;
  header.sidePadding = theme.headerSidePadding;
  header.minTouchSize = theme.minTouchSize;
  header.styles = theme.popup;
  if (header.styles.normal.border.kind == fui::PaintKind::None && theme.headerUnderline > 0) {
    header.styles.normal.border = fui::Paint::solid(fui::Color::Black);
    header.styles.normal.borderWidth = theme.headerUnderline;
  }
  header.borderEdges = fui::EdgeBottom;
  header.trailingStyles = fui::plainStyles(fui::Paint::solid(fui::Color::Black));
  GUI.applyHeaderStatus(renderer, header);
  if (backAction != fui::NO_ACTION) {
    header.leadingIcon = fui::bitmapFromIcon(icon_header_back_32);
    header.leadingAction = backAction;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto frame = screen.frame().screen();
  return BaseTheme::headerBand(renderer,
                               fui::Rect{frame.x, static_cast<int16_t>(metrics.topPadding), frame.width,
                                         static_cast<int16_t>(metrics.headerHeight)},
                               header);
}
