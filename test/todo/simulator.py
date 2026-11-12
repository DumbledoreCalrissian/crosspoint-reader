#!/usr/bin/env python3
"""Offline touch workflows against real firmware and host-backed SD."""
import json
import os
from pathlib import Path
import struct
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
    output = result.stdout + result.stderr
    (logs / f'checkbox-ui-{name}.log').write_text(output)
    assert result.returncode == 0, f'{name}: simulator failed; see log'
    assert 'WAIT timed out' not in output
    assert 'Tap a task for actions' not in output
    return output


def latest(sd):
    snapshots = [json.loads(p.read_text()) for p in (sd / '.crosspoint').glob('todo-?.json')]
    return max(snapshots, key=lambda doc: doc['generation']) if snapshots else None


def seed(sd, tasks):
    data = b''.join(struct.pack('<I', x) for x in [1, 1, len(tasks) + 1, len(tasks)])
    for task in tasks:
        data += struct.pack('<I', task['id']) + bytes([task['completed']]) + task['text'].encode() + b'\0'
    checksum = 2166136261
    for byte in data:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    (sd / '.crosspoint').mkdir(exist_ok=True)
    (sd / '.crosspoint/todo-a.json').write_text(json.dumps(dict(
        version=1, generation=1, nextId=len(tasks) + 1, checksum=checksum, tasks=tasks)))


home = '0:WAIT:Entering activity: Home;400:TAP:180,562;400:WAIT:Entering activity: Todo;'
with tempfile.TemporaryDirectory(prefix='checkbox-workflow-', dir=artifacts) as directory:
    sd = Path(directory)
    run('create', home + '300:WAIT:SCREEN:.*No tasks yet;400:TAP:180,130;400:WAIT:Entering activity: KeyboardEntry;'
        '300:TAP:216,568;650:TAP:56,632;1000:TAP:100,632;1350:TAP:380,632;1750:TAP:420,754;'
        '1750:WAIT:SCREEN:.*Todo.*task;300:QUIT', sd)
    assert latest(sd)['tasks'] == [{'id': 1, 'completed': False, 'text': 'task'}]
    run('edit', home + '300:TAP:180,210;300:WAIT:Entering activity: KeyboardEntry;'
        '300:TAP:52,568;700:TAP:420,754;700:WAIT:SCREEN:.*Todo.*taskq;300:QUIT', sd)
    assert latest(sd)['tasks'] == [{'id': 1, 'completed': False, 'text': 'taskq'}]
    generation = latest(sd)['generation']
    output = run('complete', home + '300:TAP:55,210;700:SHOT:' + str(artifacts/'todo-checkbox-complete.bmp') + ';900:QUIT', sd)
    assert 'Entering activity: KeyboardEntry' not in output
    assert latest(sd)['generation'] == generation + 1 and latest(sd)['tasks'][0]['completed']
    run('reopen', home + '300:TAP:55,210;700:QUIT', sd)
    assert latest(sd)['generation'] == generation + 2 and not latest(sd)['tasks'][0]['completed']
    before = latest(sd)
    run('cancel-edit', home + '300:TAP:180,210;300:WAIT:Entering activity: KeyboardEntry;'
        '300:TAP:52,568;700:TAP:30,64;700:WAIT:SCREEN:.*Todo.*taskq;300:QUIT', sd)
    run('cancel-create', home + '300:TAP:180,130;300:WAIT:Entering activity: KeyboardEntry;'
        '300:TAP:52,568;700:TAP:30,64;700:WAIT:SCREEN:.*Todo.*taskq;300:QUIT', sd)
    run('reject-empty', home + '300:TAP:180,130;300:WAIT:Entering activity: KeyboardEntry;'
        '300:TAP:420,754;300:WAIT:SCREEN:.*UTF-8 bytes;300:QUIT', sd)
    assert latest(sd) == before
    run('create-second', home + '300:TAP:180,130;300:WAIT:Entering activity: KeyboardEntry;'
        '300:TAP:52,568;700:TAP:420,754;700:WAIT:SCREEN:.*Todo.*taskq.*q;300:QUIT', sd)
    before = latest(sd)
    choose = '300:TAP:440,50;300:WAIT:SCREEN:.*Delete tasks;300:TAP:55,135;700:TAP:55,210;700:WAIT:Delete selected \\(2\\);'
    run('cancel-confirmation', home + choose + '400:TAP:240,765;400:WAIT:Entering activity: Confirmation;'
        '400:SHOT:' + str(artifacts/'todo-batch-confirm.bmp') + ';700:TAP:240,412;'
        '700:WAIT:SCREEN:.*Delete tasks;300:QUIT', sd)
    assert latest(sd) == before
    run('cancel-selection', home + choose + '400:TAP:30,50;400:WAIT:SCREEN:.*Todo;'
        '400:TAP:440,50;400:WAIT:Delete selected \\(0\\);400:TAP:30,50;800:TAP:30,50;'
        '800:WAIT:Entering activity: Home;300:QUIT', sd)
    assert latest(sd) == before
    run('batch-delete', home + choose + '300:SHOT:' + str(artifacts/'todo-delete-selection.bmp') +
        ';600:TAP:240,765;600:WAIT:Entering activity: Confirmation;300:TAP:240,475;'
        '300:WAIT:SCREEN:.*No tasks yet;300:QUIT', sd)
    assert latest(sd)['tasks'] == [] and latest(sd)['generation'] == before['generation'] + 1
    run('restart-empty', home + '300:WAIT:SCREEN:.*No tasks yet;300:TAP:440,50;700:QUIT', sd)

with tempfile.TemporaryDirectory(prefix='checkbox-pages-', dir=artifacts) as directory:
    sd = Path(directory)
    tasks = [dict(id=i + 1, completed=False, text=f'Task {i + 1:02}') for i in range(32)]
    seed(sd, tasks)
    run('selection-across-pages', home + '300:TAP:440,50;300:WAIT:SCREEN:.*Delete tasks;'
        '300:TAP:55,135;700:SWIPE:240,650,240,200;1200:TAP:55,135;'
        '1200:WAIT:Delete selected \\(2\\);300:TAP:240,765;300:WAIT:Entering activity: Confirmation;'
        '300:TAP:240,475;700:QUIT', sd)
    remaining = latest(sd)
    assert len(remaining['tasks']) == 30 and remaining['generation'] == 2
    assert not any(t['id'] == 1 for t in remaining['tasks'])
    assert all(not t['completed'] for t in remaining['tasks'])

with tempfile.TemporaryDirectory(prefix='checkbox-last-row-', dir=artifacts) as directory:
    sd = Path(directory)
    seed(sd, [dict(id=i + 1, completed=False, text=f'Task {i + 1:02}') for i in range(32)])
    run('last-checkbox', home + '300:SWIPE:240,650,240,200;800:SWIPE:240,650,240,200;'
        '1300:SWIPE:240,650,240,200;1800:TAP:55,696;2300:QUIT', sd)
    saved = latest(sd)
    assert saved['tasks'][31]['completed'] and saved['generation'] == 2
    assert all(not t['completed'] for t in saved['tasks'][:31])

with tempfile.TemporaryDirectory(prefix='checkbox-wrapped-', dir=artifacts) as directory:
    sd = Path(directory)
    seed(sd, [dict(id=1, completed=True, text='Completed task with a description long enough to wrap across two lines and truncate at the visible boundary. Café 日本語')])
    run('wrapped-strike', home + '300:SHOT:' + str(artifacts/'todo-wrapped-strike.bmp') + ';600:QUIT', sd)
print('Todo checkbox workflows passed: direct edit, complete/incomplete, cancel, batch delete, cross-page selection, persistence, Home')
