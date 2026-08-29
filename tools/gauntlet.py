#!/usr/bin/env python3
"""
gauntlet.py — Makruk-SF engine gauntlet / match runner.

Runs two engine configurations against each other and reports W/D/L,
average scores, crashes, and timeouts.

Typical use cases:
  1. Strong vs weak movetime (generate decisive games for dataset quality check):
       python tools/gauntlet.py --games 20 --movetime-a 500 --movetime-b 50

  2. Current binary vs previous binary (regression test):
       python tools/gauntlet.py \\
           --engine-a src/sf-kernel \\
           --engine-b src/sf-kernel-prev \\
           --games 20 --movetime-a 200 --movetime-b 200

  3. Depth handicap (generate decisive games):
       python tools/gauntlet.py --games 20 --movetime-a 300 --depth-b 3

  4. Random openings to reduce draw rate:
       python tools/gauntlet.py --games 20 --movetime-a 200 --movetime-b 50 \\
           --opening-plies 6 --opening-top-n 4

Output:
  Prints a per-game table and a summary with W/D/L, Elo estimate,
  crash count, and timeout count.
  Optionally writes game records to a JSONL file (--output).

Note on illegal move detection:
  The gauntlet trusts both engines to generate legal Makruk moves.
  Full illegal-move detection requires a Python Makruk move validator
  which is not yet implemented; it will be added in a later PR.

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

import argparse
import json
import math
import queue
import random
import re
import subprocess
import sys
import threading
import time
from datetime import date
from pathlib import Path

MAKRUK_START_FEN = "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1"
MAX_PLY = 480


def load_book(path: str) -> list[str]:
    """Load an EPD/FEN opening book: one position per line, first 6
    whitespace-separated fields are read as a FEN (trailing EPD annotation
    fields, if any, are ignored). Blank lines and '#'-comments are skipped.
    """
    fens: list[str] = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            fields = line.split()
            if len(fields) < 4:
                continue
            fens.append(" ".join(fields[:6]))
    if not fens:
        raise ValueError(f"no positions found in book: {path}")
    return fens

# Draw-by-score detection.
# Stockfish's valueDraw() returns ±1 (not 0) to avoid draw loops, so checking
# for exact 0 never accumulates.  We track streaks per engine separately:
# if either engine returns |score| <= threshold for DRAW_STREAK consecutive
# turns of that engine's side, the game is declared a draw.  This reliably
# detects Makruk counting draws where scores converge toward 0.
DRAW_SCORE_CP  = 15   # |score| <= this counts toward the streak
DRAW_STREAK    = 3    # consecutive single-engine turns required
DRAW_MIN_PLY   = 20   # game must be past this many half-moves


# ---------------------------------------------------------------------------
# Engine wrapper (two instances, one per "color configuration")
# ---------------------------------------------------------------------------

def _reader_thread(stdout, line_queue: queue.Queue) -> None:
    """Background thread: drain stdout into a queue; put None on EOF."""
    try:
        for line in stdout:
            line_queue.put(line.rstrip("\n"))
    except Exception:
        pass
    finally:
        line_queue.put(None)


class Engine:
    """Manage a single UCI engine subprocess."""

    def __init__(self, name: str, path: str, hash_mb: int,
                 threads: int, extra_options: dict | None = None) -> None:
        self.name  = name
        self.alive = True
        self._proc = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        # Background reader avoids select + TextIOWrapper buffering conflicts.
        self._q: queue.Queue = queue.Queue()
        self._reader = threading.Thread(
            target=_reader_thread, args=(self._proc.stdout, self._q), daemon=True
        )
        self._reader.start()

        self._send("uci")
        self._wait_for("uciok")
        self._send("setoption name UCI_Variant value makruk")
        self._send(f"setoption name Hash value {hash_mb}")
        self._send(f"setoption name Threads value {threads}")
        if extra_options:
            for k, v in extra_options.items():
                self._send(f"setoption name {k} value {v}")
        self._send("isready")
        self._wait_for("readyok")

    def _send(self, cmd: str) -> None:
        self._proc.stdin.write(cmd + "\n")
        self._proc.stdin.flush()

    def _readline_timeout(self, timeout: float) -> str | None:
        if not self.alive:
            return None
        try:
            line = self._q.get(timeout=timeout)
            if line is None:          # EOF sentinel
                self.alive = False
                return None
            return line
        except queue.Empty:
            return None
        except Exception:
            self.alive = False
            return None

    def _wait_for(self, prefix: str, timeout: float = 10.0) -> bool:
        for _ in range(1000):
            line = self._readline_timeout(timeout)
            if line is None:
                return False
            if line.startswith(prefix):
                return True
        return False

    def set_option(self, name: str, value) -> None:
        self._send(f"setoption name {name} value {value}")

    def new_game(self) -> None:
        self._send("ucinewgame")
        self._send("isready")
        self._wait_for("readyok")

    def search(self, moves: list[str], movetime_ms: int | None = None,
               depth: int | None = None,
               timeout_sec: float | None = None,
               start_fen: str = MAKRUK_START_FEN) -> tuple[str | None, int | None, bool]:
        """
        Search from start_fen (default: Makruk start position) + given moves.

        Returns (bestmove, score_cp, is_mate).
        Returns (None, None, False) on crash or timeout.
        """
        if not self.alive:
            return None, None, False

        pos_cmd = "position fen " + start_fen
        if moves:
            pos_cmd += " moves " + " ".join(moves)
        self._send(pos_cmd)

        if depth is not None:
            self._send(f"go depth {depth}")
            if timeout_sec is None:
                timeout_sec = 30.0
        else:
            self._send(f"go movetime {movetime_ms}")
            if timeout_sec is None:
                timeout_sec = (movetime_ms / 1000.0) * 10 + 5

        score_cp: int | None = None
        is_mate = False

        for _ in range(100_000):
            line = self._readline_timeout(timeout_sec)
            if line is None:
                self.alive = False
                return None, None, False
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

        self.alive = False
        return None, None, False

    def search_multipv(self, moves: list[str], movetime_ms: int,
                       top_n: int,
                       start_fen: str = MAKRUK_START_FEN) -> list[tuple[str, int | None]]:
        """
        Search with MultiPV and return up to top_n (move, score_cp) pairs.
        Falls back to a single move on failure.
        """
        if not self.alive:
            return []

        self.set_option("MultiPV", top_n)

        pos_cmd = "position fen " + start_fen
        if moves:
            pos_cmd += " moves " + " ".join(moves)
        self._send(pos_cmd)
        self._send(f"go movetime {movetime_ms}")

        candidates: dict[int, tuple[str, int | None]] = {}
        timeout = movetime_ms / 1000.0 * 10 + 5

        for _ in range(100_000):
            line = self._readline_timeout(timeout)
            if line is None:
                break
            if line.startswith("info") and "multipv" in line:
                rank_m = re.search(r"\bmultipv (\d+)", line)
                pv_m   = re.search(r"\bpv (\S+)", line)
                sc_m   = re.search(r"\bscore (cp|mate) (-?\d+)", line)
                if rank_m and pv_m:
                    rank = int(rank_m.group(1))
                    move = pv_m.group(1)
                    sc   = int(sc_m.group(2)) if sc_m else None
                    candidates[rank] = (move, sc)
            elif line.startswith("bestmove"):
                break

        self.set_option("MultiPV", 1)
        return [candidates[k] for k in sorted(candidates.keys())]

    def quit(self) -> None:
        try:
            self._send("quit")
            self._proc.wait(timeout=5)
        except Exception:
            self._proc.kill()
        self.alive = False


# ---------------------------------------------------------------------------
# Repetition helper (same as selfplay.py)
# ---------------------------------------------------------------------------

def _is_threefold(moves: list[str]) -> bool:
    for cycle in (2, 4, 6):
        needed = cycle * 2
        if len(moves) >= needed and moves[-cycle:] == moves[-2 * cycle:-cycle]:
            return True
    return False


# ---------------------------------------------------------------------------
# Game runner
# ---------------------------------------------------------------------------

def play_game(
    white_engine: Engine,
    black_engine: Engine,
    game_id: int,
    movetime_a_ms: int,
    movetime_b_ms: int,
    depth_a: int | None,
    depth_b: int | None,
    opening_plies: int,
    opening_top_n: int,
    rng: random.Random,
    a_is_white: bool = True,
    adjudication_threshold: int = 0,
    adjudication_streak: int = 5,
    start_fen: str = MAKRUK_START_FEN,
) -> dict:
    """Play one game between white_engine (plays White) and black_engine (plays Black).

    a_is_white: True when engine A plays White in this game. Used to assign time
    controls to the correct engine regardless of which color it holds.
    start_fen: starting position (default: Makruk start). When paired with a fixed
    book position and a_is_white flipped between two calls, produces a proper
    "paired game" that cancels any positional bias in that opening.
    """

    white_engine.new_game()
    black_engine.new_game()

    moves: list[str] = []
    scores: list[int | None] = []
    result   = "*"
    termination = "unterminated"
    crash_side: str | None = None
    timed_out = False
    white_streak = 0   # consecutive turns where white's |score| <= DRAW_SCORE_CP
    black_streak = 0
    white_adv_streak = 0  # consecutive half-moves where white_score >= adj_threshold
    black_adv_streak = 0

    for ply in range(MAX_PLY):
        is_white_turn = (ply % 2 == 0)
        engine = white_engine if is_white_turn else black_engine
        # Assign time controls to the engine that owns them, not by color.
        is_a_turn = is_white_turn == a_is_white
        mt  = movetime_a_ms if is_a_turn else movetime_b_ms
        dep = depth_a       if is_a_turn else depth_b

        # Use MultiPV random selection during opening
        if ply < opening_plies and opening_top_n > 1:
            candidates = engine.search_multipv(moves, mt, opening_top_n, start_fen=start_fen)
            if candidates:
                mv, sc = rng.choice(candidates)
            else:
                mv, sc = None, None
            is_mate = False
        else:
            mv, sc, is_mate = engine.search(moves, movetime_ms=mt, depth=dep, start_fen=start_fen)

        if not engine.alive:
            crash_side = "White" if is_white_turn else "Black"
            result     = "0-1" if is_white_turn else "1-0"
            termination = f"crash_{crash_side.lower()}"
            break

        if mv is None:
            if is_mate and sc == 0:
                result = "0-1" if ply % 2 == 0 else "1-0"
                termination = "checkmate"
            else:
                result = "1/2-1/2"
                termination = "stalemate"
            break

        scores.append(sc)
        moves.append(mv)

        if is_white_turn:
            if sc is not None and abs(sc) <= DRAW_SCORE_CP:
                white_streak += 1
            else:
                white_streak = 0
        else:
            if sc is not None and abs(sc) <= DRAW_SCORE_CP:
                black_streak += 1
            else:
                black_streak = 0

        # Score adjudication: declare winner if one side consistently outscores the other.
        # Score is from the active engine's side-to-move perspective.
        if adjudication_threshold > 0 and ply >= 20 and sc is not None and not is_mate:
            white_score = sc if is_white_turn else -sc
            if white_score >= adjudication_threshold:
                white_adv_streak += 1
                black_adv_streak = 0
            elif white_score <= -adjudication_threshold:
                black_adv_streak += 1
                white_adv_streak = 0
            else:
                white_adv_streak = 0
                black_adv_streak = 0
            if white_adv_streak >= adjudication_streak:
                result = "1-0"
                termination = "adjudication"
                break
            if black_adv_streak >= adjudication_streak:
                result = "0-1"
                termination = "adjudication"
                break

        if _is_threefold(moves):
            result = "1/2-1/2"
            termination = "repetition"
            break

        if ply >= DRAW_MIN_PLY and (white_streak >= DRAW_STREAK or black_streak >= DRAW_STREAK):
            result = "1/2-1/2"
            termination = "draw_score"
            break
    else:
        result = "1/2-1/2"
        termination = "max_ply"

    return {
        "game_id":     game_id,
        "date":        date.today().isoformat(),
        "white":       white_engine.name,
        "black":       black_engine.name,
        "a_is_white":  a_is_white,
        "start_fen":   start_fen,
        "movetime_a":  movetime_a_ms,
        "movetime_b":  movetime_b_ms,
        "depth_a":     depth_a,
        "depth_b":     depth_b,
        "opening_plies": opening_plies,
        "moves":       moves,
        "scores":      scores,
        "result":      result,
        "termination": termination,
        "ply_count":   len(moves),
        "decisive":    result != "1/2-1/2",
    }


# ---------------------------------------------------------------------------
# Summary helpers
# ---------------------------------------------------------------------------

def _elo_diff(wins: int, draws: int, losses: int) -> float | None:
    """Approximate Elo difference from W/D/L for engine A."""
    n = wins + draws + losses
    if n == 0:
        return None
    score = (wins + 0.5 * draws) / n
    if score <= 0 or score >= 1:
        return None
    return -400 * math.log10(1 / score - 1)


def _print_summary(games: list[dict], name_a: str, name_b: str) -> None:
    print()
    print("=" * 62)
    print(" Gauntlet summary")
    print("=" * 62)

    a_wins = b_wins = draws = crashes = 0
    all_scores: list[int] = []
    term_counts: dict[str, int] = {}

    for g in games:
        r = g["result"]
        aiw = g.get("a_is_white", True)
        a_won = (r == "1-0" and aiw) or (r == "0-1" and not aiw)
        b_won = (r == "1-0" and not aiw) or (r == "0-1" and aiw)
        if a_won:
            a_wins += 1
        elif b_won:
            b_wins += 1
        else:
            draws += 1
        t = g["termination"]
        term_counts[t] = term_counts.get(t, 0) + 1
        if "crash" in t:
            crashes += 1
        for s in g["scores"]:
            if s is not None:
                all_scores.append(abs(s))

    total  = len(games)
    dec    = a_wins + b_wins
    sc_avg = sum(all_scores) / len(all_scores) if all_scores else 0.0
    elo    = _elo_diff(a_wins, draws, b_wins)

    print(f"  Games     : {total}")
    print(f"  {name_a:<12s} wins  : {a_wins}  ({100*a_wins/total:.1f}%)" if total else "")
    print(f"  Draws     : {draws}  ({100*draws/total:.1f}%)" if total else "")
    print(f"  {name_b:<12s} wins  : {b_wins}  ({100*b_wins/total:.1f}%)" if total else "")
    print(f"  Decisive  : {dec} / {total}  ({100*dec/total:.1f}%)" if total else "")
    print(f"  Avg |score|: {sc_avg:.0f} cp")
    print(f"  Crashes   : {crashes}")
    if elo is not None:
        print(f"  Elo diff  : {elo:+.0f}  (A relative to B, approximate)")
    if term_counts:
        parts = ", ".join(f"{k}={v}" for k, v in sorted(term_counts.items(), key=lambda x: -x[1]))
        print(f"  Termination: {parts}")
    print("=" * 62)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(
        description="Makruk-SF engine gauntlet / match runner",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument("--engine-a", default=None,
                    help="Engine A binary path (default: src/sf-kernel next to this file)")
    ap.add_argument("--engine-b", default=None,
                    help="Engine B binary path (default: same as engine A)")
    ap.add_argument("--games",       type=int,   default=10,
                    help="Total games to play (half A as White, half B as White)")
    ap.add_argument("--movetime-a",  type=int,   default=200,
                    help="Movetime for engine A in ms")
    ap.add_argument("--movetime-b",  type=int,   default=50,
                    help="Movetime for engine B in ms (set lower for decisive games)")
    ap.add_argument("--depth-a",     type=int,   default=None,
                    help="Fixed depth for engine A (overrides --movetime-a)")
    ap.add_argument("--depth-b",     type=int,   default=None,
                    help="Fixed depth for engine B (overrides --movetime-b)")
    ap.add_argument("--hash",        type=int,   default=16,
                    help="Hash table size in MB (shared by both engines)")
    ap.add_argument("--threads",     type=int,   default=1,
                    help="Thread count per engine")
    ap.add_argument("--opening-plies", type=int, default=0,
                    help="Randomize this many opening plies using MultiPV")
    ap.add_argument("--opening-top-n", type=int, default=3,
                    help="Number of top moves to randomly choose from during opening")
    ap.add_argument("--seed",        type=int,   default=None,
                    help="Random seed (for reproducible opening randomization / book sampling)")
    ap.add_argument("--book",        default=None,
                    help="EPD/FEN opening book path (e.g. tools/books/makruk_no_counting.epd). "
                         "Each game pair samples one book position and plays it twice with "
                         "colors swapped (proper paired games, cancels per-position bias). "
                         "Mutually exclusive with --opening-plies.")
    ap.add_argument("--adjudication-threshold", type=int, default=0,
                    help="Declare win when |score| >= this for --adjudication-streak half-moves "
                         "(0 = disabled).  Useful for handicap gauntlets where checkmate is rare.")
    ap.add_argument("--adjudication-streak",    type=int, default=5,
                    help="Consecutive half-moves above --adjudication-threshold needed to adjudicate")
    ap.add_argument("--output",      default=None,
                    help="Optional JSONL output file for game records")
    args = ap.parse_args()

    if args.book and args.opening_plies:
        print("ERROR: --book and --opening-plies are mutually exclusive "
              "(the book position already sets the opening; keep the game-pairing "
              "invariant clean by not randomizing further plies from it).", file=sys.stderr)
        sys.exit(1)

    book_fens: list[str] | None = None
    if args.book:
        book_fens = load_book(args.book)
        print(f"Book      : {args.book}  ({len(book_fens)} positions)")

    # Resolve engine paths
    script_dir = Path(__file__).resolve().parent
    default_engine = script_dir.parent / "src" / "sf-kernel"

    path_a = Path(args.engine_a) if args.engine_a else default_engine
    path_b = Path(args.engine_b) if args.engine_b else path_a

    for p in (path_a, path_b):
        if not p.exists():
            print(f"ERROR: engine not found: {p}", file=sys.stderr)
            sys.exit(1)

    name_a = f"A({path_a.name})"
    name_b = f"B({path_b.name})"
    if path_a == path_b:
        mt_a = args.depth_a if args.depth_a else f"{args.movetime_a}ms"
        mt_b = args.depth_b if args.depth_b else f"{args.movetime_b}ms"
        name_a = f"A({mt_a})"
        name_b = f"B({mt_b})"

    print(f"Engine A  : {path_a}  ({name_a})")
    print(f"Engine B  : {path_b}  ({name_b})")
    print(f"Games     : {args.games}")
    if args.depth_a:
        print(f"Depth A   : {args.depth_a}  (fixed depth)")
    else:
        print(f"Movetime A: {args.movetime_a} ms")
    if args.depth_b:
        print(f"Depth B   : {args.depth_b}  (fixed depth)")
    else:
        print(f"Movetime B: {args.movetime_b} ms")
    if args.opening_plies:
        print(f"Opening   : {args.opening_plies} random plies, top-{args.opening_top_n}")
    if args.adjudication_threshold:
        print(f"Adjudicate: |score| >= {args.adjudication_threshold} cp "
              f"for {args.adjudication_streak} half-moves")
    print()

    rng = random.Random(args.seed)

    # Launch engines
    try:
        eng_a = Engine(name_a, str(path_a), args.hash, args.threads)
        eng_b = Engine(name_b, str(path_b), args.hash, args.threads)
    except Exception as exc:
        print(f"ERROR: failed to start engine: {exc}", file=sys.stderr)
        sys.exit(1)

    games: list[dict] = []
    out_f = open(args.output, "w") if args.output else None

    print(f"{'Game':>5}  {'White':<14} {'Black':<14} {'Result':>7}  "
          f"{'Ply':>4}  Termination")
    print("-" * 62)

    pair_fen: str = MAKRUK_START_FEN  # current book position shared by a game pair

    try:
        for gid in range(1, args.games + 1):
            # Alternate which engine plays White
            a_is_white = (gid % 2 == 1)
            if a_is_white:
                white_eng, black_eng = eng_a, eng_b
            else:
                white_eng, black_eng = eng_b, eng_a

            # Book mode: draw a new position at the start of each pair (odd gid),
            # reuse it for the paired game (even gid) with colors swapped above.
            if book_fens is not None and a_is_white:
                pair_fen = rng.choice(book_fens)

            # Restart crashed engines
            for eng, path in ((eng_a, path_a), (eng_b, path_b)):
                if not eng.alive:
                    print(f"  Restarting {eng.name} after crash...")
                    try:
                        eng.__init__(eng.name, str(path), args.hash, args.threads)
                    except Exception:
                        pass

            t0 = time.monotonic()
            game = play_game(
                white_eng, black_eng, gid,
                args.movetime_a, args.movetime_b,
                args.depth_a, args.depth_b,
                args.opening_plies, args.opening_top_n,
                rng,
                a_is_white=a_is_white,
                adjudication_threshold=args.adjudication_threshold,
                adjudication_streak=args.adjudication_streak,
                start_fen=pair_fen if book_fens is not None else MAKRUK_START_FEN,
            )
            elapsed = time.monotonic() - t0
            games.append(game)

            if out_f:
                out_f.write(json.dumps(game, separators=(",", ":")) + "\n")
                out_f.flush()

            tag = "*" if game["decisive"] else ""
            print(f"{gid:5d}  {game['white']:<14} {game['black']:<14}"
                  f" {game['result']:>7}  {game['ply_count']:4d}"
                  f"  {game['termination']}{tag}")

    finally:
        eng_a.quit()
        eng_b.quit()
        if out_f:
            out_f.close()

    _print_summary(games, name_a, name_b)

    if args.output:
        print(f"\nOutput    : {args.output}")


if __name__ == "__main__":
    main()
