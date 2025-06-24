#pragma once
#include <stdio.h>
#include <limits.h>
#include "chess.h"

int evaluate_position(gamestate_t *game);
int is_game_over(gamestate_t *game);
int score_move(gamestate_t *game, move_t *move);
void sort_moves(gamestate_t *game, move_list_t *moves);
int alpha_beta(gamestate_t *game, int depth, int alpha, int beta, int maximizing_player);
move_t find_best_move(gamestate_t *game, int depth);
void print_search_info(gamestate_t *game, int depth, int score, move_t *move);