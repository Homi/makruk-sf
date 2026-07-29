#!/usr/bin/env python3
"""
teacher_label.py — Re-score training positions using a teacher engine.

Reads a TSV produced by convert.py, queries each unique FEN from a stronger
"teacher" engine (typically Fairy-Stockfish) at a fixed depth, and writes a
new TSV where score_cp comes from the teacher rather than sf-kernel's own
search.  This produces higher-quality labels for NNUE training.

The score_source column is set to "teacher_<engine-name>".

Duplicate FENs are queried only once.  An optional --resume flag skips FENs
already present in the output file so long jobs can be interrupted and
restarted.

Usage:
    python teacher_label.py \\
        --input  tools/training/train_mixed.tsv \\
        --output tools/training/train_fairy_d8.tsv \\
        --teacher /path/to/fairy-stockfish \\
        --depth 8

    # Resume after interruption:
    python teacher_label.py \\
        --input  train_mixed.tsv \\
        --output train_fairy_d8.tsv \\
        --teacher /path/to/fairy-stockfish \\
        --depth 8 --resume

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

import argparse
import queue
import re
import subprocess
import sys
import threading
import time
from pathlib import Path


def _has_bare_king(fen: str) -> bool:
    """Return True if either side has only a King (counting likely active)."""
    board = fen.split()[0]
    white_non_king = sum(1 for c in board if c.isupper() and c != 'K')
    black_non_king = sum(1 for c in board if c.islower() and c != 'k')
    return white_non_king == 0 or black_non_king == 0


# ---------------------------------------------------------------------------
# Teacher engine wrapper
# ---------------------------------------------------------------------------

class TeacherEngine:
    """Minimal UCI engine wrapper for static position evaluation."""

    def __init__(self, path: str, hash_mb: int = 32) -> None:
        self._proc = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        self._q: queue.Queue = queue.Queue()
        self._alive = True
        self._reader = threading.Thread(
            target=self._read_thread, args=(), daemon=True
        )
        self._reader.start()

        self._send("uci")
        self._wait_for("uciok")
        self._send("setoption name UCI_Variant value makruk")
        self._send(f"setoption name Hash value {hash_mb}")
        self._send("setoption name Threads value 1")
        self._send("isready")
        self._wait_for("readyok")

    def _read_thread(self) -> None:
        try:
            for line in self._proc.stdout:
                self._q.put(line.rstrip("\n"))
        finally:
            self._q.put(None)

    def _send(self, cmd: str) -> None:
        self._proc.stdin.write(cmd + "\n")
        self._proc.stdin.flush()

    def _readline(self, timeout: float) -> str | None:
        try:
            line = self._q.get(timeout=timeout)
            if line is None:
                self._alive = False
            return line
        except queue.Empty:
            return None

    def _wait_for(self, prefix: str, timeout: float = 10.0) -> None:
        for _ in range(2000):
            line = self._readline(timeout)
            if line is None:
                return
            if line.startswith(prefix):
                return

    def evaluate(self, fen: str, depth: int) -> int | None:
        """
        Return the position score in centipawns from White's perspective,
        or None on timeout/crash.

        The teacher engine returns scores from the side-to-move's perspective;
        this method converts to White's perspective automatically.
        """
        if not self._alive:
            return None

        stm_white = fen.split()[1] == 'w'

        self._send(f"position fen {fen}")
        self._send(f"go depth {depth}")

        score_cp: int | None = None
        timeout = depth * 1.0 + 10.0

        for _ in range(500_000):
            line = self._readline(timeout)
            if line is None:
                self._alive = False
                return None
            if line.startswith("info"):
                m = re.search(r"\bscore (cp|mate) (-?\d+)", line)
                if m:
                    kind, val = m.group(1), int(m.group(2))
                    if kind == "cp":
                        # Side-to-move → White's perspective
                        score_cp = val if stm_white else -val
                    else:
                        # Mate score: preserve sign, cap magnitude
                        plies = val  # positive = I win, negative = I lose
                        large = max(1, 30000 - abs(plies))
                        stm_sign = 1 if plies > 0 else -1
                        wcp = stm_sign * large
                        score_cp = wcp if stm_white else -wcp
            elif line.startswith("bestmove"):
                return score_cp

        return score_cp

    def quit(self) -> None:
        try:
            self._send("quit")
            self._proc.wait(timeout=5)
        except Exception:
            self._proc.kill()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def load_done_fens(path: Path) -> set[str]:
    """Read FENs already present in an existing output file (for --resume)."""
    done: set[str] = set()
    try:
        with open(path) as f:
            for line in f:
                if line.startswith("fen\t"):
                    continue
                parts = line.split("\t")
                if parts:
                    done.add(parts[0])
    except FileNotFoundError:
        pass
    return done


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Re-score training positions with a teacher engine",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument("--input",   required=True,
                    help="TSV from convert.py (fen, score_cp, wdl, decisive, mode, score_source)")
    ap.add_argument("--output",  required=True,
                    help="Output TSV with teacher-scored positions")
    ap.add_argument("--teacher", required=True,
                    help="Path to teacher engine binary (e.g. fairy-stockfish)")
    ap.add_argument("--depth",   type=int, default=8,
                    help="Search depth for teacher evaluation")
    ap.add_argument("--hash",    type=int, default=64,
                    help="Hash table size in MB for teacher engine")
    ap.add_argument("--resume",  action="store_true",
                    help="Skip FENs already present in --output (resume interrupted run)")
    ap.add_argument("--max-score", type=int, default=2500,
                    help="Drop positions where |teacher score| exceeds this (avoids near-terminal)")
    ap.add_argument("--skip-bare-king", action="store_true", default=True,
                    help="Skip positions where one side has only a King (counting active, labeled ≈0)")
    ap.add_argument("--no-skip-bare-king", dest="skip_bare_king", action="store_false",
                    help="Include bare-king positions (not recommended — Fairy-SF labels them ≈0)")
    args = ap.parse_args()

    in_path  = Path(args.input)
    out_path = Path(args.output)
    teacher_path = Path(args.teacher)

    if not in_path.exists():
        print(f"ERROR: input not found: {in_path}", file=sys.stderr)
        sys.exit(1)
    if not teacher_path.exists():
        print(f"ERROR: teacher engine not found: {teacher_path}", file=sys.stderr)
        sys.exit(1)

    teacher_name = teacher_path.name
    score_source = f"teacher_{teacher_name}"

    # Load existing output if resuming
    done_fens: set[str] = set()
    if args.resume and out_path.exists():
        done_fens = load_done_fens(out_path)
        print(f"Resume    : {len(done_fens)} positions already done")

    # Load all rows from input
    rows: list[dict] = []
    with open(in_path) as f:
        header = f.readline().rstrip("\n").split("\t")
        for line in f:
            parts = line.rstrip("\n").split("\t")
            rows.append(dict(zip(header, parts)))

    total_rows = len(rows)
    print(f"Input     : {in_path}  ({total_rows} positions)")
    print(f"Teacher   : {teacher_path}  (depth={args.depth})")
    print(f"Output    : {out_path}")
    if args.skip_bare_king:
        print(f"Filter    : bare-king positions skipped (counting-active, would be labeled ≈0)")
    print()

    # Start teacher engine
    try:
        engine = TeacherEngine(str(teacher_path), hash_mb=args.hash)
    except Exception as exc:
        print(f"ERROR: could not start teacher engine: {exc}", file=sys.stderr)
        sys.exit(1)

    # Open output in append mode (or write mode if not resuming)
    mode = "a" if args.resume and out_path.exists() else "w"
    out_f = open(out_path, mode)
    if mode == "w":
        out_f.write("fen\tscore_cp\twdl\tdecisive\tmode\tscore_source\n")

    scored = skipped = errors = bare_king_dropped = 0
    t0 = time.monotonic()

    try:
        for i, row in enumerate(rows):
            fen = row.get("fen", "")
            if not fen:
                continue

            if fen in done_fens:
                skipped += 1
                continue

            if args.skip_bare_king and _has_bare_king(fen):
                bare_king_dropped += 1
                done_fens.add(fen)
                continue

            teacher_score = engine.evaluate(fen, args.depth)

            if not engine._alive:
                print(f"\nERROR: teacher engine died at position {i+1}", file=sys.stderr)
                break

            if teacher_score is None or abs(teacher_score) > args.max_score:
                errors += 1
                done_fens.add(fen)
                continue

            out_f.write(
                f"{fen}\t{teacher_score}\t{row.get('wdl','0.5')}\t"
                f"{row.get('decisive','0')}\t{row.get('mode','normal')}\t"
                f"{score_source}\n"
            )
            out_f.flush()
            scored += 1
            done_fens.add(fen)

            # Progress report every 500 positions
            processed = scored + errors + bare_king_dropped
            if processed % 500 == 0 and processed > 0:
                elapsed = time.monotonic() - t0
                rate = processed / elapsed if elapsed > 0 else 0
                remaining = (total_rows - skipped - processed) / rate if rate > 0 else 0
                print(f"  [{i+1}/{total_rows}] scored={scored} bare_king_drop={bare_king_dropped} "
                      f"errors={errors} rate={rate:.1f}/s  eta={remaining/60:.1f}min")

    finally:
        out_f.close()
        engine.quit()

    elapsed = time.monotonic() - t0
    print()
    print(f"Done      : {scored} positions scored, {skipped} skipped (resume), "
          f"{errors} dropped (score range), {bare_king_dropped} dropped (bare-king)")
    print(f"Elapsed   : {elapsed/60:.1f} min  ({elapsed:.0f}s)")
    print(f"Output    : {out_path}")


if __name__ == "__main__":
    main()
