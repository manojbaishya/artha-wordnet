"""tests/behavioral/steps/test_steps.py

Step definitions for the behavioural (app + UI) layer.
Drives the real artha binary end-to-end on a scratch HOME / XDG_* dir
and an X display, mirroring tests/smoke/test_smoke.sh.

No third-party Python packages are required: this module is executed by
tests/behavioral/run_tests.py (a dependency-free Gherkin runner). When
pytest-bdd/behave are installed later, these same step functions can be
reused with minimal adaptation.
"""

from __future__ import annotations

import os
import signal
import subprocess
import tempfile
import time
from pathlib import Path

# ---------------------------------------------------------------------------
# Scenario state (shared between steps of one scenario)
# ---------------------------------------------------------------------------
_CTX = {
    "scratch": None,  # Path to scratch dir
    "home": None,  # Path to scratch/home
    "config": None,  # Path to scratch/config
    "proc": None,  # subprocess.Popen for the running artha
    "binary": None,  # Path to artha binary
    "display": None,  # Display in use (e.g. ":94")
    "log": None,  # Path to artha log file
}


def _find_binary() -> str:
    """Locate the staged artha binary, falling back to a release build in sandbox."""
    root = Path(__file__).resolve().parents[3]  # repo root
    candidates = [
        root / "sandbox" / "stage" / "bin" / "artha",
    ]
    # fallback: any release binary under sandbox/
    sandbox = root / "sandbox"
    if sandbox.exists():
        for cand in sandbox.rglob("artha"):
            if cand.is_file() and os.access(cand, os.X_OK) and "release" in str(cand):
                candidates.append(cand)
    for c in candidates:
        if c.is_file() and os.access(c, os.X_OK):
            return str(c)
    raise RuntimeError(
        "artha binary not found; run `task build` + `task install` first"
    )


def _env(scratch: Path) -> dict:
    """Build the scratch environment (same contract as tests/smoke/test_smoke.sh)."""
    env = os.environ.copy()
    env["HOME"] = str(scratch / "home")
    env["XDG_CONFIG_HOME"] = str(scratch / "config")
    env["XDG_DATA_HOME"] = str(scratch / "data")
    env["XDG_CACHE_HOME"] = str(scratch / "cache")
    env["GDK_DEBUG"] = ""
    env["G_DEBUG"] = "fatal-criticals"
    return env


# ---------------------------------------------------------------------------
# Given steps
# ---------------------------------------------------------------------------


def step_scratch_home_xdg():
    """Given a scratch HOME and XDG_* directory."""
    scratch = Path(tempfile.mkdtemp(prefix="artha-behavioral-"))
    for d in ("home", "config", "data", "cache"):
        (scratch / d).mkdir(parents=True, exist_ok=True)
    _CTX["scratch"] = scratch
    _CTX["home"] = scratch / "home"
    _CTX["config"] = scratch / "config"


def step_binary_built_staged():
    """And the artha binary is built and staged."""
    _CTX["binary"] = _find_binary()


def step_no_display():
    """Given no DISPLAY is set."""
    os.environ.pop("DISPLAY", None)
    _CTX["display"] = None


# ---------------------------------------------------------------------------
# When steps
# ---------------------------------------------------------------------------


def step_launch_artha():
    """When I launch artha on the display."""
    binary = _CTX["binary"]
    scratch = _CTX["scratch"]
    env = _env(scratch)
    display = env.get("DISPLAY") or ":94"
    _CTX["display"] = display
    log = scratch / "artha.log"
    _CTX["log"] = log
    proc = subprocess.Popen(
        [binary],
        env=env,
        stdout=open(log, "wb"),
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )
    _CTX["proc"] = proc


def step_launch_artha_no_display():
    """When I launch artha (no DISPLAY)."""
    binary = _CTX["binary"]
    scratch = _CTX["scratch"]
    env = _env(scratch)
    env.pop("DISPLAY", None)
    log = scratch / "artha.log"
    _CTX["log"] = log
    proc = subprocess.Popen(
        [binary],
        env=env,
        stdout=open(log, "wb"),
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )
    _CTX["proc"] = proc


def step_stop_artha():
    """When I stop artha."""
    proc = _CTX.get("proc")
    if proc and proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)


# ---------------------------------------------------------------------------
# Then steps
# ---------------------------------------------------------------------------


def step_stays_running_2s():
    """Then artha stays running for at least 2 seconds."""
    proc = _CTX.get("proc")
    assert proc is not None, "artha was not launched"
    time.sleep(2)
    assert proc.poll() is None, (
        f"artha exited early (code {proc.returncode}); see {_CTX['log']}"
    )


def step_writes_preferences():
    """And artha writes its preferences file into the scratch config dir."""
    config = _CTX["config"]
    conf = config / "artha.conf"
    deadline = time.time() + 15
    while time.time() < deadline:
        if conf.exists() and conf.stat().st_size > 0:
            return
        time.sleep(0.1)
    raise AssertionError(f"artha did not write {conf} within 15s; see {_CTX['log']}")


def step_scratch_contains_only_artha_files():
    """Then the scratch dir contains only artha-owned files."""
    scratch = _CTX["scratch"]
    assert scratch.exists(), "scratch dir missing"
    # The only top-level entries should be home/config/data/cache and artha.log
    allowed = {"home", "config", "data", "cache", "artha.log"}
    actual = {p.name for p in scratch.iterdir()}
    extra = actual - allowed
    assert not extra, f"unexpected entries in scratch dir: {extra}"


def step_exits_without_hanging():
    """Then artha exits (does not hang indefinitely)."""
    proc = _CTX.get("proc")
    assert proc is not None, "artha was not launched"
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)
        raise AssertionError("artha did not exit without a display; see log")


# ---------------------------------------------------------------------------
# Step registry: Gherkin keyword -> step function
# ---------------------------------------------------------------------------

# Keys are (keyword, step_text_substring) -> function. The runner matches
# the longest keyword prefix first, then checks substring containment.
STEPS = {
    ("Given", "a scratch HOME and XDG_* directory"): step_scratch_home_xdg,
    ("And", "the artha binary is built and staged"): step_binary_built_staged,
    ("Given", "no DISPLAY is set"): step_no_display,
    ("When", "I launch artha on the display"): step_launch_artha,
    ("When", "I launch artha"): step_launch_artha_no_display,
    ("When", "I stop artha"): step_stop_artha,
    ("Then", "artha stays running for at least 2 seconds"): step_stays_running_2s,
    ("And", "artha writes its preferences file into the scratch config dir"): (
        step_writes_preferences
    ),
    ("Then", "the scratch dir contains only artha-owned files"): (
        step_scratch_contains_only_artha_files
    ),
    ("Then", "artha exits (does not hang indefinitely)"): step_exits_without_hanging,
}


def find_step(keyword: str, text: str):
    """Return the step function matching (keyword, text), or None."""
    for (k, t), fn in STEPS.items():
        if k == keyword and t in text:
            return fn
    return None
