/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004 Paul Pogonyshev.                       *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of the GNU General Public License as  *
 * published by the Free Software Foundation; either version 2 of  *
 * the License, or (at your option) any later version.             *
 *                                                                 *
 * This program is distributed in the hope that it will be useful, *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of  *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the   *
 * GNU General Public License for more details.                    *
 *                                                                 *
 * You should have received a copy of the GNU General Public       *
 * License along with this program; if not, write to the Free      *
 * Software Foundation, Inc., 59 Temple Place - Suite 330,         *
 * Boston, MA 02111-1307, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include "othello.h"
#include "board-internals.h"

#include <stdio.h>
#include <assert.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


static inline int  is_legal_move(const char grid[BOARD_FULL_GRID_SIZE],
				 BoardRuleSet rule_set, int color, int pos);


int
othello_adjust_color_to_play(const Board *board, BoardRuleSet rule_set,
			     int color)
{
  int pos;
  int check_color = color;

  if (rule_set == OTHELLO_RULE_SET_SGF)
    return color;

  assert(rule_set < NUM_OTHELLO_RULE_SETS);

  do {
    for (pos = POSITION(0, 0); ON_GRID(board->grid, pos);
	 pos += (BOARD_MAX_WIDTH + 1) - board->width) {
      for (; ON_GRID(board->grid, pos); pos++) {
	if (board->grid[pos] == EMPTY
	    && is_legal_move(board->grid, rule_set, check_color, pos))
	  return check_color;
      }
    }

    check_color = OTHER_COLOR(check_color);
  } while (check_color != color);

  return EMPTY;
}


/* Get the default setup for Othello (only defined for even-sized
 * boards):
 *
 *	....
 *	.OX.
 *	.XO.
 *	....
 */
int
othello_get_default_setup(int width, int height,
			  BoardPositionList **black_stones,
			  BoardPositionList **white_stones)
{
  if (width % 2 != 0 || height % 2 != 0)
    return 0;

  *black_stones = board_position_list_new_empty(2);
  (*black_stones)->positions[0] = POSITION(width / 2, height / 2 - 1);
  (*black_stones)->positions[1] = POSITION(width / 2 - 1, height / 2);

  *white_stones = board_position_list_new_empty(2);
  (*white_stones)->positions[0] = POSITION(width / 2 - 1, height / 2 - 1);
  (*white_stones)->positions[1] = POSITION(width / 2, height / 2);

  return 1;
}


/* Determine if a move is legal according to specified rule set. */
int
othello_is_legal_move(const Board *board, BoardRuleSet rule_set,
		      int color, va_list move)
{
  int x = va_arg(move, int);
  int y = va_arg(move, int);
  int pos = POSITION(x, y);

  assert(rule_set < NUM_OTHELLO_RULE_SETS);
  assert(ON_BOARD(board, x, y));

  if (rule_set != OTHELLO_RULE_SET_SGF) {
    if (board->grid[pos] == EMPTY)
      return is_legal_move(board->grid, rule_set, color, pos);

    return 0;
  }

  return 1;
}


static inline int
is_legal_move(const char grid[BOARD_FULL_GRID_SIZE], BoardRuleSet rule_set,
	      int color, int pos)
{
  int k;
  int other = OTHER_COLOR(color);

  UNUSED(rule_set);

  for (k = 0; k < 8; k++) {
    int beam = pos + delta[k];

    if (grid[beam] == other) {
      do
	beam += delta[k];
      while (grid[beam] == other);

      if (grid[beam] == color)
	return 1;
    }
  }

  return 0;
}


void
othello_play_move(Board *board, int color, va_list move)
{
  char *grid = board->grid;
  int k;
  int other = OTHER_COLOR(color);
  int x = va_arg(move, int);
  int y = va_arg(move, int);
  int pos = POSITION(x, y);
  OthelloMoveStackEntry *stack_entry = ALLOCATE_OTHELLO_MOVE_STACK_ENTRY(board);

  assert(ON_BOARD(board, x, y));

  memset(stack_entry->num.flips, 0, sizeof(stack_entry->num.flips));

  for (k = 0; k < 8; k++) {
    int beam = pos + delta[k];

    if (grid[beam] == other) {
      do
	beam += delta[k];
      while (grid[beam] == other);

      if (grid[beam] == color) {
	beam -= delta[k];

	do {
	  grid[beam] = color;
	  stack_entry->num.flips[k]++;
	  beam -= delta[k];
	} while (beam != pos);
      }
    }
  }

  stack_entry->position    = pos;
  stack_entry->contents    = grid[pos];
  stack_entry->move_number = board->move_number++;

  grid[pos] = color;
}


void
othello_undo(Board *board)
{
  OthelloMoveStackEntry *stack_entry = POP_OTHELLO_MOVE_STACK_ENTRY(board);

  if (stack_entry->position != NULL_POSITION) {
    int k;
    int pos = stack_entry->position;
    int other = OTHER_COLOR(board->grid[pos]);

    for (k = 0; k < 8; k++) {
      if (stack_entry->num.flips[k]) {
	int flips = 0;
	int beam = pos;

	do {
	  beam += delta[k];
	  board->grid[beam] = other;
	} while (++flips < stack_entry->num.flips[k]);
      }
    }

    board->grid[pos] = stack_entry->contents;
  }
  else if (stack_entry->num.changes > 0)
    board_undo_changes(board, stack_entry->num.changes);

  board->move_number = stack_entry->move_number;
}


void
othello_apply_changes(Board *board, int num_changes)
{
  OthelloMoveStackEntry *stack_entry
    = ALLOCATE_OTHELLO_MOVE_STACK_ENTRY(board);

  stack_entry->position	   = NULL_POSITION;
  stack_entry->num.changes = num_changes;
  stack_entry->move_number = board->move_number;
}


void
othello_add_dummy_move_entry(Board *board)
{
  othello_apply_changes(board, 0);
}


int
othello_format_move(int board_width, int board_height,
		    char *buffer, va_list move)
{
  int x = va_arg(move, int);
  int y = va_arg(move, int);

  return game_format_point(GAME_OTHELLO, board_width, board_height,
			   buffer, x, y);
}


int
othello_parse_move(int board_width, int board_height, const char *move_string,
		   int *x, int *y, BoardAbstractMoveData *move_data)
{
  UNUSED(move_data);

  return game_parse_point(GAME_OTHELLO, board_width, board_height,
			  move_string, x, y);
}


void
othello_validate_board(const Board *board)
{
  UNUSED(board);
}


void
othello_dump_board(const Board *board)
{
  static const char contents[] = {'.', '@', 'O', '?'};
  static const char coordinates[] =
    " A B C D E F G H I J K L M N O P Q R S T U V W X Y";

  int x;
  int y;

  fprintf(stderr, "   %.*s\n", board->width * 2, coordinates);

  for (y = 0; y < board->height; y++) {
    fprintf(stderr, "%3d", board->height - y);

    for (x = 0; x < board->width; x++)
      fprintf(stderr, " %c", contents[(int) board->grid[POSITION(x, y)]]);

    fprintf(stderr, "%3d\n", board->height - y);
  }

  fprintf(stderr, "   %.*s\n", board->width * 2, coordinates);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
