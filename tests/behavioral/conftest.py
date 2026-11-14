"""tests/behavioral/conftest.py

Pytest configuration for the behavioural (app + UI) layer.

When pytest + pytest-bdd are installed, scenarios in tests/behavioral/features/
can be run directly with pytest. Step functions live in
tests/behavioral/steps/test_steps.py and are imported here so pytest-bdd can
bind them to Gherkin steps via scenario decorators or the `steps` module.

Until then, use the dependency-free runner: tests/behavioral/run_tests.py
"""

from __future__ import annotations

from steps.test_steps import (  # noqa: F401  (imported for pytest-bdd binding)
    step_binary_built_staged,
    step_exits_without_hanging,
    step_launch_artha,
    step_launch_artha_no_display,
    step_no_display,
    step_scratch_home_xdg,
    step_scratch_contains_only_artha_files,
    step_stop_artha,
    step_stays_running_2s,
    step_writes_preferences,
)

__all__ = [
    "step_scratch_home_xdg",
    "step_binary_built_staged",
    "step_no_display",
    "step_launch_artha",
    "step_launch_artha_no_display",
    "step_stop_artha",
    "step_stays_running_2s",
    "step_writes_preferences",
    "step_scratch_contains_only_artha_files",
    "step_exits_without_hanging",
]
