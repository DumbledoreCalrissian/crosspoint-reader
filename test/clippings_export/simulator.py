#!/usr/bin/env python3
"""Root clipping export remains viewable/shareable without becoming a book."""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT.parent / 'qa-artifacts/clippings-exclusion'
PROGRAM = ROOT / '.pio/build/simulator_picco/program'
EXPORT = b'Clipping export sentinel.\n'
ART.mkdir(parents=True, exist_ok=True)


def index_entries(sd):
    data = (sd / '.crosspoint/library.idx').read_bytes()
    count, folders = struct.unpack_from('<HH', data, 8)
    folder_start, _, records, _, names = struct.unpack_from('<IIIII', data, 16)
    paths = []
    pos = folder_start
    for _ in range(folders):
        length = data[pos]
        paths.append(data[pos + 1:pos + 1 + length].decode())
        pos += 1 + length
    entries = {}
    for i in range(count):
        start = records + i * 128
        name_off = struct.unpack_from('<I', data, start)[0]
        first_seen, folder_id = struct.unpack_from('<HH', data, start + 8)
        name_len = data[start + 12]
        name = data[names + name_off + 8:names + name_off + 8 + name_len].decode()
        entries[paths[folder_id].rstrip('/') + '/' + name] = first_seen
    return data[5], entries


def prepare(sd, theme):
    (sd / '.crosspoint').mkdir(exist_ok=True)
    (sd / 'books').mkdir(exist_ok=True)
    (sd / 'My Clippings.txt').write_bytes(EXPORT)
    (sd / 'books/ordinary.txt').write_text('Ordinary book sentinel.\n' * 100)
    (sd / 'books/My Clippings.txt').write_text('Nested namesake book sentinel.\n' * 100)
    (sd / '.crosspoint/state.json').write_text('{}')
    (sd / '.crosspoint/settings.json').write_text(json.dumps({'uiTheme': theme}))
    (sd / '.crosspoint/recent.json').write_text(json.dumps({'books': [
        {'path': path, 'title': title} for path, title in [
            ('/My Clippings.txt', 'Root export sentinel'),
            ('/books/ordinary.txt', 'Ordinary book sentinel'),
            ('/books/My Clippings.txt', 'Nested namesake sentinel')]]}))


def environment(sd, script):
    return dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd),
                CROSSPOINT_SIM_SCREEN_TEXT='1', CROSSPOINT_SIM_INPUT_SCRIPT=script,
                CROSSPOINT_SIM_HTTP_PORT='18090')


def verify(sd):
    assert (sd / 'My Clippings.txt').read_bytes() == EXPORT
    recent = json.loads((sd / '.crosspoint/recent.json').read_text())['books']
    assert {book['path'] for book in recent} == {'/books/ordinary.txt', '/books/My Clippings.txt'}


def run(sd, name, script):
    result = subprocess.run([str(PROGRAM)], cwd=ROOT, env=environment(sd, script),
                            capture_output=True, text=True, timeout=35)
    log = result.stdout + result.stderr
    (ART / f'{name}.log').write_text(log)
    assert result.returncode == 0 and 'WAIT timed out' not in log, name
    verify(sd)
    return log


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--legacy-card', type=Path, help='Optional card/index produced by the pre-fix simulator')
    parser.add_argument('--http', action='store_true', help='Also test file-transfer listing and download on localhost:18090')
    args = parser.parse_args()
    for theme in (0, 1, 4):
        with tempfile.TemporaryDirectory(prefix=f'home-{theme}-', dir=ART) as directory:
            sd = Path(directory)
            prepare(sd, theme)
            log = run(sd, f'home-{theme}', '0:WAIT:Entering activity: Home;1800:QUIT')
            screens = [line for line in log.splitlines() if line.startswith('[SIM] SCREEN:')]
            assert not any('Root export sentinel' in line for line in screens)
    print('Existing recents migrate on Classic, Lyra and Cover Grid Home PASS', flush=True)

    with tempfile.TemporaryDirectory(prefix='library-', dir=ART) as directory:
        sd = Path(directory)
        before = None
        if args.legacy_card:
            shutil.copytree(args.legacy_card, sd, dirs_exist_ok=True)
            version, before = index_entries(sd)
            assert version == 5 and '/My Clippings.txt' in before
        prepare(sd, 4)
        run(sd, 'library', '0:WAIT:Entering activity: Home;1200:TAP:146,760;'
            '0:WAIT:Entering activity: Library;1200:QUIT')
        version, after = index_entries(sd)
        assert version == 6
        assert set(after) == {'/books/ordinary.txt', '/books/My Clippings.txt'}
        if before:
            assert all(after[path] == before[path] for path in after)
        print('Library excludes root export, keeps nested namesake and migrates index/history PASS', flush=True)

        (sd / '.crosspoint/state.json').write_text('{}')
        view_log = run(sd, 'view', '0:WAIT:Entering activity: Home;800:TAP:52,760;'
            '0:WAIT:Entering activity: FileBrowser;800:DOWN;1400:ENTER;'
            '0:WAIT:Rendered page;800:SWIPE:0.5,0.98,0.5,0.5;'
            '0:WAIT:Entering activity: Home;1000:QUIT')
        assert 'Loading ePub: /My Clippings.txt' in view_log
        assert 'Clipping export sentinel.' in view_log.replace(' | ', ' ')
        assert set(index_entries(sd)[1]) == set(after)
        print('File browser opens root export; returning Home adds no recent book PASS', flush=True)

        if args.http:
            (sd / '.crosspoint/state.json').write_text('{}')
            script = ('0:WAIT:Entering activity: Home;800:TAP:52,760;'
                      '0:WAIT:Entering activity: FileBrowser;800:TAP:440,55;'
                      '0:WAIT:Entering activity: NetworkModeSelection;800:TAP:240,296;'
                      '0:WAIT:Access Point started;10000:QUIT')
            proc = subprocess.Popen([str(PROGRAM)], cwd=ROOT, env=environment(sd, script),
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            try:
                opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
                deadline = time.monotonic() + 12
                while True:
                    try:
                        with opener.open('http://127.0.0.1:18090/api/files?path=/', timeout=1) as response:
                            listing = json.load(response)
                        break
                    except urllib.error.URLError:
                        if time.monotonic() > deadline:
                            raise
                        time.sleep(0.1)
                assert any(item['name'] == 'My Clippings.txt' for item in listing)
                path = urllib.parse.quote('/My Clippings.txt', safe='')
                with opener.open(f'http://127.0.0.1:18090/download?path={path}', timeout=3) as response:
                    assert response.read() == EXPORT
                log = proc.communicate(timeout=20)[0]
                assert proc.returncode == 0 and 'WAIT timed out' not in log
                (ART / 'http.log').write_text(log)
                verify(sd)
                print('File Transfer lists and downloads the unchanged root export PASS', flush=True)
            finally:
                if proc.poll() is None:
                    proc.terminate()
                    proc.communicate(timeout=5)


if __name__ == '__main__':
    main()
