#pragma once
#include <stdint.h>
// Definiciones principales
#include "chess.h"

// TODO: Completas las funciones
uint64_t zobrist_hash_position(gamestate_t *game);
uint64_t zobrist_hash_piece(int piece, int square);
uint64_t zobrist_hash_castling(int castling_rights);
uint64_t zobrist_hash_en_passant(int en_passant_square);
uint64_t zobrist_hash_to_move(void);
// Función auxiliar para pasar de formato 0x88 a 64
int square_0x88_to_64(int squarze_0x88);