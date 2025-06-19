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