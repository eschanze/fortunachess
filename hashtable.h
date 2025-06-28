#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HASHTABLE_SIZE 262144       // Capacidad inicial de la tabla hash
#define MAX_MOVE_STR 6              // Largo máximo de cada string de movimiento
#define MAX_MOVES_PER_POSITION 10   // Máximo de entradas (move_entry_t) por bucket

typedef struct {
    char move[MAX_MOVE_STR];
    int priority;
} move_entry_t;

typedef struct {
    uint64_t key;                                   // Zobrist key
    move_entry_t moves[MAX_MOVES_PER_POSITION];     // Lista de move_entry (movimiento 0x88 + prioridad)
    int move_count;
    bool occupied;                                  // Flag para indicar que la key está ocupada
} hashtable_entry_t;

typedef struct {
    hashtable_entry_t entries[HASHTABLE_SIZE];
    int size;
} hashtable_t;

// Operaciones de la hashtable
// Estrategia de resolución de colisiones: sondeo lineal
hashtable_t* hashtable_create(void);
void hashtable_destroy(hashtable_t *ht);
bool hashtable_insert(hashtable_t *ht, uint64_t key, const char *move, int priority);
bool hashtable_resize(hashtable_t *ht, int new_capacity);
bool hashtable_add_move(hashtable_t *ht, uint64_t key, const char *move, int priority);
int hashtable_get_moves(hashtable_t *ht, uint64_t key, char moves[][MAX_MOVE_STR], int *priorities, int max_moves);
bool hashtable_lookup_best_move(hashtable_t *ht, uint64_t key, char *move_out);
void hashtable_remove(hashtable_t *ht, uint64_t key);
void hashtable_clear(hashtable_t *ht);
int hashtable_get_size(hashtable_t *ht);

// Las siguientes funciones se obtuvieron del código que se provee en
// http://hgm.nubati.net/book_format.html
// Es la guía de como leer entradas en formato PolyGlot (un Zobrist hash específico + leer el libro de apertura book.bin)
typedef struct {
    uint64_t key;	
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
} polyglot_entry_t;

static int read_entry(FILE *f, polyglot_entry_t *entry);
static void polyglot_move_to_string(char str[6], uint16_t move);
int load_polyglot_book(const char *filename, hashtable_t *table);
void print_moves_for_key(hashtable_t *table, uint64_t key);