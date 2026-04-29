# Aurora interpreter and owner-state cleanup design

## Goal

Make Aurora startup deterministic and safe when previous Companion or Qt bridge processes are stale.

## Requirements

- `launch.ps1` resolves Python once and exports it as `AURORA_PYTHON`.
- `aurora_companion.py` prefers `AURORA_PYTHON` for the Qt bridge so Companion and bridge use the same interpreter.
- Qt bridge cleanup must only stop the process that owns the Qt bridge port or the PID recorded in `.qt_bridge_owner.json`.
- Qt bridge cleanup must not globally stop every `qt_camera_bridge.py` process.
- Companion gets its own owner state so launcher can recycle only the previous owned Companion instance or the current listener on the selected port.
- Existing manual Aurora.exe bootstrap flow stays unchanged.

## Design

### `launch.ps1`

`Get-PythonExecutable` remains the single source for the Companion interpreter. After resolving it, launch exports `$env:AURORA_PYTHON` before starting `aurora_companion.py`.

Add Companion owner-state helpers mirroring Qt bridge ownership:

- `Load-OwnerState(path)` reads JSON when present.
- `Stop-OwnedProcess(statePath, scriptName)` stops only the recorded PID when its command line still contains the expected script.
- `Save-CompanionOwnerState(path, pid, port, script)` records the launched Companion process when using `Start-Process -PassThru` or equivalent controlled launch.
- `Clear-CompanionOwnerState(path)` removes state when stale or after process exits.

Port cleanup stays narrow: inspect `Get-NetTCPConnection -LocalPort <port> -State Listen`, then stop only the owning process whose command line contains `aurora_companion.py`. No global Python process scan.

Qt bridge cleanup changes similarly: inspect the Qt bridge port listener and `.qt_bridge_owner.json`; stop matching PIDs only when command line still contains `qt_camera_bridge.py`. Remove the existing global scan over all Python processes containing `qt_camera_bridge.py`.

### `aurora_companion.py`

Update `_select_qt_bridge_python()`:

1. Read `AURORA_PYTHON`.
2. If set, existent or command-resolvable, and `_python_has_module(..., "PySide6")` passes, return it.
3. Otherwise continue through existing candidates.

Qt bridge process state stays in `.qt_bridge_owner.json`. Existing `save_qt_bridge_owner_state`, `cleanup_qt_bridge_owner_process`, and `shutdown_qt_bridge` behavior remains, except stale-port cleanup must not perform broad global process scans.

### Tests

Extend `tools/aurora/tests/test_qt_bridge_lifecycle.py`:

- `AURORA_PYTHON` is selected when it has PySide6.
- invalid or PySide6-missing `AURORA_PYTHON` falls back to existing candidates.
- stale Qt bridge cleanup does not include global Python process sweep behavior.
- owner-state cleanup still clears `.qt_bridge_owner.json` after stopping recorded PID.

PowerShell launcher behavior is validated by keeping functions small and command-line checks explicit; no broad process matching remains in script text.

## Risks

- PowerShell process launching must still stream Companion output normally. If `Start-Process -PassThru` would hide logs, keep direct invocation for foreground run and write owner state from a tiny wrapper around current process PID when feasible.
- `AURORA_PYTHON` may point at an interpreter without PySide6. Fallback preserves current behavior.

## Success criteria

- Companion and Qt bridge run under same Python when `launch.ps1` resolves a valid interpreter.
- Restarting launch frees stale owned Companion and Qt bridge instances without killing unrelated bridges.
- `python -m pytest tools/aurora/tests/test_qt_bridge_lifecycle.py -q` passes.
- `python -m py_compile tools/aurora/aurora_companion.py` passes.
