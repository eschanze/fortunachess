#include "stack.h"
#include <stdlib.h>
#include <string.h>

stack_t* stack_create(size_t data_size) {
    stack_t *stack = malloc(sizeof(stack_t));
    if (!stack) return NULL;

    stack->top = NULL;
    stack->size = 0;
    stack->data_size = data_size;
    return stack;
}

void stack_destroy(stack_t *stack) {
    if (!stack) return;
    stack_clear(stack);
    free(stack);
}

bool stack_push(stack_t *stack, const void *data) {
    if (!stack || !data) return false;

    stack_node_t *new_node = malloc(sizeof(stack_node_t));
    if (!new_node) return false;

    new_node->data = malloc(stack->data_size);
    if (!new_node->data) {
        free(new_node);
        return false;
    }

    memcpy(new_node->data, data, stack->data_size);
    new_node->next = stack->top;
    stack->top = new_node;
    stack->size++;
    return true;
}

bool stack_pop(stack_t *stack, void *out) {
    if (!stack || !stack->top || !out) return false;

    stack_node_t *top_node = stack->top;
    memcpy(out, top_node->data, stack->data_size);

    stack->top = top_node->next;
    stack->size--;

    free(top_node->data);
    free(top_node);
    return true;
}

void* stack_peek(stack_t *stack) {
    if (!stack || !stack->top) return NULL;
    return stack->top->data;
}

bool stack_is_empty(stack_t *stack) {
    return (stack == NULL || stack->top == NULL);
}

int stack_size(stack_t *stack) {
    return stack ? stack->size : 0;
}

void stack_clear(stack_t *stack) {
    if (!stack) return;
    while (stack->top) {
        stack_node_t *temp = stack->top;
        stack->top = temp->next;
        free(temp->data);
        free(temp);
    }
    stack->size = 0;
}