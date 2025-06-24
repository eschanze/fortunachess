#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
// TDAs
#include "stack.h"
#include "hashtable.h"


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

// Declaración de función auxiliar que añade un movimiento (move_t) al array de move_t de move_list_t
void add_move(move_list_t *list, int from, int to, int piece, int captured, int promotion, int flags);

// Estructura que representa el estado actual del juego
typedef struct {
    int board[BOARD_SIZE];          // Representación del tablero 0x88
    int to_move;                    // Turno actual: WHITE o BLACK
    int castling_rights;            // Campo de bits: KQkq (Blanco: K=1, Q=2, Negro: k=4, q=8)
    int en_passant_square;          // Casilla "fantasma" detrás del peón que avanzó 2 casillas (-1 si no hay)
    int halfmove_clock;             // Movimientos desde último movimiento de peón o captura de alguna pieza
    int fullmove_number;            // Número de jugadas completas
    int king_square[2];             // Posiciones de los reyes en formato [WHITE, BLACK]
    chess_stack_t *move_history;    // Pila que almacena el historial de movimientos realizados
    int move_count;                 // Contador de movimientos realizados
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

// Estructura que guarda información acerca del estado de juego, menos el tablero
// Se utiliza para almacenar la información del turno anterior, y poder deshacer de manera rápida
typedef struct {
    int castling_rights;
    int en_passant_square;
    int halfmove_clock;
    int fullmove_number;
    int captured_piece;
    int king_square[2];
} fast_undo_t;

// Declaración de los vectores externos de movimiento
// Se definen en chess.c
extern int knight_moves[8];
extern int king_moves[8];
extern int bishop_dirs[4];
extern int rook_dirs[4];

// Todas los posibles casos en los que puede terminar una partida
typedef enum {
    GAME_ONGOING,           // Juego en curso
    GAME_CHECKMATE_WHITE,   // Jaque mate - ganan las blancas
    GAME_CHECKMATE_BLACK,   // Jaque mate - ganan las negras
    GAME_STALEMATE,         // Tablas por ahogado
    GAME_DRAW_50_MOVES,     // Tablas por regla de 50 movimientos
    GAME_DRAW_REPETITION,   // Tablas por repetición (implementación futura)
    GAME_DRAW_MATERIAL      // Tablas por material insuficiente
} game_result_t;

// Inicialización
void init_board(gamestate_t *game);
// Lógica del juego (legalidad, generación de movimientos, etc.)
bool is_slide_valid(move_t *move, gamestate_t *game, int dir);
bool is_square_attacked(gamestate_t *game, int square, int by_color);
bool is_in_check(gamestate_t *game, int color);
bool is_legal_move(move_t *move, gamestate_t *game);
void make_move(move_t *move, gamestate_t *game, bool committed);
void unmake_move(gamestate_t *game);
// Versión alternativa la función unmake_move, que no utiliza pilas
void prepare_fast_undo(gamestate_t *game, move_t *move, fast_undo_t *undo_info);
void fast_unmake_move(gamestate_t *game, move_t *move, fast_undo_t *undo_info);
// Generación de movimientos
void generate_pawn_moves(gamestate_t *game, move_list_t *list, int from);
void generate_knight_moves(gamestate_t *game, move_list_t *list, int from);
void generate_sliding_moves(gamestate_t *game, move_list_t *list, int from, int *directions, int num_dirs);
void generate_king_moves(gamestate_t *game, move_list_t *list, int from);
void generate_moves(gamestate_t *game, move_list_t *list);
// Condiciones de fin de partida
const char* get_game_result_name(game_result_t result);
bool has_legal_moves(gamestate_t *game);
void count_material(gamestate_t *game, int white_material[5], int black_material[5]);
bool is_insufficient_material(gamestate_t *game);
game_result_t evaluate_game_state(gamestate_t *game);
// Benchmarking y testing
uint64_t perft(gamestate_t *game, int depth);
void perft_benchmark(gamestate_t *game, int max_depth); // output detallado (sólo para debuggear)