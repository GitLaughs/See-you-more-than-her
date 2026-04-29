# Aurora Owner State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Aurora use one Python interpreter for Companion and Qt bridge, and clean stale Companion/Qt processes only by port owner or owner-state PID.

**Architecture:** `launch.ps1` owns process lifecycle at startup: resolve Python once, export it, clean exact stale owners, then run Companion. `aurora_companion.py` consumes `AURORA_PYTHON` for Qt bridge interpreter selection and keeps Qt owner-state cleanup narrow. Tests cover interpreter selection and removal of broad Qt bridge process sweeps.

**Tech Stack:** PowerShell launcher, Python 3, Flask Companion, PySide6 Qt bridge, unittest/pytest.

---

## File structure

- Modify `tools/aurora/launch.ps1`: Python env export, Companion owner state, narrow Companion/Qt cleanup.
- Modify `tools/aurora/aurora_companion.py`: prefer `AURORA_PYTHON`; remove broad Qt bridge Python sweep from stale cleanup.
- Modify `tools/aurora/tests/test_qt_bridge_lifecycle.py`: add interpreter env tests and stale cleanup behavior test.
- Create runtime file at execution time only: `tools/aurora/.companion_owner.json` (not source-controlled).

---

### Task 1: Add Qt bridge interpreter env tests

**Files:**
- Modify: `tools/aurora/tests/test_qt_bridge_lifecycle.py`

- [ ] **Step 1: Add failing tests**

Append these methods inside `QtBridgeLifecycleTests` before `if __name__ == "__main__":`:

```python
    def test_select_qt_bridge_python_prefers_aurora_python_when_pyside6_available(self):
        with mock.patch.dict(aurora_companion.os.environ, {"AURORA_PYTHON": r"C:\\AuroraPython\\python.exe"}), \
             mock.patch.object(aurora_companion.Path, "exists", return_value=True), \
             mock.patch.object(aurora_companion, "_python_has_module", return_value=True) as has_module_mock:
            selected = aurora_companion._select_qt_bridge_python()

        self.assertEqual(selected, r"C:\\AuroraPython\\python.exe")
        has_module_mock.assert_called_once_with(r"C:\\AuroraPython\\python.exe", "PySide6")

    def test_select_qt_bridge_python_falls_back_when_aurora_python_lacks_pyside6(self):
        fallback = r"C:\\FallbackPython\\python.exe"

        def has_module(candidate, module_name):
            return candidate == fallback and module_name == "PySide6"

        with mock.patch.dict(aurora_companion.os.environ, {"AURORA_PYTHON": r"C:\\BadPython\\python.exe"}), \
             mock.patch.object(aurora_companion.Path, "exists", return_value=True), \
             mock.patch.object(aurora_companion.Path, "__str__", return_value=fallback), \
             mock.patch.object(aurora_companion, "_python_has_module", side_effect=has_module):
            with mock.patch.object(aurora_companion.sys, "executable", fallback):
                selected = aurora_companion._select_qt_bridge_python()

        self.assertEqual(selected, fallback)
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
python -m pytest tools/aurora/tests/test_qt_bridge_lifecycle.py -q
```

Expected: at least `test_select_qt_bridge_python_prefers_aurora_python_when_pyside6_available` fails because `_select_qt_bridge_python()` ignores `AURORA_PYTHON`.

- [ ] **Step 3: Commit failing tests**

```bash
git add tools/aurora/tests/test_qt_bridge_lifecycle.py
git commit -m "test: cover Aurora Python selection"
```

---

### Task 2: Prefer `AURORA_PYTHON` in Companion

**Files:**
- Modify: `tools/aurora/aurora_companion.py:599-618`
- Test: `tools/aurora/tests/test_qt_bridge_lifecycle.py`

- [ ] **Step 1: Update `_select_qt_bridge_python()`**

Replace `tools/aurora/aurora_companion.py:599-618` with:

```python
def _select_qt_bridge_python() -> str:
    repo_root = Path(__file__).resolve().parents[2]
    candidates = []
    aurora_python = os.environ.get("AURORA_PYTHON")
    if aurora_python:
        candidates.append(Path(aurora_python))
    candidates.extend([
        repo_root / "venv_39" / "Scripts" / "python.exe",
        Path(sys.executable),
    ])
    if sys.platform == "win32":
        candidates.append(Path("python"))
    seen = set()
    for candidate in candidates:
        python_exe = str(candidate)
        key = python_exe.lower()
        if key in seen:
            continue
        seen.add(key)
        if python_exe != "python" and not candidate.exists():
            continue
        if _python_has_module(python_exe, "PySide6"):
            return python_exe
    return sys.executable
```

- [ ] **Step 2: Run lifecycle tests**

Run:

```bash
python -m pytest tools/aurora/tests/test_qt_bridge_lifecycle.py -q
```

Expected: all tests pass.

- [ ] **Step 3: Commit implementation**

```bash
git add tools/aurora/aurora_companion.py tools/aurora/tests/test_qt_bridge_lifecycle.py
git commit -m "fix: share Aurora Python with Qt bridge"
```

---

### Task 3: Remove broad Qt bridge process sweep in Companion

**Files:**
- Modify: `tools/aurora/aurora_companion.py:621-658`
- Modify: `tools/aurora/tests/test_qt_bridge_lifecycle.py`

- [ ] **Step 1: Add failing cleanup test**

Append this method inside `QtBridgeLifecycleTests`:

```python
    def test_stop_stale_qt_bridge_on_port_does_not_sweep_all_python_processes(self):
        captured = {}

        def fake_run(command, **kwargs):
            captured["script"] = command[-1]
            return mock.Mock(stdout=b"", returncode=0)

        with mock.patch.object(aurora_companion.sys, "platform", "win32"), \
             mock.patch.object(aurora_companion.subprocess, "run", side_effect=fake_run):
            aurora_companion._stop_stale_qt_bridge_on_port()

        self.assertIn("Get-NetTCPConnection", captured["script"])
        self.assertNotIn("Get-CimInstance Win32_Process -Filter \"Name='python.exe' OR Name='pythonw.exe'\"", captured["script"])
```

- [ ] **Step 2: Run test to verify failure**

Run:

```bash
python -m pytest tools/aurora/tests/test_qt_bridge_lifecycle.py::QtBridgeLifecycleTests::test_stop_stale_qt_bridge_on_port_does_not_sweep_all_python_processes -q
```

Expected: fails because script still contains global Python sweep.

- [ ] **Step 3: Replace `_stop_stale_qt_bridge_on_port()`**

Replace `tools/aurora/aurora_companion.py:621-658` with:

```python
def _stop_stale_qt_bridge_on_port() -> None:
    if sys.platform != "win32":
        return
    script = rf"""
$connections = Get-NetTCPConnection -LocalPort {QT_BRIDGE_PORT} -State Listen -ErrorAction SilentlyContinue
foreach ($conn in $connections) {{
    $procId = [int]$conn.OwningProcess
    if ($procId -le 0) {{ continue }}
    $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$procId" -ErrorAction SilentlyContinue
    if (-not $proc) {{ continue }}
    $cmd = [string]$proc.CommandLine
    if ($cmd -match "qt_camera_bridge\.py") {{
        Write-Host "[Aurora] Terminating stale Qt camera bridge on port {QT_BRIDGE_PORT} (PID $procId)"
        Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
    }}
}}
Start-Sleep -Milliseconds 400
"""
    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-Command", script],
            capture_output=True,
            timeout=8,
            check=False,
        )
        if result.stdout and len(result.stdout) > 0:
            print(result.stdout.decode('utf-8', errors='replace'))
    except Exception as e:
        print(f"[WARN] Stopping stale Qt bridge: {e}")
```

- [ ] **Step 4: Run lifecycle tests**

Run:

```bash
python -m pytest tools/aurora/tests/test_qt_bridge_lifecycle.py -q
```

Expected: all tests pass.

- [ ] **Step 5: Commit cleanup narrowing**

```bash
git add tools/aurora/aurora_companion.py tools/aurora/tests/test_qt_bridge_lifecycle.py
git commit -m "fix: limit Qt bridge stale cleanup"
```

---

### Task 4: Add owner-state helpers to launcher

**Files:**
- Modify: `tools/aurora/launch.ps1`

- [ ] **Step 1: Add owner-state helper functions**

Insert after `Test-PortAvailable` in `tools/aurora/launch.ps1`:

```powershell
function Read-OwnerState {
    param([string]$StatePath)
    try {
        if (-not (Test-Path $StatePath)) { return $null }
        return Get-Content -Raw -Encoding UTF8 $StatePath | ConvertFrom-Json
    } catch {
        return $null
    }
}

function Clear-OwnerState {
    param([string]$StatePath)
    try { Remove-Item -Force $StatePath -ErrorAction SilentlyContinue } catch {}
}

function Stop-OwnedProcess {
    param(
        [string]$StatePath,
        [string]$ScriptName,
        [string]$Label
    )
    $state = Read-OwnerState -StatePath $StatePath
    if (-not $state -or -not $state.pid) {
        Clear-OwnerState -StatePath $StatePath
        return @()
    }
    $ownedPid = [int]$state.pid
    if ($ownedPid -le 0 -or $ownedPid -eq $PID) {
        Clear-OwnerState -StatePath $StatePath
        return @()
    }
    try {
        $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$ownedPid" -ErrorAction SilentlyContinue
        if ($proc -and ([string]$proc.CommandLine) -match [regex]::Escape($ScriptName)) {
            Write-Host "[Aurora] Releasing stale $Label owner (PID $ownedPid)"
            Stop-Process -Id $ownedPid -Force -ErrorAction SilentlyContinue
            Clear-OwnerState -StatePath $StatePath
            return @($ownedPid)
        }
    } catch {}
    Clear-OwnerState -StatePath $StatePath
    return @()
}

function Save-OwnerState {
    param(
        [string]$StatePath,
        [int]$OwnerPid,
        [int]$OwnerPort,
        [string]$ScriptPath
    )
    $payload = [ordered]@{
        pid = $OwnerPid
        port = $OwnerPort
        script = $ScriptPath
    }
    try {
        $payload | ConvertTo-Json -Compress | Set-Content -Encoding UTF8 $StatePath
    } catch {}
}
```

- [ ] **Step 2: Run PowerShell parser check**

Run:

```bash
powershell -NoProfile -Command "$null = [System.Management.Automation.Language.Parser]::ParseFile('tools/aurora/launch.ps1',[ref]$null,[ref]$null); 'parse-ok'"
```

Expected: prints `parse-ok`.

- [ ] **Step 3: Commit helpers**

```bash
git add tools/aurora/launch.ps1
git commit -m "fix: add Aurora owner-state helpers"
```

---

### Task 5: Narrow launcher cleanup and export Python

**Files:**
- Modify: `tools/aurora/launch.ps1:137-198`
- Modify: `tools/aurora/launch.ps1:216-229`

- [ ] **Step 1: Replace Companion cleanup function**

Replace `Stop-StaleCompanionOnPort` in `tools/aurora/launch.ps1:137-153` with:

```powershell
function Stop-StaleCompanionOnPort {
    param(
        [int]$BindPort,
        [string]$OwnerStatePath
    )
    $killedPids = @(Stop-OwnedProcess -StatePath $OwnerStatePath -ScriptName "aurora_companion.py" -Label "Companion")
    try {
        $connections = Get-NetTCPConnection -LocalPort $BindPort -State Listen -ErrorAction SilentlyContinue
        foreach ($conn in $connections) {
            $pId = [int]$conn.OwningProcess
            if ($pId -le 0 -or $pId -eq $PID -or $killedPids -contains $pId) { continue }
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$pId" -ErrorAction SilentlyContinue
            if ($proc -and ([string]$proc.CommandLine) -match "aurora_companion\.py") {
                Write-Host "[Aurora] Releasing stale Companion port $BindPort (PID $pId)"
                Stop-Process -Id $pId -Force -ErrorAction SilentlyContinue
                $killedPids += $pId
            }
        }
    } catch {}
}
```

- [ ] **Step 2: Replace Qt bridge cleanup function**

Replace `Stop-StaleQtBridge` in `tools/aurora/launch.ps1:155-198` with:

```powershell
function Stop-StaleQtBridge {
    param([string]$OwnerStatePath)
    $QtBridgePort = 5911
    $killedPids = @(Stop-OwnedProcess -StatePath $OwnerStatePath -ScriptName "qt_camera_bridge.py" -Label "Qt camera bridge")
    try {
        $connections = Get-NetTCPConnection -LocalPort $QtBridgePort -State Listen -ErrorAction SilentlyContinue
        foreach ($conn in $connections) {
            $pId = [int]$conn.OwningProcess
            if ($pId -le 0 -or $pId -eq $PID -or $killedPids -contains $pId) { continue }
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$pId" -ErrorAction SilentlyContinue
            if ($proc -and ([string]$proc.CommandLine) -match "qt_camera_bridge\.py") {
                Write-Host "[Aurora] Terminating stale Qt camera bridge on port $QtBridgePort (PID $pId)"
                Stop-Process -Id $pId -Force -ErrorAction SilentlyContinue
                $killedPids += $pId
            }
        }
    } catch {}
    if ($killedPids.Count -gt 0) {
        $deadline = [DateTime]::UtcNow.AddSeconds(5)
        while ([DateTime]::UtcNow -lt $deadline) {
            $allGone = $true
            foreach ($kpid in $killedPids) {
                if (Get-Process -Id $kpid -ErrorAction SilentlyContinue) {
                    $allGone = $false
                    break
                }
            }
            if ($allGone) { break }
            Start-Sleep -Milliseconds 200
        }
        Start-Sleep -Milliseconds 500
    }
}
```

- [ ] **Step 3: Update launch bottom section**

Replace `tools/aurora/launch.ps1:216-229` with:

```powershell
Initialize-Utf8Console
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$Python = Get-PythonExecutable
$env:AURORA_PYTHON = $Python
$CompanionOwnerState = Join-Path $ScriptDir ".companion_owner.json"
$QtBridgeOwnerState = Join-Path $ScriptDir ".qt_bridge_owner.json"
Start-AuroraBootstrap -Disabled:$SkipAurora
Stop-StaleCompanionOnPort -BindPort $Port -OwnerStatePath $CompanionOwnerState
Stop-StaleQtBridge -OwnerStatePath $QtBridgeOwnerState
Wait-PortReleased -BindHost $ListenHost -BindPort $Port -TimeoutSeconds 5 | Out-Null
$ResolvedPort = Resolve-AvailablePort -BindHost $ListenHost -PreferredPort $Port
$browserUrl = "http://127.0.0.1:$ResolvedPort"

Start-BrowserWhenReady -ReadyPort $ResolvedPort -Url $browserUrl
try {
    Save-OwnerState -StatePath $CompanionOwnerState -OwnerPid $PID -OwnerPort $ResolvedPort -ScriptPath (Join-Path $ScriptDir "aurora_companion.py")
    & $Python aurora_companion.py --device $Device --port $ResolvedPort --host $ListenHost --source $Source
} finally {
    Clear-OwnerState -StatePath $CompanionOwnerState
}
```

- [ ] **Step 4: Run PowerShell parser check**

Run:

```bash
powershell -NoProfile -Command "$errors=$null; $tokens=$null; $null=[System.Management.Automation.Language.Parser]::ParseFile('tools/aurora/launch.ps1',[ref]$tokens,[ref]$errors); if ($errors) { $errors; exit 1 }; 'parse-ok'"
```

Expected: prints `parse-ok`.

- [ ] **Step 5: Verify broad sweep removed**

Run:

```bash
python - <<'PY'
from pathlib import Path
text = Path('tools/aurora/launch.ps1').read_text(encoding='utf-8')
assert "Name='python.exe' OR Name='pythonw.exe'" not in text
assert '$env:AURORA_PYTHON = $Python' in text
assert '.companion_owner.json' in text
print('launch-check-ok')
PY
```

Expected: prints `launch-check-ok`.

- [ ] **Step 6: Commit launcher changes**

```bash
git add tools/aurora/launch.ps1
git commit -m "fix: target Aurora stale process cleanup"
```

---

### Task 6: Final verification

**Files:**
- Verify: `tools/aurora/aurora_companion.py`
- Verify: `tools/aurora/launch.ps1`
- Verify: `tools/aurora/tests/test_qt_bridge_lifecycle.py`

- [ ] **Step 1: Run focused lifecycle tests**

Run:

```bash
python -m pytest tools/aurora/tests/test_qt_bridge_lifecycle.py -q
```

Expected: all tests pass.

- [ ] **Step 2: Run Aurora test suite**

Run:

```bash
python -m pytest tools/aurora/tests -q
```

Expected: all tests pass.

- [ ] **Step 3: Run Python compile check**

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/relay_comm.py tools/aurora/qt_camera_bridge.py tools/aurora/chassis_comm.py tools/aurora/ros_bridge.py
```

Expected: exits 0.

- [ ] **Step 4: Run PowerShell parser check**

Run:

```bash
powershell -NoProfile -Command "$errors=$null; $tokens=$null; $null=[System.Management.Automation.Language.Parser]::ParseFile('tools/aurora/launch.ps1',[ref]$tokens,[ref]$errors); if ($errors) { $errors; exit 1 }; 'parse-ok'"
```

Expected: prints `parse-ok`.

- [ ] **Step 5: Review git diff**

Run:

```bash
git diff -- tools/aurora/launch.ps1 tools/aurora/aurora_companion.py tools/aurora/tests/test_qt_bridge_lifecycle.py
```

Expected: diff shows only interpreter selection, owner-state cleanup, and tests.

- [ ] **Step 6: Commit final verification fixes if needed**

If verification required fixes, run:

```bash
git add tools/aurora/launch.ps1 tools/aurora/aurora_companion.py tools/aurora/tests/test_qt_bridge_lifecycle.py
git commit -m "fix: verify Aurora lifecycle cleanup"
```

Skip commit if no files changed after previous commits.

---

## Self-review

- Spec coverage: Python env sharing covered by Tasks 1-2 and 5; Qt cleanup narrowing by Tasks 3 and 5; Companion owner state by Tasks 4-5; tests and verification by Tasks 1-3 and 6.
- Placeholder scan: no `TBD`, `TODO`, or unspecified implementation steps remain.
- Type consistency: owner state field names use `pid`, `port`, `script` across PowerShell and existing Qt owner state.
