# Todo

Open **Todo** on Home (in the Desk theme, from the menu button on Home), then **New task**. Type a description and tap **OK**.

Desk Home also shows the first three unfinished tasks beside the current book.
Tap anywhere in that widget to open Todo. It is hidden when there are no open
tasks or the saved tasks cannot be read. Buttons continue to select books and
the menu button.

- Tap a checkbox to complete or reopen a task.
- Tap its description to edit it. Long descriptions wrap or truncate in the list;
  editing shows the full text.
- Tap the header bin to select tasks for deletion, then **Delete selected**
  and confirm. Back cancels the selection.
- The header arrow returns to Home. The bottom-edge upward Home gesture also works.

Changes save immediately. Todo works offline and supports up to 32 tasks,
with 192 UTF-8 bytes per description. Blank or invalid text is rejected.
Swipe to scroll longer lists.

## Save errors and recovery

If saving fails, the previous tasks remain visible. Resolve card access and retry.
Saves are `/.crosspoint/todo-a.json` and `/.crosspoint/todo-b.json`; back up both.

If no intact save survives damaged data, editing is disabled and files are kept.
Exit Todo, back up the saves and restore an intact copy. To deliberately start
empty, move both JSON files out of `/.crosspoint/` and reopen Todo. Leave other
files in that directory alone: they include settings, reading positions and Sudoku.
