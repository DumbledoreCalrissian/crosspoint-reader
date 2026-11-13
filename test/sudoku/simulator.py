#!/usr/bin/env python3
"""Sudoku touch workflows in the Picco simulator on isolated SD cards.

Puzzle files are built with scripts/build_sudoku_bank.py from three public-domain
Sudoku Exchange puzzles. Saves are decoded from the card after each run.
Usage (from the firmware root, after `pio run -e simulator_picco`):
    ../.venv/bin/python test/sudoku/simulator.py
"""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT.parent / 'qa-artifacts/sudoku-sim'
LOGS = ROOT.parent / 'logs'
PROGRAM = ROOT / '.pio/build/simulator_picco/program'
THEMES = ['classic', 'lyra', 'lyra-extended', 'roundedraff', 'cover-grid']
ART.mkdir(parents=True, exist_ok=True)

spec = importlib.util.spec_from_file_location('bank', ROOT / 'scripts/build_sudoku_bank.py')
bank = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bank)

PUZZLE_A = '050703060007000800000816000000030000005000100730040086906000204840572093000409000'
PUZZLE_B = '302401809001000300000000000040708010780502036000090000200609003900000008800070005'
PUZZLE_C = '080200400570000100002300000820090005000715000700020041000006700003000018007009050'
SOLUTION_A = bank.count_solutions(PUZZLE_A)[1]
GIVENS_A = 81 - PUZZLE_A.count('0')

# Lyra geometry (portrait 480 x 800), measured from captures.
ROW = [138, 200, 262, 324]          # settings rows
# Start page buttons: Continue, the first New game button without a saved game,
# and Medium below a saved game.
CONTINUE, FIRST_NEW, MEDIUM_WITH_GAME = 158, 174, 378
GEAR = '440,63'
BACK = 'TAP:30,63'  # header back arrow; the Picco profile has no Back key in the simulator
DIGIT = {1: (61, 612), 2: (149, 612), 3: (238, 612), 4: (327, 612), 5: (416, 612),
         6: (61, 682), 7: (149, 682), 8: (238, 682), 9: (327, 682), 0: (416, 682)}
TOOLS = {'notes': (72, 748), 'undo': (183, 748), 'check': (294, 748), 'reveal': (405, 748)}
BOARD_X, BOARD_Y, CELL = 20, 115, 49
SAVE_SIZE = 602
TODO_SENTINEL = b'unchanged sudoku regression sentinel'


def cell_xy(cell):
    return f'{BOARD_X + (cell % 9) * CELL + CELL // 2},{BOARD_Y + (cell // 9) * CELL + CELL // 2}'


def tap(xy):
    return xy if isinstance(xy, str) else f'{xy[0]},{xy[1]}'


class Steps:
    """Builds a script segment with increasing delays (they count from the last WAIT)."""

    def __init__(self, gap=350):
        self.parts, self.at, self.gap = [], 0, gap

    def add(self, action):
        self.at += self.gap
        self.parts.append(f'{self.at}:{action}')
        return self

    def wait(self, pattern):
        # Start waiting after this segment's earlier steps, so long segments do not time out.
        self.parts.append(f'{self.at + self.gap if self.at else 0}:WAIT:{pattern}')
        self.at = 0
        return self

    def __str__(self):
        return ';'.join(self.parts) + ';'


def source_line(digits, rating):
    return f'{hashlib.sha1(digits.encode()).hexdigest()[:12]} {digits} {rating:>4}\n'.encode()


def write_packs(sd, damaged_hard=False):
    with tempfile.TemporaryDirectory() as temporary:
        source = Path(temporary)
        files = {'easy': source_line(PUZZLE_A, '1.2') + source_line(PUZZLE_B, '1.2'),
                 'medium': source_line(PUZZLE_C, '2.0')}
        for name, data in files.items():
            (source / f'{name}.txt').write_bytes(data)
        ids = {'easy': 0, 'medium': 1}
        bank.build(source, sd / 'games/sudoku',
                   sources=tuple((n, ids[n], hashlib.sha256(d).hexdigest()) for n, d in files.items()), jobs=1)
    if damaged_hard:
        data = bytearray((sd / 'games/sudoku/medium.cpsd').read_bytes())
        data[10] = 2  # medium content under the hard name, header otherwise intact
        (sd / 'games/sudoku/hard.cpsd').write_bytes(bytes(data))


def unpack(data):
    cells = []
    for byte in data:
        cells += [byte >> 4, byte & 15]
    return cells[:81]


def read_save(sd):
    """Newest intact save as a dict, or None."""
    best = None
    for slot in 'ab':
        path = sd / f'.crosspoint/sudoku-{slot}.bin'
        if not path.exists():
            continue
        data = path.read_bytes()
        if len(data) != SAVE_SIZE or data[:4] != b'CPSG' or \
                struct.unpack_from('<I', data, SAVE_SIZE - 4)[0] != zlib.crc32(data[:SAVE_SIZE - 4]):
            continue
        generation = struct.unpack_from('<I', data, 6)[0]
        if best and best['generation'] >= generation:
            continue
        game = data[43:43 + 555]
        best = {
            'generation': generation, 'active': data[10] == 1,
            'next': struct.unpack_from('<4I', data, 11),
            'givens': unpack(game[0:41]), 'values': unpack(game[82:123]),
            'notes': struct.unpack_from('<81H', game, 123), 'hints': game[543], 'difficulty': game[544],
            'index': struct.unpack_from('<I', game, 545)[0],
        }
    return best


def enter_sudoku(theme):
    # Home rows: Browse Files, Library, Settings, Todo, Pomodoro, Sudoku.
    if theme == 4:
        return Steps().add('TAP:428,755')
    steps = Steps(300)
    for _ in range(5):
        steps.add('DOWN')
    return steps.add('ENTER')


def run(name, script, theme=1, packs=True, damaged_hard=False, prepare=None, after_wake=None, settings=None):
    with tempfile.TemporaryDirectory(prefix=f'sudoku-{name}-', dir=ART) as directory:
        sd = Path(directory)
        (sd / '.crosspoint').mkdir()
        (sd / '.crosspoint/settings.json').write_text(json.dumps(dict(uiTheme=theme, **(settings or {}))))
        (sd / '.crosspoint/todo-a.json').write_bytes(TODO_SENTINEL)
        if packs:
            write_packs(sd, damaged_hard)
        if prepare:
            prepare(sd)
        before = {p: p.read_bytes() for p in sd.rglob('*') if p.is_file() and '.crosspoint' not in p.parts}
        full = '0:WAIT:Entering activity: Home;' + str(enter_sudoku(theme)) + '0:WAIT:Entering activity: Sudoku;' + script
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd), CROSSPOINT_SIM_SCREEN_TEXT='1',
                   CROSSPOINT_SIM_WAIT_TIMEOUT_MS='15000', CROSSPOINT_SIM_INPUT_SCRIPT=full)
        if after_wake:
            env['CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_WAKE'] = after_wake
        try:
            result = subprocess.run([str(PROGRAM)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=120)
        finally:
            # Restore write access before TemporaryDirectory removes the card.
            (sd / '.crosspoint').chmod(0o755)
        output = result.stdout + result.stderr
        (LOGS / f'sudoku-sim-{name}.log').write_text(output)
        assert result.returncode == 0 and 'WAIT timed out' not in output, f'{name}: see logs/sudoku-sim-{name}.log'
        # Sudoku touches only its own files and settings.json.
        assert (sd / '.crosspoint/todo-a.json').read_bytes() == TODO_SENTINEL, name
        # Puzzle files are read only; nothing new appears outside the firmware folder.
        for path, data in before.items():
            assert path.read_bytes() == data, f'{name}: {path} changed'
        for path in sd.rglob('*'):
            if path.is_file() and path not in before:
                assert '.crosspoint' in path.parts, f'{name}: new file {path}'
        assert not (sd / '.crosspoint/todo-b.json').exists(), name
        # The card is deleted on return: hand back its firmware files.
        files = {p.name: p.read_bytes() for p in (sd / '.crosspoint').iterdir() if p.is_file()}
        return output, read_save(sd), files


def shot(name):
    return f'SHOT:{ART}/{name}.bmp'


def screens(output):
    return [line for line in output.splitlines() if line.startswith('[SIM] SCREEN:')]


def no_puzzles():
    s = Steps().wait('SCREEN:.*Copy puzzle files.*Not installed').add(shot('no-puzzles')).add(f'TAP:240,{FIRST_NEW}')
    s.add('QUIT')
    output, save, _ = run('no-puzzles', str(s), packs=False)
    assert 'Entering activity: SudokuBoard' not in output and save is None
    print('No puzzle files: install hint, rows inactive PASS', flush=True)


def play_and_resume():
    a, b = 0, 2  # empty cells in PUZZLE_A
    s = Steps().wait('SCREEN:.*New game.*Easy.*Medium').add(f'TAP:240,{FIRST_NEW}').wait('Entering activity: SudokuBoard')
    s.add(f'TAP:{cell_xy(a)}').add(f'TAP:{tap(DIGIT[1])}')
    s.add(f'TAP:{cell_xy(b)}').add(f'TAP:{tap(TOOLS["notes"])}').add(f'TAP:{tap(DIGIT[2])}').add(f'TAP:{tap(DIGIT[4])}')
    s.add(shot('board-entry-notes'))
    s.add('UP').add('DOWN').add('ENTER')  # keys must not change anything on the board
    s.add(BACK).wait(f'SCREEN:.*Continue.*Easy, {(GIVENS_A + 1) * 100 // 81}%').add(shot('list-continue')).add('QUIT')
    output, save, files = run('play', str(s))
    assert save and save['active'] and save['values'][a] == 1 and save['notes'][b] == (1 << 1) | (1 << 3), save
    assert save['next'][0] == 1 and save['index'] == 0 and save['hints'] == 0
    assert not any('Puzzles:' in line for line in screens(output)), 'start page shows no puzzle counts'
    print('Enter digit, notes saved on exit, keys inert, Continue progress PASS', flush=True)

    # Second process on a card with that save: Continue, undo the notes, leave.
    saved = {n: data for n, data in files.items() if n.startswith('sudoku-')}

    def restore(target):
        for n, data in saved.items():
            (target / '.crosspoint' / n).write_bytes(data)

    s = Steps().wait('SCREEN:.*Continue').add(f'TAP:240,{CONTINUE}').wait('Entering activity: SudokuBoard')
    s.add(f'TAP:{tap(TOOLS["undo"])}').add(f'TAP:{tap(TOOLS["undo"])}').add(BACK).wait('SCREEN:.*Continue')
    s.add('QUIT')
    _, save, _ = run('resume', str(s), prepare=restore)
    assert save['values'][a] == 1 and save['notes'][b] == 0, save
    print('Resume after restart, undo across restart PASS', flush=True)


def check_reveal_conflict():
    a = 0
    s = Steps().wait('SCREEN:.*Easy').add(f'TAP:240,{FIRST_NEW}').wait('Entering activity: SudokuBoard')
    s.add(f'TAP:{cell_xy(a)}').add(f'TAP:{tap(DIGIT[5])}')  # 5 repeats the given in row 0
    s.add(shot('board-conflict')).add(f'TAP:{tap(TOOLS["check"])}').add(shot('board-check'))
    s.add(f'TAP:{tap(TOOLS["reveal"])}').add(shot('board-reveal')).add(BACK).wait('SCREEN:.*Continue')
    s.add('QUIT')
    _, save, _ = run('check-reveal', str(s))
    assert save['values'][a] == int(SOLUTION_A[a]) and save['hints'] == 2, save
    print('Conflict mark, Check and Reveal counted as hints PASS', flush=True)


def solve_with_reveal():
    s = Steps(400).wait('SCREEN:.*Easy').add(f'TAP:240,{FIRST_NEW}').wait('Entering activity: SudokuBoard')
    for cell in range(81):
        if PUZZLE_A[cell] == '0':
            s.add(f'TAP:{cell_xy(cell)}').add(f'TAP:{tap(TOOLS["reveal"])}')
    s.wait(f'SCREEN:.*Solved.*Hints used: {81 - GIVENS_A}').add(shot('board-solved')).add('TAP:240,748')
    s.wait('SCREEN:.*Sudoku \\| New game \\| Easy').add('QUIT')
    output, save, _ = run('solve', str(s))
    assert save and not save['active'] and save['next'][0] == 1, save
    assert 'Continue' not in screens(output)[-1]
    print('Solve, completion message, saved game cleared, next puzzle kept PASS', flush=True)


def replace_confirmation():
    s = Steps().wait('SCREEN:.*Easy').add(f'TAP:240,{FIRST_NEW}').wait('Entering activity: SudokuBoard')
    s.add(f'TAP:{cell_xy(0)}').add(f'TAP:{tap(DIGIT[1])}').add(BACK).wait('SCREEN:.*Continue')
    s.add(f'TAP:240,{MEDIUM_WITH_GAME}').wait('SCREEN:.*Start a new puzzle').add(shot('replace-confirm')).add('ENTER')
    s.wait('SCREEN:.*Continue').add(f'TAP:240,{MEDIUM_WITH_GAME}').wait('SCREEN:.*Start a new puzzle')
    s.add('DOWN').add('ENTER').wait('Entering activity: SudokuBoard').wait('SCREEN:.*Medium 2.0').add('QUIT')
    _, save, _ = run('replace', str(s))
    assert save['active'] and save['difficulty'] == 1 and save['next'] == (1, 0, 0, 0), save
    print('Replace needs confirmation; Cancel keeps the game PASS', flush=True)


def settings_toggles():
    s = Steps().wait('SCREEN:.*Easy').add(f'TAP:{GEAR}').wait('Entering activity: SudokuSettings')
    s.add(shot('settings')).add(f'TAP:240,{ROW[0]}').add(f'TAP:240,{ROW[2]}').add(f'TAP:240,{ROW[3]}')
    s.add(BACK).wait('SCREEN:.*Sudoku \\| New game \\| Medium').add(shot('list-without-easy'))
    s.add(f'TAP:240,{FIRST_NEW}').wait('Entering activity: SudokuBoard').add(shot('board-without-help')).add('QUIT')
    output, save, files = run('settings', str(s))
    stored = json.loads(files['settings.json'])
    assert stored['sudokuShowEasy'] == 0 and stored['sudokuCheckButton'] == 0 and stored['sudokuRevealButton'] == 0
    assert stored['sudokuMarkConflicts'] == 1
    board = [line for line in screens(output) if 'Medium 2.0' in line][-1]
    assert 'Check' not in board and 'Reveal' not in board and 'Notes' in board
    assert save['difficulty'] == 1
    print('Settings: hide Easy, remove Check and Reveal, persisted PASS', flush=True)


def sleep_and_home_save_notes():
    for name in ('sleep', 'home-gesture'):
        s = Steps().wait('SCREEN:.*Easy').add(f'TAP:240,{FIRST_NEW}').wait('Entering activity: SudokuBoard')
        s.add(f'TAP:{cell_xy(2)}').add(f'TAP:{tap(TOOLS["notes"])}').add(f'TAP:{tap(DIGIT[7])}')
        if name == 'sleep':
            s.add('SLEEP').wait('Entering activity: Sleep').add('POWER')
        else:
            s.add('SWIPE:0.5,0.98,0.5,0.5').wait('Entering activity: Home').add('QUIT')  # Picco Home gesture
        after = '0:WAIT:Entering activity: Home;500:QUIT' if name == 'sleep' else None
        _, save, _ = run(name, str(s), after_wake=after)
        assert save['notes'][2] == 1 << 6, (name, save)
    print('Notes-only changes saved before sleep and on the Home gesture PASS', flush=True)


def damaged_files():
    def damage_save(sd):
        (sd / '.crosspoint/sudoku-a.bin').write_bytes(b'not a save')

    s = Steps().wait('SCREEN:.*Saved Sudoku data was damaged.*Hard.*Damaged').add(shot('damaged')).add('QUIT')
    _, _, files = run('damaged', str(s), damaged_hard=True, prepare=damage_save)
    assert files['sudoku-a.bin.bad'] == b'not a save'
    print('Damaged puzzle file and damaged save reported; save kept aside PASS', flush=True)


def failed_save_continue():
    def read_only(sd):
        (sd / '.crosspoint').chmod(0o555)

    s = Steps().wait('SCREEN:.*Easy').add(f'TAP:240,{FIRST_NEW}').wait('Entering activity: SudokuBoard')
    s.add('TAP:60,180')  # first cell, below the save-error notice
    s.add(f'TAP:{tap(DIGIT[1])}').add(BACK)
    s.wait('SCREEN:.*Could not save.*Continue')
    # The notice takes three lines, moving Continue below its usual position.
    s.add('ENTER').wait('Entering activity: SudokuBoard')
    s.add(f'TAP:{tap(TOOLS["undo"])}').add(BACK)
    s.wait(f'SCREEN:.*Could not save.*Continue.*Easy, {GIVENS_A * 100 // 81}%').add('QUIT')
    output, save, files = run('failed-save-continue', str(s), prepare=read_only)
    assert save is None and not any(n.startswith('sudoku-') for n in files)
    assert sum('Entering activity: SudokuBoard' in line and not line.startswith('[SIM]')
               for line in output.splitlines()) == 2
    assert any(f'Easy, {(GIVENS_A + 1) * 100 // 81}%' in line for line in screens(output))
    print('Failed first save: list notice, Continue and undo keep the in-memory game PASS', flush=True)


def theme_captures():
    for theme, name in enumerate(THEMES):
        s = Steps().wait('SCREEN:.*Easy').add(shot(f'{name}-list'))
        if theme == 1:
            s.add(f'TAP:240,{FIRST_NEW}').wait('Entering activity: SudokuBoard').add(shot(f'{name}-board'))
        s.add('QUIT')
        run(f'theme-{name}', str(s), theme=theme)
    for path in ART.glob('*.bmp'):
        Image.open(path).save(path.with_suffix('.png'))
        path.unlink()
    print('Start list in all five themes PASS', flush=True)


if __name__ == '__main__':
    assert PROGRAM.exists(), 'Build simulator_picco first'
    no_puzzles()
    play_and_resume()
    check_reveal_conflict()
    solve_with_reveal()
    replace_confirmation()
    settings_toggles()
    sleep_and_home_save_notes()
    damaged_files()
    failed_save_continue()
    theme_captures()
