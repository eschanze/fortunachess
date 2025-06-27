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

/**
 * Inicializa el tablero con la posición inicial estándar.
 * @param game: puntero al estado del juego actual.
 */
void init_board(gamestate_t *game) {
    // Limpiar todo el tablero de una vez usando memset (más eficiente que iterar)
    memset(game->board, EMPTY, sizeof(game->board));
    
    // Disposición inicial de piezas mayores (misma para ambos colores)
    static const int piece_setup[8] = {
        ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK
    };
    
    // Colocar piezas mayores para ambos colores
    for (int file = 0; file < 8; file++) {
        // Fila 0: piezas blancas (primera fila)
        game->board[SQUARE(0, file)] = MAKE_PIECE(piece_setup[file], WHITE);
        // Fila 7: piezas negras (octava fila)
        game->board[SQUARE(7, file)] = MAKE_PIECE(piece_setup[file], BLACK);
    }
    
    // Colocar peones para ambos colores
    for (int file = 0; file < 8; file++) {
        // Fila 1: peones blancos (segunda fila)
        game->board[SQUARE(1, file)] = MAKE_PIECE(PAWN, WHITE);
        // Fila 6: peones negros (séptima fila)
        game->board[SQUARE(6, file)] = MAKE_PIECE(PAWN, BLACK);
    }
    
    // Inicializar estado del juego con valores por defecto
    game->to_move = WHITE;                    // Las blancas mueven primero
    
    // Todos los enroques disponibles al inicio (se usa OR para combinar flags y que game->castling_rights sea igual a 0b1111)
    game->castling_rights = CASTLE_WHITE_KING | CASTLE_WHITE_QUEEN | 
                           CASTLE_BLACK_KING | CASTLE_BLACK_QUEEN;
    
    game->en_passant_square = -1;             // No hay captura en passant disponible
    game->halfmove_clock = 0;                 // Contador de regla de 50 movimientos
    game->fullmove_number = 1;                // Primera jugada
    
    // Posiciones iniciales de los reyes (columna 4, filas 0 y 7)
    game->king_square[WHITE] = SQUARE(0, 4);  // Rey blanco en e1
    game->king_square[BLACK] = SQUARE(7, 4);  // Rey negro en e8

    // Inicializar pila de historial de movimientos y contador de jugadas
    game->move_history = stack_create(sizeof(history_entry_t));
    game->move_count = 0;
}

// Se tuvo que implementar para evitar problemas de compilación cuando se usan algunas versiones de MINGW64-gcc en Windows
static char *strtok_c(char *str, const char *delim, char **saveptr) {
    char *token;
    
    if (str == NULL)
        str = *saveptr;
    
    str += strspn(str, delim);
    
    if (*str == '\0') {
        *saveptr = str;
        return NULL;
    }
    
    token = str;
    str = strpbrk(token, delim);
    if (str == NULL) {
        *saveptr = token + strlen(token);
    } else {
        *str = '\0';
        *saveptr = str + 1;
    }
    
    return token;
}

// Función auxiliar para convertir carácter de pieza FEN a valor interno
int fen_char_to_piece(char c) {
    int piece_type;
    int color;
    
    // Determinar color (minúscula = negro, mayúscula = blanco)
    color = islower(c) ? BLACK : WHITE;
    c = tolower(c);
    
    // Convertir carácter a tipo de pieza
    switch (c) {
        case 'p': piece_type = PAWN; break;
        case 'n': piece_type = KNIGHT; break;
        case 'b': piece_type = BISHOP; break;
        case 'r': piece_type = ROOK; break;
        case 'q': piece_type = QUEEN; break;
        case 'k': piece_type = KING; break;
        default: return EMPTY;
    }
    
    return MAKE_PIECE(piece_type, color);
}

// Función auxiliar para convertir notación de casilla (ej: "e4") a índice 0x88
int square_from_string(const char *str) {
    if (str == NULL || strlen(str) < 2) return -1;
    
    int file = str[0] - 'a';  // a=0, b=1, ..., h=7
    int rank = str[1] - '1';  // 1=0, 2=1, ..., 8=7
    
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return -1;
    
    return SQUARE(rank, file);
}

/**
 * Inicializa el tablero con una posición dada por un string en formato FEN.
 * @param game: puntero al estado del juego actual.
 * @param fen: string FEN válido.
 */
int init_board_fen(gamestate_t *game, const char *fen) {
    if (game == NULL || fen == NULL) return -1;
    
    // Inicializar tablero vacío
    for (int i = 0; i < BOARD_SIZE; i++) {
        game->board[i] = EMPTY;
    }
    
    // Crear copia del FEN para tokenizar
    char *fen_copy = strdup(fen);
    if (fen_copy == NULL) return -1;
    
    char *token;
    char *saveptr;
    int field = 0;
    
    // Dividir FEN en campos separados por espacios
    token = strtok_c(fen_copy, " ", &saveptr);
    
    while (token != NULL && field < 6) {
        switch (field) {
            case 0: {  // Posición de las piezas
                int rank = 7;  // FEN empieza desde la fila 8 (índice 7)
                int file = 0;
                
                for (int i = 0; token[i] != '\0'; i++) {
                    char c = token[i];
                    
                    if (c == '/') {
                        // Nueva fila
                        rank--;
                        file = 0;
                        if (rank < 0) break;
                    } else if (isdigit(c)) {
                        // Número de casillas vacías
                        int empty_squares = c - '0';
                        file += empty_squares;
                    } else {
                        // Pieza
                        if (rank >= 0 && rank <= 7 && file >= 0 && file <= 7) {
                            int square = SQUARE(rank, file);
                            int piece = fen_char_to_piece(c);
                            game->board[square] = piece;
                            
                            // Guardar posición de los reyes
                            if (PIECE_TYPE(piece) == KING) {
                                game->king_square[COLOR(piece)] = square;
                            }
                        }
                        file++;
                    }
                }
                break;
            }
            
            case 1: {  // Turno activo
                game->to_move = (token[0] == 'w') ? WHITE : BLACK;
                break;
            }
            
            case 2: {  // Derechos de enroque
                game->castling_rights = 0;
                if (token[0] != '-') {
                    for (int i = 0; token[i] != '\0'; i++) {
                        switch (token[i]) {
                            case 'K': game->castling_rights |= CASTLE_WHITE_KING; break;
                            case 'Q': game->castling_rights |= CASTLE_WHITE_QUEEN; break;
                            case 'k': game->castling_rights |= CASTLE_BLACK_KING; break;
                            case 'q': game->castling_rights |= CASTLE_BLACK_QUEEN; break;
                        }
                    }
                }
                break;
            }
            
            case 3: {  // Casilla en passant
                if (token[0] == '-') {
                    game->en_passant_square = -1;
                } else {
                    game->en_passant_square = square_from_string(token);
                }
                break;
            }
            
            case 4: {  // Contador de medios movimientos
                game->halfmove_clock = atoi(token);
                break;
            }
            
            case 5: {  // Número de jugada completa
                game->fullmove_number = atoi(token);
                break;
            }
        }
        
        token = strtok_r(NULL, " ", &saveptr);
        field++;
    }
    
    // Inicializar otros campos si no están en el FEN
    if (field < 6) {
        if (field <= 4) game->halfmove_clock = 0;
        if (field <= 5) game->fullmove_number = 1;
    }
    
    // Inicializar contador de movimientos
    game->move_count = 0;
    
    free(fen_copy);
    return 0;  // Éxito
}

/**
 * Valida el movimiento de una pieza deslizante (torres, alfiles, y reina) en una dirección dada.
 * Verifica si una pieza puede deslizarse desde 'from' a 'to' en la dirección especificada
 * sin ser bloqueada por otras piezas.
 * 
 * @param move: puntero al movimiento a validar.
 * @param game: puntero al estado del juego actual.
 * @param dir: vector de dirección (e.g., -17 para diagonal superior izquierda).
 * @return true si el deslizamiento es válido y no está obstruido.
 */
bool is_slide_valid(move_t *move, gamestate_t *game, int dir) {
    int from = move->from;
    int to = move->to;
    int *board = game->board;

    // Recorrer la dirección dada desde la casilla origen
    for (int sq = from + dir; IS_VALID_SQUARE(sq); sq += dir) {
        if (sq == to) {
            int target = board[to];
            return (target == EMPTY || COLOR(target) != COLOR(move->piece));
        }

        // Si hay una pieza en el camino, es inválido
        if (board[sq] != EMPTY)
            return false;
    }

    return false;
}

/**
 * Verifica si una casilla está bajo ataque por piezas del color especificado.
 * Se utiliza para detección de jaque, validación de enroque, etc.
 * 
 * @param game: puntero al estado del juego actual.
 * @param square: casilla objetivo a verificar (formato 0x88).
 * @param by_color: color de las piezas que atacan (WHITE o BLACK).
 * @return true si la casilla está bajo ataque.
 */
bool is_square_attacked(gamestate_t *game, int square, int by_color) {
    int *board = game->board;

    // Ataques de peón
    int pawn_attack_dir = (by_color == WHITE) ? -16 : 16;
    int attacking_pawn = MAKE_PIECE(PAWN, by_color);
    int pawn_left = square + pawn_attack_dir - 1;
    int pawn_right = square + pawn_attack_dir + 1;

    if (IS_VALID_SQUARE(pawn_left) && board[pawn_left] == attacking_pawn)
        return true;
    if (IS_VALID_SQUARE(pawn_right) && board[pawn_right] == attacking_pawn)
        return true;

    // Ataques de caballo
    int attacking_knight = MAKE_PIECE(KNIGHT, by_color);
    for (int i = 0; i < 8; i++) {
        int knight_sq = square + knight_moves[i];
        if (IS_VALID_SQUARE(knight_sq) && board[knight_sq] == attacking_knight)
            return true;
    }

    // Ataques de rey
    int attacking_king = MAKE_PIECE(KING, by_color);
    for (int i = 0; i < 8; i++) {
        int king_sq = square + king_moves[i];
        if (IS_VALID_SQUARE(king_sq) && board[king_sq] == attacking_king)
            return true;
    }

    // Ataques diagonales (alfil/reina)
    for (int i = 0; i < 4; i++) {
        int dir = bishop_dirs[i];
        for (int sq = square + dir; IS_VALID_SQUARE(sq); sq += dir) {
            int piece = board[sq];
            if (piece != EMPTY) {
                if (COLOR(piece) == by_color) {
                    int piece_type = PIECE_TYPE(piece);
                    if (piece_type == BISHOP || piece_type == QUEEN)
                        return true;
                }
                break;
            }
        }
    }

    // Ataques ortogonales (torre/reina)
    for (int i = 0; i < 4; i++) {
        int dir = rook_dirs[i];
        for (int sq = square + dir; IS_VALID_SQUARE(sq); sq += dir) {
            int piece = board[sq];
            if (piece != EMPTY) {
                if (COLOR(piece) == by_color) {
                    int piece_type = PIECE_TYPE(piece);
                    if (piece_type == ROOK || piece_type == QUEEN)
                        return true;
                }
                break;
            }
        }
    }

    return false;
}

/**
 * Verifica si un rey está en jaque.
 * @param game: puntero al estado del juego actual.
 * @param color: color del rey a verificar (WHITE o BLACK).
 * @return true si el rey está en jaque.
 */
bool is_in_check(gamestate_t *game, int color) {
    int king_square = game->king_square[color == WHITE ? 0 : 1];
    return is_square_attacked(game, king_square, color ^ BLACK);
}

/**
 * Determina si un movimiento es legal.
 * Valida tanto las reglas de movimiento de las piezas como reglas específicas del ajedrez.
 * @param move: puntero al movimiento a verificar.
 * @param game: puntero al estado del juego actual.
 * @return true si el movimiento es legal.
 */
bool is_legal_move(move_t *move, gamestate_t *game) {
    int from = move->from;
    int to = move->to;
    int piece = move->piece;
    int piece_type = PIECE_TYPE(piece);
    int piece_color = COLOR(piece);
    
    // Validaciones básicas
    if (!IS_VALID_SQUARE(from) || !IS_VALID_SQUARE(to)) 
        return false;
    if (from == to) 
        return false;
    if ((game->board[from] == EMPTY) || (game->board[from] != piece))
        return false;
    if (piece_color != game->to_move) 
        return false;
    
    int target_piece = game->board[to];
    if (target_piece != EMPTY) {
        // No capturar pieza propia
        if (COLOR(target_piece) == piece_color) 
            return false;
        if (move->captured != target_piece) 
            return false;
    } else {
        // Captura vacía solo si es en passant
        if (move->captured != EMPTY && move->flags != MOVE_EN_PASSANT) 
            return false;
    }
    
    // Validación por tipo de pieza
    switch (piece_type) {
        case PAWN: {
            int direction = (piece_color == WHITE) ? 16 : -16;
            int start_rank = (piece_color == WHITE) ? 1 : 6;
            int delta = to - from;
            
            if (move->flags == MOVE_EN_PASSANT) {
                if (to != game->en_passant_square) 
                    return false;
                if (delta != direction - 1 && delta != direction + 1) 
                    return false;
                
                int captured_square = to - direction;
                if (game->board[captured_square] != MAKE_PIECE(PAWN, 1 - piece_color)) 
                    return false;
                    
            } else if (target_piece == EMPTY) {
                if (delta == direction) {
                } else if (delta == 2 * direction && RANK(from) == start_rank) {
                    // Avance doble desde casilla inicial
                } else {
                    return false;
                }
            } else {
                if (delta != direction - 1 && delta != direction + 1) 
                    return false;
            }
            
            int promotion_rank = (piece_color == WHITE) ? 7 : 0;
            if (RANK(to) == promotion_rank) {
                if (move->flags != MOVE_PROMOTION || move->promotion == 0) 
                    return false;
                if (move->promotion < KNIGHT || move->promotion > QUEEN) 
                    return false;
            } else {
                if (move->flags == MOVE_PROMOTION) 
                    return false;
            }
            break;
        }
        
        case KNIGHT: {
            bool valid_knight_move = false;
            for (int i = 0; i < 8; i++) {
                if (to == from + knight_moves[i]) {
                    valid_knight_move = true;
                    break;
                }
            }
            if (!valid_knight_move) 
                return false;
            break;
        }
        
        case BISHOP: {
            bool valid_bishop_move = false;
            for (int i = 0; i < 4; i++) {
                if (is_slide_valid(move, game, bishop_dirs[i])) {
                    valid_bishop_move = true;
                    break;
                }
            }
            if (!valid_bishop_move) 
                return false;
            break;
        }
        
        case ROOK: {
            bool valid_rook_move = false;
            for (int i = 0; i < 4; i++) {
                if (is_slide_valid(move, game, rook_dirs[i])) {
                    valid_rook_move = true;
                    break;
                }
            }
            if (!valid_rook_move) 
                return false;
            break;
        }
        
        case QUEEN: {
            bool valid_queen_move = false;
            
            for (int i = 0; i < 4; i++) {
                if (is_slide_valid(move, game, bishop_dirs[i])) {
                    valid_queen_move = true;
                    break;
                }
            }
            
            if (!valid_queen_move) {
                for (int i = 0; i < 4; i++) {
                    if (is_slide_valid(move, game, rook_dirs[i])) {
                        valid_queen_move = true;
                        break;
                    }
                }
            }
            
            if (!valid_queen_move) 
                return false;
            break;
        }
        
        case KING: {
            if (move->flags == MOVE_CASTLE_KING || move->flags == MOVE_CASTLE_QUEEN) {
                if (is_in_check(game, piece_color)) 
                    return false;
                
                int required_right = (move->flags == MOVE_CASTLE_KING) ? 
                    (piece_color == WHITE ? CASTLE_WHITE_KING : CASTLE_BLACK_KING) :
                    (piece_color == WHITE ? CASTLE_WHITE_QUEEN : CASTLE_BLACK_QUEEN);
                
                if (!(game->castling_rights & required_right)) 
                    return false;
                
                int king_start = (piece_color == WHITE) ? 0x04 : 0x74;
                int king_end, rook_start, rook_end;
                
                if (move->flags == MOVE_CASTLE_KING) {
                    king_end = king_start + 2;
                    rook_start = king_start + 3;
                    rook_end = king_start + 1;
                } else {
                    king_end = king_start - 2;
                    rook_start = king_start - 4;
                    rook_end = king_start - 1;
                }
                
                if (from != king_start || to != king_end) 
                    return false;
                
                if (game->board[rook_start] != MAKE_PIECE(ROOK, piece_color)) 
                    return false;
                
                int step = (move->flags == MOVE_CASTLE_KING) ? 1 : -1;
                for (int square = king_start + step; square != rook_start; square += step) {
                    if (game->board[square] != EMPTY) 
                        return false;
                }
                
                for (int square = king_start; square != king_end + step; square += step) {
                    if (is_square_attacked(game, square, 1 - piece_color)) 
                        return false;
                }
                
            } else {
                // Movimiento normal de rey
                bool valid_king_move = false;
                for (int i = 0; i < 8; i++) {
                    if (to == from + king_moves[i]) {
                        valid_king_move = true;
                        break;
                    }
                }
                if (!valid_king_move) 
                    return false;
            }
            break;
        }
    }
    
    // Simular jugada y verificar jaque
    gamestate_t temp_game = *game;
    make_move(move, &temp_game, false);
    if (is_in_check(&temp_game, piece_color)) 
        return false;
    
    return true;
}

/**
 * Realiza un movimiento en el tablero y actualiza el estado del juego.
 * Esta función asume que el movimiento ha sido validado como legal.
 * @param move: puntero al movimiento a ejecutar.
 * @param game: puntero al estado del juego a actualizar.
 * @param committed: determina si el movimiento se deberia guardar en el historial (stack *move_history).
 */
void make_move(move_t *move, gamestate_t *game, bool committed) {

    if (committed) {
        // Guardar estado actual en el historial (stack) antes de realizar el movimiento
        history_entry_t history;
        history.move = *move;
        memcpy(history.old_board, game->board, sizeof(game->board));
        history.old_castling_rights = game->castling_rights;
        history.old_en_passant_square = game->en_passant_square;
        history.old_halfmove_clock = game->halfmove_clock;
        history.old_fullmove_number = game->fullmove_number;
        // Agregar a la pila
        stack_push(game->move_history, &history);
        game->move_count++;
    }

    int moving_piece = move->piece;
    int piece_type = PIECE_TYPE(moving_piece);
    int piece_color = COLOR(moving_piece);
    
    // Mueve la pieza
    game->board[move->from] = EMPTY;
    game->board[move->to] = moving_piece;
    
    // Promoción
    if (move->flags == MOVE_PROMOTION) {
        game->board[move->to] = MAKE_PIECE(move->promotion, piece_color);
    }
    
    // Captura al paso
    if (move->flags == MOVE_EN_PASSANT) {
        int captured_pawn_square = move->to + (piece_color == WHITE ? -16 : 16);
        game->board[captured_pawn_square] = EMPTY;
    }
    
    // Enroque
    if (move->flags == MOVE_CASTLE_KING || move->flags == MOVE_CASTLE_QUEEN) {
        int rook_from, rook_to;
        
        if (move->flags == MOVE_CASTLE_KING) {
            rook_from = move->from + 3;
            rook_to = move->from + 1;
        } else {
            rook_from = move->from - 4;
            rook_to = move->from - 1;
        }
        
        game->board[rook_to] = game->board[rook_from];
        game->board[rook_from] = EMPTY;
    }
    
    // Actualiza posición del rey
    if (piece_type == KING) {
        game->king_square[piece_color] = move->to;
    }
    
    // Elimina derechos de enroque si mueve el rey
    if (piece_type == KING) {
        if (piece_color == WHITE) {
            game->castling_rights &= ~(CASTLE_WHITE_KING | CASTLE_WHITE_QUEEN);
        } else {
            game->castling_rights &= ~(CASTLE_BLACK_KING | CASTLE_BLACK_QUEEN);
        }
    }
    
    // Elimina derechos de enroque si mueve una torre
    if (piece_type == ROOK) {
        if (move->from == SQUARE(0, 0)) {
            game->castling_rights &= ~CASTLE_WHITE_QUEEN;
        } else if (move->from == SQUARE(0, 7)) {
            game->castling_rights &= ~CASTLE_WHITE_KING;
        } else if (move->from == SQUARE(7, 0)) {
            game->castling_rights &= ~CASTLE_BLACK_QUEEN;
        } else if (move->from == SQUARE(7, 7)) {
            game->castling_rights &= ~CASTLE_BLACK_KING;
        }
    }
    
    // Elimina derechos de enroque si capturan una torre en su casilla inicial
    if (move->captured != EMPTY && PIECE_TYPE(move->captured) == ROOK) {
        if (move->to == SQUARE(0, 0)) {
            game->castling_rights &= ~CASTLE_WHITE_QUEEN;
        } else if (move->to == SQUARE(0, 7)) {
            game->castling_rights &= ~CASTLE_WHITE_KING;
        } else if (move->to == SQUARE(7, 0)) {
            game->castling_rights &= ~CASTLE_BLACK_QUEEN;
        } else if (move->to == SQUARE(7, 7)) {
            game->castling_rights &= ~CASTLE_BLACK_KING;
        }
    }
    
    // Limpia casilla de al paso
    game->en_passant_square = -1;
    
    // Establece casilla de al paso si el peón se movió dos pasos
    if (piece_type == PAWN) {
        int move_distance = abs(RANK(move->to) - RANK(move->from));
        
        if (move_distance == 2)
            game->en_passant_square = move->from + (move->to - move->from) / 2;
    }
    
    // Reloj de medio movimiento
    if (piece_type == PAWN || move->captured != EMPTY) {
        game->halfmove_clock = 0;
    } else {
        game->halfmove_clock++;
    }
    
    // Aumenta el número de jugadas completas
    if (game->to_move == BLACK)
        game->fullmove_number++;
    
    // Cambia el turno
    game->to_move = (game->to_move == WHITE) ? BLACK : WHITE;
}

/**
 * Deshace un movimiento (para algoritmos de búsqueda, etc.)
 * Restaura el estado del juego al momento anterior al movimiento especificado.
 * @param move: puntero al movimiento a deshacer.
 * @param game: puntero al estado del juego a restaurar.
 */
void unmake_move(gamestate_t *game) {
    if (stack_is_empty(game->move_history) || game->move_count == 0)
        return; // No hay movimientos en el historial para deshacer
    
    // Obtener la información de la última entrada en el historial
    history_entry_t history;
    if (!stack_pop(game->move_history, &history)) {
        printf("[DEBUG] Error al obtener la información del último movimiento jugado\n");
        return;
    }
    game->move_count--;
    
    move_t *move = &history.move;
    
    // Restaurar turno
    game->to_move = 1 - game->to_move;
    
    // Restaurar tablero. Se podría hacer con la información que se guarda en history.move
    // Pero restaurar todo el tablero es más fácil/directo, aunque un poco más lento
    // No se necesitan restaurar las piezas manualmente en caso de enroque, etc.
    memcpy(game->board, history.old_board, sizeof(game->board));
    
    // Restaurar la posición de los reyes
    if (PIECE_TYPE(move->piece) == KING)
        game->king_square[COLOR(move->piece)] = move->from;

    // Restaurar estado de juego
    game->castling_rights = history.old_castling_rights;
    game->en_passant_square = history.old_en_passant_square;
    game->halfmove_clock = history.old_halfmove_clock;
    game->fullmove_number = history.old_fullmove_number;
}

// Función auxiliar que guarda el estado necesario en fast_undo_t para un deshacer rápido.
// Se debe llamar justo antes de hacer el movimiento.
void prepare_fast_undo(gamestate_t *game, move_t *move, fast_undo_t *undo_info) {
    undo_info->castling_rights = game->castling_rights;
    undo_info->en_passant_square = game->en_passant_square;
    undo_info->halfmove_clock = game->halfmove_clock;
    undo_info->fullmove_number = game->fullmove_number;
    undo_info->king_square[WHITE] = game->king_square[WHITE];
    undo_info->king_square[BLACK] = game->king_square[BLACK];
    undo_info->captured_piece = game->board[move->to];
}

/**
 * Utiliza una estructura auxiliar (fast_undo_t) que almacena solo la información
 * mínima necesaria para revertir el último movimiento, lo cual permite mayor 
 * eficiencia durante búsquedas (por ejemplo, en DFS/minimax/etc).
 * Esta función asume que se guardó correctamente el estado previo usando fast_undo_t 
 * justo antes de hacer el movimiento.
 * 
 * @param game: puntero al estado del juego a restaurar.
 * @param move: puntero al movimiento que se desea deshacer.
 * @param undo_info: puntero a la estructura que contiene la información para revertir el estado.
 */
void fast_unmake_move(gamestate_t *game, move_t *move, fast_undo_t *undo_info) {
    // Restaurar flags del estado de juego
    game->castling_rights = undo_info->castling_rights;
    game->en_passant_square = undo_info->en_passant_square;
    game->halfmove_clock = undo_info->halfmove_clock;
    game->fullmove_number = undo_info->fullmove_number;
    game->king_square[WHITE] = undo_info->king_square[WHITE];
    game->king_square[BLACK] = undo_info->king_square[BLACK];
    
    // Devolver el turno al jugador correspondiente
    game->to_move = 1 - game->to_move;
    
    // Restaurar el tablero, en base al tipo de movimiento (captura normal + casos especiales)
    switch (move->flags) {
        case MOVE_NORMAL:
        case MOVE_CAPTURE:
            // Restaurar posición de la pieza que capturó, y la pieza capturada
            game->board[move->from] = move->piece;
            game->board[move->to] = undo_info->captured_piece;
            break;
            
        case MOVE_CASTLE_KING:
            // Restaurar enroque (lado del rey)
            if (COLOR(move->piece) == WHITE) {
                game->board[0x04] = MAKE_PIECE(KING, WHITE);   // Devolver el rey a e1
                game->board[0x07] = MAKE_PIECE(ROOK, WHITE);   // Torre a h1
                game->board[0x06] = EMPTY;                     // Limpiar g1
                game->board[0x05] = EMPTY;                     // Limpiar f1
            } else {
                game->board[0x74] = MAKE_PIECE(KING, BLACK);   // Devolver el rey a e8
                game->board[0x77] = MAKE_PIECE(ROOK, BLACK);   // Torre a h8
                game->board[0x76] = EMPTY;                     // Limpiar g8
                game->board[0x75] = EMPTY;                     // Limpiar f8
            }
            break;
            
        case MOVE_CASTLE_QUEEN:
            // Restaurar enroque (lado de la reina)
            if (COLOR(move->piece) == WHITE) {
                game->board[0x04] = MAKE_PIECE(KING, WHITE);   // Devolver el rey a e1
                game->board[0x00] = MAKE_PIECE(ROOK, WHITE);   // Torre a a1
                game->board[0x02] = EMPTY;                     // Limpiar c1
                game->board[0x03] = EMPTY;                     // Limpiar d1
            } else {
                game->board[0x74] = MAKE_PIECE(KING, BLACK);   // Devolver el rey a e8
                game->board[0x70] = MAKE_PIECE(ROOK, BLACK);   // Torre a a8
                game->board[0x72] = EMPTY;                     // Limpiar c8
                game->board[0x73] = EMPTY;                     // Limpiar d8
            }
            break;
            
        case MOVE_EN_PASSANT:
            // Restaurar peón y peón capturado (caso en passant)
            game->board[move->from] = move->piece;
            game->board[move->to] = EMPTY;
            // Restaurar peón capturado
            if (COLOR(move->piece) == WHITE) {
                game->board[move->to - 16] = MAKE_PIECE(PAWN, BLACK);
            } else {
                game->board[move->to + 16] = MAKE_PIECE(PAWN, WHITE);
            }
            break;
            
        case MOVE_PROMOTION:
            // Si el movimiento fue una promoción, transformar la pieza de vuelta a peón
            game->board[move->from] = MAKE_PIECE(PAWN, COLOR(move->piece));
            game->board[move->to] = undo_info->captured_piece;
            break;
    }
}

// Generación de movimientos del peón
void generate_pawn_moves(gamestate_t *game, move_list_t *list, int from) {
    int piece = game->board[from];
    int color = COLOR(piece);
    int direction = (color == WHITE) ? 16 : -16;    // Dirección de movimiento según el color
    int start_rank = (color == WHITE) ? 1 : 6;      // Fila inicial del peón
    int promo_rank = (color == WHITE) ? 7 : 0;      // Fila de promoción
    
    // Los peones solo se pueden mover hacia adelante
    int to = from + direction;
    if (IS_VALID_SQUARE(to) && game->board[to] == EMPTY) {
        if (RANK(to) == promo_rank) {
            // En caso de promoción del peón, generar todos los tipos de piezas posibles
            add_move(list, from, to, piece, EMPTY, QUEEN, MOVE_PROMOTION);
            add_move(list, from, to, piece, EMPTY, ROOK, MOVE_PROMOTION);
            add_move(list, from, to, piece, EMPTY, BISHOP, MOVE_PROMOTION);
            add_move(list, from, to, piece, EMPTY, KNIGHT, MOVE_PROMOTION);
        } else {
            add_move(list, from, to, piece, EMPTY, 0, MOVE_NORMAL);
            
            // Los peones se pueden mover dos casillas si están en su fila inicial
            if (RANK(from) == start_rank) {
                to = from + 2 * direction;
                if (IS_VALID_SQUARE(to) && game->board[to] == EMPTY) {
                    add_move(list, from, to, piece, EMPTY, 0, MOVE_NORMAL);
                }
            }
        }
    }
    
    // Los peones capturan en diagonal
    int capture_dirs[2] = {direction - 1, direction + 1};
    for (int i = 0; i < 2; i++) {
        to = from + capture_dirs[i];
        if (IS_VALID_SQUARE(to)) {
            int target = game->board[to];
            // Captura normal
            if (target != EMPTY && COLOR(target) != color) {
                if (RANK(to) == promo_rank) {
                    // Captura con promoción
                    add_move(list, from, to, piece, target, QUEEN, MOVE_PROMOTION);
                    add_move(list, from, to, piece, target, ROOK, MOVE_PROMOTION);
                    add_move(list, from, to, piece, target, BISHOP, MOVE_PROMOTION);
                    add_move(list, from, to, piece, target, KNIGHT, MOVE_PROMOTION);
                } else {
                    add_move(list, from, to, piece, target, 0, MOVE_CAPTURE);
                }
            } else if (to == game->en_passant_square) {
                // Captura en passant (al paso)
                add_move(list, from, to, piece, MAKE_PIECE(PAWN, color ^ BLACK), 0, MOVE_EN_PASSANT);
            }
        }
    }
}

// Generación de movimientos del caballo
void generate_knight_moves(gamestate_t *game, move_list_t *list, int from) {
    int piece = game->board[from];
    int color = COLOR(piece);
    
    // Iterar por todas las 8 posibles posiciones del caballo
    for (int i = 0; i < 8; i++) {
        int to = from + knight_moves[i];
        if (IS_VALID_SQUARE(to)) {
            int target = game->board[to];
            if (target == EMPTY) {
                // Movimiento normal a una casilla vacía
                add_move(list, from, to, piece, EMPTY, 0, MOVE_NORMAL);
            } else if (COLOR(target) != color) {
                // Captura de pieza enemiga
                add_move(list, from, to, piece, target, 0, MOVE_CAPTURE);
            }
            // Si la casilla está ocupada por una pieza propia, no se genera ni agrega el movimiento
        }
    }
}

// Generación de movimientos de piezas deslizantes (alfil, torre, reina)
void generate_sliding_moves(gamestate_t *game, move_list_t *list, int from, int *directions, int num_dirs) {
    int piece = game->board[from];
    int color = COLOR(piece);
    
    // Iterar por cada dirección válida para la pieza
    for (int d = 0; d < num_dirs; d++) {
        int dir = directions[d];
        // Continuar en la dirección hasta encontrar un obstáculo o el borde del tablero
        for (int to = from + dir; IS_VALID_SQUARE(to); to += dir) {
            int target = game->board[to];
            if (target == EMPTY) {
                // Casilla vacía: agregar movimiento normal
                add_move(list, from, to, piece, EMPTY, 0, MOVE_NORMAL);
            } else {
                // Casilla ocupada
                if (COLOR(target) != color) {
                    // Pieza enemiga: agregar captura
                    add_move(list, from, to, piece, target, 0, MOVE_CAPTURE);
                }
                break; // Camino bloqueado, dejar de generar en esta dirección
            }
        }
    }
}

// Generación de movimientos del rey
void generate_king_moves(gamestate_t *game, move_list_t *list, int from) {
    int piece = game->board[from];
    int color = COLOR(piece);
    
    // Movimientos normales del rey (una casilla en cualquier dirección)
    for (int i = 0; i < 8; i++) {
        int to = from + king_moves[i];
        if (IS_VALID_SQUARE(to)) {
            int target = game->board[to];
            if (target == EMPTY) {
                // Movimiento a casilla vacía
                add_move(list, from, to, piece, EMPTY, 0, MOVE_NORMAL);
            } else if (COLOR(target) != color) {
                // Captura de pieza enemiga
                add_move(list, from, to, piece, target, 0, MOVE_CAPTURE);
            }
        }
    }
    
    // Enroque
    if (color == WHITE) {
        // Enroque corto de las piezas blancas (lado del rey)
        if ((game->castling_rights & CASTLE_WHITE_KING) &&
            game->board[0x05] == EMPTY && game->board[0x06] == EMPTY &&
            !is_square_attacked(game, 0x04, BLACK) &&
            !is_square_attacked(game, 0x05, BLACK) &&
            !is_square_attacked(game, 0x06, BLACK)) {
            add_move(list, from, 0x06, piece, EMPTY, 0, MOVE_CASTLE_KING);
        }
        
        // Enroque largo de las piezas blancas (lado de la reina)
        if ((game->castling_rights & CASTLE_WHITE_QUEEN) &&
            game->board[0x03] == EMPTY && game->board[0x02] == EMPTY && game->board[0x01] == EMPTY &&
            !is_square_attacked(game, 0x04, BLACK) &&
            !is_square_attacked(game, 0x03, BLACK) &&
            !is_square_attacked(game, 0x02, BLACK)) {
            add_move(list, from, 0x02, piece, EMPTY, 0, MOVE_CASTLE_QUEEN);
        }
    } else {
        // Enroque corto de las piezas negras (lado del rey)
        if ((game->castling_rights & CASTLE_BLACK_KING) &&
            game->board[0x75] == EMPTY && game->board[0x76] == EMPTY &&
            !is_square_attacked(game, 0x74, WHITE) &&
            !is_square_attacked(game, 0x75, WHITE) &&
            !is_square_attacked(game, 0x76, WHITE)) {
            add_move(list, from, 0x76, piece, EMPTY, 0, MOVE_CASTLE_KING);
        }
        
        // Enroque largo de piezas negras (lado de la reina)
        if ((game->castling_rights & CASTLE_BLACK_QUEEN) &&
            game->board[0x73] == EMPTY && game->board[0x72] == EMPTY && game->board[0x71] == EMPTY &&
            !is_square_attacked(game, 0x74, WHITE) &&
            !is_square_attacked(game, 0x73, WHITE) &&
            !is_square_attacked(game, 0x72, WHITE)) {
            add_move(list, from, 0x72, piece, EMPTY, 0, MOVE_CASTLE_QUEEN);
        }
    }
}

/**
 * Genera todos los movimientos legales posibles para el jugador en turno.
 * Llama a las funciones específicas según el tipo de pieza presente en cada casilla.
 * @param game: puntero al estado actual del juego.
 * @param list: puntero a la lista donde se agregarán todos los movimientos válidos.
 */
void generate_moves(gamestate_t *game, move_list_t *list) {
    list->count = 0;
    
    for (int square = 0; square < BOARD_SIZE; square++) {
        if (!IS_VALID_SQUARE(square)) continue;
        
        int piece = game->board[square];
        if (piece == EMPTY || COLOR(piece) != game->to_move) continue;
        
        int piece_type = PIECE_TYPE(piece);
        
        switch (piece_type) {
            case PAWN:
                generate_pawn_moves(game, list, square);
                break;
            case KNIGHT:
                generate_knight_moves(game, list, square);
                break;
            case BISHOP:
                generate_sliding_moves(game, list, square, bishop_dirs, 4);
                break;
            case ROOK:
                generate_sliding_moves(game, list, square, rook_dirs, 4);
                break;
            case QUEEN:
                generate_sliding_moves(game, list, square, bishop_dirs, 4);
                generate_sliding_moves(game, list, square, rook_dirs, 4);
                break;
            case KING:
                generate_king_moves(game, list, square);
                break;
        }
    }
}

// Función auxiliar para obtener el nombre del resultado
const char* get_game_result_name(game_result_t result) {
    switch (result) {
        case GAME_ONGOING: return "Juego en curso";
        case GAME_CHECKMATE_WHITE: return "¡Jaque mate! Ganan las blancas";
        case GAME_CHECKMATE_BLACK: return "¡Jaque mate! Ganan las negras";
        case GAME_STALEMATE: return "¡Tablas por ahogado!";
        case GAME_DRAW_50_MOVES: return "¡Tablas por regla de 50 movimientos!";
        case GAME_DRAW_REPETITION: return "¡Tablas por repetición de posición!";
        case GAME_DRAW_MATERIAL: return "¡Tablas por material insuficiente!";
        default: return "Estado desconocido";
    }
}

/**
 * Verifica si el jugador actual tiene algún movimiento legal disponible.
 * @param game: puntero al estado del juego actual.
 * @return true si hay al menos un movimiento legal disponible.
 */
bool has_legal_moves(gamestate_t *game) {
    move_list_t list;
    generate_moves(game, &list);
    
    for (int i = 0; i < list.count; i++) {
        if (is_legal_move(&list.moves[i], game)) {
            return true;
        }
    }
    return false;
}

/**
 * Cuenta el material en el tablero para detectar insuficiencia de material.
 * @param game: puntero al estado del juego actual.
 * @param white_material: array para almacenar el material blanco [peones, caballos, alfiles, torres, reinas]
 * @param black_material: array para almacenar el material negro [peones, caballos, alfiles, torres, reinas]
 */
void count_material(gamestate_t *game, int white_material[5], int black_material[5]) {
    // Inicializar contadores: [peones, caballos, alfiles, torres, reinas]
    memset(white_material, 0, 5 * sizeof(int));
    memset(black_material, 0, 5 * sizeof(int));
    
    for (int square = 0; square < BOARD_SIZE; square++) {
        if (!IS_VALID_SQUARE(square)) continue;
        
        int piece = game->board[square];
        if (piece == EMPTY) continue;
        
        int piece_type = PIECE_TYPE(piece);
        int color = COLOR(piece);
        
        int *material = (color == WHITE) ? white_material : black_material;
        
        switch (piece_type) {
            case PAWN:   material[0]++; break;
            case KNIGHT: material[1]++; break;
            case BISHOP: material[2]++; break;
            case ROOK:   material[3]++; break;
            case QUEEN:  material[4]++; break;
            // El rey no se cuenta para material insuficiente
        }
    }
}

/**
 * Verifica si hay material insuficiente para dar jaque mate.
 * @param game: puntero al estado del juego actual.
 * @return true si hay material insuficiente.
 */
bool is_insufficient_material(gamestate_t *game) {
    int white_material[5], black_material[5];
    count_material(game, white_material, black_material);
    
    // Calcular totales
    int white_total = 0, black_total = 0;
    for (int i = 0; i < 5; i++) {
        white_total += white_material[i];
        black_total += black_material[i];
    }
    
    // Rey vs Rey
    if (white_total == 0 && black_total == 0) {
        return true;
    }
    
    // Rey + Caballo vs Rey o Rey + Alfil vs Rey
    if ((white_total == 1 && black_total == 0 && 
         (white_material[1] == 1 || white_material[2] == 1)) ||
        (black_total == 1 && white_total == 0 && 
         (black_material[1] == 1 || black_material[2] == 1))) {
        return true;
    }
    
    // Rey + Alfil vs Rey + Alfil (mismo color de casillas)
    if (white_total == 1 && black_total == 1 && 
        white_material[2] == 1 && black_material[2] == 1) {
        // Buscar los alfiles y verificar si están en casillas del mismo color
        int white_bishop_square = -1, black_bishop_square = -1;
        
        for (int square = 0; square < BOARD_SIZE; square++) {
            if (!IS_VALID_SQUARE(square)) continue;
            int piece = game->board[square];
            if (piece == MAKE_PIECE(BISHOP, WHITE)) {
                white_bishop_square = square;
            } else if (piece == MAKE_PIECE(BISHOP, BLACK)) {
                black_bishop_square = square;
            }
        }
        
        if (white_bishop_square != -1 && black_bishop_square != -1) {
            // Verificar si ambos alfiles están en casillas del mismo color
            bool white_on_light = ((RANK(white_bishop_square) + FILE(white_bishop_square)) % 2) == 0;
            bool black_on_light = ((RANK(black_bishop_square) + FILE(black_bishop_square)) % 2) == 0;
            
            if (white_on_light == black_on_light) {
                return true;
            }
        }
    }
    
    return false;
}

/**
 * Evalúa el estado actual del juego para determinar si ha terminado.
 * @param game: puntero al estado del juego actual.
 * @return el resultado del juego (GAME_ONGOING si continúa).
 */
game_result_t evaluate_game_state(gamestate_t *game) {
    // Verificar regla de 50 movimientos
    if (game->halfmove_clock >= 100) {  // 50 movimientos = 100 medio-movimientos
        return GAME_DRAW_50_MOVES;
    }
    
    // Verificar material insuficiente
    if (is_insufficient_material(game)) {
        return GAME_DRAW_MATERIAL;
    }
    
    // Verificar si el jugador actual tiene movimientos legales
    bool has_moves = has_legal_moves(game);
    bool in_check = is_in_check(game, game->to_move);
    
    if (!has_moves) {
        if (in_check) {
            // Jaque mate
            return (game->to_move == WHITE) ? GAME_CHECKMATE_BLACK : GAME_CHECKMATE_WHITE;
        } else {
            // Ahogado (stalemate)
            return GAME_STALEMATE;
        }
    }
    
    // TODO: Implementar detección de repetición de posiciones (regla de Triple Repetición)
    
    return GAME_ONGOING;
}

/**
 * Realiza un conteo recursivo de nodos a partir del estado actual del juego.
 * Utilizado para pruebas (perft) de generación de movimientos.
 *  Se usa para verificar que todas las reglas de movimiento estén implementadas correctamente.
 * @param game: puntero al estado actual del juego.
 * @param depth: profundidad máxima a explorar.
 * @return el número total de nodos generados hasta esa profundidad.
 */
uint64_t perft(gamestate_t *game, int depth) {
    // Caso base
    if (depth == 0) return 1;

    move_list_t list;
    generate_moves(game, &list);

    uint64_t total = 0;
    gamestate_t backup;

    for (int i = 0; i < list.count; i++) {
        move_t move = list.moves[i];
        if (!is_legal_move(&move, game)) continue;

        // Guardar el estado del juego
        fast_undo_t undo_info;
        prepare_fast_undo(game, &move, &undo_info);
        make_move(&move, game, false);
        // Llamada recursiva
        total += perft(game, depth - 1);
        // Devolver la partida a su estado previo
        fast_unmake_move(game, &move, &undo_info);
    }

    return total;
}

/**
 * Ejecuta pruebas de rendimiento de generación de movimientos (perft) hasta cierta profundidad.
 * Muestra en consola el tiempo que toma y el número de nodos por segundo.
 * @param game: puntero al estado actual del juego.
 * @param max_depth: profundidad máxima que se quiere evaluar.
 */
void perft_benchmark(gamestate_t *game, int max_depth) {
    printf("Resultados PERFT:\n");
    printf("========================\n\n");
    
    for (int depth = 1; depth <= max_depth; depth++) {
        clock_t start = clock();
        uint64_t nodes = perft(game, depth);
        clock_t end = clock();
        
        double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
        double nps = (time_taken > 0) ? nodes / time_taken : 0;
        
        printf("Profundidad %d: %12llu nodos en %8.3f segundos (%10.0f NPS)\n", 
               depth, (unsigned long long)nodes, time_taken, nps);
    }
    printf("\n");
}