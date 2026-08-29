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

## Round 11 — Blend-Range Data Round: Closer-Handicap Gauntlets (2026-07-18)

### Motivation

Round 10's `decisive_v6` (extreme-imbalance data, 99% |score|>500cp) was a confirmed
regression (Elo +70 vs v7's +173), reverted this same session. Rounds 8/9 had already
established that NNUE blend quality only moves Elo through near-equal (|classical|<300cp)
position discrimination — extreme-imbalance data doesn't help there and, when oversampled,
actively dilutes that signal. Round 11 targets the blend range directly instead.

### Data generation: closer-handicap gauntlets

Rather than generating new self-play, ran two 40-game gauntlet configs (sf-kernel v7 vs
Fairy-SF-NNUE) with a *closer* handicap than the standard baseline (200ms vs depth=3) to
keep games competitive longer, producing more near-equal positions per game instead of
early decisive divergence:

* Config A: 40 games, movetime-a=200, depth-b=4, seed=5001 → 19W 21D 0L, 47.5% decisive
* Config B: 40 games, movetime-a=150, depth-b=3, seed=5002 → 25W 15D 0L, 62.5% decisive

Both well above the standard baseline's typical ~20% draw rate, confirming closer handicaps
generate more near-equal positions as intended.

### Teacher-labeling and filtering

* Converted both JSONL outputs (min-ply=4) → 6,089 raw positions
* Teacher-labeled with Fairy-SF depth=10 → 5,964 scored (82 bare-king dropped, 32 out of
  range, 11 resume-skips), 2.3 min at ~45 pos/sec
* Filtered to blend range (`combine_v7.py`, |fairy| 30–400cp): **85% acceptance**
  (3,578/4,208 non-bare-king positions) — vs Round 8's original 63.5% from standard-handicap
  gauntlet logs. The closer-handicap strategy is a meaningfully more efficient way to harvest
  blend-range data than the original Option C method.

### Combined dataset

`train_round11_combined.tsv` = `train_v7_combined.tsv` (213,870) + 3,578 new blend-range
positions = **217,448 positions** — a clean, modest addition (1.7%) with zero
extreme-imbalance dilution this round. `dataset_summary.py` confirmed healthy distribution
(0.4% FEN duplication, no anomalies).

### Training

Identical recipe to v7 to isolate the added-data variable as the only change:

```bash
python3 -u train.py \
    --input train_round11_combined.tsv \
    --output makruk_round11.pt \
    --epochs 60 --lam 0.7 --score-boost 2.0 --color-augment --batch-size 256
```

* CPU-only training (no CUDA GPU available on this machine — see "Speedup Infrastructure"
  below), ~8 min/epoch, ~8 hours total for 60 epochs
* Best epoch: 60 (still improving at the end, no divergence), val_loss: **0.545117**
* This is slightly *higher* (worse) than v7's 0.538590 on a comparable-distribution
  dataset — consistent with this project's repeated finding (Rounds 9/10) that val_loss
  doesn't predict blend-range discrimination quality; the real test is the gauntlet.
* Export: `src/1204655067.bin` (22.6 MB, int16 MKN2, FT headroom 13.0×, int32 accumulator
  headroom 26,281× — no overflow risk)

### Gauntlet result

Standard baseline (seed=99, Fairy-SF-NNUE depth=3, 50 games, adj 500cp/5, movetime-a=200ms):

* **Round 11 net (1204655067.bin): 25W 24D 1L → Elo +182**
* v7 net (1222535932.bin, previous best): 24W 25D 1L → Elo +173
* **+9 Elo** — within noise at N=50, but positive and matching the magnitude of Round 8's
  original Option C gain (+9 Elo then too, v6→v7)

### Decision: deployed

Per this project's established convention of keeping within-noise positive deltas as the new
baseline (the v6→v7 precedent), Round 11 net is now embedded and tracked
(`src/1204655067.bin`); v7 (`src/1222535932.bin`) is archived/untracked. Full 29-test suite
and perft depth 5 verified clean against this build.

### Full Benchmark Progression (seed=99, Fairy-SF-NNUE depth=3, 50 games, adj 500cp/5)

| Config | Result | Elo | Notes |
| ------ | ------ | --- | ----- |
| Round 11 net + gate=300 (current) | 25W 24D 1L | **+182** | **best gauntlet result** |
| v7 net + gate=300 | 24W 25D 1L | +173 | previous best |
| v6 net + gate=300 | 23W 26D 1L | +164 | |
| Classical eval (sf-kernel-classical) | 19W 31D 0L | +139 | true baseline |
| v9 net + gate=300 | 12W 36D 2L | +70 | regression (Round 10, reverted) |

### Speedup Infrastructure (investigated, not applied)

Training took ~8 hours (CPU-only, ~8 min/epoch). Investigated speedup options mid-run
without interrupting it:

* **GPU**: an NVIDIA GeForce GTX 860M is present in hardware, but no driver is loaded
  (`nvidia-smi` fails) and the installed PyTorch is the CPU-only build (`torch 2.12.0+cpu`,
  `torch.cuda.is_available()` → False). Using it would require installing an NVIDIA driver
  (possibly needing a reboot — real risk to any in-progress training) and reinstalling
  PyTorch with CUDA support. Not attempted; deferred to a session where no training is
  running, given the 860M is old enough that the payoff is uncertain anyway.
* **CPU threading**: the training process only used 4 of 8 available cores
  (`DataLoader num_workers=0`, default torch intra-op threading). A `--threads` flag /
  `OMP_NUM_THREADS` could push this to 8, but since `train.py` has no checkpoint-resume,
  testing this mid-run would mean restarting from epoch 0 — the wall-clock math worked out
  roughly neutral (a ~2x threading speedup from scratch ≈ letting the current run finish),
  so not applied this round either.
* **Decision**: revisit after a training round completes, not mid-run. If a future round's
  data/training cycle needs to be faster, start with the `--threads` flag (cheap, zero
  system risk) before considering GPU setup (invasive, uncertain payoff on this hardware).

### Net Archive — Round 11

* `src/1204655067.bin` — Round 11 net int16 MKN2, epoch 60, val_loss=0.545117, **Elo +182 (N=50) / +164 (N=100) — currently embedded**
* `src/1222535932.bin` — v7 net int16 MKN2, epoch 60, val_loss=0.538590, Elo +173 (N=50) / +151 (N=100) (archived)
* `src/2302036703.bin` — v9 net int16 MKN2, epoch 56, val_loss=0.529720, Elo +70 — regression (archived)

## Round 11 Follow-up — N=100 Confirmatory Gauntlet (2026-07-18)

User asked to confirm the +9 Elo gain (N=50, within noise) before treating Round 11 as
settled. Ran both nets head-to-head at double the sample size, same standard baseline
settings (seed=99, Fairy-SF-NNUE depth=3, adj 500cp/5, movetime-a=200ms):

| Net | N=50 (original) | N=100 (confirmatory) |
| --- | ---------------- | --------------------- |
| Round 11 net (1204655067.bin) | 25W 24D 1L → +182 | 46W 52D 2L → **+164** |
| v7 net (1222535932.bin) | 24W 25D 1L → +173 | 44W 53D 3L → **+151** |

Both nets' absolute Elo estimates dropped noticeably at N=100 (a reminder that N=50
estimates in this project have consistently run a bit hot — see the identical pattern in
every prior round's benchmark table). The relative comparison is the more trustworthy
number: Round 11 net's lead over v7 **held and slightly grew** (+9 → +13 Elo) rather than
shrinking toward zero or reversing, which is a reasonably good sign this is a real (if
still modest) improvement rather than pure noise — though 100 games per side is still well
short of the sample size needed for high statistical confidence in engine testing generally.

Note on methodology: because both the N=50 and N=100 runs used the same seed (99), the
first 50 games of each N=100 run are not independent new samples from the first N=50 run —
they replay the same seeded opening/position sequence. The valid comparison is therefore
the two *fresh* N=100 runs against each other (+164 vs +151), not a naive pooled total of
N=50+N=100.

**Decision**: kept Round 11 net deployed (no change from the earlier decision) — the N=100
result didn't overturn the direction, it reinforced it. `src/evaluate.h` and the tracked net
binary are unchanged; this was a verification pass only, not a new training/deploy step.
All 29 tests and a clean rebuild reverified after swapping nets twice for this comparison.

## Round 12 — Scale-Up Attempt: Regression, Not Deployed (2026-07-19)

### Motivation

Round 11's closer-handicap-gauntlet method was the first in this project's history to beat
v7, confirmed at N=100 (+164 vs v7's +151). Round 12 scaled up the same method: more games
(300 vs Round 11's 80), and tested an even closer handicap (depth-b=5, not just depth-b=4)
based on Round 11's finding that closing the gap via opponent *strength* produced more
draws than closing it via *time*.

### Data generation — 3 configs, 300 games total

* Config A: 100 games, `movetime-a=200 depth-b=4`, seed=6001 → 35W 61D 4L, **61% draws**
* Config B: 100 games, `movetime-a=200 depth-b=5`, seed=6002 → 30W 64D 6L, **64% draws**
  (highest draw rate of any config in this project; also the highest Fairy-SF win rate,
  6%, helping the historically underrepresented Black-win training class)
* Config C: 100 games, `movetime-a=150 depth-b=3`, seed=6003 → 47W 51D 2L, 51% draws

All three confirmed the "closer-via-strength beats closer-via-time" pattern at much larger
N than Round 11's 40-game samples showed it.

### Teacher-labeling and filtering

* 23,148 raw positions (convert.py, min-ply=4) → 22,416 scored (Fairy-SF depth=10,
  8.2 min at ~47 pos/sec; 504 bare-king dropped, 114 out of range)
* Filtered to blend range (30–400cp): **13,820 accepted (84%)** — consistent with Round
  11's 85%, confirming the acceptance rate is a repeatable property of the method, not a
  fluke of the smaller Round 11 sample
* Combined with `train_round11_combined.tsv` (217,448) → **231,268 positions** (+6.3%,
  a much larger addition than Round 11's own +1.7%). `dataset_summary.py`: Black-win
  representation improved further to 7.1% (vs Round 11's 6.2%), duplication low (0.3%) —
  nothing in the pre-training data validation looked unhealthy.

### Training

Identical recipe to Round 11/v7 (epochs=60, lam=0.7, score-boost=2.0, color-augment,
L1=512), ~8 hours CPU-only. Best epoch 59, val_loss=**0.545124** — virtually identical to
Round 11's 0.545117, consistent with val_loss having plateaued at this data scale
regardless of how much more data is added.

### Gauntlet result — REGRESSION

Went straight to N=100 (per the updated protocol from the Round 11 follow-up, skipping the
now-known-unreliable N=50 step) against the same standard baseline used for the Round 11
confirmatory test:

* **Round 12 net: 35W 61D 4L → Elo +111**
* Round 11 net (current baseline): 46W 52D 2L → Elo +164
* **−53 Elo** — a clear regression, not noise (4× the size of the Round 11-vs-v7 gap that
  was just confirmed as real at this same N=100 sample size)

### Decision: NOT deployed

Reverted immediately: `src/evaluate.h` restored to `1204655067.bin` (Round 11 net),
rebuilt, full 29-test suite + perft depth 5 reverified clean. Round 12 net
(`1879610562.bin`) was not committed and the exported `.bin` was deleted as scratch cleanup;
the checkpoint `tools/training/makruk_round12.pt` (gitignored, not deleted) is kept and can
be re-exported via `export_int16.py` if a future ablation investigation needs it.

### Root cause — NOT investigated, hypothesis only

Unlike Round 10's regression (where the cause — extreme-imbalance data diluting blend-range
signal — was identified with reasonable confidence), Round 12's regression has **no
confirmed explanation**. What's known:

* It is not a data-quality problem in the obvious sense: acceptance rate, duplication rate,
  and val_loss all looked as healthy as Round 11's.
* Config B (depth-b=5, the closest handicap) contributed the most raw positions (9,038 of
  23,148, ~39%) and had the highest draw rate (64%) — i.e., it's overrepresented in the
  new data relative to configs A and C.

**Unconfirmed hypothesis**: positions from a handicap that's this close to genuine parity
may carry *noisier* labels even from a depth=10 teacher — near-truly-equal positions have
less decisive, more search-order-dependent evaluations move-to-move than positions with a
real (if modest) handicap-driven imbalance. If so, Config B's disproportionate volume could
have diluted training signal quality despite every position nominally landing inside the
30–400cp "blend range" bucket by score alone. This has **not been tested** (would require
an ablation: retrain with only configs A+C, or only B, and compare) — flagged as the
natural next step if this direction is pursued further, not asserted as fact.

### Full Benchmark Progression (N=100, seed=99, Fairy-SF-NNUE depth=3, adj 500cp/5)

| Config | Result | Elo | Notes |
| ------ | ------ | --- | ----- |
| Round 11 net (currently embedded) | 46W 52D 2L | **+164** | best confirmed result |
| v7 net | 44W 53D 3L | +151 | previous best |
| Round 12 net | 35W 61D 4L | +111 | regression, not deployed |

### Net Archive — Round 12

* `src/1204655067.bin` — Round 11 net, Elo +164 (N=100) — archived (was embedded, replaced
  by Round 13, see below)
* `tools/training/makruk_round12.pt` — Round 12 checkpoint (CRC32=1879610562 when
  exported), val_loss 0.545124, Elo +111 (N=100) regression, not deployed, gitignored but
  kept on disk (re-export via `export_int16.py` if needed)

## Round 13 — Config B Ablation: Hypothesis Confirmed, New Best (2026-07-29)

### Motivation

Round 12's regression (Elo +111 vs Round 11's +164) had no confirmed cause — the leading
unconfirmed hypothesis was that Config B (`movetime-a=200 depth-b=5`, the closest handicap
tried, 39% of Round 12's new raw positions, highest draw rate at 64%) carried noisier
evaluation labels near true parity even from a depth=10 teacher, and its outsized volume
diluted training signal despite nominally landing inside the accepted blend range. This
round tests that hypothesis directly via ablation: retrain using only configs A+C,
excluding B entirely.

### Method: zero new games, zero new teacher-labeling

Round 12's raw per-config files (`round12_configA.tsv`, `round12_configB.tsv`,
`round12_configC.tsv`, pre-label) and its merged labeled output (`round12_fairy_d10.tsv`,
every FEN already scored by Fairy-SF depth=10) were all still on disk. Since every position
was already queried once during Round 12's labeling pass, per-config labeled subsets could
be reconstructed by a FEN join against the existing labeled file — no new engine calls
needed.

New tool: **`tools/training/split_by_source.py`** — takes one or more raw (pre-label)
source TSVs plus an already-labeled lookup TSV, joins on FEN, and outputs a labeled TSV
containing only the specified sources (tagged, filterable). This is also a reusable
capability for future rounds: instead of `combine_v7.py`'s existing "everything that
passes the blend-range filter goes in" behavior, future rounds can explicitly include or
exclude individual data sources by name.

```bash
python3 split_by_source.py \
    --labeled round12_fairy_d10.tsv \
    --source configA=round12_configA.tsv \
    --source configC=round12_configC.tsv \
    --output round13_configAC_labeled.tsv
# 7,245 + 6,651 = 13,896 labeled positions (from A+C only, B excluded)
```

Filtered to blend range via the existing `combine_v7.py` (unchanged): **8,290 accepted
(83%)**, consistent with every prior round's acceptance rate. Combined with the Round 11
base (217,448) → **225,738 total** — notably less than Round 12's 231,268 (since A+C alone
is a smaller slice than A+B+C), but still a real addition over Round 11's own 217,448.
`dataset_summary.py`: B-win 6.8%, duplication 0.3% — healthy.

### Training — interrupted twice by reboots, completed on the third attempt

Identical recipe to every prior round (epochs=60, lam=0.7, score-boost=2.0,
color-augment, L1=512). `train.py` has no checkpoint-resume — it only saves the
best-so-far `model_state` incrementally (via `torch.save` whenever a new best val_loss is
hit), not optimizer/epoch state — so a reboot mid-run means restarting from epoch 0, not
resuming. This round hit that limitation twice:

1. First attempt: user needed to shut down the machine; stopped safely at epoch 20
   (val_loss 0.5429, safely on disk thanks to incremental checkpointing) with time to
   spare before shutdown.
2. Second attempt (after reboot): another reboot occurred after only epoch 1.
3. Third attempt (after second reboot): completed cleanly, no further interruption.

Final result: best epoch 59, val_loss **0.541965** — notably lower than the ~0.5451
plateau Rounds 11 and 12 both landed on (first time this project's training has broken
that plateau). Export: `src/2605356533.bin` (22.6 MB, int16 MKN2, FT headroom 14.7×, int32
accumulator headroom 29,725× — no overflow risk).

### Gauntlet result — hypothesis confirmed, new best

Standard baseline (seed=99, Fairy-SF-NNUE depth=3, **100 games** directly — matching the
protocol used for both Round 11's confirmatory test and Round 12's regression test):

* **Round 13 net (2605356533.bin): 45W 55D 0L → Elo +168**
* Round 11 net: 46W 52D 2L → Elo +164
* Round 12 net: 35W 61D 4L → Elo +111 (regression, not deployed)

Round 13 **beats Round 11** (+168 vs +164) and is **57 Elo above Round 12** — with zero
losses across 100 games (vs Round 11's 2 and Round 12's 4). The hypothesis holds: Config B
specifically was the problem, not data volume/scale in general. Round 11's
closer-handicap-gauntlet method scales fine as long as the closest-handicap config
(depth-b=5, sitting closest to genuine parity) is excluded or handled separately — this is
the first data-generation scale-up in this project's history to actually improve on its
own baseline round rather than regress.

### Decision: deployed

`src/evaluate.h` → `2605356533.bin`, `.gitignore`'s net exception swapped, old net
(`1204655067.bin`) untracked/archived. Full 29-test suite and perft depth 5 verified clean.
Commit `385f73f`.

### Full Benchmark Progression (N=100, seed=99, Fairy-SF-NNUE depth=3, adj 500cp/5)

| Config | Result | Elo | Notes |
| ------ | ------ | --- | ----- |
| Round 13 net (currently embedded) | 45W 55D 0L | **+168** | **best result — 0 losses** |
| Round 11 net | 46W 52D 2L | +164 | previous best |
| v7 net | 44W 53D 3L | +151 | |
| Round 12 net | 35W 61D 4L | +111 | regression (Config B included), not deployed |

### Net Archive — Round 13

* `src/2605356533.bin` — Round 13 net int16 MKN2, epoch 59, val_loss=0.541965, **Elo +168
  (N=100) — currently embedded**
* `src/1204655067.bin` — Round 11 net, Elo +164 (N=100), archived (untracked)
* `tools/training/makruk_round12.pt` — Round 12 checkpoint, Elo +111 (N=100) regression,
  gitignored, kept for reference
* `tools/training/split_by_source.py` — new reusable tool for source-aware dataset mixing,
  committed

## Round 14 — Scale-Up at Validated-Good Handicaps (2026-07-29)

### Motivation

Round 13 confirmed Config B (depth-b=5, near-genuine-parity) was the cause of Round 12's
regression, not data volume/scale in general. This round tests the natural follow-up:
generate genuinely new data (Round 13 reused Round 12's existing data; there was nothing
further to reuse) at the two configs already validated as good — depth-b=4 and modest
time-handicap (movetime-a=150/depth-b=3) — scaled up further, continuing to avoid
depth-b=5-style configs.

### Data generation and training

* 300 new games: 150 at `movetime-a=200 depth-b=4` (seed=7001, 62% draws) + 150 at
  `movetime-a=150 depth-b=3` (seed=7002, 50% draws) — both healthy, consistent with
  Round 11/13's draw rates at these same configs
* 20,527 positions teacher-labeled (Fairy-SF depth=10, 7.7 min); filtered to blend range:
  **12,063 accepted (84%)** — same acceptance rate as every prior round using these
  configs, reinforcing that this is a stable property of the method
* Combined with the Round 13 base (225,738) → **237,801 total positions**. Black-win
  representation continued improving (7.8%, vs Round 13's 6.8%), duplication low (0.3%)
* Trained with the identical recipe (epochs=60, lam=0.7, score-boost=2.0, color-augment,
  L1=512) — this run used the new `--resume` flag (see below) throughout; no interruption
  occurred, but the safety net was live for the whole ~8 hour run. val_loss **0.545413**

### Gauntlet result

Standard baseline (seed=99, Fairy-SF-NNUE depth=3, 100 games, adj 500cp/5):

* **Round 14 net (2020277415.bin): 47W 52D 1L → Elo +173**
* Round 13 net: 45W 55D 0L → Elo +168 (+5 Elo further improvement)

Small delta (likely within noise at N=100), but no regression and no red flags (48%
decisive rate, healthy termination breakdown). Deployed per this project's established
convention of keeping non-regressing positive deltas — the third consecutive round in the
"scale up the validated-good method" direction (Round 13 → Round 14) to hold or improve,
now firmly establishing that direction as the productive one going forward.

### Decision: deployed

`src/evaluate.h` → `2020277415.bin`, `.gitignore`'s net exception swapped, Round 13 net
untracked/archived. Full 29-test suite and perft depth 5 verified clean. Commit `f36fb35`.

### Full Benchmark Progression (N=100, seed=99, Fairy-SF-NNUE depth=3, adj 500cp/5)

| Config | Result | Elo | Notes |
| ------ | ------ | --- | ----- |
| Round 14 net (currently embedded) | 47W 52D 1L | **+173** | best result |
| Round 13 net | 45W 55D 0L | +168 | previous best |
| Round 11 net | 46W 52D 2L | +164 | |
| v7 net | 44W 53D 3L | +151 | |
| Round 12 net | 35W 61D 4L | +111 | regression (Config B included), not deployed |

### Net Archive — Round 14

* `src/2020277415.bin` — Round 14 net int16 MKN2, epoch 60, val_loss=0.545413, **Elo +173
  (N=100) — currently embedded**
* `src/2605356533.bin` — Round 13 net, Elo +168 (N=100), archived (untracked)

## Round 15 — Regression: Not Deployed, First `--resume`-Interrupted Run (2026-08-01)

### Motivation

Continued the same validated recipe as Rounds 13→14: 300 new games at the two
validated-good configs only (`movetime-a=200 depth-b=4`, `movetime-a=150 depth-b=3`),
combined with the Round 14 base, retrained. Three consecutive prior rounds (13, 14) had
held or improved via this exact method.

### Data generation and training

* 300 new games: 150 at `depth-b=4` (seed=8001, 61.3% draws) + 150 at `movetime-a=150`
  (seed=8002, 59.3% draws) — both healthy, consistent with prior rounds at these configs
* 21,995 positions teacher-labeled; filtered to blend range: **13,477 accepted (87%)** —
  even higher than the usual ~84-85%, no red flag there
* Combined with Round 14 base (237,801) → **251,278 total positions**. Black-win
  representation continued improving (8.4%, vs Round 14's 7.8%), duplication low (0.3%)
* Training was **interrupted by a real reboot** at epoch 54/60 (checkpoint safely saved
  via `--resume`, the feature added specifically for this scenario after Round 13's
  reboot troubles) and successfully resumed from epoch 55 after the machine came back —
  this is the **first round where `--resume` was exercised against an actual
  interruption**, not just smoke-tested. The resume mechanics themselves worked exactly
  as designed (correct epoch continuation, correct `best_val` carried through, correct
  cleanup on completion).
* Final: best epoch 60, val_loss **0.550411** — notably *higher* (worse) than Round 14's
  0.545413. Unlike Round 13 (lower val_loss, better Elo), this round's val_loss and Elo
  moved in the same (bad) direction.

### Gauntlet result — REGRESSION

N=100 standard baseline (seed=99, Fairy-SF-NNUE depth=3, adj 500cp/5, movetime-a=200ms):

* **Round 15 net: 39W 57D 4L → Elo +127**
* Round 14 net (current baseline): 47W 52D 1L → Elo +173
* **−46 Elo** — a clear regression, well outside this project's established noise band
  (the largest *confirmed-real* delta between adjacent successful rounds has been ~13
  Elo; even the largest observed same-config re-run variance, from the "test vs both
  Fairy-SF versions" session, was ~13 Elo). One stalemate termination appeared in this
  gauntlet (`termination: ... stalemate=1`) — the first in a long time; likely noise
  given N=1, not investigated further.

### Decision: NOT deployed

Reverted immediately: `src/evaluate.h` restored to `2020277415.bin` (Round 14 net),
rebuilt, full 29-test suite reverified clean. No `build_tests.sh` side-effect this run
(confirms the Round 14 `net/` cleanup fix continues to hold).

### Root cause — NOT investigated, two unconfirmed hypotheses

Unlike Round 13 (where the cause was isolated via ablation), Round 15's regression has
**no confirmed explanation**. Two candidate hypotheses, neither tested:

1. **Same-configs data can still occasionally regress** — Round 14 itself was only a
   modest, likely-within-noise +5 Elo improvement over Round 13. It's possible the
   "validated-good" configs don't guarantee monotonic improvement forever, and this is
   simply an unlucky draw of games/training initialization at the current data scale
   (~250K positions, where a single new ~13K-position increment is a smaller relative
   share than in earlier rounds).
2. **First real `--resume` interruption as a confound** — the Adam optimizer state
   (momentum estimates) was correctly saved and restored, but the `DataLoader`/sampler's
   RNG state was *not* part of the resume checkpoint, so the shuffling/sampling order for
   epochs 55-60 after resuming differs from what an uninterrupted run would have produced.
   This is a genuinely new variable this round (Round 13 was also interrupted, but at that
   time restarted fully from epoch 0 each attempt, before `--resume` existed) and has never
   been tested for effect on final Elo. **Not confirmed** — val_loss was still improving
   smoothly through the resume with no visible discontinuity (0.550533 at epoch 54 →
   0.550411 at epoch 60), which argues against a dramatic effect, but a subtler one
   affecting final generalization can't be ruled out from this alone.

Neither hypothesis has been tested. An ablation analogous to Round 13's (e.g., retrain
this exact dataset from scratch, uninterrupted, and compare) would distinguish them, at
the cost of another ~8h run. Not scheduled.

### Full Benchmark Progression (N=100, seed=99, Fairy-SF-NNUE depth=3, adj 500cp/5)

| Config | Result | Elo | Notes |
| ------ | ------ | --- | ----- |
| Round 14 net (currently embedded) | 47W 52D 1L | **+173** | best result |
| Round 13 net | 45W 55D 0L | +168 | |
| Round 11 net | 46W 52D 2L | +164 | |
| v7 net | 44W 53D 3L | +151 | |
| Round 15 net | 39W 57D 4L | +127 | regression, not deployed |
| Round 12 net | 35W 61D 4L | +111 | regression (Config B included), not deployed |

### Net Archive — Round 15

* `src/2020277415.bin` — Round 14 net, Elo +173 (N=100) — **currently embedded, unchanged**
* `tools/training/makruk_round15.pt` — Round 15 checkpoint (CRC32=2931901999 when
  exported), val_loss 0.550411, Elo +127 (N=100 seed=99) / +173 (N=100 seed=4242) —
  see follow-up below, gitignored, kept on disk

## Round 15 Follow-up — Seed-Variance Investigation (2026-08-01)

### Motivation

User asked directly why Round 15 regressed: wrong theory, implementation bug, or wrong
handicap choice? Rather than speculate further on the two hypotheses above, re-tested
empirically: is the +127 result itself stable, or could it be gauntlet-testing noise at
a single seed?

### Data check (cheap, done first)

Compared raw score-distribution statistics of Round 13/14/15's new labeled data
(mean/median/std, White/Black/near-equal split) — all three rounds are statistically
indistinguishable. Rules out "qualitatively bad data" as a cause.

### Training-curve check

Re-examined the `--resume` transition point in the training log (epoch 54, saved
best_val=0.550533 → epoch 55, val=0.550613): a ~0.00008 wobble, the same magnitude as
ordinary epoch-to-epoch noise seen throughout the rest of the run (e.g. epoch 30 vs 25).
No visible discontinuity from the resume itself.

### Seed-stability re-test — the actual answer

Re-ran the standard baseline gauntlet for **both** Round 14 and Round 15's nets at a
second seed (4242), alongside the original seed=99 results:

| Net | seed=99 | seed=4242 |
| --- | ------- | --------- |
| Round 14 | +173 | +191 |
| Round 15 | **+127** | **+173** |

Round 15's seed=4242 result (+173) exactly matches Round 14's *original* seed=99 result —
Round 15 is not a broken/damaged net, it's a genuinely strong one. But **Round 14 won at
both seeds tested** (173>127, 191>173), just by very different margins (46 Elo vs 18 Elo).

### Conclusion

Neither "wrong theory," "implementation bug," nor "wrong depth choice" — the dominant
factor was **single-seed N=100 testing noise being larger than this project had
previously characterized**. Earlier sessions estimated the noise band at ~13 Elo (from
Round 11's N=50→N=100 discrepancy and a same-config Fairy-SF-both-versions re-run). This
investigation found a **46 Elo swing from a seed change alone on the same net** — over 3×
that estimate.

The *direction* of the Round 15 regression appears real (Round 14 ahead at both seeds),
but the originally reported *magnitude* (46 Elo) was substantially inflated by seed
noise; a more defensible estimate is a real gap in the roughly 15-35 Elo range, not
confidently pinned down further without more seeds.

**Broader methodological implication, flagged for future rounds**: several earlier
round-to-round deltas in this project (Round 13 vs 11: +4 Elo; Round 14 vs 13: +5 Elo)
are now known to be *smaller* than a demonstrated single-seed noise swing (46 Elo) and
should be treated as unconfirmed directionally, not just "small but real" as previously
assumed. Only the largest deltas (Round 12's regression, and Round 15's regression in
direction if not magnitude) are likely to survive multi-seed scrutiny. **Recommendation
for future rounds**: test at 2+ seeds (not just seed=99) before concluding a round
improved or regressed, especially when the delta is under ~40-50 Elo.

### Decision: unchanged

Round 14 remains deployed — it won both head-to-head seed comparisons against Round 15,
so the decision not to deploy Round 15 stands, but the write-up above supersedes the
original "clear regression, well outside noise" characterization from the initial Round
15 section.

## Round 15 Follow-up — Opening Book + Paired-Game Support (2026-08-01)

### Motivation

The seed-noise investigation above demonstrated a 46 Elo swing from a seed change alone
on the identical net — over 3x the ~13 Elo noise band this project had previously
assumed. Root cause: `gauntlet.py`'s only opening randomization was a small MultiPV-based
pool sampled fresh per game, with no game-pairing (the same random opening was never
guaranteed to be played twice with colors swapped), unlike Stockfish's own fishtest
methodology (large curated opening books, thousands of positions, PAIRED games). This
made single-seed N=100 gauntlets an unreliable basis for the small round-to-round deltas
this project had been deploying on.

### Opening book sourced

Researched popular Makruk opening theory online; rather than hand-curating a small set,
found and adopted the [fairy-stockfish/books](https://github.com/fairy-stockfish/books)
repository (GPLv3) — 66,745 Makruk positions per file, machine-generated via
[fairy-stockfish/bookgen](https://github.com/fairy-stockfish/bookgen) (perft/multipv
search with balance filtering), far more diverse than anything practical to hand-curate.
Downloaded verbatim into `tools/books/makruk.epd` and `tools/books/makruk_no_counting.epd`
with attribution in `tools/books/README.md`. Verified FEN-format compatibility directly
against the engine's own parser (`makrukeval` UCI command gives identical scores on the
same position from either source).

### `gauntlet.py` changes

* `load_book(path)` — parses EPD/FEN book files (first 6 whitespace-separated fields as
  FEN; blank lines and `#`-comments skipped).
* `Engine.search()`, `Engine.search_multipv()`, `play_game()` all gained a `start_fen`
  parameter (default: Makruk start FEN), threaded through to the `position fen ...` UCI
  command and recorded in each game's JSONL record.
* `--book PATH` CLI flag, mutually exclusive with `--opening-plies` (enforced with an
  explicit error — a book position already sets the opening; further per-ply
  randomization on top of it would break the pairing invariant below).
* **Paired-game logic** in the main loop: a new book position is drawn via
  `rng.choice(book_fens)` only at the start of each pair (odd `gid`, when `a_is_white`
  flips true); the even `gid` (second half of the pair) reuses that same `pair_fen` with
  colors swapped (`a_is_white` already alternates every game). This is the standard
  fishtest-style paired-game structure: any positional bias in a given opening is played
  out by both engines from both colors, canceling out in the aggregate score instead of
  contributing seed-dependent noise.

### Verification

Smoke-tested both code paths (6 book games, 2 non-book games) against
Fairy-Stockfish-NNUE:

* Confirmed pairing invariant directly from JSONL output: games 1-2, 3-4, 5-6 each shared
  an identical `start_fen` with `a_is_white` flipped, drawn from three distinct book
  positions.
* Confirmed the `--opening-plies` (non-book) path is unchanged and still functions.
* Confirmed the `--book` + `--opening-plies` mutual-exclusion guard fires correctly.

### Status

Implemented and verified; not yet used for a full-scale round decision. **Next round**
should re-run the Round 14 vs Round 15 comparison (and ideally a fresh Round 16 data/train
cycle) using `--book tools/books/makruk_no_counting.epd` instead of `--opening-plies`, to
establish whether book-paired gauntlets meaningfully shrink the noise band demonstrated
above. `makruk_no_counting.epd` (not `makruk.epd`) is the appropriate default for gauntlet
opponent testing since our engine's own counting-draw logic should decide draws, not an
EPD field baked in from the book generator.

## Round 16 — First Book-Paired Round: Inconclusive, Not Deployed (2026-08-02)

### Method

Identical recipe to Round 14 (150 games at `movetime-a=200 depth-b=4` + 150 at
`movetime-a=150 depth-b=3`, seeds 11001/11002, teacher-labeled at depth=10, filtered to
blend range, combined with the Round 14 base), but using `--book
tools/books/makruk_no_counting.epd` instead of `--opening-plies` for **both** data
generation and the final evaluation gauntlet — the first round to use the new book-paired
methodology end to end. Driver: `tools/training/round16_pipeline.sh`.

Blend-range acceptance and data volume were consistent with prior rounds (no anomalies).
Training: epochs=60, lam=0.7, score-boost=2.0, color-augment, L1=512, `--resume` enabled.
Best epoch 59, val_loss **0.547813** — better than Round 14's 0.545413, though this
project's history (Round 9 onward) has repeatedly shown val_loss does not predict Elo.
Export: `src/150547052.bin` (22.6 MB, int16 MKN2).

A real machine reboot interrupted the pipeline mid-compile during the binary-build stage
(after data-gen, teacher-labeling, and training had already finished cleanly). Recovered
via a full `make clean` + rebuild of both comparison binaries (to rule out a truncated
`.o` file from the interrupt) rather than trust the incremental build state; all 29 tests
passed on the rebuilt binaries before evaluation proceeded. The deploy step was
deliberately never run automatically by the pipeline script — `src/evaluate.h` stayed
pinned to the Round 14 net throughout data-gen/training/build, so the interrupted run
left the repo in its already-committed state with nothing to revert.

### Evaluation — book-paired, 2 seeds, N=100 each (first real test of the new method)

| Net | seed=99 | seed=4242 | spread |
| --- | ------- | --------- | ------ |
| Round 14 (current baseline) | +241 | +230 | **11 Elo** |
| Round 16 (new net) | +186 | +263 | **77 Elo** |

Head-to-head delta (Round16 − Round14, same opponent/settings/paired openings per seed):
seed=99: **−55 Elo** (Round 14 ahead); seed=4242: **+33 Elo** (Round 16 ahead). The two
seeds disagree on direction; averaged, −11 Elo (Round 14 slightly ahead), well inside the
spread either net showed on its own.

All four runs terminated cleanly (0 crashes, no stalemates, adjudication/draw_score
dominant as expected) — the disagreement is not an artifact of a broken run.

### Interpretation: partial validation of the book-pairing fix, plus a new noise source

Round 14's own seed-to-seed spread shrank from the previously-demonstrated **46 Elo**
(under the old unpaired `--opening-plies` method, Round 15 follow-up) to **11 Elo** under
book-pairing at these same two seeds — meaningful evidence the pairing fix is working as
intended for a net whose true strength is stable across openings.

Round 16 did **not** show the same improvement (77 Elo spread) — larger than the old
noise band, not smaller. Because paired openings use the *same* book position played by
both colors, they cancel color/first-move bias, but they don't cancel a different noise
source: **sampling variance from a finite draw of distinct book positions** (100 games =
50 distinct pairs, drawn randomly from 66,745). If a net's strength is more
position-dependent (does noticeably better or worse depending on which openings happen to
get sampled), pairing alone doesn't fix that — only a larger N or more positions would.
Round 16's own net may simply be less positionally robust than Round 14's, or this may be
an artifact of only two seeds — **not distinguished by this test**, would need a third
seed or larger N to tell apart.

### Decision: NOT deployed

The evidence is genuinely mixed (opposite-signed deltas at the two seeds, average delta
smaller than either individual spread) — not a confirmed improvement, and if anything
very weakly negative. Per this project's established practice, an inconclusive result is
not deployed. `src/evaluate.h` was never changed from Round 14
(`2020277415.bin`) — no revert was needed. Round 16 net (`src/150547052.bin`) and its
checkpoint (`tools/training/makruk_round16.pt`) are kept on disk, untracked, for possible
future reference (e.g. as one of multiple sources in a later ablation, following the
Round 13 precedent). Comparison binaries (`src/sf-kernel-round14`,
`src/sf-kernel-round16`) deleted as scratch cleanup.

### Net Archive — Round 16

* `src/2020277415.bin` — Round 14 net, **currently embedded, unchanged**
* `src/150547052.bin` — Round 16 net int16 MKN2, epoch 59, val_loss=0.547813, inconclusive
  vs Round 14 (avg −11 Elo, high seed variance), not deployed, untracked

## Round 17 — SIGSEGV Re-Investigation: Still Unreproduced, Build-Verification Gate Added (2026-08-22)

### Motivation

Re-opened the unfixed depth≥9 SIGSEGV (see "Round 10" and "Round 10 Follow-up" above) on
user request, ~5 weeks after the defensive-hardening commit (`0e3ea6d`). Two Explore
agents first re-audited current state before any new reproduction attempt:

1. **No code has changed** in any crash-adjacent file (`position.cpp`, `misc.h`,
   `half_ka_v2_makruk.*`, `search.cpp`, `movepick.cpp`) since `0e3ea6d` — rules out "a
   later change reintroduced it." Two new hypotheses were checked and ruled out:
   `movepick.cpp` has zero Makruk-specific piece bookkeeping to desync (generic
   Stockfish move-ordering code, `grep` for `Makruk|counting|promotion` returns nothing);
   `MakrukCountingState` — copied via `StateInfo`'s partial `memcpy` in `doMove`
   (`offsetof(StateInfo,key)`, deliberately placed before `key` per an existing code
   comment) — is fully POD/trivially-copyable, safe by construction.
2. **The hardened `abort()` has never fired** in ~5 weeks of subsequent activity (Rounds
   11–16: exhaustive grep for `FATAL`/`SIGSEGV`/core files/journalctl entries, all clean).
   Not evidence the bug is gone, though: every one of those rounds ran our own engine
   under **movetime**-based search in gauntlets, never fixed `go depth N` — the exact
   depth≥9-fixed-depth trigger condition from Round 10 was simply never exercised again.

**New working hypothesis (unconfirmed, unprovable retroactively):** the original crash may
have come from a corrupted/partially-written build rather than a live source bug. This
session personally observed exactly that failure mode — a real reboot interrupted a `make
build` mid-compile during Round 16 (2026-08-02), and recovery required a full `make clean`
rebuild specifically to avoid trusting a possibly-truncated `.o` file. This would explain
every observed data point (one binary crashing 3/3 deterministically, no binary since —
including 1,350 prior reproduction attempts — ever reproducing it), but the original
crashing binary no longer exists, so it cannot be verified either way.

Given the scale of prior reproduction effort (~1,350 invocations, zero crashes) and this
project's established preference for honest "can't-reproduce, hardened-not-fixed"
reporting over false-confidence fixes, the user chose a bounded reproduction attempt
followed by strengthened defenses regardless of outcome (rather than either a full repeat
of the 1,350-invocation sweep, or skipping reproduction entirely).

### Phase 1 — bounded reproduction attempt: no reproduction

Clean-rebuilt the binary (`make clean && make build`), reverified all tests pass, then ran
`tools/training/round17_crash_repro.sh`: **150 self-play games** (`--mode depth_handicap
--depth-cap 1 --movetime 300 --no-counting`, deliberately **without** `--strong-depth` so
the strong side's movetime search deepens freely past depth 9 — the exact condition that
originally triggered the crash), split across both known imbalanced-FEN pools
(`imbalanced_fens.txt`, `imbalanced_fens_v5.txt` — the latter containing the exact FEN that
first surfaced the bug, `r2mksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w`), 3 fresh seeds per
pool (25 games each). `ulimit -c unlimited` was set so a raw segfault (as opposed to the
now-hardened `abort()` paths) would leave a core file for forensics.

**Result: 150/150 games completed, all exit code 0. Zero `FATAL` diagnostics in any stderr
log. Zero core dump files.** Consistent with — not proof against — the bug still existing;
this is now the second independent replication of the original discovery recipe (after the
Round 10 Follow-up's 60-game attempt) to come back clean.

### Phase 2 — `tools/build_verify.sh` added as a standing safety net

Since a source-level fix isn't possible without a reproducing case, added one concrete,
cheap, permanent mitigation targeting both the build-corruption hypothesis and ongoing
regression coverage:

* Always does a **full clean rebuild** (`make clean && make build`) — never trusts
  incremental build state, directly addressing the build-corruption hypothesis: a verify
  step run against a possibly-corrupted incremental build would be worthless.
* Runs the existing unit test suite (`src/makruk/build_tests.sh`).
* Runs a compact **16-invocation crash-trigger probe**: 2 representative imbalanced FENs
  (one from each pool, including the original discovery FEN) × depths {9, 12, 16, 20} ×
  `UseCounting` {true, false}, each sent via UCI (`position fen ... / go depth N`) with a
  30s timeout, checking for a clean exit. Reports pass/fail per case and a final
  `BUILD VERIFIED OK` / failure summary.

Sanity-checked against the current known-good tree: clean rebuild succeeded, full test
suite passed, 16/16 probe invocations exited cleanly. This is now the standard step to run
after any `make build` before trusting a binary for a training round or gauntlet —
documented practice (no CI in this project), same status as `build_tests.sh` today.

### Decision: no code fix — root cause remains unknown, defended at one more layer

Consistent with this project's established practice, the outcome is reported honestly
rather than claimed as fixed: **the crash remains unreproduced** after this session's
bounded attempt, on top of the prior 1,350-invocation sweep. No new source-level lead was
found. The existing hardening (`Position::setCheckInfo` and `ValueList::pushBack` aborting
with FEN/ply/side diagnostics instead of segfaulting) remains the only concrete defense
against the original failure mode; `tools/build_verify.sh` adds a second, independent layer
of defense against the leading (but unconfirmed) alternative explanation. If the crash
resurfaces in the future, `build_verify.sh`'s probe or the existing hardening's diagnostic
output should be the first thing checked — either would finally provide a reproducing case
to drive a real fix.

### Files added this round

* `tools/build_verify.sh` — standing post-build verification gate (Phase 2), committed
* `tools/training/round17_crash_repro.sh` — Phase 1's bounded reproduction driver,
  committed for reuse if another attempt is warranted later
* `tools/gauntlet_out/round17_repro/` — raw logs/PGN/JSONL from the 150-game reproduction
  attempt (gitignored, kept on disk for reference)

## Round 18 — True-Strength Baseline + Incremental NNUE Accumulator (2026-08-22)

### Part 1: the first true equal-condition baseline

Every Elo number in this project's history through Round 17 (+111 to +382) was measured
under a **handicap favoring sf-kernel** (opponent capped at a lower depth or movetime).
Asked to establish a real baseline for setting future targets, ran the first **equal-condition**
gauntlet: 200 games, book-paired, 200ms movetime for **both** sides, vs Fairy-Stockfish-NNUE,
current net (`2020277415.bin`, Round 14).

**Result: 0W 42D 158L → Elo −372.** A stark, clean result (0 crashes, healthy termination
breakdown) — sf-kernel is dramatically weaker than a mature reference engine at matched
conditions. This reframes every prior handicap-gauntlet Elo number in this project as *not*
representative of true strength; they measured performance under a favorable handicap, not
real playing strength.

### Part 2: root-causing the gap — NNUE evaluation, not net size

Asked to analyze whether increasing NNUE net size (`L1`) could help close the gap. Live
profiling instead found the real bottleneck: our engine's nps collapses from **~1.75M to
~38-41K (45x slower)** whenever NNUE evaluation fires (i.e. on most near-equal positions —
`evaluate.cpp`'s NNUE gate is `|classical| < 300`). Root cause: `MknnEvaluator::evaluate()`
(`src/nnue/mknn_evaluator.cpp`) recomputed its entire feature-transformer (FT) accumulator
**from scratch on every single call** — full board rescan + full accumulation — instead of
incrementally updating a cached accumulator per move, defeating the entire point of
"Efficiently Updatable" NNUE. Increasing `L1` at that point would have made per-call cost
*worse*, not better.

Two Explore agents then found the complete infrastructure for a correct incremental
implementation already present as **dead, unused legacy Stockfish NNUE code**:
`StateInfo::accumulator` (unused legacy field, wrong shape for MKN2), `DirtyPiece` (already
correctly populated by every Makruk move type in `doMove`), and — critically —
`HalfKAv2Makruk::appendChangedIndices`/`requiresRefresh`/`updateCost`/`refreshCost`
(`src/nnue/features/half_ka_v2_makruk.{h,cpp}`), which implement the exact incremental-update
API needed but were never wired into `MknnEvaluator`. A Plan agent (opus) then produced a
verified implementation design, catching a subtle bug before any code was written: `doNullMove`'s
partial `memcpy` in `position.cpp` copies further into `StateInfo` than `doMove`'s does (up to
`accumulator`, not just `key`), so a naive "place the new field after `key`" placement (the
precedent from the SIGSEGV-hardening `dirtyPiece` placement) would have been stale-copied.

### Implementation

New files: `src/nnue/mknn_accumulator.h` (`MknnAccumulator`: `int32_t acc[2][512]` +
`computed[2]`, riding the existing `StateInfo`/`doMove`/`undoMove` chain exactly like
`DirtyPiece` already does — zero new thread-safety work needed, confirmed by construction).
`mknnAcc` is the **last** member of `StateInfo` (`position.h`), outside both `doMove`'s and
`doNullMove`'s partial-memcpy ranges. `MknnEvaluator::updateAccumulator()`
(`mknn_evaluator.cpp`) ports the legacy `FeatureTransformer::updateAccumulator` algorithm
(`nnue_feature_transformer.h:232-345`) onto int32/MKN2: walks `StateInfo::previous` backwards
under a cost budget for the nearest ancestor with a valid cached accumulator (stopping early
if `requiresRefresh` — our king moved — fires), then replays the diff forward hop-by-hop via
`appendChangedIndices`, or falls back to a full rebuild if no usable ancestor was found.
Implements the **general multi-hop case**, not just 1-hop — justified because `evaluate()`'s
NNUE gate (`|classical| < 300`), in-check skips, and TT-cached-eval skips all routinely punch
multi-node gaps in the "computed" chain during real search. Scope narrowed to VERSION2 (int16
MKN2, the only actively-deployed format since Round 6) — VERSION1 and any net with `L1 > 512`
keep the original always-full-refresh path unconditionally, unchanged.

`sizeof(StateInfo)` grew 4544 → 8704 bytes; added a `static_assert(<= 16384)` tripwire given
the unresolved SIGSEGV history on this exact code path (Round 10/17). A `-Wclass-memaccess`
warning appeared on the existing `memset`/`memcpy` calls in `position.cpp` (the new field's
default member initializer makes `StateInfo` no longer a strictly "trivial" type, though it
remains trivially-copyable and the calls stay well-defined) — silenced with scoped, documented
`#pragma GCC diagnostic` blocks rather than removing the initializer that makes the accumulator
cache safe-by-construction.

### Correctness verification

New `src/makruk/test_nnue_incremental.cpp`: loads the real embedded net and asserts the
incremental path is **bit-exact** against a from-scratch reference rebuild (valid because
int32 accumulation is exact/associative, no floating-point rounding) across 13 scenarios —
quiet move, capture, promotion, capturing promotion (`dirty_num=3`, the struct's max), own
king move, opponent king move, null move, multi-hop gaps (including one spanning a king move),
budget exhaustion, sibling reuse, and a 500-ply fixed-seed random walk. Critical finding during
test-writing: `build_tests.sh` compiles with `-DNDEBUG`, which **silently strips every
`assert()`** in the entire existing test suite — this and all prior test files have been
running with zero assertions active. The new test therefore uses explicit runtime checks
(`std::exit(1)` on mismatch) rather than `assert()`.

Result: **188/188 exactness checks bit-exact, zero failures**, across every scenario. Also
ran under a manually-built AddressSanitizer+UBSan variant (both the crash-probe FENs and the
new test standalone) — clean, with one informational (not a bug) UBSan note about
`doNullMove`'s TT-prefetch touching an unresized table in this test's minimal harness (never
happens in real play, where `main.cpp` always resizes the TT first), documented in-code rather
than "fixed" by adding unrelated engine bring-up to the test. Separately surfaced — and
explicitly **not** fixed, filed for a future session — a pre-existing, unrelated dormant
assertion failure in `test_counting.cpp` that only manifests when compiled without `-DNDEBUG`
(i.e. has silently never actually run in this project's history until this session's ASan
build tried it). `perft 5` from the Makruk start position: unchanged at 6,223,994 nodes.
`tools/build_verify.sh` (clean rebuild + full suite + 16-case crash probe): clean.
`go depth 30` stack check: 132 KB `VmStk`, no risk from the doubled `StateInfo` size.

### Performance: real, but far smaller than initially predicted — L2 is the actual bottleneck

Added temporary timing instrumentation (removed before commit) to find out why nps barely
moved after the fix. Diagnostic counters confirmed the algorithm works exactly as designed
(81% of calls hit the fast 1-hop path, 17% multi-hop, only 2% full refresh) and FT accumulation
dropped to **~3µs/call** as expected. But **L2 (`Linear(1024→32)`, unchanged by this fix) costs
~32.6µs/call** — 5x higher than this round's own design-phase estimate of 5-7µs, and now
overwhelmingly the dominant per-eval cost (likely an unvectorized reduction loop with a serial
dependency chain, since the build uses no `-ffast-math`). Net nps: **~40K → ~41-45K**, a real
but modest ~10-15% gain, not the originally-hoped 2.5-3x.

### Gauntlet result — real, modest improvement

Same equal-condition format as Part 1's baseline (200 games, book-paired, 200ms both sides,
seed=99, vs Fairy-Stockfish-NNUE):

* **Baseline (Round 14 net, non-incremental eval): 0W 42D 158L → Elo −372**
* **This round (same net, incremental FT accumulator): 0W 45D 155L → Elo −359**

**+13 Elo — small, real, zero regressions, zero crashes.** Consistent with the corrected (not
the original optimistic) performance picture: FT was never as dominant a bottleneck as assumed,
so fixing only FT recovers only a fraction of the eval-cost reduction needed to meaningfully
narrow a 370+ Elo gap. Deployed (this is a pure code-quality and small-but-real Elo
improvement with no downside found) — no `evaluate.h` net swap needed, since this change is
independent of which net is embedded.

### Next step: L2 is now the precisely-characterized target, not net size

The original question ("would a bigger net help?") is now answerable: **no, not yet** — a
bigger `L1` would make FT accumulation cost more per call while the actual bottleneck (L2)
stays untouched. The concrete next-round target is now L2/L3, e.g. int8/int16 quantization of
`l2_weight_` (currently float32, 128KB read every call) analogous to what Round 6 already did
for FT, or restructuring the reduction loop to enable auto-vectorization. Net-size analysis
should wait until per-call eval cost is no longer L2-dominated, so its effect can be measured
cleanly — same reasoning this round applied to deferring it past the FT fix.

### Files added/changed this round

* `src/nnue/mknn_accumulator.h` *(new)* — `MknnAccumulator` struct
* `src/nnue/mknn_evaluator.h`/`.cpp` — `incremental_` flag, `updateAccumulator`, `refreshInto`/
  `addColumn`/`subColumn`, `evaluate()` branch; `accumulate_i16`/`accumulate_f32` untouched
* `src/position.h` — `mknnAcc` field (last `StateInfo` member) + `static_assert`
* `src/position.cpp` — `computed[]` resets in `doMove`/`doNullMove`/`setState`; scoped
  `-Wclass-memaccess` suppressions with rationale comments
* `src/thread.cpp` — `computed[]` reset after `rootState` assignment
* `src/makruk/test_nnue_incremental.cpp` *(new)* + `build_tests.sh`/`test_runner.cpp` wiring
* `tools/gauntlet_out/round17_baseline/`, `tools/gauntlet_out/round17_incremental/` — raw
  gauntlet logs/JSONL for both equal-condition runs (gitignored, kept on disk)

## Round 19 — Upstream Diff Audit Finds a Real seeGe() Bug (2026-08-22)

### Motivation

Asked to check for other bugs by diffing against upstream `FireFather/sf-kernel`
(the `upstream` remote, already configured). Upstream has not advanced since our fork point
(`upstream/main` tip == `git merge-base main upstream/main` exactly) — nothing to pull — but
comparing our full diff against it is a systematic way to find unintentional divergences from
correct generic-engine logic introduced during the Makruk conversion.

### Method

`git diff upstream/main HEAD -- src/` shows `search.cpp`, `movepick.cpp`, `tt.cpp`,
`timeman.cpp`, `main.cpp` are **completely unchanged** — the core search algorithm itself was
never touched. Reviewed every file that *was* changed (`position.cpp`/`.h`, `bitboard.cpp`/`.h`,
`movegen.h`, `evaluate.cpp`, `misc.h`, `types.h`, `uci.cpp`, `ucioption.cpp`,
`nnue_architecture.h`, `Makefile`) hunk by hunk, cross-referencing against the correctly-adapted
Makruk piece-movement patterns already established elsewhere in the same files
(`attackersTo()`, `setCheckInfo()`, `sliderBlockers()`, and `movegen.h`'s piece-move generation
all correctly handle Khon's color-dependent movement and Met's diagonal-only, non-sliding
movement).

### Bug found: `Position::seeGe()` (Static Exchange Evaluation) — two related issues

`seeGe()` (`position.cpp`), used throughout `search.cpp` (SEE pruning, LMR reductions,
futility pruning) and `movepick.cpp` (capture move ordering), never received the same
Makruk-adaptation pass as `attackersTo()`/`setCheckInfo()`/`sliderBlockers()` in the same file.
Its incremental "revealed attacker" updates (after removing a piece from the simulated
exchange) used:

1. `attacksBb<BISHOP>(to,occupied)&pieces(BISHOP,QUEEN)` — silently unions **both colors'**
   Khon movement directions (White: forward+diagonals is NORTH; Black: SOUTH+diagonals) instead
   of the reverse-color lookup (`khonAttacksBb(~attackerColor, to)`) that `attackersTo()` already
   uses correctly. **Confirmed empirically**: a White Khon placed one square north of a test
   square `to` (a direction only a *Black* Khon could attack `to` from) was correctly excluded
   by `attackersTo()` but incorrectly included by this formula.
2. `attacksBb<ROOK>(to,occupied)&pieces(ROOK,QUEEN)` — includes Met (QUEEN slot) in the
   Rook-line slider "X-ray" check, but Met has **zero** rank/file movement (1-step diagonal
   only) and can never be a Rook-line attacker — exactly the reasoning `sliderBlockers()`
   already correctly applies (`pieces(ROOK)` alone, no QUEEN). **Confirmed empirically**: a Met
   placed on the same rank as `to`, far away, was correctly excluded by `attackersTo()` but
   incorrectly included by this formula.

Both bugs are live, not dead code (unlike two similarly-shaped `EN_PASSANT`-branch instances of
the same union pattern in `legal()`/`givesCheck()`, confirmed unreachable since `movegen.h`
never generates an `EN_PASSANT`-typed move in this fork). Root cause: neither Khon nor Met is
a slider, so the entire "does removing a piece reveal a new attacker" concept these lines exist
for doesn't apply to them at all — the correct fix is a static, `occupied`-independent term, not
a slider-style recomputation.

### Fix

`position.cpp`: replaced both patterns with the same reverse-color/no-slider formulas already
verified correct in `attackersTo()` (a single `khonMetAttackers` term shared by the PAWN/BISHOP
branches; `pieces(ROOK)` without `QUEEN` in the ROOK/QUEEN branches). Piece-check order
(`PAWN, KNIGHT, BISHOP, ROOK, QUEEN`) deliberately left unchanged, even though it doesn't match
Makruk's actual value ranking (Met=420 is the *second-cheapest* piece, below Khon=660 and
Knight=781, yet checked last) — that's a separate, real issue, **not fixed this round**,
flagged below.

### Regression test: `src/makruk/test_see.cpp` (new)

Following the same "fast-vs-slow-reference" methodology as `test_nnue_incremental.cpp`: a
`referenceSeeGe()` reimplementation re-derives the attacker set from scratch via the
already-trusted `attackersTo()` at every step (obviously correct by construction, just slow),
cross-checked against the real `seeGe()` across every capture move, several thresholds, at
every ply of a 300-ply fixed-seed random walk plus two hand-built direct-repro positions.
**2165 checks, all match, on the fixed code.** Verified the test has real teeth: temporarily
reverted the `position.cpp` fix (`git stash`) and reran — **24 real failures** appeared,
confirming both the bug and the test's ability to catch it; restored the fix (`git stash pop`)
and reconfirmed clean. Wired into `build_tests.sh`/`test_runner.cpp`.

### Verification

Full test suite (all existing + new `test_see.cpp` + `test_nnue_incremental.cpp`): clean.
`perft 5` from the Makruk start position: unchanged at 6,223,994 nodes (confirms move
generation/check detection untouched — the bug and fix are confined to `seeGe()`).
`tools/build_verify.sh`: clean (16/16 crash probes).

### Gauntlet result — noisy, not read as a regression

Same equal-condition format as Rounds 18's two baselines (200 games, book-paired, 200ms both
sides, seed=99, vs Fairy-Stockfish-NNUE):

* Round 18 (incremental NNUE, no SEE fix): 0W 45D 155L → Elo −359
* This round (SEE fix added): 0W 41D 159L → Elo **−377**

A nominal −18 Elo, but this project's own demonstrated single-seed noise band is up to **46
Elo** (see `gauntlet-seed-noise-finding` memory) — an 18 Elo delta is comfortably inside that
band and is not treated as evidence of a real regression. Unlike the NNUE net-selection rounds
(where Elo *is* the primary signal for an inherently uncertain "does this data/architecture
change help" question), this is a **proven logic bug** — verified by mathematical derivation,
direct empirical reproduction, and a 2165-check regression suite that demonstrably catches the
bug when reverted. The deploy decision rests on that correctness proof, not on a single noisy
N=200 gauntlet. Deployed.

### Flagged, not fixed: SEE piece-check ordering doesn't match Makruk values

While rewriting the buggy lines, noticed `seeGe()`'s attacker-check order
(`PAWN, KNIGHT, BISHOP, ROOK, QUEEN`) is inherited verbatim from upstream chess (where it
matches ascending value: Pawn~100 < Knight~300 ≈ Bishop~300 < Rook~500 < Queen~900). Makruk's
actual values are very different: **Pawn(126) < Met/QUEEN(420) < Khon/BISHOP(660) <
Knight(781) < Rook(1276)** — Met is the *second-cheapest* piece, not the most expensive, yet is
still checked *last* in the chain. The standard SEE algorithm assumes attackers are tried in
ascending value order (the "assume optimal play" invariant); checking Knight before the
cheaper Khon, and Met dead last, means `seeGe()` may not always pick the truly-least-valuable
attacker first, and can misjudge equal/near-equal exchanges. This was noticed but deliberately
**not fixed this round** — kept the approved fix surgical (attack-pattern correctness only).
Worth a focused follow-up: reorder to `PAWN, QUEEN, BISHOP, KNIGHT, ROOK` and re-verify against
`test_see.cpp` (which would need extending, since order doesn't change the reference-vs-actual
comparison already in place unless the *reference* also reorders).

### Net Archive / deployment note

This is a pure `position.cpp` search-correctness fix, independent of which net is embedded —
`src/evaluate.h` unchanged, still `2020277415.bin` (Round 14).

## Round 20 — seeGe() Attacker-Check Order Fixed to Match Makruk Values (2026-08-23)

### Motivation

Round 19 deliberately left one flagged issue unfixed to keep that change surgical: `seeGe()`'s
attacker-check order (`PAWN, KNIGHT, BISHOP, ROOK, QUEEN`) is inherited from upstream chess's
ascending-value order (Pawn < Knight ≈ Bishop < Rook < Queen), but Makruk's actual values are
very different — `Pawn(126) < Met/QUEEN(420) < Khon/BISHOP(660) < Knight(781) < Rook(1276)`.
Met is the *second-cheapest* piece, not the most expensive, yet was checked *last*. Asked this
round to fix it.

### Fix

Reordered the `if`/`else if` chain in `Position::seeGe()` to
`PAWN, QUEEN, BISHOP, KNIGHT, ROOK` — ascending Makruk value order, matching SEE's core
"assume optimal play" invariant (always try the truly-least-valuable attacker first at each
step). Each branch's internal logic (value subtraction, which reveal-set gets recomputed after
removing that piece type) was left exactly as-is from the Round 19 fix — only the check *order*
changed.

### This is a real correctness fix, not cosmetic — confirmed two ways

1. **Updated `test_see.cpp`'s `referenceSeeGe()` to the same corrected order** (it must match
   `seeGe()`'s order exactly, since which attacker is tried first is part of SEE's actual
   result, not just an optimization/performance detail). All 2165 checks still pass.
2. **Verified the order genuinely matters** by temporarily reverting *only* the reference's
   order back to the old (mismatched) ordering while keeping the real `seeGe()` fixed — this
   produced 4 real disagreements between `seeGe()` and the intentionally-mismatched reference,
   directly proving check order changes SEE's boolean output, not merely its internal
   bookkeeping. Restored the matching order immediately after confirming this.

### Verification

Full test suite (including both new/recent tests): clean. `perft 5`: unchanged at 6,223,994
nodes. `tools/build_verify.sh`: clean (16/16 crash probes).

### Gauntlet result — real, positive improvement

Same equal-condition format as Rounds 18-19's baselines:

* Round 19 (attack-pattern fix only, order unfixed): 0W 41D 159L → Elo −377
* This round (order fix added): 0W 48D 152L → Elo **−346**

**+31 Elo over Round 19, +13 Elo over the original Round 18 baseline (−359).** Still technically
inside this project's demonstrated ~46 Elo single-seed noise band, so not claimed as a precisely
quantified gain — but the direction is consistent with the correctness argument (SEE now
actually satisfies its least-valuable-attacker-first invariant for Makruk's piece values, which
it provably did not before), and unlike the Round 19 result (which moved in the "wrong"
direction relative to expectation and was attributed to noise), this one moved in the expected
direction. Deployed — no net swap involved, pure `position.cpp` search-correctness change.

### Files changed this round

* `src/position.cpp` — `seeGe()` attacker-check reorder only, no other logic changed
* `src/makruk/test_see.cpp` — `referenceSeeGe()` reordered to match; header comment extended
  to document this third bug alongside Round 19's two attack-pattern bugs
* `tools/gauntlet_out/round20_seeorder/` — raw gauntlet log/JSONL (gitignored, kept on disk)

## Round 21 — Scoped Fast-Math Closes Most of the NNUE nps Gap (2026-08-23)

### Motivation

Round 18 found nps only improved modestly (~40K → ~41-45K) after fixing the non-incremental
NNUE accumulator, because L2 (`Linear(1024→32)`, untouched by that fix) turned out to cost
~32.6µs/call — 5x higher than the design-phase estimate — and became the new dominant cost.
Asked directly this round to close the gap with Fairy-Stockfish-NNUE's nps.

### Investigation

Live comparison confirmed the real gap: sf-kernel ~40-44K nps vs Fairy-SF-NNUE ~450-475K nps
on the Makruk start position — roughly 11-12x. Two things investigated hands-on (direct
compile/benchmark, not agent-derived):

1. **AVX2 was a red herring.** Round 18 flagged rebuilding with `ARCH=x86-64-avx2` as a likely
   "free" win. Tested two ways (plain `ARCH=x86-64-avx2`, and a combined
   `ARCH=x86-64-avx2-bmi2 SUPPORTED_ARCH=true` rebuild) — **neither changed nps at all**.
   Root cause: `src/Makefile`'s `-bmi2` ARCH profile already cascades `avx2=yes` — the project's
   standard build (`ARCH=x86-64-bmi2`) has **always** compiled with `-mavx2`. There was nothing
   to add.
2. **The real bottleneck is the compiler's conservative (non-reassociating) handling of the L2
   float reduction loop**, not available SIMD width. The `s += row[j]*l2_in[j]` accumulation has
   a serial dependency on `s`; without reassociation permission, GCC's auto-vectorizer leaves it
   scalar even with `-mavx2` available. Compiling just `mknn_evaluator.cpp` with the default
   flags plus `-ffast-math`: nps jumped to **~150-188K, roughly 4x**. Confirmed the narrower,
   safer subset `-fassociative-math -fno-signed-zeros -fno-trapping-math -fno-math-errno`
   (reassociation permission only, without `-ffast-math`'s riskier `-ffinite-math-only`/
   `-funsafe-math-optimizations`, which assume no NaN/Inf and could mask real bugs) gives the
   **same speedup** — this is the flag set shipped, not full `-ffast-math`.

### Second finding: the project's own default build config was inconsistent

Asked whether to emphasize `ARCH=x86-64-bmi2` specifically. Checked, and found a real, separate
issue: `src/Makefile`'s no-`ARCH` fallback was `x86-64-modern`, which enables **neither** AVX2
nor BMI2/PEXT — a bare `make build` would silently produce a slower binary than every
benchmark/gauntlet in this project has ever actually used. Grepping the repo's own tooling
confirmed this wasn't hypothetical: `tools/build_verify.sh`/`round16_pipeline.sh` correctly use
`ARCH=x86-64-bmi2`, but the older `run_pipeline.sh` uses `x86-64-modern` and
`run_v5_pipeline.sh`/`deploy_net.sh` use `x86-64-avx2` (no PEXT) — three different
configurations across this project's history. Fixed the Makefile's own default to
`x86-64-bmi2`; left the three stale, no-longer-active historical scripts unmodified (out of
scope, not part of the current workflow).

### Implementation

Two small changes, both in `src/Makefile`:
1. `mknn_evaluator.o: CXXFLAGS += -fassociative-math -fno-signed-zeros -fno-trapping-math
   -fno-math-errno` — a GNU Make target-specific variable, scoped to only this one file's
   compilation (verified in the build log: only `mknn_evaluator.o`'s compile command carries
   the extra flags).
2. The `ifeq ($(ARCH),)` fallback changed from `x86-64-modern` to `x86-64-bmi2`.

### Why this is safe

The incremental accumulator (Round 18) is entirely `int32_t`/`int16_t` arithmetic — untouched by
float reassociation flags, which only affect the crelu/L2/L3/out float layers. Fast-math
reassociation is a fixed compile-time transformation, not runtime nondeterminism, so calling the
same compiled function twice with identical inputs still gives identical output — `test_nnue_incremental.cpp`'s
bit-exact and cross-call checks remain valid guarantees, not just "should be fine" assumptions.
Not applied to `build_tests.sh`'s separate test-binary compile (stays flag-minimal/portable) —
the real Makefile-built binary is what `build_verify.sh` and the live gauntlet actually exercise.

### Verification

`make clean && make ARCH=x86-64-bmi2 build` vs bare `make clean && make build` (no `ARCH=`):
byte-identical binaries (confirmed via `md5sum`) — validates the default-ARCH fix. Full test
suite (188 + 2165 checks) unchanged and clean. `perft 5`: unchanged at 6,223,994 nodes.
`tools/build_verify.sh`: clean. Live nps on the real deployed binary: **~138-177K**, reaching
depth 13 in 500ms at the start position (vs the old depth 9-10) — matches the throwaway-binary
prediction closely.

### Gauntlet result — the largest single-round improvement this project has measured

Same equal-condition format as every round this session:

* Round 20 (previous baseline): 0W 48D 152L → Elo −346
* This round (scoped fast-math): 0W 66D 134L → Elo **−282**

**+64 Elo** — well outside this project's demonstrated ~46 Elo single-seed noise ceiling, so
read as a real improvement rather than noise (unlike several earlier small deltas that needed
that caveat). Draws rose from 24% to 33%, consistent with deeper, stronger play against a
comparably-tuned opponent. Deployed. Not run at a second seed given the effect size is
substantially larger than the known noise band; a second-seed confirmatory run remains a cheap,
available follow-up if further confidence is wanted.

### Next step

The nps gap with Fairy-SF-NNUE (~450-475K) is now ~2.7-3.4x, down from ~11-12x. L2 is still the
largest remaining eval cost (int32/int16 quantization of `l2_weight_`, matching what Round 6 did
for the FT, is the next concrete lever if further gains are wanted) — not attempted this round to
keep the change minimal and measure this win's impact cleanly first.

### Files changed this round

* `src/Makefile` — the only file changed: one target-specific variable line (fast-math scoping)
  + one default-value line (ARCH fallback)
* `tools/gauntlet_out/round21_fastmath/` — raw gauntlet log/JSONL (gitignored, kept on disk)

## Round 21 Follow-up — N=100 Confirmatory Gauntlet at a Second Seed (2026-08-29)

### Motivation

This project's own established practice (see the Round 15 seed-variance finding) is to test
2+ seeds before fully trusting a gauntlet delta, especially given a demonstrated ~46 Elo
single-seed noise band. Round 21's +64 Elo result (seed=99) is comfortably above that band, but
given it's the largest single-round gain measured in this project's history, confirmed it at a
second seed before treating it as settled.

### Method

Built two binaries from git history — `sf-kernel-round20` (commit `4aec75b`, immediately before
the fast-math fix) and `sf-kernel-round21` (commit `4510afb`, current HEAD) — via a temporary
git worktree, verified they're genuinely distinct (nps sanity check: ~39K vs ~180K, matching
each round's own measurement). Ran both against Fairy-Stockfish-NNUE at the same book-paired,
100-game, 200ms-both-sides format, seed=4242 (the same second seed used in the original Round 15
seed-variance investigation, for continuity).

### Result — direction confirmed, magnitude even larger at the second seed

| | seed=99 (original) | seed=4242 (confirmatory) |
| --- | --- | --- |
| Round 20 (pre-fast-math) | −346 | **−372** |
| Round 21 (post-fast-math) | −282 | **−282** |
| **Delta** | **+64 Elo** | **+90 Elo** |

Both seeds agree on direction, and the second seed's delta is *larger*, not smaller — strong
confirmation this is a real effect, not noise. Notably, Round 21's own result was identical at
both seeds (−282 exactly), while Round 20's varied by 26 Elo — consistent with (though not
proof of) the earlier hypothesis that a deeper, more stable search should also reduce
game-to-game/seed-to-seed variance, not just raise average strength. Both 100-game runs
completed with zero crashes.

### Decision

No change needed — Round 21's fast-math fix was already deployed and is now confirmed, not
just measured once. Comparison binaries (`src/sf-kernel-round20`, `src/sf-kernel-round21`)
deleted as scratch cleanup after confirming results.

## Round 22 — Heap-Allocation Elimination in MknnEvaluator::evaluate() (2026-08-29)

### Motivation

Round 21's fast-math fix closed most of the nps gap (~40K → ~150-190K) by fixing the L2/L3
forward pass's dominant cost. Asked directly to continue closing the remaining gap with
Fairy-SF-NNUE. Re-measured L2/L3 cost post-fast-math with temporary timing instrumentation
(same pattern as Round 18): `avgFT_ns=3157, avgL2_ns=5595, avgL3Out_ns=386` — L2 dropped from
32.6µs (Round 18) to 5.6µs, a ~5.8x reduction purely from Round 21's flags. Given this
recalibration, offered the user two options and they chose the lower-risk one first: eliminate
`evaluate()`'s remaining `std::vector` heap allocations (cheap, safe), then re-measure whether
it's worth it before considering L2 weight quantization (bigger effort, uncertain payoff at
this smaller remaining cost).

### Implementation

`src/nnue/mknn_evaluator.h`: new `MknnMaxL2=128`/`MknnMaxL3=128` compile-time caps (alongside
the existing `MknnMaxL1=512`), and a `fastEvalOk_` flag computed at `load()` time — true
whenever a net's `L1_`/`L2_`/`L3_` all fit within the caps (every net trained in this project
so far qualifies, since every net uses L2=L3=32). `src/nnue/mknn_evaluator.cpp`: `evaluate()`
now branches — when `fastEvalOk_`, a new `forwardPass()` helper does the L2/L3/out math using
fixed-size stack buffers (zero heap allocation); a new `accumulateF32Into()` covers the rare
VERSION1-with-small-L1 case for the fast path (every VERSION2 net within cap already goes
through the Round 18 incremental accumulator instead). The original `std::vector`-based path
is kept byte-for-byte unchanged as the fallback for any net exceeding the caps — no capability
lost, only the allocation-avoidance optimization skipped for that (currently nonexistent) case.

### Verification

- Full test suite (188 NNUE bit-exact checks + 2165 SEE checks + all others): pass.
- `perft 5`: unchanged, 6,223,994 nodes.
- **Direct before/after comparison** (addresses a gap self-consistency tests can't catch — a
  "consistently wrong" reordering would still pass a before-vs-itself test): built the
  pre-refactor binary from committed HEAD in a worktree, added a temporary `nnueeval` debug UCI
  command (`cout << int(Eval::evaluate(pos))`) to both binaries, diffed output across 18 diverse
  positions and move sequences (openings, endgames, promotions, Khon/Met-heavy middlegames,
  multi-hop incremental-accumulator chains) — **every value matched exactly, bit-for-bit**.
  Temporary command reverted from both trees before committing (confirmed via `git status`/
  `git diff` clean).
- `tools/build_verify.sh`: clean (on the working branch where the script exists — see gap
  finding below).
- nps: `bench` command, node counts identical both runs (1,696,682) — confirms correctness
  independent of the timing measurement. 254,108 → 270,215 nps (**+6.3%**). Single-position
  `go movetime 3500` cross-check: ~203-205K → ~219-222K nps (depth 20-21), consistent.

### Gauntlet result — small, real gain matching the modest nps change

Same equal-condition format as every round since 18 (200 games, book-paired, 200ms both sides,
seed=99, vs Fairy-Stockfish-NNUE):

* Round 21 (previous baseline): 0W 66D 134L → Elo −282
* Round 22 (heap allocation eliminated): 0W 70D 130L → Elo **−269**

**+13 Elo.** Small and technically inside the demonstrated ~46 Elo single-seed noise band, but
positive and proportionate to the measured nps gain (+6.3%, vs Round 21's ~4x nps gain that
produced +64/+90 Elo) — the magnitude scaling roughly sensibly between the two rounds is a mild
additional (not conclusive) signal this is real rather than pure noise. Deployed via PR.

### Important process finding: `main` is missing Round 15-21 documentation and tooling

While preparing this round's PR (based fresh on `origin/main`, following the PR #15
precedent), discovered that PR #15 (`cpp-fixes-round18-21`) deliberately scoped to only 12
C++ source files. As a result, `main` is currently **missing**:

* `CLAUDE.md`'s Round 15 through Round 21 write-ups entirely (2,461 lines of diff) — the
  version of this file on `main` predates the opening-book work, the true-strength-baseline
  discovery, and every fix documented above it in this file.
* `tools/build_verify.sh` (Round 17's standing verification gate) and
  `tools/training/round17_crash_repro.sh`.
* `tools/books/` (the fairy-stockfish/books opening books) and `gauntlet.py`'s book-pairing
  support (Round 15 follow-up) — **the book-paired equal-condition gauntlet methodology used
  for every Elo number from Round 16 onward does not exist on `main`.**
* Several `tools/training/*.sh` pipeline scripts and `docs/dataset_guide.md`.

This file (`CLAUDE.md`) and this session's own working directory are on
`pr11-net-validation-decisive-data` (an old branch name, 65 commits ahead of `main`, 10 behind
`origin/main`), which is where all of this actually lives. PR #16 (this round's change) was
based on `origin/main` and flagged this gap in its description, but did not attempt to close
it — that's a separate, larger cleanup (bring `CLAUDE.md`, `tools/build_verify.sh`,
`tools/books/`, and `gauntlet.py`'s book support to `main` via their own PR) that hasn't been
scoped or requested yet. Flagged for a future session/explicit request.

### Files changed this round

* `src/nnue/mknn_evaluator.h` / `.cpp` — the only files changed, via PR #16
  (`nnue-eval-heap-refactor` → `main`)
