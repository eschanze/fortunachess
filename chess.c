#include "chess.h"

// Añade un movimiento a la lista de movimientos
void add_move(move_list_t *list, int from, int to, int piece, int captured, int promotion, int flags) {
    if (list->count < 256) {
        move_t *move = &list->moves[list->count++];
        move->from = from;
        move->to = to;
        move->piece = piece;
        move->captured = captured;
        move->promotion = promotion;
        move->flags = flags;
    }
}

// Vectores de dirección para cada tipo de pieza (en relación a su representación en formato 0x88)
int knight_moves[8] = {-33, -31, -18, -14, 14, 18, 31, 33};     // Movimientos de caballo (forma de L)
int king_moves[8] = {-17, -16, -15, -1, 1, 15, 16, 17};         // Movimientos de rey (8 direcciones)
int bishop_dirs[4] = {-17, -15, 15, 17};                        // Direcciones diagonales del alfil
int rook_dirs[4] = {-16, -1, 1, 16};                            // Direcciones ortogonales de la torre
// No es necesario un vector para la reina, ya que se puede usar una combinación de bishop_dirs y rook_dirs