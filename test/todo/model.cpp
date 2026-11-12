#include <TodoModel.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
int main() {
  auto m = std::make_unique<TodoModel>();
  using R = TodoModel::Result;
  assert(m->size() == 0 && !m->at(0));
  assert(m->add("") == R::InvalidText);
  assert(m->add(" \t\n") == R::InvalidText);
  assert(m->add(std::string(193, 'a')) == R::InvalidText);
  assert(m->add(std::string(192, 'a')) == R::Ok);
  assert(m->edit(1, "  Café 日本語 😀 & \"quoted\" ") == R::Ok);
  assert(std::string(m->at(0)->text) == "Café 日本語 😀 & \"quoted\"");
  assert(m->edit(1, m->at(0)->text) == R::Unchanged);
  assert(m->edit(1, "") == R::InvalidText);
  assert(m->toggle(1) == R::Ok && m->find(1)->completed);
  assert(m->toggle(1) == R::Ok && !m->find(1)->completed);
  assert(m->add(std::string("a\0b", 3)) == R::InvalidText);
  assert(m->add("\xc0\x80") == R::InvalidText);
  assert(m->add("\xed\xa0\x80") == R::InvalidText);
  assert(m->add("\xf4\x90\x80\x80") == R::InvalidText);
  assert(m->add("\xe3\x81") == R::InvalidText);
  for (int i = 1; i < 32; ++i) assert(m->add("task") == R::Ok);
  assert(m->add("one too many") == R::Full);
  assert(m->erase(1) == R::Ok && !m->find(1));
  assert(m->add("new") == R::Ok && m->at(31)->id == 33);
  assert(m->erase(999) == R::NotFound);
  assert(m->edit(999, "test") == R::NotFound);
  assert(m->toggle(999) == R::NotFound);
  assert(!m->appendLoaded(2, false, "duplicate"));
  assert(!m->setNextId(1));
  std::cout << "Todo model checks passed; model bytes=" << sizeof(TodoModel) << '\n';
}
