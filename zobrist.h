#pragma once
#include <stdint.h>
#include "chess.h"  // Para gamestate_t

// https://en.wikipedia.org/wiki/Zobrist_hashing
// https://www.chessprogramming.org/Zobrist_Hashing

// Funciones
uint64_t polyglot_hash(const char *fen);