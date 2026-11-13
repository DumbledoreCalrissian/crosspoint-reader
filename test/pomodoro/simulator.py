#!/usr/bin/env python3
"""Native Pomodoro touch, navigation, repaint and sleep regression checks."""
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT.parent / 'qa-artifacts/pomodoro'
LOGS = ROOT.parent / 'logs'
PROGRAM = ROOT / '.pio/build/simulator_picco/program'
THEMES = ['classic', 'lyra', 'lyra-extended', 'roundedraff', 'cover-grid']
# Bottom-anchored controls: one full-width primary, or primary left / Reset right.
PRIMARY = '240,684'
LEFT = '128,684'
RESET = '350,684'
GEAR = '440,50'
BACK = '30,55'
ART.mkdir(exist_ok=True, parents=True)


def enter(theme, extra=False, restored=False):
    # Home rows: Browse Files, Library, [Plugins], Settings, Todo, Pomodoro, Sudoku.
    if theme == 4:
        return '400:TAP:352,760;'
    if not extra:
        y = [694, 634, 692, 670][theme]
        return f'400:TAP:240,{y};'
    count = 5
    return ''.join(f'{300*(i+1)}:DOWN;' for i in range(count)) + f'{300*(count+1)}:ENTER;'


def run(theme, name, steps, timeout=45, settings=None, after_wake=None, check=None):
    with tempfile.TemporaryDirectory(prefix=f'pomodoro-{name}-', dir=ART) as directory:
        sd = Path(directory)
        (sd/'.crosspoint').mkdir()
        (sd/'.crosspoint/settings.json').write_text(json.dumps(dict(uiTheme=theme, **(settings or {}))))
        # The timer must not touch either Todo snapshot, even when data is damaged.
        snapshot = sd/'.crosspoint/todo-a.json'
        snapshot.write_bytes(b'unchanged timer regression sentinel')
        script = '0:WAIT:Entering activity: Home;' + enter(theme, bool(settings and settings.get('pluginsEnabled')))
        script += '0:WAIT:Entering activity: Pomodoro;' + steps
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd),
                   CROSSPOINT_SIM_SCREEN_TEXT='1', CROSSPOINT_SIM_INPUT_SCRIPT=script)
        if after_wake:
            env['CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_WAKE'] = after_wake
        try:
            result = subprocess.run([str(PROGRAM)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (LOGS/f'pomodoro-{name}.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            raise
        output = result.stdout + result.stderr
        (LOGS/f'pomodoro-{name}.log').write_text(output)
        assert result.returncode == 0 and 'WAIT timed out' not in output, name
        assert snapshot.read_bytes() == b'unchanged timer regression sentinel', name
        assert not (sd/'.crosspoint/todo-b.json').exists(), name
        assert not any('pomodoro' in p.name.lower() for p in sd.rglob('*')), name
        if check:
            check(sd)
        return output


def theme_workflow(theme):
    name = THEMES[theme]
    output = run(theme, name,
        f'300:WAIT:SCREEN:.*25:00.*Ready;400:SHOT:{ART}/{name}-ready.bmp;'
        f'600:TAP:{PRIMARY};600:WAIT:SCREEN:.*Running.*Pause.*Reset phase;'
        f'400:SHOT:{ART}/{name}-running.bmp;800:TAP:{LEFT};800:WAIT:SCREEN:.*Paused.*Resume;'
        f'400:SHOT:{ART}/{name}-paused.bmp;600:TAP:{LEFT};600:WAIT:SCREEN:.*Running.*Pause;'
        f'600:TAP:{RESET};600:WAIT:SCREEN:.*25:00.*Ready;'
        f'500:TAP:{PRIMARY};500:WAIT:SCREEN:.*Running;600:TAP:{BACK};'
        '600:WAIT:Entering activity: Home;' + enter(theme, restored=True) +
        '0:WAIT:Entering activity: Pomodoro;400:WAIT:SCREEN:.*25:00.*Ready;300:QUIT')
    assert 'Focus complete' not in output
    print(name, 'touch start/pause/resume/reset/exit/reopen PASS', flush=True)


def crowded_home(theme):
    name = THEMES[theme]
    steps = ('400:TAP:30,55;400:WAIT:Entering activity: Home;'
             f'400:SHOT:{ART}/{name}-crowded-home.bmp;500:ENTER;'
             '500:WAIT:Entering activity: Pomodoro;400:WAIT:SCREEN:.*25:00.*Ready;300:QUIT')
    run(theme, name + '-crowded', steps, settings={'pluginsEnabled': 1})
    print(name, 'crowded Home navigation PASS', flush=True)


def stationary_home_touch():
    # Lyra Extended with Plugins overflows; returning from Pomodoro scrolls Library to the top row.
    run(2, 'stationary-home-touch',
        '400:TAP:30,55;400:WAIT:Entering activity: Home;'
        '500:TAP:180,404;500:WAIT:Entering activity: Library;300:QUIT',
        settings={'pluginsEnabled': 1})
    print('Touching a scrolled Home row preserves press/release target PASS', flush=True)


def repaint_and_sleep():
    output = run(0, 'repaint',
        f'400:TAP:{PRIMARY};400:WAIT:SCREEN:.*Running;'
        f'12000:WAIT:SCREEN:.*24:50;200:TAP:{LEFT};200:WAIT:SCREEN:.*Paused;'
        '1200:QUIT')
    print('Countdown repaint cadence and pause PASS', flush=True)
    output = run(0, 'manual-sleep',
        f'400:TAP:{PRIMARY};400:WAIT:SCREEN:.*Running;600:SLEEP;'
        '600:WAIT:Entering activity: Sleep;500:POWER;',
        after_wake='0:WAIT:Entering activity: Home;' + enter(0) +
        '0:WAIT:Entering activity: Pomodoro;300:WAIT:SCREEN:.*25:00.*Ready;200:QUIT')
    print('Manual sleep cancels the foreground timer PASS', flush=True)


def control_center():
    run(0, 'control-center',
        f'400:TAP:{PRIMARY};400:WAIT:SCREEN:.*Running;'
        '600:SWIPE:0.5,0.01,0.5,0.45;600:WAIT:Entering activity: FrontlightPanel;'
        '12000:TAP:240,730;12000:WAIT:Exiting activity: FrontlightPanel;'
        '400:WAIT:SCREEN:.*2[45]:[0-9][0-9].*Paused.*Resume;300:QUIT')
    print('Control Center pauses the timer PASS', flush=True)


def settings_workflow():
    def saved(sd):
        data = json.loads((sd/'.crosspoint/settings.json').read_text())
        assert data['pomodoroFocusMinutes'] == 26, data
    # Opening settings pauses a running phase; the new length applies after Reset.
    run(1, 'settings',
        f'300:WAIT:SCREEN:.*25:00.*Ready;400:TAP:{PRIMARY};400:WAIT:SCREEN:.*Running;'
        f'1500:TAP:{GEAR};400:WAIT:Entering activity: PomodoroSettings;'
        f'400:SHOT:{ART}/settings-list.bmp;600:TAP:240,140;400:WAIT:Entering activity: PomodoroInterval;'
        f'400:SHOT:{ART}/settings-dialog.bmp;600:TAP:167,494;400:WAIT:SCREEN:.*26 min;'
        '400:TAP:300,494;400:WAIT:SCREEN:.*Timer settings.*Focus \\| 26 min;'
        f'400:TAP:{BACK};400:WAIT:SCREEN:.*Pomodoro.*2[45]:[0-9][0-9].*Paused;'
        f'400:TAP:{RESET};400:WAIT:SCREEN:.*26:00.*Ready;300:QUIT', check=saved)
    print('Settings pause timer, persist, and apply after reset PASS', flush=True)
    run(3, 'settings-load',
        '300:WAIT:SCREEN:.*40:00.*Ready.*Focus sessions: 0 / 6;300:QUIT',
        settings={'pomodoroFocusMinutes': 40, 'pomodoroSessionsPerCycle': 6})
    run(0, 'settings-clamp', '300:WAIT:SCREEN:.*90:00.*Ready.*Focus sessions: 0 / 2;300:QUIT',
        settings={'pomodoroFocusMinutes': 200, 'pomodoroSessionsPerCycle': 0})
    print('Saved settings load and out-of-range values clamp PASS', flush=True)


def power_policy():
    output = run(0, 'power-policy-runtime',
        f'400:TAP:{PRIMARY};400:WAIT:SCREEN:.*Running;'
        f'65000:TAP:{LEFT};65000:WAIT:SCREEN:.*Paused;'
        '65000:WAIT:Entering activity: Sleep;500:POWER;', timeout=155,
        settings={'sleepTimeoutMinutes': 1}, after_wake='0:WAIT:Entering activity: Home;500:QUIT')
    events = [line for line in output.splitlines() if '[ACT] Entering activity: Sleep' in line and not line.startswith('[SIM]')]
    assert len(events) == 1, 'Unexpected sleep while running'
    paused = next(line for line in output.splitlines() if line.startswith('[SIM] SCREEN:') and ' | Paused | ' in line)
    assert output.index(paused) < output.index(events[0])
    print('Running stays awake past timeout; paused auto-sleeps PASS', flush=True)


def reset_defaults():
    custom = {'pomodoroFocusMinutes': 40, 'pomodoroShortBreakMinutes': 10,
              'pomodoroLongBreakMinutes': 30, 'pomodoroSessionsPerCycle': 6}
    defaults = {'pomodoroFocusMinutes': 25, 'pomodoroShortBreakMinutes': 5,
                'pomodoroLongBreakMinutes': 15, 'pomodoroSessionsPerCycle': 4}

    def saved(expected):
        def check(sd):
            data = json.loads((sd/'.crosspoint/settings.json').read_text())
            assert {key: data.get(key) for key in expected} == expected, data
        return check
    # Cancel leaves custom values; Reset restores all four and a Ready timer follows.
    run(1, 'reset-defaults',
        f'300:WAIT:SCREEN:.*40:00.*Ready;600:TAP:{GEAR};400:WAIT:Entering activity: PomodoroSettings;'
        '600:TAP:240,386;400:WAIT:SCREEN:.*Reset timer settings\\?;600:TAP:240,440;'
        '400:WAIT:SCREEN:.*Timer settings.*Focus \\| 40 min;600:TAP:240,386;'
        '400:WAIT:SCREEN:.*Reset timer settings\\?;600:TAP:240,501;'
        '400:WAIT:SCREEN:.*Timer settings.*Focus \\| 25 min.*Short break \\| 5 min.*Long break \\| 15 min.*cycle \\| 4;'
        f'400:TAP:{BACK};400:WAIT:SCREEN:.*Pomodoro.*25:00.*Ready.*0 / 4;300:QUIT',
        settings=custom, check=saved(defaults))
    # Keys only: Down to the last row, Confirm, Down to Reset, Confirm.
    run(0, 'reset-defaults-keys',
        '300:WAIT:SCREEN:.*40:00.*Ready;400:UP;900:ENTER;400:WAIT:Entering activity: PomodoroSettings;'
        '0:WAIT:SCREEN:.*Timer settings.*Reset to defaults;'
        '500:DOWN;1000:DOWN;1500:DOWN;2000:DOWN;2600:ENTER;400:WAIT:SCREEN:.*Reset timer settings\\?;'
        '400:DOWN;900:ENTER;400:WAIT:SCREEN:.*Timer settings.*Focus \\| 25 min;300:QUIT',
        settings=custom, check=saved(defaults))
    print('Reset to defaults: cancel keeps values, confirm restores and applies PASS', flush=True)


def button_navigation():
    # Up/Down move focus (gear, primary, Reset); Confirm activates it. No touch.
    # Script delays count from the last WAIT, so steps in one segment increase.
    run(1, 'buttons',
        '300:WAIT:SCREEN:.*25:00.*Ready;400:ENTER;400:WAIT:SCREEN:.*Running.*Pause;'
        '1200:ENTER;400:WAIT:SCREEN:.*Paused.*Resume;400:ENTER;400:WAIT:SCREEN:.*Running;'
        '400:DOWN;900:ENTER;400:WAIT:SCREEN:.*25:00.*Ready;'
        '400:DOWN;900:ENTER;400:WAIT:Entering activity: PomodoroSettings;300:QUIT')
    run(0, 'buttons-up', '300:WAIT:SCREEN:.*Ready;400:UP;900:ENTER;'
        '400:WAIT:Entering activity: PomodoroSettings;300:QUIT')
    print('Up/Down focus and Confirm drive Start, Pause, Reset and settings PASS', flush=True)


def home_scroll_indicator():
    # Six shortcuts (with Sudoku) fit Lyra but not Lyra Extended (five rows); Plugins
    # adds a seventh, which overflows Lyra too.
    def ink(theme, name, extra=None):
        with tempfile.TemporaryDirectory(prefix=f'pomodoro-{name}-', dir=ART) as directory:
            sd = Path(directory)
            (sd/'.crosspoint').mkdir()
            (sd/'.crosspoint/settings.json').write_text(json.dumps(dict(uiTheme=theme, **(extra or {}))))
            shot = ART/f'{name}.bmp'
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd),
                       CROSSPOINT_SIM_INPUT_SCRIPT=f'0:WAIT:Entering activity: Home;800:SHOT:{shot};1500:QUIT')
            result = subprocess.run([str(PROGRAM)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=45)
            (LOGS/f'pomodoro-{name}.log').write_text(result.stdout + result.stderr)
            assert result.returncode == 0, name
        image = Image.open(shot).convert('L')
        strip = image.crop((464, 380, 480, 790))
        return sum(strip.histogram()[:64])
    plugins = {'pluginsEnabled': 1}
    assert ink(2, 'home-indicator-lyra-extended') > 100, 'Lyra Extended Home overflow has no indicator'
    assert ink(1, 'home-indicator-lyra-fits') == 0, 'Lyra Home draws an indicator although all rows fit'
    assert ink(1, 'home-indicator-lyra', plugins) > 100, 'Lyra Home overflow with Plugins has no indicator'
    print('Home scroll indicator only when rows overflow PASS', flush=True)


def launch(theme, name, script, settings=None, files=None):
    """Run one script on an isolated card with a books folder; return the log."""
    with tempfile.TemporaryDirectory(prefix=f'pomodoro-{name}-', dir=ART) as directory:
        sd = Path(directory)
        (sd/'.crosspoint').mkdir()
        (sd/'books').mkdir()
        (sd/'books/sample.txt').write_text('text')
        (sd/'.crosspoint/settings.json').write_text(json.dumps(dict(uiTheme=theme, **(settings or {}))))
        for relative, text in (files or {}).items():
            (sd/relative).write_text(text)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd),
                   CROSSPOINT_SIM_SCREEN_TEXT='1', CROSSPOINT_SIM_INPUT_SCRIPT=script)
        result = subprocess.run([str(PROGRAM)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)
        output = result.stdout + result.stderr
        (LOGS/f'pomodoro-{name}.log').write_text(output)
        assert result.returncode == 0 and 'WAIT timed out' not in output, name
        return output


def file_transfer_entry():
    # File Transfer moved from Home to the file browser's root header.
    home = '0:WAIT:Entering activity: Home;'
    output = launch(1, 'transfer-home', home + '300:QUIT')
    screens = [line for line in output.splitlines() if line.startswith('[SIM] SCREEN:') and 'Browse Files' in line]
    assert screens and 'File Transfer' not in screens[-1], 'Home still lists File Transfer'
    launch(1, 'transfer-touch', home + '400:TAP:240,346;0:WAIT:Entering activity: FileBrowser;'
           f'800:SHOT:{ART}/file-browser-root.bmp;1500:TAP:440,55;0:WAIT:Entering activity: CrossPointWebServer;300:QUIT')
    launch(0, 'transfer-keys', home + '400:ENTER;0:WAIT:Entering activity: FileBrowser;'
           '600:UP;1200:ENTER;0:WAIT:Entering activity: CrossPointWebServer;300:QUIT')
    # Top edges of the header buttons sit in the status band; they must not open Control Center.
    output = launch(1, 'transfer-top-edge', home + '400:TAP:240,346;0:WAIT:Entering activity: FileBrowser;'
                    '800:TAP:440,43;0:WAIT:Entering activity: CrossPointWebServer;300:QUIT')
    assert 'Entering activity: FrontlightPanel' not in output, 'Header icon edge opened Control Center'
    launch(1, 'transfer-control-center', home + '400:TAP:240,346;0:WAIT:Entering activity: FileBrowser;'
           '800:SWIPE:0.5,0.01,0.5,0.45;0:WAIT:Entering activity: FrontlightPanel;300:QUIT')
    output = launch(1, 'transfer-subfolder', home + '400:TAP:240,346;0:WAIT:Entering activity: FileBrowser;'
                    '800:ENTER;0:WAIT:SCREEN:.*sample;800:TAP:440,55;2000:QUIT')
    assert 'Entering activity: CrossPointWebServer' not in output, 'Subfolder header opened File Transfer'
    print('File Transfer opens from the browser root by touch and keys, not from subfolders PASS', flush=True)


def file_transfer_back():
    # Back goes up one level: option -> File Transfer page -> file browser root.
    # The device reboots to that page after Wi-Fi; the simulator's restart is a no-op.
    page = ('0:WAIT:Entering activity: Home;400:TAP:240,346;0:WAIT:Entering activity: FileBrowser;'
            '800:TAP:440,55;0:WAIT:Entering activity: NetworkModeSelection;')
    back = '1500:TAP:30,63;'
    launch(1, 'back-page', page + back + '0:WAIT:Entering activity: FileBrowser;300:QUIT')
    output = launch(1, 'back-hotspot', page + '800:TAP:240,296;0:WAIT:Access Point started;' + back +
                    '0:WAIT:Silent restart \\(target=file-transfer\\);0:WAIT:Entering activity: NetworkModeSelection;300:QUIT')
    launch(1, 'back-calibre', page + '800:TAP:240,220;0:WAIT:Entering activity: CalibreConnect;800:WAIT:Entering activity: WifiSelection;'
           + back + '0:WAIT:Entering activity: NetworkModeSelection;300:QUIT')
    launch(1, 'back-join', page + '800:TAP:240,145;0:WAIT:Entering activity: WifiSelection;' + back +
           '0:WAIT:Entering activity: NetworkModeSelection;300:QUIT')
    launch(1, 'back-opds', page + '800:TAP:240,380;0:WAIT:Entering activity: OpdsServerList;' + back +
           '0:WAIT:Entering activity: NetworkModeSelection;300:QUIT')
    # One saved server opens the catalog directly; it turns Wi-Fi on itself.
    opds = {'.crosspoint/opds.json': json.dumps({'servers': [{'name': 'Test', 'url': 'http://127.0.0.1:9/opds'}]})}
    output = launch(1, 'back-opds-catalog', page + '800:TAP:240,380;0:WAIT:Entering activity: OpdsBookBrowser;'
                    '0:WAIT:Entering activity: WifiSelection;1500:TAP:30,63;0:WAIT:SCREEN:.*connection failed;1500:TAP:30,63;'
                    '0:WAIT:Silent restart \\(target=file-transfer\\);0:WAIT:Entering activity: NetworkModeSelection;300:QUIT',
                    settings={'pluginsEnabled': 1}, files=opds)
    assert 'Silent restart (target=home)' not in output, 'OPDS catalog Back rebooted to Home'
    output = launch(1, 'home-hotspot', page + '800:TAP:240,296;0:WAIT:Access Point started;'
                    '1500:SWIPE:0.5,0.98,0.5,0.5;0:WAIT:Silent restart \\(target=home\\);0:WAIT:Entering activity: Home;300:QUIT')
    print('File Transfer Back returns one level; Home gesture still goes Home PASS', flush=True)


def main():
    with ThreadPoolExecutor(max_workers=3) as executor:
        for job in [executor.submit(theme_workflow, theme) for theme in range(5)]:
            job.result()
    with ThreadPoolExecutor(max_workers=3) as executor:
        for job in [executor.submit(crowded_home, theme) for theme in range(5)]:
            job.result()
    stationary_home_touch()
    repaint_and_sleep()
    control_center()
    settings_workflow()
    reset_defaults()
    button_navigation()
    home_scroll_indicator()
    file_transfer_entry()
    file_transfer_back()
    print('Pomodoro simulator workflows passed; Todo data unchanged')


if __name__ == '__main__':
    import sys
    if '--power' in sys.argv:
        power_policy()
    else:
        main()
