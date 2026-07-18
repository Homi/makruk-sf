# Makruk-SF Dataset Guide

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

---

## Why all-draw data is weak for training

NNUE training uses a combined loss:

```
target = λ · sigmoid(score_cp / 400) + (1 − λ) · wdl
```

When `λ = 0.5`, half the signal comes from the engine's centipawn score
and half from the game result (W/D/L).

When every game is a draw, the **result half of the signal collapses**:
every position gets `wdl = 0.5` regardless of material or tactical situation.
The model cannot learn that winning positions should score high and losing
positions should score low from game outcomes — that information is simply
absent in the data.

The score_cp half still carries signal (the engine evaluates positions
differently even in drawn games), but it is noisy at short movetime
and anchored near zero in symmetric, repetitive play.

**Empirical consequence:** The first training run on 21 250 positions from
110 all-draw games achieved a best validation loss of ~0.6715.  This is
barely better than predicting the draw mean (loss ≈ ln 2 ≈ 0.693).
The model learned some structure but cannot generalise to unbalanced positions.

---

## Recommended dataset mix

| Source | Share | Purpose |
|---|---|---|
| Normal self-play | 40% | Balanced positions, search score signal |
| Time/depth handicap self-play | 20% | Decisive games, W/D/L signal |
| Random-opening self-play | 20% | Opening diversity, fewer draw loops |
| Tactical / endgame synthetic positions | 10% | Extreme material imbalance |
| Teacher-labeled positions (future) | 10% | High-quality score labels |

Start with the first three sources.  The synthetic and teacher rows require
additional tooling not yet implemented.

---

## How to generate decisive games

### Option 1 — Time handicap (recommended for large batches)

White plays at full movetime; Black plays at a fraction of that time.
A ratio of 0.10–0.20 typically produces 30–60% decisive games.

```bash
cd tools/selfplay

# Small smoke test (5 games)
python selfplay.py --games 5 --mode time_handicap --movetime 200 \
    --handicap-ratio 0.15 --outdir out/handicap/

# Larger batch
python selfplay.py --games 200 --mode time_handicap --movetime 300 \
    --handicap-ratio 0.15 --outdir out/handicap/ --resume
```

### Option 2 — Depth handicap (fast, very decisive)

White uses movetime search; Black is capped at fixed depth.
Depth 3–5 is weak enough to lose material and decisive enough to finish quickly.

```bash
python selfplay.py --games 100 --mode depth_handicap --movetime 200 \
    --depth-cap 3 --outdir out/depth_handicap/
```

### Option 3 — Random openings (break draw loops)

Both sides use identical time, but the first N plies randomly pick from the
top K MultiPV candidates.  This disrupts the engine's tendency to repeat
the same optimal main lines.

```bash
python selfplay.py --games 100 --mode random_opening --movetime 150 \
    --opening-plies 8 --opening-top-n 4 --seed 42 --outdir out/random_open/
```

### Option 4 — Combine modes

Use random opening + time handicap together for maximum diversity:

```bash
python selfplay.py --games 200 --mode time_handicap --movetime 300 \
    --handicap-ratio 0.15 --opening-plies 6 --opening-top-n 3 \
    --seed 100 --outdir out/mixed/
```

---

## Gauntlet testing

The gauntlet runner (`tools/gauntlet.py`) measures engine strength before
and after training.  It is also useful for verifying that the dataset
generation settings actually produce decisive games.

```bash
# Quick 20-game match: strong (200ms) vs weak (40ms)
python tools/gauntlet.py --games 20 --movetime-a 200 --movetime-b 40

# Depth handicap gauntlet
python tools/gauntlet.py --games 20 --movetime-a 200 --depth-b 3

# With random openings, save results
python tools/gauntlet.py --games 30 --movetime-a 200 --movetime-b 50 \
    --opening-plies 6 --opening-top-n 4 \
    --output tools/gauntlet_out/results.jsonl
```

---

## Net validation

After training and exporting with `tools/training/export.py`, validate the
binary before using it:

```bash
# Binary checks only (no PyTorch needed)
python tools/validate_net.py --net makruk_net.bin

# Include engine smoke test
python tools/validate_net.py --net makruk_net.bin --engine src/sf-kernel --eval
```

Expected output for a valid float32 export (dims=22528, L1=256, L2=32, L3=32):

```
Header:
  [OK]    Magic     — 0x4D4B4E4E ('MKNN')
  [OK]    Version   — 1
  [OK]    Dims      — 22528
  [OK]    L1/L2/L3  — L1=256  L2=32  L3=32

Size consistency:
  [OK]    File size — 23,139,740 bytes (22.07 MB) — matches architecture

Weights:
  [OK]    NaN check — none found
  [OK]    Inf check — none found
```

---

## Converting and inspecting data

### Convert JSONL to training TSV

```bash
cd tools/training

# All games
python convert.py --input ../selfplay/out/selfplay.jsonl --output train.tsv

# Decisive games only (for mixed training)
python convert.py --input ../selfplay/out/handicap/selfplay.jsonl \
    --output train_decisive.tsv --decisive-only
```

### Dataset summary

```bash
# Game-level stats (fast)
python tools/training/dataset_summary.py \
    --jsonl tools/selfplay/out/selfplay.jsonl

# Full stats including position distribution
python tools/training/dataset_summary.py \
    --jsonl tools/selfplay/out/selfplay.jsonl \
    --tsv   tools/training/train.tsv \
    --duplicates
```

---

## Training recommendations when data is draw-heavy

If you must train on mostly-draw data while building up decisive games:

1. **Increase λ** toward 1.0 to weight score_cp more than wdl:
   ```bash
   python tools/training/train.py --input train.tsv --lam 0.9 --output makruk.pt
   ```
   This de-emphasises the useless draw WDL signal and focuses on the
   centipawn evaluations.

2. **Use longer movetime** when generating data so score_cp scores are
   more accurate and consistent.

3. **Mix datasets**: combine normal self-play with decisive games using
   `cat normal.jsonl decisive.jsonl > mixed.jsonl` before converting.

4. **Do not increase model size or training epochs** until decisive game
   data exists and validation loss drops meaningfully below 0.65.

---

## Checking decisive game rate

After generating data, quickly check the decisive rate:

```bash
python -c "
import json, sys
games = [json.loads(l) for l in open('tools/selfplay/out/selfplay.jsonl') if l.strip()]
decisive = sum(1 for g in games if g['result'] != '1/2-1/2')
print(f'{decisive}/{len(games)} decisive ({100*decisive/len(games):.1f}%)')
"
```

Target: at least 30% decisive for useful WDL signal.
At 50%+ decisive, training should produce a meaningfully stronger net.
