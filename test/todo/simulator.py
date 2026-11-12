#!/usr/bin/env python3
"""Offline touch workflow against the real firmware and host-backed SD."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
artifacts = root.parent / 'qa-artifacts'
logs = root.parent / 'logs'
artifacts.mkdir(exist_ok=True)
logs.mkdir(exist_ok=True)
program = root / '.pio/build/simulator_picco/program'

def run(name, steps, sd):
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd),
               CROSSPOINT_SIM_SCREEN_TEXT='1', CROSSPOINT_SIM_INPUT_SCRIPT=steps)
    result = subprocess.run([str(program)], cwd=root, env=env, capture_output=True, text=True, timeout=45)
    (logs / f'todo-ui-{name}.log').write_text(result.stdout + result.stderr)
    assert result.returncode == 0, f'{name}: simulator failed; see log'
    assert 'WAIT timed out' not in result.stdout
    return result.stdout

def latest(sd):
    snapshots = [json.loads(p.read_text()) for p in (sd / '.crosspoint').glob('todo-?.json')]
    return max(snapshots, key=lambda doc: doc['generation']) if snapshots else None

home = '0:WAIT:Entering activity: Home;400:TAP:180,635;400:WAIT:Entering activity: Todo;'
with tempfile.TemporaryDirectory(prefix='todo-workflow-', dir=artifacts) as directory:
    sd = Path(directory)
    run('create', home + '300:WAIT:SCREEN:.*No tasks yet;400:TAP:180,130;400:WAIT:Entering activity: KeyboardEntry;'
        '300:TAP:216,568;650:TAP:56,632;1000:TAP:100,632;1350:TAP:380,632;1750:TAP:420,754;'
        '1750:WAIT:SCREEN:.*task.*Open;300:SHOT:' + str(artifacts/'todo-created.bmp') + ';600:QUIT', sd)
    assert latest(sd)['tasks'] == [{'id': 1, 'completed': False, 'text': 'task'}]
    run('edit-toggle', home + '300:WAIT:SCREEN:.*task.*Open;400:TAP:180,255;400:WAIT:SCREEN:.*Mark complete;'
        '400:TAP:180,255;400:WAIT:Entering activity: KeyboardEntry;300:TAP:52,568;700:TAP:420,754;'
        '700:WAIT:SCREEN:.*taskq.*Mark complete;400:TAP:180,193;400:WAIT:SCREEN:.*Mark incomplete;300:QUIT', sd)
    assert latest(sd)['tasks'] == [{'id': 1, 'completed': True, 'text': 'taskq'}]
    before = latest(sd)
    run('cancel-edit-delete', home + '300:WAIT:SCREEN:.*taskq.*Done;400:TAP:180,255;400:WAIT:SCREEN:.*Mark incomplete;'
        '400:TAP:180,255;400:WAIT:Entering activity: KeyboardEntry;300:TAP:52,568;700:TAP:30,64;'
        '700:WAIT:SCREEN:.*taskq.*Mark incomplete;400:TAP:180,317;400:WAIT:Entering activity: Confirmation;'
        '400:TAP:240,412;400:WAIT:SCREEN:.*taskq.*Mark incomplete;400:TAP:30,50;800:TAP:30,50;'
        '800:WAIT:Entering activity: Home;300:QUIT', sd)
    assert latest(sd) == before
    run('cancel-create', home + '300:TAP:180,130;300:WAIT:Entering activity: KeyboardEntry;'
        '300:TAP:52,568;700:TAP:30,64;700:WAIT:SCREEN:.*taskq.*Done;300:QUIT', sd)
    assert latest(sd) == before
    run('reject-empty', home + '300:TAP:180,130;300:WAIT:Entering activity: KeyboardEntry;'
        '300:TAP:420,754;300:WAIT:SCREEN:.*UTF-8 bytes;300:QUIT', sd)
    assert latest(sd) == before
    run('delete', home + '300:TAP:180,255;300:WAIT:SCREEN:.*Mark incomplete;'
        '300:TAP:180,317;300:WAIT:Entering activity: Confirmation;300:TAP:240,475;'
        '300:WAIT:SCREEN:.*No tasks yet;300:QUIT', sd)
    assert latest(sd)['tasks'] == []
    run('restart-empty', home + '300:WAIT:SCREEN:.*No tasks yet;300:QUIT', sd)
print('Todo simulator touch workflows passed: create, edit, toggle, cancel, reject empty, delete, restart, Home')
