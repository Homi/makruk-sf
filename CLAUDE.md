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

## Next Development Priority

Before making the network larger or training longer:

1. Generate decisive games using `time_handicap` or `depth_handicap` mode
2. Verify decisive rate ≥ 30% with `dataset_summary.py`
3. Mix normal + decisive datasets before training
4. Run gauntlet before/after training to measure actual strength gain
5. Only then consider teacher-labeled positions or quantized net export

Avoid assuming that a lower validation loss means stronger play until gauntlet tests confirm it.
