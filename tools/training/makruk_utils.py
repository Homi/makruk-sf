"""
makruk_utils.py — Shared Makruk board and NNUE feature utilities.

Used by convert.py, train.py, and export.py.

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

# ── Piece constants ───────────────────────────────────────────────────────────
PAWN, KNIGHT, KHON, ROOK, MET, KING = 1, 2, 3, 4, 5, 6
WHITE, BLACK = 0, 1

# FEN character mappings (Makruk-SF: S/s = Khon, M/m = Met)
FEN_TO_PIECE: dict = {
    'P': (PAWN, WHITE),   'N': (KNIGHT, WHITE), 'S': (KHON, WHITE),
    'R': (ROOK, WHITE),   'M': (MET, WHITE),    'K': (KING, WHITE),
    'p': (PAWN, BLACK),   'n': (KNIGHT, BLACK), 's': (KHON, BLACK),
    'r': (ROOK, BLACK),   'm': (MET, BLACK),    'k': (KING, BLACK),
}
PIECE_TO_FEN: dict = {v: k for k, v in FEN_TO_PIECE.items()}


# ── Minimal Makruk board ──────────────────────────────────────────────────────

class MakrukBoard:
    """Parse FEN, apply UCI moves, emit FEN.  No legality checking."""

    __slots__ = ('board', 'stm', 'halfmove', 'fullmove')

    def __init__(self, fen: str) -> None:
        self.board: list = [None] * 64   # sq = rank*8 + file, rank-0 = board-rank-1
        self.stm = WHITE
        self.halfmove = 0
        self.fullmove = 1
        self._parse(fen)

    def _parse(self, fen: str) -> None:
        parts = fen.split()
        rank, file = 7, 0
        for ch in parts[0]:
            if ch == '/':
                rank -= 1
                file = 0
            elif ch.isdigit():
                file += int(ch)
            else:
                self.board[rank * 8 + file] = FEN_TO_PIECE[ch]
                file += 1
        self.stm = WHITE if parts[1] == 'w' else BLACK
        self.halfmove = int(parts[4]) if len(parts) > 4 else 0
        self.fullmove = int(parts[5]) if len(parts) > 5 else 1

    @staticmethod
    def _sq(s: str) -> int:
        """Convert 'a1'→0 .. 'h8'→63."""
        return (int(s[1]) - 1) * 8 + (ord(s[0]) - ord('a'))

    def apply_move(self, move: str) -> None:
        """Apply a UCI move (e.g. 'e2e4', 'a5a6', 'e5e6m').  Promotion suffix ignored."""
        fr = self._sq(move[0:2])
        to = self._sq(move[2:4])
        piece, color = self.board[fr]
        captured = self.board[to]
        self.board[to] = (piece, color)
        self.board[fr] = None
        # Auto-promote: white pawn → rank 6 (rank-idx 5), black → rank 3 (rank-idx 2)
        to_rank = to >> 3
        if piece == PAWN:
            if color == WHITE and to_rank == 5:
                self.board[to] = (MET, WHITE)
            elif color == BLACK and to_rank == 2:
                self.board[to] = (MET, BLACK)
        self.halfmove = 0 if (captured or piece == PAWN) else self.halfmove + 1
        if self.stm == BLACK:
            self.fullmove += 1
        self.stm ^= 1

    def to_fen(self) -> str:
        rows = []
        for rank in range(7, -1, -1):
            row, empty = '', 0
            for file in range(8):
                p = self.board[rank * 8 + file]
                if p is None:
                    empty += 1
                else:
                    if empty:
                        row += str(empty)
                        empty = 0
                    row += PIECE_TO_FEN[p]
            if empty:
                row += str(empty)
            rows.append(row)
        stm = 'w' if self.stm == WHITE else 'b'
        return f"{'/'.join(rows)} {stm} - - {self.halfmove} {self.fullmove}"

    def pieces(self) -> list:
        """Return [(square, piece_type, color)] for every piece on the board."""
        return [
            (sq, pt, c)
            for sq, entry in enumerate(self.board)
            if entry is not None
            for pt, c in (entry,)
        ]


# ── NNUE feature extraction ───────────────────────────────────────────────────
# Constants mirror half_ka_v2_makruk.h exactly.

_SQ_NB = 64
_PS_W_PAWN   =  0 * _SQ_NB
_PS_B_PAWN   =  1 * _SQ_NB
_PS_W_KNIGHT =  2 * _SQ_NB
_PS_B_KNIGHT =  3 * _SQ_NB
_PS_W_KHON   =  4 * _SQ_NB
_PS_B_KHON   =  5 * _SQ_NB
_PS_W_ROOK   =  6 * _SQ_NB
_PS_B_ROOK   =  7 * _SQ_NB
_PS_W_MET    =  8 * _SQ_NB
_PS_B_MET    =  9 * _SQ_NB
_PS_KING     = 10 * _SQ_NB
_PS_NB       = 11 * _SQ_NB   # 704

NNUE_DIMS = _SQ_NB * _PS_NB // 2   # 22 528

KING_BUCKETS: list[int] = [
    -1, -1, -1, -1, 31, 30, 29, 28,
    -1, -1, -1, -1, 27, 26, 25, 24,
    -1, -1, -1, -1, 23, 22, 21, 20,
    -1, -1, -1, -1, 19, 18, 17, 16,
    -1, -1, -1, -1, 15, 14, 13, 12,
    -1, -1, -1, -1, 11, 10,  9,  8,
    -1, -1, -1, -1,  7,  6,  5,  4,
    -1, -1, -1, -1,  3,  2,  1,  0,
]

# PieceSquareIndex base offsets, keyed by (piece_type, color).
# WHITE perspective: own pieces → PS_W_*, opponent → PS_B_*.
# BLACK perspective: own pieces → PS_W_*, opponent → PS_B_* (roles swapped).
# Both kings share PS_KING regardless of color.
_PS_TABLE: list[dict] = [
    # WHITE perspective
    {
        (PAWN,   WHITE): _PS_W_PAWN,   (KNIGHT, WHITE): _PS_W_KNIGHT,
        (KHON,   WHITE): _PS_W_KHON,   (ROOK,   WHITE): _PS_W_ROOK,
        (MET,    WHITE): _PS_W_MET,    (KING,   WHITE): _PS_KING,
        (PAWN,   BLACK): _PS_B_PAWN,   (KNIGHT, BLACK): _PS_B_KNIGHT,
        (KHON,   BLACK): _PS_B_KHON,   (ROOK,   BLACK): _PS_B_ROOK,
        (MET,    BLACK): _PS_B_MET,    (KING,   BLACK): _PS_KING,
    },
    # BLACK perspective (own/opp roles swapped, but KING stays same)
    {
        (PAWN,   BLACK): _PS_W_PAWN,   (KNIGHT, BLACK): _PS_W_KNIGHT,
        (KHON,   BLACK): _PS_W_KHON,   (ROOK,   BLACK): _PS_W_ROOK,
        (MET,    BLACK): _PS_W_MET,    (KING,   BLACK): _PS_KING,
        (PAWN,   WHITE): _PS_B_PAWN,   (KNIGHT, WHITE): _PS_B_KNIGHT,
        (KHON,   WHITE): _PS_B_KHON,   (ROOK,   WHITE): _PS_B_ROOK,
        (MET,    WHITE): _PS_B_MET,    (KING,   WHITE): _PS_KING,
    },
]


def _orient(perspective: int, s: int, ksq: int) -> int:
    """Orient square s from perspective, anchored on king square ksq.

    Mirrors vertically for BLACK (XOR 56 = SQ_A8) and horizontally when the
    king is on a–d files (XOR 7 = SQ_H1).  Matches half_ka_v2_makruk.cpp.
    """
    v = 56 if perspective == BLACK else 0      # SQ_A8
    h = 7  if (ksq & 7) < 4   else 0          # SQ_H1, king on a–d files
    return s ^ v ^ h


def make_nnue_index(perspective: int, s: int, piece: tuple, ksq: int) -> int:
    """Return the flat NNUE feature index for one piece from one king perspective."""
    oriented_ksq = _orient(perspective, ksq, ksq)
    return (
        _orient(perspective, s, ksq)
        + _PS_TABLE[perspective][piece]
        + _PS_NB * KING_BUCKETS[oriented_ksq]
    )


def flip_fen(fen: str) -> str:
    """Color-flip a Makruk FEN: reverse ranks, swap W↔B pieces, swap side-to-move.

    The result is a valid Makruk position from the opponent's perspective.
    score_cp and wdl should be negated / inverted by the caller.
    """
    parts = fen.split()
    ranks = parts[0].split('/')
    ranks.reverse()
    new_ranks = []
    for rank in ranks:
        new_rank = ''
        for ch in rank:
            if ch.isupper():
                new_rank += ch.lower()
            elif ch.islower():
                new_rank += ch.upper()
            else:
                new_rank += ch
        new_ranks.append(new_rank)
    new_board = '/'.join(new_ranks)
    new_stm = 'b' if parts[1] == 'w' else 'w'
    hm = parts[4] if len(parts) > 4 else '0'
    fm = parts[5] if len(parts) > 5 else '1'
    return f"{new_board} {new_stm} - - {hm} {fm}"


def get_active_features(board: MakrukBoard) -> tuple:
    """Return (white_features, black_features) as lists of active NNUE indices.

    Includes all pieces (both kings included, matching appendActiveIndices in C++).
    """
    pieces = board.pieces()
    wksq = next(sq for sq, pt, c in pieces if pt == KING and c == WHITE)
    bksq = next(sq for sq, pt, c in pieces if pt == KING and c == BLACK)

    w_feats: list[int] = []
    b_feats: list[int] = []
    for sq, pt, color in pieces:
        pc = (pt, color)
        w_feats.append(make_nnue_index(WHITE, sq, pc, wksq))
        b_feats.append(make_nnue_index(BLACK, sq, pc, bksq))

    return w_feats, b_feats
