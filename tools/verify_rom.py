"""Verify the player's ESP Ra.De. ROM set (CRC32 and SHA-1 of every file)."""
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS), str(TOOLS / "core")]
import game_profile  # noqa: E402,F401
import verify_rom  # noqa: E402

if __name__ == "__main__":
    sys.argv[0] = "verify_rom.py"
    sys.exit(verify_rom.main())
