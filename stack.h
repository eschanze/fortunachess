#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct stack_node {
    void *data;
    struct stack_node *next;
} stack_node_t;

typedef struct {
    stack_node_t *top;
    int size;
    size_t data_size;   // Para saber cuanto copiar
} chess_stack_t;
// 17/05/25: Se cambió de stack_t a chess_stack_t
// Ya que algunos OS (como macOS) tienen definido su propio stack_t

// Operaciones del stack
chess_stack_t* stack_create(size_t data_size);
void stack_destroy(chess_stack_t *stack);
bool stack_push(chess_stack_t *stack, const void *data);
bool stack_pop(chess_stack_t *stack, void *out);
void* stack_peek(chess_stack_t *stack);
bool stack_is_empty(chess_stack_t *stack);
int stack_size(chess_stack_t *stack);
void stack_clear(chess_stack_t *stack);