#pragma once
#include <stdint.h>
#include <stdbool.h>

#define HASHTABLE_SIZE 4096         // Capacidad inicial de la tabla hash
#define MAX_MOVE_STR 6              // Largo máximo de cada string de movimiento
#define MAX_MOVES_PER_POSITION 10   // Máximo de entradas (move_entry_t) por bucket

typedef struct {
    char move[MAX_MOVE_STR];
    int priority;
} move_entry_t;

typedef struct {
    uint64_t key;                          // Zobrist key
    move_entry_t moves[MAX_MOVES_PER_POSITION];
    int move_count;
    bool occupied;                         // Flag para indicar que la key está ocupada
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