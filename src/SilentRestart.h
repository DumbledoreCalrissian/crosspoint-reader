#pragma once

// ESP.restart() with an RTC_NOINIT flag that survives the reboot, so setup()
// skips the boot splash and routes straight to a destination. Used to clear
// heap fragmentation accumulated during a wifi session. The live frontlight
// state rides along in the same flag so the reboot is invisible: the light
// comes back exactly as it was, regardless of the Restore Light on Wake
// preference.

void silentRestart();          // home screen
void silentRestartToReader();  // currently-open EPUB (APP_STATE.openEpubPath)
void silentRestartToSudokuSettings();
void silentRestartToSettings();      // settings screen
void silentRestartToFileTransfer();  // File Transfer's mode page
void silentRestartToFileBrowser();   // file browser root (File Transfer's entry point)
// Reboot into the File Transfer > Join Network flow on a pristine heap, so the
// WiFi + TLS working set has the contiguous RAM it needs on tight boards. A
// no-op on touch boards (a soft reset would cycle their externally-powered
// rails); the caller then proceeds without a reboot.
void silentRestartToJoinNetwork();

// Reboot immediately after an activity releases exclusive raw storage. The RTC
// target lands on Home or on File Transfer's mode page (where USB Drive opens)
// instead of resuming a reader.
void restartToHomeAfterStorageHandoff();
void restartToFileTransferAfterStorageHandoff();
