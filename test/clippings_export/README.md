# Clipping export book exclusion

Build `simulator_picco`, then run from the firmware checkout:

```sh
sh test/clippings_export/run.sh
../.venv/bin/python test/clippings_export/simulator.py
../.venv/bin/python test/clippings_export/simulator.py --http
```

Cards are isolated under workspace QA artifacts. Checks cover removal of saved
root-export recents, three Home themes, Library indexing, a nested same-named
book, viewing the export from Browse Files and optional localhost HTTP listing
and byte-for-byte download. The HTTP check needs permission to bind/connect to
localhost ports 18090/18091.

For upgrade verification, `--legacy-card PATH` copies a card produced by the
pre-fix simulator that contains the root export in its version-5 Library index.
It checks automatic index migration and preserves the other books' `firstSeen`
values. The host LibraryBuilder tests independently verify version migration.

The existing FsHelpers and LibraryBuilder CMake tests cover root aliases, FAT
case matching, nested paths and continued TXT recognition. Physical USB/Wi-Fi
and SD behavior remain owner checks.
The standalone recent-store test uses the real persistence template and
ArduinoJson with fake storage I/O under ASan/UBSan. It checks deferred migration
resave, add/rename exclusion, nested files and a failed migration resave.
