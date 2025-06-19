#include "zobrist.h"
#include <stdlib.h>

zobrist_table_t zobrist;

static uint64_t random_u64() {
    return ((uint64_t)rand() << 32) | rand();
}

void zobrist_init() {
    srand(ZOBRIST_SEED);

    for (int color = 0; color < 2; ++color) {
        for (int piece = 1; piece <= 6; ++piece) {
            for (int sq = 0; sq < BOARD_SIZE; ++sq) {
                if (IS_VALID_SQUARE(sq)) {
                    zobrist.piece_keys[color][piece][sq] = random_u64();
                }
            }
        }
    }

    for (int sq = 0; sq < BOARD_SIZE; ++sq) {
        if (IS_VALID_SQUARE(sq)) {
            zobrist.en_passant_keys[sq] = random_u64();
        }
    }

    for (int i = 0; i < 16; ++i) {
        zobrist.castling_keys[i] = random_u64();
    }

    zobrist.side_to_move_key = random_u64();
}

uint64_t zobrist_hash(const gamestate_t *game) {
    uint64_t h = 0;

    for (int sq = 0; sq < BOARD_SIZE; ++sq) {
        if (!IS_VALID_SQUARE(sq)) continue;

        int piece = game->board[sq];
        if (piece != EMPTY) {
            int color = COLOR(piece);
            int type = PIECE_TYPE(piece);
            h ^= zobrist.piece_keys[color][type][sq];
        }
    }

    if (game->en_passant_square != -1) {
        h ^= zobrist.en_passant_keys[game->en_passant_square];
    }

    h ^= zobrist.castling_keys[game->castling_rights];

    if (game->to_move == BLACK) {
        h ^= zobrist.side_to_move_key;
    }

    return h;
}