"""Run the shared core's 68000 translation scaffold for ESP Ra.De. (C in build/generated/)."""
import runpy
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

import game_profile  # noqa: E402  (puts the core on the path and gives the translator its settings)
from m68000 import analysis  # noqa: E402

analysis.MAX_VALUES = getattr(game_profile, "ANALYSIS_MAX_VALUES", analysis.MAX_VALUES)
runpy.run_path(str(TOOLS / "core" / "recompile.py"), run_name="__main__")
