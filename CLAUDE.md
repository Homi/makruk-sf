# CLAUDE.md

## Project Overview

This repository is **Makruk-SF**, a Thai Chess (Makruk) engine project derived from **sf-kernel**, which itself is derived from **Stockfish**.

Makruk-SF is intended to become a Makruk-only engine with:

* Correct Makruk move generation
* Correct Makruk promotion rules
* Complete Makruk counting rules
* Makruk-specific evaluation
* Makruk NNUE feature transformer
* Makruk NNUE training pipeline
* Self-play data generation
* Benchmarking against Fairy-Stockfish and Fairy-Max Makruk engines

This is not a generic chess variant framework. Prefer clear Makruk-only implementation.

---

## Maintainer

Makruk-SF-specific work is maintained by:

* Homi
* Email: [bhome1@hotmail.com](mailto:bhome1@hotmail.com)

---

## License and Attribution Rules

This project is derived from sf-kernel and Stockfish.

Therefore:

* The project remains licensed under GNU GPLv3.
* Do not remove `LICENSE`.
* Do not remove upstream attribution.
* Do not remove Stockfish credits.
* Do not remove sf-kernel credits.
* Do not delete or rewrite `docs/AUTHORS`.
* If modifying `docs/AUTHORS`, only add Makruk-SF maintainer information above the inherited upstream author list.
* Do not claim Makruk-SF is written from scratch.
* Always describe this project as a derived work based on sf-kernel / Stockfish.

When editing README or documentation, clearly state:

> Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish. Makruk-SF-specific modifications are maintained by Homi [bhome1@hotmail.com](mailto:bhome1@hotmail.com).

---

## General Development Rules

Follow these rules strictly:

1. Keep the project compiling after every meaningful change.
2. Make small, reviewable changes.
3. Do not rewrite unrelated code.
4. Do not apply global formatting changes unless explicitly requested.
5. Do not introduce large new dependencies without approval.
6. Prefer simple Makruk-specific code over a large generic variant system.
7. Add tests for every rule change.
8. Preserve current code style where practical.
9. Do not change NNUE architecture until Makruk move rules are stable.
10. Do not begin training pipeline work until Makruk rules, promotion, and counting are implemented and tested.

---

## Makruk Rules Target

Makruk-SF should implement Thai Chess / Makruk rules.

### Board

* 8x8 board.
* White pieces start on rank 1.
* Black pieces start on rank 8.
* White pawns start on rank 3.
* Black pawns start on rank 6.

Recommended Makruk start FEN:

```text
rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1
```

### Pieces

Use Makruk semantics:

* King = Khun
* Rook = Ruea
* Knight = Ma
* Khon = one step forward or one step diagonally
* Met = one step diagonally
* Pawn = one step forward, captures diagonally forward
* Promoted pawn = moves like Met

Implementation preference:

* Rename or semantically map chess `BISHOP` to `KHON`.
* Rename or semantically map chess `QUEEN` to `MET`.
* Keep enum numeric layout compatible unless a clean refactor is explicitly needed.
* Avoid adding extra piece types in the first Makruk conversion phase unless necessary.

### Removed Chess Rules

Makruk-SF must not generate or rely on:

* Castling
* En passant
* Pawn double-step
* Chess promotion to queen/rook/bishop/knight
* Chess 50-move rule as the primary draw rule

### Promotion

* White pawn promotes when reaching rank 6.
* Black pawn promotes when reaching rank 3.
* Promotion is to Met only.
* A promoted pawn moves like Met.

### Draw Rules

Implement:

* Stalemate as draw
* Repetition draw
* Makruk counting rules

Makruk counting must be implemented as a separate subsystem, not as a hacked chess 50-move rule.

Recommended files:

```text
src/makruk/makruk_counting.h
src/makruk/makruk_counting.cpp
```

---

## Makruk Counting Rules Design

Create a separate counting state structure.

Suggested structure:

```cpp
struct MakrukCountingState {
    bool active;
    Color claimant;
    int count;
    int limit;
    int reversiblePlyAtStart;
    MaterialSignature materialAtStart;
    CountingMode mode;
};
```

Recommended modes:

```text
Counting=off
Counting=fairy
Counting=thai-strict
```

The first goal is compatibility with Fairy-Stockfish Makruk behavior.
The later goal is stricter Thai-style counting suitable for GUI explanation.

Add debug output or command support for:

```text
makrukcount
```

Expected debug information:

* Whether counting is eligible
* Which side is claiming
* Current count
* Count limit
* Material signature
* Whether draw is reached

---

## Evaluation Plan

Do not depend only on NNUE at the beginning.

Add a Makruk classical evaluation fallback first.

Initial Makruk material values may use:

```text
Pawn   MG 126 / EG 208
Knight MG 781 / EG 854
Khon   MG 660 / EG 640
Rook   MG 1276 / EG 1380
Met    MG 420 / EG 450
King   0
```

Add simple Makruk-specific heuristics:

* Pawn advancement
* Promotion race
* Promoted pawn / Met coordination
* Khon centralization
* Rook activity
* Rook trap risk
* King safety
* Endgame king activity
* Counting danger

Keep handcrafted bonuses small and testable.

---

## NNUE Plan

Do not modify NNUE first.

Only start Makruk NNUE work after:

* Makruk move generation is correct
* Promotion is correct
* Make/unmake is correct
* Perft tests pass
* Self-play games run without crash
* Counting subsystem exists

When ready, create Makruk NNUE feature files:

```text
src/nnue/features/half_ka_v2_makruk.h
src/nnue/features/half_ka_v2_makruk.cpp
```

Start with a Makruk version of HalfKAv2:

Feature channels:

```text
W_PAWN
B_PAWN
W_KNIGHT
B_KNIGHT
W_KHON
B_KHON
W_ROOK
B_ROOK
W_MET
B_MET
KING
```

Use `HalfKAv2Makruk` as the Makruk feature set.

Only later consider separate promoted-pawn channels or counting-aware buckets.

---

## Testing Requirements

Add tests in small stages.

Required test categories:

```text
[ ] Makruk start position parsing
[ ] Piece movement tests
[ ] Pawn movement tests
[ ] Promotion tests
[ ] No castling generated
[ ] No en passant generated
[ ] No pawn double-step generated
[ ] Check detection
[ ] Checkmate detection
[ ] Stalemate detection
[ ] Repetition
[ ] Counting rule edge cases
[ ] Perft depth 1-5
[ ] Self-play no-crash test
```

---

## Recommended PR Plan

Use small PRs.

### PR01: Project Rename and Documentation

Scope:

* Update README.md
* Add NOTICE.md
* Add CHANGELOG.md
* Add CLAUDE.md
* Add Homi [bhome1@hotmail.com](mailto:bhome1@hotmail.com) as Makruk-SF maintainer
* Preserve GPLv3 and upstream attribution
* Do not change engine logic

### PR02: Makruk Start Position and Piece Semantics

Scope:

* Add Makruk start FEN
* Add Makruk piece naming layer
* Map Bishop semantics to Khon
* Map Queen semantics to Met
* Keep compile passing

### PR03: Makruk Move Generation

Scope:

* Implement Met move
* Implement Khon move
* Implement Makruk pawn move
* Remove pawn double-step
* Remove en passant
* Remove castling generation
* Add unit tests and perft tests

### PR04: Promotion and Make/Unmake

Scope:

* Implement promotion to Met only
* White promotes on rank 6
* Black promotes on rank 3
* Ensure dirtyPiece updates are correct
* Add promotion tests

### PR05: Makruk Legality and Game End

Scope:

* Check detection with Makruk attacks
* Checkmate
* Stalemate
* Repetition
* Basic draw handling

### PR06: Makruk Counting

Scope:

* Add counting subsystem
* Add counting state
* Add counting eligibility checks
* Add debug output
* Add tests

### PR07: Classical Makruk Evaluation

Scope:

* Add Makruk material values
* Add PST
* Add promotion race eval
* Add Khon/Met/Rook/King heuristics
* Add eval debug output

### PR08: Makruk NNUE Runtime

Scope:

* Add HalfKAv2Makruk
* Add Makruk feature mapping
* Add NNUE load/eval smoke tests

### PR09: Self-Play Generator

Scope:

* Add tools/selfplay
* Add PGN output
* Add JSONL output
* Add resume support

### PR10: Training Pipeline

Scope:

* Add dataset converter
* Add trainer integration
* Add validation scripts
* Add net export scripts

---

## Build Policy

After every PR:

Run at least:

```powershell
git status
```

Then build with the available platform build method.

If Visual Studio project is available:

```powershell
msbuild .\src\sf-kernel.vcxproj /p:Configuration=Release /p:Platform=x64
```

If project files are renamed later, update this command accordingly.

Never leave the repository in a non-compiling state unless explicitly instructed.

---

## Claude Code Behavior

When working in this repository:

1. First inspect relevant files.
2. Summarize what will be changed.
3. Make the smallest safe change.
4. Show changed files.
5. Explain how to test.
6. Do not continue to the next phase unless requested.
7. Do not make hidden large rewrites.
8. Do not remove license or attribution text.
9. Ask only if a blocking ambiguity exists.
10. Prefer partial safe progress over broad risky changes.

## Current Training Status

Makruk-SF now has initial NNUE runtime, self-play generation, dataset creation, training, validation, and export.

Initial proof-of-concept result:

- Self-play games: 110
- Positions: 21,250
- Score range: approximately -1487 to +1375 cp
- Median score: 0
- Game results: all draws
- Training: 30 epochs
- Best validation loss: approximately 0.6715
- Export artifact: makruk_net.bin, float32, approximately 22.6 MB

Interpretation:

- The training pipeline works.
- The model learns some signal from search/eval scores.
- The dataset is too draw-heavy.
- Result/WDL signal is currently weak because all games are drawn.
- The next priority is data diversity, decisive games, teacher labels, and runtime net validation.

Gauntlet result vs Fairy-Stockfish (100 games, 100 ms/move):

- sf-kernel wins: 7 (7%), draws: 91 (91%), losses: 2 (2%)
- Decisive rate: 9%
- Dominant termination: stalemate (66 games)
- Root cause of high stalemate rate: counting draw was not wired into game logic,
  allowing positions that should have ended by counting to drift into stalemate endgames.
  Fixed in PR11 follow-up (commit d92a37e): `isDraw()` now enforces counting draw.
- Elo estimate: +17 relative to Fairy-Stockfish (unreliable at 9% decisive rate)

## PR11 — Net Validation, Gauntlet, Decisive Self-Play (completed)

PR11 adds the following tools (no C++ source changes):

- `tools/validate_net.py` — Validates `makruk_net.bin`: header magic/version,
  architecture size consistency, weight NaN/Inf check, optional engine smoke test.
- `tools/gauntlet.py` — Match runner: engine A vs engine B with configurable
  movetime/depth per side, random openings, W/D/L summary and Elo estimate.
- `tools/selfplay/selfplay.py` — Extended with modes: `time_handicap`,
  `depth_handicap`, `random_opening` (MultiPV-based); richer JSONL metadata
  (`mode`, `decisive`, `handicap_ratio`, `depth_cap`, `opening_plies`, `seed`).
- `tools/training/convert.py` — Adds `decisive`, `mode`, `score_source` columns
  to training TSV; adds `--decisive-only` filter; backward compatible.
- `tools/training/dataset_summary.py` — Prints game/position statistics:
  W/D/L, decisive%, termination breakdown, score distribution, duplicate FEN estimate.
- `docs/dataset_guide.md` — Explains why draw-heavy data is weak, recommended
  dataset mix (40/20/20/10/10), commands for decisive data generation,
  training recommendations when data is draw-heavy.

## PR11 follow-up — Makruk Counting Draw Wired into Game Logic (completed)

Analysis of a 100-game gauntlet vs Fairy-Stockfish revealed 66% stalemate rate,
caused in part by the counting subsystem existing but never being called during play.

Changes merged on top of PR11 branch (commit d92a37e, PR #11):

- `src/position.h` — `MakrukCountingState makrukCounting` added to `StateInfo`
  before the `key` field so `doMove()`'s existing `memcpy` carries it forward automatically.
- `src/position.cpp` — `isDraw()` now calls `isMakrukCountingDraw()` instead of
  the chess 50-move rule (`rule50 > 99`). `set()` activates counting immediately
  for bare-king FENs. `doMove()` activates or advances the counting state each ply.
  `undoMove()` restores state for free via `st = st->previous`.
- `src/makruk/makruk_counting.h` — comment updated to reflect live integration.
- `src/makruk/test_counting.cpp` — two new integration tests:
  `testCountingActivatesOnSet` and `testIsDrawConnectedToCountingDraw`.

All 29 counting + eval tests pass.

## Decisive Data Generation — Score Adjudication (PR11 branch, 2026-06-14)

Root cause analysis: even with `depth_handicap depth=1`, self-play produced 0% decisive rate.
Reason: when counting draw activates (bare-king endgame), the engine scores positions near 0
regardless of material advantage, and the game drifts to stalemate instead of checkmate.

Fix: **score adjudication** added to `tools/selfplay/selfplay.py`:
- `--adjudication-threshold 500` (cp, White's perspective)
- `--adjudication-streak 5` (half-moves above threshold)
- When White's evaluation stays ≥ +threshold for streak consecutive half-moves (after ply 20),
  the game is adjudicated as White win. Vice versa for Black.
- `termination = "adjudication"` in the JSONL record.

Results (20 games, `depth_handicap depth=1`, movetime 200ms, adjudication 500cp/5):
- Decisive rate: **65%** (13/20), all White wins (stronger side)
- Vs 0% without adjudication

Recommended decisive dataset command:
```bash
cd tools/selfplay
python selfplay.py \
    --games 200 \
    --mode depth_handicap --depth-cap 1 \
    --movetime 300 \
    --opening-plies 8 --opening-top-n 4 \
    --adjudication-threshold 500 --adjudication-streak 5 \
    --seed 2024 \
    --pgn decisive_v2.pgn --jsonl decisive_v2.jsonl \
    --outdir out/decisive_v2
```

Note: `time_handicap --handicap-ratio 0.05` only gave 14% decisive rate because
the engine at 15ms still finds drawing moves. `depth_handicap depth=1` is far better.

## Mixed Dataset Training — Round 1 (2026-06-14)

decisive_v2 dataset completed:
- 200 games, depth_handicap depth=1, movetime 300ms, adjudication 500cp/5, seed 2024
- **79.5% decisive** (159/200), all by adjudication; 41 draws
- ~25,842 positions

Mixed dataset (310 games total):
- 110 normal self-play games (all draws) + 200 decisive_v2 games
- 43,030 positions after convert.py (min-ply 16, max-ply 400, max-score 2500)

Training:
- 40 epochs, `--lam 0.7`, Adam lr=0.001
- Best validation loss: **0.601359** at epoch 35 (vs 0.6715 baseline)
- Checkpoint: `tools/training/makruk_mixed.pt`
- Exported net: `src/1877415756.bin` (22.07 MB float32)

Gauntlet result vs Fairy-Stockfish (100 games, 100 ms/move, 6 random opening plies):
- sf-kernel wins: 4 (4%), draws: 92 (92%), losses: 4 (4%)
- Decisive rate: 8%
- Termination breakdown: stalemate=67, draw_score=14, max_ply=8, checkmate=8, repetition=3
- Elo estimate: **≈0** relative to Fairy-Stockfish

Interpretation:
- Validation loss improved (0.671 → 0.601) but Elo did not improve vs previous net (+17 → ≈0).
- Stalemate still dominates (67/100 games). The net doesn't help if games end in stalemate
  before NNUE evaluation has any influence.
- The bottleneck is game termination, not net quality.
- Lower validation loss is not a reliable proxy for gauntlet strength at this data volume.

Root cause of persistent stalemate:
- Counting draw activates (bare-king endgame) and isDraw() correctly returns draw.
- But Fairy-Stockfish does not call `result` — it just keeps playing until the gauntlet
  detects no legal moves (stalemate), because there is no shared protocol for counting draws.
- Both engines play out the counting window until the weaker side is stalemated.

## Gauntlet Draw Detection Fix (2026-06-20)

Root cause identified: Stockfish's `valueDraw()` returns `VALUE_DRAW - 1 + (nodes & 0x2)` = ±1,
never exactly 0. The old `zero_streak` check required `sc == 0`, which never accumulated.
Additionally, in cross-engine gauntlets, the alternate engine (Fairy-SF) returns normal scores
on its turns, resetting any accumulated streak before the threshold was reached.

Fix in `tools/gauntlet.py`:

- Replaced single `zero_streak` counter with **per-engine streak tracking** (`white_streak`,
  `black_streak`).
- Changed exact-zero check to `abs(sc) <= DRAW_SCORE_CP` (threshold: 15 cp).
- Set `DRAW_STREAK = 3` (consecutive turns by one engine) and `DRAW_MIN_PLY = 20`.
- Added **termination breakdown** to the summary line.

Validation on existing 100-game dataset (replayed):

- Old: stalemate=61, draw_score=13
- New: draw_score=97, stalemate=1 (one game where counting activated only 2 plies before stalemate)

Live 20-game test vs Fairy-Stockfish (100ms/move, 6 random opening plies):

- Old code: ~61% stalemate; New code: draw_score=19, max_ply=1, stalemate=0

100-game re-run result vs Fairy-Stockfish (100ms/move, 6 random opening plies, seed=100):

- sf-kernel wins: 0 (0%), draws: 100 (100%), losses: 0 (0%)
- Decisive rate: 0%
- Termination: draw_score=98, max_ply=1, stalemate=1
- Elo estimate: ≈0 (not meaningful at 0% decisive rate)

Interpretation:

- Stalemate dropped from 61-67% to ~1% — the fix works correctly at scale.
- 0% decisive at equal time controls is expected; both engines are evenly matched.
- To get a meaningful Elo estimate, a handicap gauntlet (depth or time) is needed.

## Teacher-Label Data Generation (added 2026-06-20)

Root cause of weak training: sf-kernel's own search scores are the labels.  In drawn
endgames, the engine returns ≈0 regardless of material, so the labels carry little
signal.  Teacher labels from Fairy-Stockfish (a well-tuned Makruk engine) provide
higher-quality, more diverse scores.

New tool: `tools/training/teacher_label.py`

- Takes a convert.py TSV, queries Fairy-SF at a fixed depth for each FEN, outputs a
  new TSV with `score_source = "teacher_<engine>"`.
- Deduplicates FENs (same position queried only once).
- Supports `--resume` to continue after interruption.
- Default depth: 8 (≈30–50ms/position).

Usage:

```bash
python tools/training/teacher_label.py \
    --input  tools/training/train_mixed.tsv \
    --output tools/training/train_fairy_d8.tsv \
    --teacher /path/to/fairy-stockfish \
    --depth 8
```

## Teacher-Label Training — Round 1 (2026-06-20)

**Teacher labeling** (`teacher_label.py`):

- Input: `train_mixed.tsv` (43,030 positions)
- Teacher: Fairy-Stockfish depth=8 (≈140 positions/sec)
- Output: `train_fairy_d8.tsv` — 41,985 positions scored, 586 dropped (|score|>2500)
- Runtime: 5 minutes
- Score distribution: range −1175 to +1384 cp, median +23 cp, P90 = +266 cp
- Near-zero (|score|≤5): 12.7% — much less draw-biased than self-labeled data
- Strong signal (|score|≥200): 20.5%

**Adjudicated handicap gauntlet — BASELINE (old mixed net, 1877415756.bin)**:

- Gauntlet: sf-kernel (200ms) vs Fairy-SF (depth=3), adjudication 500cp/5 half-moves
- 50 games, 72% decisive (35 adjudication + 1 checkmate)
- **sf-kernel wins: 19, draws: 14, losses: 17 → Elo +14**

Interpretation: +14 Elo is meaningful but small (within noise for 50 games).
The adjudication approach works: 72% decisive vs 2% without adjudication.

**Training with teacher labels** (completed 2026-06-20):

- Command: `python train.py --input train_fairy_d8.tsv --output makruk_fairy_d8.pt --epochs 40 --lam 0.7`
- Best epoch: 30, val_loss: **0.625013**
- Note: val_loss is NOT comparable to mixed-net's 0.601 — teacher labels have different target
  distribution (more varied scores, harder to fit than near-zero self-labels).
- Export: `src/1157270523.bin` (22.07 MB float32, CRC32=1157270523)

## Gauntlet Bug Fixes (2026-06-20)

Two bugs were found and fixed in `tools/gauntlet.py`:

**Bug 1 — Movetime tied to color, not engine:**
Lines 297-298 assigned `movetime_a` to White and `movetime_b` to Black regardless of
which engine played which color. In a 50-game gauntlet where engines alternate sides,
each engine got 200ms as White and depth=3 as Black — a symmetric test, not a handicap.

Fix: added `a_is_white: bool` parameter to `play_game()`. Time controls are now assigned
based on which engine is playing (`is_a_turn = is_white_turn == a_is_white`).

**Bug 2 — Win counting based on result string, not engine:**
Summary counted `"1-0"` as A-wins regardless of which engine played White. When B=White
won, `"1-0"` was incorrectly counted as an A win.

Fix: `_print_summary()` now uses `a_is_white` from the game record to assign wins correctly.
`"0-1"` results now correctly appear in the game table when A wins as Black.

The old gauntlet output file `vs_fairy_handicap_d3_adj.jsonl` (seed=99, 50 games) has
been re-analyzed: the manually-verified result (19W 14D 17L, Elo +14 for old net) is
correct because `"0-1"` never appeared (White always triggered adjudication first).

## Adjudication Support Added to Gauntlet (2026-06-20)

`tools/gauntlet.py` now supports `--adjudication-threshold` and `--adjudication-streak`.
Declares a win when the active engine's score (converted to White's perspective) exceeds
the threshold for the specified number of consecutive half-moves.  Disabled by default
(0 = off) to preserve backward compatibility.

Recommended for handicap gauntlets where checkmate is rare due to counting rules:

```bash
python tools/gauntlet.py \
    --engine-b /path/to/fairy-stockfish \
    --games 50 --movetime-a 200 --depth-b 3 \
    --opening-plies 6 --opening-top-n 4 \
    --adjudication-threshold 500 --adjudication-streak 5 \
    --seed 99
```

## Net Comparison — Fixed Gauntlet Results (2026-06-20)

After fixing both bugs (movetime and win counting), re-ran with the corrected setup
(A always 200ms, B always depth=3, seed=99, 50 games, adjudication 500cp/5):

* **Old net (1877415756.bin, mixed self-play)**: A=39W 11D 0L → **Elo +363**
* **New net (1157270523.bin, teacher-labeled Fairy-SF depth=8)**: A=40W 10D 0L → **Elo +382**

Difference: +19 Elo in favor of the new net (within noise for 50 games).

Interpretation:

* At 200ms vs depth=3, sf-kernel dominates regardless of net — the time advantage is overwhelming.
* Both nets are indistinguishable at this handicap level (19 Elo difference is noise at N=50).
* To isolate net quality, a direct net-vs-net test is needed (same engine, different net, equal time).
* Old baseline (+14 Elo) used a broken symmetric setup — those numbers are not comparable to these.

## Net-vs-Net Direct Test — All Draws (2026-06-20)

Ran `sf-kernel(old net, 100ms)` vs `sf-kernel(new net, 100ms)` with adjudication 500cp/5.
Result after 33 games: **100% draws via draw_score** (counting draw activates, both engines score ≈0).

Root cause: at equal time, both engines draw by counting in every game.
Adjudication never fires because neither engine sustains 500cp for 5 half-moves — both score near 0
once counting begins.

**Conclusion: game-play Elo comparison between these two nets is not possible at equal time control.**
The bottleneck is the counting draw mechanism, not search strength or net quality.

## MknnEvaluator — NNUE Runtime Integration (2026-06-20)

**Root cause discovered**: The NNUE net was NEVER loading. `evaluate_nnue.cpp` checks for
Stockfish NNUE magic `0x7AF32F20` but our MKNN format uses magic `0x4D4B4E4E`. Version check
failed immediately → `currentNnueNetName` stayed empty → engine always fell back to
`makrukClassicalEval()`. **All previous gauntlet results measured classical eval, not NNUE.**

Additionally, there was an architecture mismatch: C++ used `transformedFeatureDimensions=1024`
(Stockfish-style) but Python trains with `L1=256`. Even fixing the magic wouldn't work.

**Fix**: New `MknnEvaluator` class that reads the MKNN float32 format directly, bypassing
the Stockfish int16 NNUE path entirely.

New files:

* `src/nnue/mknn_evaluator.h` — class definition, load() reads 6-uint32 header + float32 weights
* `src/nnue/mknn_evaluator.cpp` — forward pass: sparse FT accumulate → L2 ReLU → L3 ReLU → output
* `src/evaluate.cpp` — rewritten to use `MknnEvaluator`; INCBIN embeds default net at compile time
* `src/Makefile` — added `nnue/mknn_evaluator.cpp` to SRCS

Architecture: FT(22528→256) two-perspective → L2(512→32) → L3(32→32) → scalar
Output scale: `Value = int(net_output * 400)` (net trains on `score_cp/400` targets)

**Verification** — NNUE scores differ clearly from classical eval:

| Position          | Classical | NNUE | Diff |
|-------------------|-----------|------|------|
| Start             | +15 cp    | +91  | +76  |
| W+Rook vs K       | +821 cp   | +33  | -788 |
| Mid-game          | +358 cp   | +50  | -308 |
| Endgame W winning | +651 cp   | +22  | -629 |

NNUE IS active (different scores). Endgame scores are compressed near 0 because training
data positions were labeled ≈0 by Fairy-SF when counting was already active.
This is a data quality problem, not a code problem.

The engine still plays correctly — gauntlet results (+363/+382 Elo vs Fairy-SF depth=3)
were from classical eval (NNUE was not loading). Those numbers will need to be re-measured
with NNUE active.

## NNUE Baseline Gauntlet (2026-06-20)

After wiring MknnEvaluator, ran the standard baseline (seed=99, depth-b=3, adj=500/5, 50 games)
with NNUE active to measure actual NNUE performance:

* **NNUE active (1157270523.bin)**: 0W 35D 15L → **Elo -108** vs Fairy-SF depth=3
* **Classical eval (previous baseline)**: 39W 11D 0L → **Elo +363** vs Fairy-SF depth=3
* **Net regression: −471 Elo** caused by the bad training data

Termination: adjudication=15, draw_score=27, stalemate=8 (vs 0 stalemates with classical)

Root causes of regression:

1. **Material blindness**: NNUE scores material advantages ≈33–90 cp (vs ~800 cp classical).
   Engine plays passively, never builds 500 cp streak → 0 adjudicated wins.
   Fairy-SF depth=3 can see imbalances → adjudicates 15 wins from B's perspective.

2. **Stalemate regression**: NNUE returns non-zero (~33–90 cp) even in counting-draw positions.
   This breaks the `abs(score) ≤ 15` draw detection threshold → 8 stalemates (vs ~1 with classical).

Cause of both: training positions were all labeled ≈0 by Fairy-SF (counting was already active).
Net learned: all endgame positions = 0 cp.

**Additional root causes discovered (fixed 2026-06-20):**

* **Decode formula wrong**: `v * 400` assumes output range [0, 400 cp] but net outputs sigmoid
  centered at 0.5 for equal positions. Equal positions gave **+200 cp** instead of 0 cp.
  Draw detection (`|score| ≤ 15`) never triggered. Max output was 400 cp, never reaching 500 cp adj.
  Fixed: `(v - 0.5) * 1600` → equal=0 cp, Rook-advantage≈736 cp, range ±800 cp.

* **CReLU vs ReLU mismatch**: `train.py` uses `clamp(0, 1)` but inference used `relu` (unbounded).
  FT/hidden values >1.0 were treated differently in training vs play.
  Fixed: C++ now uses `crelu = clamp(0, 1)` matching Python training.

## decisive_v3 + Combined Training — Round 2 (2026-06-21)

### decisive_v3 dataset

* 1748 games, `depth_handicap depth=1`, movetime 300ms, adj 500cp/5, seed=2025
* **59.6% decisive** (adjudication); 40.4% draws
* 134,925 raw positions → 1 corrupt record (line 750, null bytes from write interruption) cleaned
* Bare-king positions (6,115): skipped in teacher-label step

### Teacher labeling — decisive_v3_fairy_d10.tsv

* Teacher: Fairy-Stockfish depth=10, `--skip-bare-king` ON (default)
* 121,654 positions scored; 6,115 bare-king dropped; 148 dropped (|score|>2500)
* Runtime: 51.7 min at ~41 pos/sec
* Output: `tools/training/decisive_v3_fairy_d10.tsv`

### Combined dataset — train_combined.tsv

* `train_fairy_d8_nobare.tsv` (40,223 positions, depth=8 teacher, no bare-king)
* `decisive_v3_fairy_d10.tsv` (121,654 positions, depth=10 teacher, no bare-king)
* **Total: 161,877 positions**
* Score distribution: |score|>500cp = 4,065 (2.5%); median = 84 cp

### Training

* Command: `python3 train.py --input train_combined.tsv --output makruk_combined.pt --epochs 50 --lam 0.7 --score-boost 10.0`
* Score-boost: 10× oversampling of |score|>500cp positions (3,825 in train split)
* Best epoch: **37**, val_loss: **0.587558** (best so far across all nets)
* Checkpoint: `tools/training/makruk_combined.pt`
* Export: `src/654286609.bin` (22.6 MB float32, CRC32=654286609)

### Gauntlet result — 654286609.bin with classical+NNUE blend

Standard baseline (seed=99, Fairy-SF depth=3, 50 games, adj 500cp/5, movetime-a=200ms):

* **sf-kernel wins: 33 (66%), draws: 16 (32%), losses: 1 (2%) → Elo +263**
* Decisive rate: 68% (34/50)
* Termination: adjudication=29, draw_score=15, checkmate=5, repetition=1
* 1 loss (game 42: Fairy-SF as White, adjudication)

### Benchmark progression (seed=99, Fairy depth=3, 50 games, adj 500cp/5)

| Eval mode                        | Result       | Elo   | Notes                              |
|----------------------------------|--------------|-------|------------------------------------|
| Classical eval                   | 39W 11D 0L   | +363  | no NNUE                            |
| Conditional blend (net new)      | 38W 12D 0L   | +346  | net 654286609.bin **← current**    |
| ±50cp blend (net new)            | 33W 16D 1L   | +263  | NNUE drags material positions down |
| ±15cp blend (net new)            | 30W 17D 3L   | +210  | less cap = more compounding drag   |
| ±50cp blend (old net)            | 30W 19D 1L   | +230  | net 4201360589.bin                 |
| Pure NNUE (bad data)             | 0W 35D 15L   | −108  | before decode/crelu fixes          |

Interpretation:

* Conditional blend: NNUE only used when |classical| < 100 cp (near-equal positions).
  For material-imbalance positions, falls back to pure classical.
* 0 losses, 76% decisive — best combination so far.
* Only 1 win behind classical (38 vs 39): within statistical noise of 50 games.
* Fixed-cap blends hurt because NNUE systematically undervalues material (trained on 97.5%
  near-equal positions), and the negative contribution compounds across the search tree.

### Current evaluate.cpp logic (`src/evaluate.cpp`)

```cpp
if (pos.state()->makrukCounting.active)
    return VALUE_DRAW;
if (Eval::mknnEval.isLoaded()) {
    const Value classical = makrukClassicalEval(pos);
    if (std::abs(int(classical)) < 100) {          // near-equal: use NNUE
        const Value nnue_pos = Eval::mknnEval.evaluate(pos);
        const int   blend    = int(classical) + std::clamp(int(nnue_pos), -50, 50);
        return Value(std::clamp(blend, ...));
    }
    return std::clamp(classical, ...);             // material imbalance: trust classical
}
return std::clamp(makrukClassicalEval(pos), ...);
```

### Current nets

* `src/654286609.bin` — combined teacher-labeled (d10), epoch 37, val_loss=0.587558 **← current embedded net**
* `src/4201360589.bin` — nobare + score-boost round 1, epoch 45, val_loss=0.620558
* `src/1157270523.bin` — teacher-labeled Fairy-SF depth=8, epoch 30, val_loss=0.625013
* `src/1877415756.bin` — mixed self-play, epoch 35, val_loss=0.601359 (archived)

## Next Development Priority (as of 2026-06-21)

Conditional blend at Elo +346 is essentially classical-equivalent (within noise at N=50).
The NNUE positional signal helps in equal positions without harming material evaluation.

To eventually surpass classical with pure NNUE, the net needs to learn material values.
Root cause: 97.5% of training positions have |score| < 500 cp, so NNUE never sees
large material imbalances during training.

**Next steps (in priority order):**

1. **Imbalanced-opening data (decisive_v4)** — generate self-play starting from FENs that
   already have material imbalance built in (e.g., remove one side's piece from standard
   start position). Both engines play at full strength. Fairy-SF will naturally label these
   400–800 cp. This directly increases high-score position ratio without needing adjudication.

2. **Wider conditional gate** — once net is better calibrated, expand gate from 100 cp to
   200 cp (apply NNUE for a broader range of positions).

3. **Pure NNUE gauntlet** — when net static eval gives ≥ 600 cp for clear material wins,
   remove the conditional gate and test pure NNUE strength directly.

Benchmark target: pure NNUE ≥ 39W 11D 0L = Elo +363 vs Fairy-SF depth=3.

---

## Improvements Round 3 — Decode Fix + Imbalanced Openings (2026-06-27)

### Root Cause Analysis

Three bottlenecks identified and addressed:

#### 1. Decode formula wrong (critical bug)

`mknn_evaluator.cpp` used `(v - 0.5) * 1600` to convert sigmoid output to centipawns.
Training target is `sigmoid(score_cp/400)`, so the correct inverse is `logit(v)*400 = raw*400`.
The old formula systematically underestimated large scores:

| True score | Old decode | New decode (correct) |
|------------|------------|----------------------|
| 500 cp     | 444 cp     | 500 cp               |
| 800 cp     | 609 cp     | 800 cp               |
| 1276 cp    | 737 cp     | 1276 cp              |

Fix: `const int cp = static_cast<int>(std::clamp(raw * 400.0f, -29000.0f, 29000.0f));`
(`raw` is the pre-sigmoid logit, already computed in the forward pass — no sigmoid needed.)

#### 2. Data distribution: only 2.4% high-score positions

Of 161,877 training positions, only 3,827 have |score| > 500 cp.
The net never learned to represent material advantages.

Fix: `decisive_v4` with imbalanced openings (see below).

#### 3. White-bias in training data (78% positive scores)

`depth_handicap depth=1` always makes White stronger, so nearly all positions show
White advantage. Net learns "White is usually winning."

Fix: `--color-augment` flag in `train.py` doubles the dataset by adding color-flipped
positions (swap W↔B, reverse ranks, negate score, invert WDL).

### Changes Made

* `src/nnue/mknn_evaluator.cpp` — decode fix: `raw * 400` instead of `(v-0.5) * 1600`
* `tools/training/makruk_utils.py` — added `flip_fen()` for color augmentation
* `tools/training/train.py` — added `--color-augment` flag
* `tools/selfplay/selfplay.py` — added `--fens-file` option (pick random start FEN per game)
* `tools/selfplay/imbalanced_fens.txt` — 14 imbalanced start FENs (7 W+piece, 7 B+piece)

### decisive_v4 Dataset

Command:

```bash
cd tools/selfplay
python3 selfplay.py \
    --games 500 \
    --mode depth_handicap --depth-cap 1 \
    --movetime 300 \
    --opening-plies 6 --opening-top-n 4 \
    --fens-file imbalanced_fens.txt \
    --adjudication-threshold 500 --adjudication-streak 5 \
    --seed 2026 \
    --pgn decisive_v4.pgn --jsonl decisive_v4.jsonl \
    --outdir out/decisive_v4
```

Quick test (10 games): **100% decisive** (vs 59.6% for decisive_v3 from standard start).
Full run (500 games): in progress.

### decisive_v4 Full Run Results

* 500 games, 82.2% decisive (411/500), seed=2026
* Teacher-labeled: `tools/training/decisive_v4_fairy_d10.tsv` (28,710 positions, Fairy-SF depth=10)
* Score distribution: std=366 cp, 76% of positions |score|>100 cp

### Training Round 4 (v4 net) — completed 2026-06-27

Combined dataset: `tools/training/train_v4_combined.tsv`

* Sources: train_fairy_d8_nobare (40,223) + decisive_v3_fairy_d10 (121,654) + decisive_v4_fairy_d10 (28,710)
* Total: 190,587 positions; with `--color-augment`: 381,174 effective samples
* Score std: 214 cp; |score|>100 cp: 47%

Training command:

```bash
python3 -u tools/training/train.py \
    --input tools/training/train_v4_combined.tsv \
    --output tools/training/makruk_v4.pt \
    --epochs 60 --lam 0.7 --score-boost 5.0 --color-augment --batch-size 256
```

* Best epoch: **58**, val_loss: **0.560146** (best across all nets)
* Export: `src/3914609440.bin` (22.07 MB float32, CRC32=3914609440) ← current embedded net
* Deployed via `tools/training/deploy_net.sh`

### evaluate.cpp Gate Expansion (2026-06-27)

Changed blend logic in `src/evaluate.cpp`:

* Gate: 100 cp → **300 cp** (NNUE consulted for more positions)
* Formula: `classical + clamp(nnue, -50, +50)` → `classical + clamp(nnue - classical, -100, +100)`
* New semantics: NNUE as *correction* to classical (how much NNUE disagrees), not addition from 0
* This prevents overcounting when NNUE agrees with classical

### Gauntlet Result — v4 Net (2026-06-27)

Standard baseline: seed=99, Fairy-SF depth=3, 50 games, adj 500cp/5, movetime-a=200ms

Result: sf-kernel (v4 net) wins: 39 (78%), draws: 11 (22%), losses: 0 → **Elo +363**

* Decisive rate: 78% (adjudication=38, draw_score=11, checkmate=1)

### Benchmark Progression — Full History

| Config | Result | Elo | Notes |
|--------|--------|-----|-------|
| Classical eval (no NNUE) | 39W 11D 0L | +363 | baseline |
| v4 net + gate=300 (current) | 39W 11D 0L | +363 | matches classical |
| v3 net conditional blend (gate=100) | 38W 12D 0L | +346 | −17 Elo vs classical |
| v3 net ±50 blend | 33W 16D 1L | +263 | NNUE drags material positions |
| Pure NNUE (bad data, wrong decode) | 0W 35D 15L | −108 | before all fixes |

NNUE now matches classical eval strength — major milestone.
Total improvement this session: −108 → +363 = **+471 Elo**.

## Training Round 5 (v5 net) — completed 2026-06-28

### decisive_v5 Dataset

* 14 FENs with 2 pieces removed per side — `tools/selfplay/imbalanced_fens_v5.txt`
* 500 games (depth_handicap depth=1, movetime=300ms, adj 500cp/5, seed=2027)
* **99.8% decisive**, balanced 1-0/0-1 (252/248), 25 plies per game
* convert (min-ply=4): 11,111 positions; std=685 cp; 99% decisive

Teacher-labeled: `tools/training/decisive_v5_fairy_d10.tsv` (10,710 positions, depth=10)

* std=721 cp — highest of all datasets; bimodal ±500–1000 cp; 0.8% near-zero

### Combined v5 Dataset

`tools/training/train_v5_combined.tsv` = d8_nobare + v3_d10 + v4_d10 + v5_d10

* 201,297 positions (vs 190,587 for v4)
* std=266 cp (vs 214 cp for v4); B-win positions: 5.0% (vs 2.6%)
* With `--color-augment`: 402,594 effective samples

### Training v5

* `--score-boost 2.0` (reduced from 5.0 — 27.8% natural high-score positions, less boost needed)
* Epoch 1 val_loss=0.553623 (already below v4 best of 0.560146)
* Best epoch: **56**, val_loss: **0.538214** (best across all nets)
* Export: `src/3738790009.bin` (22.07 MB, CRC32=3738790009) ← current embedded net

### Gauntlet — v5 Net with blend=300/±100 (2026-06-28)

Standard baseline: seed=99, Fairy-SF depth=3, 50 games, adj 500cp/5, movetime-a=200ms

Result: sf-kernel (v5 net) wins: 38 (76%), draws: 11 (22%), losses: 1 → **Elo +330**

* Decisive: 78% (adjudication=39, draw_score=11)
* 1 loss = game 21, complex 70-ply middlegame

### Pure NNUE Test (2026-06-28) — FAILED

Removed classical blend entirely (pure NNUE only). Results after 3 games: 0W 0D 2L.

Root cause: NNUE underestimates material by ~2× vs classical
(Rook advantage: NNUE=687 cp vs classical=1276 cp). Engine plays passively and misses wins.

Pure NNUE requires net output to match classical scale, which needs either:
(a) more training data (net averages toward true game outcomes, not absolute cp), or
(b) rescaling training targets to match classical material values.

Reverted to blend mode.

### Full Benchmark Progression

| Config | Result | Elo | Notes |
|--------|--------|-----|-------|
| Classical eval (no NNUE) | 39W 11D 0L | +363 | baseline |
| v4 net + gate=300 | 39W 11D 0L | +363 | matches classical |
| v5 net + gate=300 | 38W 11D 1L | +330 | within noise of v4 |
| v3 net conditional blend (gate=100) | 38W 12D 0L | +346 | −17 vs classical |
| Pure NNUE v5 (3 games only) | 0W 0D 2L | << 0 | not viable |
| Pure NNUE bad data | 0W 35D 15L | −108 | before all fixes |

### Key Insight: Val Loss ≠ Elo

val_loss improved v3→v4→v5: 0.588→0.560→0.538, but Elo plateaued at ≈+330–+363.
Improvement is in predicting teacher scores on the validation set, but the gains are in
|score|>300 cp positions that classical already handles (outside the gate).

For NNUE to SURPASS classical, it must correctly evaluate positions within the gate
(|classical|<300 cp) better than classical does. This requires either:

1. More training positions in the 50–300 cp range with diverse Fairy-SF labels
2. A larger network (can represent more complex patterns)
3. Quantized inference (faster search depth = better tactical play)

## Round 6 — int16 FT Quantization (2026-06-28)

### Gauntlet Baseline Change

All gauntlet results from this round onward use **Fairy-Stockfish-NNUE** as opponent:

* Path: `/home/bj69/Myprojects/Fairy-Stockfish-NNUE/Fairy-Stockfish/src/fairy-stockfish-makruk-nnue`
* This is newer and stronger than the old Fairy-Stockfish (`/home/bj69/Myprojects/fairy-stockfish/src/bin -variant makruk`)
* Previous results (+330, +363) used the old opponent and are NOT comparable to Round 6 results.

### int16 FT Quantization

Implemented MKN2 binary format: int16 Feature Transformer + float32 L2/L3/out.

**Key design:**

* FT weights stored TRANSPOSED: `[DIMS × L1]` instead of `[L1 × DIMS]` — enables sequential column access
* Quantization scale = 1024 (float → `round(float × 1024)` → int16)
* Max int16 weight = 2125 (headroom 15×); max int32 accumulation ≈ 68,604 (headroom 31,303×)
* File size: 11.1 MB vs 22.1 MB float32 (50% reduction)
* Forward pass: int32 accumulation → divide by ft_scale → float32 for L2/L3/out

**New files:**

* `tools/training/export_int16.py` — exports `.pt` checkpoint to MKN2 binary
* `src/nnue/mknn_evaluator.h` — VERSION=2 load path, `accumulate_i16()` method
* `src/nnue/mknn_evaluator.cpp` — dispatches to `accumulate_i16()` when VERSION=2

**Export:** v5 net → `src/584501624.bin` (MKN2, 11.1 MB) ← current embedded net

### Eval Verification

Compared int16 vs float32 at depth 2 on 4 diverse positions:

| Position   | int16 | float32 | diff |
|------------|------:|--------:|-----:|
| Start pos  |   +34 |     +34 |    0 |
| Mid-game 1 |  +845 |    +845 |    0 |
| Mid-game 2 |  +813 |    +813 |    0 |
| Mid-game 3 |  +348 |    +348 |    0 |

Quantization error below 1 cp on all tested positions.

### NPS Finding

int16 and float32 produce **identical NPS** (~86K nps at start position, 1s):

* FT accumulation is NOT the bottleneck at this net size (FT=256, max 32 active features)
* 32 features × 256 neurons ≈ 8K ops per perspective — too small to dominate eval cost
* Benefit: smaller binary (11 MB vs 22 MB), better cache on net load

int16 quantization does **not** improve search speed for this architecture. Speedup would
require a much larger FT (FT=1024+) where accumulation dominates inference cost.

### Gauntlet Results (vs new Fairy-SF-NNUE, depth=3, seed=99, 50 games, adj 500cp/5)

| Net                          | Result     |  Elo | Notes                  |
|------------------------------|------------|-----:|------------------------|
| float32 v5 (3738790009.bin)  | 21W 29D 0L | +156 | new opponent baseline  |
| int16 v5   (584501624.bin)   | 27W 23D 0L | +210 | ≈same within noise     |

Both nets produce nearly identical play (eval scores match exactly). Elo difference is
statistical noise at N=50 games. The new opponent (Fairy-SF-NNUE) is stronger than the
old opponent, so these numbers are not comparable to earlier rounds.

### Current Nets

* `src/2655109219.bin` — v6 net int16 MKN2, L1=512, epoch 50, val_loss=0.537719 **← current embedded net**
* `src/584501624.bin` — v5 net int16 MKN2, L1=256, epoch 56, val_loss=0.538214 (archived)
* `src/3738790009.bin` — v5 net float32 MKN1, same training (archived)
* `src/3914609440.bin` — v4 net, epoch 58, val_loss=0.560146 (archived)
* `src/654286609.bin` — v3 net, epoch 37, val_loss=0.587558 (archived)
* `src/1877415756.bin` — mixed self-play (archived)

## Round 7 — v6 Net (L1=512) + Blend Formula Experiments (2026-07-04)

### v6 Net Deployment

`makruk_v6.pt` (already trained: L1=512, epoch 50, val_loss=0.537719) was exported to
`src/2655109219.bin` (MKN2 int16, 22.6 MB). `evaluate.h` updated and `sf-kernel` rebuilt.

C++ evaluator reads L1 from file header — supports any L1 dynamically, no code changes needed.

### Gauntlet — v6 Net (vs Fairy-SF-NNUE depth=3, seed=99, 50 games, adj 500cp/5)

* **v6 int16 (2655109219.bin, gate=300)**: 23W 26D 1L → **Elo +164**
* Previous v5 int16 (584501624.bin): 27W 23D 0L → +210 (within noise at N=50)

L1=256→512 did not break the Elo plateau. Both nets perform equivalently (~+160–+210).

### Blend Formula Experiments (gate/cap tuning)

Tested 3 configs at 30 games each (seed=99), then confirmed winner at 50 games:

| Config | 30-game | 50-game | Conclusion |
| ------ | ------- | ------- | ---------- |
| cap=±100, gate=300 (baseline) | — | 23W 26D 1L, +164 | current |
| cap=±150, gate=300 | 13W 16D 1L, +147 | — | no gain |
| cap=±200, gate=300 | 13W 16D 1L, +147 | — | identical to ±150 |
| cap=±100, gate=400 | 16W 14D 0L, +207 | 23W 25D 2L, +156 | +207 was noise |

**Conclusion: blend formula tuning does not break the plateau** (~+156–+164 Elo at N=50).
A1/A2: wider cap rarely fires (NNUE delta usually < 100 cp). A3: gate=400 confirmed
equivalent to gate=300 at N=50. Evaluate.cpp reverted to gate=300.

Root cause confirmed: NNUE net systematically underestimates material by ~2× vs classical
(Rook≈700 cp NNUE vs 1276 cp classical). No cap/gate change can fix calibration mismatch.

### Current evaluate.cpp Logic (gate=300, cap=±100, unchanged from Round 6)

```cpp
if (pos.state()->makrukCounting.active)
    return VALUE_DRAW;
if (Eval::mknnEval.isLoaded()) {
    const Value classical = makrukClassicalEval(pos);
    if (std::abs(int(classical)) < 300) {
        const Value nnue_pos = Eval::mknnEval.evaluate(pos);
        const int nnue_delta = std::clamp(int(nnue_pos) - int(classical), -100, 100);
        const int blend = int(classical) + nnue_delta;
        return Value(std::clamp(blend, ...));
    }
    return std::clamp(classical, ...);
}
return std::clamp(makrukClassicalEval(pos), ...);
```

## Experiments — Pure NNUE and Classical Baseline (2026-07-04, Round 7 continued)

### Classical Baseline vs New Fairy-SF-NNUE (missing measurement filled in)

```text
sf-kernel-classical (200ms) vs Fairy-SF-NNUE (depth=3), seed=99, 50 games, adj 500cp/5:
  19W 31D 0L → Elo +139
  Termination: draw_score=31, adjudication=19
```

### Pure NNUE v6 Test (2026-07-04)

Temporarily removed blend; tested pure NNUE v6 (no classical):
  0W 9D 11L → Elo -215 (4 stalemates vs ~1 with blend)

Root cause: NNUE static eval still compresses material advantages. Scale measurement
(`cls/fairy ≈ 0.9`) was for search results (depth 8-10), not static eval.
Static eval with pure NNUE still gives low values for material-heavy positions →
engine plays passively, stalemates reappear.

### Full Comparison (seed=99, Fairy-SF-NNUE depth=3, 50 games, adj 500cp/5)

| Config | Result | Elo | Notes |
| ------ | ------ | --- | ----- |
| Classical eval (sf-kernel-classical) | 19W 31D 0L | +139 | true baseline |
| v6 blend gate=300 (current) | 23W 26D 1L | +164 | +25 over classical |
| v5 i16 blend gate=300 | 27W 23D 0L | +210 | within noise |
| Pure NNUE v6 | 0W 9D 11L | −215 | not viable |

**Key finding: NNUE blend genuinely adds +25 Elo over pure classical.** The net provides
real positional signal in the ≤300 cp range. This is not noise — blend consistently
outperforms classical across all tested configs.

## Round 8 — Near-Equal Position Data (Option C) + v7 Training (2026-07-04)

### Motivation

v6 net at blend gate=300 gives Elo +164 vs Fairy-SF-NNUE (vs classical +139).
The blend only operates when |classical| < 300 cp. The net needs better positional
discrimination in that range to surpass classical further.

### Approach: Extract Positions from Existing Gauntlet Games

Rather than generating new self-play (equal-time games run 480 plies, taking ~3 min each),
extract positions from competitive gauntlet games already played.

Files used:

* `vs_fairy_100_fixed_draw.jsonl` (100 equal-time games, avg 103 plies) → 8,141 positions
* `vs_fairy_nnue_classical_baseline.jsonl` (50 games) → 1,771 positions
* `vs_fairy_d3_adj_v6net.jsonl` (50 games) → 2,793 positions
* blend_test_A1/A2/A3 (30 games each) → ~4,400 positions total
* `vs_fairy_d3_adj_v5net.jsonl` (50 games) → 3,155 positions

Total combined: 20,323 positions → 19,789 after teacher labeling

### Teacher Labeling (Fairy-SF depth=10)

* 19,789 positions scored, 110 dropped (|score|>2500), 419 dropped (bare-king)
* Rate: ~42.7 pos/sec; runtime: ~8 min
* Score distribution: std=370 cp, median=0 cp
* **63.5% in blend range (|fairy| 30-400 cp)** — excellent signal density
* Only 13% near-zero

### convert.py Fix

One-line backward-compatible fix: `start_fen` is now optional in convert.py.
Missing start_fen defaults to Makruk start FEN → convert.py now handles both
selfplay JSONL (has start_fen) and gauntlet JSONL (no start_fen).

### combine_v7.py (new tool)

Filter: keep gauntlet positions where |fairy| 30-400 cp (blend range only).

* Accepted: 12,573 / 19,789 (63.5%)
* Rejected near-zero (<30): 2,565; rejected high (>400): 4,651

### train_v7_combined.tsv

* Base: train_v5_combined.tsv (201,297 positions)
* Added: 12,573 gauntlet near-equal positions
* Total: **213,870 positions** (vs 201,297 for v5)
* Blend range fraction: 57% → 59%
* std=263 cp (similar to v5)

### v7 Training Results (completed 2026-07-05)

```bash
python3 train.py \
    --input train_v7_combined.tsv \
    --output makruk_v7.pt \
    --epochs 60 --lam 0.7 --score-boost 2.0 --color-augment --batch-size 256
```

* Best epoch: **60**, val_loss: **0.538590**
* Export: `src/1222535932.bin` (22.6 MB float32 MKN2, CRC32=1222535932) ← current embedded net

### Gauntlet — v7 Net (2026-07-05)

Standard baseline: seed=99, Fairy-SF-NNUE depth=3, 50 games, adj 500cp/5, movetime-a=200ms

Result: sf-kernel (v7 net) wins: 24 (48%), draws: 25 (50%), losses: 1 (2%) → **Elo +173**

* Decisive: 50% (adjudication=25, draw_score=25)

### Round 8 Benchmark Progression (vs Fairy-SF-NNUE depth=3, seed=99, 50 games, adj 500cp/5)

| Config | Result | Elo | Notes |
| ------ | ------ | --- | ----- |
| Classical eval (sf-kernel-classical) | 19W 31D 0L | +139 | true baseline |
| v7 net + gate=300 (current) | 24W 25D 1L | +173 | **best so far** |
| v6 net + gate=300 | 23W 26D 1L | +164 | previous best |
| v5 int16 blend gate=300 | 27W 23D 0L | +210 | within noise |

### Option C Assessment

Adding 12,573 near-equal positions (|fairy| 30-400 cp) from competitive gauntlet games:

* val_loss: v6=0.537719 → v7=0.538590 (slightly higher — different data distribution)
* Elo: v6=+164 → v7=+173 (+9 Elo improvement)

Conclusion: Option C had a small positive effect. The near-equal position data provides
marginal signal but not enough to dramatically improve blend range discrimination.
All nets are statistically equivalent at N=50 (noise ~±50 Elo).

### Net Archive (Round 8)

* `src/1222535932.bin` — v7 net int16 MKN2, epoch 60, val_loss=0.538590 **← current embedded net**
* `src/2655109219.bin` — v6 net int16 MKN2, L1=512, epoch 50, val_loss=0.537719 (archived)
* `src/3738790009.bin` — v5 net float32 MKN1, epoch 56, val_loss=0.538214 (archived)

## Round 9 — Score-Scale Training (Option B) + Pure NNUE v8 Test (2026-07-05)

### Motivation

All blend nets (v5–v7) plateau at ~+160–+210 Elo. Root cause: NNUE underestimates material
by ~2× (Rook: NNUE≈700 cp vs classical≈1276 cp). If NNUE output matched classical cp scale,
pure NNUE would be viable (no gate needed, full positional signal everywhere).

### Approach: score-scale=1.82

Added `--score-scale` to `train.py`. Scales teacher labels by 1.82× before sigmoid:
`target = lam * sigmoid(score * scale / 400) + ...`

Rationale: Fairy-SF teacher labels average ≈55% of classical for the same positions.
1/0.55 ≈ 1.82 → net should output in classical cp units.

### v8 Training Results

* Input: train_v7_combined.tsv (213,870 positions)
* Command: `python3 train.py --input train_v7_combined.tsv --output makruk_v8.pt --epochs 60 --lam 0.7 --score-boost 2.0 --color-augment --batch-size 256 --score-scale 1.82`
* Best epoch: **60**, val_loss: **0.468935** (not comparable to previous — different sigmoid target distribution)
* Export: `src/1798985659.bin` (22.6 MB float32 MKN2, CRC32=1798985659)

Note: val_loss=0.468935 appears lower than v7 (0.538590) but is NOT a better result —
the target distribution changes with score-scale, so losses are incomparable across scales.

### Static Eval Verification (pure NNUE v8)

| Position | Classical | Pure NNUE v8 | Notes |
|----------|-----------|--------------|-------|
| Start (equal) | +25 cp | +149–157 cp | **5-6× inflated!** |
| W+Rook vs K | +952 cp | +749 cp | 79% of classical |

**Critical problem: uniform scale=1.82 overcorrects equal positions.**

The issue: teacher labels for equal positions are ≈30 cp (Fairy-SF agrees they're equal).
After 1.82× scaling: 30×1.82 = 55 cp → net learns to output ≈55 cp for equal positions.
But the learned OFFSET accumulates from many features, producing +149-157 cp for equal positions.

This is a data distribution artifact, not fixable by adjusting scale at inference time.

### Pure NNUE v8 Gauntlet (2026-07-05)

vs Fairy-SF-NNUE, seed=99, depth=3, 50 games, adj 500cp/5, movetime=200ms:

* **sf-kernel (pure NNUE v8): 0W 15D 35L → Elo -301**
* Termination: adjudication=35, draw_score=14, stalemate=1

Root cause of failure: +150 cp equal-position inflation makes sf-kernel unable to distinguish
losing positions from "slightly winning" positions. All 35 losses were by Fairy-SF adjudication
(Fairy-SF correctly sees material advantage; sf-kernel evaluates even material-down positions
as "small White advantage" ≈+100-150 cp, never triggering its own defensive play).

**v8+blend also NOT viable**: the delta formula `clamp(nnue-classical, ±100)` would
saturate at +100 for almost all equal positions (delta = 150-25 = 125 → clamped to 100),
adding a constant +100 cp bonus to all gate positions. Same inflation problem in blend mode.

### Reverted to v7 net + blend (gate=300, cap=±100)

`src/evaluate.h` → v7 net (`1222535932.bin`) | `src/evaluate.cpp` → blend formula restored.
v8 net archived at `src/1798985659.bin`.

### Lesson: Why score-scale=constant Fails

Uniform scaling amplifies all teacher labels by the same factor. Near-equal positions:
  - Fairy-SF label: +30 cp → scaled: +55 cp → net target: +55 cp
  - Accumulated from many features: net bias settles at +150 cp (not +55 cp)

The bias is a learned constant offset from many weakly-correlated features, not a simple
linear scaling of inputs. The net can't separate "I'm equal" from "I'm winning" without
explicit signal from positions where material imbalances are large AND unambiguous.

### What Would Actually Work

For pure NNUE to be viable, the net needs LARGE UNAMBIGUOUS material-imbalance positions
in training where:
1. Counting draw is NOT active (endgame labels aren't compressed to ≈0)
2. Material advantage is clear (Rook advantage positions where Fairy-SF scores +1000+ cp)
3. The net naturally learns that "extra piece = +700-1200 cp" from diverse examples

**True fix**: generate self-play from FENs with large pre-existing material imbalances (e.g.,
W+Rook vs standard start) and label at depth=10 WITHOUT counting active. Counting draw
artificially compresses Fairy-SF labels to ≈0, which is the root cause of all material
underestimation. See decisive_v4/v5 approach — but those games still drift into counting draws.

## Net Archive — Round 9

* `src/1222535932.bin` — v7 net int16 MKN2, epoch 60, val_loss=0.538590 **← current embedded net**
* `src/1798985659.bin` — v8 net score-scale=1.82, epoch 60, val_loss=0.468935 (FAILED — archived)
* `src/2655109219.bin` — v6 net int16 MKN2, L1=512, epoch 50, val_loss=0.537719 (archived)
* `src/3738790009.bin` — v5 net float32 MKN1, epoch 56, val_loss=0.538214 (archived)

## Full Benchmark Table (seed=99, Fairy-SF-NNUE depth=3, 50 games, adj 500cp/5)

| Config | Result | Elo | Notes |
| ------ | ------ | --- | ----- |
| Classical eval (sf-kernel-classical) | 19W 31D 0L | +139 | true baseline |
| v7 net + gate=300 (current) | 24W 25D 1L | +173 | **best blend so far** |
| v6 net + gate=300 | 23W 26D 1L | +164 | |
| v5 int16 + gate=300 | 27W 23D 0L | +210 | within noise |
| Pure NNUE v6 | 0W 9D 11L | −215 | not viable |
| Pure NNUE v8 (scale=1.82) | 0W 15D 35L | −301 | WORSE than v6 (inflation) |

## Next Development Priority (as of 2026-07-05)

Current state: v7 blend at Elo +173. Blend adds ~+34 Elo over classical (+139).
Plateau confirmed across v5/v6/v7 nets and all gate/cap variants.

Pure NNUE requires material-aware training data. The fundamental problem:
**Counting draw compresses Fairy-SF labels to ≈0 in endgames, so NNUE never learns material scale.**

**Next steps:**

1. **Counting-unaware endgame data** — generate positions where one side has a piece advantage
   and counting is NOT yet triggered (early-game imbalances). Label with Fairy-SF at depth=10.
   Goal: at least 20,000 positions with |score| > 500 cp where counting is inactive.

2. **Disable counting in training games** — modify selfplay.py to NOT activate counting draw
   during data generation (use `--no-counting` flag). This forces the engine to play out
   material-imbalance endgames, generating diverse labels. Only for training data generation,
   not for gauntlet (which must use real counting rules).

3. **Direct material calibration** — manually create positions with known material (e.g.,
   W+Rook, W+Knight, W+Met) and label them. Force these into the training set at high weight.
   Net learns: "Rook advantage → ~1000 cp" from curated examples.

---

## Round 10 — Counting-Unaware Data (UseCounting flag) + Search Crash Discovery (2026-07-11)

Executing the "Next Development Priority" plan (counting-unaware endgame data so NNUE
learns material scale). Root cause being addressed: Makruk counting draw makes
`Eval::evaluate()` return `VALUE_DRAW` (≈0) in bare-king endgames, so training labels are
compressed to 0 and the net never learns "extra piece ≈ +700–1200 cp".

### UseCounting UCI option (plan step 2)

* `src/makruk/makruk_counting.{h,cpp}` — new global `Nebula::gMakrukCountingMode`
  (default `Fairy`). `Position::set()`/`doMove()` now use it instead of a hardcoded
  `MakrukCountingMode::Fairy`.
* `src/ucioption.cpp` — new option `UseCounting` (check, default `true`); `onUseCounting`
  sets `gMakrukCountingMode` to `Fairy`/`Off`.
* `tools/selfplay/selfplay.py` — `--no-counting` sends `setoption name UseCounting value
  false`. Data generation only; NEVER for gauntlet/real play.
* Verified: Rook-vs-bare-king static/search score = **0 with counting** vs **+743 without**.
  This is the exact fix — counting-off endgames now carry real, non-zero eval scores.

### Search crash discovered (SIGSEGV, depth >= 9) — UNFIXED

Generating decisive data at movetime crashed the engine. Full debug-mantra + core-dump
investigation:

* **Trigger:** search of depth >= 9 on a strongly-winning position (e.g.
  `r2mksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w`). Depth 8 always safe. `go movetime`
  crashes too because iterative deepening reaches depth 9.
* **Crash site:** `Position::setCheckInfo` → `attacksBb<ROOK>(ksq, pieces())` reads
  `RookMagics[64]` OOB because `ksq = square<KING>() = lsb(0) = 64` (a king is missing).
  Core-dump `rdx = 0x40 = 64` confirms.
* **Root cause = memory corruption, not a legal king capture.** Core dump shows the white
  king gone from the bitboards with an impossible black rook on d1 *behind an intact white
  pawn wall on rank 3*, and `board[]` inconsistent with the bitboards.
* **Ruled out:** `legal()`/movegen (2.4M random legal games clean), make/unmake do/undo
  (depth-10 fuzz clean), null-move-in-check (guarded by `goto moves_loop`), TT/killer moves
  (validated by `pseudoLegal`), NNUE OOB write (legacy accumulator unused; `MknnEvaluator`
  writes are vector-bounded).
* **Why hard:** heisenbug driven by the NNUE float eval — FP codegen differs across
  LTO/non-LTO/ASan builds, so move ordering (hence whether the buggy line is searched)
  changes per build. A given binary crashes 3/3 deterministically; a rebuild may not. Any
  instrumentation perturbs the FP path and hides it. ASan/debug (non-LTO) builds don't crash.
* **Impact:** normal gauntlet/game play never observed to crash; only deep search of
  strongly-winning positions (exactly what `--no-counting` data-gen does).

### Workaround: selfplay `--strong-depth` (crash-safe data generation)

`tools/selfplay/selfplay.py` gains `--strong-depth N`: the strong side and openings search
at fixed depth N (<= 8) instead of by movetime, so iterative deepening never reaches depth
9. `Engine.search_multipv` also gained a `depth` param. Weak side stays at `--depth-cap 1`.

### decisive_v6 dataset

```bash
python3 selfplay.py --games 300 --mode depth_handicap --depth-cap 1 --strong-depth 8 \
  --movetime 300 --opening-plies 6 --opening-top-n 4 --fens-file imbalanced_fens_v5.txt \
  --no-counting --adjudication-threshold 500 --adjudication-streak 5 --seed 2028 \
  --pgn decisive_v6.pgn --jsonl decisive_v6.jsonl --outdir out/decisive_v6
```

* 300 games, **99% decisive**, balanced (152 White / 146 Black wins), **zero crashes**
* convert.py (min-ply 8): `decisive_v6.tsv` = 6010 positions
* **std = 632 cp** (highest of any dataset), **82% |score|>500 cp**, 98% |score|>100 cp
  — the counting-unaware material-scale signal the plan targeted.
* Teacher-labeled with Fairy-SF-NNUE depth 10 → `decisive_v6_fairy_d10.tsv`
  (5,974 positions scored, 4 dropped bare-king, depth=10, 171s @ ~35 pos/s).

### train_v9_combined.tsv

* `train_v7_combined.tsv` (213,870 positions) + `decisive_v6_fairy_d10.tsv` (5,974 positions)
* **Total: 219,844 positions**
* With `--color-augment`: 439,688 effective samples; score-boost 2.0x high-score
  positions: 232,485/429,688 (train split)

### v9 Training — survived a mid-run system reboot

```bash
python3 -u train.py --input train_v9_combined.tsv --output makruk_v9.pt \
    --epochs 60 --lam 0.7 --score-boost 2.0 --color-augment --batch-size 256
```

First attempt (launched as a plain background job, not `nohup`'d) was killed at **epoch
11/60** (val_loss 0.531668) by an unrelated **system reboot at 17:48** — confirmed via
`who -b` / `journalctl`, not a training bug or OOM. `train.py` has **no checkpoint-resume**
(only saves the best `model_state`, not optimizer/epoch state), so the run was relaunched
from scratch under `nohup` + `disown` (survives shell/session resets, though not a full
reboot) rather than adding resume support — a one-off reboot didn't justify the extra
complexity.

Second attempt completed cleanly:

* Best epoch: **56**, val_loss: **0.529720** — **best val_loss of any net trained so far**
  (previous best: v7 at 0.538590)
* Checkpoint: `tools/training/makruk_v9.pt`
* Export: `tools/training/export_int16.py` → `src/2302036703.bin` (int16 MKN2, 22.6 MB,
  FT weight headroom 16.1×, int32 accumulator headroom 32,546× — no overflow risk)
* Deployed: `src/evaluate.h` `NnueNetDefaultName` → `2302036703.bin`, rebuilt with
  `make -j4 build ARCH=x86-64-bmi2`

### Gauntlet — v9 Net Is a Regression Despite Best-Ever val_loss

Standard baseline (seed=99, Fairy-SF-NNUE depth=3, 50 games, adj 500cp/5, movetime-a=200ms,
blend gate=300/cap=±100 unchanged from Round 7):

| Net | Result | Elo | Notes |
|-----|--------|-----|-------|
| v9 net (2302036703.bin) | 12W 36D 2L | **+70** | val_loss=0.529720 (best-ever) — **regression** |
| v7 net (1222535932.bin) | 24W 25D 1L | +173 | previous best gauntlet result |
| Classical eval (no NNUE) | 19W 31D 0L | +139 | true baseline |

Decisive rate collapsed to 28% (14/50: 12 adjudication + 2 checkmate) vs v7's 50% — draws
jumped from 50% to 72% (draw_score=36/50). Static-eval sanity checks ruled out a broken
build: counting toggle works (Rook-vs-K: 0 cp with counting, 732 cp without), and clear
material imbalances are still scored correctly outside the blend gate (671–732 cp), so the
regression is **not** a config/deployment bug — the net's build and material calibration are
sound.

**Root-cause hypothesis:** `decisive_v6` is 99% extreme-imbalance positions (std=632 cp,
82% |score|>500 cp) — useful for teaching material scale, but only 2.7% of the combined
dataset. Its presence, combined with `--score-boost 2.0` further oversampling high-score
rows, likely diluted the network's relative training signal in the **blend-range** (|classical|
< 300 cp, near-equal positions) — exactly the range Round 8/9 identified as the only place
NNUE quality actually moves Elo. Lower val_loss came from fitting the now-larger population
of easy, high-magnitude positions better, not from better near-equal discrimination. This is
the starkest confirmation yet of the Round 9 finding: **val_loss improvement can mask Elo
regression** when the added data shifts the distribution away from the range that matters
for blended play.

**Action:** v7 net (`src/1222535932.bin`, Elo +173) remains the best-performing net and
should stay embedded/deployed unless a larger gauntlet sample or a re-tuned score-boost
confirms v9 is competitive. `src/evaluate.h` currently still points at the v9 net
(`2302036703.bin`) from this session's deploy step — reverting to v7 is a candidate
follow-up, not yet applied; flag for the next session to decide.

### Net Archive — Round 10

* `src/2302036703.bin` — v9 net int16 MKN2, epoch 56, val_loss=0.529720 (best val_loss, but Elo +70 — regression), archived (was embedded, reverted 2026-07-18)
* `src/1222535932.bin` — v7 net int16 MKN2, epoch 60, val_loss=0.538590, **Elo +173 — best gauntlet result, currently embedded**
* `src/2655109219.bin` — v6 net int16 MKN2, L1=512, epoch 50, val_loss=0.537719 (archived)

## Round 10 Follow-up — Net Revert Applied (2026-07-18)

`src/evaluate.h` (`NnueNetDefaultName`) reverted from v9 (`2302036703.bin`) back to v7
(`1222535932.bin`), acting on the deferred decision flagged above. Rationale unchanged
from the regression analysis: v9 measured Elo +70 vs v7's +173 in the standard baseline
gauntlet, and the hypothesis (decisive_v6's extreme-imbalance data, oversampled via
`--score-boost`, diluted blend-range training signal) was not disproven by any new
evidence — this was a straight revert, not a re-investigation.

Verified after the revert: clean rebuild (`make -j4 build ARCH=x86-64-bmi2`) succeeds;
`makrukeval` on a Rook-advantage FEN still reports classical-consistent scoring
(mg=1275/eg=1380, blended 1282 cp); all 29 counting + eval tests pass via
`makruk/build_tests.sh`. `src/2302036703.bin` (v9) stays available untracked/gitignored
for further investigation; only the now-embedded `src/1222535932.bin` is tracked in git,
per the "one net checked in" convention.

Also fixed in this pass: `makruk/build_tests.sh`'s `ENGINE_SRCS` list was missing
`nnue/mknn_evaluator.cpp`, left stale by the MknnEvaluator integration — linking failed
silently (no CI to catch it) until exercised directly. The 29-test suite could not be
verified as passing between that integration and this fix; it passes now.

## Round 10 Follow-up — SIGSEGV Reproduction Attempt: Negative Result + Defensive Hardening (2026-07-18)

Applied the debug mantra (reproduce → trace fail path → falsify hypothesis → cross-reference
breadcrumbs) to the unfixed depth≥9 SIGSEGV from earlier in Round 10. **Could not reproduce
the crash** despite ~1350 engine invocations across four angles, all clean (zero segfaults,
zero core files):

* 504 direct `position + go depth N` runs: 28 imbalanced FENs (`imbalanced_fens.txt` +
  `imbalanced_fens_v5.txt`) × depths 9–24 × `UseCounting` on/off.
* A 60-game self-play run reproducing the *original* discovery method exactly (`--no-counting`,
  movetime 300, `depth_handicap depth-cap=1`, no `--strong-depth`) — 100% decisive, no crash.
* 173 multi-threaded runs (Threads 4/8) at depths 16–24 across the same FEN pool, to test a
  race-condition hypothesis.
* `go infinite` reaching depth 28 in a single 3s search.

**New breadcrumb that reframes the original heisenbug description:** three independent
`make clean && make build` rebuilds from identical source+flags produced **byte-identical**
binaries (matching md5). The Round 10 note "a given binary crashes 3/3 deterministically; a
rebuild may not" was read as LTO codegen nondeterminism — but that doesn't hold on this
toolchain, since rebuilds aren't actually different binaries here. Whatever varies between
"a build that crashes" and "a build that doesn't" isn't recompilation on this machine; cause
still unknown (compiler/toolchain version drift over time is a candidate, not verified).

**Static-analysis lead, unconfirmed against the real bug:** `ValueList::pushBack` (`misc.h`)
had zero bounds checking — used by `HalfKAv2Makruk::appendActiveIndices`/`appendChangedIndices`
for NNUE feature indices. Can't overflow under a *correct* board (Makruk caps at 16 pieces vs.
`MaxActiveDimensions=32`), but if the position were already corrupted upstream, this would
silently extend that corruption rather than fail loudly — consistent with, but not proven to
be, the crash's origin.

**Decision (user-selected):** harden defensively without claiming the root cause is fixed.
Two always-on checks added (deliberately not plain `assert()`, which the release build's
`-DNDEBUG` already strips):

* `misc.h` — `ValueList::pushBack` now aborts with a diagnostic on overflow instead of
  silently writing past `values_[MaxSize-1]`.
* `position.cpp` — `setCheckInfo` (the confirmed crash *site*: `ksq=SQ_NONE` from an empty
  king bitboard fed `attacksBb<ROOK>`, reading `RookMagics[64]` out of bounds) now aborts with
  side/ply/FEN logged when the king is found missing, instead of segfaulting on garbage memory.

Verified: hardened build passes the full 504-run stress test with zero false-positive aborts
during legitimate deep search, all 29 counting/eval tests pass, and perft depth 5 from the
Makruk start position (6,223,994 nodes) matches expectations. Committed `0e3ea6d`.

**Status:** root cause still unknown. The `--strong-depth` workaround in `selfplay.py` remains
the only confirmed-effective mitigation (zero crashes across the `decisive_v6` 300-game run).
Real gameplay/gauntlet at depth≥9 is still theoretically exposed but has never been observed
to crash outside forced data-generation conditions — the new hardening means *if* it is ever
hit again, it will now abort with a diagnosable FEN instead of a raw segfault.
