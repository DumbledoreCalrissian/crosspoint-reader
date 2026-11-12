#!/usr/bin/env python3
"""Compare Todo's actual selected-row paint/focus against native Settings."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

from PIL import Image
from visuals import ROOT, PROGRAM, LOGS, seed

ART = ROOT.parent / 'qa-artifacts/todo-native-reference'
ART.mkdir(parents=True, exist_ok=True)
# Fixed Picco geometry/current font tier: native category and task-row centers.
THEMES = [(0, 'classic', 135, 127, 189), (1, 'lyra', 146, 138, 200),
          (3, 'roundedraff', 143, 135, 197)]


def capture(name, theme, script, tasks=None):
    with tempfile.TemporaryDirectory(prefix=name + '-', dir=ART) as directory:
        sd = Path(directory)
        (sd / '.crosspoint').mkdir()
        (sd / '.crosspoint/settings.json').write_text(json.dumps(dict(uiTheme=theme)))
        if tasks is not None:
            seed(sd, tasks)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd),
                   CROSSPOINT_SIM_SCREEN_TEXT='1', CROSSPOINT_SIM_INPUT_SCRIPT=script)
        result = subprocess.run([str(PROGRAM)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=45)
        output = result.stdout + result.stderr
        (LOGS / f'native-reference-{name}.log').write_text(output)
        assert result.returncode == 0, f'{name}: see log'
        if tasks is not None:
            snapshots = [json.loads(p.read_text()) for p in (sd / '.crosspoint').glob('todo-?.json')]
            latest = max(snapshots, key=lambda doc: doc['generation'])
            assert latest['generation'] == 2
            assert latest['tasks'] == [dict(id=1, completed=True, text='Read a chapter'),
                                       dict(id=2, completed=False, text='Walk')]
            # Confirm is used only as a QA probe of focus after checkbox activation.
            assert 'Edit task | Read a chapter' in output
            assert sum('[ACT] Entering activity: KeyboardEntry' in line and not line.startswith('[SIM]') for line in output.splitlines()) == 1


def ink(path, y):
    with Image.open(path) as image:
        assert image.size == (480, 800)
        patch = image.convert('RGB').crop((346, y - 4, 354, y + 4))
        return sum(sum(pixel) < 384 for pixel in (patch.getpixel((x, y)) for y in range(8) for x in range(8)))


for theme, name, native_y, new_y, task_y in THEMES:
    reference = ART / f'{name}-settings.bmp'
    before = ART / f'{name}-todo-before.bmp'
    focused = ART / f'{name}-todo-focused.bmp'
    capture(name + '-settings', theme,
            '0:WAIT:Entering activity: Home;300:DOWN;600:DOWN;900:DOWN;1200:ENTER;'
            f'1200:WAIT:Entering activity: SettingsList;400:SHOT:{reference};700:QUIT')
    capture(name + '-todo', theme,
            '0:WAIT:Entering activity: Home;300:DOWN;600:DOWN;900:DOWN;1200:DOWN;1500:ENTER;'
            f'1500:WAIT:Entering activity: Todo;400:SHOT:{before};700:TAP:55,{task_y};'
            f'1200:SHOT:{focused};1500:ENTER;1500:WAIT:Entering activity: KeyboardEntry;'
            '300:TAP:30,64;700:QUIT',
            [dict(id=1, completed=False, text='Read a chapter'), dict(id=2, completed=False, text='Walk')])
    expected = ink(reference, native_y)
    assert expected > 0, name
    assert ink(before, new_y) == expected, f'{name}: initial focus differs from native'
    assert ink(focused, task_y) == expected, f'{name}: checkbox focus differs from native'
    assert ink(focused, new_y) == 0, f'{name}: stale focus remains on New task'
    for path in (reference, before, focused):
        with Image.open(path) as image:
            image.save(path.with_suffix('.png'))
    print(f'{name}: native selection paint and checkbox focus match ({expected}/64 ink pixels)', flush=True)
print('Native appearance comparisons passed; checkbox focus and keyboard target verified')
