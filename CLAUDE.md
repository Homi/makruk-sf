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

## Current Status (as of Round 44, 2026-11-01)

* **Embedded net**: `src/3587509696.bin` (still Round 27 — Rounds 28-31 were all
  negative/inconclusive, not deployed). Check `src/evaluate.h`'s `NnueNetDefaultName` for
  what's actually shipped at any given time, this note can drift.
* **Target**: -150 Elo vs Fairy-Stockfish-NNUE (user-set, 2026-10-10) — needs **+50 to +55
  Elo** from the current -205/-200, a single-round-sized gain by this project's own history
  (Round 21: +64; Round 27: +55/+60), not a multi-round accumulation.
* **True strength** (book-paired, equal-condition 200ms/move, vs Fairy-Stockfish-NNUE, the
  only trustworthy Elo methodology in this project's history — see "Handicap gauntlets are not
  true strength" below): progressed **-372 → -260 → -205/-200** across Rounds 18-27, then held
  there through Rounds 28-44 (all not deployed). Rounds 18-26 were engine-side fixes
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
  so Round 32's mechanism never actually ran. Round 33 fixed that bug — gauntlet inconclusive
  (0/-30) but **kept anyway on correctness grounds** (resolved 2026-10-11). Round 34 then tried
  narrowing the blend gate (300→200, the untested opposite of Round 30) on top of Round 33's
  fix and also got a small, consistent regression (-15/-22) — both gate directions now
  regress, closing out blend-gate tuning as a lever. Round 35 then tried the first of the two
  remaining classical-eval ideas — promotion-race urgency — and got all **three seeds
  positive** (+7/+25/+29, avg +20) — the strongest multi-seed signal of any lever since
  Round 27, though no single seed clears the noise band, so it's kept but not yet "confirmed"
  by the project's strict single-round bar. Round 36 then tried the last classical-eval idea —
  rook-trap risk — stacked on top of Round 35, hoping two small positives would compound.
  Instead it pulled both seeds *down* relative to Round 35 alone (+7→0, +25→-25) — a net
  negative, reverted cleanly. This closes out all three "not yet implemented" classical-eval
  ideas (counting-danger, promotion-race, rook-trap); only promotion-race urgency (Round 35)
  showed a positive signal, and it stands alone. Round 37 then tried a broader, more standard
  technique not on that original list — a general piece-mobility *bonus* (Knight/Khon/Rook/
  Met) stacked on Round 35 — and also regressed relative to Round 35 alone (-30/-37, both
  seeds consistently negative). Reverted. Round 38 then completed the full 4-table correction
  history Round 29 had deferred (new materialKey()/minorPieceKey()/nonPawnKey() Zobrist
  infrastructure, deliberately full-recompute rather than incremental XOR, validated via 1230+
  random-walk cross-checks before ever touching search) — also stacked on Round 35, and
  regressed the *most* of all three attempts (-60/-47, both seeds). Reverted completely (10
  files back to pre-Round-38 state). Round 39 then tried a different kind of change on Round
  35 itself — not adding a new term, but doubling the magnitude of the one term already
  working — and got a clean sign-flip (+19 at seed 99, -58 at seed 4242) — inconclusive, not
  adopted, reverted to the original 1× magnitude. Rounds 40-41 then re-tested the blend gate
  (narrow 200, wide 450) with Round 35 active, since promotion-urgency scoring could plausibly
  shift which positions fall inside the gate range — a genuine untested interaction. Both
  directions regressed again (-27/-45 and -43/-37 vs Round 35 alone), matching Rounds 30/34's
  original sign exactly — the interaction doesn't change the conclusion. Both reverted to
  gate=300.
* **Next priority**: Round 44 (2026-11-01) tested the teacher-depth-vs-data-volume trade-off
  (depth=8 teacher labeling, ~4x Round 42's self-play volume) — also a small consistent
  regression (-12/-33 vs Round 27), and roughly flat vs Round 42 itself (+8/-3), so trading
  depth for volume at this scale didn't clearly help *or* hurt relative to Round 42's own
  depth=10/smaller-volume recipe. This closes out the "two previously unexamined NNUE/
  training levers" identified after Round 43 (architecture scaling, closed in Round 43;
  teacher-depth/volume trade-off, closed here) — **both have now failed to beat Round 27**.
  Net stays on Round 27. The next genuinely untried lever is a training-objective change
  (WDL-blended loss instead of pure CP-regression — motivated by "val_loss never predicts
  Elo" holding across 8 independent rounds now). Lower-priority options, none a quick win:
  (1) further magnitude tuning of Round 35's promotion-urgency constant; (2) Round 29's
  untuned pawn-only correction-history constants, un-swept if revisited; (3) a deeper, riskier
  rook-trap-risk version that checks actual capturability instead of raw mobility.
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
feeds teacher-label quality) than it does in this particular fast equal-condition format.

**Resolved (2026-10-11)**: user decided to keep the fix on correctness grounds — it matches
the original code's own documented intent, carries zero correctness risk, and an unconfirmed
gauntlet result doesn't make a real logic error "correct to leave in."

### Round 34 — Blend Gate Narrowed 300→200cp (on top of Round 33's fix): Small Regression, Not Deployed (2026-10-11/16)

The untested-opposite direction of Round 30's gate-widening regression, tried now on top of
Round 33's fix (classical eval's bare-king endgame branch is finally live during search, so
this seemed like a more meaningful time to retry gate tuning than before). Single-line change
(`evaluate.cpp`, gate constant only), verified via the full test suite + `build_verify.sh` +
`perft 5`.

Session was interrupted mid-run for a machine shutdown after seed=99 completed but seed=4242
had only reached 23/200 games; `gauntlet.py` has no `--resume` (unlike `train.py`), so the
partial seed=4242 run was discarded and rerun from scratch next session after re-verifying the
build matched the committed WIP state.

**Gauntlet, confirmed at 2 seeds — small, consistent regression:**

| Seed | Round 34 (gate=200) | Round 27 (ref) | Delta |
|---|---|---|---|
| 99 | 0W 88D 112L → -220 | -205 | -15 |
| 4242 | 0W 87D 113L → -222 | -200 | -22 |

Both seeds agree in direction (negative) and land within 2 Elo of each other — tighter
agreement than Round 27's own confirming spread, reading as a real small regression rather
than noise (same evidentiary pattern as Round 29's correction-history finding; by
coincidence, the exact same absolute Elo values and deltas as Round 29, -220/-222 and
-15/-22). **Not deployed** — gate reverted to 300 (matching Round 27's tracked state exactly),
rebuilt and re-verified. Kept on branch `classical-eval-new-heuristics`, not merged.

**Takeaway**: combined with Round 30 (wider gate, 450cp, also a regression — more clearly
confirmed there, -20/-49), both directions of gate-tuning now regress, even with Round 33's
fix applied underneath. 300cp looks like a genuine local optimum for this gate, not an
artifact of the net's known material-underestimation weakness being newly exposed by a wider
gate (Round 30's original theory) — narrowing it (trusting classical *more*) should have been
safe under that theory and wasn't. This closes out blend-gate tuning as a lever for now; the
two classical-eval ideas that remain genuinely untried are promotion-race urgency and
rook-trap risk, both still live candidates and, like Round 32's mechanism, now have a real
chance to matter during search given Round 33's fix.

### Round 35 — Classical Eval: Promotion-Race Urgency — Promising but Not Confirmed (2026-10-16)

The first of the two remaining classical-eval ideas, now with a real chance to matter during
search given Round 33's fix (bare-king endgame logic is finally live). Makruk pawns promote
to Met after only 3 ranks of advance (relativeRank 2 start → 5 promotion), so a passed pawn
one step from promotion is usually close to winning a full Met's worth of material — but
`PawnPST`'s existing rank-4 (one-step-from-promotion) eg values are actually slightly
*negative*, giving no urgency signal at all.

Added `promotionRaceUrgency()` in `makruk_eval.cpp`: reuses Stockfish's existing, already-
tested passed-pawn bitboard helpers (`passedPawnSpan()`, `relativeRank()` from `bitboard.h`/
`types.h` — present but previously unused by classical eval) rather than inventing new
infrastructure. A tiered bonus (`PromotionUrgency[]`, indexed by `relativeRank - 2`: `S(0,0)`
at the start rank, `S(0,5)` two steps out, `S(15,35)` one step from promotion) applies only to
pawns with no enemy pawn in their `passedPawnSpan` (the standard "passed pawn" definition —
deliberately not attempting full move-by-move unstoppability proof, which would carry the
same subtle-bug risk class as the `seeGe()` bugs from Rounds 19-20). Wired into both
`makrukClassicalEval()` and `makrukEvalReport()` (new "promotion race" report line). Two new
tests isolate the effect cleanly: a passed vs. blocked runner on otherwise PST-identical
positions (black pawn on c-file vs. mirror-equal f-file, same rank) at one-step-from-promotion
(must differ) and at the start rank (must be identical, confirming the tiering gates
correctly). Full verification gate passed (`build_tests.sh` 22/22, `build_verify.sh`,
`perft 5` exact); spot-checked via `makrukeval` on both test positions, matching expected
values exactly (15/35 vs 0/0).

**Gauntlet, 2 seeds vs Fairy-Stockfish-NNUE — both positive, not yet confirmed:**

| Seed | Round 35 (promotion-race urgency) | Round 27 (ref) | Delta |
|---|---|---|---|
| 99 | 0W 97D 103L → -198 | -205 | **+7** |
| 4242 | 0W 107D 93L → -175 | -200 | **+25** |

Both seeds landed positive — the first same-sign-positive result at both seeds since Round 27
itself (Round 28 was sign-flipped; every other round since has been flat or negative at both).
But neither delta individually clears the ~46 Elo single-seed noise band, so by this
project's own standing bar (both seeds agree *and* at least one clears the band) this was
not yet a confirmed win on 2 seeds alone — prompting a third-seed test (user's call) rather
than stacking with rook-trap risk immediately.

**Third seed (777), with its own same-seed Round 27 baseline** (main's clean pre-Round-33
state, built and gauntleted fresh at seed=777 for a fair comparison, since baseline Elo
itself varies by seed — Round 27 itself scored -205 at seed 99 but -200 at seed 4242):

| Seed | Round 35 | Round 27 (same-seed baseline) | Delta |
|---|---|---|---|
| 777 | 0W 90D 110L → -215 | 0W 79D 121L → **-244** | **+29** |

**All three seeds now positive** (+7, +25, +29; average +20) — a materially stronger signal
than 2/2, even though no single seed clears the noise band: if this were pure noise around a
true zero effect, 3-for-3 same-direction has roughly 1-in-8 odds by chance alone. **Decision:
kept** on branch `classical-eval-new-heuristics`, committed locally, not merged to main. Not
yet "confirmed" by this project's strict single-round bar, but the strongest multi-seed signal
of any lever tried since Round 27 itself.

**Takeaway**: unlike every classical-eval/search-constant attempt since Round 27 (correction
history, blend gate × 2, counting-danger), this is the first one that didn't land flat or
negative across repeated testing. Still far short of the +50/+55 Elo needed to reach the -150
target on its own (even the 3-seed average of +20 falls short), but it's real evidence that
classical-eval heuristics can help now that Round 33's fix makes the bare-king endgame branch
live.

### Round 36 — Classical Eval: Rook-Trap Risk Stacked on Round 35 — Regression, Reverted (2026-10-16)

The last "not yet implemented" classical-eval idea, tried by stacking it on top of Round 35's
kept promotion-race urgency to test whether two small positives compound. Added
`rookTrapRisk()` to `makruk_eval.cpp`: a tiered penalty (`S(-60,-40)` at mobility 0, `S(-25,
-15)` at mobility 1) for a rook with very few reachable squares, computed via
`attacksBb<ROOK>(sq, occupied) & ~ownPieces` — reusing the engine's existing, already-tested
magic-bitboard sliding-attack generator rather than attempting a full move-by-move
"is it actually capturable" proof (deliberately scoped away from that `seeGe()`-class bug
risk, same reasoning as Round 35). Three new tests verified the exact tiered penalty via the
`makrukEvalReport()` string directly (isolating the component cleanly from any PST/material
confound). Full verification gate passed clean (`build_tests.sh` 25/25, `build_verify.sh`,
`perft 5` exact).

**Gauntlet, 2 seeds vs Fairy-Stockfish-NNUE — stacking made things worse, not better:**

| Seed | Round 36 (rook-trap + promotion-race) | Round 35 alone (ref) | Round 27 (ref) | Delta vs R27 |
|---|---|---|---|---|
| 99 | 0W 94D 106L → -205 | -198 | -205 | **0** |
| 4242 | 0W 86D 114L → -225 | -175 | -200 | **-25** |

Adding rook-trap risk pulled *both* seeds down relative to Round 35 alone (+7→0 at seed 99,
+25→-25 at seed 4242) — the opposite of compounding. This specific mechanism is a net
negative on its own, not merely neutral. **Reverted cleanly**: `rookTrapRisk()`, its call
sites, and its three tests all removed; rebuilt and re-verified
(`build_verify.sh` + `perft 5` exact) to confirm this restores exactly Round 35's tested
state (promotion-race urgency alone, no rook-trap component).

**Takeaway**: closes out the "not yet implemented" classical-eval list — all three ideas
(counting-danger, promotion-race urgency, rook-trap risk) have now been tried; only
promotion-race urgency (Round 35) showed a positive signal, and it stands alone, not
stacked. A plausible explanation for why mobility-based trap risk hurts rather than helps:
penalizing *any* low-mobility rook regardless of whether it's actually in danger likely
discourages legitimately strong rook placements (e.g., a rook temporarily boxed in while
supporting an attack, or one guarding a key square) at least as often as it catches real
danger — the "deliberately not proving actual capturability" scoping decision that kept this
round low-risk to implement may be exactly why it doesn't work as a *signal*, since mobility
alone doesn't distinguish a genuinely endangered rook from a quietly well-placed one. Round
35 remains the standing candidate; a future revisit of rook-trap risk would need to add the
"is it actually attacked" condition this round deliberately avoided, accepting the
higher implementation-risk tradeoff that was sidestepped here.

### Round 37 — Piece Mobility Bonus Stacked on Round 35 — Regression, Reverted (2026-10-16)

User asked directly whether any lever remained after Round 36 closed out the classical-eval
"not yet implemented" list. Proposed and tried a broader, more standard technique not on that
original list: a general per-square mobility *bonus* (reward, not penalty) for Knight, Khon,
Rook, and Met — distinct from Round 36's reverted `rookTrapRisk()` (which only penalized
near-zero mobility as a danger signal). Per-square values scaled roughly inversely to each
piece type's maximum possible mobility in Makruk (Khon/Met are short-range step pieces, max
5/4 squares; Knight max 8; Rook the only slider, max 14) so each piece type's maximum bonus
lands in a similar ~25-40cp band. Correctly used the color-dependent `khonAttacksBb(c, sq)`
for Khon (its forward direction differs by color; the color-blind `pseudoAttacks[BISHOP]`
union would have been wrong). Five new tests verified exact per-piece-type values via the
`makrukEvalReport()` string at a known corner square. Full verification gate passed clean
(`build_tests.sh` 27/27, `build_verify.sh`, `perft 5` exact).

**Gauntlet, 2 seeds vs Fairy-Stockfish-NNUE — stacking regressed again:**

| Seed | Round 37 (mobility + promotion-race) | Round 35 alone (ref) | Round 27 (ref) |
|---|---|---|---|
| 99 | 0W 85D 115L → -228 | -198 (Δ **-30**) | -205 (Δ -23) |
| 4242 | 1W 89D 110L → -212 | -175 (Δ **-37**) | -200 (Δ -12) |

Both seeds shifted consistently negative relative to Round 35 alone (-30/-37, within 7 Elo of
each other) — the **second** stacking attempt in a row (after Round 36's rook-trap risk) that
erodes rather than compounds the signal. **Reverted cleanly** (pure addition since Round 35's
commit, so `git checkout` restored byte-identical files; rebuilt and re-verified).

**Takeaway**: two independent classical-eval additions (a narrow trap-risk penalty, a broad
mobility bonus) have now both hurt when stacked on Round 35, despite being mechanically
unrelated to each other and to promotion-race urgency itself. This is suggestive, not just
two isolated bad calibrations: adding *any* second eval term on top of Round 35 has so far
made things worse, which could mean either (a) Round 35's own positive signal is fragile/
coincidental and further changes simply add noise that overwhelms it, or (b) this classical
eval already sits near a local optimum where additional heuristics of this complexity level
tend to interfere with existing terms (PST, rook activity, king safety) in ways that cost
more than they add, consistent with every stacking attempt this session actively losing
ground. Round 35 alone remains the standing candidate; further classical-eval additions on
top of it are not obviously promising without a different approach (e.g. retuning existing
terms' weights jointly with a new one, rather than adding a new term in isolation).

### Round 38 — Full 4-Table Correction History (Material/Minor-Piece/Non-Pawn) — Confirmed Regression, Reverted (2026-10-17)

User asked to complete the full correction-history suite Round 29 had deliberately deferred
("this codebase has no materialKey()/minorPieceKey()/nonPawnKey() at all... the same class of
subtle-bug risk that caused the historical seeGe() bugs"). Cherry-picked Round 29's pawn-only
commit (40df71c, clean, no conflicts) as the foundation, then built the missing Zobrist-key
infrastructure.

**Safety choice, deliberately deviating from true Stockfish style**: `materialKey`,
`minorPieceKey`, and `nonPawnKey[color]` are maintained by a **full recompute from board
state** (`Position::recomputeAuxKeys()`, called once at the end of `doMove()` and inside
`setState()`) rather than true incremental XOR maintenance like `pawnKey()`. This trades a few
dozen extra ops per move (not in any hot inner loop) for eliminating the entire incremental-
divergence bug class entirely — there is only one code path, never two that could drift apart.
`doNullMove()` needed no change (these fields live before `key` in `StateInfo`, within its
partial-memcpy range, and piece positions never change on a null move anyway).

New tests isolated the key-computation logic before touching search: targeted FEN checks that
each key changes only when it should (quiet pawn/knight move, a knight capture, a promotion)
and a 300-ply random-walk check (1230 checks) confirming all three keys match an independent
fresh `Position::set()` recompute at *every single ply* — two real test-FEN bugs (an illegal
diagonal "rook" capture, a knight destination blocked by Makruk's full pawn rank 3) were caught
immediately by the test failing to construct a legal move, before ever reaching the gauntlet.

Then wired three new tables (`MaterialCorrectionHistory`, `MinorPieceCorrectionHistory`,
`NonPawnCorrectionHistory`) matching the existing `PawnCorrectionHistory`'s shape and untuned
constants exactly (no stronger rationale to differentiate them). `Search::correctedStaticEval()`
sums all 4 tables' individually-clamped (±64 each) contributions, then clamps the *total* to a
new combined cap (±100, matching this codebase's existing NNUE-blend cap magnitude) before
applying it. New tests confirmed the 4 tables combine additively and that saturating all 4 at
once still stays safely inside the TB-range via the combined cap. Full verification gate passed
clean throughout (1237 total correction-history/aux-key checks at the final stage), `perft 5`
exact.

**Gauntlet, 2 seeds vs Fairy-Stockfish-NNUE — confirmed regression, the largest of this
session's three stacking attempts:**

| Seed | Round 38 (full correction history) | Round 35 alone (ref) | Round 27 (ref) |
|---|---|---|---|
| 99 | 0W 74D 126L → -258 | -198 (Δ **-60**) | -205 (Δ -53) |
| 4242 | 0W 87D 113L → -222 | -175 (Δ **-47**) | -200 (Δ -22) |

Both seeds consistently negative relative to Round 35 alone, both clearly larger in magnitude
than Round 36's rook-trap risk (-30/-37) or Round 37's piece mobility (-30/-37) — this is the
**worst** of the three stacking attempts, not merely another one. **Reverted completely**: all
10 files touched (the cherry-pick, the new keys, and the 3 new tables) checked out back to the
pre-Round-38 commit; `build_tests.cpp` no longer even contains a correction-history test.
Rebuilt and re-verified (`build_verify.sh` + `perft 5` exact) — confirmed restores exactly
Round 35's state. The code remains in git history (3 commits) for potential future reference,
but is not part of the kept/active state.

**Takeaway**: the infrastructure itself (the new Zobrist keys) checked out as correct via 1230+
independent random-walk cross-checks — this is not a correctness bug, it's a genuine Elo
regression from the *mechanism*. Combined with Rounds 36-37, this is the **third** stacking
attempt in a row to actively hurt Round 35's standing state, and by a clearly larger margin
than the other two. This further reinforces the Round 37 takeaway: adding further eval/search
terms on top of Round 35 in isolation is not a promising direction right now, regardless of how
well-motivated or carefully-implemented the addition is (unlike Rounds 36/37, this one came
with the most implementation care of the three — full test coverage of the key-computation
layer *before* touching search — and still regressed more, not less). Round 29's own untuned
constants remain a candidate for a future tuning sweep if this direction is revisited, but
that's a different kind of change (retuning existing constants) than what was tried here
(adding 3 more tables at the same untuned defaults).

### Round 39 — Promotion-Race Urgency, Doubled Magnitude — Sign-Flipped, Inconclusive (2026-10-17)

After three stacking attempts in a row failed (Rounds 36-38), tried a different kind of
change on Round 35 itself: not adding a new term, but tuning the one term already showing a
real signal. Round 35's `PromotionUrgency` constants (`S(0,5)` two steps from promotion,
`S(15,35)` one step away) are modest relative to the actual material stakes — a Met is worth
roughly 250-300cp more than a pawn — so there was room to test a larger magnitude without
being unreasonable. Doubled both: `S(0,10)` / `S(30,70)`. Single-constant change, verified via
the full test suite (the existing Round 35 tests use relative comparisons, not exact values,
so they remained valid unchanged) + `build_verify.sh` + `perft 5` exact.

**Gauntlet, 2 seeds vs Fairy-Stockfish-NNUE — sign-flipped relative to Round 35 itself:**

| Seed | Round 39 (2×) | Round 35 (1×, ref) | Round 27 (ref) |
|---|---|---|---|
| 99 | -179 | -198 (Δ **+19**) | -205 (Δ +26) |
| 4242 | -233 | -175 (Δ **-58**) | -200 (Δ -33) |

Doubling helped strongly at seed 99 and hurt strongly at seed 4242 — a clean sign-flip by this
project's own standing rule (Round 28 precedent: a sign-flip means inconclusive, not "small
effect either way"). **Not adopted** — reverted to the original 1× magnitude (confirmed via
`build_verify.sh` + `perft 5` exact), since this result doesn't demonstrate the original
magnitude is wrong, only that doubling it isn't a clean improvement.

**Takeaway**: unlike Rounds 36-38 (adding a brand-new term, each confirmed negative),
*tuning* Round 35's existing term produced genuine uncertainty rather than a clean regression
— a qualitatively different (and less conclusive) result. This leaves the question of whether
Round 35's magnitude is well-calibrated genuinely open rather than closed: a smaller
magnitude (halving, or intermediate values) remains untested and could behave differently
again, but each such test costs a full 2-seed gauntlet for an uncertain payoff, and the
signal-to-noise ratio of single-constant tuning sweeps has been poor throughout this
project's history (Rounds 24-25's search-constant sweep: 3 candidates, all within 11 Elo of
baseline). Round 35 at its original, 3-seed-confirmed-positive magnitude remains the standing
candidate.

### Rounds 40-41 — Blend Gate Re-Tested With Round 35 Active — Both Directions Still Regress (2026-10-17)

Rounds 30 (wide, 450cp) and 34 (narrow, 200cp) were both tested *before* Round 35 existed.
Since promotion-race urgency adds directly to the classical score for passed pawns, it could
plausibly shift how often positions fall inside/outside the gate range, so the user asked to
re-test both directions with Round 35 active — a genuine interaction never checked. Two
single-constant changes, each verified via the full gate (`build_verify.sh` + `perft 5` exact)
before gauntlet testing.

**Round 40 (narrow, 300→200, + Round 35) — confirmed regression:**

| Seed | Round 40 | Round 35 alone (ref) | Round 27 (ref) |
|---|---|---|---|
| 99 | -225 | -198 (Δ **-27**) | -205 (Δ -20) |
| 4242 | -220 | -175 (Δ **-45**) | -200 (Δ -20) |

**Round 41 (wide, 300→450, + Round 35) — confirmed regression:**

| Seed | Round 41 | Round 35 alone (ref) | Round 27 (ref) |
|---|---|---|---|
| 99 | -241 | -198 (Δ **-43**) | -205 (Δ -36) |
| 4242 | -212 | -175 (Δ **-37**) | -200 (Δ -12) |

Both directions regress with Round 35 active, both seeds negative in each case — matching
Rounds 30/34's original findings exactly in sign (though not in whether Round 35 changes the
*magnitude*: both Round 40/41 deltas-vs-Round-35 are somewhat larger than Round 30/34's
deltas-vs-Round-27 were, but not dramatically so, and with only 2 seeds per condition this
isn't a confident "the interaction makes it worse," just "the interaction doesn't make it
better or flip the sign"). **Both reverted** to gate=300 (confirmed via `build_verify.sh` +
`perft 5` exact after the final revert).

**Takeaway**: the hypothesis that Round 35's active promotion-urgency scoring might shift
enough positions across the gate threshold to change Rounds 30/34's conclusions did not pan
out — 300cp remains a genuine local optimum for the blend gate independent of whether Round
35 is active. This closes out the blend-gate-interaction question cleanly: it was worth
checking once (a real untested interaction, unlike re-running an already-settled question),
and the answer is no, it doesn't change anything.

### Round 42 — Fresh Training Data from the Post-Round-33/35 Engine: Small Regression, Not Deployed (2026-10-18)

First of a 2-track NNUE/training plan (Track A: fresh data; Track B: bigger architecture,
run in sequence per user's explicit choice). Mirrors Round 27's exact recipe — the only
variable changed is which engine generated the self-play games: Round 27's data predates
both Round 33's `evaluate.cpp` counting-draw fix and Round 35's promotion-race urgency
heuristic, so this round regenerated data with the current engine instead.

Froze `src/sf-kernel` to `/tmp/sf-kernel-round42-datagen` after a clean `build_verify.sh`
pass on `classical-eval-new-heuristics`, then ran Round 27's exact 3-config self-play recipe
against it (configA: movetime-a=200/depth-b=4; configC: movetime-a=150/depth-b=3; configD:
movetime-a=30/depth-b=4; 150 games each, 450 total, 0 crashes). Converted, teacher-labeled
with Fairy-SF depth=10 (24,543 positions scored), filtered to blend range (30-400cp) and
combined with `train_round27_combined.tsv` as the base → **281,690 positions** (base
267,971 + 13,719 accepted, 55.9% acceptance — slightly healthier than Round 27's own 53.1%).
`dataset_summary.py` found nothing unhealthy (WDL 40.6/51.3/8.2%, no extreme score outliers).

Trained 60 epochs, architecture unchanged (L1/L2/L3 = 512/32/32 — isolating data freshness
as the only variable, matching Round 31's isolation discipline for its own single-variable
test). Best epoch 57, val_loss 0.560082 — *worse* than Round 27's 0.553515, another data
point for "val_loss doesn't predict Elo." Exported to VERSION3/MKN3 (L2 headroom 1.0×, same
tight-by-design pattern as Round 31). Full verification gate passed clean (`build_tests.sh`,
`build_verify.sh`, `perft 5` exact).

**Gauntlet, 2 seeds vs Fairy-Stockfish-NNUE — small, consistent regression:**

| Seed | Round 42 (fresh data) | Round 27 (ref) | Delta |
|---|---|---|---|
| 99 | 0W 86D 114L → -225 | -205 | -20 |
| 4242 | 0W 84D 116L → -230 | -200 | -30 |

Both seeds agree in direction (negative), 10 Elo apart — the same evidentiary pattern as
Rounds 29/32/34 (small, consistent, individually inside the ~46 Elo noise band but reading as
a real effect rather than pure noise). **Not deployed** — `evaluate.h` reverted to
`3587509696.bin` (Round 27), rebuilt and re-verified (`build_verify.sh` + `perft 5` exact).
Checkpoint `tools/training/makruk_round42.pt` (val_loss 0.560082) kept on disk, gitignored,
for possible future ablation — same treatment as Rounds 12/15/16/28/31.

**Takeaway**: regenerating training data with a *better* engine (Round 33's fix + Round 35's
heuristic both live during self-play) did not reproduce Round 27's win — unlike Round 27,
where the underlying engine had changed drastically (10x faster, SEE-correct) between data
generations, the Round 33/35 changes are comparatively minor adjustments to classical eval,
so there may simply not have been enough of a quality shift in the self-play games themselves
to move the needle, even though the *playing* engine is measurably stronger. This echoes
Round 28's finding that scaling/regenerating a working recipe isn't a guaranteed lever on its
own. Track B of the same plan (NNUE L1=1024 paired with larger L2/L3 this time, unlike
Round 31's L1-only isolation) is the next step, training on Round 27's dataset unchanged to
isolate architecture as the only variable.

### Round 43 — NNUE L1=1024/L2=64/L3=64: Confirmed Regression, Worst NNUE-Architecture Result, Not Deployed (2026-10-24)

Track B of the 2-track NNUE/training plan (Track A was Round 42). Round 31 tested
`MknnMaxL1` doubled 512→1024 in isolation (L2/L3 left at 32) and got a confirmed regression;
Round 31's own write-up explicitly flagged that L2/L3 were never scaled up alongside L1,
leaving that combination untested. This round paired the wider L1 with wider L2/L3 too:
`MknnMaxL1` 512→1024 in `src/nnue/mknn_accumulator.h` (verified backward-compatible/bit-exact
with the Round 27 net first), then `train.py`'s `L1, L2, L3 = 512, 32, 32` →
`1024, 64, 64`. Trained on `train_round27_combined.tsv` — the *exact same dataset* Round 27
and Round 31 both used — so any Elo change is attributable to architecture alone, not data.

Training survived a real mid-run machine reboot at epoch 58/60 (not a code bug — confirmed
via `journalctl`: a full reboot, `systemd-oomd`/`NetworkManager` restarting, not an OOM-kill
of the training process specifically) and resumed cleanly via `--resume` for the final 2
epochs. Best epoch 57 (0-indexed), val_loss **0.553249** — essentially identical to Round
27's 0.553515 (L1=512) and Round 31's 0.553548 (L1=1024/L2=32), the third confirmation of
"val_loss doesn't predict Elo" at this architecture family. Exported to VERSION3/MKN3; L2
int16 overflow-safety headroom 1.0× (tight by design, matching Round 31's pattern at the
wider architecture too — the auto-calibration always picks the highest-precision scale the
safety margin allows). Full verification gate (`build_tests.sh`, `build_verify.sh`, `perft 5`
exact) passed clean, including 188/188 NNUE incremental-accumulator bit-exactness checks at
the new L1=1024 width.

**Gauntlet, confirmed at 2 seeds vs Fairy-Stockfish-NNUE — clear regression, the largest
NNUE-architecture result in this project's history:**

| Seed | Round 43 (L1=1024/L2=64/L3=64) | Round 27 (ref, L1=512/32/32) | Delta |
|---|---|---|---|
| 99 | 0W 67D 133L → -279 | -205 | **-74** |
| 4242 | 0W 66D 134L → -282 | -200 | **-82** |

Both seeds agree in direction (negative) and **both individually clear** the ~46 Elo
single-seed noise band — a confirmed regression by the strongest possible evidentiary
standard this project uses, larger even than Round 31's own confirmed regression (-44/-69).
**Not deployed** — `evaluate.h` reverted to `3587509696.bin` (Round 27), rebuilt and
re-verified (`build_verify.sh` + `perft 5` exact). The L1=1024/L2=64/L3=64 infra changes
(`MknnMaxL1` cap, `train.py`'s constants) were split onto their own branch
(`nnue-l1024-l2l3-64-round43`), not merged to `classical-eval-new-heuristics`'s net-selection
state — same treatment Round 31 gave its own `nnue-l1-1024` branch. Checkpoint
`tools/training/makruk_round43.pt` (val_loss 0.553249) kept on disk, gitignored, for
potential future ablation.

**Takeaway**: pairing wider L2/L3 with the wider L1 didn't just fail to fix Round 31's
regression — it made it substantially *worse* (-44/-69 → -74/-82). This rules out "L2/L3
were left too narrow" as the explanation for Round 31's result and closes out NNUE
architecture-scaling as a lever on this dataset/training recipe entirely: more capacity,
in any combination tried so far (L1 alone, or L1+L2+L3 together), consistently hurts rather
than helps. Combined with Round 42's result (fresh data, same architecture: small
regression), both halves of the "two genuinely untried NNUE/training levers" plan from
Rounds 31-41's accumulated findings have now failed, leaving classical-eval constant tuning
(Round 29's correction-history sweep, Round 35's magnitude) as the only remaining low-effort
options — see "Next priority" above.

### Round 44 — Teacher-Depth vs Data-Volume Trade-off: Small Regression, Roughly Flat vs Round 42, Not Deployed (2026-11-01)

The first of two previously-unexamined NNUE/training levers identified after Round 43 closed
out architecture scaling (the other being a training-objective change, still untried).
`teacher_label.py --depth 10` has been fixed since Round 27 despite the script's own default
being `--depth 8`; depth is normally the dominant cost in alpha-beta search, so this round
tested whether trading teacher precision for data volume — label a much larger self-play
pool more shallowly — beats labeling a smaller pool more deeply.

**Calibration** (on a 2,000-position slice of Round 42's leftover raw pool): depth=8 ran at
~150 pos/sec vs depth=10's known ~45-48 pos/sec (~3.1-3.3x faster), comfortably clearing the
plan's 2x go/no-go threshold; depth=6 measured ~500 pos/sec (~10-11x), noted for a possible
future round but not used here.

**Self-play generation**: reused Round 27/42's exact 3-config recipe unchanged, scaled ~4x
Round 42's volume (600 games/config vs 150, split into 2x300 sub-batches each for resilience
against `gauntlet.py`'s lack of `--resume`) — 1,800 games total, ~2.3 hr unattended, 0
crashes, yielding 107,404 raw positions (vs Round 42's 26,307). Teacher-labeled at depth=8:
99,206 scored in 10.5 minutes (169 pos/sec, matching calibration). Filtered to blend range
(30-400cp) and combined with `train_round27_combined.tsv` — **56,812 accepted (57.3%
acceptance), 4.14x Round 42's 13,719-position addition** — combining to **324,783 positions
total**, the largest single-round dataset in this project's history. `dataset_summary.py`
found nothing unhealthy (0.3% duplication, no extreme score outliers).

Trained 60 epochs, architecture unchanged (512/32/32 — isolating the depth/volume trade-off
as the only variable). Survived an unattended-training interruption mid-epoch-1 (process
stopped for a machine shutdown before its first checkpoint ever saved — not a crash, just no
checkpoint existed yet to resume from, so the next session restarted cleanly from epoch 0
rather than resuming). Best epoch 60 (the final epoch itself), val_loss **0.568544** — worse
than Round 27's 0.553515 and Round 42's 0.560082, continuing this project's "val_loss doesn't
predict Elo" pattern across yet another round. Exported to VERSION3/MKN3 (L2 headroom 1.0×,
consistent tight-by-design pattern). Full verification gate passed clean throughout
(`build_tests.sh`, `build_verify.sh`, `perft 5` exact).

**Gauntlet, 2 seeds vs Fairy-Stockfish-NNUE — small consistent regression vs Round 27, flat
vs Round 42:**

| Seed | Round 44 (depth=8, 4x volume) | Round 27 (ref) | Round 42 (ref, depth=10, 1x volume) |
|---|---|---|---|
| 99 | 0W 89D 111L → -217 | -205 (Δ **-12**) | -225 (Δ **+8**) |
| 4242 | 0W 83D 117L → -233 | -200 (Δ **-33**) | -230 (Δ **-3**) |

Both seeds negative vs Round 27 — the same small-consistent-regression evidentiary pattern
as Rounds 29/32/34/42 (individually inside the ~46 Elo noise band, but consistent in sign).
Against Round 42 specifically, the deltas are tiny and sign-inconsistent (+8/-3) — trading
teacher depth for 4x the volume neither clearly helped nor hurt relative to Round 42's
depth=10/1x-volume recipe. **Not deployed** — `evaluate.h` reverted to `3587509696.bin`
(Round 27), rebuilt and re-verified (`build_verify.sh` + `perft 5` exact). Checkpoint
`tools/training/makruk_round44.pt` (val_loss 0.568544) and `train_round44_combined.tsv`
(324,783 positions) kept on disk, gitignored, for potential future ablation.

**Takeaway**: the depth/volume trade-off is not a lever that moves Elo at this scale (4x) —
whatever signal a 4x-larger, shallower-labeled slice adds is approximately cancelled out by
the extra label noise from the shallower teacher search, landing within noise of Round 42's
smaller/deeper-labeled slice rather than clearly beating or losing to it. Combined with Round
43 (architecture scaling, confirmed regression), both of the two genuinely untried NNUE/
training levers identified since Round 31 have now failed to beat Round 27. The next
candidate lever is qualitatively different from everything tried in Rounds 28-44 so far: a
training-objective change (WDL-blended loss) rather than another variation on data or
architecture — motivated directly by the "val_loss never predicts Elo" finding that has now
held independently across 8 rounds (9, 10, 15, 16, 28, 31, 42, 43, 44).

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
| Round 33 (fix: `evaluate.cpp` no longer blinds whole counting-eligible endgames) | -205/-230 | 0/-30 | one seed flat, one down 30 — inconclusive by this project's bar; kept anyway on correctness grounds (resolved 2026-10-11) |
| Round 34 (blend gate narrowed 300→200cp, on top of Round 33's fix) | -220/-222 | -15/-22 | both seeds agree (negative), tight 2-Elo agreement — small consistent regression, not deployed; closes out blend-gate tuning as a lever (both directions now regress) |
| Round 35 (classical eval: promotion-race urgency) | -198/-175/-215 | +7/+25/+29 | all 3 seeds positive (avg +20) — strongest multi-seed signal since Round 27, but no single seed clears the noise band; kept, not yet "confirmed" by the strict bar |
| Round 36 (rook-trap risk stacked on Round 35) | -205/-225 | 0/-25 | stacking made both seeds worse than Round 35 alone (+7→0, +25→-25) — net negative, reverted; closes out the classical-eval "not yet implemented" list |
| Round 37 (piece mobility bonus stacked on Round 35) | -228/-212 | -23/-12 | both seeds consistently worse than Round 35 alone (-30/-37) — second stacking attempt in a row that regressed, reverted |
| Round 38 (full 4-table correction history, stacked on Round 35) | -258/-222 | -53/-22 | both seeds consistently worse than Round 35 alone (-60/-47) — worst of 3 stacking attempts, reverted |
| Round 39 (promotion-race urgency, magnitude doubled) | -179/-233 | +26/-33 | sign-flipped vs Round 35 itself (+19/-58) — inconclusive, not adopted; reverted to 1× magnitude |
| Round 40 (blend gate narrowed 300→200, + Round 35) | -225/-220 | -20/-20 | both seeds worse than Round 35 alone (-27/-45) — confirmed regression, interaction doesn't help; reverted |
| Round 41 (blend gate widened 300→450, + Round 35) | -241/-212 | -36/-12 | both seeds worse than Round 35 alone (-43/-37) — confirmed regression, interaction doesn't help; reverted |
| Round 42 (fresh training data from the post-Round-33/35 engine) | -225/-230 | -20/-30 | both seeds negative, 10 Elo apart — small consistent regression (same pattern as 29/32/34), not deployed; Track A of a 2-track NNUE/training plan, Track B (bigger architecture) is next |
| Round 43 (NNUE L1=1024/L2=64/L3=64, same Round 27 dataset) | -279/-282 | -74/-82 | both seeds confirmed regression, both individually clear the noise band — worst NNUE-architecture result in this project's history, closes out that lever entirely; not deployed |
| Round 44 (teacher depth=8, ~4x Round 42's self-play volume) | -217/-233 | -12/-33 | both seeds negative vs Round 27 (small consistent regression); roughly flat vs Round 42 itself (+8/-3) — depth/volume trade-off doesn't move Elo at this scale; not deployed |

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
* `tools/training/makruk_round42.pt` — Round 42 checkpoint (fresh training data from the
  post-Round-33/35 engine, architecture unchanged 512/32/32), val_loss 0.560082, Elo
  -225/-230 (2-seed, small consistent regression), not deployed, gitignored, kept for
  potential future ablation. `tools/training/train_round42_combined.tsv` (281,690 positions)
  kept alongside it.
* `tools/training/makruk_round43.pt` — Round 43 checkpoint (NNUE L1=1024/L2=64/L3=64, Round
  27's exact dataset), val_loss 0.553249, Elo -279/-282 (2-seed confirmed regression, both
  seeds clear the noise band — the worst NNUE-architecture result to date), not deployed,
  gitignored, kept for potential future ablation. Infra changes (`MknnMaxL1` cap, `train.py`
  constants) live on branch `nnue-l1024-l2l3-64-round43`, not merged.
* `tools/training/makruk_round44.pt` — Round 44 checkpoint (teacher depth=8, ~4x Round 42's
  self-play volume, architecture unchanged 512/32/32), val_loss 0.568544, Elo -217/-233
  (2-seed, small consistent regression vs Round 27, roughly flat vs Round 42), not deployed,
  gitignored, kept for potential future ablation. `tools/training/train_round44_combined.tsv`
  (324,783 positions, the largest dataset in this project's history) kept alongside it.
* `src/3769833465.bin` — Round 23 VERSION3/MKN3 (int16 L2), archived
* `src/2020277415.bin` — Round 14 net, MKN2 int16, L1=512 — same underlying weights as
  3769833465.bin before L2 quantization, archived
* `src/150547052.bin` — Round 16 net (first book-paired round), inconclusive, untracked
* `tools/training/makruk_round12.pt`, `makruk_round15.pt` — regression/noise-affected
  checkpoints kept for potential future ablation, gitignored

Earlier nets (v1 through v9, Rounds 1-10) are fully superseded and not individually listed
here — see git history / the Development History section above for what each round changed.
