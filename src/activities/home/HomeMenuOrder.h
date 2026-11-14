#pragma once

#include "activities/ActivityManager.h"

// Home shortcut order, shared by the list Home and the Desk menu panel.
// slot: Plugins, OPDS or NONE (no slot row). NONE past the last item.
inline HomeMenuItem homeMenuItemAt(const int idx, const HomeMenuItem slot) {
  int i = 0;
  if (idx == i++) return HomeMenuItem::FILE_BROWSER;
  if (idx == i++) return HomeMenuItem::LIBRARY;
  if (slot != HomeMenuItem::NONE && idx == i++) return slot;
  if (idx == i++) return HomeMenuItem::SETTINGS_MENU;
  if (idx == i++) return HomeMenuItem::TODO;
  if (idx == i++) return HomeMenuItem::POMODORO;
  if (idx == i) return HomeMenuItem::SUDOKU;
  return HomeMenuItem::NONE;
}
