"""Run the shared core's 68000 static analysis for ESP Ra.De.: listing and report."""
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

import game_profile  # noqa: E402  (puts the core on the path and gives the analysis its settings)
from m68000 import analysis  # noqa: E402

analysis.MAX_VALUES = getattr(game_profile, "ANALYSIS_MAX_VALUES", analysis.MAX_VALUES)
analysis.main()
