#include "bot.h"

// Valores de las piezas para evaluación material
static const int piece_values[7] = {
    0,    // EMPTY
    100,  // PAWN
    320,  // KNIGHT
    330,  // BISHOP
    500,  // ROOK
    900,  // QUEEN
    20000 // KING
};

// Función auxiliar que filtra los movimientos pseudo-legales de generate_moves(...)
void filter_legal_moves(gamestate_t *game, move_list_t *moves) {
    int write_idx = 0;
    for (int i = 0; i < moves->count; i++) {
        if (is_legal_move(&moves->moves[i], game)) {
            moves->moves[write_idx++] = moves->moves[i];
        }
    }
    moves->count = write_idx;
}

// Función de evaluación simple: material + movilidad
int evaluate_position(gamestate_t *game) {
    int score = 0;
    int white_material = 0, black_material = 0;
    int white_mobility = 0, black_mobility = 0;
    
    // Evaluación material
    for (int sq = 0; sq < BOARD_SIZE; sq++) {
        if (!IS_VALID_SQUARE(sq)) continue;
        
        int piece = game->board[sq];
        if (piece == EMPTY) continue;
        
        int piece_type = PIECE_TYPE(piece);
        int piece_color = COLOR(piece);
        
        if (piece_color == WHITE) {
            white_material += piece_values[piece_type];
        } else {
            black_material += piece_values[piece_type];
        }
    }
    
    // Evaluación de movilidad (número de movimientos legales)
    move_list_t moves;
    
    // Contar movimientos para las blancas
    int original_turn = game->to_move;
    game->to_move = WHITE;
    generate_moves(game, &moves);
    filter_legal_moves(game, &moves);
    white_mobility = moves.count;
    
    // Contar movimientos para las negras
    game->to_move = BLACK;
    generate_moves(game, &moves);
    filter_legal_moves(game, &moves);
    black_mobility = moves.count;
    
    // Restaurar turno original
    game->to_move = original_turn;
    
    // Calcular puntuación final
    score = (white_material - black_material) + 
            (white_mobility - black_mobility) * 2; // Peso menor para movilidad
    
    // Devolver desde perspectiva del jugador actual
    return (game->to_move == WHITE) ? score : -score;
}

// Verifica si el juego ha terminado (jaque mate, ahogado, etc.)
int is_game_over(gamestate_t *game) {
    return evaluate_game_state(game) != GAME_ONGOING;
}

// Función auxiliar para ordenar movimientos (mejora la poda alpha-beta)
int score_move(gamestate_t *game, move_t *move) {
    int score = 0;
    
    // Priorizar capturas
    if (move->captured != EMPTY) {
        score += piece_values[PIECE_TYPE(move->captured)] - 
                 piece_values[PIECE_TYPE(move->piece)];
    }
    
    // Priorizar promociones
    if (move->flags == MOVE_PROMOTION) {
        score += piece_values[move->promotion];
    }
    
    // Priorizar movimientos hacia el centro
    int to_file = FILE(move->to);
    int to_rank = RANK(move->to);
    int center_distance = abs(to_file - 3.5) + abs(to_rank - 3.5);
    score += (7 - center_distance) * 5;
    
    return score;
}

// Ordenar movimientos por puntuación (insertion sort simple)
void sort_moves(gamestate_t *game, move_list_t *moves) {
    for (int i = 1; i < moves->count; i++) {
        move_t key = moves->moves[i];
        int key_score = score_move(game, &key);
        int j = i - 1;
        
        while (j >= 0 && score_move(game, &moves->moves[j]) < key_score) {
            moves->moves[j + 1] = moves->moves[j];
            j--;
        }
        moves->moves[j + 1] = key;
    }
}

// Algoritmo minimax con poda alpha-beta
int alpha_beta(gamestate_t *game, int depth, int alpha, int beta, int maximizing_player) {
    // Caso base: profundidad 0 o juego terminado
    if (depth == 0 || is_game_over(game)) {
        return evaluate_position(game);
    }
    
    move_list_t moves;
    generate_moves(game, &moves);
    filter_legal_moves(game, &moves);
    
    // Ordenar movimientos para mejorar la poda
    sort_moves(game, &moves);
    
    if (maximizing_player) {
        int max_eval = INT_MIN;
        gamestate_t backup;
        
        for (int i = 0; i < moves.count; i++) {
            // Hacer el movimiento
            fast_undo_t undo_info;
            prepare_fast_undo(game, &moves.moves[i], &undo_info);
            make_move(&moves.moves[i], game, false);
            
            // Llamada recursiva
            int eval = alpha_beta(game, depth - 1, alpha, beta, 0);
            
            // Deshacer el movimiento
            fast_unmake_move(game, &moves.moves[i], &undo_info);
            
            max_eval = (eval > max_eval) ? eval : max_eval;
            alpha = (alpha > eval) ? alpha : eval;
            
            // Poda beta
            if (beta <= alpha) {
                break;
            }
        }
        
        return max_eval;
    } else {
        int min_eval = INT_MAX;
        gamestate_t backup;
        
        for (int i = 0; i < moves.count; i++) {
            // Hacer el movimiento
            fast_undo_t undo_info;
            prepare_fast_undo(game, &moves.moves[i], &undo_info);
            make_move(&moves.moves[i], game, false);
            
            // Llamada recursiva
            int eval = alpha_beta(game, depth - 1, alpha, beta, 1);
            
            // Deshacer el movimiento
            fast_unmake_move(game, &moves.moves[i], &undo_info);
            
            min_eval = (eval < min_eval) ? eval : min_eval;
            beta = (beta < eval) ? beta : eval;
            
            // Poda alpha
            if (beta <= alpha) {
                break;
            }
        }
        
        return min_eval;
    }
}

// Función principal para encontrar el mejor movimiento
move_t find_best_move(gamestate_t *game, int depth) {
    move_list_t moves;
    generate_moves(game, &moves);
    filter_legal_moves(game, &moves);
    
    if (moves.count == 0) {
        // No hay movimientos legales
        move_t null_move = {0};
        return null_move;
    }
    
    // Ordenar movimientos
    sort_moves(game, &moves);
    
    move_t best_move = moves.moves[0];
    int best_score = INT_MIN;
    int alpha = INT_MIN;
    int beta = INT_MAX;
    gamestate_t backup;
    
    printf("Explorando estados con profunidad = %d...\n", depth);

    for (int i = 0; i < moves.count; i++) {
        // Hacer el movimiento
        fast_undo_t undo_info;
        prepare_fast_undo(game, &moves.moves[i], &undo_info);
        make_move(&moves.moves[i], game, false);
        
        // Evaluar la posición resultante
        int score = alpha_beta(game, depth - 1, alpha, beta, 0);
        
        // Deshacer el movimiento
        fast_unmake_move(game, &moves.moves[i], &undo_info);
        
        // Imprimir información de cada movimiento candidato
        /*printf("Movimiento %d: %c%d%c%d, Evaluación: %d\n", 
               i + 1,
               'a' + FILE(moves.moves[i].from), RANK(moves.moves[i].from) + 1,
               'a' + FILE(moves.moves[i].to), RANK(moves.moves[i].to) + 1,
               score);*/
        
        // Actualizar mejor movimiento si es necesario
        if (score > best_score) {
            best_score = score;
            best_move = moves.moves[i];
        }
        
        alpha = (alpha > score) ? alpha : score;
    }
    
    // Imprimir el mejor movimiento encontrado
    print_search_info(game, depth, best_score, &best_move);
    
    return best_move;
}

// Función auxiliar para imprimir información de búsqueda
void print_search_info(gamestate_t *game, int depth, int score, move_t *move) {
    printf("Profundidad: %d, Evaluación: %d, Mejor movimiento: %c%d%c%d\n", 
           depth, score,
           'a' + FILE(move->from), RANK(move->from) + 1,
           'a' + FILE(move->to), RANK(move->to) + 1);
}