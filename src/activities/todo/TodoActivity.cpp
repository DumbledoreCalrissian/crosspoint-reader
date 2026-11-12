#include "TodoActivity.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>
#include <Memory.h>

#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "components/icons/headerIcons.h"
namespace fui = freeink::ui;

void TodoActivity::onEnter() {
  UiListActivity::onEnter();
  {
    RenderLock lock(*this);
    status = store.load();
  }
  app.on(ACTION_USER, backAction, this);
}
int TodoActivity::listCount() const { return view == View::Tasks ? static_cast<int>(store.model().size()) + 2 : 4; }
void TodoActivity::backAction(const fui::ActionEvent&, void* context) {
  auto* self = static_cast<TodoActivity*>(context);
  self->app.clearTapFlash();
  self->onBackButton();
}
void TodoActivity::onBackButton() {
  if (view == View::Tasks) {
    activityManager.goHome(HomeMenuItem::TODO);
    return;
  }
  {
    RenderLock lock(*this);
    closeRouting();
    view = View::Tasks;
    nav.reset();
  }
  requestUpdate();
}
void TodoActivity::provideRow(void* context, uint16_t index, fui::ListItem& item) {
  const auto& self = *static_cast<TodoActivity*>(context);
  item.actionValue = static_cast<int16_t>(index);
  if (self.view == View::Tasks) {
    if (index == 0) {
      item.label = tr(STR_TODO_NEW);
      item.enabled = self.store.canEdit();
      return;
    }
    if (index == 1) {
      item.label = self.status != TodoStore::Status::Ok
                       ? self.statusText()
                       : (self.store.model().size() ? tr(STR_TODO_HINT) : tr(STR_TODO_EMPTY));
      item.enabled = false;
      return;
    }
    const auto* task = self.store.model().at(index - 2);
    if (!task) return;
    item.label = task->text;
    item.value = (task->completed ? tr(STR_TODO_DONE) : tr(STR_TODO_OPEN));
    return;
  }
  item.enabled = self.store.canEdit();
  const auto* task = self.store.model().find(self.selectedId);
  if (!task) return;
  switch (index) {
    case 0:
      item.label = task->text;
      item.subtitle = self.statusText();
      item.enabled = false;
      break;
    case 1:
      item.label = (task->completed ? tr(STR_TODO_REOPEN) : tr(STR_TODO_COMPLETE));
      break;
    case 2:
      item.label = tr(STR_TODO_EDIT);
      break;
    case 3:
      item.label = tr(STR_TODO_DELETE);
      break;
  }
}
void TodoActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding), 0, 0, 0});
  screen.navHeader(tr(STR_TODO), ACTION_USER, fui::bitmapFromIcon(icon_header_back_32));
  screen.spacer(screen.theme().spaceMd);
  fui::ListProps props;
  props.count = static_cast<uint16_t>(listCount());
  props.rowProvider = provideRow;
  props.rowProviderCtx = this;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = view == View::Tasks ? 2 : 4;
  syncListViewport(screen, props);
  screen.list(props);
}
void TodoActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (view == View::Tasks && index == 0) {
    enterText(0);
    return;
  }
  if (view == View::Actions && index == 2) {
    enterText(selectedId);
    return;
  }
  if (view == View::Actions && index == 3) {
    confirmDelete();
    return;
  }
  {
    RenderLock lock(*this);
    if (view == View::Tasks) {
      if (index < 2) return;
      const auto* task = store.model().at(index - 2);
      if (!task) return;
      selectedId = task->id;
      closeRouting();
      view = View::Actions;
      nav.reset();
      nav.selected = 1;
    } else if (index == 1)
      status = store.change(TodoStore::Change::Toggle, selectedId);
  }
  requestUpdate();
}

void TodoActivity::enterText(uint32_t id) {
  if (!id && store.model().size() == TodoModel::MAX_TASKS) {
    {
      RenderLock lock(*this);
      status = TodoStore::Status::Full;
    }
    requestUpdate();
    return;
  }
  const auto* task = store.model().find(id);
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, id ? tr(STR_TODO_EDIT) : tr(STR_TODO_NEW),
                                               task ? task->text : "", TodoModel::MAX_TEXT_BYTES);
  if (!keyboard) {
    LOG_ERR("TODO", "Keyboard allocation failed: %u bytes", static_cast<unsigned>(sizeof(KeyboardEntryActivity)));
    {
      RenderLock lock(*this);
      status = TodoStore::Status::OutOfMemory;
    }
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(keyboard), [this, id](const ActivityResult& result) {
    if (result.isCancelled) return;
    if (const auto* entry = std::get_if<KeyboardResult>(&result.data)) acceptText(id, entry->text);
  });
}

void TodoActivity::acceptText(uint32_t id, const std::string& text) {
  RenderLock lock(*this);
  status = store.change(id ? TodoStore::Change::Edit : TodoStore::Change::Add, id, text);
  if (status == TodoStore::Status::Ok && !id) {
    closeRouting();
    nav.requestSelection(static_cast<int>(store.model().size()) + 1);
  }
}

void TodoActivity::confirmDelete() {
  const auto* task = store.model().find(selectedId);
  if (!task) return;
  auto confirmation = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_TODO_DELETE_CONFIRM),
                                                              task->text, tr(STR_CANCEL), tr(STR_TODO_DELETE));
  if (!confirmation) {
    LOG_ERR("TODO", "Confirmation allocation failed: %u bytes", static_cast<unsigned>(sizeof(ConfirmationActivity)));
    {
      RenderLock lock(*this);
      status = TodoStore::Status::OutOfMemory;
    }
    requestUpdate();
    return;
  }
  const uint32_t id = selectedId;
  startActivityForResult(std::move(confirmation), [this, id](const ActivityResult& result) {
    if (result.isCancelled) return;
    RenderLock lock(*this);
    status = store.change(TodoStore::Change::Delete, id);
    if (status == TodoStore::Status::Ok) {
      closeRouting();
      view = View::Tasks;
      nav.reset();
    }
  });
}

const char* TodoActivity::statusText() const {
  switch (status) {
    case TodoStore::Status::Ok:
      return nullptr;
    case TodoStore::Status::InvalidText:
      return tr(STR_TODO_INVALID_TEXT);
    case TodoStore::Status::Full:
      return tr(STR_TODO_FULL);
    case TodoStore::Status::StorageError:
      return tr(STR_TODO_SAVE_ERROR);
    case TodoStore::Status::InvalidData:
      return tr(STR_TODO_DATA_ERROR);
    case TodoStore::Status::OutOfMemory:
      return tr(STR_TODO_MEMORY_ERROR);
  }
  return nullptr;
}
