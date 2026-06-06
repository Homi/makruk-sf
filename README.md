# Makruk-SF

Makruk-SF is a Thai Chess (Makruk) engine project derived from **sf-kernel**, a reduced and cleaned Stockfish-derived engine core.

The goal of this project is to build a practical Makruk engine with:

- Correct Makruk move generation
- Correct Makruk promotion rules
- Complete Makruk counting rules
- Makruk-specific evaluation
- Makruk NNUE support
- Self-play data generation
- Training pipeline for a strong Makruk NNUE network
- Benchmarking against Fairy-Stockfish and Fairy-Max Makruk engines

## Status

This project is currently in early development.

Current focus:

1. Rename and prepare the project structure.
2. Convert chess rules into Makruk rules.
3. Implement Makruk counting rules.
4. Add Makruk-specific evaluation.
5. Add Makruk NNUE feature transformer.
6. Build self-play and training infrastructure.

## Project Author / Maintainer

Makruk-SF modifications and Makruk-specific development:

- Homi
- Email: bhome1@hotmail.com

## Based On

This project is derived from:

- sf-kernel  
  https://github.com/FireFather/sf-kernel

sf-kernel itself is a reduced and cleaned engine core derived from:

- Stockfish  
  https://github.com/official-stockfish/Stockfish

## License

Makruk-SF is distributed under the **GNU General Public License version 3 (GPLv3)**.

This project is derived from sf-kernel / Stockfish, which are GPLv3-licensed projects.  
Therefore, Makruk-SF remains licensed under GPLv3.

See the `LICENSE` file for the full GPLv3 license text.

## Attribution

This project contains code and design inherited from sf-kernel and Stockfish.

Original upstream projects:

- Stockfish contributors
- sf-kernel contributors

Makruk-SF-specific modifications are maintained by:

- Homi <bhome1@hotmail.com>

## Modification Notice

Makruk-SF is not the original sf-kernel project and is not the original Stockfish project.

This project modifies the original chess engine core to support Thai Chess (Makruk), including but not limited to:

- Makruk piece movement
- Makruk starting position
- Makruk pawn promotion
- Removal of chess-only rules such as castling, en passant, and pawn double-step
- Makruk counting rules
- Makruk-specific evaluation
- Makruk NNUE feature design
- Makruk self-play and training tools

## Planned Makruk Rules

Makruk-SF intends to implement Makruk rules based primarily on Fairy-Stockfish's Makruk variant behavior, with additional Makruk-specific features where useful.

Planned rules include:

- No castling
- No en passant
- No two-square pawn move
- Pawn promotion to Met only
- White pawns start on rank 3 and promote on rank 6
- Black pawns start on rank 6 and promote on rank 3
- Stalemate is draw
- Makruk counting rules
- Makruk-specific FEN and notation compatibility

## Development Roadmap

### Phase 1: Project Preparation

- Rename project from sf-kernel to Makruk-SF
- Update README, NOTICE, and documentation
- Keep GPLv3 license and upstream attribution
- Confirm clean build on Windows and Linux

### Phase 2: Makruk Rules

- Implement Makruk start position
- Replace chess Bishop semantics with Khon
- Replace chess Queen semantics with Met
- Implement Makruk pawn movement and promotion
- Remove castling, en passant, and pawn double-step
- Add Makruk perft tests

### Phase 3: Counting Rules

- Implement Makruk counting state
- Add counting eligibility checks
- Add draw adjudication
- Add debug output for counting status

### Phase 4: Makruk Evaluation

- Add Makruk material values
- Add piece-square tables for Makruk
- Add pawn promotion race evaluation
- Add Khon, Met, and rook-specific heuristics
- Add endgame and counting-aware evaluation

### Phase 5: Makruk NNUE

- Add Makruk NNUE feature transformer
- Add Makruk-specific feature mapping
- Add NNUE loading and validation
- Add Makruk network training pipeline

### Phase 6: Self-Play and Training

- Add self-play generator
- Add position extraction tools
- Add teacher labeling tools
- Add dataset converter
- Add training scripts
- Add benchmark scripts

## Build

Build instructions will be updated as the project is converted from sf-kernel to Makruk-SF.

Initial expected build targets:

- Visual Studio / MSVC on Windows
- MSYS2 MinGW on Windows
- GCC / Clang on Linux

## Disclaimer

Makruk-SF is an independent derived project.

It is not affiliated with, endorsed by, or maintained by the original Stockfish or sf-kernel authors.

This software is provided under the terms of the GNU GPLv3 and comes with no warranty.