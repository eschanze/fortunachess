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
// Interfaz
void display_board(gamestate_t *game, int p1);
// Input
bool parse_move(const char *move_str, move_t *move, gamestate_t *game);
// Menú principal
void main_menu();
void game_submenu(int is_bot);
int time_submenu();
int piece_submenu();
void start_game(int player_piece, int time_format, int is_bot);

// Tabla hash que se utilizará como libro de apertura para el modo Jugador vs CPU
hashtable_t *book = NULL;

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
 * Muestra el estado actual del tablero en la terminal.
 * @param game: puntero al estado del juego actual.
 * @param p1: 1 sí el jugador 1 controla las piezas blancas, 2 en caso contrario.
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

    char fen[128];
    gamestate_to_fen(game, fen);
    printf("[ DEBUG ] Hash de la posición: %016llx\n", polyglot_hash(fen));

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
    
    //Verificar si es enroque (ej: "e1g1" o "e8c8")
    if (PIECE_TYPE(move->piece) == KING) {
        int diff = move->to - move->from;
        if ((COLOR(move->piece) == WHITE && move->from == SQUARE(0,4)) ||
            (COLOR(move->piece) == BLACK && move->from == SQUARE(7,4))) {
            if (diff == 2) {
                move->flags = MOVE_CASTLE_KING; // Enroque corto
            } else if (diff == -2) {
                move->flags = MOVE_CASTLE_QUEEN; // Enroque largo
            }
        }
    }

    // Verificar si es una promoción (peón llega a la última fila)
    if (PIECE_TYPE(move->piece) == PAWN) {
        int dest_rank = RANK(move->to);
        if ((COLOR(move->piece) == WHITE && dest_rank == 7) ||
            (COLOR(move->piece) == BLACK && dest_rank == 0)) {
            move->flags = MOVE_PROMOTION;
            // Si hay un quinto carácter, entonces es una promoción (ej: "e7e8r")
            if (strlen(move_str) >= 5) {
                switch (tolower(move_str[4])) {
                    case 'q': move->promotion = QUEEN; break;
                    case 'r': move->promotion = ROOK; break;
                    case 'b': move->promotion = BISHOP; break;
                    case 'n': move->promotion = KNIGHT; break;
                    default: move->promotion = QUEEN; break; // Por defecto, promoción a reina
                }
            } else {
                move->promotion = QUEEN; // Por defecto, promoción a reina
            }
        }
    }

    // Verificar si es una captura al paso (en passant)
    if (PIECE_TYPE(move->piece) == PAWN) {
        if (move->to == game->en_passant_square &&
            move->from % 16 != move->to % 16 && // Asegurarse de que no se mueve en la misma columna
            game->board[move->to] == EMPTY) { // Verifica que la casilla de destino esté vacía
                move->flags = MOVE_EN_PASSANT;
                move->captured = MAKE_PIECE(PAWN, game->to_move == WHITE ? BLACK : WHITE);
            }
    }
    return true;
}

/**
 * Muestra el menú principal del juego.
 * Permite elegir entre jugar contra otro jugador, contra la CPU, o salir.
 */
void main_menu() {
    int option;
    do {
        puts("◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎");
        puts("◻︎        ♜   𝓕𝓞𝓡𝓣𝓤𝓝𝓐   𝓒𝓗𝓔𝓢𝓢   ♜        ◻︎");
        puts("◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎◻︎◼︎");
        puts("Bienvenido/a, elija una opción:");

        puts("1. Jugador vs Jugador (PvP) ⚔︎");
        puts("2. Jugador vs CPU (PvE) ⌨︎");
        puts("3. Salir :(");

        scanf("%d", &option);
        
        switch(option) {
            case 1:
                game_submenu(0); // is_bot = 0 para PvP
                break;
            case 2:
                game_submenu(1); // is_bot = 1 para PvE
                break;
            case 3:
                puts("\nSaliendo de Fortuna Chess. Muchas gracias por jugar, vuelva pronto ♞");
                break;
            default:
                puts("\nOpción no válida. Por favor, vuelva a intentarlo:");
                puts("Presione ENTER para volver al menú...");
                getchar();
                getchar();
        }
    } while (option != 3);
}

/**
 * Muestra el submenú de configuración de partida y lanza el juego.
 * Permite elegir formato de tiempo y color de pieza.
 * @param is_bot: 1 si se juega contra la CPU, 0 si es PvP.
 */
void game_submenu(int is_bot) {
    int time_format = 0;
    int player_piece = 0;
    
    // Elección de format
    time_format = time_submenu();
    if (time_format == -1) 
        return; // Volver al menú principal
    
    // Elección de pieza
    player_piece = piece_submenu();
    if (player_piece == -1) 
        return; // Volver al menú principal
    
    // Empezar el juego
    start_game(player_piece, time_format, is_bot);
    exit(EXIT_SUCCESS);
}

/**
 * Muestra el submenú de selección de formato de tiempo.
 * Permite al usuario elegir entre distintos controles de tiempo o volver al menú principal.
 * @return número correspondiente al formato elegido, o -1 para volver al menú principal.
 */
int time_submenu() {
    int option = 0;

    while (option < 1 || option > 4) {
        puts("\n⏱︎ FORMATO DE TIEMPO ⏱︎");
        puts("1. Blitz (3 min)");
        puts("2. Rápido (10 min)");
        puts("3. Sin tiempo");
        puts("4. Volver al menú principal");
        puts("Elija una opción: ");

        scanf("%d", &option);
        
        if (option < 1 || option > 4) {
            puts("\nOpción no válida. Por favor, vuelva a intentarlo...");
            puts("Presione ENTER para volver al submenú...");
            getchar();
            getchar();
        }
    }
    
    if (option == 4) {
        return -1; // Volver al menú principal
    }
    return option;
}

/**
 * Muestra el submenú de selección de color de piezas.
 * Permite al usuario elegir jugar con blancas, negras o aleatorio.
 * @return número correspondiente a la elección, o -1 para volver al menú principal.
 */
int piece_submenu() {
    int option = 0;

    while (option < 1 || option > 4) {
        puts("\n𖣯 SELECCIÓN DE PIEZAS 𖣯");
        puts("1. Blancas");
        puts("2. Negras");
        puts("3. Aleatorio");
        puts("4. Volver al menú principal");
        puts("Elija una opción: ");

        scanf("%d", &option);   

        if (option < 1 || option > 4) {
            puts("\nOpción no válida. Por favor, vuelva a intentarlo...");
            puts("Presione ENTER para volver al submenú...");
            getchar();
            getchar();
        }
    }
    
    if (option == 4) {
        return -1; // Volver al menú principal
    }
    return option;
}

/**
 * Inicia el juego con los parámetros elegidos.
 * @param player_piece: color elegido por el jugador.
 * @param time_format: formato de tiempo elegido.
 * @param is_bot: 1 si se juega contra la CPU, 0 si es PvP.
 */
void start_game(int p1, int format, int is_bot) {
    // Si el usuario eligio aleatorio, asignamos las piezas de forma aleatoria
    if (p1 == 3) {
        srand((unsigned int)time(NULL));
        p1 = (rand() % 2) + 1; // 1 o 2 aleatoriamente
    }
    
    puts("\n♚ INICIANDO PARTIDA ♛");
    // TODO: IMPLEMENTAR ASIGNACIÓN ALEATORIA
    printf("Jugador 1: %s\n", p1 == 1 ? "Blancas" : p1 == 2 ? "Negras" : "Aleatorio");
    printf("Formato: %s\n", format == 1 ? "Blitz" : format == 2 ? "Rápido" : "Sin tiempo");
    printf("Modo: %s\n", is_bot ? "vs CPU" : "vs Jugador");
    puts("¡Que comience el juego! :)\n");

    gamestate_t game;
    move_t move;
    char input[16];

    // Finalizados los tests, se inicializa el tablero nuevamente:
    init_board(&game);

    // Variables locales para el tiempo
    int white_time = 0, black_time = 0;
    if (format == 1) {
        white_time = 180;
        black_time = 180;
    } else if (format == 2) {
        white_time = 600;
        black_time = 600;
    }

    // Se muestra el tablero en pantalla
    display_board(&game, p1);

    printf("Ingrese movimientos en formato: e2e4\n");
    printf("Escriba 'ayuda' para ver todos los comandos disponibles\n");
    printf("Escriba 'salir' para salir\n\n");
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF); // limpia el búfer por si quedó un \n pendiente después de los scanf(...)

    while (1) {
        // Evaluar estado del juego, para saber si el bucle principal debe terminar
        game_result_t result = evaluate_game_state(&game);
        if (result != GAME_ONGOING) {
            printf("\n=== FINAL DEL JUEGO ===\n");
            // Mostrar en pantalla al ganador (o si es un empate, comunicar la razón)
            printf("%s\n", get_game_result_name(result));
            break;
        }

        // Si es que juega el bot:
        if (is_bot && ((p1 == 1 && game.to_move == BLACK) || (p1 == 2 && game.to_move == WHITE))) {
            printf("Turno de la CPU...\n");
            move_t best_move = find_best_move(&game, 4);
            make_move(&best_move, &game, true);
            display_board(&game, p1);
            continue; // Salta al siguiente turno después de que la CPU haga su movimiento 
        }

        // Medir tiempo de inicio del turno
        time_t start_time = time(NULL);

        printf("Ingrese movimiento o comando: ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;

        // Medir tiempo de fin del turno
        time_t end_time = time(NULL);
        int elapsed_time = (int)(end_time - start_time); // Tiempo transcurrido
        
        // Solo descontar y mostrar tiempo si hay un formato de tiempo activo
        if (format == 1 || format == 2) {
            // Restar tiempo al jugador que movió
            if (game.to_move == WHITE)
                white_time -= elapsed_time;
            else
                black_time -= elapsed_time;
            
            // Mostrar tiempo restante
            printf("Tiempo restante - Blancas: %d:%02d | Negras: %d:%02d\n", 
                white_time / 60, white_time % 60,
                black_time / 60, black_time % 60);
            
            // Verificar si algun jugador se quedó sin tiempo
            if (white_time <= 0) {
                printf("=== FINAL DEL JUEGO ===\n");
                printf("¡Tiempo agotado para las blancas! Las negras ganan por tiempo.\n");
                break;
            }
            if (black_time <= 0) {
                printf("=== FINAL DEL JUEGO ===\n");
                printf("¡Tiempo agotado para las negras! Las blancas ganan por tiempo.\n");
                break;
            }
        }
        
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
                display_board(&game, p1);
                printf("Movimiento deshecho.\n");
            }
            continue;
        }
        
        if (parse_move(input, &move, &game)) {
            if (is_legal_move(&move, &game)) {
                make_move(&move, &game, true);
                display_board(&game, p1);
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

    // Initializar estructuras principales para los tests
    gamestate_t game;
    move_t move;

    // Realizar benchmark PERFT en una posición complicada
    // Debería dar:
    // Profunidad 1: 48 nodos
    // Profunidad 2: 2039 nodos
    // Profunidad 3: 97862 nodos
    // Profunidad 4: 4085603 nodos
    // Profunidad 5: 193690690 nodos
    const char *perft_fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
    init_board_fen(&game, perft_fen);
    perft_benchmark(&game, 4);

    // Test funcionamiento minimax (Grafo implícito)
    //init_board(&game);
    //move_t best_move = find_best_move(&game, 6);

    init_board(&game);
    
    // Test funcionamiento de TDA tabla hash + Zobrist hashing
    book = hashtable_create();
    if (!load_polyglot_book("book.bin", book)) {
        printf("[ HASHTABLE ] No se pudo cargar libro de aperturas (book.bin)\n");
        hashtable_destroy(book);
        return 1;
    }

    printf("[ HASHTABLE ] Se cargaron %d posiciones correctamente\n", hashtable_get_size(book));

    // Test gamestate_t a FEN
    char fen[128];
    gamestate_to_fen(&game, fen);
    printf("[ DEBUG ] FEN: %s\n", fen);

    // Obtener clave Zobrist para posición inicial
    uint64_t key_initial = polyglot_hash(fen);
    printf("[ ZOBRIST ] Clave posición inicial: %016llx\n", key_initial);

    // Simulamos e2e4 y obtenemos la nueva clave
    make_dummy_e2e4(&game);
    gamestate_to_fen(&game, fen);
    uint64_t key_after_e4 = polyglot_hash(fen);
    printf("[ ZOBRIST ] Clave después de e2e4: %016llx\n", key_after_e4);

    // Utilizamos nuestra hashtable (libro de apertura) para obtener los mejores movimientos en 2 posiciones de prueba 
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
    main_menu();
    
    return 0;
}