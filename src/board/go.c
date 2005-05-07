/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *	\
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
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,     *
 * Boston, MA 02110-1301, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include "go.h"
#include "board-internals.h"
#include "utils.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


#define LIBERTY(grid, pos)	((grid) [pos] == EMPTY)

#define STRING_NUMBER(board, pos)					\
  ((board)->data.go.string_number[pos])

#define LIBERTIES(board, pos)						\
  ((board)->data.go.liberties[STRING_NUMBER ((board), (pos))])

#define MARK_POSITION(board, pos)					\
  ((board)->data.go.marked_positions[pos]				\
   = (board)->data.go.position_mark)
#define UNMARKED_POSITION(board, pos)					\
  ((board)->data.go.marked_positions[pos]				\
   != (board)->data.go.position_mark)

#define MARK_STRING(board, pos)						\
  ((board)->data.go.marked_strings[STRING_NUMBER ((board), (pos))]	\
   = (board)->data.go.string_mark)
#define UNMARKED_STRING(board, pos)					\
  ((board)->data.go.marked_strings[STRING_NUMBER ((board), (pos))]	\
   != (board)->data.go.string_mark)


enum {
  ALLY = EMPTY + 1,
  OPPONENT,
  CAPTURE
};

enum {
  NORMAL_MOVE = PASS_MOVE + 1,
  POSITION_CHANGE,
  DUMMY_MOVE_ENTRY
};


static int	is_suicide (const Board *board, int color, int pos);


static void	rebuild_strings (Board *board);

static void	do_play_move (Board *board, int color, int pos);
static void	do_play_over_own_stone (Board *board, int pos);
static void	do_play_over_enemy_stone (Board *board, int pos);

static int	join_strings (Board *board, int color, int pos,
			      int new_liberties, int *allies, int num_allies);
static int	remove_string (Board *board, int pos);
static int	change_string_number (Board *board, int pos,
				      int string_number);
static void	reconstruct_string (Board *board, int color, int pos,
				    int single_liberty);


static int	allocate_string (Board *board);


void
go_reset_game_data (Board *board, int forced_reset)
{
  board->data.go.ko_master		= EMPTY;
  board->data.go.prisoners[BLACK_INDEX] = 0;
  board->data.go.prisoners[WHITE_INDEX] = 0;

  board->data.go.last_string_number = -1;
  if (forced_reset) {
    int k;

    for (k = 0; k < GO_STRING_RING_SIZE; k++)
      board->data.go.liberties[k] = -1;
  }

  if (forced_reset || board->data.go.position_mark != 0) {
    int x;
    int y;
    int pos;

    board->data.go.position_mark = 0;

    for (y = -1, pos = POSITION (-1, -1); y <= board->height; y++) {
      for (x = -1; x <= board->width; x++, pos++)
	board->data.go.marked_positions[pos] = 0;
      pos += BOARD_MAX_WIDTH + 1 - (1 + board->width + 1);
    }
  }

  if (forced_reset || board->data.go.string_mark != 0) {
    board->data.go.string_mark = 0;
    memset (board->data.go.marked_strings, 0,
	    sizeof board->data.go.marked_strings);
  }
}


int
go_is_game_over (const Board *board, BoardRuleSet rule_set, int color_to_play)
{
  GoMoveStackEntry *stack_entry = ((GoMoveStackEntry *)
				   board->move_stack_pointer);

  UNUSED (rule_set);
  UNUSED (color_to_play);

  while (--stack_entry > (GoMoveStackEntry *) board->move_stack) {
    if (stack_entry->type == PASS_MOVE)
      break;
    if (stack_entry->type != DUMMY_MOVE_ENTRY)
      return 0;
  }

  if (stack_entry > (GoMoveStackEntry *) board->move_stack) {
    int last_pass_color = stack_entry->suicide_or_pass_color;

    while (--stack_entry >= (GoMoveStackEntry *) board->move_stack) {
      if (stack_entry->type == PASS_MOVE) {
	return (stack_entry->suicide_or_pass_color
		== OTHER_COLOR (last_pass_color));
      }

      if (stack_entry->type != DUMMY_MOVE_ENTRY)
	break;
    }
  }

  return 0;
}


/* Determine if a move is suicide.  This is rule set independent. */
static int
is_suicide (const Board *board, int color, int pos)
{
  const char *grid = board->grid;
  int k;
  int single_liberty = (grid[pos] == EMPTY ? 1 : 0);

  for (k = 0; k < 4; k++) {
    int neighbor = pos + delta[k];

    if (LIBERTY (grid, neighbor)
	|| (ON_GRID (grid, neighbor)
	    && ((grid[neighbor] == color)
		^ (LIBERTIES (board, neighbor) == single_liberty))))
      return 0;
  }

  return 1;
}


/* Determine if a move is legal according to specified rule set.  This
 * typically involves checks for ko rule violation, disallowed suicide
 * moves etc.
 */
int
go_is_legal_move (const Board *board, BoardRuleSet rule_set,
		  int color, va_list move)
{
  int x = va_arg (move, int);
  int y = va_arg (move, int);
  int pos = POSITION (x, y);

  assert (rule_set < NUM_GO_RULE_SETS);

  if (rule_set == GO_RULE_SET_SGF)
    return 1;

  if (pos != PASS_MOVE) {
    assert (ON_BOARD (board, x, y));

    /* Determine if a move is legal in terms of default rule set.
     * It is illegal to violate the ko rule or play suicides.
     */
    return (board->grid[pos] == EMPTY
	    && (color != OTHER_COLOR (board->data.go.ko_master)
		|| pos != board->data.go.ko_position)
	    && !is_suicide (board, color, pos));
  }

  return 1;
}


void
go_play_move (Board *board, int color, va_list move)
{
  GoMoveStackEntry *stack_entry;
  int x = va_arg (move, int);
  int y = va_arg (move, int);
  int pos = POSITION (x, y);

  if (pos != PASS_MOVE) {
    assert (ON_BOARD (board, x, y));

    if (board->grid[pos] == EMPTY)
      do_play_move (board, color, pos);
    else if (board->grid[pos] == color)
      do_play_over_own_stone (board, pos);
    else
      do_play_over_enemy_stone (board, pos);
  }
  else {
    stack_entry			       = ALLOCATE_GO_MOVE_STACK_ENTRY (board);
    stack_entry->type		       = PASS_MOVE;
    stack_entry->suicide_or_pass_color = color;

    stack_entry->ko_master   = board->data.go.ko_master;
    stack_entry->ko_position = board->data.go.ko_position;
    board->data.go.ko_master = EMPTY;

    stack_entry->move_number = board->move_number++;
  }
}


void
go_apply_changes (Board *board, int num_changes)
{
  GoMoveStackEntry *stack_entry = ALLOCATE_GO_MOVE_STACK_ENTRY (board);

  stack_entry->type	   = POSITION_CHANGE;
  stack_entry->num.changes = num_changes;
  stack_entry->ko_master   = board->data.go.ko_master;
  stack_entry->ko_position = board->data.go.ko_position;
  stack_entry->move_number = board->move_number;

  board->data.go.ko_master = EMPTY;

  rebuild_strings (board);
}


void
go_add_dummy_move_entry (Board *board)
{
  GoMoveStackEntry *stack_entry = ALLOCATE_GO_MOVE_STACK_ENTRY (board);

  /* Ko data is not changed, but go_undo() restores it anyway, so we
   * need to initialize stack's copy of it.
   */
  stack_entry->type	   = DUMMY_MOVE_ENTRY;
  stack_entry->ko_master   = board->data.go.ko_master;
  stack_entry->ko_position = board->data.go.ko_position;
  stack_entry->move_number = board->move_number;
}



/* Rebuild all board strings from scratch.  Used after complex
 * position changes that are not handled incrementally.
 */
static void
rebuild_strings (Board *board)
{
  int pos;
  int string_number = 0;

  for (pos = POSITION (0, 0); ON_GRID (board->grid, pos);
       pos += (BOARD_MAX_WIDTH + 1) - board->width) {
    for (; ON_GRID (board->grid, pos); pos++)
      STRING_NUMBER (board, pos) = -1;
  }

  for (pos = POSITION (0, 0); ON_GRID (board->grid, pos);
       pos += (BOARD_MAX_WIDTH + 1) - board->width) {
    for (; ON_GRID (board->grid, pos); pos++) {
      if (board->grid[pos] != EMPTY && STRING_NUMBER (board, pos) == -1) {
	board->data.go.position_mark++;
	board->data.go.liberties[string_number]
	  = change_string_number (board, pos, string_number);

	string_number++;
      }
    }
  }

  board->data.go.last_string_number = string_number - 1;
  while (string_number < GO_STRING_RING_SIZE)
    board->data.go.liberties[string_number++] = -1;

#if BOARD_VALIDATION_LEVEL > 0
  go_validate_board (board);
#endif
}


/* Do the real job of go_play_move() when the move position is empty
 * (most of the cases).
 */
static void
do_play_move (Board *board, int color, int pos)
{
  char *grid = board->grid;
  int k;
  int direct_liberties = 0;
  int move_is_suicide = 1;
  int allies[4];
  int num_allies = 0;
  int captures[4];
  int num_captures = 0;
  GoMoveStackEntry *stack_entry = ALLOCATE_GO_MOVE_STACK_ENTRY (board);

  stack_entry->type		      = NORMAL_MOVE;
  stack_entry->contents		      = EMPTY;
  stack_entry->position		      = pos;
  stack_entry->ko_master	      = board->data.go.ko_master;
  stack_entry->ko_position	      = board->data.go.ko_position;
  stack_entry->prisoners[BLACK_INDEX] = board->data.go.prisoners[BLACK_INDEX];
  stack_entry->prisoners[WHITE_INDEX] = board->data.go.prisoners[WHITE_INDEX];
  stack_entry->move_number	      = board->move_number++;

  board->data.go.position_mark++;
  board->data.go.string_mark++;

  for (k = 0; k < 4; k++) {
    int neighbor = pos + delta[k];

    stack_entry->status[k] = EMPTY;
    if (LIBERTY (grid, neighbor)) {
      direct_liberties++;
      MARK_POSITION (board, neighbor);
    }
    else if (ON_GRID (grid, neighbor) && UNMARKED_STRING (board, neighbor)) {
      if (grid[neighbor] == color) {
	stack_entry->status[k] = ALLY;
	allies[num_allies++] = neighbor;
	if (LIBERTIES (board, neighbor) > 1)
	  move_is_suicide = 0;
      }
      else {
	if (LIBERTIES (board, neighbor) > 1) {
	  stack_entry->status[k] = OPPONENT;
	  LIBERTIES (board, neighbor)--;
	}
	else {
	  stack_entry->status[k] = CAPTURE;
	  captures[num_captures++] = neighbor;
	  move_is_suicide = 0;
	}
      }

      MARK_STRING (board, neighbor);
    }
  }

  if (!move_is_suicide || direct_liberties > 0) {
    int captured_stones = 0;

    stack_entry->num.liberties = join_strings (board, color, pos,
					       direct_liberties,
					       allies, num_allies);

    for (k = 0; k < num_captures; k++)
      captured_stones += remove_string (board, captures[k]);

    if (captured_stones != 1 || direct_liberties > 0 || num_allies > 0)
      board->data.go.ko_master = EMPTY;
    else {
      board->data.go.ko_master = color;
      board->data.go.ko_position = captures[0];
    }

    board->data.go.prisoners[COLOR_INDEX (color)] += captured_stones;
  }
  else {
    int other = OTHER_COLOR (color);

    stack_entry->suicide_or_pass_color = color;

    board->data.go.ko_master = EMPTY;

    for (k = 0; k < 4; k++) {
      if (stack_entry->status[k] == OPPONENT)
	LIBERTIES (board, pos + delta[k])++;
    }

    board->data.go.prisoners[COLOR_INDEX (other)]++;
    for (k = 0; k < num_allies; k++) {
      board->data.go.prisoners[COLOR_INDEX (other)]
	+= remove_string (board, allies[k]);
    }
  }
}


static void
do_play_over_own_stone (Board *board, int pos)
{
  char *grid = board->grid;
  int k;
  int color = grid[pos];
  int other = OTHER_COLOR (color);
  int captured_stones = 0;
  GoMoveStackEntry *stack_entry = ALLOCATE_GO_MOVE_STACK_ENTRY (board);

  stack_entry->type		      = NORMAL_MOVE;
  stack_entry->contents		      = color;
  stack_entry->position		      = pos;
  stack_entry->num.liberties	      = LIBERTIES (board, pos);
  stack_entry->ko_master	      = board->data.go.ko_master;
  stack_entry->ko_position	      = board->data.go.ko_position;
  stack_entry->prisoners[BLACK_INDEX] = board->data.go.prisoners[BLACK_INDEX];
  stack_entry->prisoners[WHITE_INDEX] = board->data.go.prisoners[WHITE_INDEX];
  stack_entry->move_number	      = board->move_number++;

  for (k = 0; k < 4; k++) {
    int neighbor = pos + delta[k];

    stack_entry->status[k] = EMPTY;
    if (grid[neighbor] == other && LIBERTIES (board, neighbor) == 0) {
      stack_entry->status[k] = CAPTURE;
      captured_stones += remove_string (board, neighbor);
    }
  }

  board->data.go.ko_master = EMPTY;

  if (LIBERTIES (board, pos) > 0)
    board->data.go.prisoners[COLOR_INDEX (color)] += captured_stones;
  else {
    stack_entry->suicide_or_pass_color = color;
    board->data.go.prisoners[COLOR_INDEX (other)] += remove_string (board,
								    pos);
  }
}


static void
do_play_over_enemy_stone (Board *board, int pos)
{
  char *grid = board->grid;
  int other = grid[pos];
  int color = OTHER_COLOR (other);
  int k;
  int string_number = STRING_NUMBER (board, pos);

  grid[pos] = EMPTY;
  board->data.go.string_mark++;

  for (k = 0; k < 4; k++) {
    int neighbor = pos + delta[k];

    if (grid[neighbor] == other) {
      if (STRING_NUMBER (board, neighbor) == string_number) {
	int new_string_number = allocate_string (board);

	board->data.go.position_mark++;
	board->data.go.liberties[new_string_number]
	  = change_string_number (board, neighbor, new_string_number);
      }
    }
    else if (grid[neighbor] == color && UNMARKED_STRING (board, neighbor)) {
      LIBERTIES (board, neighbor)++;
      MARK_STRING (board, neighbor);
    }
  }

  LIBERTIES (board, pos) = -1;

  do_play_move (board, color, pos);
  ((GoMoveStackEntry *) board->move_stack_pointer - 1)->contents = other;

#if BOARD_VALIDATION_LEVEL == 2
  ((GoMoveStackEntry *) board->move_stack_pointer - 1)->grid_copy[pos] = other;
#endif
}


/* Join several strings together and calculate the number of liberties
 * of the resulting string.  The strings to be joined must be found by
 * the caller of this function.  It also actually plays the move given
 * by `color' and `pos' parameters on the board.
 *
 * Return the number of liberties the resulting string has had before.
 * This information is used when undoing moves.
 */
static int
join_strings (Board *board, int color, int pos, int new_liberties,
	      int *allies, int num_allies)
{
  char *grid = board->grid;
  int liberties;
  int string_number;

  if (num_allies == 0) {
    string_number = allocate_string (board);
    liberties = -1;
  }
  else {
    int k;

    string_number = STRING_NUMBER (board, allies[0]);
    liberties = LIBERTIES (board, allies[0]);

    if (num_allies == 1) {
      new_liberties = liberties - 1;
      for (k = 0; k < 4; k++) {
	int neighbor = pos + delta[k];

	if (LIBERTY (grid, neighbor)) {
	  int i;

	  for (i = 0; i < 4; i++) {
	    if (grid[neighbor + delta[i]] == color
		&& STRING_NUMBER (board, neighbor + delta[i]) == string_number)
	      break;
	  }

	  if (i == 4)
	    new_liberties++;
	}
      }
    }
    else {
      MARK_POSITION (board, pos);
      new_liberties += change_string_number (board, allies[0], string_number);
      for (k = 1; k < num_allies; k++) {
	LIBERTIES (board, allies[k]) = -1;
	new_liberties += change_string_number (board, allies[k],
					       string_number);
      }
    }
  }

  grid[pos] = color;
  STRING_NUMBER (board, pos) = string_number;
  board->data.go.liberties[string_number] = new_liberties;

  return liberties;
}


/* Remove the given string from the board and recalculate the number
 * of liberties of each its neighbor.  The number of stones removed is
 * returned.
 */
static int
remove_string (Board *board, int pos)
{
  char *grid = board->grid;
  int color = grid[pos];
  int other = OTHER_COLOR (color);
  int queue[BOARD_MAX_POSITIONS];
  int queue_start = 0;
  int queue_end = 1;

  grid[pos] = EMPTY;
  queue[0] = pos;

  do {
    int k;
    int stone = queue[queue_start++];

    board->data.go.string_mark++;
    for (k = 0; k < 4; k++) {
      int neighbor = stone + delta[k];

      if (grid[neighbor] == other) {
	if (UNMARKED_STRING (board, neighbor)) {
	  LIBERTIES (board, neighbor)++;
	  MARK_STRING (board, neighbor);
	}
      }
      else if (grid[neighbor] == color) {
	grid[neighbor] = EMPTY;
	queue[queue_end++] = neighbor;
      }
    }
  } while (queue_start < queue_end);

  LIBERTIES (board, pos) = -1;

  return queue_end;
}


/* Change the number of given string and return the number of its
 * unmarked liberties.
 */
static int
change_string_number (Board *board, int pos, int string_number)
{
  const char *grid = board->grid;
  int color = grid[pos];
  int queue[BOARD_MAX_POSITIONS];
  int queue_start = 0;
  int queue_end = 1;
  int unmarked_liberties = 0;

  queue[0] = pos;
  MARK_POSITION (board, pos);

  do {
    int stone = queue[queue_start++];

    STRING_NUMBER (board, stone) = string_number;

    /* Profiling shows that this function is by far the most expensive
     * in the whole Go board code.  Therefore, it is the only one that
     * uses loop unrolling for speedup.
     */
    if (UNMARKED_POSITION (board, SOUTH (stone))) {
      if (grid[SOUTH (stone)] == color)
	queue[queue_end++] = SOUTH (stone);
      else if (LIBERTY (grid, SOUTH (stone)))
	unmarked_liberties++;

      MARK_POSITION (board, SOUTH (stone));
    }

    if (UNMARKED_POSITION (board, WEST (stone))) {
      if (grid[WEST (stone)] == color)
	queue[queue_end++] = WEST (stone);
      else if (LIBERTY (grid, WEST (stone)))
	unmarked_liberties++;

      MARK_POSITION (board, WEST (stone));
    }

    if (UNMARKED_POSITION (board, NORTH (stone))) {
      if (grid[NORTH (stone)] == color)
	queue[queue_end++] = NORTH (stone);
      else if (LIBERTY (grid, NORTH (stone)))
	unmarked_liberties++;

      MARK_POSITION (board, NORTH (stone));
    }

    if (UNMARKED_POSITION (board, EAST (stone))) {
      if (grid[EAST (stone)] == color)
	queue[queue_end++] = EAST (stone);
      else if (LIBERTY (grid, EAST (stone)))
	unmarked_liberties++;

      MARK_POSITION (board, EAST (stone));
    }
  } while (queue_start < queue_end);

  return unmarked_liberties;
}


void
go_undo (Board *board)
{
  GoMoveStackEntry *stack_entry = POP_GO_MOVE_STACK_ENTRY (board);

  if (stack_entry->type == NORMAL_MOVE) {
    char *grid = board->grid;
    int k;
    int pos = stack_entry->position;
    int color = grid[pos];

    if (color != EMPTY) {
      int first_ally = 1;
      int single_liberty = (stack_entry->contents != color ? 1 : 0);

      grid[pos] = EMPTY;
      for (k = 0; k < 4; k++) {
	if (stack_entry->status[k] == ALLY) {
	  if (first_ally)
	    first_ally = 0;
	  else {
	    int string_number = allocate_string (board);

	    board->data.go.position_mark++;
	    board->data.go.liberties[string_number]
	      = change_string_number (board, pos + delta[k], string_number);
	  }
	}
	else if (stack_entry->status[k] == OPPONENT)
	  LIBERTIES (board, pos + delta[k])++;
      }

      grid[pos] = OFF_GRID;
      for (k = 0; k < 4; k++) {
	if (stack_entry->status[k] == CAPTURE) {
	  reconstruct_string (board, OTHER_COLOR (color), pos + delta[k],
			      single_liberty);
	}
      }

      LIBERTIES (board, pos) = stack_entry->num.liberties;
      grid[pos] = stack_entry->contents;
    }
    else {
      color = stack_entry->suicide_or_pass_color;
      if (stack_entry->contents != color) {
	grid[pos] = OFF_GRID;
	for (k = 0; k < 4; k++) {
	  if (stack_entry->status[k] == ALLY)
	    reconstruct_string (board, color, pos + delta[k], 1);
	}

	grid[pos] = stack_entry->contents;
      }
      else
	reconstruct_string (board, color, pos, 0);
    }

    if (stack_entry->contents == OTHER_COLOR (color)) {
      int allies[4];
      int num_allies = 0;
      int direct_liberties = 0;

      board->data.go.position_mark++;
      board->data.go.string_mark++;

      for (k = 0; k < 4; k++) {
	int neighbor = pos + delta[k];

	if (LIBERTY (grid, neighbor)) {
	  direct_liberties++;
	  MARK_POSITION (board, neighbor);
	}
	else if (ON_GRID (grid, neighbor)
		 && UNMARKED_STRING (board, neighbor)) {
	  if (grid[neighbor] == color)
	    LIBERTIES (board, neighbor)--;
	  else
	    allies[num_allies++] = neighbor;

	  MARK_STRING (board, neighbor);
	}
      }

      join_strings (board, stack_entry->contents, pos, direct_liberties,
		    allies, num_allies);
    }

    board->data.go.prisoners[BLACK_INDEX]
      = stack_entry->prisoners[BLACK_INDEX];
    board->data.go.prisoners[WHITE_INDEX]
      = stack_entry->prisoners[WHITE_INDEX];
  }
  else if (stack_entry->type == POSITION_CHANGE) {
    board_undo_changes (board, stack_entry->num.changes);
    rebuild_strings (board);
  }

  board->data.go.ko_master   = stack_entry->ko_master;
  board->data.go.ko_position = stack_entry->ko_position;
  board->move_number	     = stack_entry->move_number;
}


/* Reconstruct a string of given color at given position.  It takes
 * all empty intersections linked with `pos' and makes a string in
 * their place.  Liberties of neighbors are properly adjusted.
 */
static void
reconstruct_string (Board *board, int color, int pos, int single_liberty)
{
  char *grid = board->grid;
  int other = OTHER_COLOR (color);
  int string_number = allocate_string (board);
  int queue[BOARD_MAX_POSITIONS];
  int queue_start = 0;
  int queue_end = 1;

  grid[pos] = color;
  STRING_NUMBER (board, pos) = string_number;
  queue[0] = pos;

  do {
    int k;
    int stone = queue[queue_start++];

    board->data.go.string_mark++;
    for (k = 0; k < 4; k++) {
      int neighbor = stone + delta[k];

      if (grid[neighbor] == other) {
	if (UNMARKED_STRING (board, neighbor)) {
	  LIBERTIES (board, neighbor)--;
	  MARK_STRING (board, neighbor);
	}
      }
      else if (LIBERTY (grid, neighbor)) {
	grid[neighbor] = color;
	STRING_NUMBER (board, neighbor) = string_number;
	queue[queue_end++] = neighbor;
      }
    }
  } while (queue_start < queue_end);

  LIBERTIES (board, pos) = single_liberty;
}


/* Allocate a string on the board.  Finds the first free string in the
 * ring and returns its number.
 */
static int
allocate_string (Board *board)
{
  int string_number = board->data.go.last_string_number;

  do {
    if (string_number < GO_STRING_RING_SIZE - 1)
      string_number++;
    else
      string_number = 0;
  } while (board->data.go.liberties[string_number] != -1);

  board->data.go.last_string_number = string_number;
  return string_number;
}


int
go_format_move (int board_width, int board_height,
		char *buffer, va_list move)
{
  int x = va_arg (move, int);
  int y = va_arg (move, int);

  if (!IS_PASS (x, y)) {
    return game_format_point (GAME_GO, board_width, board_height,
			      buffer, x, y);
  }

  strcpy (buffer, "pass");
  return 4;
}


int
go_parse_move (int board_width, int board_height, const char *move_string,
	       int *x, int *y, BoardAbstractMoveData *move_data)
{
  UNUSED (move_data);

  if (strncasecmp (move_string, "pass", 4) != 0) {
    return game_parse_point (GAME_GO, board_width, board_height,
			     move_string, x, y);
  }

  *x = PASS_X;
  *y = PASS_Y;

  return 4;
}


void
go_validate_board (const Board *board)
{
  const char *grid = board->grid;
  int k;
  int x;
  int y;
  int present_strings[GO_STRING_RING_SIZE];
  int liberties[GO_STRING_RING_SIZE];

  memset (present_strings, 0, sizeof present_strings);
  memset (liberties, 0, sizeof liberties);

  for (y = 0; y < board->height; y++) {
    for (x = 0; x < board->width; x++) {
      int pos = POSITION (x, y);

      if (IS_STONE (grid[pos])) {
	present_strings[STRING_NUMBER (board, pos)] = 1;

	for (k = 0; k < 4; k++) {
	  if (grid[pos + delta[k]] == grid[pos]) {
	    assert (STRING_NUMBER (board, pos + delta[k])
		    == STRING_NUMBER (board, pos));
	  }
	}
      }
      else {
	int num_neighbor_strings = 0;
	int neighbor_strings[4];

	assert (grid[pos] == EMPTY);

	for (k = 0; k < 4; k++) {
	  int neighbor = pos + delta[k];

	  if (IS_STONE (grid[neighbor])) {
	    int i;
	    int string = STRING_NUMBER (board, neighbor);

	    for (i = 0; i < num_neighbor_strings; i++) {
	      if (string == neighbor_strings[i])
		break;
	    }

	    if (i == num_neighbor_strings) {
	      liberties[string]++;
	      neighbor_strings[num_neighbor_strings++] = string;
	    }
	  }
	}
      }
    }
  }

  for (k = 0; k < GO_STRING_RING_SIZE; k++) {
    if (present_strings[k])
      assert (board->data.go.liberties[k] == liberties[k]);
    else
      assert (board->data.go.liberties[k] == -1);
  }
}


void
go_dump_board (const Board *board)
{
  static const char contents[NUM_VALID_BOARD_VALUES]
    = {'.', '@', 'O', '?', '?'};
  static const char coordinates[] =
    " A B C D E F G H J K L M N O P Q R S T U V W X Y Z";

  int x;
  int y;

  fprintf (stderr, "   %.*s\n", board->width * 2, coordinates);

  for (y = 0; y < board->height; y++) {
    fprintf (stderr, "%3d", board->height - y);

    for (x = 0; x < board->width; x++)
      fprintf (stderr, " %c", contents[(int) board->grid[POSITION (x, y)]]);

    fprintf (stderr, board->height > 9 ? "%3d\n" : "%2d\n", board->height - y);
  }

  fprintf (stderr, "   %.*s\n", board->width * 2, coordinates);
}



/* Go-specific functions. */

/* Get maximal number of fixed handicap stones that can be placed on a
 * board of given dimensions.
 */
int
go_get_max_fixed_handicap (int board_width, int board_height)
{
  assert (BOARD_MIN_WIDTH <= board_width
	  && board_width <= BOARD_MAX_WIDTH);
  assert (BOARD_MIN_HEIGHT <= board_height
	  && board_height <= BOARD_MAX_HEIGHT);

  if (board_width > 7 && board_height > 7) {
    return ((board_width % 2 == 1 && board_width > 7 ? 3 : 2)
	    * (board_height % 2 == 1 && board_height > 7 ? 3 : 2));
  }
  else
    return 0;
}


/* Get positions of given number of fixed handicap stones. */
BoardPositionList *
go_get_fixed_handicap_stones (int board_width, int board_height,
			      int num_stones)
{
  /* Minimal handicap at which nth stone (counting by ascending board
   * position) is set.
   */
  static const int min_handicaps[9] = { 3, 8, 2, 6, 5, 6, 2, 8, 4 };

  BoardPositionList *handicap_stones;
  int horizontal_edge_gap = (board_width >= 13 ? 3 : 2);
  int vertical_edge_gap = (board_height >= 13 ? 3 : 2);
  int stone_index;
  int k;

  if (num_stones == 0)
    return NULL;

  assert (num_stones > 1
	  && num_stones <= go_get_max_fixed_handicap (board_width,
						      board_height));

  handicap_stones = board_position_list_new_empty (num_stones);

  for (k = 0, stone_index = 0; k < 9; k++) {
    /* There is an additional requirement that the fifth (tengen)
     * stone is only placed when the number of handicap stones is odd.
     */
    if (num_stones >= min_handicaps[k] && (k != 4 || num_stones % 2 == 1)) {
      /* A little of obscure arithmetics that works. */
      int stone_x = ((1 - (k % 3)) * horizontal_edge_gap
		     + ((k % 3) * (board_width - 1)) / 2);
      int stone_y = ((1 - (k / 3)) * vertical_edge_gap
		     + ((k / 3) * (board_height - 1)) / 2);

      handicap_stones->positions[stone_index++] = POSITION (stone_x, stone_y);
    }
  }

  return handicap_stones;
}


/* Get the hoshi points for given board size.  Up to 9 points can be
 * filled, so the caller should simply statically allocate an
 * appropriate array.  The number of hoshi points is returned.
 *
 * The `hoshi_points' array is not sorted in any particular order.
 */
int
go_get_hoshi_points (int board_width, int board_height,
		     BoardPoint hoshi_points[9])
{
  assert (BOARD_MIN_WIDTH <= board_width
	  && board_width <= BOARD_MAX_WIDTH);
  assert (BOARD_MIN_HEIGHT <= board_height
	  && board_height <= BOARD_MAX_HEIGHT);
  assert (hoshi_points);

  if (board_width >= 5 && board_width != 6
      && board_height >= 5 && board_height != 6) {
    int edge_distance_x = (board_width > 11 ? 3 : 2);
    int edge_distance_y = (board_height > 11 ? 3 : 2);
    int num_hoshi_points;

    /* Four hoshi points in corners. */

    hoshi_points[0].x = edge_distance_x;
    hoshi_points[0].y = edge_distance_y;

    hoshi_points[1].x = board_width - 1 - edge_distance_x;
    hoshi_points[1].y = edge_distance_y;

    hoshi_points[2].x = edge_distance_x;
    hoshi_points[2].y = board_height - 1 - edge_distance_y;

    hoshi_points[3].x = board_width - 1 - edge_distance_x;
    hoshi_points[3].y = board_height - 1 - edge_distance_y;

    if (board_width % 2 == 1 && board_height % 2 == 1) {
      /* The tengen. */
      hoshi_points[4].x = board_width / 2;
      hoshi_points[4].y = board_height / 2;

      num_hoshi_points = 5;
    }
    else
      num_hoshi_points = 4;

    if (board_width % 2 == 1 && board_width >= 13) {
      /* The top hoshi. */
      hoshi_points[num_hoshi_points].x = board_width / 2;
      hoshi_points[num_hoshi_points++].y = edge_distance_y;

      /* The bottom hoshi. */
      hoshi_points[num_hoshi_points].x = board_width / 2;
      hoshi_points[num_hoshi_points++].y = board_height - 1 - edge_distance_y;
    }

    if (board_height % 2 == 1 && board_height >= 13) {
      /* The left hoshi. */
      hoshi_points[num_hoshi_points].x = edge_distance_x;
      hoshi_points[num_hoshi_points++].y = board_height / 2;

      /* The right hoshi. */
      hoshi_points[num_hoshi_points].x = board_width - 1 - edge_distance_x;
      hoshi_points[num_hoshi_points++].y = board_height / 2;
    }

    return num_hoshi_points;
  }
  else
    return 0;
}


BoardPositionList *
go_get_string_stones (Board *board, int x, int y)
{
  int pos = POSITION (x, y);

  assert (board);
  assert (board->game == GAME_GO);
  assert (ON_BOARD (board, x, y));

  if (IS_STONE (board->grid[pos])) {
    BoardPositionList *position_list;
    const char *grid = board->grid;
    int color = grid[pos];
    int stones[BOARD_MAX_POSITIONS];
    int queue_start = 0;
    int queue_end = 1;

    board->data.go.position_mark++;

    stones[0] = pos;
    MARK_POSITION (board, pos);

    do {
      int k;
      int stone = stones[queue_start++];

      for (k = 0; k < 4; k++) {
	int neighbor = stone + delta[k];

	if (grid[neighbor] == color && UNMARKED_POSITION (board, neighbor)) {
	  stones[queue_end++] = neighbor;
	  MARK_POSITION (board, neighbor);
	}
      }
    } while (queue_start < queue_end);

    position_list = board_position_list_new (stones, queue_end);
    board_position_list_sort (position_list);

    return position_list;
  }
  else
    return NULL;
}


/* Find all stones that shold be logically dead if the stone at given
 * position is dead and the game is finished.  At present, all strings
 * that are connectable over empty vertecies are included (i.e. a
 * player should have territory around opponent's dead stones).  This
 * policy may need refinement.
 */
BoardPositionList *
go_get_logically_dead_stones (Board *board, int x, int y)
{
  int pos = POSITION (x, y);

  assert (board);
  assert (board->game == GAME_GO);
  assert (ON_BOARD (board, x, y));

  if (IS_STONE (board->grid[POSITION (x, y)])) {
    BoardPositionList *position_list;
    const char *grid = board->grid;
    int color = grid[pos];
    int stones[BOARD_MAX_POSITIONS];
    int empty_vertices[BOARD_MAX_POSITIONS];
    int stones_queue_start = 0;
    int stones_queue_end = 1;
    int empty_vertices_queue_start = 0;
    int empty_vertices_queue_end = 0;

    board->data.go.position_mark++;

    stones[0] = pos;
    MARK_POSITION (board, pos);

    do {
      int k;

      pos = (stones_queue_start < stones_queue_end
	     ? stones[stones_queue_start++]
	     : empty_vertices[empty_vertices_queue_start++]);

      for (k = 0; k < 4; k++) {
	int neighbor = pos + delta[k];

	if ((grid[neighbor] == color || grid[neighbor] == EMPTY)
	    && UNMARKED_POSITION (board, neighbor)) {
	  if (grid[neighbor] == color)
	    stones[stones_queue_end++] = neighbor;
	  else
	    empty_vertices[empty_vertices_queue_end++] = neighbor;

	  MARK_POSITION (board, neighbor);
	}
      }
    } while (stones_queue_start < stones_queue_end
	     || empty_vertices_queue_start < empty_vertices_queue_end);

    position_list = board_position_list_new (stones, stones_queue_end);
    board_position_list_sort (position_list);

    return position_list;
  }
  else
    return NULL;
}


void
go_score_game (Board *board, const char *dead_stones, double komi,
	       double *score, char **detailed_score,
	       BoardPositionList **black_territory,
	       BoardPositionList **white_territory)
{
  char territory[BOARD_GRID_SIZE];
  int num_territory_positions[NUM_COLORS] = { 0, 0 };
  int territory_positions[NUM_COLORS][BOARD_MAX_POSITIONS];
  int num_prisoners[NUM_COLORS];
  int x;
  int y;
  int pos;
  int black_score;
  double white_score;

  assert (board);

  num_prisoners[BLACK_INDEX] = board->data.go.prisoners[BLACK_INDEX];
  num_prisoners[WHITE_INDEX] = board->data.go.prisoners[WHITE_INDEX];

  board_fill_grid (board, territory, EMPTY);
  go_mark_territory_on_grid (board, territory, dead_stones, BLACK, WHITE);

  for (y = 0, pos = POSITION (0, 0); y < board->height; y++) {
    for (x = 0; x < board->width; x++, pos++) {
      if (territory[pos] != EMPTY) {
	int color_index = COLOR_INDEX (territory[pos]);

	territory_positions[color_index]
			   [num_territory_positions[color_index]++] = pos;
	if (board->grid[pos] != EMPTY)
	  num_prisoners[color_index]++;
      }
    }

    pos += BOARD_MAX_WIDTH + 1 - board->width;
  }

  black_score = (num_territory_positions[BLACK_INDEX]
		 + num_prisoners[BLACK_INDEX]);
  white_score = (num_territory_positions[WHITE_INDEX]
		 + num_prisoners[WHITE_INDEX] + komi);
  if (score)
    *score = black_score - white_score;

  if (detailed_score) {
    *detailed_score
      = utils_printf (("White: %d territory + %d capture(s) %c %.*f komi"
		       " = %.*f\n"
		       "Black: %d territory + %d capture(s) = %.1f\n\n"),
		      num_territory_positions[WHITE_INDEX],
		      num_prisoners[WHITE_INDEX],
		      (komi >= 0.0 ? '+' : '-'),
		      ((int) floor (komi * 100.0 + 0.5) % 10 == 0 ? 1 : 2),
		      fabs (komi),
		      ((int) floor (white_score * 100.0 + 0.5) % 10 == 0
		       ? 1 : 2),
		      white_score,
		      num_territory_positions[BLACK_INDEX],
		      num_prisoners[BLACK_INDEX],
		      (double) black_score);

    if ((double) black_score != white_score) {
      char *string_to_free = *detailed_score;

      *detailed_score
	= utils_printf ("%s%s wins by %.*f", *detailed_score,
			((double) black_score > white_score
			 ? "Black" : "White"),
			((int) floor ((black_score - white_score) * 100.0
				      + 0.5)
			 % 10 == 0 ? 1 : 2),
			fabs (black_score - white_score));
      utils_free (string_to_free);
    }
    else
      *detailed_score = utils_cat_string (*detailed_score, "The game is draw");
  }

  if (black_territory) {
    *black_territory
      = board_position_list_new (territory_positions[BLACK_INDEX],
				 num_territory_positions[BLACK_INDEX]);
  }

  if (white_territory) {
    *white_territory
      = board_position_list_new (territory_positions[WHITE_INDEX],
				 num_territory_positions[WHITE_INDEX]);
  }
}


/* Mark territory on given grid with specified marks.  The grid must
 * be reset to whatever value the caller wants before calling this
 * function.  go_mark_territory_on_grid() will not overwrite grid
 * values' at dame and stone positions.
 *
 * Grid `dead_stones' should have non-zeros in positions of dead
 * stones and zeros elsewhere.  It is assumed that `dead_stones' are
 * filled sanely, i.e all stones of any string are either dead or
 * alive.
 *
 * FIXME: Improve this function.  It doesn't detect sekis.
 */
void
go_mark_territory_on_grid (Board *board, char *grid, const char *dead_stones,
			   char black_territory_mark,
			   char white_territory_mark)
{
  const char *board_grid = board->grid;
  int x;
  int y;
  int pos;
  char territory[BOARD_GRID_SIZE];
  char false_eyes[BOARD_GRID_SIZE];
  int queue[BOARD_MAX_POSITIONS];

  assert (board);
  assert (board->game == GAME_GO);
  assert (grid);
  assert (dead_stones);

  /* First simply look for regions bounded by alive stones of only one
   * color and claim they are territory.  Also look for false eyes.
   */

  board_fill_grid (board, territory, EMPTY);
  board_fill_grid (board, false_eyes, 0);

  board->data.go.position_mark++;

  for (y = 0, pos = POSITION (0, 0); y < board->height; y++) {
    for (x = 0; x < board->width; x++, pos++) {
      if (UNMARKED_POSITION (board, pos)
	  && (board_grid[pos] == EMPTY || dead_stones[pos])) {
	/* Found another connected set of empty vertices and/or dead
	 * stones.  Loop over it marking positions and determine if it
	 * is a proper territory, i.e. if it has adjacent living
	 * stones of only one color.
	 */
	int queue_start = 0;
	int queue_end = 1;
	char alive_neighbors = 0;

	queue[0] = pos;
	MARK_POSITION (board, pos);

	do {
	  int k;
	  int pos2 = queue[queue_start++];

	  for (k = 0; k < 4; k++) {
	    int neighbor = pos2 + delta[k];

	    if ((board_grid[neighbor] == EMPTY
		 || (IS_STONE (board_grid[neighbor])
		     && dead_stones[neighbor]))
		&& UNMARKED_POSITION (board, neighbor)) {
	      queue[queue_end++] = neighbor;
	      MARK_POSITION (board, neighbor);
	    }
	    else if (IS_STONE (board_grid[neighbor]) && !dead_stones[neighbor])
	      alive_neighbors |= board_grid[neighbor];
	  }
	} while (queue_start < queue_end);

	if (IS_STONE (alive_neighbors)) {
	  for (queue_start = 0; queue_start < queue_end; queue_start++) {
	    int k;
	    int pos2 = queue[queue_start];
	    int other = OTHER_COLOR (alive_neighbors);
	    int diagonal_score = 0;

	    territory[pos2] = alive_neighbors;

	    /* Does this position look like a false eye? */
	    for (k = 4; k < 8; k++) {
	      if (board_grid[pos2 + delta[k]] == other
		  && !dead_stones[pos2 + delta[k]])
		diagonal_score += 2;
	      else if (!ON_GRID (board_grid, pos2 + delta[k]))
		diagonal_score++;
	    }

	    if (diagonal_score >= 4)
	      false_eyes[pos2] = 1;
	  }
	}
      }
    }

    pos += BOARD_MAX_WIDTH + 1 - board->width;
  }

  /* Determine which false eyes don't yield territory points and erase
   * territory under them.
   */

  do {
  restart_false_eye_checking:
    board->data.go.position_mark++;

    for (y = 0, pos = POSITION (0, 0); y < board->height; y++) {
      for (x = 0; x < board->width; x++, pos++) {
	if (UNMARKED_POSITION (board, pos)
	    && board_grid[pos] != EMPTY
	    && !dead_stones[pos]) {
	  int color = board_grid[pos];
	  int queue_start = 0;
	  int queue_end = 1;
	  int affected_false_eye = NULL_POSITION;
	  int not_connected_to_an_eye = 1;

	  queue[0] = pos;
	  MARK_POSITION (board, pos);

	  do {
	    int k;
	    int pos2 = queue[queue_start++];

	    for (k = 0; k < 4; k++) {
	      int neighbor = pos2 + delta[k];

	      if (ON_GRID (board_grid, neighbor)) {
		if ((board_grid[neighbor] == color
		     || ((board_grid[neighbor] == EMPTY
			  || dead_stones[neighbor])
			 && !false_eyes[neighbor]
			 && (territory[neighbor] != EMPTY
			     || (board_grid[neighbor] == EMPTY
				 && board_grid[pos2] == color))))
		    && UNMARKED_POSITION (board, neighbor)) {
		  queue[queue_end++] = neighbor;
		  MARK_POSITION (board, neighbor);

		  if (territory[neighbor] != EMPTY)
		    not_connected_to_an_eye = 0;
		}
		else if (false_eyes[neighbor]
			 && territory[neighbor] != EMPTY) {
		  if (affected_false_eye != NULL_POSITION
		      && neighbor != affected_false_eye)
		    not_connected_to_an_eye = 0;

		  affected_false_eye = neighbor;
		}
	      }
	    }
	  } while (queue_start < queue_end);

	  if (not_connected_to_an_eye && affected_false_eye != NULL_POSITION) {
	    territory[affected_false_eye] = EMPTY;
	    goto restart_false_eye_checking;
	  }
	}
      }

      pos += BOARD_MAX_WIDTH + 1 - board->width;
    }
  } while (0);

  /* Finally, mark the territory on the supplied grid. */
  for (y = 0, pos = POSITION (0, 0); y < board->height; y++) {
    for (x = 0; x < board->width; x++, pos++) {
      if (territory[pos] != EMPTY) {
	grid[pos] = (territory[pos] == BLACK
		     ? black_territory_mark : white_territory_mark);
      }
    }

    pos += BOARD_MAX_WIDTH + 1 - board->width;
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
