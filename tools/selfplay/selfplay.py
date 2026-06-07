#!/usr/bin/env python3
"""
selfplay.py — Makruk-SF self-play game generator.

Plays sf-kernel against itself from the Makruk start position,
writing one PGN file and one JSONL file (one record per game).

Usage examples:
    python selfplay.py --games 100 --movetime 100
    python selfplay.py --games 200 --movetime 100 --resume
    python selfplay.py --engine ../../src/sf-kernel --games 50 --outdir out/

Resume support:
    selfplay_state.json in --outdir tracks games_completed.
    Pass --resume to append to existing PGN/JSONL and continue from last game.
"""

import argparse
import json
import re
import subprocess
import sys
import time
from datetime import date
from pathlib import Path

MAKRUK_START_FEN = "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1"
MAX_PLY = 480  # safety cap; Makruk counting rules typically end games earlier

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_ENGINE = SCRIPT_DIR.parent.parent / "src" / "sf-kernel"
STATE_FILE = "selfplay_state.json"


# ---------------------------------------------------------------------------
# Engine wrapper
# ---------------------------------------------------------------------------

class Engine:
    """Manages a single UCI engine subprocess."""

    def __init__(self, path: str, hash_mb: int, threads: int) -> None:
        self._proc = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        self._send("uci")
        self._wait_for("uciok")
        self._send("setoption name UCI_Variant value makruk")
        self._send(f"setoption name Hash value {hash_mb}")
        self._send(f"setoption name Threads value {threads}")
        self._send("isready")
        self._wait_for("readyok")

    def _send(self, cmd: str) -> None:
        self._proc.stdin.write(cmd + "\n")
        self._proc.stdin.flush()

    def _readline(self) -> str:
        return self._proc.stdout.readline().rstrip("\n")

    def _wait_for(self, prefix: str) -> None:
        while True:
            line = self._readline()
            if line.startswith(prefix):
                return

    def search(
        self, moves: list[str], movetime_ms: int
    ) -> tuple[str | None, int | None, bool]:
        """
        Set position and search.

        Returns (bestmove, score_cp, is_mate_score).
        bestmove is None when no legal moves exist (checkmate or stalemate).
        score_cp is the deepest-depth eval; None if engine gave no score.
        is_mate_score is True when the score was a 'mate' distance, not cp.
        """
        pos_cmd = "position fen " + MAKRUK_START_FEN
        if moves:
            pos_cmd += " moves " + " ".join(moves)
        self._send(pos_cmd)
        self._send(f"go movetime {movetime_ms}")

        score_cp: int | None = None
        is_mate = False

        while True:
            line = self._readline()
            if line.startswith("info"):
                m = re.search(r"\bscore (cp|mate) (-?\d+)", line)
                if m:
                    kind, val = m.group(1), int(m.group(2))
                    score_cp = val
                    is_mate = kind == "mate"
            elif line.startswith("bestmove"):
                parts = line.split()
                mv = parts[1] if len(parts) > 1 else None
                if mv in (None, "(none)", "0000"):
                    return None, score_cp, is_mate
                return mv, score_cp, is_mate

    def new_game(self) -> None:
        self._send("ucinewgame")
        self._send("isready")
        self._wait_for("readyok")

    def quit(self) -> None:
        try:
            self._send("quit")
            self._proc.wait(timeout=5)
        except Exception:
            self._proc.kill()


# ---------------------------------------------------------------------------
# Game logic
# ---------------------------------------------------------------------------

def _is_threefold(moves: list[str]) -> bool:
    """Return True if the last moves form a 3-fold repetition cycle.

    Two consecutive repetitions of a cycle of length L means the position at
    the start of that cycle has appeared 3 times (the original plus 2 returns).
    Checks cycle lengths 2, 4, and 6 half-moves.
    """
    for cycle in (2, 4, 6):
        needed = cycle * 2
        if len(moves) >= needed:
            if moves[-cycle:] == moves[-2 * cycle: -cycle]:
                return True
    return False


def play_game(engine: Engine, game_id: int, movetime_ms: int) -> dict:
    """Play one game; return a complete game record."""
    engine.new_game()

    moves: list[str] = []
    scores: list[int | None] = []
    result = "*"
    termination = "unterminated"
    zero_streak = 0  # consecutive plies where engine score == 0

    for ply in range(MAX_PLY):
        mv, score_cp, is_mate = engine.search(moves, movetime_ms)

        if mv is None:
            # No legal moves — checkmate or stalemate.
            # 'score mate 0' means the side to move is mated (checkmate).
            # Stalemate typically returns score cp 0 with no moves.
            if is_mate and score_cp == 0:
                # Side to move is checkmated; the player who just moved wins.
                result = "0-1" if ply % 2 == 0 else "1-0"
                termination = "checkmate"
            else:
                result = "1/2-1/2"
                termination = "stalemate"
            break

        scores.append(score_cp)
        moves.append(mv)

        # Track consecutive zero-score plies (engine sees no win/loss).
        if score_cp == 0:
            zero_streak += 1
        else:
            zero_streak = 0

        # Detect draw by exact move cycle (3-fold repetition via move pattern).
        if _is_threefold(moves):
            result = "1/2-1/2"
            termination = "repetition"
            break

        # Detect drawn endgame: engine scores 0 for many plies after the opening.
        # This catches repetitions and counting draws that the engine recognises
        # internally but keeps playing (both sides evaluate the position as drawn).
        if ply >= 40 and zero_streak >= 16:
            result = "1/2-1/2"
            termination = "draw_score"
            break
    else:
        result = "1/2-1/2"
        termination = "max_ply"

    return {
        "game_id": game_id,
        "date": date.today().isoformat(),
        "movetime_ms": movetime_ms,
        "start_fen": MAKRUK_START_FEN,
        "moves": moves,
        "scores": scores,
        "result": result,
        "termination": termination,
        "ply_count": len(moves),
    }


# ---------------------------------------------------------------------------
# Output writers
# ---------------------------------------------------------------------------

def write_pgn(game: dict, f) -> None:
    f.write('[Event "Makruk-SF Self-Play"]\n')
    f.write('[Site "localhost"]\n')
    f.write(f'[Date "{game["date"]}"]\n')
    f.write(f'[Round "{game["game_id"]}"]\n')
    f.write('[White "sf-kernel"]\n')
    f.write('[Black "sf-kernel"]\n')
    f.write(f'[Result "{game["result"]}"]\n')
    f.write('[Variant "Makruk"]\n')
    f.write(f'[FEN "{game["start_fen"]}"]\n')
    f.write(f'[PlyCount "{game["ply_count"]}"]\n')
    f.write(f'[Termination "{game["termination"]}"]\n')
    f.write(f'[MoveTime "{game["movetime_ms"]}"]\n')
    f.write("\n")

    tokens: list[str] = []
    for i, mv in enumerate(game["moves"]):
        if i % 2 == 0:
            tokens.append(f"{i // 2 + 1}.")
        tokens.append(mv)
    tokens.append(game["result"])

    line = ""
    for tok in tokens:
        if line and len(line) + 1 + len(tok) > 80:
            f.write(line + "\n")
            line = tok
        else:
            line = (line + " " + tok).lstrip()
    if line:
        f.write(line + "\n")
    f.write("\n")


def write_jsonl(game: dict, f) -> None:
    """One JSON record per game. Training pipeline splits into positions."""
    record = {
        "game_id": game["game_id"],
        "date": game["date"],
        "start_fen": game["start_fen"],
        "moves": game["moves"],
        "scores": game["scores"],
        "result": game["result"],
        "termination": game["termination"],
        "ply_count": game["ply_count"],
        "movetime_ms": game["movetime_ms"],
    }
    f.write(json.dumps(record, separators=(",", ":")) + "\n")


# ---------------------------------------------------------------------------
# State (resume support)
# ---------------------------------------------------------------------------

def load_state(out_dir: Path) -> dict:
    p = out_dir / STATE_FILE
    if p.exists():
        return json.loads(p.read_text())
    return {"games_completed": 0}


def save_state(out_dir: Path, games_completed: int) -> None:
    (out_dir / STATE_FILE).write_text(
        json.dumps({"games_completed": games_completed}, indent=2)
    )


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Makruk-SF self-play game generator",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--engine", default=str(DEFAULT_ENGINE),
                        help="Path to sf-kernel binary")
    parser.add_argument("--games", type=int, default=10,
                        help="Number of games to generate")
    parser.add_argument("--movetime", type=int, default=100,
                        help="Time per move in milliseconds")
    parser.add_argument("--threads", type=int, default=1,
                        help="Engine thread count")
    parser.add_argument("--hash", type=int, default=16,
                        help="Engine hash table size in MB")
    parser.add_argument("--pgn", default="selfplay.pgn",
                        help="PGN output filename")
    parser.add_argument("--jsonl", default="selfplay.jsonl",
                        help="JSONL output filename")
    parser.add_argument("--outdir", default=".",
                        help="Output directory")
    parser.add_argument("--resume", action="store_true",
                        help="Resume from last completed game (append to outputs)")
    args = parser.parse_args()

    out_dir = Path(args.outdir)
    out_dir.mkdir(parents=True, exist_ok=True)

    engine_path = Path(args.engine)
    if not engine_path.exists():
        print(f"ERROR: engine not found: {engine_path}", file=sys.stderr)
        sys.exit(1)

    state = load_state(out_dir) if args.resume else {"games_completed": 0}
    already_done = state["games_completed"]
    first_game = already_done + 1
    last_game = already_done + args.games

    pgn_path = out_dir / args.pgn
    jsonl_path = out_dir / args.jsonl

    print(f"Engine   : {engine_path}")
    print(f"Games    : {first_game}–{last_game}  ({args.games} games, movetime={args.movetime}ms)")
    print(f"PGN      : {pgn_path}")
    print(f"JSONL    : {jsonl_path}")
    if args.resume and already_done:
        print(f"Resuming : continuing after game {already_done}")
    print()

    engine = Engine(str(engine_path), hash_mb=args.hash, threads=args.threads)
    open_mode = "a" if args.resume else "w"

    games_written = 0
    try:
        with open(pgn_path, open_mode) as pgn_f, \
             open(jsonl_path, open_mode) as jsonl_f:

            for gid in range(first_game, last_game + 1):
                t0 = time.monotonic()
                try:
                    game = play_game(engine, gid, args.movetime)
                except Exception as exc:
                    print(f"\nERROR on game {gid}: {exc}", file=sys.stderr)
                    break
                elapsed = time.monotonic() - t0

                write_pgn(game, pgn_f)
                pgn_f.flush()
                write_jsonl(game, jsonl_f)
                jsonl_f.flush()
                save_state(out_dir, gid)
                games_written += 1

                print(
                    f"  [{gid:4d}/{last_game}]"
                    f"  {game['ply_count']:3d} ply"
                    f"  {game['result']:7s}"
                    f"  {elapsed:5.1f}s"
                    f"  {game['termination']}"
                )
    finally:
        engine.quit()

    print(f"\nDone — {games_written} game{'s' if games_written != 1 else ''} written.")
    print(f"PGN   : {pgn_path}")
    print(f"JSONL : {jsonl_path}")


if __name__ == "__main__":
    main()
