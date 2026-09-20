#!/usr/bin/env python3
"""Regression tests: full games against the computer in the headless emulator.

Black (the "human") always accepts the cursor, i.e. plays the first legal
square in row-major order. At level 1 White is the greedy CPU, which is also
simulated here, so every turn marker and the final score are predicted and
the emulator's screen must agree. At level 3 the search is not modelled;
instead each CPU move is read from the "Wit speelt" note and checked for
legality, and the scores must match the replayed game. Run after `make build`.
"""
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import keys_for_game as rules
from render import EMULATOR, HD0, HD1, IPL, ROOT, make_image


def cpu_choose(board, me):
    best, found = 0, None
    for c in range(64):
        flips = sum(rules.flips(board, c, d, me) for d in rules.DIRS) if board[c] == 0 else 0
        if not flips:
            continue
        score = flips + (10 if (c & 7) in (0, 7) and (c >> 3) in (0, 7) else 0)
        if found is None or score > best:
            found, best = c, score
    return found


def simulate():
    board = [0] * 64
    board[27] = board[36] = 2
    board[28] = board[35] = 1
    actions, me, move = [], 1, 1
    while True:
        if me == 1:
            cell = next((c for c in range(64) if rules.legal(board, c, 1)), None)
            if cell is None:
                break                                   # cannot happen: the CPU loop passes for us
            actions += ["--send", "\\r"]
        else:
            cell = cpu_choose(board, 2)
        rules.play(board, cell, me)
        move += 1
        other = 3 - me
        if any(rules.legal(board, c, other) for c in range(64)):
            me = other
        elif not any(rules.legal(board, c, me) for c in range(64)):
            actions += ["--wait-for", "Spel afgelopen", "--run", "1000000"]
            break
        if me == 1:                                     # the CPU loop ends with Black to move;
            actions += ["--wait-for", f"Zet     {move:2d}"]   # the move number is a unique marker
    return actions, board.count(1), board.count(2)


def emulate(level, actions, timeout=900):
    """Boots, selects the level and performs the actions; returns the panel rows."""
    work = ROOT / "build"
    cmd = [str(EMULATOR), "--ipl", str(IPL), "--hard-disk-0", str(HD0), "--hard-disk-1", str(work / "hd1.hda"),
           "--fast-storage", "--chunk-cycles", "5000", "--wait-for", "A>", "--send", "F:OTHELLO\\r",
           "--wait-for", "Kies de sterkte", "--send", str(level), "--wait-for", "Zwart aan zet", *actions,
           "--dump-graphics", str(work / "graphics.bin"), "--output", "json"]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    state = json.loads(result.stdout)
    flat = "".join(state["screen"])
    return state, {r: flat[r * 64 + 48:(r + 1) * 64].strip() for r in (5, 6, 9, 10, 13)}


def test_level1():
    actions, black, white = simulate()
    state, panel = emulate(1, actions)
    print(f"level 1: {state['status']} {state.get('message', '')} | {panel}")
    print(f"         expected Zwart {black} Wit {white}")
    return state["status"] == "ok" and panel[5].endswith(str(black)) and panel[6].endswith(str(white)) \
        and panel[10] == "Spel afgelopen"


def test_level3():
    """Replays a level-3 game round by round, learning White's moves from the note."""
    board = [0] * 64
    board[27] = board[36] = 2
    board[28] = board[35] = 1
    actions, move = [], 1

    def can(colour):
        return any(rules.legal(board, c, colour) for c in range(64))

    def white_reply(wait_actions):
        """Waits for White's next move, checks it and applies it."""
        nonlocal move
        state, panel = emulate(3, actions + wait_actions)
        note = panel[10]
        if state["status"] != "ok" or not note.startswith("Wit speelt "):
            print(f"level 3: {state['status']} {state.get('message', '')} | {panel}")
            return False
        cell = (int(note[12]) - 1) * 8 + ord(note[11]) - ord("A")
        if not rules.legal(board, cell, 2):
            print(f"level 3: illegal CPU move {note} at move {move}")
            return False
        rules.play(board, cell, 2)
        move += 1
        if not panel[13].endswith(f"{move:2d}") or not panel[5].endswith(str(board.count(1))) \
                or not panel[6].endswith(str(board.count(2))):
            print(f"level 3: mismatch at move {move}: {panel} vs Zwart {board.count(1)} Wit {board.count(2)}")
            return False
        actions.extend(wait_actions)
        return True

    while True:
        cell = next(c for c in range(64) if rules.legal(board, c, 1))
        rules.play(board, cell, 1)
        move += 1
        if not can(2):
            if not can(1):
                actions += ["--send", "\\r"]
                break
            actions += ["--send", "\\r", "--wait-for", "Wit denkt", "--wait-for", f"Zet     {move:2d}"]
            continue                                        # White passes, Black again
        actions += ["--send", "\\r", "--wait-for", "Wit denkt"]
        if not white_reply(["--wait-for", f"Zet     {move + 1:2d}"]):
            return False
        while not can(1) and can(2):                        # Black passes, White moves again
            if not white_reply(["--wait-for", f"Zet     {move + 1:2d}"]):
                return False
        if not can(1) and not can(2):
            break
    state, panel = emulate(3, actions + ["--wait-for", "Spel afgelopen", "--run", "1000000"])
    print(f"level 3: {state['status']} | {panel} | replayed Zwart {board.count(1)} Wit {board.count(2)}, {move - 1} moves")
    return state["status"] == "ok" and panel[10] == "Spel afgelopen" \
        and panel[5].endswith(str(board.count(1))) and panel[6].endswith(str(board.count(2)))


def main():
    make_image(ROOT / "build/OTHELLO.COM", ROOT / "build")
    ok1 = test_level1()
    ok3 = test_level3()
    print("PASS" if ok1 and ok3 else "FAIL")
    return 0 if ok1 and ok3 else 1


if __name__ == "__main__":
    sys.exit(main())
