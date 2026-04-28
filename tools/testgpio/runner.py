from __future__ import annotations

import subprocess
import threading
import time
import uuid
from pathlib import Path
from typing import Callable, Dict, List, Optional


class GpioRunner:
    def __init__(
        self,
        log_dir: Path,
        executable: str = "./ssne_ai_demo",
        ssh_host: str = "",
        remote_workdir: str = "",
        spawn_process: Optional[Callable[[List[str]], object]] = None,
    ) -> None:
        self._log_dir = Path(log_dir)
        self._log_dir.mkdir(parents=True, exist_ok=True)
        self._executable = executable
        self._ssh_host = ssh_host.strip()
        self._remote_workdir = remote_workdir.strip()
        self._spawn_process = spawn_process or self._default_spawn_process
        self._lock = threading.Lock()
        self._current_run: Optional[Dict[str, object]] = None
        self._worker: Optional[threading.Thread] = None

    def update_target(self, ssh_host: str, remote_workdir: str) -> None:
        with self._lock:
            self._ssh_host = ssh_host.strip()
            self._remote_workdir = remote_workdir.strip()

    def target_config(self) -> Dict[str, str]:
        with self._lock:
            return {
                "ssh_host": self._ssh_host,
                "remote_workdir": self._remote_workdir,
            }

    def build_gpio_command(self, mode: str, pin: int, period_ms: int, duration_s: int) -> List[str]:
        return [
            self._executable,
            "--gpio-test",
            "--pin",
            str(pin),
            "--mode",
            str(mode),
            "--period-ms",
            str(period_ms),
            "--duration-s",
            str(duration_s),
        ]

    def build_launch_command(self, mode: str, pin: int, period_ms: int, duration_s: int) -> List[str]:
        gpio_command = self.build_gpio_command(mode=mode, pin=pin, period_ms=period_ms, duration_s=duration_s)
        if not self._ssh_host:
            return gpio_command
        remote_cmd = " ".join(gpio_command)
        if self._remote_workdir:
            remote_cmd = f"cd {self._remote_workdir} && {remote_cmd}"
        return ["ssh", self._ssh_host, remote_cmd]

    def _default_spawn_process(self, command: List[str]):
        return subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
        )

    def start_run(self, mode: str, pin: int, period_ms: int, duration_s: int) -> Dict[str, object]:
        run_id = uuid.uuid4().hex[:8]
        timestamp = time.strftime("%Y%m%d-%H%M%S")
        log_path = self._log_dir / f"gpio-{timestamp}-{run_id}.log"
        command = self.build_launch_command(mode=mode, pin=pin, period_ms=period_ms, duration_s=duration_s)
        log_path.write_text("", encoding="utf-8")
        run = {
            "id": run_id,
            "mode": mode,
            "pin": pin,
            "period_ms": period_ms,
            "duration_s": duration_s,
            "status": "running",
            "command": command,
            "log_path": str(log_path),
            "started_at": time.time(),
        }
        with self._lock:
            self._current_run = run
        self._worker = threading.Thread(target=self._run_process, args=(dict(run),), daemon=True)
        self._worker.start()
        return dict(run)

    def _run_process(self, run: Dict[str, object]) -> None:
        process = self._spawn_process(list(run["command"]))
        log_path = Path(str(run["log_path"]))
        stdout_lines = getattr(process, "stdout_lines", None)
        if stdout_lines is None:
            stdout = getattr(process, "stdout", None)
            stdout_lines = stdout if stdout is not None else []
        with log_path.open("a", encoding="utf-8") as handle:
            for line in stdout_lines:
                handle.write(str(line))
            exit_code = int(process.wait())
        with self._lock:
            if self._current_run and self._current_run.get("id") == run.get("id"):
                self._current_run["status"] = "passed" if exit_code == 0 else "failed"
                self._current_run["exit_code"] = exit_code
                self._current_run["finished_at"] = time.time()

    def wait_for_completion(self, timeout: float = 5.0) -> bool:
        worker = self._worker
        if worker is None:
            return True
        worker.join(timeout)
        return not worker.is_alive()

    def current_run(self) -> Optional[Dict[str, object]]:
        with self._lock:
            if self._current_run is None:
                return None
            return dict(self._current_run)
