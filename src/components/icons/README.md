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

## Sudoku

`sudoku.h` uses the same licensed Lucide pipeline with `grid-3x3.svg`:

```sh
rsvg-convert -w 32 -h 32 freeink-sdk/libs/assets/Icons/lucide/icons/grid-3x3.svg -o ../qa-artifacts/sudoku-icon-source.png
../.venv/bin/python scripts/convert_icon.py ../qa-artifacts/sudoku-icon-source.png sudoku 32 32
./bin/clang-format-fix -g
```

## Desk reading stats

`clock.h` and `pages.h` use the same licensed Lucide pipeline with `clock.svg`
and `book-open.svg`, at 24 px for the Desk theme's current-book stats:

```sh
rsvg-convert -w 24 -h 24 freeink-sdk/libs/assets/Icons/lucide/icons/clock.svg -o ../qa-artifacts/clock-icon-source.png
../.venv/bin/python scripts/convert_icon.py ../qa-artifacts/clock-icon-source.png clock 24 24
rsvg-convert -w 24 -h 24 freeink-sdk/libs/assets/Icons/lucide/icons/book-open.svg -o ../qa-artifacts/pages-icon-source.png
../.venv/bin/python scripts/convert_icon.py ../qa-artifacts/pages-icon-source.png pages 24 24
```

## Weather

`weather.h` contains a 32 px Desk menu mask (`cloud-rain`), 48 px widget masks
and 64 px Weather screen masks from the pinned SDK's Lucide `sun`, `moon`,
`cloud-sun`, `cloud-moon`, `cloud`, `cloud-fog`, `cloud-rain`, `cloud-snow` and
`cloud-lightning` SVGs. Regenerate with Homebrew `librsvg` and workspace Pillow:

```sh
../.venv/bin/python scripts/generate_weather_icons.py
./bin/clang-format-fix -g
```

The generator uses the same pre-rotation as the existing native icon converter.
The original Lucide copyright and licence remain in the SDK path above.
