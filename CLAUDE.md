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
Architecture and training history are in the Development History below (Rounds 1–26).

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

## Current Status (as of Round 26, 2026-08-30)

* **Embedded net**: `src/3769833465.bin` (Round 23's VERSION3/MKN3, int16-quantized L2 —
  the same underlying weights as Round 14's `2020277415.bin`, just re-exported with L2
  quantization). Check `src/evaluate.h`'s `NnueNetDefaultName` for what's actually shipped at
  any given time, this note can drift.
* **True strength** (book-paired, equal-condition 200ms/move, vs Fairy-Stockfish-NNUE, the
  only trustworthy Elo methodology in this project's history — see "Handicap gauntlets are not
  true strength" below): progressed **-372 → -260** across Rounds 18-26 (incremental NNUE fix,
  two real `seeGe()` bugs, fast-math, heap-alloc elimination, L2 quantization, a
  vectorization-blocking bug fix). Two consecutive large nps wins (Round 23 +34%, Round 26
  +42.7%) produced ~0 combined net Elo — **nps is no longer the lever**; the engine already
  searches deep enough at 200ms/move. **nps gap with Fairy-Stockfish-NNUE is now fully closed**
  (we meet/exceed it).
* **Next priority** (flagged repeatedly, not yet acted on): training-data/net-quality work, not
  more engine speed or search-parameter tuning — Round 25 exhausted 3 well-reasoned
  search-constant candidates with no signal; Round 26 confirmed nps has plateaued as a lever.
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

---

## Development History

### Training pipeline era (net/data quality work, before the C++ correctness pivot)

Initial proof-of-concept (110 self-play games, all draws, val_loss 0.67) established the
pipeline works but the data was too draw-heavy to teach real signal. PR11 added the core
tooling still in use today: `tools/gauntlet.py`, `tools/selfplay/selfplay.py`'s handicap
modes, `tools/training/convert.py`/`dataset_summary.py`, `docs/dataset_guide.md`. A cascade of
early bugs were found and fixed in sequence — each blocking meaningful net evaluation until
fixed:

1. **Counting draw never wired into game logic** → 66% stalemate rate in gauntlets. Fixed by
   connecting `isDraw()` to `isMakrukCountingDraw()`.
2. **0% decisive self-play** even with handicaps, because counting-draw endgames score ≈0
   regardless of material. Fixed with **score adjudication** (`--adjudication-threshold`/
   `--adjudication-streak` in `selfplay.py` and `gauntlet.py`) — a sustained ≥500cp streak
   after ply 20 ends the game. Decisive rate: 0% → 65-80%.
3. **`gauntlet.py`'s draw detection never fired** (`sc == 0` exact-match against a value that's
   never exactly zero) → 61-67% spurious stalemates even after the counting fix. Fixed with a
   tolerance-based per-engine streak check; stalemate rate dropped to ~1%.
4. **NNUE was never loading at all** — wrong magic-number check meant `MknnEvaluator` silently
   fell back to classical eval every game. *Every gauntlet Elo number before this fix measured
   classical eval, not NNUE.* Fixed by writing a purpose-built `MknnEvaluator` reading the
   project's own MKNN format directly.
5. Once NNUE was actually active, it was a **-471 Elo regression** vs classical (-108 vs
   +363), from three compounding bugs: (a) a wrong sigmoid-decode formula compressing large
   scores, (b) CReLU/ReLU mismatch between training and inference, (c) training labels
   compressed toward 0 in counting-draw endgames so the net never learned material scale.
   Fixing the decode formula, matching CReLU exactly, and adding `--color-augment` +
   imbalanced-opening self-play data (`decisive_v3`/`v4`, `imbalanced_fens.txt`) closed the
   gap: NNUE blend eventually matched (Round 6-7, `evaluate.cpp`'s gate/cap blend formula:
   `classical + clamp(nnue - classical, -100, 100)`, gate 300cp) and then modestly beat
   (Round 8, +25 Elo over pure classical) classical-only play.
6. **Pure NNUE (no classical blend) was never viable** in any training configuration tried —
   every attempt (Round 7's v6, Round 9's score-scaled v8) underestimated material by ~2× or
   overcorrected into a different failure mode (v8: equal positions inflated to +150cp,
   Elo -301). The underlying cause (Round 9, confirmed in Round 10): Makruk counting draw
   compresses Fairy-Stockfish teacher labels to ≈0 in endgames, so the net never sees large,
   unambiguous material-advantage examples during training. A `UseCounting` UCI option /
   `--no-counting` selfplay flag (Round 10) let training data generation disable counting to
   get real material-scale labels — this is training-data-generation-only, never for real
   play/gauntlets.
7. **A depth≥9 SIGSEGV** was discovered while generating counting-unaware data (Round 10) —
   root cause never confirmed despite ~1500 reproduction attempts across two rounds (10, 17);
   hardened defensively (`ValueList::pushBack` and `Position::setCheckInfo` now abort with a
   diagnostic instead of reading out-of-bounds) and `tools/build_verify.sh` added as a standing
   crash-probe gate. `selfplay.py --strong-depth` remains the only confirmed-effective
   mitigation for data generation.

**Net-quality rounds 11-16**: closer-handicap self-play (opponent depth 4-5 instead of the
full-strength baseline) proved an efficient way to harvest "blend-range" (near-equal position)
training data — 84-87% acceptance vs the original method's 63.5%. One scale-up attempt (Round
12) regressed sharply; an ablation (Round 13, reusing already-labeled data, no new games)
confirmed the closest-handicap config (depth-b=5, nearest genuine parity) was the cause —
noisier labels near true equality diluting signal despite passing the blend-range filter by
score alone. Rounds 13-14 then scaled the *validated-good* configs cleanly, holding or
improving each time. Round 15 appeared to regress sharply (+127 vs +173) but a follow-up
seed-variance investigation found this was mostly the 46-Elo single-seed noise described above,
not a real regression — which directly motivated adding opening-book pairing (`tools/books/`,
Round 15 follow-up) to shrink that noise band. Round 16, the first book-paired round, came back
genuinely inconclusive (opposite-signed deltas at two seeds) and was correctly not deployed.

### C++ correctness/performance era (Rounds 17-26, current main development thrust)

**Round 17** (2026-08-22): re-investigated the unfixed SIGSEGV — still unreproduced after a
further ~150-game bounded attempt. Added `tools/build_verify.sh` as a standing post-build gate.

**Round 18** (2026-08-22): asked to test without any handicap for the first time —
revealed the **true baseline is -372 Elo**, not the +111 to +382 range every prior handicap
gauntlet had reported (see "Key Methodological Findings" above). Root-caused via live
profiling: `MknnEvaluator` recomputed its entire FT accumulator from scratch on every call
despite the incremental-update infrastructure (`HalfKAv2Makruk::appendChangedIndices` etc.)
already existing as dead code. Implemented `MknnAccumulator` (rides `StateInfo`) and
`updateAccumulator()` (backward-walk + forward-replay), verified bit-exact against a
from-scratch reference across 13 scenarios (188 checks). Result: -372 → -359 (+13 Elo).

**Round 19-20** (2026-08-22/23): diffed the Makruk-adapted files against the `upstream` git
remote (`FireFather/sf-kernel`) looking for adaptation bugs — found two real ones in
`Position::seeGe()` (Static Exchange Evaluation): (1) Khon/Met attacker detection used a
chess-style slider-union formula instead of the reverse-color/non-slider logic
`attackersTo()` already used correctly elsewhere, and (2) the attacker-check order was
inherited verbatim from chess's ascending-value order (Pawn<Knight<Bishop<Rook<Queen) instead
of Makruk's actual order (Pawn<Met<Khon<Knight<Rook). Both fixed, verified via a 2165-check
fast-vs-slow-reference test (`test_see.cpp`) that was confirmed to have teeth (reverting the
fix reintroduces real failures). Result: -359 → -346.

**Round 21** (2026-08-23): closed most of the nps gap. Root cause: GCC's auto-vectorizer left
the L2 forward-pass reduction loop scalar because it lacked permission to reorder floating-point
operations — not a SIMD-width problem (AVX2 was already active; `ARCH=x86-64-bmi2` cascades
`avx2=yes`). Added `-fassociative-math -fno-signed-zeros -fno-trapping-math -fno-math-errno`
(a safe subset, not full `-ffast-math`) scoped to just `mknn_evaluator.cpp`. Also fixed the
Makefile's own default `ARCH` (was `x86-64-modern`, no AVX2/BMI2 at all) to `x86-64-bmi2`.
nps: ~40K → ~150-190K (~4×). Result: -346 → -282 (**+64 Elo**), confirmed real at a second seed
(+90 Elo there) — the largest single-round gain in this project's history.

**Round 22** (2026-08-29): eliminated the remaining `std::vector` heap allocations in
`MknnEvaluator::evaluate()` via fixed-size stack buffers (`fastEvalOk_` fast path, unchanged
`std::vector` fallback preserved for any future oversized net). nps +6.3%. Result: -282 → -269
(+13 Elo).

**Round 23** (2026-08-29): added VERSION3 (MKN3) — int16-quantized L2 weight, with a
load()-time overflow-safety check that rejects an unsafely-calibrated net. nps +34% (bench),
real-net eval drift ≤2cp vs the float32 twin across 11 positions. Gauntlet Elo was
**inconclusive** (+14 at seed=99, +0 at seed=4242) — deployed anyway on the strength of the
independently-verified nps gain and correctness, explicitly flagged as lower-confidence than
Round 21/22's deployments.

**Round 24-25** (2026-08-29/30): diffed `search.cpp`/`movepick.cpp`/`timeman.cpp` against
upstream — **byte-identical, no adaptation bugs** (unlike position.cpp/evaluate.cpp/SEE).
Tested 3 well-reasoned search-constant candidates against Fairy-Stockfish-NNUE (null-move R,
LMR base, quiet-move SEE margin) — all landed within 11 Elo of baseline, all discarded per
the pre-agreed decision criteria. Conclusion: single search-constant tuning is not where this
engine's remaining Elo deficit lives.

**Round 26** (2026-08-30): `gprof` (works in this sandbox; `perf` is blocked by
`perf_event_paranoid=4`) profiling found `evaluate()`+`updateAccumulator()` still at 83.5% of
total CPU time — eval was nowhere near played out. Diagnostic counters confirmed the Round 18
incremental algorithm itself was working perfectly (98.3% of calls take a 1-hop incremental
path). The actual bug: `addColumn()`/`subColumn()`/`refreshInto()` used a runtime class member
(`L1_`) directly as a loop bound, which GCC's vectorizer cannot prove as a stable trip count
("number of iterations cannot be computed") — the hottest loop in the engine was compiling
fully scalar despite being a textbook auto-vectorizable pattern. Fix: cache to a local
`const int` before each loop. nps: 368,821 → 526,342 (**+42.7%**, node counts identical both
runs, confirming zero behavioral change — also confirmed via the existing 188-check bit-exact
suite). This closes the nps gap with Fairy-Stockfish-NNUE entirely. Gauntlet Elo flat (-260,
delta -5) — deployed anyway (zero correctness risk, unlike Round 23). Two consecutive large nps
wins with ~0 combined Elo (Round 23 + Round 26) is strong evidence the next real lever is
eval/training quality, not engine speed.

### Central-Control Eval Experiment — Concluded Inconclusive, Not Merged (2026-08-30)

Added `centralControl()` to `makruk_eval.cpp`: an MG-only bonus for Khon (BISHOP) and Met
(QUEEN — which automatically covers promoted pawns too, since Makruk promotes to Met only with
no separate piece type) on a widening central zone (rank5 d/e, rank4 c/d/e/f, rank3
b/c/d/e/f/g), tiered by file (d/e=40cp, c/f=25cp, b/g=12cp — deliberately larger than this
file's other bonuses so the effect would be observable). Verified square-by-square via
`makrukeval` against the exact spec, including correct Black-side mirroring; new test
`testCentralControlTieredByFile` added.

Gauntlet result: **inconclusive** — seed=99 gave +11 Elo, seed=4242 gave -21 Elo (sign flip).
Per the standing decision criteria (see "Key Methodological Findings"), not deployed.

**Closed out (not pursued further) rather than iterated on.** Work is committed and pushed on
branch `classical-eval-central-control` (commit `b9da620`, not merged) — kept on GitHub for
reference, not deleted, in case the tier values/zone shape are worth revisiting later. If
resumed: candidates are different tier values/zone shape, extending the bonus to EG (currently
MG-only), or a different heuristic shape entirely — but per "Key Methodological Findings,"
any resumption should budget for a proper 2-seed test before trusting a first result, the same
mistake-cost this round already paid once.

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

Handicap-gauntlet numbers from Rounds 11-16 (roughly +111 to +382 Elo) are **not comparable**
to this table — different methodology, not true strength. See "Key Methodological Findings."

---

## Net Archive (most recent only; superseded nets are gitignored/untracked, kept on disk where noted)

* `src/3769833465.bin` — Round 23 VERSION3/MKN3 (int16 L2) — **currently embedded** (verify
  against `src/evaluate.h`, this can drift)
* `src/2020277415.bin` — Round 14 net, MKN2 int16, L1=512 — same underlying weights as
  3769833465.bin before L2 quantization, archived
* `src/150547052.bin` — Round 16 net (first book-paired round), inconclusive, untracked
* `tools/training/makruk_round12.pt`, `makruk_round15.pt` — regression/noise-affected
  checkpoints kept for potential future ablation, gitignored

Earlier nets (v1 through v9, Rounds 1-10) are fully superseded and not individually listed
here — see git history / the Development History section above for what each round changed.
