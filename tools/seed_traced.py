"""Write tools/traced_targets.txt into symbols.txt as a preserved block.

The runtime reports computed jump/call targets that the static analysis cannot follow.
They are appended to tools/traced_targets.txt during the coverage loop; this script keeps
the symbols.txt block in sync without touching the other sections.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
START = "# --- trace-seeded handlers ---"
END = "# --- end trace-seeded handlers ---"

targets = []
for line in (ROOT / "tools" / "traced_targets.txt").read_text(encoding="utf-8").splitlines():
    line = line.split("#", 1)[0].strip()
    if not line:
        continue
    targets.append(int(line, 16))

path = ROOT / "symbols.txt"
text = path.read_text(encoding="utf-8")
kept, inside = [], False
for line in text.splitlines():
    if line.strip() == START:
        inside = True
        continue
    if line.strip() == END:
        inside = False
        continue
    if not inside:
        kept.append(line)
while kept and not kept[-1].strip():
    kept.pop()
lines = [START]
lines += [f"{address:06X}  func  trace_{address:06X}" for address in sorted(set(targets))]
lines += [END, ""]
path.write_text("\n".join(kept + lines), encoding="utf-8")
print(f"{len(set(targets))} traced handlers written")
