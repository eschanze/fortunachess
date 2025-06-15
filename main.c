#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

// TDAs
#include "stack.h"

// Fortuna Chess
// Motor de ajedrez simple usando representación de tablero 0x88
// La representación 0x88 usa un array de 128 elementos donde solo 64 son válidos
// Permite detección rápida de casillas válidas usando operación AND con 0x88
// Se definen los siguientes macros para operaciones binarias para mejorar la legibilidad del código:
#define BOARD_SIZE 128
#define RANK(sq) ((sq) >> 4)                            // Obtiene la fila (0-7) de una casilla
#define FILE(sq) ((sq) & 7)                             // Obtiene la columna (0-7) de una casilla
#define SQUARE(rank, file) (((rank) << 4) | (file))     // Crea una casilla desde fila y columna
#define IS_VALID_SQUARE(sq) (!((sq) & 0x88))            // Verifica si una casilla es válida en el tablero

// Definiciones de piezas
#define EMPTY 0      // Casilla vacía
#define PAWN 1       // Peón
#define KNIGHT 2     // Caballo
#define BISHOP 3     // Alfil
#define ROOK 4       // Torre
#define QUEEN 5      // Reina/Dama
#define KING 6       // Rey

// Definiciones de colores
#define WHITE 0      // Blancas
#define BLACK 1      // Negras
#define COLOR(piece) ((piece) >> 3)                         // Extrae el color de una pieza
#define PIECE_TYPE(piece) ((piece) & 7)                     // Extrae el tipo de una pieza
#define MAKE_PIECE(type, color) ((type) | ((color) << 3))   // Crea una pieza dado un tipo y color

// Flags para tipos especiales de movimientos
#define MOVE_NORMAL 0        // Movimiento normal
#define MOVE_CAPTURE 1       // Captura
#define MOVE_CASTLE_KING 2   // Enroque corto (lado del rey)
#define MOVE_CASTLE_QUEEN 3  // Enroque largo (lado de la reina)
#define MOVE_EN_PASSANT 4    // En passant (captura al paso)
#define MOVE_PROMOTION 5     // Promoción de un peón

// Flags para derechos de enroque
// Se usan potencias de 2 (1, 2, 4, 8) para poder representar las 4 flags con un sólo número (0b1111 al inicio de la partida)
// Bit 0 (1): Enroque corto blanco - 0001
// Bit 1 (2): Enroque largo blanco - 0010  
// Bit 2 (4): Enroque corto negro  - 0100
// Bit 3 (8): Enroque largo negro  - 1000
// Ejemplo: castling_rights = 15 (0b1111) = todos los enroques disponibles
#define CASTLE_WHITE_KING 1
#define CASTLE_WHITE_QUEEN 2
#define CASTLE_BLACK_KING 4
#define CASTLE_BLACK_QUEEN 8

// Estructura para representar un movimiento
typedef struct {
    int from;           // Casilla de origen (formato 0x88)
    int to;             // Casilla de destino (formato 0x88)
    int piece;          // Pieza que se mueve
    int captured;       // Pieza capturada (EMPTY si no hay captura)
    int promotion;      // Tipo de pieza a la que se promociona (0 si no es promoción)
    int flags;          // Banderas especiales (enroque, en passant, etc.)
} move_t;

// Estructura de lista de movimientos para generación de jugadas
typedef struct {
    move_t moves[256];  // Máximo número posible de movimientos en una posición
    int count;          // Contador de movimientos en la lista
} move_list_t;

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

// Estructura que representa el estado actual del juego
typedef struct {
    int board[BOARD_SIZE];      // Representación del tablero 0x88
    int to_move;                // Turno actual: WHITE o BLACK
    int castling_rights;        // Campo de bits: KQkq (Blanco: K=1, Q=2, Negro: k=4, q=8)
    int en_passant_square;      // Casilla "fantasma" detrás del peón que avanzó 2 casillas (-1 si no hay)
    int halfmove_clock;         // Movimientos desde último movimiento de peón o captura de alguna pieza
    int fullmove_number;        // Número de jugadas completas
    int king_square[2];         // Posiciones de los reyes en formato [WHITE, BLACK]
    stack_t *move_history;        // Pila que almacena el historial de movimientos realizados
    int move_count;             // Contador de movimientos realizados
} gamestate_t;

// Estructura que representa una entrada en el historial de movimientos
typedef struct {
    move_t move;                    // El movimiento que se realizó
    // Información adicional que debe ser restaurada (tablero, derechos de enroque, contadores de turnos, etc.)
    int old_board[BOARD_SIZE];
    int old_castling_rights;
    int old_en_passant_square;
    int old_halfmove_clock;
    int old_fullmove_number;
} history_entry_t;

// Vectores de dirección para cada tipo de pieza (en relación a su representación en formato 0x88)
int knight_moves[8] = {-33, -31, -18, -14, 14, 18, 31, 33};     // Movimientos de caballo (forma de L)
int king_moves[8] = {-17, -16, -15, -1, 1, 15, 16, 17};         // Movimientos de rey (8 direcciones)
int bishop_dirs[4] = {-17, -15, 15, 17};                        // Direcciones diagonales del alfil
int rook_dirs[4] = {-16, -1, 1, 16};                            // Direcciones ortogonales de la torre
// No es necesario un vector para la reina, ya que se puede usar una combinación de bishop_dirs y rook_dirs

//// Prototipos de funciones
// Funciones auxiliares
char piece_to_char(int piece);
int char_to_piece(char c);
int algebraic_to_square(const char *algebraic);
void square_to_algebraic(int square, char *algebraic);
// Funciones de inicialización
void init_board(gamestate_t *game);
void display_board(gamestate_t *game);
// Input
bool parse_move(const char *move_str, move_t *move, gamestate_t *game);
// Lógica del juego (legalidad, generación de movimientos, etc.)
bool is_slide_valid(move_t *move, gamestate_t *game, int dir);
bool is_square_attacked(gamestate_t *game, int square, int by_color);
bool is_in_check(gamestate_t *game, int color);
bool is_legal_move(move_t *move, gamestate_t *game);
void make_move(move_t *move, gamestate_t *game, bool committed);
void unmake_move(gamestate_t *game);
// Generación de movimientos
// TODO: void generate_moves(gamestate_t *game, move_list_t *list);
// Benchmarking y testing
// TODO: uint64_t perft(gamestate_t *game, int depth);
// TODO: void perft_benchmark(gamestate_t *game, int max_depth); // output detallado (sólo para debuggear)

/**
 * Convierte un tipo de pieza a su carácter representativo.
 * Ej: MAKE_PIECE(PAWN, WHITE) => 'P'
 * @param piece: entero que representa la pieza.
 * @return carácter correspondiente.
 */
char piece_to_char(int piece) {
    if (piece == EMPTY) return '.';
    
    char pieces[] = " PNBRQK";
    char c = pieces[PIECE_TYPE(piece)];
    
    return COLOR(piece) == WHITE ? c : tolower(c);
}

/**
 * Convierte un carácter a su pieza representativa.
 * Ej: 'n' => MAKE_PIECE(KNIGHT, BLACK)
 * @param c: carácter de la pieza.
 * @return entero que representa la pieza.
 */
int char_to_piece(char c) {
    switch (tolower(c)) {
        case 'p': return MAKE_PIECE(PAWN, isupper(c) ? WHITE : BLACK);
        case 'n': return MAKE_PIECE(KNIGHT, isupper(c) ? WHITE : BLACK);
        case 'b': return MAKE_PIECE(BISHOP, isupper(c) ? WHITE : BLACK);
        case 'r': return MAKE_PIECE(ROOK, isupper(c) ? WHITE : BLACK);
        case 'q': return MAKE_PIECE(QUEEN, isupper(c) ? WHITE : BLACK);
        case 'k': return MAKE_PIECE(KING, isupper(c) ? WHITE : BLACK);
        default: return EMPTY;
    }
}

/**
 * Convierte notación algebraica a índice 0x88.
 * Ej: "e4" => SQUARE(3, 4)
 * @param algebraic: string con notación algebraica.
 * @return índice del tablero en formato 0x88.
 */
int algebraic_to_square(const char *algebraic) {
    if (strlen(algebraic) != 2) return -1;
    
    int file = algebraic[0] - 'a';
    int rank = algebraic[1] - '1';
    
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return -1;
    
    return SQUARE(rank, file);
}

/**
 * Convierte un índice 0x88 a notación algebraica.
 * Ej: SQUARE(3, 4) => "e4"
 * @param square: índice del tablero en formato 0x88.
 * @param algebraic: buffer donde se guarda la notación.
 */
void square_to_algebraic(int square, char *algebraic) {
    if (!IS_VALID_SQUARE(square)) {
        strcpy(algebraic, "??");
        return;
    }
    
    algebraic[0] = 'a' + FILE(square);
    algebraic[1] = '1' + RANK(square);
    algebraic[2] = '\0';
}

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

/**
 * Muestra el estado actual del tablero en la terminal.
 * @param game: puntero al estado del juego actual.
 */
void display_board(gamestate_t *game) {
    printf("\n    a b c d e f g h\n");
    printf("  +-----------------+\n");
    
    // Recorre las filas del tablero de arriba hacia abajo (8 a 1)
    for (int rank = 7; rank >= 0; rank--) {
        printf("%d | ", rank + 1);
        for (int file = 0; file < 8; file++) {
            int square = SQUARE(rank, file);
            char piece_char = piece_to_char(game->board[square]);
            printf("%c ", piece_char); // Imprime el carácter correspondiente a la pieza
        }
        printf("| %d\n", rank + 1);
    }

    printf("  +-----------------+\n");
    printf("    a b c d e f g h\n\n");

    // Muestra de quién es el turno
    printf("Turno: %s\n", game->to_move == WHITE ? "Blancas" : "Negras");

    // Muestra los derechos de enroque disponibles
    printf("Derechos de enroque: %s%s%s%s\n",
           (game->castling_rights & CASTLE_WHITE_KING) ? "K" : "",
           (game->castling_rights & CASTLE_WHITE_QUEEN) ? "Q" : "",
           (game->castling_rights & CASTLE_BLACK_KING) ? "k" : "",
           (game->castling_rights & CASTLE_BLACK_QUEEN) ? "q" : "");

    // [DEBUG] Imprimir la casilla "fantasma" que deja un peón que avanza 2 casillas
    // Útil para poder testear que las reglas de en passant estén funcionando correctamente
    if (game->en_passant_square != -1) {
        char ep_square[3];
        square_to_algebraic(game->en_passant_square, ep_square);
        printf("Casilla en passant: %s\n", ep_square);
    }

    printf("\n");
}

/**
 * Parsea una cadena como "e2e4" y la convierte en un move_t.
 * @param move_str: string con movimiento en notación algebraica.
 * @param move: puntero al struct move_t que almacenará información del movimiento.
 * @param game: puntero al estado del juego actual.
 * @return true si el movimiento fue válido.
 */
bool parse_move(const char *move_str, move_t *move, gamestate_t *game) {
    if (strlen(move_str) < 4) return false;
    
    // Obtener casillas de origen y destino a partir de la cadena
    char from_str[3] = {move_str[0], move_str[1], '\0'};
    char to_str[3] = {move_str[2], move_str[3], '\0'};
    
    move->from = algebraic_to_square(from_str);
    move->to = algebraic_to_square(to_str);
    
    // Verificar si las casillas de origen y destino son válidas
    if (move->from == -1 || move->to == -1) return false;
    if (!IS_VALID_SQUARE(move->from) || !IS_VALID_SQUARE(move->to)) return false;
    
    move->piece = game->board[move->from];
    move->captured = game->board[move->to];
    move->promotion = 0;
    move->flags = MOVE_NORMAL;
    
    // Verificar si es una captura
    if (move->captured != EMPTY)
        move->flags = MOVE_CAPTURE;
    
    // Verificar si es una promoción (peón llega a la última fila)
    if (PIECE_TYPE(move->piece) == PAWN) {
        int dest_rank = RANK(move->to);
        if ((COLOR(move->piece) == WHITE && dest_rank == 7) ||
            (COLOR(move->piece) == BLACK && dest_rank == 0)) {
            move->flags = MOVE_PROMOTION;
            // Promoción por defecto a reina
            // TODO: Agregar las otras opciones de promoción (el usuario debe poder elegir)
            move->promotion = QUEEN;
        }
    }
    // TODO: Agregar detección de mensajes para enroque y en passant (captura al paso)
    return true;
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
        
        if (move_distance == 2) {
            game->en_passant_square = move->from + (move->to - move->from) / 2;
        }
    }
    
    // Reloj de medio movimiento
    if (piece_type == PAWN || move->captured != EMPTY) {
        game->halfmove_clock = 0;
    } else {
        game->halfmove_clock++;
    }
    
    // Aumenta el número de jugadas completas
    if (game->to_move == BLACK) {
        game->fullmove_number++;
    }
    
    // Cambia el turno
    game->to_move = (game->to_move == WHITE) ? BLACK : WHITE;
}

/**
 * Deshace un movimiento (para algoritmos de búsqueda, etc.)
 * Restaura el estado del juego al momento anterior al movimiento jugado.
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

    // Restore estado de juego
    game->castling_rights = history.old_castling_rights;
    game->en_passant_square = history.old_en_passant_square;
    game->halfmove_clock = history.old_halfmove_clock;
    game->fullmove_number = history.old_fullmove_number;
}

/**
 * Bucle principal del juego.
 * Se encarga de recibir los movimientos del usuario y de mostrar el tablero.
 * Por el momento, no existe un menú, y solo se puede jugar una partida.
 */
int main() {
    gamestate_t game;
    move_t move;
    char input[10];
    
    printf("¡Bienvenido a Fortuna Chess!\n");
    printf("Esta es una versión experimental, por lo que gran parte de las funcionalidades no están disponibles.\n");
    printf("Ingrese movimientos en formato: e2e4\n");
    printf("Escriba 'salir' para salir\n\n");
    
    init_board(&game);
    display_board(&game);
    
    while (1) {
        printf("Ingrese movimiento: ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        
        // Remover newline
        input[strcspn(input, "\n")] = '\0';
        
        if (strcmp(input, "salir") == 0) break;
        
        if (parse_move(input, &move, &game)) {
            if (is_legal_move(&move, &game)) {
                make_move(&move, &game, true);
                display_board(&game);
                
                // TODO: Agregar checks para jaque mate, aguas, etc. y terminar la partida con su correspondiente mensaje
                if (is_in_check(&game, game.to_move)) {
                    printf("¡Jaque!\n");
                }
            } else {
                printf("¡Movimiento ilegal!\n");
            }
        } else {
            printf("Formato de movimiento inválido. Use el formato [origen][destino]. Ejemplo: e2e4\n");
        }
    }
    
    printf("¡Gracias por jugar!\n");
    return 0;
}