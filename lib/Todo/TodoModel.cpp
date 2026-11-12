#include "TodoModel.h"

#include <cstring>
#include <limits>

namespace {
bool space(unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
}  // namespace

std::string_view TodoModel::trimmed(std::string_view text) {
  while (!text.empty() && space(text.front())) text.remove_prefix(1);
  while (!text.empty() && space(text.back())) text.remove_suffix(1);
  return text;
}

bool TodoModel::validText(std::string_view text) {
  if (text.empty() || text.size() > MAX_TEXT_BYTES || trimmed(text).empty()) return false;
  for (size_t i = 0; i < text.size();) {
    const auto c = static_cast<unsigned char>(text[i++]);
    if (c < 0x80) {
      if (c < 0x20 || c == 0x7f) return false;
      continue;
    }
    int extra;
    uint32_t code;
    uint32_t minimum;
    if (c >= 0xc2 && c <= 0xdf) {
      extra = 1;
      code = c & 0x1f;
      minimum = 0x80;
    } else if (c >= 0xe0 && c <= 0xef) {
      extra = 2;
      code = c & 0x0f;
      minimum = 0x800;
    } else if (c >= 0xf0 && c <= 0xf4) {
      extra = 3;
      code = c & 7;
      minimum = 0x10000;
    } else
      return false;
    if (i + extra > text.size()) return false;
    while (extra--) {
      const auto part = static_cast<unsigned char>(text[i++]);
      if ((part & 0xc0) != 0x80) return false;
      code = (code << 6) | (part & 0x3f);
    }
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
  }
  return true;
}

const TodoModel::Task* TodoModel::find(uint32_t id) const {
  for (size_t i = 0; i < count; ++i)
    if (tasks[i].id == id) return &tasks[i];
  return nullptr;
}

TodoModel::Result TodoModel::add(std::string_view text) {
  text = trimmed(text);
  if (!validText(text)) return Result::InvalidText;
  if (count == MAX_TASKS) return Result::Full;
  if (next == std::numeric_limits<uint32_t>::max()) return Result::IdExhausted;
  auto& task = tasks[count++];
  task = {};
  task.id = next++;
  std::memmove(task.text, text.data(), text.size());
  return Result::Ok;
}

TodoModel::Result TodoModel::edit(uint32_t id, std::string_view text) {
  text = trimmed(text);
  if (!validText(text)) return Result::InvalidText;
  for (size_t i = 0; i < count; ++i) {
    auto& task = tasks[i];
    if (task.id != id) continue;
    if (text == task.text) return Result::Unchanged;
    std::memmove(task.text, text.data(), text.size());
    task.text[text.size()] = '\0';
    return Result::Ok;
  }
  return Result::NotFound;
}

TodoModel::Result TodoModel::toggle(uint32_t id) {
  for (size_t i = 0; i < count; ++i) {
    if (tasks[i].id != id) continue;
    tasks[i].completed = !tasks[i].completed;
    return Result::Ok;
  }
  return Result::NotFound;
}

TodoModel::Result TodoModel::erase(uint32_t id) {
  for (size_t i = 0; i < count; ++i) {
    if (tasks[i].id != id) continue;
    for (size_t j = i + 1; j < count; ++j) tasks[j - 1] = tasks[j];
    tasks[--count] = {};
    return Result::Ok;
  }
  return Result::NotFound;
}

bool TodoModel::appendLoaded(uint32_t id, bool completed, std::string_view text) {
  if (!id || id == UINT32_MAX || find(id) || count == MAX_TASKS || !validText(text)) return false;
  auto& task = tasks[count++];
  task = {};
  task.id = id;
  task.completed = completed;
  std::memmove(task.text, text.data(), text.size());
  if (next <= id) next = id + 1;
  return true;
}

bool TodoModel::setNextId(uint32_t id) {
  if (id < next) return false;
  next = id;
  return true;
}

uint32_t TodoModel::checksum(uint32_t generation) const {
  uint32_t hash = 2166136261u;
  auto byte = [&hash](uint8_t b) { hash = (hash ^ b) * 16777619u; };
  auto number = [&byte](uint32_t n) {
    for (int i = 0; i < 4; ++i) {
      byte(n & 0xff);
      n >>= 8;
    }
  };
  number(1);
  number(generation);
  number(next);
  number(count);
  for (size_t i = 0; i < count; ++i) {
    number(tasks[i].id);
    byte(tasks[i].completed);
    for (const char* p = tasks[i].text; *p; ++p) byte(static_cast<uint8_t>(*p));
    byte(0);
  }
  return hash;
}
