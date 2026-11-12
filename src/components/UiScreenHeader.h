#pragma once

#include "UiAppHost.h"

// Configure a fresh header with shared theme/status chrome and return its
// bezel-adjusted band. Callers own actions, optional styles and drawing.
freeink::ui::Rect prepareUiHeader(UiAppHost::UiScreen& screen, const GfxRenderer& renderer,
                                  freeink::ui::HeaderProps& header, const char* title,
                                  freeink::ui::ActionId backAction = freeink::ui::NO_ACTION);
