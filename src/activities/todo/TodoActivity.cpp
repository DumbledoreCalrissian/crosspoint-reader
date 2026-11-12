#include "TodoActivity.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <bit>
#include <cstdio>

#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "components/UiScreenHeader.h"
#include "components/icons/headerIcons.h"
#include "components/icons/listIcons.h"

namespace fui = freeink::ui;

void TodoActivity::onEnter() {
  UiListActivity::onEnter();
  {
    RenderLock lock(*this);
    status = store.load();
  }
  app.on(ACTION_BACK, action, this);
  app.on(ACTION_CHECK, action, this);
  app.on(ACTION_DELETE_MODE, action, this);
  app.on(ACTION_DELETE_SELECTED, action, this);
}

int TodoActivity::listCount() const { return static_cast<int>(store.model().size()) + (view == View::Tasks ? 1 : 0); }

unsigned TodoActivity::selectedCount() const { return std::popcount(selection); }

void TodoActivity::action(const fui::ActionEvent& event, void* context) {
  auto* self = static_cast<TodoActivity*>(context);
  self->app.clearTapFlash();
  switch (event.action) {
    case ACTION_BACK:
      self->onBackButton();
      break;
    case ACTION_CHECK:
      self->toggleTask(event.value - 1);
      break;
    case ACTION_DELETE_MODE:
      self->setView(View::DeleteSelection);
      break;
    case ACTION_DELETE_SELECTED:
      self->confirmDelete();
      break;
  }
}

void TodoActivity::setView(View next) {
  {
    RenderLock lock(*this);
    closeRouting();
    app.clearTapFlash();
    view = next;
    selection = 0;
    nav.reset();
  }
  requestUpdate();
}

void TodoActivity::onBackButton() {
  switch (view) {
    case View::Tasks:
      activityManager.goHome(HomeMenuItem::TODO);
      break;
    case View::DeleteSelection:
      setView(View::Tasks);
      break;
  }
}

void TodoActivity::provideRow(void* context, uint16_t index, fui::ListItem& item) {
  const auto& self = *static_cast<TodoActivity*>(context);
  item.actionValue = static_cast<int16_t>(index);
  item.enabled = self.store.canEdit();
  if (self.view == View::Tasks && index == 0) {
    item.label = tr(STR_TODO_NEW);
    return;
  }
  const auto taskIndex = self.view == View::Tasks ? index - 1 : index;
  const auto* task = self.store.model().at(taskIndex);
  if (!task) return;
  item.label = task->text;
  item.checkbox = true;
  item.checked = self.view == View::Tasks ? task->completed : (self.selection & (uint32_t{1} << taskIndex));
  item.strikethrough = self.view == View::Tasks && task->completed;
}

void TodoActivity::buildHeader(UiScreen& screen) {
  fui::HeaderProps header;
  const auto band = prepareUiHeader(screen, renderer, header,
                                    view == View::Tasks ? tr(STR_TODO) : tr(STR_TODO_DELETE_TITLE), ACTION_BACK);
  if (view == View::Tasks) {
    header.trailingIcon = fui::bitmapFromIcon(icon_trash_2_32);
    header.trailingAction = ACTION_DELETE_MODE;
    header.trailingEnabled = store.canEdit() && store.model().size();
  }
  fui::header(screen.frame(), band, header);
}

void TodoActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& theme = screen.theme();
  buildHeader(screen);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0, 0, 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (view == View::DeleteSelection) {
    std::snprintf(deleteLabel, sizeof(deleteLabel), tr(STR_TODO_DELETE_SELECTED), selectedCount());
    fui::ButtonProps button;
    button.label = deleteLabel;
    button.action = ACTION_DELETE_SELECTED;
    button.enabled = selection != 0 && store.canEdit();
    const int16_t inset = static_cast<int16_t>(theme.spaceLg * 2);
    const auto rect = screen.takeBottom(theme.rowHeight, theme.spaceLg).inset(fui::Insets{0, inset, 0, inset});
    screen.button(button, rect);
  }
  if (const char* error = statusText()) {
    fui::TextAreaProps text;
    text.text = error;
    text.showCaret = false;
    text.style = screen.theme().smallText;
    text.style.maxLines = 3;
    const auto inset = static_cast<int16_t>(theme.listInset + theme.listSidePadding);
    const auto band = screen.takeTop(static_cast<int16_t>(uiTarget.lineHeight(text.style.font) * 3));
    fui::textArea(screen.frame(), band.inset(fui::Insets{0, inset, 0, inset}), text);
  }

  fui::ListProps props;
  props.count = static_cast<uint16_t>(listCount());
  props.rowProvider = provideRow;
  props.rowProviderCtx = this;
  props.action = ACTION_ROW;
  props.checkboxAction = view == View::Tasks ? ACTION_CHECK : fui::NO_ACTION;
  props.checkboxWidth = std::max<int16_t>(48, screen.theme().minTouchSize);
  props.inputMask = fui::InputTouch;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 2;
  props = screen.resolveListProps(props);
  // Preserve native row sizing while bounding the two-hit task rows to the
  // shared host's 24 interactions, including header actions and New task.
  if (view == View::Tasks && listCount() > 10) {
    const int16_t maxHeight = static_cast<int16_t>(props.rowHeight * 10 + props.rowGap * 9);
    if (screen.body().height > maxHeight) screen.takeBottom(screen.body().height - maxHeight);
  }
  syncListViewport(screen, props);
  if (!store.model().size()) {
    screen.list(props, static_cast<int16_t>(props.rowHeight + props.rowGap));
    if (status == TodoStore::Status::Ok) screen.centeredText(tr(STR_TODO_EMPTY));
  } else {
    screen.list(props);
  }
}

void TodoActivity::toggleTask(int index) {
  {
    RenderLock lock(*this);
    if (view != View::Tasks) return;
    const auto* task = store.model().at(index);
    if (!task) return;
    nav.selected = index + 1;
    status = store.change(TodoStore::Change::Toggle, task->id);
  }
  requestUpdate();
}

void TodoActivity::activateIndex(int index) {
  app.clearTapFlash();
  switch (view) {
    case View::Tasks: {
      if (index == 0) {
        enterText(0);
        return;
      }
      const auto* task = store.model().at(index - 1);
      if (task) enterText(task->id);
      return;
    }
    case View::DeleteSelection: {
      if (index < 0 || !store.model().at(index)) return;
      {
        RenderLock lock(*this);
        selection ^= uint32_t{1} << index;
      }
      requestUpdate();
      return;
    }
  }
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
    nav.requestSelection(static_cast<int>(store.model().size()));
  }
}

void TodoActivity::confirmDelete() {
  if (view != View::DeleteSelection || !selection) return;
  char body[64];
  std::snprintf(body, sizeof(body), tr(STR_TODO_DELETE_SELECTED), selectedCount());
  auto confirmation = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_TODO_DELETE_CONFIRM), body,
                                                              tr(STR_CANCEL), tr(STR_TODO_DELETE));
  if (!confirmation) {
    LOG_ERR("TODO", "Confirmation allocation failed: %u bytes", static_cast<unsigned>(sizeof(ConfirmationActivity)));
    {
      RenderLock lock(*this);
      status = TodoStore::Status::OutOfMemory;
    }
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(confirmation), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    bool deleted;
    {
      RenderLock lock(*this);
      uint32_t ids[TodoModel::MAX_TASKS];
      size_t count = 0;
      for (size_t i = 0; i < store.model().size(); ++i)
        if (selection & (uint32_t{1} << i)) ids[count++] = store.model().at(i)->id;
      status = store.eraseTasks(ids, count);
      deleted = status == TodoStore::Status::Ok;
    }
    if (deleted) setView(View::Tasks);
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
