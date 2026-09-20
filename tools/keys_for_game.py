#!/usr/bin/env python3
"""Rules model and key-sequence helper for scripted emulator games.

Mirrors the rules in rules.asm and the cursor placement (cursor jumps to the first legal
square in scan order after every move) so scripted games can be replayed in
the headless emulator. Prints one --send/--wait-for action pair per move.
"""
import sys

DIRS = (-9, -8, -7, -1, 1, 7, 8, 9)


def flips(board, cell, d, me):
    n, pos = 0, cell
    while True:
        nxt = pos + d
        if nxt < 0 or nxt > 63 or abs((nxt & 7) - (pos & 7)) > 1:
            return 0
        pos = nxt
        if board[pos] == 0:
            return 0
        if board[pos] == me:
            return n
        n += 1


def legal(board, cell, me):
    return board[cell] == 0 and any(flips(board, cell, d, me) for d in DIRS)


def play(board, cell, me):
    board[cell] = me
    for d in DIRS:
        for k in range(1, flips(board, cell, d, me) + 1):
            board[cell + k * d] = me


def main():
    board = [0] * 64
    board[27] = board[36] = 2
    board[28] = board[35] = 1
    me = 1
    actions = []
    for token in sys.argv[1:]:
        target = (int(token[1]) - 1) * 8 + (ord(token[0].lower()) - ord("a"))
        assert legal(board, target, me), f"{token} is not legal for {'black' if me == 1 else 'white'}"
        cursor = next(c for c in range(64) if legal(board, c, me))
        dc, dr = (target & 7) - (cursor & 7), (target >> 3) - (cursor >> 3)
        keys = "d" * dc + "a" * -dc + "s" * dr + "w" * -dr + "\\r"
        play(board, target, me)
        other = 3 - me
        if any(legal(board, c, other) for c in range(64)):
            me = other
            marker = "Black to move" if me == 1 else "White to move"
        elif any(legal(board, c, me) for c in range(64)):
            marker = "passes"
        else:
            marker = "Game over"
        actions += ["--send", keys, "--wait-for", marker]
    b, w = board.count(1), board.count(2)
    print(f"# expected final score black {b} white {w}", file=sys.stderr)
    print(" ".join(f'"{a}"' if a[0] != "-" else a for a in actions))


if __name__ == "__main__":
    main()
