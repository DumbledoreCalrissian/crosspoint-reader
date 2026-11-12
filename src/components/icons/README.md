Lucide and Feather icons keep their original licenses.
The licenses and copyright notices are in `freeink-sdk/libs/assets/Icons/lucide/LICENSE` at the repository root.
Keep those notices with copies of those icons, including converted bitmaps.

## Todo checklist

`todo.h` comes from the pinned SDK's Lucide `list-todo.svg`. Regenerate from the
firmware root (Homebrew `librsvg` and Pillow in the workspace venv are required):

```sh
rsvg-convert -w 32 -h 32 freeink-sdk/libs/assets/Icons/lucide/icons/list-todo.svg -o ../qa-artifacts/todo-icon-source.png
../.venv/bin/python scripts/convert_icon.py ../qa-artifacts/todo-icon-source.png todo 32 32
```

The legacy converter pre-rotates the mask to match `GfxRenderer::drawIcon`.
Do not substitute the SDK's unrotated UI bitmap. Home uses this icon only in
themes that already show menu icons; label-only themes keep their native layout.

## Pomodoro timer

`pomodoro.h` uses the same licensed Lucide pipeline with `timer.svg`:

```sh
rsvg-convert -w 32 -h 32 freeink-sdk/libs/assets/Icons/lucide/icons/timer.svg -o ../qa-artifacts/pomodoro-icon-source.png
../.venv/bin/python scripts/convert_icon.py ../qa-artifacts/pomodoro-icon-source.png pomodoro 32 32
```
