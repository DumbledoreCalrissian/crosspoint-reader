#!/usr/bin/env python3
"""Capture Todo states across every built-in theme without changing task data."""
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT.parent / 'qa-artifacts' / 'todo-native'
LOGS = ROOT.parent / 'logs'
ART.mkdir(parents=True, exist_ok=True)
PROGRAM = ROOT / '.pio/build/simulator_picco/program'
THEMES = ['classic', 'lyra', 'lyra-extended', 'roundedraff', 'cover-grid']


def seed(sd, tasks):
    data = b''.join(struct.pack('<I', x) for x in [1, 1, len(tasks) + 1, len(tasks)])
    for task in tasks:
        data += struct.pack('<I', task['id']) + bytes([task['completed']]) + task['text'].encode() + b'\0'
    checksum = 2166136261
    for byte in data:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    path = sd / '.crosspoint/todo-a.json'
    path.write_text(json.dumps(dict(version=1, generation=1, nextId=len(tasks) + 1,
                                   checksum=checksum, tasks=tasks)))
    return path.read_bytes()


def run(theme, case, tasks, *, damaged=False, hide_battery=False):
    name = f'{THEMES[theme]}-{case}'
    with tempfile.TemporaryDirectory(prefix=f'visual-{name}-', dir=ART) as directory:
        sd = Path(directory)
        (sd / '.crosspoint').mkdir()
        (sd / '.crosspoint/settings.json').write_text(json.dumps(dict(
            uiTheme=theme, clockShowHeader=1, hideBatteryPercentage=2 if hide_battery else 0)))
        saved = seed(sd, tasks)
        if damaged:
            saved = b'{broken'
            (sd / '.crosspoint/todo-a.json').write_bytes(saved)
        # Only QA entry uses the provisional Confirm key on list Home themes.
        # The dedicated simulator.py regression exercises touch-only navigation.
        entry = ('300:TAP:277,760;' if theme == 4 else
                 '300:DOWN;600:DOWN;900:DOWN;1200:ENTER;')
        home = ART / f'{name}-home.bmp'
        paths = [ART / f'{name}.bmp']
        script = f'0:WAIT:Entering activity: Home;200:SHOT:{home};' + entry + '0:WAIT:Entering activity: Todo;'
        script += f'400:SHOT:{paths[0]};'
        if tasks and not damaged:
            paths += [ART / f'{name}-delete-disabled.bmp', ART / f'{name}-delete-selected.bmp']
            script += ('700:TAP:430,60;700:WAIT:SCREEN:.*Delete tasks;'
                       f'300:SHOT:{paths[1]};600:TAP:55,140;'
                       '600:WAIT:Delete selected \\(1\\);' + f'300:SHOT:{paths[2]};600:TAP:30,60;')
        script += '1000:QUIT'
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd),
                   CROSSPOINT_SIM_SCREEN_TEXT='1', CROSSPOINT_SIM_INPUT_SCRIPT=script)
        result = subprocess.run([str(PROGRAM)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=45)
        output = result.stdout + result.stderr
        (LOGS / f'visual-{name}.log').write_text(output)
        assert result.returncode == 0, f'{name}: see log'
        assert (sd / '.crosspoint/todo-a.json').read_bytes() == saved
        assert not (sd / '.crosspoint/todo-b.json').exists(), f'{name}: unexpected save'
        for path in [home, *paths]:
            assert path.exists(), path
            subprocess.run(['/usr/bin/sips', '-s', 'format', 'png', str(path), '--out', str(path.with_suffix('.png'))],
                           check=True, capture_output=True)
        print(name, 'PASS', flush=True)


def main():
    with ThreadPoolExecutor(max_workers=3) as executor:
        jobs = []
        for theme in range(len(THEMES)):
            jobs.append(executor.submit(run, theme, 'empty', []))
            jobs.append(executor.submit(run, theme, 'tasks', [
                dict(id=1, completed=False, text='Read a chapter'),
                dict(id=2, completed=True, text='Return library books with a longer description that wraps and truncates at two lines'),
            ]))
            jobs.append(executor.submit(run, theme, 'error', [], damaged=True, hide_battery=True))
        for job in jobs:
            job.result()
    print('All five theme visual states passed; no task snapshots changed')


if __name__ == '__main__':
    main()
