# Todo checks

See `docs/picco-todo.md` for commands, persistence format, resource bounds, and
physical testing requirements. `run.sh` tests the real model/store with
ArduinoJson and injected storage failures. `simulator.py` exercises the actual
activity stack and host SD across process restarts using scripted touch only.
No device is accessed, uploaded, or flashed.

`visuals.py` captures all five built-in themes (empty, wrapped/completed tasks,
delete selection and errors), checks that opening/selecting screens does not
rewrite task snapshots, and saves PNGs under `../qa-artifacts/todo-native/`.
There is one fixed UI font tier; no adjustable scale setting exists.
