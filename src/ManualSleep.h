#pragma once

// Inspect the current input snapshot and latch a manual sleep request. Blocking
// transfers must unwind before the main loop performs sleep and activity teardown.
bool requestManualSleep();
