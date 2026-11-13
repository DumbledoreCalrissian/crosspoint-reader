#!/usr/bin/env python3
"""Check release provenance and compile the real silent-boot routing branches."""
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
main = (ROOT / 'src/main.cpp').read_text()
catalog = (ROOT / 'lib/Sudoku/SudokuCatalog.cpp').read_text()
pins = re.findall(r'\{(\d+), (\d+), "([0-9a-f]{64})"\}', catalog)
provenance = ROOT.parent / 'qa-artifacts/sudoku-bank/PROVENANCE.json'
if provenance.exists():
    expected = json.loads(provenance.read_text())['files']
    assert [(int(c), int(b), h) for c, b, h in pins] == [(p['records'], p['output_bytes'], p['output_sha256']) for p in expected]
    proposal = (ROOT.parent / 'GAMES_PROPOSAL.md').read_text()
    for count, size, digest in pins:
        assert digest in proposal and f'{int(size):,}' in proposal and f'{int(count):,}' in proposal
else:
    assert len(pins) == 4
    print('Workspace provenance unavailable; only four complete firmware pins checked')
# Compare non-comment, non-whitespace tokens: simulator must use the pinned SDK algorithm.
def tokens(text):
    return re.findall(r'\w+|[^\s]', re.sub(r'//[^\n]*', '', text))
assert tokens((ROOT.parent / 'crosspoint-simulator/src/ResumableFetch.h').read_text()) == tokens(
    (ROOT / 'freeink-sdk/libs/network/SecureNet/include/ResumableFetch.h').read_text())

constants = '\n'.join(re.findall(r'^constexpr uint32_t SILENT_REBOOT_TARGET_.*;', main, re.M))
start = main.index('  } else if (resume == BootResume::Silent && snapshotTarget == SILENT_REBOOT_TARGET_READER')
end = main.index('  } else if (APP_STATE.openEpubPath.empty()', start)
routing = main[start:end].replace('  } else if', '  if', 1) + '  }'
source = r'''
#include <cassert>
#include <cstdint>
#include <string>
enum class BootResume { Silent, Splash };
enum class SettingsSection { Network };
struct { std::string openEpubPath = "book.epub"; } APP_STATE;
struct Manager {
  std::string target;
  void goToReader(const std::string&) { target = "reader"; }
  void goToJoinNetwork() { target = "join"; }
  void goToFileTransfer() { target = "transfer"; }
  void goToFileBrowser(const char*) { target = "browser"; }
  void goToSudokuSettings() { target = "sudoku"; }
  void goToSettings(SettingsSection) { target = "settings"; }
  void goHome() { target = "home"; }
} activityManager;
'''+constants+'\nvoid route(uint32_t snapshotTarget) { auto resume = BootResume::Silent;\n'+routing+r'''
}
int main() {
  const char* expected[] = {"home", "reader", "settings", "join", "transfer", "browser", "sudoku"};
  for (uint32_t i = SILENT_REBOOT_TARGET_HOME; i <= SILENT_REBOOT_TARGET_MAX; ++i) {
    route(i); assert(activityManager.target == expected[i]);
  }
  APP_STATE.openEpubPath.clear(); route(SILENT_REBOOT_TARGET_SUDOKU_SETTINGS);
  assert(activityManager.target == "sudoku");
  route(SILENT_REBOOT_TARGET_READER); assert(activityManager.target == "home");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'boot.cpp').write_text(source)
    subprocess.run(['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', str(path / 'boot.cpp'), '-o', str(path / 'boot')], check=True)
    subprocess.run([str(path / 'boot')], check=True)
print('Pinned provenance, SDK fetch parity and all seven real boot routing branches PASS')

# Execute the production transfer prologue while another thread attempts a render.
settings_source = (ROOT / 'src/activities/sudoku/SudokuSettingsActivity.cpp').read_text()
start = settings_source.index('\n  {', settings_source.index('void SudokuSettingsActivity::transfer()'))
end = settings_source.index('\n  cancelRequested = false;', start)
prologue = settings_source[start:end]
concurrency = r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>
#include <thread>
std::mutex renderMutex;
std::atomic<bool> releasing{false}, observed{false}, rendering{false}, allowRelease{false};
struct Cache {
  void releaseSdFontCaches() {
    releasing = true;
    while (!allowRelease) std::this_thread::yield();
  }
};
struct Renderer { Cache cache; Cache* getFontCacheManager() { return &cache; } };
struct RenderLock { std::unique_lock<std::mutex> lock; template<class T> RenderLock(T&) : lock(renderMutex) {} };
struct Settings {
  Renderer renderer;
  int received = 1, total = 2, completed = 3;
  void closeRouting() {}
  void beginTransfer() {
'''+prologue+r'''
  }
};
int main() {
  Settings settings;
  std::thread transfer([&] { settings.beginTransfer(); });
  while (!releasing) std::this_thread::yield();
  std::thread render([] {
    if (renderMutex.try_lock()) {
      rendering = true; renderMutex.unlock(); observed = true;
    } else {
      observed = true; std::lock_guard<std::mutex> lock(renderMutex); rendering = true;
    }
  });
  while (!observed) std::this_thread::yield();
  const bool overlapped = rendering;
  allowRelease = true;
  transfer.join(); render.join();
  if (overlapped || !rendering) return 1;
  assert(settings.received == 0 && settings.total == 0 && settings.completed == 0);
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'race.cpp').write_text(concurrency)
    subprocess.run(['c++', '-std=c++20', '-pthread', '-Wall', '-Wextra', '-Werror',
                    str(path / 'race.cpp'), '-o', str(path / 'race')], check=True)
    subprocess.run([str(path / 'race')], check=True, timeout=5)
    release = '    if (auto* cache = renderer.getFontCacheManager()) cache->releaseSdFontCaches();'
    assert release in prologue
    unlocked = prologue.replace(release, '') + '\n' + release
    (path / 'unsafe.cpp').write_text(concurrency.replace(prologue, unlocked))
    subprocess.run(['c++', '-std=c++20', '-pthread', '-Wall', '-Wextra', '-Werror',
                    str(path / 'unsafe.cpp'), '-o', str(path / 'unsafe')], check=True)
    assert subprocess.run([str(path / 'unsafe')], timeout=5).returncode == 1
print('Production transfer prologue serializes cache release against a concurrent render PASS')

# Exercise the real sleep decision with the real delayed-click classifier.
sleep_decision = main.split('bool requestManualSleep() {', 1)[1].split('\nvoid loop()', 1)[0]
settings_header = (ROOT / 'src/CrossPointSettings.h').read_text()
actions = 'enum SHORT_PWRBTN {' + settings_header.split('enum SHORT_PWRBTN {', 1)[1].split('};', 1)[0] + '};'
sleep_completion = settings_source.split('  if (requestManualSleep()) {', 1)[1].split(
    '  mappedInput.resetHomeButtonInput();', 1)[0]
custom_input = settings_source.split('bool SudokuSettingsActivity::handleCustomInput() {', 1)[1].split(
    '\nvoid SudokuSettingsActivity::onBackButton()', 1)[0]
states = re.search(r'enum class State : uint8_t \{[^}]+\};',
                  (ROOT / 'src/activities/sudoku/SudokuSettingsActivity.h').read_text()).group()
sleep_harness = r'''
#include <cassert>
#include <cstdint>
#include <string>
#include "util/PowerButtonInput.h"
#define LOG_DBG(...) ((void)0)
unsigned long now = 1000;
unsigned long millis() { return now; }
struct GPIO {
  enum { BTN_POWER, BTN_DOWN };
  bool power = false, down = false;
  unsigned long held = 0;
  bool isPressed(int button) { return button == BTN_POWER ? power : down; }
  unsigned long getPowerButtonHeldTime() { return held; }
} gpio;
using HalGPIO = GPIO;
namespace BoardConfig { bool x4Pro = false; bool isX4Pro() { return x4Pro; } }
struct CrossPointSettings {
  ACTIONS
  int shortPwrBtn = IGNORE, doublePwrBtn = PWR_FRONTLIGHT;
  unsigned long getPowerButtonDuration() { return shortPwrBtn == SLEEP ? 10 : 400; }
} SETTINGS;
struct Input {
  PowerButtonInput clicks;
  bool sleep = false, deferredSleep = false;
  bool powerSleepClick() { return sleep; }
  void resetPowerButtonInput() { clicks.reset(); sleep = deferredSleep = false; }
  void sample(bool released = false) {
    sleep = false;
    if (BoardConfig::x4Pro && !gpio.down) {
      sleep = clicks.update(now, gpio.power, released, gpio.held) == PowerButtonInput::Click::Single;
    } else clicks.reset();
    deferredSleep |= sleep;
  }
} mappedInputManager;
unsigned long allowSleepAt = 0;
bool wakePowerReleasePending = false;
bool requestManualSleep() { SLEEP_DECISION
struct RenderLock { template<class T> RenderLock(T&) {} };
struct Settings {
  STATES
  State state = State::Transferring;
  unsigned transfers = 0;
  void closeRouting() {}
  void requestUpdate() {}
  void transfer() { ++transfers; }
  void completeTransfer() { if (requestManualSleep()) { SLEEP_COMPLETION }
  bool handleCustomInput() { CUSTOM_INPUT
};
int main(int argc, char** argv) {
  assert(argc == 2);
  const bool screenshot = std::string(argv[1]) == "screenshot";
  const bool wake = std::string(argv[1]) == "wake";
  if (wake) {
    gpio.power = true; gpio.held = 2000;
    assert(!requestManualSleep());
  }
  gpio.power = false; gpio.held = 0;
  assert(!requestManualSleep());
  if (screenshot) {
    BoardConfig::x4Pro = true;
    SETTINGS.shortPwrBtn = CrossPointSettings::SLEEP;
    gpio.power = gpio.down = true;
    mappedInputManager.sample(); assert(!requestManualSleep());
    now += 100; gpio.down = false; gpio.held = 100;
    mappedInputManager.sample(); assert(!requestManualSleep());
    now += 50; gpio.power = false; gpio.held = 150;
    mappedInputManager.sample(true); assert(!requestManualSleep());
    now += 501;
    mappedInputManager.sample(); assert(!requestManualSleep());
    assert(!mappedInputManager.deferredSleep);
  }
  gpio.power = true; gpio.held = 500; now += 1000;
  assert(requestManualSleep());
  gpio.power = false; mappedInputManager.sample();
  assert(requestManualSleep());  // Unwinding must retain the request after release.
  Settings settings;
  settings.completeTransfer();
  assert(settings.state == Settings::State::AwaitingSleep);
  assert(settings.handleCustomInput() && settings.transfers == 0);
}
'''
sleep_harness = (sleep_harness.replace('ACTIONS', actions).replace('SLEEP_DECISION', sleep_decision)
                 .replace('SLEEP_COMPLETION', sleep_completion).replace('CUSTOM_INPUT', custom_input)
                 .replace('STATES', states))
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    source = path / 'sleep.cpp'
    source.write_text(sleep_harness)
    command = ['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'src'),
               str(source), '-o', str(path / 'sleep')]
    subprocess.run(command, check=True)
    for scenario in ('hold', 'wake', 'screenshot'):
        subprocess.run([str(path / 'sleep'), scenario], check=True)
    source.write_text(sleep_harness.replace('    mappedInputManager.resetPowerButtonInput();', ''))
    subprocess.run(command, check=True)
    assert subprocess.run([str(path / 'sleep'), 'screenshot'], capture_output=True).returncode != 0
print('Manual sleep retention, wake/screenshot guards and no transfer reentry PASS (unsafe variant rejected)')
