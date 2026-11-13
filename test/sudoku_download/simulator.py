#!/usr/bin/env python3
"""Sudoku pack settings/download UI using isolated cards and a local HTTP server."""
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT.parent / 'qa-artifacts/sudoku-download'
LOGS = ROOT.parent / 'logs'
spec = importlib.util.spec_from_file_location('sudoku_ui', ROOT / 'test/sudoku/simulator.py')
ui = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ui)
Steps = ui.Steps
PACKS = {}
REQUESTS = []
MODE = 'success'

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args): pass
    def do_GET(self):
        name = self.path.rsplit('/', 1)[-1]
        REQUESTS.append((name, self.headers.get('Range')))
        data = PACKS[name]
        offset = int(self.headers.get('Range', 'bytes=0-')[6:-1])
        self.send_response(206 if offset else 200)
        self.send_header('Content-Length', str(len(data) - offset))
        if offset: self.send_header('Content-Range', f'bytes {offset}-{len(data)-1}/{len(data)}')
        self.end_headers()
        try:
            for pos in range(offset, len(data), 2048):
                chunk = data[pos:pos+2048]
                if MODE == 'wrong': chunk = bytes([chunk[0] ^ 1]) + chunk[1:]
                self.wfile.write(chunk); self.wfile.flush()
                if MODE == 'slow' or (MODE == 'all-cancel' and name == 'medium.cpsd'): time.sleep(.025)
                if MODE == 'long': time.sleep(.18)
        except (BrokenPipeError, ConnectionResetError): pass


def make_packs():
    record = ui.bank.encode_record(ui.PUZZLE_A, ui.SOLUTION_A, hashlib.sha1(ui.PUZZLE_A.encode()).digest()[:6], 12)
    for d, name in enumerate(('easy', 'medium', 'hard', 'diabolical')):
        records = record * 8192
        header = ui.bank.encode_header(name, d, records, 8192, 12, 12)
        PACKS[name + '.cpsd'] = header + records


def start_settings(theme=1):
    return Steps().wait('Entering activity: Home').add('TAP:428,755') if theme == 4 else ui.enter_sudoku(theme)


def run(name, script, server, prepare=None, theme=1, before_settings=None, settings=None, wait_timeout=18000):
    REQUESTS.clear()
    with tempfile.TemporaryDirectory(prefix=name+'-', dir=ART) as directory:
        sd = Path(directory)
        (sd / '.crosspoint').mkdir()
        (sd / 'games/sudoku').mkdir(parents=True)
        (sd / '.crosspoint/settings.json').write_text(json.dumps(dict(uiTheme=theme, clockHasBeenSynced=1, **(settings or {}))))
        (sd / '.crosspoint/todo-a.json').write_bytes(b'TODO sentinel')
        (sd / 'book.epub').write_bytes(b'book sentinel')
        (sd / '.crosspoint/wifi.json').write_text(json.dumps({'lastConnectedSsid': 'Simulator WiFi (fake)',
            'credentials': [{'ssid': 'Simulator WiFi (fake)', 'password_obf': '', 'password_len': 0, 'password_crc32': 0}]}))
        if prepare: prepare(sd)
        full = '0:WAIT:Entering activity: Home;' + str(ui.enter_sudoku(theme))
        full += '0:WAIT:Entering activity: Sudoku;' + (str(before_settings) if before_settings else '')
        full += '350:TAP:440,63;0:WAIT:Entering activity: SudokuSettings;' + str(script)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd), CROSSPOINT_SIM_SCREEN_TEXT='1',
                   CROSSPOINT_SIM_WAIT_TIMEOUT_MS='100000' if name == 'long-idle' else str(wait_timeout), CROSSPOINT_SIM_INPUT_SCRIPT=full,
                   CROSSPOINT_SIM_SUDOKU_BANK_URL=f'http://127.0.0.1:{server.server_port}/')
        for d, data in enumerate(PACKS.values()):
            env[f'CROSSPOINT_SIM_SUDOKU_PACK_{d}'] = f'{len(data)}:{hashlib.sha256(data).hexdigest()}'
        try:
            result = subprocess.run([str(ui.PROGRAM)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
        finally:
            (sd / '.crosspoint').chmod(0o755)
        output = result.stdout + result.stderr
        (LOGS / f'sudoku-download-ui-{name}.log').write_text(output)
        assert result.returncode == 0 and 'WAIT timed out' not in output, name
        assert (sd / '.crosspoint/todo-a.json').read_bytes() == b'TODO sentinel'
        assert (sd / 'book.epub').read_bytes() == b'book sentinel'
        files = {p.relative_to(sd).as_posix(): p.read_bytes() for p in sd.rglob('*') if p.is_file()}
        print(name + ' PASS', flush=True)
        return files, output, list(REQUESTS)


def select_row(index):
    s = Steps(160)
    for _ in range(index): s.add('DOWN')
    return s.add('ENTER')


def exit_and_sleep(server):
    global MODE
    MODE = 'success'
    home = 'SWIPE:0.5,0.98,0.5,0.5'
    for child in (False, True):
        s = select_row(5).wait('SCREEN:.*Download complete.*Installed: 1')
        if child:
            s.add('SWIPE:0.5,0.01,0.5,0.45').wait('Entering activity: FrontlightPanel')
            s.add(home).wait('SCREEN:.*Download complete.*Done')
        s.add(home).wait(r'Silent restart \(target=home\)').wait('Entering activity: Home').add('QUIT')
        _, out, _ = run('home-child' if child else 'home-result', s, server)
        assert 'target=sudoku-settings' not in out

    s = select_row(5).wait('SCREEN:.*Download complete.*Installed: 1')
    s.add(ui.BACK).wait(r'Silent restart \(target=sudoku-settings\)').add('QUIT')
    _, out, _ = run('back-result', s, server)
    assert 'target=home' not in out

    def no_credentials(sd):
        (sd / '.crosspoint/wifi.json').unlink()
    for go_home in (False, True):
        target = 'home' if go_home else 'sudoku-settings'
        s = select_row(5).wait('Entering activity: WifiSelection').add(home if go_home else ui.BACK)
        s.wait(r'Silent restart \(target=' + target + r'\)').add('QUIT')
        _, out, requests = run('home-wifi' if go_home else 'back-wifi', s, server, prepare=no_credentials)
        assert not requests
        assert ('target=sudoku-settings' if go_home else 'target=home') not in out

    MODE = 'slow'
    def installed(sd):
        (sd / 'games/sudoku/easy.cpsd').write_bytes(PACKS['easy.cpsd'])
    # Home during the transfer is Cancel; acknowledging its result still returns to settings.
    s = select_row(5).wait('SCREEN:.*Downloading.*Easy').add(home)
    s.wait('SCREEN:.*Cancelled.*Installed: 0').add('ENTER').wait(r'Silent restart \(target=sudoku-settings\)').add('QUIT')
    files, _, _ = run('home-transfer', s, server, prepare=installed)
    assert files['games/sudoku/easy.cpsd'] == PACKS['easy.cpsd']
    assert 0 < len(files['games/sudoku/easy.part']) < len(PACKS['easy.cpsd'])

    for short_sleep in (False, True):
        s = select_row(5).wait('SCREEN:.*Downloading.*Easy').add('POWER:80' if short_sleep else 'POWER:900')
        s.wait('Entering activity: Sleep').add('QUIT')
        files, out, _ = run('sleep-short' if short_sleep else 'sleep-hold', s, server, prepare=installed,
                            settings={'shortPwrBtn': 1 if short_sleep else 0}, wait_timeout=4000)
        assert 'Silent restart' not in out and 'Download result:' not in out
        assert files['games/sudoku/easy.cpsd'] == PACKS['easy.cpsd']
        assert 0 < len(files['games/sudoku/easy.part']) < len(PACKS['easy.cpsd'])

    # Releasing Down first must not turn a screenshot chord's remaining power hold into sleep.
    s = select_row(5).wait('SCREEN:.*Downloading.*Easy').add('DOWN:600').add('POWER:900')
    s.wait('SCREEN:.*Download complete.*Installed: 1').add('QUIT')
    files, out, _ = run('screenshot-chord', s, server, prepare=installed, settings={'shortPwrBtn': 0})
    assert 'Entering activity: Sleep' not in out
    assert files['games/sudoku/easy.cpsd'] == PACKS['easy.cpsd']
    MODE = 'success'


def main():
    global MODE
    ART.mkdir(parents=True, exist_ok=True)
    make_packs()
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        import sys
        if '--exit-sleep' in sys.argv:
            exit_and_sleep(server)
            return
        s = select_row(4).wait('Download result:').wait('SCREEN:.*Download complete.*Installed: 4')
        s.add(f'SHOT:{ART}/download-all.bmp').add('ENTER').wait('Silent restart.*target=sudoku-settings').add('QUIT')
        files, out, requests = run('all', s, server)
        for name, data in PACKS.items(): assert files['games/sudoku/' + name] == data
        assert len(requests) == 4 and not any(k.endswith('.part') or k.endswith('.bak') for k in files)

        def installed(sd):
            (sd / 'games/sudoku/easy.cpsd').write_bytes(PACKS['easy.cpsd'])
        def partial(sd):
            installed(sd)
            (sd / 'games/sudoku/easy.part').write_bytes(PACKS['easy.cpsd'][:8192])
        s = select_row(5).wait('SCREEN:.*Download complete.*Installed: 1').add('QUIT')
        files, _, requests = run('resume-replace', s, server, prepare=partial)
        assert requests[0][1] == 'bytes=8192-'
        assert files['games/sudoku/easy.cpsd'] == PACKS['easy.cpsd']
        assert 'games/sudoku/easy.part' not in files

        MODE = 'wrong'
        files, _, _ = run('wrong-hash', select_row(5).wait('SCREEN:.*Verification failed.*Installed: 0').add('QUIT'), server, prepare=installed)
        assert files['games/sudoku/easy.cpsd'] == PACKS['easy.cpsd'] and 'games/sudoku/easy.part' not in files
        MODE = 'slow'
        s = select_row(5).wait('SCREEN:.*Downloading.*Easy').add(f'SHOT:{ART}/progress.bmp').add('TAP:240,510')
        s.wait('SCREEN:.*Cancelled.*Installed: 0').add(f'SHOT:{ART}/cancel.bmp').add('QUIT')
        files, _, _ = run('cancel', s, server, prepare=installed)
        assert files['games/sudoku/easy.cpsd'] == PACKS['easy.cpsd']
        assert 0 < len(files['games/sudoku/easy.part']) < len(PACKS['easy.cpsd'])
        MODE = 'all-cancel'
        s = select_row(4).wait('SCREEN:.*Downloading.*Medium').add('TAP:240,510')
        s.wait('SCREEN:.*Cancelled.*Installed: 1').add('QUIT')
        files, _, requests = run('all-cancel', s, server)
        assert files['games/sudoku/easy.cpsd'] == PACKS['easy.cpsd']
        assert 'games/sudoku/medium.part' in files and 'games/sudoku/medium.cpsd' not in files
        assert not any(name in ('hard.cpsd', 'diabolical.cpsd') for name, _ in requests)
        MODE = 'success'

        s = select_row(6).wait('SCREEN:.*Delete puzzle pack').add('ENTER')
        s.wait('SCREEN:.*Sudoku settings').add(f'SHOT:{ART}/delete-cancel.bmp').add('QUIT')
        files, _, _ = run('delete-cancel', s, server, prepare=installed)
        assert files['games/sudoku/easy.cpsd'] == PACKS['easy.cpsd']

        before = Steps().add('TAP:240,138').wait('Entering activity: SudokuBoard')
        before.add(f'TAP:{ui.cell_xy(0)}').add('TAP:149,612').add(ui.BACK).wait('SCREEN:.*Continue')
        s = select_row(6).wait('SCREEN:.*Delete puzzle pack').add('DOWN').add('ENTER')
        s.wait('SCREEN:.*Puzzle pack deleted').add(ui.BACK).wait('SCREEN:.*Continue.*Not installed')
        s.add('ENTER').wait('Entering activity: SudokuBoard').add(f'SHOT:{ART}/continue-no-pack.bmp').add('QUIT')
        files, _, _ = run('delete-continue', s, server, prepare=partial, before_settings=before)
        assert 'games/sudoku/easy.cpsd' not in files and 'games/sudoku/easy.part' not in files
        assert any(k.startswith('.crosspoint/sudoku-') for k in files)

        def readonly(sd):
            installed(sd)
            (sd / '.crosspoint').chmod(0o555)
        s = select_row(4).wait('SCREEN:.*Progress is not saved').add(f'SHOT:{ART}/unsaved.bmp').add(ui.BACK)
        s.wait('SCREEN:.*Continue').add('ENTER').wait('Entering activity: SudokuBoard').add('QUIT')
        files, out, requests = run('unsaved-block', s, server, prepare=readonly, before_settings=before)
        assert not requests and 'Entering activity: WifiSelection' not in out
        assert not any(k.startswith('.crosspoint/sudoku-') for k in files)

        for theme in range(5):
            s = Steps().wait('SCREEN:.*Download all packs').add(f'SHOT:{ART}/settings-{theme}.bmp')
            for _ in range(12): s.add('DOWN')
            s.wait('SCREEN:.*Download Diabolical.*Delete Diabolical').add(f'SHOT:{ART}/settings-end-{theme}.bmp').add('QUIT')
            run(f'theme-{theme}', s, server, prepare=installed, theme=theme)
        MODE = 'long'
        s = select_row(5).wait('SCREEN:.*Download complete.*Installed: 1')
        s.add(f'SHOT:{ART}/long-result.bmp').add('QUIT')
        files, out, _ = run('long-idle', s, server, settings={'sleepTimeoutMinutes': 1})
        assert 'Entering activity: Sleep' not in out and 'Auto-sleep triggered' not in out
        assert files['games/sudoku/easy.cpsd'] == PACKS['easy.cpsd']
        exit_and_sleep(server)
    finally:
        server.shutdown(); server.server_close()

if __name__ == '__main__': main()
