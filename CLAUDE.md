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
detection. **Not yet implemented**: promotion-race urgency, rook-trap risk, explicit
"counting danger" awareness. An experimental Khon/Met central-control bonus was tried and
found inconclusive (sign-flipped between seeds) — see "Central-Control Eval Experiment" below;
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

## Current Status (as of Round 27, 2026-09-13)

* **Embedded net**: `src/3587509696.bin` (Round 27, trained on fresh training data from the
  post-Round-26 engine). Check `src/evaluate.h`'s `NnueNetDefaultName` for what's actually
  shipped at any given time, this note can drift.
* **True strength** (book-paired, equal-condition 200ms/move, vs Fairy-Stockfish-NNUE, the
  only trustworthy Elo methodology in this project's history — see "Handicap gauntlets are not
  true strength" below): progressed **-372 → -260 → -205/-200** across Rounds 18-27. Rounds
  18-26 were engine-side fixes (incremental NNUE fix, two real `seeGe()` bugs, fast-math,
  heap-alloc elimination, L2 quantization, a vectorization-blocking bug fix); two consecutive
  large nps wins there (Round 23 +34%, Round 26 +42.7%) produced ~0 combined net Elo, closing
  the nps gap with Fairy-Stockfish-NNUE entirely but confirming nps had plateaued as a lever.
  Round 27 then pivoted to training-data quality (regenerating data with the now-much-faster,
  tactically-correct engine) and delivered **+55/+60 Elo, confirmed at 2 seeds** — the first
  round in a long time to move the needle by more than single-digit/noise-band amounts.
* **Next priority**: continue the training-data-quality direction that just paid off in Round
  27 — e.g. further recalibrating handicap configs, or scaling up the Round 27 recipe with more
  games, following the same validated-then-scaled pattern as Rounds 11→13→14.
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

Handicap-gauntlet numbers from Rounds 11-16 (roughly +111 to +382 Elo) are **not comparable**
to this table — different methodology, not true strength. See "Key Methodological Findings."

---

## Net Archive (most recent only; superseded nets are gitignored/untracked, kept on disk where noted)

* `src/3587509696.bin` — Round 27 VERSION3/MKN3 (int16 L2), fresh training data from the
  post-Round-26 engine, Elo -205/-200 (2-seed confirmed, +55/+60 over Round 26) — **currently
  embedded** (verify against `src/evaluate.h`, this can drift)
* `src/3769833465.bin` — Round 23 VERSION3/MKN3 (int16 L2), archived
* `src/2020277415.bin` — Round 14 net, MKN2 int16, L1=512 — same underlying weights as
  3769833465.bin before L2 quantization, archived
* `src/150547052.bin` — Round 16 net (first book-paired round), inconclusive, untracked
* `tools/training/makruk_round12.pt`, `makruk_round15.pt` — regression/noise-affected
  checkpoints kept for potential future ablation, gitignored

Earlier nets (v1 through v9, Rounds 1-10) are fully superseded and not individually listed
here — see git history / the Development History section above for what each round changed.
