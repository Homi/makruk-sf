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

---

## Makruk Rules Target (implemented)

* 8x8 board. White starts rank 1 (pawns rank 3); Black starts rank 8 (pawns rank 6).
* Start FEN: `rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1`
* Pieces: King=Khun, Rook=Ruea, Knight=Ma, Khon (chess `BISHOP` slot, one step forward or
  diagonal), Met (chess `QUEEN` slot, one step diagonal only), Pawn (one step forward,
  diagonal capture). Enum numeric layout kept compatible with upstream chess piece types.
* Removed from chess: castling, en passant, pawn double-step, chess-style promotion choices,
  the chess 50-move rule as a draw trigger.
* Promotion: White on rank 6, Black on rank 3, to Met only. A promoted pawn IS a QUEEN-slot
  piece — there is no separate "promoted pawn" type in the board representation.
* Draw rules: stalemate, repetition, and Makruk counting (see below) — not the chess 50-move
  rule.

---

## Makruk Counting Subsystem (implemented)

`src/makruk/makruk_counting.{h,cpp}` — `MakrukCountingState` tracks eligibility, claimant,
count, limit, and material signature. `Nebula::gMakrukCountingMode` (UCI option `UseCounting`,
default on) selects `Fairy`-compatible counting or `Off` (the latter only for training-data
generation, never for real play/gauntlets — counting-off endgames carry real material-based
eval instead of being compressed toward a draw). Debug command: `makrukcount`.

`Position::isDraw()` calls `isMakrukCountingDraw()` directly (not the chess 50-move rule);
`doMove()`/`undoMove()`/`set()` maintain the counting state as part of `StateInfo`, so it rides
along the existing search state-copy machinery for free.

---

## Evaluation

Classical eval (`src/makruk/makruk_eval.{h,cpp}`) is **not a fallback** — `evaluate.cpp` blends
`classical + clamp(nnue - classical, -100, 100)` and consults NNUE only when `|classical| < 300`,
so classical eval directly and significantly shapes real play at all times.

Material values (MG/EG): Pawn 126/208, Knight 781/854, Khon 660/640, Rook 1276/1380, Met
420/450, King 0. Implemented heuristics: PST tables (all piece types), rook activity
(open/semi-open file), king safety (pawn shield, MG only), KBK/mopup endgame guidance
(push weak king to a Khon-reachable corner, close own king), insufficient-material draw
detection. **Not yet implemented**: promotion-race urgency, rook-trap risk. A counting-danger
awareness heuristic (amplify the mop-up bonus as the counting-draw ply count approaches its
limit) was tried in Round 32 and came back a small, consistent regression — not merged, see
Development History. An experimental Khon/Met central-control bonus was tried and found
inconclusive (sign-flipped between seeds) — see "Central-Control Eval Experiment" below;
not merged.

---

## NNUE (implemented)

`src/nnue/features/half_ka_v2_makruk.{h,cpp}` — `HalfKAv2Makruk` feature set (channels:
W/B Pawn, Knight, Khon, Rook, Met + King buckets). `src/nnue/mknn_evaluator.{h,cpp}` — the
`MknnEvaluator` runtime, supporting three binary formats:

* **VERSION1 (MKN1)**: float32 throughout.
* **VERSION2 (MKN2)**: int16 FT (transposed weight layout for cache-friendly access), float32
  L2/L3/out.
* **VERSION3 (MKN3)**: as MKN2, plus int16 L2 weight with a per-net-calibrated scale
  (`--quantize-l2` in `tools/training/export_int16.py`). L3/out stay float32 (too small —
  32×32 and 32×1 — to be worth quantizing).

An incremental FT accumulator (`MknnAccumulator`, riding `StateInfo`) avoids full
recomputation on most moves (backward-walk + forward-replay, ~1.35 average hops in practice).
Architecture and training history are in the Development History below (Rounds 1–27).

---

## Testing

`src/makruk/test_*.cpp` + `src/makruk/build_tests.sh` (also exercised by
`tools/build_verify.sh`, the standing post-build gate — clean rebuild + full suite + a
16-case crash-trigger regression probe). Covers: move generation, legality/check/checkmate/
stalemate, counting rules, classical eval heuristics, SEE (`seeGe()`, 2165 checks against a
slow reference), NNUE incremental-accumulator bit-exactness (188 checks), and L2 quantization
tolerance (308 checks). **Note**: `build_tests.sh` compiles with `-DNDEBUG`, which strips
`assert()` — all these tests use explicit runtime checks (`std::exit(1)` on failure), not
`assert()`. Run `perft 5` from the Makruk start position separately — expect exactly
**6,223,994 nodes**; `build_verify.sh` does not run this itself.

---

## Build Policy

Build via the Makefile, **always with an explicit `ARCH=x86-64-bmi2`** (a bare `make build`
now defaults to this too, but pass it explicitly to match every benchmark/gauntlet this
project has actually used):

```bash
cd src && make clean && make -j$(nproc) ARCH=x86-64-bmi2 build
```

After any change touching build output (source, Makefile, or embedded net), run in order:
`bash src/makruk/build_tests.sh`, `bash tools/build_verify.sh`, and a manual `perft 5` check
(6,223,994 nodes). Never run compile/test work concurrently with a movetime-based gauntlet —
CPU contention distorts effective search depth within a fixed time budget and taints results
(learned the hard way in Round 25).

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

---

## Current Status (as of Round 33, 2026-10-11)

* **Embedded net**: `src/3587509696.bin` (still Round 27 — Rounds 28-31 were all
  negative/inconclusive, not deployed). Check `src/evaluate.h`'s `NnueNetDefaultName` for
  what's actually shipped at any given time, this note can drift.
* **Target**: -150 Elo vs Fairy-Stockfish-NNUE (user-set, 2026-10-10) — needs **+50 to +55
  Elo** from the current -205/-200, a single-round-sized gain by this project's own history
  (Round 21: +64; Round 27: +55/+60), not a multi-round accumulation.
* **True strength** (book-paired, equal-condition 200ms/move, vs Fairy-Stockfish-NNUE, the
  only trustworthy Elo methodology in this project's history — see "Handicap gauntlets are not
  true strength" below): progressed **-372 → -260 → -205/-200** across Rounds 18-27, then held
  there through Rounds 28-33 (all not deployed). Rounds 18-26 were engine-side fixes
  (incremental NNUE fix, two real `seeGe()` bugs, fast-math, heap-alloc elimination, L2
  quantization, a vectorization-blocking bug fix); two consecutive large nps wins there (Round
  23 +34%, Round 26 +42.7%) produced ~0 combined net Elo, closing the nps gap with
  Fairy-Stockfish-NNUE entirely but confirming nps had plateaued as a lever. Round 27 then
  pivoted to training-data quality and delivered **+55/+60 Elo, confirmed at 2 seeds**. Rounds
  28-31 each tried one of the four levers from Round 28's own analysis (data scale-up, search
  correction-history backport, blend gate/cap retune, larger NNUE L1) — **none beat Round 27**
  (28: sign-flipped/inconclusive; 29: small consistent regression -15/-22; 30: confirmed
  regression -20/-49; 31: confirmed regression -44/-69, doubling NNUE L1 on the exact Round 27
  dataset). Round 32 tried a classical-eval lever instead (counting-danger awareness) and
  measured a small regression (-23/-28) — but Round 33 then discovered this was almost
  certainly noise: `evaluate.cpp` had an unrelated pre-existing bug (predates Round 18) that
  made `makrukClassicalEval()`'s entire bare-king endgame branch dead code during real search,
  so Round 32's mechanism never actually ran. Round 33 fixed that bug but the gauntlet came
  back inconclusive (0/-30 across seeds) — not a confirmed win despite being a genuine
  correctness fix; left as an open decision (see Development History) rather than deployed.
* **Next priority**: every lever tried since Round 27 has failed to confirm a win (data,
  search correction-history, blend gate, NNUE L1, counting-danger eval, and now the
  evaluate.cpp counting-draw fix itself). Two directions remain genuinely untried: (1) a
  *narrower* blend gate (300→200ish, trusting classical more — the untested opposite of
  Round 30, cheap, no retraining needed — and now arguably more promising to try *on top of*
  Round 33's fix, since classical eval's endgame signal is finally live during search), and
  (2) the other two classical-eval ideas from the "not yet implemented" list — promotion-race
  urgency and rook-trap risk — which are mechanically unrelated to counting-danger and so
  aren't pre-judged by Round 32's result, and which now have a real chance to matter given
  Round 33's fix. Round 29's untuned correction-history constants also remain un-swept if
  revisited.
* **Closed**: a central-control classical-eval experiment (Khon/Met, see below) concluded
  inconclusive and was not merged — see Development History.
* **Unfixed**: a depth≥9 SIGSEGV in `setCheckInfo` (empty king bitboard) has never been
  reproduced despite ~1500+ invocations across two investigation rounds (10, 17); hardened
  defensively (aborts with a diagnostic instead of segfaulting) but root cause unknown.

---

## Key Methodological Findings (apply to all future rounds)

These were each learned the hard way and materially changed how this project measures things —
read before starting new tuning/benchmarking work.

* **Handicap gauntlets are not true strength.** Every Elo number from Rounds 11-17 (roughly
  +111 to +382) used a gauntlet where the opponent was capped at a lower depth/movetime. The
  first equal-condition (200ms both sides) gauntlet (Round 18) gave **-372 Elo** — a completely
  different picture. Always use equal-condition, book-paired gauntlets for absolute strength
  claims; handicap gauntlets remain usable only for fast *relative* net-vs-net comparison.
* **Single-seed gauntlet noise is large — up to 46 Elo** on the identical binary (Round 15
  follow-up investigation). Round-to-round deltas under ~40-50 Elo are not trustworthy from one
  seed; confirm at a second seed (a different one each time, e.g. 99 and 4242) before deploying
  or discarding. A sign-flip between seeds means "inconclusive," not "small effect."
* **Book-pairing shrinks but does not eliminate this noise.** `tools/books/` (from
  fairy-stockfish/books, 66,745 positions) + `gauntlet.py --book` pairs each opening across
  both colors, canceling color/first-move bias — demonstrated to shrink one net's seed-to-seed
  spread from 46 Elo to 11 Elo (Round 16) — but sampling variance from *which* book positions
  get drawn remains a real, uncontrolled noise source.
* **val_loss does not predict Elo.** Demonstrated repeatedly (Rounds 9, 10, 15, 16): the
  best-ever val_loss net (Round 10's v9) was an outright Elo regression, and blend-range
  (near-equal position) discrimination — not raw prediction accuracy — is what actually moves
  Elo, because the blend is gated to `|classical| < 300`.
* **nps improvements do not reliably translate to Elo** once search is already reasonably deep.
  Round 23 (+34% nps) and Round 26 (+42.7% nps) both landed within noise of flat. Diff-audit the
  actual bottleneck (`gprof` works in this sandbox; `perf` does not —
  `perf_event_paranoid=4`) before assuming eval speed is still the constraint.
* **`gh pr create`/`gh repo view` default to the wrong repo** (the real public upstream
  `FireFather/sf-kernel`, not this project's fork `Homi/makruk-sf`) — has caused 3 accidental
  PRs against the real upstream maintainer's repo. **Always** pass `--repo Homi/makruk-sf`
  explicitly and verify the returned PR URL's owner before reporting success. Never open a PR
  against upstream.
* **Never run CPU-competing work (builds, test suites, a second gauntlet) concurrently with a
  movetime-based gauntlet** — contention silently distorts effective search depth and produced
  a spurious -407 Elo reading in Round 25 before this was caught.
* **Background processes**: use `setsid` (not bare `nohup`+`disown`) for long-running gauntlets/
  training, with a persistent Monitor — a sandbox restart once silently killed a `nohup`+
  `disown`'d background pair mid-run.
* **Always match the reference round's exact gauntlet flags, not just movetime/book/seed** —
  omitting `--adjudication-threshold 500 --adjudication-streak 5` in Round 27's first attempt
  produced a spurious "100% draws, Elo 0" result on *both* the new and the old net (confirmed
  via a same-flags control run), because without adjudication a competent engine at equal
  200ms/move essentially never gets checkmated in Makruk — every game just plays out to a
  counting draw. The Round 26 reference log (`tools/gauntlet_out/round26_vecfix/gauntlet.log`)
  had the real flags all along; check a prior round's actual saved log before assuming which
  flags apply, don't reconstruct the command from memory/docs alone.

---

## Development History

### Training pipeline era (pre-Round-17, foundational bugs — historical reference)

PR11 added the core tooling still in use today: `tools/gauntlet.py`, `tools/selfplay/
selfplay.py`'s handicap modes, `tools/training/convert.py`/`dataset_summary.py`. A cascade of
bugs was found and fixed, each blocking meaningful net evaluation until resolved:

* Counting draw never wired into game logic → 66% stalemate rate. Fixed by connecting
  `isDraw()` to `isMakrukCountingDraw()`.
* 0% decisive self-play even with handicaps (counting-draw endgames score ≈0 regardless of
  material). Fixed with **score adjudication** (`--adjudication-threshold`/`-streak` in
  `selfplay.py`/`gauntlet.py` — a sustained ≥500cp streak after ply 20 ends the game).
  Decisive rate 0%→65-80%.
* `gauntlet.py`'s draw detection used exact `sc==0` (never true) → 61-67% spurious stalemates.
  Fixed with a tolerance-based per-engine streak check.
* NNUE was never actually loading (wrong magic-number check, silent fallback to classical) —
  *every gauntlet number before this fix measured classical eval, not NNUE.* Fixed with a
  purpose-built `MknnEvaluator`.
* Once active, NNUE was a **-471 Elo regression** vs classical, from 3 compounding bugs: wrong
  sigmoid-decode formula, CReLU/ReLU train/inference mismatch, and counting-compressed training
  labels (net never learned material scale). Fixed by correcting decode/CReLU and adding
  `--color-augment` + imbalanced-opening data; NNUE blend eventually matched then modestly beat
  (Round 8, +25 Elo) classical-only play. Blend formula since Round 7:
  `classical + clamp(nnue - classical, -100, 100)`, gate 300cp.
* **Pure NNUE (no classical blend) was never viable** in any config tried — underestimates
  material ~2× or overcorrects. Root cause: Makruk counting draw compresses teacher labels to
  ≈0 in endgames, so the net never sees unambiguous material-advantage examples. `UseCounting`
  UCI option / `--no-counting` selfplay flag lets data-gen disable counting for real
  material-scale labels — **data-generation only, never for real play/gauntlets.**
* A depth≥9 SIGSEGV surfaced while generating counting-unaware data — never confirmed
  reproducible despite ~1500 attempts across 2 rounds; hardened defensively (abort with a
  diagnostic instead of an OOB read) rather than "fixed". `selfplay.py --strong-depth` is the
  only confirmed-effective mitigation.

**Net-quality rounds 11-16**: closer-handicap self-play (opponent depth 4-5) harvested
"blend-range" (near-equal) training data far more efficiently (84-87% acceptance vs 63.5%). A
scale-up (Round 12) regressed sharply; an ablation (Round 13) isolated the closest-handicap
config (depth-b=5, nearest genuine parity) as the cause — noisier near-parity labels dilute
signal despite passing the score filter. Rounds 13-14 scaled the validated-good configs
cleanly. Round 15's apparent regression was mostly the 46-Elo single-seed noise described
below — motivated adding opening-book pairing (`tools/books/`). Round 16, the first
book-paired round, was genuinely inconclusive (sign-flipped across seeds) and not deployed.

### C++ correctness/performance era (Rounds 17-26)

* **17** (08-22): SIGSEGV re-investigated, still unreproduced. Added `tools/build_verify.sh`.
* **18** (08-22): First true equal-condition (no-handicap) test — baseline is **-372 Elo**, not
  the historical +111/+382 (all handicap-gauntlet artifacts). Root cause: `MknnEvaluator`
  recomputed its NNUE accumulator from scratch every call. Fixed with `MknnAccumulator`/
  `updateAccumulator()` (incremental, bit-exact vs from-scratch, 188 checks). -372→-359.
* **19-20** (08-22/23): Diffed against upstream, found 2 real `seeGe()` bugs — wrong Khon/Met
  attacker detection (chess-style slider union instead of the correct reverse-color/non-slider
  formula) and an attacker-check order inherited from chess's value order instead of Makruk's
  (Pawn<Met<Khon<Knight<Rook). Both fixed, 2165-check regression test (`test_see.cpp`).
  -359→-346.
* **21** (08-23): Closed most of the nps gap — GCC's auto-vectorizer left the L2 reduction loop
  scalar without float-reassociation permission (not a SIMD-width problem). Added a scoped
  fast-math subset to `mknn_evaluator.cpp`; also fixed the Makefile's bad default ARCH
  (`x86-64-modern`→`x86-64-bmi2`). nps ~40K→~150-190K (~4×). -346→-282 (**+64 Elo**, confirmed
  +90 at a 2nd seed — largest single-round gain in this project's history).
* **22** (08-29): Eliminated remaining heap allocations in `evaluate()` via fixed-size stack
  buffers. nps +6.3%. -282→-269 (+13).
* **23** (08-29): VERSION3 (MKN3) int16-quantized L2, with a load()-time overflow-safety check.
  nps +34%. Elo inconclusive (+14/+0 across 2 seeds) — deployed on nps+correctness grounds
  alone, flagged as lower-confidence than 21/22.
* **24-25** (08-29/30): `search.cpp`/`movepick.cpp`/`timeman.cpp` confirmed byte-identical to
  upstream. Tested 3 search-constant candidates (null-move R, LMR base, quiet SEE margin) — all
  within 11 Elo of baseline, all discarded.
* **26** (08-30): `gprof` profiling found eval still 83.5% of CPU time. Root cause:
  `addColumn`/`subColumn`/`refreshInto` used a runtime member (`L1_`) directly as a loop bound,
  blocking GCC auto-vectorization ("number of iterations cannot be computed"). 3-line fix
  (cache to a local `const int`). nps +42.7%, bit-exact (identical node counts). Elo flat
  (-260, delta -5) — deployed anyway (zero correctness risk). Two consecutive large nps wins
  (23+26) with ~0 combined Elo confirmed nps had plateaued as a lever.

### Central-Control Eval Experiment — concluded inconclusive, not merged (08-30)

Added `centralControl()` to `makruk_eval.cpp`: an MG-only bonus for Khon/Met on a widening
central zone (rank5 d/e, rank4 c/d/e/f, rank3 b/c/d/e/f/g), tiered by file (d/e=40cp, c/f=25cp,
b/g=12cp). Verified square-by-square via `makrukeval`; new test `testCentralControlTieredByFile`
added. Gauntlet: **inconclusive** — seed=99 gave +11 Elo, seed=4242 gave -21 Elo (sign flip) —
not deployed. The reference branch was deleted (2026-09-13, GitHub branch cleanup) — if
resumed, this section's spec (zone shape, tier values) is the starting point; the code itself
would need to be reimplemented from scratch. Any resumption should budget for a proper 2-seed
test up front.

### Round 27 — Fresh Training Data from the Post-Round-26 Engine (2026-09-13)

With nps confirmed plateaued (Round 26), pivoted to the deferred priority: regenerate training
data using the current engine (post Rounds 18-26: ~10x faster, SEE-correct) instead of
continuing to build on data from the old, much weaker engine.

**Discovery: the "validated-good" handicap configs from Rounds 11-14 no longer work.**
Regenerating at the same settings (`movetime-a=200/depth-b=4`, `movetime-a=150/depth-b=3`)
gave a 97-99% draw rate (vs historical 50-62%) and blend-range acceptance collapsed from
84-87% to 44.8% — those configs were calibrated for an engine ~10x slower than today's, so the
*absolute* numbers no longer produce the intended *relative* handicap gap. Raising the
opponent's `depth-b` barely moved acceptance (44.8%→46.4%); cutting our own `movetime-a`
(200ms→30ms) to compensate for the nps gain was far more effective (→53.1% at full scale).
**Generalizable finding: handicap configs need periodic recalibration as engine speed
changes** — a "validated-good" label doesn't survive a large enough nps jump.

Generated 3 data configs (A/C: the old settings; D: the recalibrated `movetime-a=30/
depth-b=4`; 150 games each), teacher-labeled with Fairy-SF depth=10, filtered to blend range
(30-400cp), combined with the existing base → 267,971 positions. Trained 60 epochs (standard
recipe, `--resume` exercised across a real multi-day pause/resume). Best epoch 55, val_loss
0.553515. Exported to VERSION3/MKN3.

**Gauntlet, confirmed at 2 seeds:**

| Seed | Result | Elo |
|---|---|---|
| 99 | 0W 94D 106L | **-205** |
| 4242 | 0W 96D 104L | **-200** |
| Round 26 baseline (ref) | 0W 73D 127L | -260 |

**+55/+60 Elo**, both seeds within 5 Elo of each other — outside the ~46 Elo single-seed noise
band. Deployed via PR #20. First round in a long time to beat single-digit/noise-band amounts.

**Process note**: the first gauntlet attempt this round omitted `--adjudication-threshold
500 --adjudication-streak 5` (the flags Round 26 actually used, per its saved log) and produced
a spurious 100%-draw/Elo-0 reading on *both* nets — see "Key Methodological Findings" above.
Caught via a same-flags control run before deploying on the bad reading.

### Round 28 — Scaling Up Round 27's Recipe: Inconclusive, Not Deployed (2026-09-14)

Direct follow-up to Round 27's own recommended next step (scale up the recalibrated
`movetime-a=30/depth-b=4` config, matching the Round 11→13→14 validate-then-scale pattern).
Generated 450 games (3× Round 27's Config D) at that exact config, teacher-labeled with
Fairy-SF depth=10 → 37,275 scored, filtered to blend range: **67.6% acceptance** — notably
*higher* than Round 27's 53.1%, and the largest single-round dataset addition in this
project's history (267,971→293,161 positions, +9.4%). `dataset_summary.py` found nothing
unhealthy (0.3% duplication, improved Black-win share 9.3%). Trained 60 epochs, standard
recipe. Best epoch 59, val_loss **0.555746** — slightly *worse* than Round 27's 0.553515,
despite the better acceptance rate and larger dataset (another data point for "val_loss
doesn't predict Elo," this time in the opposite direction from usual: more/better-filtered
data didn't even improve val_loss, let alone Elo).

**Gauntlet, confirmed at 2 seeds — sign-flipped, inconclusive:**

| Seed | Round 28 | Round 27 (ref) | Delta |
|---|---|---|---|
| 99 | 0W 89D 111L → -217 | -205 | -12 |
| 4242 | 0W 97D 103L → -198 | -200 | +2 |

The two seeds disagree in direction (-12 vs +2) — per the standing decision criteria, this is
**inconclusive, not a small effect**, unlike Round 27 where both seeds agreed within 5 Elo.
**Not deployed.** Reverted cleanly (`src/evaluate.h` never left `3587509696.bin` on the
tracked branch; the working branch was discarded unpushed). Checkpoint
`tools/training/makruk_round28.pt` (val_loss 0.555746) kept on disk, gitignored, for possible
future ablation — same treatment as Round 12/15/16.

**Takeaway**: a higher blend-range acceptance rate and a bigger dataset addition do not
automatically transfer to Elo — consistent with, and now the second reminder of, this
project's core finding that raw data-quality metrics (acceptance %, duplication %, val_loss)
are necessary-but-not-sufficient; the gauntlet is still the only real signal. Round 27's win
may have been closer to the ceiling of what this specific config/scale can deliver, or Round
28 may simply be within the "further recalibration has diminishing returns" territory flagged
back in Round 27's own calibration probes. Either way, blindly scaling a working recipe is not
guaranteed to keep working — this doesn't invalidate Round 27's result (still deployed,
still 2-seed-confirmed), it just means the next attempt at this direction should try a
different lever (different config mix, or one of the other three options from the Round 27
follow-up analysis: blend gate/cap re-tuning, larger NNUE L1, or backporting newer Stockfish
search techniques) rather than a third straight data-volume scale-up.

### Round 29 — Pawn Correction History Backport: Small Regression, Not Deployed (2026-09-14)

Took the third lever from Round 28's list: backported a static-eval bias-correction mechanism
("correction history") that modern Stockfish added around 2023, missing entirely from this
codebase's `search.cpp` (which has `continuationHistory`/`captureHistory` and singular
extensions — ~2020-2021-era techniques — but nothing from the correction-history generation).

**Scoping, forced by a real gap found during investigation**: this codebase has `pawnKey()`
(confirmed correctly promotion-aware via a subtle double-XOR-cancellation in `doMove`) but **no
`materialKey()`/`minorPieceKey()`/`nonPawnKey()` at all** — real Stockfish's correction history
normally uses all four. Building the other three from scratch would mean new incremental
Zobrist-key infrastructure touching Makruk's promotion semantics — the same class of subtle-bug
risk that caused the `seeGe()` bugs (Round 19-20). Scoped this round to **pawn correction
history only**, reusing `pawnKey()` read-only; material/minor-piece/non-pawn correction and
`qsearch()` integration explicitly deferred. Applied the correction only to the local `eval`
variable (never `ss->staticEval`, never persisted to TT) at both TT-hit and no-TT-hit branches
in `search()` plus the ProbCut margin — zero changes to the mate-range-sensitive TT-store path
or already-tuned improving/razoring/extension heuristics.

New `PawnCorrectionHistory` table (`Statistics<int16_t,1024,COLOR_NB,16384>`, reusing the
existing history-table template family), updated at the end of each node
(`(bestValue-ss->staticEval)*depth`, clamped, guarded against `excludedMove`/in-check/mate-range
results) and consulted via a new `Search::correctedStaticEval()`. **All constants are untuned
first-pass values** — no verifiable upstream source to copy exactly, so this was as much a test
of "does the mechanism help with reasonable constants" as "is the technique worth having."

New test `test_corrhist.cpp` (16 checks) surfaced a real, pre-existing, unrelated finding while
being written: **a directly-constructed `Thread` object (bypassing `ThreadPool`) crashes even
on this project's unmodified code** — confirmed empirically by reproducing on pristine `main`.
`Thread`'s startup depends on global state (UCI `Options`) that only `ThreadPool::set()`
establishes correctly. The test was written to go through the real `ThreadPool` instead
(matching `main.cpp`'s own init order), not flagged as a bug to fix this round (out of scope,
noted for awareness — nothing in normal engine operation ever constructs a bare `Thread`).

**Gauntlet, confirmed at 2 seeds — small but consistent regression:**

| Seed | Round 29 | Round 27 (ref) | Delta |
|---|---|---|---|
| 99 | 0W 88D 112L → -220 | -205 | -15 |
| 4242 | 0W 87D 113L → -222 | -200 | -22 |

Unlike Round 28 (sign-flipped, genuinely inconclusive), both seeds here **agree in direction**
(both negative) and land within 2 Elo of each other (-220 vs -222) — tighter agreement than
Round 27's own +55/+60 spread. This reads as a real, if small, regression rather than noise,
though both deltas (-15/-22) are individually inside the ~46 Elo single-seed noise band.
**Not deployed** — reverted cleanly (this was a pure `search.cpp`/`movepick.h`/`thread.{h,cpp}`
change, no net involved; `src/evaluate.h` was never touched). Kept on branch
`pawn-correction-history`, committed locally, not merged.

**Takeaway**: this is evidence *against these specific untuned constants*, not proof the
technique can't work for Makruk-SF. The `Grain`/`MaxCorrection`/`Divisor`/`UpdateCap` values
were picked by analogy to existing tables' bonus-to-divisor ratios, not tuned — a plausible
first follow-up (if revisited) is a Round-24-25-style constant sweep before concluding the
technique itself is a dead end, though per this project's "small changes, don't over-invest in
an unproven direction" philosophy this hasn't been scheduled.

### Round 30 — Blend Gate Widened 300→450cp: Confirmed Regression, Not Deployed (2026-09-14)

The other constant-tuning lever flagged since Round 28: `evaluate.cpp`'s classical/NNUE blend
gate (`|classical|<300cp` triggers NNUE consultation) and cap (delta clamped to ±100cp) were
last tuned in Round 7, against a net several generations weaker than Round 27's. Round 7 itself
found cap-widening (tested ±150/±200) gave no gain because the NNUE delta rarely even
approaches ±100cp — so this round targeted the gate instead, widening it to 450cp (cap left
unchanged at 100) to let the now-substantially-better Round 27 net participate in a wider range
of positions.

Single-line change (`evaluate.cpp`, the gate constant only), verified via the full test suite +
`build_verify.sh` + `perft 5` (unaffected by construction — a pure eval-formula constant, no
logic/structure change).

**Gauntlet, confirmed at 2 seeds — clear regression:**

| Seed | Round 30 (gate=450) | Round 27 (ref) | Delta |
|---|---|---|---|
| 99 | 0W 86D 114L → -225 | -205 | -20 |
| 4242 | 0W 77D 123L → -249 | -200 | **-49** |

Both seeds agree in direction (negative), and the seed=4242 delta (-49) clears the ~46 Elo
single-seed noise band outright — a confirmed, not merely suggestive, regression. **Not
deployed** — reverted cleanly (gate restored to 300, matching Round 27's tracked state exactly;
no net involved).

**Takeaway**: reaffirms Round 6/7's original finding that the NNUE net still underestimates
material relative to classical eval, even after Round 27's data-quality improvements — widening
the gate exposes more positions (specifically those in the 300-450cp range, where classical is
usually correctly identifying a real, clear advantage) to NNUE's calibration weakness, actively
hurting rather than helping. This is a meaningfully different failure mode from Round 29's small
regression: here the mechanism (blend formula) is unchanged and well-understood, and the result
cleanly confirms a known, specific weakness rather than surfacing a new, ambiguous one. A
*narrower* gate (trusting classical more, not less) is the untested opposite direction and
remains a candidate, though the net's known material-underestimation problem would need fixing
first (more/better training data in the 300+cp range, or explicit calibration) before a wider
gate has any chance of working — simply moving the constant further in either direction without
addressing that root cause is unlikely to help.

### Round 31 — NNUE L1 Doubled 512→1024: Confirmed Regression, Not Deployed (2026-10-10)

The last of the four levers identified in Round 28's analysis. Raised `MknnMaxL1` in
`src/nnue/mknn_accumulator.h` 512→1024 (prerequisite, verified backward-compatible/bit-exact
with the Round 27 net before touching training), then bumped `train.py`'s `L1` constant
512→1024 (L2/L3 held at 32 to isolate L1 as the only architecture variable). Trained on
`train_round27_combined.tsv` — the *exact same dataset* Round 27 used — so any Elo change is
attributable to capacity alone, not data.

Training ran 60 epochs, survived a real multi-day pause/resume (SIGTERM at epoch 47 for a
machine shutdown, resumed cleanly from `makruk_round31_resume.pt`). Best epoch 58, val_loss
**0.553548** — essentially identical to Round 27's 0.553515 (L1=512), again consistent with
this project's "val_loss doesn't predict Elo" finding. Exported to VERSION3/MKN3; the
int16-L2 overflow-safety check (Round 23) passed with headroom 1.0× (worst-case accumulator
sum 1,073,416,192 against a 1,073,741,824 budget) — tight by design (the auto-calibration
picks the highest-precision scale the safety margin allows), not a defect. Full verification
gate (`build_tests.sh`, `build_verify.sh`, `perft 5` = 6,223,994 nodes exact) passed clean.

**Gauntlet, confirmed at 2 seeds — clear regression:**

| Seed | Round 31 (L1=1024) | Round 27 (ref, L1=512) | Delta |
|---|---|---|---|
| 99 | 0W 77D 123L → -249 | -205 | -44 |
| 4242 | 0W 70D 130L → -269 | -200 | **-69** |

Both seeds agree in direction (negative), and the seed=4242 delta (-69) clears the ~46 Elo
single-seed noise band outright — a confirmed regression, not noise. **Not deployed** —
`evaluate.h` reverted to `3587509696.bin` (Round 27), rebuilt and re-verified
(`build_verify.sh` + `perft 5` exact) before reporting. The L1=1024 infra changes
(`MknnMaxL1` cap, `train.py`'s `L1` constant) remain committed on branch `nnue-l1-1024`, not
merged to main's net-selection state. Checkpoint `tools/training/makruk_round31.pt` (val_loss
0.553548) kept on disk, gitignored, for possible future ablation — same treatment as
Round 12/15/16/28.

**Takeaway**: doubling capacity on the identical dataset did not help — more capacity without
more/better data just gives the net more room to fit the same underlying signal, which per
Round 27/28's findings is already close to this data's ceiling. This completes the set of all
four Round-28-identified levers (data scale-up: inconclusive; correction history: small
regression; blend gate: confirmed regression; NNUE L1: confirmed regression) — **none has beaten
Round 27**, suggesting Round 27's net is a local optimum that small variations around it
(more data at the same quality, more capacity, search-side bias correction, wider blend trust)
do not improve on. The two remaining untried directions are a *narrower* blend gate (trusting
classical more — the untested opposite of Round 30) and new classical-eval heuristics
(promotion-race urgency, rook-trap risk, counting-danger awareness) that attack the net's
known material-underestimation weakness from the classical side rather than the net itself.

### Round 32 — Classical Eval: Counting-Danger Awareness — Small Regression, Not Deployed (2026-10-10)

The first of the "not yet implemented" classical-eval ideas flagged since Round 28/31 (the
other two — promotion-race urgency, rook-trap risk — are mechanically unrelated and untried).
Makruk's counting rule compresses a bare-king endgame to a draw once the weaker side's claimed
ply count reaches its limit (`src/makruk/makruk_counting.h`); the existing mop-up bonus
(push weak king to the edge, close own king) is otherwise flat regardless of how close that
cliff is, so search only "discovers" the draw at the exact leaf where the limit is hit, with
no earlier pressure to make faster progress.

Added `applyCountingUrgency()` in `src/makruk/makruk_eval.cpp`: reads the live
`pos.state()->makrukCounting` (reusing the already-tested counting state, no new
infrastructure) and scales the mop-up bonus up to +50% as `count` approaches `limit`
(`mg + mg*count/(2*limit)`, same for eg), applied at both existing mop-up call sites
(`mopupBonus()` and `kbkMopup()`) in both `makrukClassicalEval()` and `makrukEvalReport()`.
Two new tests (`testCountingUrgencyAmplifiesMopup`, `testCountingUrgencyNoOpWhenInactive`)
directly manipulate `cs.count` the same way `test_counting.cpp` already does, isolating the
effect from position changes. Full verification gate passed clean (`build_tests.sh` 19/19,
`build_verify.sh`, `perft 5` = 6,223,994 nodes exact); also spot-checked end-to-end via real
UCI moves (mop-up bonus visibly grew from 90→91 after 30 plies of shuffling in a KRK position).

Found and noted, but out of scope to fix, a pre-existing unrelated cosmetic bug while
investigating: the `makrukcount` UCI debug command constructs a fresh default
`MakrukCountingState` (`uci.cpp` line ~131) instead of reading `pos.state()->makrukCounting`,
so it always prints `active: no` regardless of the real state — display-only, does not affect
real eval/search, which read the correct live state directly.

**Gauntlet, confirmed at 2 seeds vs Fairy-Stockfish-NNUE — small, consistent regression:**

| Seed | Round 32 (counting urgency) | Round 27 (ref) | Delta |
|---|---|---|---|
| 99 | 0W 85D 115L → -228 | -205 | -23 |
| 4242 | 0W 85D 115L → -228 | -200 | -28 |

Both seeds landed on an *identical* result (0W 85D 115L) and agree in direction — tighter
agreement than Round 27's own confirming spread, reading as a real small regression rather
than noise, even though both individual deltas (-23/-28) sit inside the ~46 Elo single-seed
band — the same evidentiary pattern as Round 29's correction-history finding. **Not deployed**
— the heuristic is reverted from main's eval path; kept on branch `classical-eval-new-
heuristics`, committed locally, not merged.

**Takeaway**: evidence against *this specific mechanism/constant* (linear up-to-+50%
amplification of an already-existing bonus), not proof that counting-aware eval can't help at
all — plausible explanations include the amplification over-weighting positional distance
(corner/king-closeness) relative to concrete tactical progress, or simply mattering too little
within a 200ms search horizon that rarely reaches deep enough to approach the real limit. The
other two classical-eval ideas (promotion-race urgency, rook-trap risk) are mechanically
unrelated to this one and remain untried.

**Correction (discovered in Round 33, below)**: this takeaway should be read with a major
caveat. `evaluate.cpp` had an unrelated pre-existing bug that made `makrukClassicalEval()`'s
entire bare-king endgame branch — mop-up bonus, KBK handling, and this round's urgency
amplification — dead code during real search (see Round 33). The -23/-28 Elo measured here was
almost certainly pure build-to-build noise on an inert code path, not a real effect of the
mechanism tested. The mechanism itself has not actually been evaluated under real search yet.

### Round 33 — Fix: `evaluate.cpp` No Longer Blinds Entire Counting-Eligible Endgames — Inconclusive, Not Deployed (2026-10-11)

While investigating Round 32's suspiciously-identical-across-seeds result, found that
`evaluate.cpp` has had this since the very first Makruk-eval commit (predates Round 18 — every
Elo number in this project's history was measured with it in place):

```cpp
if (pos.state()->makrukCounting.active)
  return VALUE_DRAW;
```

`search.cpp` already checks `pos.isDraw(ss->ply)` — which covers the actual counting-draw via
`isMakrukCountingDraw()` — before calling `evaluate()` on any non-root node (confirmed by
reading `search.cpp` directly). But `makrukCounting.active` is true for the *entire* duration
of a counting-eligible endgame (up to 128 plies for a lone-knight ending), not just the
terminal draw ply. The result: every node throughout every pawnless bare-king endgame got a
flat `VALUE_DRAW`, making `makrukClassicalEval()`'s mop-up/KBK endgame logic — listed in
CLAUDE.md as "Implemented" — **dead code during real search**. It only ever ran via direct
unit-test calls (bypassing `evaluate()`) and the `makrukeval` debug command. The original
commit message's own stated intent ("Counting-draw positions still short-circuit to
VALUE_DRAW before either eval runs") confirms this was meant to catch the terminal draw only —
the implementation just used the wrong predicate the whole time.

Fix: changed the condition to `isMakrukCountingDraw(pos.state()->makrukCounting)`, the
predicate that actually means "this position is a confirmed draw" (internally guards on
`cs.active && cs.limit > 0 && cs.count >= cs.limit`, so it's safe when inactive). One new
regression test (`testEvaluateUsesRealEvalBeforeCountingLimit`) confirms `Eval::evaluate()`
now returns real material/mop-up signal early in a KRK endgame and exactly `VALUE_DRAW` right
at the limit. Full verification gate passed (`build_tests.sh` 20/20, `build_verify.sh`,
`perft 5` exact).

Checked whether the fix even had a chance to show up in this gauntlet format before trusting
the result: 60/200 (seed 99) and 45/200 (seed 4242) games exceeded 80 plies, so long/bare-king-
capable endgames were not rare in this sample.

**Gauntlet, 2 seeds vs Fairy-Stockfish-NNUE — flat to slightly negative, inconclusive:**

| Seed | Round 33 (fix applied) | Round 27 (ref) | Delta |
|---|---|---|---|
| 99 | 0W 94D 106L → -205 | -205 | **0** |
| 4242 | 0W 84D 116L → -230 | -200 | -30 |

One seed landed exactly on the Round 27 baseline, the other regressed by 30 — this doesn't meet
this project's bar for a confirmed result in either direction (needs both seeds to agree and at
least one to clear the ~46 Elo noise band). **Not deployed as an Elo improvement** — kept on
branch `classical-eval-new-heuristics`, committed locally, not merged, pending a decision on
whether to keep it anyway on correctness grounds (see Takeaway).

**Takeaway**: this is a case this project hasn't hit before — a change motivated by provable
*correctness* (the implementation didn't match its own documented intent) rather than a tuning
guess, that the gauntlet doesn't confirm as an Elo win. Two honest readings: (1) per this
project's consistent "the gauntlet is the only real signal" philosophy, an unconfirmed result
is an unconfirmed result regardless of how principled the reasoning was, and it should sit
un-merged like any other inconclusive round; (2) the fix removes a real logic error with zero
correctness risk (full test suite + perft unaffected) and the gauntlet's silence here may
reflect the specific methodology (200ms, adjudicated at 500cp) rather than the fix being
neutral in general — bare-king conversion quality plausibly matters more in longer games,
against weaker/non-adjudicated opponents, or for training-data generation (classical eval
feeds teacher-label quality) than it does in this particular fast equal-condition format. Left
as an open decision rather than resolved unilaterally.

---

## Benchmark History (true equal-condition Elo vs Fairy-Stockfish-NNUE, book-paired, 200ms both sides)

| Milestone | Elo | Delta | Notes |
|---|---|---|---|
| Round 18 baseline (first equal-condition test) | -372 | — | every prior "+111 to +382" number was a handicap-gauntlet artifact |
| Round 18 (incremental NNUE accumulator fix) | -359 | +13 | |
| Round 19-20 (two real `seeGe()` bugs fixed) | -346 | +13 | |
| Round 21 (scoped fast-math, ~4× nps) | -282 | +64 | confirmed at 2nd seed (+90 there); largest single-round gain |
| Round 22 (heap-allocation elimination) | -269 | +13 | |
| Round 23 (VERSION3 int16 L2 quantization, +34% nps) | ~-254 (pooled) | ~+15 | Elo gain not reliably confirmed (sign-inconsistent across seeds); deployed on nps+correctness grounds |
| Round 26 (vectorization fix, +42.7% nps) | -260 | -5 (flat) | nps gap with Fairy-SF-NNUE now fully closed; deployed (zero risk) |
| Round 27 (fresh training data, recalibrated handicaps) | -205/-200 | **+55/+60** | confirmed at 2 seeds (5 Elo apart); first round in a long time to beat single-digit/noise |
| Round 28 (3× scale-up of Round 27's recipe) | -217/-198 | -12/+2 | sign-flipped across seeds — inconclusive, not deployed; net stayed on Round 27 |
| Round 29 (pawn correction history, untuned constants) | -220/-222 | -15/-22 | both seeds agree (negative), small consistent regression — not deployed |
| Round 30 (blend gate widened 300→450cp) | -225/-249 | -20/-49 | both seeds agree (negative), -49 clears the noise band — confirmed regression, not deployed |
| Round 31 (NNUE L1 doubled 512→1024, same dataset) | -249/-269 | -44/-69 | both seeds agree (negative), -69 clears the noise band — confirmed regression, not deployed; completes all 4 of Round 28's levers, none beat Round 27 |
| Round 32 (classical eval: counting-danger awareness) | -228/-228 | -23/-28 | both seeds landed identical (0W 85D 115L) — later found to be dead code during real search (see Round 33), so this was almost certainly noise, not a real effect |
| Round 33 (fix: `evaluate.cpp` no longer blinds whole counting-eligible endgames) | -205/-230 | 0/-30 | one seed flat, one down 30 — inconclusive by this project's bar; correctness fix, not deployed as an Elo win |

Handicap-gauntlet numbers from Rounds 11-16 (roughly +111 to +382 Elo) are **not comparable**
to this table — different methodology, not true strength. See "Key Methodological Findings."

---

## Net Archive (most recent only; superseded nets are gitignored/untracked, kept on disk where noted)

* `src/3587509696.bin` — Round 27 VERSION3/MKN3 (int16 L2), fresh training data from the
  post-Round-26 engine, Elo -205/-200 (2-seed confirmed, +55/+60 over Round 26) — **currently
  embedded** (verify against `src/evaluate.h`, this can drift)
* `tools/training/makruk_round28.pt` — Round 28 checkpoint (3× scale-up of Round 27's recipe),
  val_loss 0.555746, Elo -217/-198 (2-seed, sign-flipped/inconclusive), not deployed,
  gitignored, kept for potential future ablation
* `tools/training/makruk_round31.pt` — Round 31 checkpoint (NNUE L1 doubled 512→1024, Round
  27's exact dataset), val_loss 0.553548, Elo -249/-269 (2-seed confirmed regression), not
  deployed, gitignored, kept for potential future ablation (e.g. pairing L1=1024 with a larger
  L2/L3 instead of leaving them at 32)
* `src/3769833465.bin` — Round 23 VERSION3/MKN3 (int16 L2), archived
* `src/2020277415.bin` — Round 14 net, MKN2 int16, L1=512 — same underlying weights as
  3769833465.bin before L2 quantization, archived
* `src/150547052.bin` — Round 16 net (first book-paired round), inconclusive, untracked
* `tools/training/makruk_round12.pt`, `makruk_round15.pt` — regression/noise-affected
  checkpoints kept for potential future ablation, gitignored

Earlier nets (v1 through v9, Rounds 1-10) are fully superseded and not individually listed
here — see git history / the Development History section above for what each round changed.
