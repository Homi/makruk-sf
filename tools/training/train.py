#!/usr/bin/env python3
"""
train.py — Minimal PyTorch NNUE trainer for Makruk-SF.

Architecture matches half_ka_v2_makruk.h layout:
  FT  : Linear(22528 → L1=256) × 2 perspectives, CReLU  (shared weights)
  L2  : Linear(512 → 32),  CReLU
  L3  : Linear(32  → 32),  CReLU
  out : Linear(32  →  1),  Sigmoid

Training target (per position):
  target = lam * sigmoid(score_stm / 400) + (1 - lam) * wdl_stm
  where score_stm is the engine eval from the side-to-move's perspective
  and   wdl_stm   is the game result from the side-to-move's perspective.

Requirements:
  pip install torch

Usage:
  python train.py --input train.tsv --output makruk.pt
  python train.py --input train.tsv --output makruk.pt --epochs 50 --batch-size 512

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

import argparse
import csv
import sys
from pathlib import Path

try:
    import torch
    import torch.nn as nn
    from torch.utils.data import Dataset, DataLoader
    import torch.utils.data
except ImportError:
    print('ERROR: PyTorch not installed.  Run: pip install torch', file=sys.stderr)
    sys.exit(1)

from makruk_utils import WHITE, MakrukBoard, get_active_features, NNUE_DIMS

# Network sizes
L1, L2, L3 = 256, 32, 32


# ── Dataset ───────────────────────────────────────────────────────────────────

def _cp_to_prob(cp: float) -> float:
    """Centipawn → win probability, matching Stockfish's sigmoid scale."""
    return 1.0 / (1.0 + 10.0 ** (-cp / 400.0))


class MakrukDataset(Dataset):
    def __init__(self, path: str, lam: float = 0.5) -> None:
        self.lam = lam
        self.data: list[tuple] = []
        with open(path, newline='') as f:
            for row in csv.DictReader(f, delimiter='\t'):
                self.data.append((
                    row['fen'],
                    int(row['score_cp']),
                    float(row['wdl']),
                ))

    def __len__(self) -> int:
        return len(self.data)

    def __getitem__(self, idx: int):
        fen, score_white, wdl_white = self.data[idx]
        board = MakrukBoard(fen)
        w_feats, b_feats = get_active_features(board)

        # Convert White-perspective values to side-to-move perspective.
        if board.stm == WHITE:
            stm_feats, opp_feats = w_feats, b_feats
            score_stm = score_white
            wdl_stm   = wdl_white
        else:
            stm_feats, opp_feats = b_feats, w_feats
            score_stm = -score_white
            wdl_stm   = 1.0 - wdl_white

        target = self.lam * _cp_to_prob(score_stm) + (1.0 - self.lam) * wdl_stm
        return stm_feats, opp_feats, float(target)


def _collate(batch):
    """Build dense multi-hot tensors from variable-length index lists."""
    N = len(batch)
    stm_t = torch.zeros(N, NNUE_DIMS, dtype=torch.float32)
    opp_t = torch.zeros(N, NNUE_DIMS, dtype=torch.float32)
    tgt_t = torch.zeros(N, 1,         dtype=torch.float32)
    for i, (sf, of, t) in enumerate(batch):
        for f in sf:
            stm_t[i, f] = 1.0
        for f in of:
            opp_t[i, f] = 1.0
        tgt_t[i, 0] = t
    return stm_t, opp_t, tgt_t


# ── Network ───────────────────────────────────────────────────────────────────

class NNUENet(nn.Module):
    def __init__(self) -> None:
        super().__init__()
        # Shared feature transformer; applied to both king perspectives.
        self.ft  = nn.Linear(NNUE_DIMS, L1, bias=True)
        self.l2  = nn.Linear(2 * L1,    L2, bias=True)
        self.l3  = nn.Linear(L2,        L3, bias=True)
        self.out = nn.Linear(L3,         1, bias=True)

    def forward(self, stm: torch.Tensor, opp: torch.Tensor) -> torch.Tensor:
        # Accumulate both sides with CReLU, concat, pass through hidden layers.
        acc = torch.cat([self.ft(stm).clamp(0.0, 1.0),
                         self.ft(opp).clamp(0.0, 1.0)], dim=1)
        x = self.l2(acc).clamp(0.0, 1.0)
        x = self.l3(x).clamp(0.0, 1.0)
        return torch.sigmoid(self.out(x))


# ── Training loop ─────────────────────────────────────────────────────────────

def train(args) -> None:
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f'Device   : {device}')
    print(f'Dims     : {NNUE_DIMS}   FT→{L1}  L2→{L2}  L3→{L3}→1')

    dataset = MakrukDataset(args.input, lam=args.lam)
    n = len(dataset)
    print(f'Samples  : {n}')
    if n == 0:
        print('ERROR: empty dataset', file=sys.stderr)
        sys.exit(1)

    val_n   = max(1, min(n // 10, 10_000))
    train_n = n - val_n
    train_ds, val_ds = torch.utils.data.random_split(
        dataset, [train_n, val_n],
        generator=torch.Generator().manual_seed(42),
    )

    train_dl = DataLoader(train_ds, batch_size=args.batch_size, shuffle=True,
                          collate_fn=_collate, num_workers=0)
    val_dl   = DataLoader(val_ds,   batch_size=args.batch_size, shuffle=False,
                          collate_fn=_collate, num_workers=0)

    model   = NNUENet().to(device)
    opt     = torch.optim.Adam(model.parameters(), lr=args.lr)
    loss_fn = nn.BCELoss()

    best_val = float('inf')
    for epoch in range(1, args.epochs + 1):
        model.train()
        train_loss = 0.0
        for stm, opp, tgt in train_dl:
            stm, opp, tgt = stm.to(device), opp.to(device), tgt.to(device)
            opt.zero_grad()
            loss = loss_fn(model(stm, opp), tgt)
            loss.backward()
            opt.step()
            train_loss += loss.item() * stm.size(0)
        train_loss /= train_n

        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for stm, opp, tgt in val_dl:
                stm, opp, tgt = stm.to(device), opp.to(device), tgt.to(device)
                val_loss += loss_fn(model(stm, opp), tgt).item() * stm.size(0)
        val_loss /= val_n

        marker = ' *' if val_loss < best_val else ''
        print(f'Epoch {epoch:3d}/{args.epochs}'
              f'  train={train_loss:.6f}  val={val_loss:.6f}{marker}')

        if val_loss < best_val:
            best_val = val_loss
            torch.save({
                'epoch': epoch,
                'model_state': model.state_dict(),
                'val_loss': val_loss,
                'arch': {'dims': NNUE_DIMS, 'l1': L1, 'l2': L2, 'l3': L3},
            }, args.output)

    print(f'Best val : {best_val:.6f}')
    print(f'Saved    : {args.output}')


def main() -> None:
    ap = argparse.ArgumentParser(
        description='Makruk-SF NNUE trainer',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument('--input',      required=True,
                    help='train.tsv from convert.py')
    ap.add_argument('--output',     default='makruk.pt',
                    help='Output PyTorch checkpoint (.pt)')
    ap.add_argument('--epochs',     type=int,   default=20)
    ap.add_argument('--batch-size', type=int,   default=256)
    ap.add_argument('--lr',         type=float, default=1e-3,
                    help='Adam learning rate')
    ap.add_argument('--lam',        type=float, default=0.5,
                    help='Score weight in target: 1.0=score-only, 0.0=result-only')
    args = ap.parse_args()
    train(args)


if __name__ == '__main__':
    main()
