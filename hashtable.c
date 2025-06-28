#include "hashtable.h"
#include <stdlib.h>
#include <string.h>

static uint64_t hash_index(uint64_t key) {
    return key % HASHTABLE_SIZE;
}

hashtable_t* hashtable_create(void) {
    hashtable_t *ht = malloc(sizeof(hashtable_t));
    if (!ht) return NULL;
    memset(ht, 0, sizeof(hashtable_t));
    return ht;
}

void hashtable_destroy(hashtable_t *ht) {
    if (!ht) return;
    free(ht);
}

bool hashtable_insert(hashtable_t *ht, uint64_t key, const char *move, int priority) {
    uint64_t index = hash_index(key);
    for (int i = 0; i < HASHTABLE_SIZE; i++) {
        uint64_t try = (index + i) % HASHTABLE_SIZE;
        hashtable_entry_t *entry = &ht->entries[try];
        if (!entry->occupied) {
            entry->key = key;
            strncpy(entry->moves[0].move, move, MAX_MOVE_STR);
            entry->moves[0].move[MAX_MOVE_STR - 1] = '\0';
            entry->moves[0].priority = priority;
            entry->move_count = 1;
            entry->occupied = true;
            ht->size++;
            return true;
        } else if (entry->occupied && entry->key == key) {
            if (entry->move_count >= MAX_MOVES_PER_POSITION) return false;
            for (int j = 0; j < entry->move_count; j++) {
                if (strncmp(entry->moves[j].move, move, MAX_MOVE_STR) == 0) return false;
            }
            strncpy(entry->moves[entry->move_count].move, move, MAX_MOVE_STR);
            entry->moves[entry->move_count].move[MAX_MOVE_STR - 1] = '\0';
            entry->moves[entry->move_count].priority = priority;
            entry->move_count++;
            return true;
        }
    }
    return false;
}

bool hashtable_add_move(hashtable_t *ht, uint64_t key, const char *move, int priority) {
    return hashtable_insert(ht, key, move, priority);
}

int hashtable_get_moves(hashtable_t *ht, uint64_t key, char moves[][MAX_MOVE_STR], int *priorities, int max_moves) {
    uint64_t index = hash_index(key);
    for (int i = 0; i < HASHTABLE_SIZE; i++) {
        uint64_t try = (index + i) % HASHTABLE_SIZE;
        hashtable_entry_t *entry = &ht->entries[try];
        if (entry->occupied && entry->key == key) {
            int count = (entry->move_count < max_moves) ? entry->move_count : max_moves;
            for (int j = 0; j < count; j++) {
                strncpy(moves[j], entry->moves[j].move, MAX_MOVE_STR);
                moves[j][MAX_MOVE_STR - 1] = '\0';
                if (priorities) priorities[j] = entry->moves[j].priority;
            }
            return count;
        }
    }
    return 0;
}

bool hashtable_lookup_best_move(hashtable_t *ht, uint64_t key, char *move_out) {
    uint64_t index = hash_index(key);
    for (int i = 0; i < HASHTABLE_SIZE; i++) {
        uint64_t try = (index + i) % HASHTABLE_SIZE;
        hashtable_entry_t *entry = &ht->entries[try];
        if (entry->occupied && entry->key == key && entry->move_count > 0) {
            int best_idx = 0;
            for (int j = 1; j < entry->move_count; j++) {
                if (entry->moves[j].priority > entry->moves[best_idx].priority) {
                    best_idx = j;
                }
            }
            strncpy(move_out, entry->moves[best_idx].move, MAX_MOVE_STR);
            move_out[MAX_MOVE_STR - 1] = '\0';
            return true;
        }
    }
    return false;
}

void hashtable_remove(hashtable_t *ht, uint64_t key) {
    uint64_t index = hash_index(key);
    for (int i = 0; i < HASHTABLE_SIZE; i++) {
        uint64_t try = (index + i) % HASHTABLE_SIZE;
        hashtable_entry_t *entry = &ht->entries[try];
        if (entry->occupied && entry->key == key) {
            memset(entry, 0, sizeof(hashtable_entry_t));
            ht->size--;
            return;
        }
    }
}

void hashtable_clear(hashtable_t *ht) {
    memset(ht->entries, 0, sizeof(ht->entries));
    ht->size = 0;
}

int hashtable_get_size(hashtable_t *ht) {
    return ht->size;
}

bool hashtable_resize(hashtable_t *ht, int new_capacity) {
    return false;
}

static int read_entry(FILE *f, polyglot_entry_t *entry) {
    uint64_t r = 0;
    int c;

    #define READ_BYTES(n) do { \
        r = 0; \
        for (int i = 0; i < (n); i++) { \
            c = fgetc(f); \
            if (c == EOF) return 0; \
            r = (r << 8) | (uint8_t)c; \
        } \
    } while (0)

    READ_BYTES(8); entry->key = r;
    READ_BYTES(2); entry->move = (uint16_t)r;
    READ_BYTES(2); entry->weight = (uint16_t)r;
    READ_BYTES(4); entry->learn = (uint32_t)r;
    return 1;

    #undef READ_BYTES
}

static void polyglot_move_to_string(char str[6], uint16_t move) {
    const char *promote_pieces = " nbrq";

    int from = (move >> 6) & 0x3F;
    int to   = move & 0x3F;
    int prom = (move >> 12) & 0x7;

    int from_file = from & 7, from_rank = from >> 3;
    int to_file   = to & 7, to_rank   = to >> 3;

    str[0] = 'a' + from_file;
    str[1] = '1' + from_rank;
    str[2] = 'a' + to_file;
    str[3] = '1' + to_rank;
    if (prom) {
        str[4] = promote_pieces[prom];
        str[5] = '\0';
    } else {
        str[4] = '\0';
    }

    // Enroque
    if (strcmp(str, "e1h1") == 0) strcpy(str, "e1g1");
    else if (strcmp(str, "e1a1") == 0) strcpy(str, "e1c1");
    else if (strcmp(str, "e8h8") == 0) strcpy(str, "e8g8");
    else if (strcmp(str, "e8a8") == 0) strcpy(str, "e8c8");
}

int load_polyglot_book(const char *filename, hashtable_t *table) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("[ HASHTABLE ] Error al abrir el archivo");
        return 0;
    }

    polyglot_entry_t entry;
    while (read_entry(f, &entry)) {
        char move_str[6];
        polyglot_move_to_string(move_str, entry.move);
        hashtable_add_move(table, entry.key, move_str, entry.weight);
    }

    fclose(f);
    return 1;
}

void print_moves_for_key(hashtable_t *table, uint64_t key) {
    char moves[MAX_MOVES_PER_POSITION][MAX_MOVE_STR];
    int priorities[MAX_MOVES_PER_POSITION];
    int count = hashtable_get_moves(table, key, moves, priorities, MAX_MOVES_PER_POSITION);

    if (count == 0) {
        printf("[ HASHTABLE ] No se encontraron movimientso para la llave %016llx\n", (unsigned long long)key);
        return;
    }

    printf("[ HASHTABLE ] Se encontraron %d movimientos para la llave %016llx:\n", count, (unsigned long long)key);
    for (int i = 0; i < count; i++) {
        printf("  Movimiento = %s (prioridad = %d)\n", moves[i], priorities[i]);
    }
}