# Makruk Opening Books

`makruk.epd` and `makruk_no_counting.epd` are opening books in EPD format,
sourced verbatim from the [fairy-stockfish/books](https://github.com/fairy-stockfish/books)
repository (GNU GPLv3), generated with
[fairy-stockfish/bookgen](https://github.com/fairy-stockfish/bookgen) — an
opening book generator based on Fairy-Stockfish using perft/multipv search
with balance filtering.

66,745 diverse Makruk starting positions per file. `makruk.epd` includes
Fairy-Stockfish's counting-related EPD fields; `makruk_no_counting.epd` uses
the standard placeholder fields. Both use the same FEN piece-placement
format as Makruk-SF's own position parser (no conversion needed).

Used by `tools/gauntlet.py --book` to sample diverse opening positions for
engine-strength testing, replacing/supplementing the small MultiPV-based
random-opening pool that was found to introduce large (~46 Elo) seed-to-seed
variance in gauntlet results (see `CLAUDE.md`, Round 15 follow-up).

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
