#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Fortuna Chess
// Motor de ajedrez simple usando representación de tablero 0x88
// Definiciones principales
#include "chess.h"
// Archivo que contiene las función de hash para libro de apertura
#include "zobrist.h"
// Función que contiene las funciones relacionadas al bot (Jugador vs CPU)
#include "bot.h"

//// Prototipos de funciones
// Funciones auxiliares
char piece_to_char(int piece);
int char_to_piece(char c);
int algebraic_to_square(const char *algebraic);
void square_to_algebraic(int square, char *algebraic);
// Menú principal
void display_board(gamestate_t *game, int p1);
// Input
bool parse_move(const char *move_str, move_t *move, gamestate_t *game);

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
 * Muestra el estado actual del tablero en la terminal.
 * @param game: puntero al estado del juego actual.
 */
void display_board(gamestate_t *game, int p1) {
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
    printf("Turno: %s (Jugador %d)\n", game->to_move == WHITE ? "Blancas" : "Negras", game->to_move == WHITE ? (p1 == 1 ? 1 : 2) : (p1 == 1 ? 2 : 1));

    // Muestra los derechos de enroque disponibles
    printf("Derechos de enroque: %s%s%s%s\n",
           (game->castling_rights & CASTLE_WHITE_KING) ? "K" : "",
           (game->castling_rights & CASTLE_WHITE_QUEEN) ? "Q" : "",
           (game->castling_rights & CASTLE_BLACK_KING) ? "k" : "",
           (game->castling_rights & CASTLE_BLACK_QUEEN) ? "q" : "");

    printf("[ DEBUG ] Hash de la posición: %llu\n", zobrist_hash(game));

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

// Función auxiliar para poder testear funcionamiento de la función Zobrist Hashing + TDA hashtable
// Simula el movimiento e2e4 en el tablero
// Asume que gamestate_t *game es un puntero al estado del juego en posición inicial
void make_dummy_e2e4(gamestate_t *game) {
    int from = SQUARE(1, 4);  // e2 (0x14)
    int to = SQUARE(3, 4);    // e4 (0x34)
    game->board[to] = game->board[from];
    game->board[from] = EMPTY;
    game->en_passant_square = SQUARE(2, 4);  // e3 (0x24)
    game->to_move = BLACK;
    game->move_count++;
}

int submenu_tiempo() {
    int opcion = 0;

    while (opcion < 1 || opcion > 4) {
        puts("\n⏱︎ FORMATO DE TIEMPO ⏱︎");
        puts("1. Blitz (3 min)");
        puts("2. Rápido (10 min)");
        puts("3. Sin tiempo");
        puts("4. Volver al menú principal");
        puts("Elija una opción: ");

        scanf("%d", &opcion);
        
        if (opcion < 1 || opcion > 4) {
            puts("\nOpcion no válida. Por favor, vuelva a intentarlo...");
            puts("Presione ENTER para volver al submenú...");
            getchar();
            getchar();
        }
    }
    
    if (opcion == 4) {
        return -1; // Volver al menú principal
    }
    return opcion;
}

int submenu_piezas() {
    int opcion = 0;

    while (opcion < 1 || opcion > 4) {
        puts("\n𖣯 SELECCIÓN DE PIEZAS 𖣯");
        puts("1. Blancas");
        puts("2. Negras");
        puts("3. Aleatorio");
        puts("4. Volver al menú principal");
        puts("Elija una opción: ");

        scanf("%d", &opcion);   

        if (opcion < 1 || opcion > 4) {
            puts("\nOpcion no válida. Por favor, vuelva a intentarlo...");
            puts("Presione ENTER para volver al submenú...");
            getchar();
            getchar();
        }
    }
    
    if (opcion == 4) {
        return -1; // Volver al menú principal
    }
    return opcion;
}

void iniciar_partida(int j1, int formato, int es_bot) {
    puts("\n♚ INICIANDO PARTIDA ♛");
    // TODO: IMPLEMENTAR ASIGNACIÓN ALEATORIA
    printf("Jugador 1: %s\n", j1 == 1 ? "Blancas" : j1 == 2 ? "Negras" : "Aleatorio");
    printf("Formato: %s\n", formato == 1 ? "Blitz" : formato == 2 ? "Rápido" : "Sin tiempo");
    printf("Modo: %s\n", es_bot ? "vs CPU" : "vs Jugador");
    puts("¡Que comience el juego! :)\n");

    gamestate_t game;
    move_t move;
    char input[16];

    // Finalizados los tests, se inicializa el tablero nuevamente:
    init_board(&game);

    // Menú principal

    // Se muestra el tablero en pantalla
    display_board(&game, j1);

    printf("Ingrese movimientos en formato: e2e4\n");
    printf("Escriba 'ayuda' para ver todos los comandos disponibles\n");
    printf("Escriba 'salir' para salir\n\n");
    
    while (1) {
        // Evaluar estado del juego, para saber si el bucle principal debe terminar
        game_result_t result = evaluate_game_state(&game);
        if (result != GAME_ONGOING) {
            printf("\n=== FINAL DEL JUEGO ===\n");
            // Mostrar en pantalla al ganador (o si es un empate, comunicar la razón)
            printf("%s\n", get_game_result_name(result));
            break;
        }

        printf("Ingrese movimiento o comando: ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        
        // Remover newline
        input[strcspn(input, "\n")] = '\0';
        
        if (strcmp(input, "salir") == 0) {
            break;
        }

        if (strcmp(input, "ayuda") == 0) {
            printf("Comandos disponibles:\n");
            printf("  [movimiento] - Realizar movimiento (ej: e2e4)\n");
            printf("  ayuda        - Mostrar esta ayuda\n");
            printf("  historial    - Mostrar historial de movimientos\n");
            printf("  deshacer     - Deshacer último movimiento\n");
            printf("  salir        - Salir del juego\n\n");
            continue;
        }

        if (strcmp(input, "historial") == 0) {
            printf("El historial de movimientos tiene %d movimientos almacenados.\n", stack_size(game.move_history));
            // TODO: Imprimir una lista de los movimientos de cada jugador, por turno.
            continue;
        }

        if (strcmp(input, "deshacer") == 0) {
            if (stack_is_empty(game.move_history)) {
                printf("No hay movimiento que deshacer. (Posición inicial)\n");
            } else {
                unmake_move(&game);
                display_board(&game, j1);
                printf("Movimiento deshecho.\n");
            }
            continue;
        }
        
        if (parse_move(input, &move, &game)) {
            if (is_legal_move(&move, &game)) {
                make_move(&move, &game, true);
                display_board(&game, j1);
                // TODO: Agregar checks para jaque mate, aguas, etc. y terminar la partida con su correspondiente mensaje
                if (is_in_check(&game, game.to_move)) {
                    printf("¡Jaque!\n");
                }
            } else {
                printf("¡Movimiento ilegal!\n");
            }
        } else {
            printf("Formato de movimiento inválido. Use el formato [origen][destino]. Ejemplo: e2e4\n");
            printf("O escriba 'ayuda' para ver todos los comandos disponibles.\n");
        }
    }
    
    printf("¡Gracias por jugar!\n");
}

void submenu_partida(int es_bot) {
    int formato = 0;
    int j1 = 0;
    
    // Elección de formato
    formato = submenu_tiempo();
    if (formato == -1) 
        return; // Volver al menú principal
    
    // Elección de pieza
    j1 = submenu_piezas();
    if (j1 == -1) 
        return; // Volver al menú principal
    
    // Empezar el juego
    iniciar_partida(j1, formato, es_bot);
    exit(EXIT_SUCCESS);
}

void menu_principal() {
    int opcion;
    
    do {
        puts("◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎");
        puts("◻︎        ♜   𝓕𝓞𝓡𝓣𝓤𝓝𝓐   𝓒𝓗𝓔𝓢𝓢   ♜        ◻︎");
        puts("◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎");
        puts("Bienvenido/a, elija una opción:");

        puts("1. Jugador vs Jugador (PvP) ⚔︎");
        puts("2. Jugador vs CPU (PvE) ⌨︎");
        puts("3. Salir :(");

        scanf("%d", &opcion);
        
        switch(opcion) {
            case 1:
                submenu_partida(0); // es_bot = 0 para PvP
                break;
            case 2:
                submenu_partida(1); // es_bot = 1 para PvE
                break;
            case 3:
                puts("\nSaliendo de Fortuna Chess. Muchas gracias por jugar, vuelva pronto ♞");
                break;
            default:
                puts("\nOpcion no válida. Por favor, vuelva a intentarlo:");
                puts("Presione ENTER para volver al menú...");
                getchar();
                getchar();
        }
    } while (opcion != 3);
}

/**
 * Bucle principal del juego.
 * Se encarga de recibir los movimientos del usuario y de mostrar el tablero.
 * Por el momento, no existe un menú, y solo se puede jugar una partida.
 */
int main() {
    // Establece la página de códigos de salida usada por la consola
    // Necesitamos hacer esto para que los caracteres especiales de se rendericen bien
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif

    gamestate_t game;
    move_t move;
    char input[10];

    // Realizar benchmark PERFT
    init_board(&game);
    perft_benchmark(&game, 5);

    // Test funcionamiento minimax (Grafo implícito)
    //init_board(&game);
    //move_t best_move = find_best_move(&game, 6);
    
    // Test funcionamiento de TDA tabla hash + Zobrist hashing
    zobrist_init();
    hashtable_t *book = hashtable_create();
    if (!book) {
        printf("[ HASHTABLE ] No se pudo crear un libro de aperturas como tabla hash\n");
        return 1;
    }

    // Obtener clave Zobrist para posición inicial
    uint64_t key_initial = zobrist_hash(&game);
    printf("[ ZOBRIST ] Clave posición inicial: %llu\n", key_initial);

    // Simulamos e2e4 y obtenemos la nueva clave
    make_dummy_e2e4(&game);
    uint64_t key_after_e4 = zobrist_hash(&game);
    printf("[ ZOBRIST ] Clave después de e2e4: %llu\n", key_after_e4);

    // Agregamos movimientos recomendados para las dos posiciones
    hashtable_add_move(book, key_initial, "e2e4", 10);
    hashtable_add_move(book, key_initial, "d2d4", 8);
    hashtable_add_move(book, key_after_e4, "e7e5", 10);
    hashtable_add_move(book, key_after_e4, "c7c5", 9);

    char recommended_move[MAX_MOVE_STR];
    if (hashtable_lookup_best_move(book, key_initial, recommended_move))
        printf("[ HASHTABLE ] Movimiento recomendado para posición inicial: %s\n", recommended_move);
    else
        printf("[ HASHTABLE ] No se encontró un movimiento para la posición inicial.\n");

    if (hashtable_lookup_best_move(book, key_after_e4, recommended_move))
        printf("[ HASHTABLE ] Movimiento recomendado después de e2e4: %s\n", recommended_move);
    else
        printf("[ HASHTABLE ] No se encontró un movimiento para la posición después de 1. e2e4.\n");

    hashtable_destroy(book);

    // Menú principal
    menu_principal();
    
    return 0;
}