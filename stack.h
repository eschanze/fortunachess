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
} stack_t;

stack_t* stack_create(size_t data_size);
void stack_destroy(stack_t *stack);
bool stack_push(stack_t *stack, const void *data);
bool stack_pop(stack_t *stack, void *out);
void* stack_peek(stack_t *stack);
bool stack_is_empty(stack_t *stack);
int stack_size(stack_t *stack);
void stack_clear(stack_t *stack);