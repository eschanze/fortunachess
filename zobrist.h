#pragma once
#include "chess.h"
#include <stdint.h>

#define ZOBRIST_SEED 1804289383

// https://en.wikipedia.org/wiki/Zobrist_hashing
// https://www.chessprogramming.org/Zobrist_Hashing
typedef struct {
    uint64_t piece_keys[2][7][BOARD_SIZE];      // [color][piece_type][square]
    uint64_t en_passant_keys[BOARD_SIZE];       // Una clave por cada columna válida (solo FILE relevante)
    uint64_t castling_keys[16];                 // 16 combinaciones posibles de castling_rights (0 a 15)
    uint64_t side_to_move_key;                  // Cambia entre turnos
} zobrist_table_t;

// Tabla global
extern zobrist_table_t zobrist;

// Funciones
void zobrist_init();
uint64_t zobrist_hash(const gamestate_t *game);