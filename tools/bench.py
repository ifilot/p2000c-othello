#!/usr/bin/env python3
"""Benchmark: emulated seconds for 20 rounds (human RETURN + computer reply)
at the given level, split into thinking and display time. Run after make build.
"""
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from render import EMULATOR, HD0, IPL, ROOT, make_image

MHZ = 4.0


def run(level, rounds, marker):
    actions = []
    for _ in range(rounds):
        actions += ["--send", "\\r", "--wait-for", "Wit denkt", "--wait-for", marker]
    cmd = [str(EMULATOR), "--ipl", str(IPL), "--hard-disk-0", str(HD0), "--hard-disk-1", str(ROOT / "build/hd1.hda"),
           "--fast-storage", "--chunk-cycles", "2000", "--wait-for", "A>", "--send", "F:OTHELLO\\r", "--run", "12000000", "--send", " ",
           "--wait-for", "Kies de sterkte", "--send", str(level), "--wait-for", "Zwart aan zet", *actions, "--output", "json"]
    state = json.loads(subprocess.run(cmd, capture_output=True, text=True, timeout=900).stdout)
    assert state["status"] == "ok", state.get("message")
    return state["cycles"]


def main():
    make_image(ROOT / "build/OTHELLO.COM", ROOT / "build")
    level = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    rounds = 20
    base = run(level, 0, "")
    total = (run(level, rounds, "Zwart aan zet") - base) / MHZ / 1e6
    # "Wit speelt" is written right after the search, before White's move is flushed:
    # the time up to it is the human move's flush plus thinking.
    print(f"level {level}: {rounds} rounds in {total:.1f} s emulated ({total / rounds:.2f} s/round)")


if __name__ == "__main__":
    main()
