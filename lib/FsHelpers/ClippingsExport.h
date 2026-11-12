#pragma once

#include <string_view>

namespace ClippingsExport {

inline constexpr char PATH[] = "/My Clippings.txt";

// The generated root export is viewable text, but not a Library/recent book.
inline bool isPath(std::string_view path) {
  while (!path.empty()) {
    if (path.front() == '/') {
      path.remove_prefix(1);
    } else if (path.starts_with("./")) {
      path.remove_prefix(2);
    } else {
      break;
    }
  }
  constexpr std::string_view name{PATH + 1};
  if (path.size() != name.size()) return false;
  const auto lower = [](const char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; };
  for (size_t i = 0; i < name.size(); ++i) {
    if (lower(path[i]) != lower(name[i])) return false;
  }
  return true;
}

}  // namespace ClippingsExport
