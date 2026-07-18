#!/usr/bin/env python3
"""
validate_net.py — Validate an exported Makruk-SF NNUE binary (makruk_net.bin).

Checks performed:
  1. File exists and is readable.
  2. Binary header: magic ('MKNN'), version, valid architecture dimensions.
  3. File size matches the size implied by the architecture.
  4. Weight range: no NaN, no Inf, reports [min, max].
  5. Optional engine smoke test: launch sf-kernel, confirm it responds and evals.

Note on net loading:
  makruk_net.bin is a float32 binary produced by tools/training/export.py.
  It is NOT a Stockfish .nnue file (which uses int8/int16 weights + Stockfish
  header). The engine smoke test verifies the engine binary works, not that it
  loads this net — that integration step is planned for a future PR once the
  training pipeline is mature enough for quantized export.

Usage:
    python tools/validate_net.py --net makruk_net.bin
    python tools/validate_net.py --net makruk_net.bin --engine src/sf-kernel
    python tools/validate_net.py --net makruk_net.bin --engine src/sf-kernel --eval

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

import argparse
import re
import struct
import subprocess
import sys
import threading
import queue
from pathlib import Path

MAGIC_EXPECTED   = 0x4D4B4E4E   # 'MKNN'
VERSION_EXPECTED = 1
HEADER_FMT       = '<IIIIII'
HEADER_SIZE      = struct.calcsize(HEADER_FMT)   # 24 bytes

MAKRUK_START_FEN = "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1"

_OK   = "[OK]  "
_FAIL = "[FAIL]"
_WARN = "[WARN]"
_INFO = "[INFO]"


def _ok(label: str, detail: str = "") -> bool:
    print(f"  {_OK}  {label}" + (f"  — {detail}" if detail else ""))
    return True


def _fail(label: str, detail: str = "") -> bool:
    print(f"  {_FAIL}  {label}" + (f"  — {detail}" if detail else ""))
    return False


def _info(msg: str) -> None:
    print(f"  {_INFO}  {msg}")


# ---------------------------------------------------------------------------
# Header parsing
# ---------------------------------------------------------------------------

def check_header(data: bytes) -> tuple[bool, dict]:
    """Parse and validate the binary header.  Returns (ok, arch_dict)."""
    if len(data) < HEADER_SIZE:
        _fail("Header size", f"file is {len(data)} bytes, need at least {HEADER_SIZE}")
        return False, {}

    magic, version, dims, l1, l2, l3 = struct.unpack_from(HEADER_FMT, data, 0)
    arch = {"dims": dims, "l1": l1, "l2": l2, "l3": l3}

    ok = True
    if magic != MAGIC_EXPECTED:
        ok = _fail("Magic", f"got 0x{magic:08X}, expected 0x{MAGIC_EXPECTED:08X} ('MKNN')")
    else:
        _ok("Magic", f"0x{magic:08X} ('MKNN')")

    if version != VERSION_EXPECTED:
        ok = _fail("Version", f"got {version}, expected {VERSION_EXPECTED}") and ok
    else:
        _ok("Version", str(version))

    if dims == 0 or dims > 65536:
        ok = _fail("Dims", f"dims={dims} out of range") and ok
    else:
        _ok("Dims", str(dims))

    if l1 == 0 or l2 == 0 or l3 == 0:
        ok = _fail("L1/L2/L3", f"L1={l1}  L2={l2}  L3={l3} (zero not allowed)") and ok
    else:
        _ok("L1/L2/L3", f"L1={l1}  L2={l2}  L3={l3}")

    return ok, arch


# ---------------------------------------------------------------------------
# Size check
# ---------------------------------------------------------------------------

def expected_file_size(arch: dict) -> int:
    """Compute the expected file size in bytes from the header architecture."""
    dims, l1, l2, l3 = arch["dims"], arch["l1"], arch["l2"], arch["l3"]
    n_floats = (
        l1            # FT bias
        + l1 * dims   # FT weight
        + l2          # L2 bias
        + l2 * 2 * l1 # L2 weight
        + l3          # L3 bias
        + l3 * l2     # L3 weight
        + 1           # out bias
        + 1 * l3      # out weight
    )
    return HEADER_SIZE + n_floats * 4


def check_size(data: bytes, arch: dict) -> bool:
    actual   = len(data)
    expected = expected_file_size(arch)
    size_mb  = actual / (1024 * 1024)
    if actual == expected:
        return _ok("File size", f"{actual:,} bytes ({size_mb:.2f} MB) — matches architecture")
    else:
        diff = actual - expected
        _fail("File size", f"got {actual:,}  expected {expected:,}  diff={diff:+,} bytes")
        return False


# ---------------------------------------------------------------------------
# Weight sanity
# ---------------------------------------------------------------------------

def check_weights(data: bytes) -> bool:
    raw = data[HEADER_SIZE:]
    n = len(raw) // 4
    if n == 0:
        return _fail("Weights", "no weight data after header")

    floats = struct.unpack_from(f"<{n}f", raw)

    nan_count = sum(1 for v in floats if v != v)
    inf_count = sum(1 for v in floats if v == float("inf") or v == float("-inf"))

    ok = True
    if nan_count:
        ok = _fail("NaN check", f"{nan_count:,} NaN values found") and ok
    else:
        _ok("NaN check", "none found")

    if inf_count:
        ok = _fail("Inf check", f"{inf_count:,} Inf values found") and ok
    else:
        _ok("Inf check", "none found")

    finite = [v for v in floats if v == v and v not in (float("inf"), float("-inf"))]
    if finite:
        vmin, vmax = min(finite), max(finite)
        _info(f"Weight range: [{vmin:.5f}, {vmax:.5f}]  ({n:,} floats)")
        large = sum(1 for v in finite if abs(v) > 1000)
        if large:
            print(f"  {_WARN}  {large:,} floats with |w|>1000 "
                  "(large biases are normal for FT; large weights may indicate training issues)")

    return ok


# ---------------------------------------------------------------------------
# Engine smoke test
# ---------------------------------------------------------------------------

def _reader_thread(stdout, line_queue: queue.Queue) -> None:
    """Background thread that reads lines from stdout into a queue."""
    try:
        for line in stdout:
            line_queue.put(line.rstrip("\n"))
    except Exception:
        pass
    finally:
        line_queue.put(None)   # sentinel


def engine_smoke_test(engine_path: Path, eval_flag: bool) -> bool:
    print(f"\n  Launching: {engine_path}")

    try:
        proc = subprocess.Popen(
            [str(engine_path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
    except OSError as exc:
        return _fail("Engine launch", str(exc))

    # Background reader avoids buffering issues with select + TextIOWrapper.
    q: queue.Queue = queue.Queue()
    t = threading.Thread(target=_reader_thread, args=(proc.stdout, q), daemon=True)
    t.start()

    def send(cmd: str) -> None:
        proc.stdin.write(cmd + "\n")
        proc.stdin.flush()

    def readline(timeout: float = 8.0) -> str | None:
        try:
            line = q.get(timeout=timeout)
            return line   # None = EOF sentinel
        except queue.Empty:
            return None

    def wait_for(prefix: str, timeout: float = 8.0) -> str | None:
        for _ in range(200):
            line = readline(timeout)
            if line is None:
                return None
            if line.startswith(prefix):
                return line
        return None

    ok = True
    try:
        send("uci")
        uciok = wait_for("uciok")
        if uciok is None:
            return _fail("Engine UCI", "no 'uciok' within 8 s")
        _ok("Engine UCI", "received uciok")

        send("setoption name UCI_Variant value makruk")
        send("isready")
        readyok = wait_for("readyok")
        if readyok is None:
            return _fail("Engine ready", "no 'readyok'")
        _ok("Engine ready", "received readyok")

        if eval_flag:
            send(f"position fen {MAKRUK_START_FEN}")
            send("go movetime 50")

            score_cp: int | None = None
            bestmove: str | None = None

            for _ in range(2000):
                line = readline(3.0)
                if line is None:
                    break
                if line.startswith("info"):
                    m = re.search(r"\bscore (cp|mate) (-?\d+)", line)
                    if m:
                        score_cp = int(m.group(2))
                elif line.startswith("bestmove"):
                    parts = line.split()
                    bestmove = parts[1] if len(parts) > 1 else None
                    break

            if bestmove and bestmove not in ("(none)", "0000"):
                _ok("Eval smoke test", f"bestmove={bestmove}"
                    + (f"  score={score_cp:+d}cp" if score_cp is not None else ""))
            else:
                ok = _fail("Eval smoke test", "no bestmove returned")
    except Exception as exc:
        ok = _fail("Smoke test", str(exc))
    finally:
        try:
            send("quit")
            proc.wait(timeout=5)
        except Exception:
            proc.kill()

    _info("Note: engine smoke test verifies the engine binary works, not that it loads"
          "\n         makruk_net.bin — quantized net loading is planned for a future PR.")
    return ok


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(
        description="Validate Makruk-SF NNUE binary (makruk_net.bin)",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument("--net",    required=True,
                    help="Path to makruk_net.bin (from tools/training/export.py)")
    ap.add_argument("--engine", default=None,
                    help="Optional: path to sf-kernel binary for smoke test")
    ap.add_argument("--eval",   action="store_true",
                    help="Run eval on the Makruk start position during smoke test")
    args = ap.parse_args()

    net_path = Path(args.net)

    print("=" * 62)
    print(f" Net validation: {net_path}")
    print("=" * 62)

    all_ok = True

    # 1. Existence
    if not net_path.exists():
        _fail("File exists", str(net_path))
        print(f"\n  {_FAIL}  Cannot continue — file not found.")
        sys.exit(1)

    size    = net_path.stat().st_size
    size_mb = size / (1024 * 1024)
    _ok("File exists", f"{size:,} bytes ({size_mb:.2f} MB)")

    # 2. Read bytes
    data = net_path.read_bytes()

    # 3. Header
    print("\nHeader:")
    ok_hdr, arch = check_header(data)
    all_ok = all_ok and ok_hdr

    # 4. Size consistency
    if arch:
        print("\nSize consistency:")
        all_ok = check_size(data, arch) and all_ok

    # 5. Weights
    print("\nWeights:")
    all_ok = check_weights(data) and all_ok

    # 6. Engine smoke test
    if args.engine:
        print("\nEngine smoke test:")
        all_ok = engine_smoke_test(Path(args.engine), args.eval) and all_ok

    print()
    print("=" * 62)
    if all_ok:
        print(f"  {_OK}  All checks passed.")
    else:
        print(f"  {_FAIL}  Some checks failed — see output above.")
    print("=" * 62)

    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
