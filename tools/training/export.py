#!/usr/bin/env python3
"""
export.py — Export a Makruk-SF NNUE checkpoint to a loadable binary.

The output is a custom float32 binary (NOT a Stockfish .nnue file).
Full quantized .nnue export (int16/int8 weights with Stockfish header) is a
future step once training data volume justifies quantization.

Binary layout (all values little-endian):
  Header (6 × uint32):
    magic    = 0x4D4B4E4E  ('MKNN')
    version  = 1
    dims     = NNUE_DIMS   (22528)
    l1, l2, l3             (256, 32, 32)
  Weights (float32, row-major):
    FT  bias     [L1]
    FT  weight   [L1 × DIMS]
    L2  bias     [L2]
    L2  weight   [L2 × 2*L1]
    L3  bias     [L3]
    L3  weight   [L3 × L2]
    out bias     [1]
    out weight   [1 × L3]

Requirements:
  pip install torch

Usage:
  python export.py --input makruk.pt --output makruk_net.bin
  python export.py --input makruk.pt --output makruk_net.bin --info

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

import argparse
import struct
import sys
from pathlib import Path

try:
    import torch
except ImportError:
    print('ERROR: PyTorch not installed.  Run: pip install torch', file=sys.stderr)
    sys.exit(1)

MAGIC   = 0x4D4B4E4E   # 'MKNN'
VERSION = 1


def _pack_floats(t: 'torch.Tensor') -> bytes:
    """Flatten tensor and pack as little-endian float32 bytes."""
    flat = t.float().cpu().detach().numpy().flatten()
    return struct.pack(f'<{len(flat)}f', *flat.tolist())


def export_model(pt_path: Path, bin_path: Path, info: bool) -> None:
    ckpt = torch.load(pt_path, map_location='cpu', weights_only=True)
    arch  = ckpt['arch']
    dims, l1, l2, l3 = arch['dims'], arch['l1'], arch['l2'], arch['l3']
    state = ckpt['model_state']
    epoch = ckpt['epoch']
    val   = ckpt['val_loss']

    print(f'Loaded   : {pt_path}')
    print(f'Epoch    : {epoch}   val_loss={val:.6f}')
    print(f'Arch     : dims={dims}  L1={l1}  L2={l2}  L3={l3}')

    if info:
        total = 0
        for name, t in state.items():
            n = t.numel()
            total += n
            print(f'  {name:<28s}: {list(t.shape)}  ({n} params)')
        print(f'  Total params: {total:,}')

    def w(name: str) -> 'torch.Tensor':
        return state[name].float().cpu()

    layers = [
        ('ft.bias',  w('ft.bias')),      # (L1,)
        ('ft.weight', w('ft.weight')),   # (L1, DIMS)
        ('l2.bias',  w('l2.bias')),      # (L2,)
        ('l2.weight', w('l2.weight')),   # (L2, 2*L1)
        ('l3.bias',  w('l3.bias')),      # (L3,)
        ('l3.weight', w('l3.weight')),   # (L3, L2)
        ('out.bias',  w('out.bias')),    # (1,)
        ('out.weight', w('out.weight')), # (1, L3)
    ]

    with open(bin_path, 'wb') as f:
        # Header
        f.write(struct.pack('<IIIIII', MAGIC, VERSION, dims, l1, l2, l3))
        # Weights
        for _name, tensor in layers:
            f.write(_pack_floats(tensor))

    size_kb = bin_path.stat().st_size / 1024
    print(f'Exported : {bin_path}  ({size_kb:.1f} KB)')
    print()
    print('NOTE: This is a float32 binary, not a Stockfish .nnue file.')
    print('      Quantized int16/int8 export for sf-kernel will be added')
    print('      in a future step once training data volume is sufficient.')


def main() -> None:
    ap = argparse.ArgumentParser(
        description='Export Makruk-SF NNUE checkpoint to binary',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument('--input',  required=True,
                    help='.pt checkpoint from train.py')
    ap.add_argument('--output', default='makruk_net.bin',
                    help='Output binary file')
    ap.add_argument('--info',   action='store_true',
                    help='Print per-layer parameter counts')
    args = ap.parse_args()

    pt_path = Path(args.input)
    if not pt_path.exists():
        print(f'ERROR: not found: {pt_path}', file=sys.stderr)
        sys.exit(1)

    export_model(pt_path, Path(args.output), args.info)


if __name__ == '__main__':
    main()
