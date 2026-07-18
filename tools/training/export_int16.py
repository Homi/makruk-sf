#!/usr/bin/env python3
"""
export_int16.py — Export Makruk-SF NNUE to MKN2 format.

MKN2 = int16 Feature Transformer, float32 L2/L3/out.

The FT weight matrix is stored TRANSPOSED compared to MKN1:
  MKN1: ft_weight[neuron * DIMS + input]  (non-contiguous column access)
  MKN2: ft_weight[input  * L1   + neuron] (contiguous column access, cache-friendly)

Binary layout (all values little-endian):
  Header (7 × uint32):
    magic    = 0x4D4B4E4E  ('MKNN')
    version  = 2
    dims     = NNUE_DIMS   (22528)
    l1, l2, l3             (256, 32, 32)
    ft_scale               (default 1024)
  Weights:
    FT  bias     [L1]          int16   (round(float * ft_scale))
    FT  weight   [DIMS × L1]   int16   (transposed, round(float * ft_scale))
    L2  bias     [L2]          float32
    L2  weight   [L2 × 2*L1]  float32
    L3  bias     [L3]          float32
    L3  weight   [L3 × L2]    float32
    out bias     [1]           float32
    out weight   [1 × L3]     float32

Quantization note:
  The FT accumulates at most MaxActiveDimensions (32) features.
  With ft_scale=1024 and max |weight|≈2.1:
    max int16 weight ≈ 2150 << 32767  (safe headroom)
    max int32 acc    ≈ 32 × 2150 = 68800 << 2^31  (no overflow)

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

import argparse
import struct
import sys
from pathlib import Path

try:
    import torch
    import numpy as np
except ImportError:
    print('ERROR: torch / numpy not installed.  Run: pip install torch numpy', file=sys.stderr)
    sys.exit(1)

MAGIC            = 0x4D4B4E4E   # 'MKNN'
VERSION          = 2
DEFAULT_FT_SCALE = 1024


def export_int16(pt_path: Path, bin_path: Path, ft_scale: int, info: bool) -> None:
    ckpt  = torch.load(pt_path, map_location='cpu', weights_only=True)
    arch  = ckpt['arch']
    dims, l1, l2, l3 = arch['dims'], arch['l1'], arch['l2'], arch['l3']
    state = ckpt['model_state']
    epoch = ckpt['epoch']
    val   = ckpt['val_loss']

    print(f'Loaded   : {pt_path}')
    print(f'Epoch    : {epoch}   val_loss={val:.6f}')
    print(f'Arch     : dims={dims}  L1={l1}  L2={l2}  L3={l3}')
    print(f'FT_SCALE : {ft_scale}')

    def w(name: str) -> 'torch.Tensor':
        return state[name].float().cpu()

    ft_b = w('ft.bias')    # (L1,)
    ft_w = w('ft.weight')  # (L1, DIMS) — checkpoint stores [neuron × input]

    print(f'FT weight: [{ft_w.min():.4f}, {ft_w.max():.4f}]  mean_abs={ft_w.abs().mean():.4f}')
    print(f'FT bias  : [{ft_b.min():.4f}, {ft_b.max():.4f}]  mean_abs={ft_b.abs().mean():.4f}')

    # Quantize FT bias to int16
    ft_b_i16 = ft_b.mul(ft_scale).round().clamp(-32768, 32767).to(torch.int16)

    # Transpose FT weight: (L1, DIMS) → (DIMS, L1) then quantize
    ft_w_t_i16 = ft_w.T.mul(ft_scale).round().clamp(-32768, 32767).to(torch.int16)  # (DIMS, L1)

    max_abs_w = ft_w_t_i16.abs().max().item()
    print(f'FT weight int16 max_abs={max_abs_w}  headroom={32767/max_abs_w:.1f}×')
    max_acc = 32 * max_abs_w + ft_b_i16.abs().max().item()
    print(f'Max int32 acc estimate (32 feats): {max_acc}  '
          f'(INT32_MAX headroom: {2**31/max_acc:.0f}×)')

    if info:
        for name, t in state.items():
            print(f'  {name:<28s}: {list(t.shape)}  abs_max={t.abs().max():.4f}')

    def pack_i16(t: 'torch.Tensor') -> bytes:
        return t.numpy().astype('<i2').tobytes()

    def pack_f32(t: 'torch.Tensor') -> bytes:
        return t.float().numpy().astype('<f4').tobytes()

    with open(bin_path, 'wb') as f:
        # Header: 7 × uint32
        f.write(struct.pack('<IIIIIII', MAGIC, VERSION, dims, l1, l2, l3, ft_scale))
        # FT: int16 (transposed weight)
        f.write(pack_i16(ft_b_i16))
        f.write(pack_i16(ft_w_t_i16))
        # L2 / L3 / out: float32 (identical to MKN1 layout)
        f.write(pack_f32(w('l2.bias')))
        f.write(pack_f32(w('l2.weight')))
        f.write(pack_f32(w('l3.bias')))
        f.write(pack_f32(w('l3.weight')))
        f.write(pack_f32(w('out.bias')))
        f.write(pack_f32(w('out.weight')))

    size_kb = bin_path.stat().st_size / 1024
    print(f'Exported : {bin_path}  ({size_kb:.1f} KB)')
    print('Format   : MKN2 (int16 FT transposed, float32 L2/L3/out)')


def main() -> None:
    ap = argparse.ArgumentParser(
        description='Export Makruk-SF NNUE checkpoint to MKN2 (int16 FT) format',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument('--input',    required=True, help='.pt checkpoint from train.py')
    ap.add_argument('--output',   default='makruk_net_i16.bin', help='Output .bin file')
    ap.add_argument('--ft-scale', type=int, default=DEFAULT_FT_SCALE,
                    help='Quantization scale: int16 = round(float × scale)')
    ap.add_argument('--info',     action='store_true', help='Print per-layer weight stats')
    args = ap.parse_args()

    pt_path = Path(args.input)
    if not pt_path.exists():
        print(f'ERROR: not found: {pt_path}', file=sys.stderr)
        sys.exit(1)

    export_int16(pt_path, Path(args.output), args.ft_scale, args.info)


if __name__ == '__main__':
    main()
