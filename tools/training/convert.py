#!/usr/bin/env python3
"""
convert.py — Convert Makruk-SF selfplay JSONL to NNUE training positions.

Reads selfplay.jsonl (one game record per line), replays each game with a
minimal Python Makruk board, and writes one training record per eligible
position.

Output TSV format (header line + one row per position):
    fen          Makruk FEN (piece placement, side-to-move, -, -, halfmove, fullmove)
    score_cp     Engine eval in centipawns from White's perspective
    wdl          Game result from White's view: 1.0 = W wins, 0.5 = draw, 0.0 = B wins
    decisive     1 if the game was decisive (non-draw), 0 if drawn
    mode         Self-play mode: normal / time_handicap / depth_handicap / random_opening
    score_source Always "engine_search" for now (future: "teacher_label")

Usage:
    python convert.py --input selfplay.jsonl --output train.tsv
    python convert.py --input selfplay.jsonl --output train.tsv --min-ply 16 --max-ply 400
    python convert.py --input selfplay.jsonl --output train.tsv --decisive-only

Backward compatibility:
    Existing JSONL records without the new fields (mode, decisive) are handled
    gracefully — 'mode' defaults to "normal" and 'decisive' is derived from result.

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

import argparse
import json
import sys
from pathlib import Path

from makruk_utils import MakrukBoard, WHITE


def _result_wdl(result: str) -> float:
    if result == '1-0':
        return 1.0
    if result == '0-1':
        return 0.0
    return 0.5


def convert(in_path: Path, out_path: Path,
            min_ply: int, max_ply: int, max_score: int,
            decisive_only: bool) -> None:
    games      = 0
    positions  = 0
    skipped_games = 0

    with open(in_path) as fin, open(out_path, 'w') as fout:
        fout.write('fen\tscore_cp\twdl\tdecisive\tmode\tscore_source\n')

        for raw in fin:
            raw = raw.strip()
            if not raw:
                continue
            rec = json.loads(raw)
            games += 1

            result = rec.get('result', '*')
            # Derive decisive flag from result if not stored explicitly.
            decisive = int(rec.get('decisive', result != '1/2-1/2'))
            mode     = rec.get('mode', 'normal')

            if decisive_only and not decisive:
                skipped_games += 1
                continue

            moves  = rec.get('moves', [])
            scores = rec.get('scores', [])
            wdl    = _result_wdl(result)
            board  = MakrukBoard(rec.get('start_fen',
                           'rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1'))

            for ply, move in enumerate(moves):
                if ply > max_ply:
                    break

                score = scores[ply] if ply < len(scores) else None

                if ply >= min_ply and score is not None and abs(score) <= max_score:
                    # Convert engine score from side-to-move to White's perspective.
                    score_white = score if board.stm == WHITE else -score
                    fout.write(
                        f'{board.to_fen()}\t{score_white}\t{wdl}\t'
                        f'{decisive}\t{mode}\tengine_search\n'
                    )
                    positions += 1

                board.apply_move(move)

    print(f'Games     : {games}')
    if decisive_only:
        print(f'Skipped   : {skipped_games} drawn games (--decisive-only)')
    print(f'Positions : {positions}')
    if games - skipped_games:
        print(f'Per game  : {positions / (games - skipped_games):.1f}')
    print(f'Output    : {out_path}')


def main() -> None:
    ap = argparse.ArgumentParser(
        description='Convert selfplay JSONL to NNUE training TSV',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument('--input',          required=True,
                    help='selfplay.jsonl produced by tools/selfplay/selfplay.py')
    ap.add_argument('--output',         required=True,
                    help='Output training.tsv')
    ap.add_argument('--min-ply',        type=int, default=16,
                    help='Skip positions before this ply (avoid opening noise)')
    ap.add_argument('--max-ply',        type=int, default=400,
                    help='Skip positions after this ply')
    ap.add_argument('--max-score',      type=int, default=2500,
                    help='Drop positions with |score_cp| above this (avoids near-terminal)')
    ap.add_argument('--decisive-only',  action='store_true',
                    help='Only include positions from decisive (non-draw) games')
    args = ap.parse_args()

    in_path = Path(args.input)
    if not in_path.exists():
        print(f'ERROR: not found: {in_path}', file=sys.stderr)
        sys.exit(1)

    convert(in_path, Path(args.output),
            args.min_ply, args.max_ply, args.max_score,
            args.decisive_only)


if __name__ == '__main__':
    main()
