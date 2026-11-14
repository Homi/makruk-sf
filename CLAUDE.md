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
so classical eval directly and significantly shapes real play at all times. Both the gate
(300cp) and cap (±100cp) have each been re-tested, in both directions, multiple times
(Rounds 7, 30, 34, 40-41) — every variant regressed; 300/100 is a confirmed local optimum.

Material values (MG/EG): Pawn 126/208, Knight 781/854, Khon 660/640, Rook 1276/1380, Met
420/450, King 0. Implemented heuristics: PST tables (all piece types), rook activity
(open/semi-open file), king safety (pawn shield, MG only), KBK/mopup endgame guidance
(push weak king to a Khon-reachable corner, close own king), promotion-race urgency (passed
pawns near promotion, Round 35), insufficient-material draw detection. Tried and **not
merged** (see Development History for each): central-control bonus (inconclusive), counting-
danger awareness (regression, and later found to be measuring dead code — see Round 33),
rook-trap risk (regression), piece-mobility bonus (regression), full 4-table correction
history (regression).

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
Current architecture is L1/L2/L3 = 512/32/32 — larger architectures (1024/32/32, 1024/64/64)
have both been tried and both confirmed regressed (Rounds 31, 43); see Development History.

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

## Current Status (as of Round 45, 2026-11-08)

* **Embedded net**: `src/3587509696.bin` (Round 27 — nothing since has beaten it, see
  Benchmark History). Check `src/evaluate.h`'s `NnueNetDefaultName` for what's actually
  shipped, this note can drift.
* **Target**: -150 Elo vs Fairy-Stockfish-NNUE (user-set, 2026-10-10) — needs **+50 to +55
  Elo** from the current -205/-200, a single-round-sized gain by this project's own history
  (Round 21: +64; Round 27: +55/+60), not a multi-round accumulation.
* **True strength** (book-paired, equal-condition 200ms/move vs Fairy-Stockfish-NNUE — the
  only trustworthy Elo methodology here, see "Handicap gauntlets are not true strength"
  below): progressed **-372 → -260 → -205/-200** across Rounds 18-27 (engine-side fixes, then
  a training-data pivot), then held at -205/-200 through Rounds 28-45 — **18 straight
  rounds** across every lever tried (data scale-up/freshness, search correction history,
  blend-gate tuning both directions, NNUE architecture scaling, teacher-depth/volume
  trade-off, three classical-eval additions stacked three ways, and a training-objective
  change) have failed to beat it. Full detail per round: Development History below;
  numbers: Benchmark History below.
* **Next priority**: the training-objective-change hypothesis (Round 45) is closed only in
  its tested direction (anneal toward WDL-dominance); the *reverse* anneal (start
  WDL-dominant, finish cp-dominant) is genuinely untested. Lower-priority, none a quick win:
  further magnitude tuning of Round 35's promotion-urgency constant; Round 29's untuned
  pawn-only correction-history constants, un-swept if revisited; a rook-trap-risk version
  that checks actual capturability instead of raw mobility.
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
  or discarding. A sign-flip between seeds means "inconclusive," not "small effect" — escalate
  to a third seed (Round 35's precedent) rather than guessing.
* **Book-pairing shrinks but does not eliminate this noise.** `tools/books/` (from
  fairy-stockfish/books, 66,745 positions) + `gauntlet.py --book` pairs each opening across
  both colors, canceling color/first-move bias — demonstrated to shrink one net's seed-to-seed
  spread from 46 Elo to 11 Elo (Round 16) — but sampling variance from *which* book positions
  get drawn remains a real, uncontrolled noise source.
* **val_loss does not predict Elo.** Demonstrated repeatedly — now 10+ independent instances
  (Rounds 9, 10, 15, 16, 28, 31, 42, 43, 44, 45): better val_loss has meant better, worse, or
  unchanged Elo in different rounds, and even a round with an *incomparable* val_loss (Round
  45, different validation target) still needed the gauntlet as the real signal. Blend-range
  (near-equal position) discrimination — not raw prediction accuracy — is what actually moves
  Elo, because the blend is gated to `|classical| < 300`.
* **nps improvements do not reliably translate to Elo** once search is already reasonably deep.
  Round 23 (+34% nps) and Round 26 (+42.7% nps) both landed within noise of flat. Diff-audit the
  actual bottleneck (`gprof` works in this sandbox; `perf` does not —
  `perf_event_paranoid=4`) before assuming eval speed is still the constraint.
* **Stacking a second eval/search term on top of a working one tends to erode it, not compound
  it.** Three independent additions stacked on Round 35's kept promotion-race heuristic (Round
  36 rook-trap risk, Round 37 piece mobility, Round 38 full correction history) all made things
  *worse* than Round 35 alone, despite being mechanically unrelated to each other. Treat a new
  idea as its own isolated test against the current standing candidate, not a free compounding
  stack.
* **`gh pr create`/`gh repo view` default to the wrong repo** (the real public upstream
  `FireFather/sf-kernel`, not this project's fork `Homi/makruk-sf`) — has caused 3 accidental
  PRs against the real upstream maintainer's repo historically. Structurally resolved
  2026-09-13 (left the GitHub fork network), but still **always** verify `--repo Homi/makruk-sf`
  and the returned PR URL's owner before reporting success. Never open a PR against upstream.
* **Never run CPU-competing work (builds, test suites, a second gauntlet) concurrently with a
  movetime-based gauntlet** — contention silently distorts effective search depth and produced
  a spurious -407 Elo reading in Round 25 before this was caught.
* **Background processes**: use `setsid` (not bare `nohup`+`disown`) for long-running gauntlets/
  training, with a persistent Monitor or background-task watcher — a sandbox restart once
  silently killed a `nohup`+`disown`'d background pair mid-run. `gauntlet.py` has no
  `--resume` (unlike `train.py`'s `--resume`) — an interrupted gauntlet run must restart from
  scratch, not be patched up.
* **Always match the reference round's exact gauntlet flags, not just movetime/book/seed** —
  omitting `--adjudication-threshold 500 --adjudication-streak 5` in Round 27's first attempt
  produced a spurious "100% draws, Elo 0" result on *both* the new and the old net (confirmed
  via a same-flags control run), because without adjudication a competent engine at equal
  200ms/move essentially never gets checkmated in Makruk — every game just plays out to a
  counting draw. Check a prior round's actual saved log before assuming which flags apply,
  don't reconstruct the command from memory/docs alone.

---

## Development History

Full narrative detail for every round lives in git history (`git log -- CLAUDE.md`); this
section keeps only the mechanism, the quantified result, and the decision for each. See
"Benchmark History" for the Elo table and "Net Archive" for which checkpoints are kept on disk.

### Training pipeline era (pre-Round-17, foundational bugs — historical reference)

PR11 added the core tooling still in use today: `tools/gauntlet.py`, `tools/selfplay/
selfplay.py`'s handicap modes, `tools/training/convert.py`/`dataset_summary.py`. A cascade of
bugs was found and fixed, each blocking meaningful net evaluation until resolved: counting
draw never wired into game logic (66% stalemate rate, fixed by connecting `isDraw()` to
`isMakrukCountingDraw()`); 0% decisive self-play even with handicaps, fixed with score
adjudication (`--adjudication-threshold`/`-streak`, decisive rate 0%→65-80%);
`gauntlet.py`'s exact-zero draw detection (fixed with a tolerance-based check); NNUE never
actually loading (wrong magic-number check — *every* pre-fix gauntlet number measured
classical eval, not NNUE); once active, NNUE was a **-471 Elo regression** from 3 compounding
bugs (wrong sigmoid-decode, CReLU/ReLU train/inference mismatch, counting-compressed labels),
fixed by correcting decode/CReLU and adding `--color-augment` + imbalanced-opening data.
**Pure NNUE (no classical blend) was never viable** in any config tried — the counting draw
compresses endgame labels to ≈0, so the net never learns unambiguous material advantage;
`UseCounting`/`--no-counting` lets data-gen disable counting for real material-scale labels
(data-generation only, never for real play). A depth≥9 SIGSEGV surfaced during this work,
never confirmed reproducible — hardened defensively, not fixed.

**Net-quality rounds 11-16**: closer-handicap self-play (opponent depth 4-5) harvested
near-equal training data far more efficiently (84-87% vs 63.5% acceptance). A scale-up
(Round 12) regressed sharply; an ablation (Round 13) isolated the closest-handicap config as
the cause (noisier near-parity labels dilute signal). Rounds 13-14 scaled the validated
configs cleanly. Round 15's apparent regression was mostly single-seed noise — motivated
adding opening-book pairing (`tools/books/`). Round 16, the first book-paired round, was
sign-flipped/inconclusive, not deployed.

### C++ correctness/performance era (Rounds 17-26)

* **17**: SIGSEGV re-investigated, still unreproduced. Added `tools/build_verify.sh`.
* **18**: First true equal-condition test — baseline **-372 Elo**, not the historical
  handicap-gauntlet +111/+382. Fixed `MknnEvaluator` recomputing its NNUE accumulator from
  scratch every call (`MknnAccumulator`, incremental, bit-exact). -372→-359.
* **19-20**: Found 2 real `seeGe()` bugs (wrong Khon/Met attacker detection; wrong
  attacker-check order). Fixed, 2165-check regression test. -359→-346.
* **21**: GCC's auto-vectorizer left the L2 reduction loop scalar (needed float-reassociation
  permission). Scoped fast-math fix + Makefile ARCH fix. nps ~4×. -346→-282 (**+64 Elo**,
  confirmed +90 at a 2nd seed — largest single-round gain in this project's history).
* **22**: Eliminated heap allocations in `evaluate()`. nps +6.3%. -282→-269 (+13).
* **23**: VERSION3 (MKN3) int16-quantized L2. nps +34%. Elo inconclusive (+14/+0) — deployed
  on nps+correctness grounds alone.
* **24-25**: `search.cpp`/`movepick.cpp`/`timeman.cpp` confirmed byte-identical to upstream.
  3 search-constant candidates tested (null-move R, LMR base, quiet SEE margin) — all within
  11 Elo of baseline, discarded.
* **26**: `addColumn`/`subColumn`/`refreshInto` used a runtime member directly as a loop
  bound, blocking auto-vectorization. 3-line fix. nps +42.7%, bit-exact. Elo flat (-260,
  delta -5) — deployed anyway (zero correctness risk). Two consecutive large nps wins
  (23+26) with ~0 combined Elo confirmed nps had plateaued as a lever.

### Central-Control Eval Experiment — inconclusive, not merged

Khon/Met central-zone MG bonus, tiered by file. Gauntlet sign-flipped (+11/-21) — not
deployed. Reference branch deleted; would need reimplementing from scratch if resumed.

### Round 27 — Fresh Training Data from the Post-Round-26 Engine (2026-09-13) — DEPLOYED

Regenerated self-play data with the current (post-18-26: ~10x faster, SEE-correct) engine
instead of continuing to build on data from the old, much weaker one. Found the Rounds 11-14
handicap configs no longer produced the intended gap at this speed (97-99% draw rate) —
recalibrated by cutting `movetime-a` 200→30ms (draw rate fixed, acceptance 44.8%→53.1%).
Combined 3 configs → 267,971 positions, trained 60 epochs, val_loss 0.553515.

| Seed | Result | Elo |
|---|---|---|
| 99 | 0W 94D 106L | **-205** |
| 4242 | 0W 96D 104L | **-200** |
| Round 26 baseline | 0W 73D 127L | -260 |

**+55/+60 Elo**, confirmed at 2 seeds. Deployed via PR #20 — the standing net ever since.
(Process note: first attempt omitted adjudication flags, giving a spurious 100%-draw/Elo-0
reading on both nets — caught via a same-flags control run.)

### Round 28 — Scaling Up Round 27's Recipe (2026-09-14) — inconclusive

3× scale-up of Round 27's recalibrated config (450 games). Acceptance improved (67.6% vs
53.1%), val_loss slightly worse (0.555746). Gauntlet sign-flipped: seed 99 -217 (Δ-12), seed
4242 -198 (Δ+2). **Inconclusive, not deployed.** Takeaway: higher acceptance/bigger dataset
doesn't automatically transfer to Elo; scaling a working recipe isn't a guaranteed lever.

### Round 29 — Pawn Correction History Backport (2026-09-14) — small regression

Backported Stockfish's static-eval bias-correction mechanism, scoped to pawn-only (no
`materialKey()`/`minorPieceKey()`/`nonPawnKey()` existed in this codebase — building them was
deferred to Round 38). Untuned constants. Gauntlet: -220/-222 (Δ-15/-22), both seeds agree
(negative), tight agreement. **Small consistent regression, not deployed** (kept on branch
`pawn-correction-history`, not merged). Found, out of scope: a directly-constructed `Thread`
object crashes even on unmodified code (pre-existing, unrelated).

### Round 30 — Blend Gate Widened 300→450cp (2026-09-14) — confirmed regression

`evaluate.cpp`'s classical/NNUE blend gate widened to let the stronger Round 27 net
participate in more positions. Gauntlet: -225/-249 (Δ-20/**-49**, the -49 clears the noise
band). **Confirmed regression, reverted.** Reaffirms the net still underestimates material
relative to classical even after Round 27.

### Round 31 — NNUE L1 Doubled 512→1024 (2026-10-10) — confirmed regression

`MknnMaxL1` 512→1024, same Round 27 dataset, isolating capacity as the only variable. val_loss
~unchanged (0.553548). Gauntlet: -249/-269 (Δ-44/**-69**, clears the noise band). **Confirmed
regression, not deployed** (infra kept on branch `nnue-l1-1024`). Completes all four of Round
28's levers (data scale-up, correction history, blend gate, NNUE L1) — none beat Round 27.

### Round 32 — Classical Eval: Counting-Danger Awareness (2026-10-10) — regression, later found moot

Scaled the mop-up bonus up to +50% as the counting-draw ply limit approaches. Gauntlet:
-228/-228 (Δ-23/-28, identical at both seeds). **Not deployed.** Round 33 (below) then found
`evaluate.cpp` had a pre-existing bug making this entire code branch dead during real search
— the regression measured here was almost certainly noise on an inert path, not a real effect.

### Round 33 — Fix: `evaluate.cpp` No Longer Blinds Counting-Eligible Endgames (2026-10-11) — kept on correctness grounds

Found `evaluate.cpp` returned flat `VALUE_DRAW` for the *entire duration* of any
counting-eligible endgame (not just the terminal draw ply), making
`makrukClassicalEval()`'s mop-up/KBK logic dead code during real search since the first
Makruk-eval commit — predates Round 18, so every historical Elo number was measured with this
bug in place. Fixed: condition changed to `isMakrukCountingDraw(...)`. Gauntlet: 0/-30,
inconclusive by the project's bar. **Kept anyway on correctness grounds** (2026-10-11): fixes
a real logic error with zero correctness risk; an unconfirmed gauntlet result doesn't make a
known bug "correct to leave in."

### Round 34 — Blend Gate Narrowed 300→200cp, on Round 33's fix (2026-10-11/16) — small regression

Untested-opposite direction of Round 30, now with the bare-king branch live. Gauntlet:
-220/-222 (Δ-15/-22, tight agreement). **Not deployed.** Both gate directions now regress —
closes out blend-gate tuning as a lever (reconfirmed later with Round 35 active, Rounds 40-41).

### Round 35 — Classical Eval: Promotion-Race Urgency (2026-10-16) — KEPT, strongest signal since Round 27

Added a tiered eg bonus for passed pawns near Makruk's short 3-rank promotion
(`promotionRaceUrgency()`, reusing existing passed-pawn helpers). Gauntlet: seed 99 -198
(Δ+7), seed 4242 -175 (Δ+25) — both positive but neither clears the noise band, so a third
seed (777) was run with its own same-seed baseline: Δ+29. **All 3 seeds positive (avg +20)**
— not yet "confirmed" by the strict single-round bar, but the strongest multi-seed signal
since Round 27. **Kept** on branch `classical-eval-new-heuristics`, not merged to main.

### Round 36 — Rook-Trap Risk Stacked on Round 35 (2026-10-16) — regression, reverted

Mobility-based rook-danger penalty, stacked on Round 35. Pulled both seeds down relative to
Round 35 alone (+7→0, +25→-25). **Reverted.** Closes out the classical-eval "not yet
implemented" list (counting-danger, promotion-race, rook-trap) — only promotion-race showed
a positive signal, and it stands alone.

### Round 37 — Piece Mobility Bonus Stacked on Round 35 (2026-10-16) — regression, reverted

General mobility bonus for Knight/Khon/Rook/Met, stacked on Round 35. Both seeds worse than
Round 35 alone (Δ-30/-37). **Reverted.** Second stacking attempt in a row that erodes rather
than compounds the signal.

### Round 38 — Full 4-Table Correction History, Stacked on Round 35 (2026-10-17) — worst stacking regression, reverted

Completed the material/minor-piece/non-pawn Zobrist-key infrastructure Round 29 deferred
(full-recompute, not incremental, to eliminate the incremental-divergence bug class;
validated via 1230+ random-walk cross-checks). Both seeds worse than Round 35 alone
(Δ-60/-47) — the largest of the three stacking regressions despite the most careful
implementation. **Reverted completely** (10 files back to pre-Round-38 state; infra stays in
git history, 3 commits, not in the active state).

### Round 39 — Promotion-Race Urgency, Doubled Magnitude (2026-10-17) — sign-flipped, inconclusive

Doubled Round 35's `PromotionUrgency` constants. Gauntlet: seed 99 Δ+19, seed 4242 Δ-58 vs
Round 35 — clean sign-flip. **Not adopted**, reverted to 1× magnitude. Whether Round 35's
magnitude is well-calibrated remains genuinely open, not closed.

### Rounds 40-41 — Blend Gate Re-Tested With Round 35 Active (2026-10-17) — both regress again

Re-tested both gate directions (narrow 200, wide 450) with Round 35's scoring active, since
it could plausibly shift which positions fall in/out of the gate range. Both confirmed
regressions vs Round 35 alone (narrow: Δ-27/-45; wide: Δ-43/-37), matching Rounds 30/34's
original sign. **Both reverted to gate=300.** Closes the blend-gate-interaction question —
300cp is a genuine local optimum independent of Round 35.

### Round 42 — Fresh Training Data from the Post-Round-33/35 Engine (2026-10-18) — small regression

Track A of a 2-track plan: regenerated Round 27's exact 3-config self-play recipe with the
current (post-33/35) engine → 281,690 positions (55.9% acceptance), same architecture.
val_loss 0.560082. Gauntlet: -225/-230 (Δ-20/-30). **Not deployed.** Unlike Round 27's original
10x-engine-speed jump, the Round 33/35 changes were too minor to move self-play quality enough.

### Round 43 — NNUE L1=1024/L2=64/L3=64 (2026-10-24) — worst architecture regression

Track B: paired the wider L1 (Round 31) with wider L2/L3 too, same Round 27 dataset. val_loss
0.553249 (near-identical to Round 27/31). Gauntlet: -279/-282 (Δ**-74**/**-82**, both seeds
individually clear the noise band — the strongest-evidence regression in this project's
history). **Not deployed** (infra on branch `nnue-l1024-l2l3-64-round43`). Closes out NNUE
architecture-scaling entirely: more capacity, in any combination tried, hurts rather than helps.

### Round 44 — Teacher-Depth vs Data-Volume Trade-off (2026-11-01) — small regression, flat vs Round 42

Tested trading teacher-label precision for self-play volume: depth=8 labeling (vs the
standing depth=10) on ~4x Round 42's volume (1,800 games → 324,783 total positions, the
largest dataset in this project's history). val_loss 0.568544. Gauntlet: -217/-233 vs Round 27
(Δ-12/-33, small consistent regression) but roughly flat vs Round 42 itself (Δ+8/-3) — the
depth/volume trade-off doesn't move Elo at this scale. **Not deployed.** Closes out both of
the two NNUE/training levers identified after Round 43 (architecture scaling; depth/volume).

### Round 45 — Lambda-Annealed WDL Blend, Training-Objective Change (2026-11-08) — small regression

First change to the training *objective* itself (not data or architecture), motivated by
"val_loss never predicts Elo" holding across 9 rounds. `train.py`'s cp/WDL target blend was a
constant `lam=0.7` every round ever run; refactored to anneal per-epoch,
`lam_t = lam_start + (lam_end-lam_start)*(epoch-1)/(epochs-1)`, tested 0.7→0.3 (toward
WDL-dominance) on Round 27's exact dataset. Defaults (`--lam-start`/`--lam-end` both 0.7)
reproduce the old constant-blend path bit-for-bit, so the refactor is kept directly on this
branch rather than split off. val_loss 0.483230 (not comparable to Round 27's — the
validation target itself changed). Gauntlet: -244/-230 (Δ-39/-30, both seeds negative,
9 Elo apart). **Not deployed.** Closes the training-objective hypothesis in this tested
direction; the reverse anneal (start WDL-dominant, finish cp-dominant) remains untested.
This also marks 18 straight rounds (28-45) that have failed to beat Round 27.

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
| Round 45 (lambda-annealed WDL blend, 0.7→0.3, same Round 27 dataset) | -244/-230 | -39/-30 | both seeds negative, 9 Elo apart — small consistent regression (same pattern as 29/32/34/42/44), not deployed; closes out the training-objective-change hypothesis in this tested direction |

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
* `tools/training/makruk_round45.pt` — Round 45 checkpoint (lambda-annealed WDL blend,
  lam 0.7→0.3, Round 27's exact dataset, architecture unchanged 512/32/32), val_loss 0.483230
  (not comparable to Round 27's — validation target itself changed), Elo -244/-230 (2-seed,
  small consistent regression), not deployed, gitignored, kept for potential future ablation.
  `src/376020885.bin` (the exported candidate net, superseded) also kept on disk, gitignored.
* Round 23's net (formerly tracked as `src/3769833465.bin`, VERSION3/MKN3 int16 L2) was
  removed from the repo (2026-11-14) — not needed to build, since only the embedded net
  above is; still recoverable from git history if needed for reference.
* `src/2020277415.bin` (Round 14, MKN2 int16, pre-L2-quantization) and `src/150547052.bin`
  (Round 16, first book-paired net, inconclusive) are likewise superseded and not tracked.
* `tools/training/makruk_round12.pt`, `makruk_round15.pt` — regression/noise-affected
  checkpoints kept for potential future ablation, gitignored

Earlier nets (v1 through v9, Rounds 1-10) are fully superseded and not individually listed
here — see git history / the Development History section above for what each round changed.
