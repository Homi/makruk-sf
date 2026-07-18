#!/usr/bin/env python3
"""
selfplay.py — Makruk-SF self-play game generator.

Plays sf-kernel against itself from the Makruk start position,
writing one PGN file and one JSONL file (one record per game).

Modes:
  normal          — Both sides use identical movetime.  Produces mostly draws.
  time_handicap   — White plays at full movetime; Black plays at
                    movetime * handicap_ratio (default 0.2).
                    Generates decisive games for dataset diversity.
  depth_handicap  — White plays at full movetime; Black plays at a fixed depth
                    (--depth-cap, default 4).  Fast and decisive.
  random_opening  — First --opening-plies plies randomly select from the top
                    --opening-top-n candidates (via MultiPV).  Reduces draw
                    rate by diversifying the opening.  Combine with
                    time_handicap for maximum decisiveness.

Score adjudication:
  Makruk counting rules prevent many decisive endgames even when one side has
  a large material advantage.  Score adjudication declares a winner based on
  evaluation rather than waiting for checkmate:
    --adjudication-threshold 500  (cp from White's perspective)
    --adjudication-streak 5       (consecutive half-moves above threshold)
  When White's evaluation stays ≥ +threshold for streak half-moves (after ply
  20), the game is adjudicated as a White win, and vice versa.  Set threshold
  to 0 to disable adjudication.

Usage examples:
    # Normal self-play (baseline, mostly draws)
    python selfplay.py --games 100 --movetime 100

    # Generate decisive games — strong vs weak movetime + adjudication
    python selfplay.py --games 100 --mode time_handicap --movetime 300 \\
        --handicap-ratio 0.15 --adjudication-threshold 500

    # Depth handicap — fast decisive games
    python selfplay.py --games 50 --mode depth_handicap --movetime 200 \\
        --depth-cap 3 --adjudication-threshold 500 --adjudication-streak 5

    # Randomized openings to break draw loops, then normal play
    python selfplay.py --games 100 --mode random_opening --movetime 150 \\
        --opening-plies 8 --opening-top-n 4 --seed 42

    # Resume a run
    python selfplay.py --games 200 --movetime 100 --resume

Resume support:
    selfplay_state.json in --outdir tracks games_completed.
    Pass --resume to append to existing PGN/JSONL and continue.

JSONL record fields (all games):
    game_id, date, start_fen, moves, scores, result, termination,
    ply_count, movetime_ms, mode, decisive

Additional fields when applicable:
    handicap_ratio          — time_handicap mode
    depth_cap               — depth_handicap mode
    opening_plies           — random_opening (and any mode with --opening-plies > 0)
    opening_top_n           — same
    seed                    — when --seed is set
    adjudication_threshold  — when adjudication is enabled (threshold > 0)
    adjudication_streak     — when adjudication is enabled

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

import argparse
import json
import random
import re
import subprocess
import sys
import time
from datetime import date
from pathlib import Path

MAKRUK_START_FEN = "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1"
MAX_PLY = 480

SCRIPT_DIR     = Path(__file__).resolve().parent
DEFAULT_ENGINE = SCRIPT_DIR.parent.parent / "src" / "sf-kernel"
STATE_FILE     = "selfplay_state.json"

MODE_NORMAL        = "normal"
MODE_TIME_HANDICAP = "time_handicap"
MODE_DEPTH_HANDICAP = "depth_handicap"
MODE_RANDOM_OPENING = "random_opening"
ALL_MODES = [MODE_NORMAL, MODE_TIME_HANDICAP, MODE_DEPTH_HANDICAP, MODE_RANDOM_OPENING]


# ---------------------------------------------------------------------------
# Engine wrapper
# ---------------------------------------------------------------------------

class Engine:
    """Manage a single UCI engine subprocess."""

    def __init__(self, path: str, hash_mb: int, threads: int,
                 use_counting: bool = True) -> None:
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
        # Disable Makruk counting draw for data generation so material-imbalance
        # endgames are played out (and evaluated with real, non-zero scores)
        # rather than being drawn by counting. Never use for real play.
        if not use_counting:
            self._send("setoption name UseCounting value false")
        self._send("isready")
        self._wait_for("readyok")

    def _send(self, cmd: str) -> None:
        self._proc.stdin.write(cmd + "\n")
        self._proc.stdin.flush()

    def _readline(self) -> str:
        return self._proc.stdout.readline().rstrip("\n")

    def _wait_for(self, prefix: str) -> None:
        for _ in range(2000):
            line = self._readline()
            if line.startswith(prefix):
                return

    def set_option(self, name: str, value) -> None:
        self._send(f"setoption name {name} value {value}")

    def search(
        self,
        moves: list[str],
        movetime_ms: int | None = None,
        depth: int | None = None,
        start_fen: str | None = None,
    ) -> tuple[str | None, int | None, bool]:
        """
        Search from start_fen (or the Makruk start position) + moves list.

        Returns (bestmove, score_cp, is_mate_score).
        bestmove is None when no legal move exists (checkmate / stalemate).
        """
        pos_cmd = "position fen " + (start_fen or MAKRUK_START_FEN)
        if moves:
            pos_cmd += " moves " + " ".join(moves)
        self._send(pos_cmd)

        if depth is not None:
            self._send(f"go depth {depth}")
        else:
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

    def search_multipv(
        self,
        moves: list[str],
        movetime_ms: int,
        top_n: int,
        start_fen: str | None = None,
        depth: int | None = None,
    ) -> list[tuple[str, int | None]]:
        """
        Search with MultiPV and return up to top_n (move, score_cp) pairs
        in descending quality order.  Falls back gracefully to bestmove only.
        When `depth` is given, search to a fixed depth instead of by movetime.
        """
        self.set_option("MultiPV", top_n)

        pos_cmd = "position fen " + (start_fen or MAKRUK_START_FEN)
        if moves:
            pos_cmd += " moves " + " ".join(moves)
        self._send(pos_cmd)
        if depth is not None:
            self._send(f"go depth {depth}")
        else:
            self._send(f"go movetime {movetime_ms}")

        candidates: dict[int, tuple[str, int | None]] = {}

        while True:
            line = self._readline()
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
# Repetition helper
# ---------------------------------------------------------------------------

def _is_threefold(moves: list[str]) -> bool:
    """Return True if the last moves form a 3-fold repetition cycle."""
    for cycle in (2, 4, 6):
        needed = cycle * 2
        if len(moves) >= needed and moves[-cycle:] == moves[-2 * cycle: -cycle]:
            return True
    return False


# ---------------------------------------------------------------------------
# Game logic
# ---------------------------------------------------------------------------

def play_game(
    engine: Engine,
    game_id: int,
    movetime_ms: int,
    mode: str,
    handicap_ratio: float,
    depth_cap: int | None,
    opening_plies: int,
    opening_top_n: int,
    rng: random.Random,
    adjudication_threshold: int = 0,
    adjudication_streak: int = 5,
    start_fen: str | None = None,
    strong_depth: int | None = None,
) -> dict:
    """Play one game; return a complete game record.

    When `strong_depth` is set, the strong side (and opening) searches to a
    fixed depth instead of by movetime.  This is a workaround for a search
    crash on strongly-winning positions at depth >=9 (see CLAUDE.md); capping
    the strong side at depth <=8 avoids the crash during data generation.
    """
    engine.new_game()
    fen = start_fen or MAKRUK_START_FEN

    moves:  list[str]       = []
    scores: list[int | None] = []
    result      = "*"
    termination = "unterminated"
    zero_streak = 0
    white_adv_streak = 0
    black_adv_streak = 0

    for ply in range(MAX_PLY):
        is_white = (ply % 2 == 0)

        # Determine search parameters for this ply
        if ply < opening_plies and opening_top_n > 1:
            # Random opening: use MultiPV to get candidates then pick one.
            # Cap depth (strong_depth) so imbalanced openings don't hit the
            # depth>=9 crash on already-winning positions.
            candidates = engine.search_multipv(moves, movetime_ms, opening_top_n,
                                               start_fen=fen, depth=strong_depth)
            if candidates:
                mv, score_cp = rng.choice(candidates)
                is_mate = False
            elif strong_depth is not None:
                mv, score_cp, is_mate = engine.search(moves, depth=strong_depth, start_fen=fen)
            else:
                mv, score_cp, is_mate = engine.search(moves, movetime_ms=movetime_ms, start_fen=fen)
        elif mode == MODE_TIME_HANDICAP and not is_white:
            # Black plays at reduced movetime
            weak_ms = max(1, int(movetime_ms * handicap_ratio))
            mv, score_cp, is_mate = engine.search(moves, movetime_ms=weak_ms, start_fen=fen)
        elif mode == MODE_DEPTH_HANDICAP and not is_white:
            # Black plays at fixed depth
            mv, score_cp, is_mate = engine.search(moves, depth=depth_cap, start_fen=fen)
        elif strong_depth is not None:
            # Strong side at a fixed depth (crash-safe workaround)
            mv, score_cp, is_mate = engine.search(moves, depth=strong_depth, start_fen=fen)
        else:
            # Normal movetime search
            mv, score_cp, is_mate = engine.search(moves, movetime_ms=movetime_ms, start_fen=fen)

        if mv is None:
            if is_mate and score_cp == 0:
                result = "0-1" if ply % 2 == 0 else "1-0"
                termination = "checkmate"
            else:
                result = "1/2-1/2"
                termination = "stalemate"
            break

        scores.append(score_cp)
        moves.append(mv)

        # Score from White's perspective (score_cp is from side-to-move's view)
        white_score = (score_cp or 0) if is_white else -(score_cp or 0)
        # Mate scores count as very large wins regardless of mate distance
        if is_mate:
            white_score = 30000 if is_white else -30000

        if score_cp is not None and abs(score_cp) <= 2 and not is_mate:
            zero_streak += 1
        else:
            zero_streak = 0

        # Score adjudication (only after ply 20 to skip opening volatility)
        if adjudication_threshold > 0 and ply >= 20:
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

        if ply >= 20 and zero_streak >= 6:
            result = "1/2-1/2"
            termination = "draw_score"
            break
    else:
        result = "1/2-1/2"
        termination = "max_ply"

    record: dict = {
        "game_id":    game_id,
        "date":       date.today().isoformat(),
        "start_fen":  fen,
        "moves":      moves,
        "scores":     scores,
        "result":     result,
        "termination": termination,
        "ply_count":  len(moves),
        "movetime_ms": movetime_ms,
        "mode":       mode,
        "decisive":   result != "1/2-1/2",
    }
    if mode == MODE_TIME_HANDICAP:
        record["handicap_ratio"] = handicap_ratio
    if mode == MODE_DEPTH_HANDICAP:
        record["depth_cap"] = depth_cap
    if opening_plies > 0:
        record["opening_plies"] = opening_plies
        record["opening_top_n"] = opening_top_n
    if adjudication_threshold > 0:
        record["adjudication_threshold"] = adjudication_threshold
        record["adjudication_streak"]    = adjudication_streak

    return record


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
    f.write(f'[Mode "{game["mode"]}"]\n')
    f.write(f'[MoveTime "{game["movetime_ms"]}"]\n')
    if game.get("handicap_ratio") is not None:
        f.write(f'[HandicapRatio "{game["handicap_ratio"]}"]\n')
    if game.get("depth_cap") is not None:
        f.write(f'[DepthCap "{game["depth_cap"]}"]\n')
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
    f.write(json.dumps(game, separators=(",", ":")) + "\n")


# ---------------------------------------------------------------------------
# State (resume support)
# ---------------------------------------------------------------------------

def load_state(out_dir: Path) -> dict:
    p = out_dir / STATE_FILE
    return json.loads(p.read_text()) if p.exists() else {"games_completed": 0}


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
    parser.add_argument("--engine",   default=str(DEFAULT_ENGINE),
                        help="Path to sf-kernel binary")
    parser.add_argument("--games",    type=int, default=10,
                        help="Number of games to generate")
    parser.add_argument("--movetime", type=int, default=100,
                        help="Time per move in ms (strong side or both sides in normal mode)")
    parser.add_argument("--threads",  type=int, default=1,
                        help="Engine thread count")
    parser.add_argument("--hash",     type=int, default=16,
                        help="Engine hash table size in MB")
    parser.add_argument("--mode",     choices=ALL_MODES, default=MODE_NORMAL,
                        help=(
                            "normal: symmetric; "
                            "time_handicap: Black uses movetime*handicap-ratio; "
                            "depth_handicap: Black uses fixed depth; "
                            "random_opening: randomise opening plies via MultiPV"
                        ))
    parser.add_argument("--handicap-ratio", type=float, default=0.2,
                        help="[time_handicap] Black movetime fraction (0.0–1.0)")
    parser.add_argument("--depth-cap", type=int, default=4,
                        help="[depth_handicap] Fixed search depth for Black")
    parser.add_argument("--opening-plies", type=int, default=0,
                        help="Randomise this many opening plies using MultiPV "
                             "(works with any mode; 0 = disabled)")
    parser.add_argument("--opening-top-n", type=int, default=3,
                        help="Number of top moves to pick from during random opening")
    parser.add_argument("--seed",     type=int, default=None,
                        help="Random seed for opening randomisation (reproducibility)")
    parser.add_argument("--adjudication-threshold", type=int, default=0,
                        help="Adjudicate as decisive when |score| ≥ this value (cp, White's view) "
                             "for --adjudication-streak consecutive half-moves. 0 = disabled.")
    parser.add_argument("--adjudication-streak", type=int, default=5,
                        help="Number of consecutive half-moves above threshold to trigger adjudication")
    parser.add_argument("--pgn",      default="selfplay.pgn",
                        help="PGN output filename")
    parser.add_argument("--jsonl",    default="selfplay.jsonl",
                        help="JSONL output filename")
    parser.add_argument("--outdir",   default=".",
                        help="Output directory")
    parser.add_argument("--resume",   action="store_true",
                        help="Resume from last completed game (append to outputs)")
    parser.add_argument("--fens-file", default=None,
                        help="File with one start FEN per line; each game picks a random FEN. "
                             "Lines starting with '#' and blank lines are ignored.")
    parser.add_argument("--no-counting", action="store_true",
                        help="Disable Makruk counting draw in the engine (UseCounting=false). "
                             "Data generation only: forces material-imbalance endgames to be "
                             "played out with real, non-zero eval scores. Never for real play.")
    parser.add_argument("--strong-depth", type=int, default=0,
                        help="Search the strong side (and openings) at this fixed depth instead "
                             "of by movetime. Set <=8 to avoid the depth>=9 search crash on "
                             "strongly-winning positions during data generation. 0 = use movetime.")
    args = parser.parse_args()

    out_dir = Path(args.outdir)
    out_dir.mkdir(parents=True, exist_ok=True)

    engine_path = Path(args.engine)
    if not engine_path.exists():
        print(f"ERROR: engine not found: {engine_path}", file=sys.stderr)
        sys.exit(1)

    if args.mode == MODE_TIME_HANDICAP and not (0.0 < args.handicap_ratio < 1.0):
        print("ERROR: --handicap-ratio must be between 0.0 and 1.0 (exclusive)",
              file=sys.stderr)
        sys.exit(1)

    start_fens: list[str] = []
    if args.fens_file:
        fens_path = Path(args.fens_file)
        if not fens_path.exists():
            print(f"ERROR: fens-file not found: {fens_path}", file=sys.stderr)
            sys.exit(1)
        for line in fens_path.read_text().splitlines():
            line = line.strip()
            if line and not line.startswith('#'):
                start_fens.append(line)
        if not start_fens:
            print("ERROR: fens-file is empty or has no valid FEN lines", file=sys.stderr)
            sys.exit(1)
        print(f"FENs     : {len(start_fens)} imbalanced start positions from {fens_path.name}")

    state = load_state(out_dir) if args.resume else {"games_completed": 0}
    already_done = state["games_completed"]
    first_game   = already_done + 1
    last_game    = already_done + args.games

    pgn_path   = out_dir / args.pgn
    jsonl_path = out_dir / args.jsonl

    print(f"Engine   : {engine_path}")
    print(f"Mode     : {args.mode}")
    print(f"Games    : {first_game}–{last_game}  ({args.games} games)")
    print(f"Movetime : {args.movetime} ms", end="")
    if args.mode == MODE_TIME_HANDICAP:
        weak_ms = max(1, int(args.movetime * args.handicap_ratio))
        print(f"  |  Black: {weak_ms} ms  (ratio={args.handicap_ratio})", end="")
    elif args.mode == MODE_DEPTH_HANDICAP:
        print(f"  |  Black: depth {args.depth_cap}", end="")
    print()
    if args.opening_plies:
        print(f"Opening  : {args.opening_plies} random plies, top-{args.opening_top_n}")
    if args.seed is not None:
        print(f"Seed     : {args.seed}")
    if args.adjudication_threshold > 0:
        print(f"Adjudicate: ±{args.adjudication_threshold} cp × {args.adjudication_streak} half-moves")
    print(f"PGN      : {pgn_path}")
    print(f"JSONL    : {jsonl_path}")
    if args.resume and already_done:
        print(f"Resuming : continuing after game {already_done}")
    print()

    if args.no_counting:
        print("Counting : DISABLED (UseCounting=false) — data generation mode")
    if args.strong_depth:
        print(f"StrongDep: fixed depth {args.strong_depth} (crash-safe workaround)")
    rng       = random.Random(args.seed)
    engine    = Engine(str(engine_path), hash_mb=args.hash, threads=args.threads,
                       use_counting=not args.no_counting)
    open_mode = "a" if args.resume else "w"

    games_written = 0
    decisive      = 0
    try:
        with open(pgn_path, open_mode) as pgn_f, open(jsonl_path, open_mode) as jsonl_f:
            for gid in range(first_game, last_game + 1):
                t0 = time.monotonic()
                try:
                    game_fen = rng.choice(start_fens) if start_fens else None
                    game = play_game(
                        engine, gid, args.movetime,
                        mode=args.mode,
                        handicap_ratio=args.handicap_ratio,
                        depth_cap=args.depth_cap,
                        opening_plies=args.opening_plies,
                        opening_top_n=args.opening_top_n,
                        adjudication_threshold=args.adjudication_threshold,
                        adjudication_streak=args.adjudication_streak,
                        rng=rng,
                        start_fen=game_fen,
                        strong_depth=args.strong_depth or None,
                    )
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
                if game["decisive"]:
                    decisive += 1

                dec_tag = " *DECISIVE*" if game["decisive"] else ""
                print(
                    f"  [{gid:4d}/{last_game}]"
                    f"  {game['ply_count']:3d} ply"
                    f"  {game['result']:7s}"
                    f"  {elapsed:5.1f}s"
                    f"  {game['termination']}"
                    f"{dec_tag}"
                )
    finally:
        engine.quit()

    pct = 100 * decisive / games_written if games_written else 0.0
    print(f"\nDone — {games_written} game{'s' if games_written != 1 else ''} written.")
    print(f"Decisive : {decisive} / {games_written}  ({pct:.1f}%)")
    print(f"PGN      : {pgn_path}")
    print(f"JSONL    : {jsonl_path}")


if __name__ == "__main__":
    main()
