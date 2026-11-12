#pragma once
#include <TodoStore.h>

#include "activities/UiListActivity.h"

class TodoActivity final : public UiListActivity {
 public:
  TodoActivity(GfxRenderer& renderer, MappedInputManager& input) : UiListActivity("Todo", renderer, input) {}
  void onEnter() override;

 private:
  TodoStore store;
  TodoStore::Status status = TodoStore::Status::Ok;
  const char* statusText() const;
  enum class View { Tasks, Actions };
  View view = View::Tasks;
  uint32_t selectedId = 0;
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  void enterText(uint32_t id);
  void acceptText(uint32_t id, const std::string& text);
  void confirmDelete();
  void drawChrome() override {}
  void drawFooter() override {}
  static void provideRow(void* context, uint16_t index, freeink::ui::ListItem& item);
  static void backAction(const freeink::ui::ActionEvent&, void* context);
};
