/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003 Paul Pogonyshev.                             *
 * Copyright (C) 2004 Paul Pogonyshev and Martin Holters.          *
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


#include "board-internals.h"
#include "game-info.h"
#include "utils.h"

#include <assert.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


#define CHANGE_STACK_SIZE_INCREMENT	BOARD_MAX_POSITIONS


static void	clear_board_grid(Board *board);

static void	ensure_change_stack_space(Board *board, int num_entries);


const int delta[8] = {
  SOUTH(0),
  WEST(0),
  NORTH(0),
  EAST(0),
  SOUTH(WEST(0)),
  NORTH(WEST(0)),
  NORTH(EAST(0)),
  SOUTH(EAST(0))
};


/* Dynamically allocate a Board structure for specified game and with
 * specified dimensions.  The board is cleared.
 */
Board *
board_new(Game game, int width, int height)
{
  Board *board = utils_malloc(sizeof(Board));
  int move_stack_bytes
    = (((int) (width * height * game_info[game].relative_num_moves_per_game))
       * game_info[game].stack_entry_size);

  assert(game >= FIRST_GAME && GAME_IS_SUPPORTED(game));
  assert(BOARD_MIN_WIDTH <= width && width <= BOARD_MAX_WIDTH);
  assert(BOARD_MIN_HEIGHT <= height && height <= BOARD_MAX_HEIGHT);

  board->game   = game;
  board->width  = width;
  board->height = height;

  board->is_legal_move = game_info[game].is_legal_move;
  board->play_move     = game_info[game].play_move;
  board->undo	       = game_info[game].undo;

  clear_board_grid(board);
  if (game_info[game].reset_game_data)
    game_info[game].reset_game_data(board, 1);

  board->move_number = 0;

  board->move_stack = utils_malloc(move_stack_bytes);
  board->move_stack_pointer = board->move_stack;
  board->move_stack_end = (char *) board->move_stack + move_stack_bytes;

  board->change_stack = utils_malloc(width * height
				     * sizeof(BoardChangeStackEntry));
  board->change_stack_pointer = board->change_stack;
  board->change_stack_end = (board->change_stack + width * height);

  return board;
}


/* Free a previously allocated Board structure and its stacks. */
void
board_delete(Board *board)
{
  assert(board);

  utils_free(board->move_stack);
  utils_free(board->change_stack);
  utils_free(board);
}


/* Duplicate given board, but don't copy the stacks.  The stacks of
 * the copy will be empty.
 */
Board *
board_duplicate_without_stacks(const Board *board)
{
  Board *board_copy;
  int x;
  int y;
  int pos;

  assert(board);

  board_copy = board_new(board->game, board->width, board->height);

  board_copy->move_number = board->move_number;

  for (y = 0, pos = POSITION(0, 0); y < board->height; y++) {
    for (x = 0; x < board->width; x++, pos++)
      board_copy->grid[pos] = board->grid[pos];
    pos += BOARD_MAX_WIDTH + 1 - board->width;
  }

  if (board->game == GAME_GO)
    memcpy(&board_copy->data.go, &board->data.go, sizeof(GoBoardData));

  return board_copy;
}


/* Set board dimensions to specified values and clear the board as
 * needed.  This may include clearing board's grid, resetting
 * game-specific data (e.g. ko state for Go) and reallocating board
 * stacks (to save memory).  Board stacks will be empty regardless of
 * whether they were reallocated.
 */
void
board_set_parameters(Board *board, Game game, int width, int height)
{
  int move_stack_bytes
    = (((int) (width * height * game_info[game].relative_num_moves_per_game))
       * game_info[game].stack_entry_size);
  int need_full_reset = 1;

  assert(board);
  assert(game >= FIRST_GAME && GAME_IS_SUPPORTED(game));
  assert(BOARD_MIN_WIDTH <= width && width <= BOARD_MAX_WIDTH);
  assert(BOARD_MIN_HEIGHT <= height && height <= BOARD_MAX_HEIGHT);

  if (board->width != width || board->height != height
      || board->move_stack_pointer != board->move_stack) {
    board->width  = width;
    board->height = height;
    clear_board_grid(board);
  }
  else if (board->game != game) {
    board->game = game;
    board->is_legal_move = game_info[game].is_legal_move;
    board->play_move     = game_info[game].play_move;
    board->undo		 = game_info[game].undo;
  }
  else
    need_full_reset = 0;

  if (game_info[game].reset_game_data)
    game_info[game].reset_game_data(board, need_full_reset);

  board->move_number = 0;

  if ((char *) board->move_stack_end - (char *) board->move_stack
      != move_stack_bytes) {
    board->move_stack = utils_realloc(board->move_stack, move_stack_bytes);
    board->move_stack_end = ((char *) board->move_stack + move_stack_bytes);
  }

  board->move_stack_pointer = board->move_stack;

  if (board->change_stack_end - board->change_stack != width * height) {
    board->change_stack = utils_realloc(board->change_stack,
					(width * height
					 * sizeof(BoardChangeStackEntry)));
    board->change_stack_end = board->change_stack + width * height;
  }

  board->change_stack_pointer = board->change_stack;
}


static void
clear_board_grid(Board *board)
{
  char *grid;
  int x;
  int y;
  int pos;

  grid = board->grid;

  pos = 0;
  for (x = -1; x <= board->width; x++)
    grid[pos++] = OFF_GRID;

  for (y = 0; y < board->height; y++) {
    pos += BOARD_MAX_WIDTH - 1 - board->width;

    grid[pos++] = OFF_GRID;
    for (x = 0; x < board->width; x++)
      grid[pos++] = EMPTY;
    grid[pos++] = OFF_GRID;
  }

  pos += BOARD_MAX_WIDTH - 1 - board->width;
  for (x = -1; x <= board->width; x++)
    grid[pos++] = OFF_GRID;
}



/* Normally color to play is alternating.  However, in some games
 * (i.e. Othello) there are situations where one of the players has to
 * skip his move.  Also, changing board state by adding/removing
 * pieces can lead to situation where one of the players doesn't have
 * a legal move.  Then it's logical to let the other to take his turn.
 * Finally, if neither of the player has a legal move, this function
 * returns EMPTY.
 *
 * Note that pass is a legal move in Go, so this function just returns
 * `color' if the game is Go.
 */
int
board_adjust_color_to_play(const Board *board, BoardRuleSet rule_set,
			   int color)
{
  assert(board);
  assert(rule_set >= FIRST_RULE_SET);

  if (!IS_STONE(color)) {
    assert(color == EMPTY);
    color = game_info[board->game].color_to_play_first;
  }

  if (game_info[board->game].adjust_color_to_play) {
    color = game_info[board->game].adjust_color_to_play(board, rule_set,
							color);
  }

  return color;
}


/* Determine if the game played on a given board is over. Parameter
 * `color_to_play' must be adjusted with board_adjust_color_to_play().
 */
int
board_is_game_over(const Board *board, BoardRuleSet rule_set,
		   int color_to_play)
{
  assert(board);
  assert(rule_set >= FIRST_RULE_SET);

  if (color_to_play == EMPTY)
    return 1;

  assert(IS_STONE(color_to_play));

  if (game_info[board->game].is_game_over == NULL)
    return 0;

  return game_info[board->game].is_game_over(board, rule_set, color_to_play);
}


/* Determine if a move is legal according to specified rules. */
inline int
board_is_legal_move(const Board *board, BoardRuleSet rule_set, int color, ...)
{
  va_list move;
  int result;

  assert(board);
  assert(rule_set >= FIRST_RULE_SET);
  assert(IS_STONE(color));

  va_start(move, color);
  result = board->is_legal_move(board, rule_set, color, move);
  va_end(move);

  return result;
}


/* Play the specified move on the given board. */
inline void
board_play_move(Board *board, int color, ...)
{
  va_list move;

  assert(board);
  assert(IS_STONE(color));

#if BOARD_VALIDATION_LEVEL == 2
  if (board->move_stack_pointer == board->move_stack_end)
    board_increase_move_stack_size(board);

  memcpy(board->move_stack_pointer, board->grid,
	 BOARD_GRID_SIZE * sizeof(char));
#endif

  va_start(move, color);
  board->play_move(board, color, move);
  va_end(move);

#if BOARD_VALIDATION_LEVEL > 0
  board_validate(board);
#endif
}


/* Apply changes (adding/removing of stones) on board.  Unused change
 * list pointers can be set to NULL.
 */
void
board_apply_changes(Board *board,
		    const BoardPositionList *const
		      change_lists[NUM_ON_GRID_VALUES])
{
  int color;
  int num_changes;

  assert(board);
  assert(!change_lists[SPECIAL_ON_GRID_VALUE] ||board->game == GAME_AMAZONS);

#if BOARD_VALIDATION_LEVEL == 2
  if (board->move_stack_pointer == board->move_stack_end)
    board_increase_move_stack_size(board);

  memcpy(board->move_stack_pointer, board->grid,
	 BOARD_GRID_SIZE * sizeof(char));
#endif

  num_changes = 0;
  for (color = 0; color < NUM_ON_GRID_VALUES; color++) {
    if (change_lists[color])
      num_changes += change_lists[color]->num_positions;
  }

  assert(num_changes > 0);

  ensure_change_stack_space(board, num_changes);

  for (color = 0; color < NUM_ON_GRID_VALUES; color++) {
    if (change_lists[color]) {
      int k;

      for (k = 0; k < change_lists[color]->num_positions; k++) {
	int pos = change_lists[color]->positions[k];

	board->change_stack_pointer->position = pos;
	board->change_stack_pointer->contents = board->grid[pos];
	board->change_stack_pointer++;

	board->grid[pos] = color;
      }
    }
  }

  game_info[board->game].apply_changes(board, num_changes);

#if BOARD_VALIDATION_LEVEL > 0
  board_validate(board);
#endif
}


/* Add a dummy stack entry that can be removed later with
 * board_undo().  This can serve two purposes: not caring about
 * whether board_undo() should be called and providing a simple way of
 * tracking unexpectedly changing move number (i.e. with MN[] SGF
 * property that doesn't correspond a real move).
 */
inline void
board_add_dummy_move_entry(Board *board)
{
  assert(board);

#if BOARD_VALIDATION_LEVEL == 2
  if (board->move_stack_pointer == board->move_stack_end)
    board_increase_move_stack_size(board);

  memcpy(board->move_stack_pointer, board->grid,
	 BOARD_GRID_SIZE * sizeof(char));
#endif

  game_info[board->game].add_dummy_move_entry(board);
}


/* Undo a number of previously played moves and/or sets of board
 * changes.  If not moves or changes are in the stack yet, return
 * zero.
 */
inline int
board_undo(Board *board, int num_undos)
{
  int k;

  assert(board);

  assert(((char *) board->move_stack_pointer
	  - (num_undos * game_info[board->game].stack_entry_size))
	 >= (char *) board->move_stack);

  for (k = 0; k < num_undos; k++)
    board->undo(board);

#if BOARD_VALIDATION_LEVEL > 0
  board_validate(board);
#endif

#if BOARD_VALIDATION_LEVEL == 2
  {
    const char * grid_copy = (const char *) board->move_stack_pointer;
    int x;
    int y;

    for (y = 0; y < board->height; y++) {
      for (x = 0; x < board->width; x++)
	assert(board->grid[POSITION(x, y)] == grid_copy[POSITION(x, y)]);
    }
  }
#endif

  return 1;
}


inline void
board_undo_changes(Board *board, int num_changes)
{
  int k;

  for (k = 0; k < num_changes; k++) {
    board->change_stack_pointer--;
    board->grid[board->change_stack_pointer->position]
      = board->change_stack_pointer->contents;
  }
}



/* Allocate an entry on stack.  The duty of the function is to
 * reallocate the stack if there is no more space in it.  It also
 * saves boards' grid on heap if heavy board debugging is on.
 */
void
board_increase_move_stack_size(Board *board)
{
  int stack_bytes_increment
    = (((int) (board->width * board->height
	       * 0.5 * game_info[board->game].relative_num_moves_per_game))
       * game_info[board->game].stack_entry_size);
  int new_stack_bytes = (((char *) board->move_stack_end
			  - (char *) board->move_stack)
			 + stack_bytes_increment);

  board->move_stack = utils_realloc(board->move_stack, new_stack_bytes);
  board->move_stack_end = (char *) board->move_stack + new_stack_bytes;
  board->move_stack_pointer = ((char *) board->move_stack_end
			       - stack_bytes_increment);
}


static void
ensure_change_stack_space(Board *board, int num_entries)
{
  if (board->change_stack_pointer + num_entries > board->change_stack_end) {
    int size_increment = ((1 + (num_entries - 1) / CHANGE_STACK_SIZE_INCREMENT)
			  * CHANGE_STACK_SIZE_INCREMENT);
    int new_stack_size = (board->change_stack_end - board->change_stack
			  + size_increment);

    board->change_stack
      = utils_realloc(board->change_stack,
		      new_stack_size * sizeof(BoardChangeStackEntry));
    board->change_stack_end = board->change_stack + new_stack_size;
    board->change_stack_pointer = board->change_stack_end - size_increment;
  }
}


int
determine_position_delta(int delta_x, int delta_y)
{
  if (delta_x > 0) {
    if (delta_y == delta_x)
      return SOUTH(EAST(0));
    else if (delta_y == -delta_x)
      return NORTH(EAST(0));
    else if (delta_y == 0)
      return EAST(0);
  }
  else if (delta_x < 0) {
    if (delta_y == delta_x)
      return NORTH(WEST(0));
    else if (delta_y == -delta_x)
      return SOUTH(WEST(0));
    else if (delta_y == 0)
      return WEST(0);
  }
  else {
    if (delta_y > 0)
      return SOUTH(0);
    else if (delta_y < 0)
      return NORTH(0);
  }

  return 0;
}



/* Dump the contents of given board's grid to stderr. */
inline void
board_dump(const Board *board)
{
  assert(board);

  game_info[board->game].dump_board(board);
}


inline void
board_validate(const Board *board)
{
  assert(board);

  game_info[board->game].validate_board(board);
}



BoardPositionList *
board_position_list_new(const int *positions, int num_positions)
{
  BoardPositionList *list;

  assert(0 <= num_positions && num_positions < BOARD_MAX_POSITIONS);

  list = utils_malloc(sizeof(BoardPositionList)
		      - (BOARD_MAX_POSITIONS - num_positions) * sizeof(int));
  list->num_positions = num_positions;
  memcpy(list->positions, positions, num_positions * sizeof(int));

  return list;
}


BoardPositionList *
board_position_list_new_empty(int num_positions)
{
  BoardPositionList *list;

  assert(0 <= num_positions && num_positions < BOARD_MAX_POSITIONS);

  list = utils_malloc(sizeof(BoardPositionList)
		      - (BOARD_MAX_POSITIONS - num_positions) * sizeof(int));
  list->num_positions = num_positions;

  return list;
}


void
board_position_list_sort(BoardPositionList *list)
{
  assert(list);

  if (list->num_positions > 1) {
    qsort(list->positions, list->num_positions, sizeof(int),
	  utils_compare_ints);
  }
}


/* Position lists are always stored sorted in ascending order of
 * positions.  This is essential in the following functions and also
 * for SGF writer.
 */

BoardPositionList* 
board_position_list_union(BoardPositionList *list1, BoardPositionList *list2)
{
  int i = 0;
  int j = 0;
  int k = 0;
  BoardPositionList *dest;

  assert(list1);
  assert(list2);

  dest = board_position_list_new_empty(list1->num_positions 
				       + list2->num_positions);

  while (i < list1->num_positions || j < list2->num_positions) {
    if (i >= list1->num_positions 
	|| list1->positions[i] > list2->positions[j])
      dest->positions[k++] = list2->positions[j++];
    else if (j >= list2->num_positions 
	     || list1->positions[i] < list2->positions[j])
      dest->positions[k++] = list1->positions[i++];
    else {
      /* Same element in both source lists. */
      dest->positions[k++] = list1->positions[i++];
      j++;
    }
  }

  dest->num_positions = k;

  return utils_realloc(dest, (sizeof(BoardPositionList)
			      - (BOARD_MAX_POSITIONS - k) * sizeof(int)));

}


/* Determine if two position lists are equal, i.e. contain identical
 * positions.
 */
int
board_position_lists_are_equal(const BoardPositionList *first_list,
			       const BoardPositionList *second_list)
{
  int k;

  assert(first_list);
  assert(second_list);

  if (first_list->num_positions != second_list->num_positions)
    return 0;

  for (k = 0; k < first_list->num_positions; k++) {
    if (first_list->positions[k] != second_list->positions[k])
      return 0;
  }

  return 1;
}


/* Determine if two position have at least one common position. */
int
board_position_lists_overlap(const BoardPositionList *first_list,
			     const BoardPositionList *second_list)
{
  int i;
  int j;

  assert(first_list);
  assert(second_list);

  for (i = 0, j = 0;
       i < first_list->num_positions && j < second_list->num_positions;) {
    if (first_list->positions[i] < second_list->positions[j])
      i++;
    else if (first_list->positions[i] > second_list->positions[j])
      j++;
    else
      return 1;
  }

  return 0;
}


/* Find given position in list.  If position is found, return its
 * index or -1 otherwise.  The index may be useful for locating
 * associated data in array.
 */
int
board_position_list_find_position(const BoardPositionList *list, int pos)
{
  assert(list);

  if (list->num_positions > 1) {
    int *position_pointer = bsearch(&pos, list->positions, list->num_positions,
				    sizeof(int), utils_compare_ints);

    return position_pointer ? position_pointer - list->positions : -1;
  }

  return list->num_positions == 1 && list->positions[0] == pos ? 0 : -1;
}



void
board_position_list_mark_on_grid(const BoardPositionList *list,
				 char grid[BOARD_GRID_SIZE], char value)
{
  int k;

  assert(list);
  assert(grid);

  for (k = 0; k < list->num_positions; k++)
    grid[list->positions[k]] = value;
}



int
game_format_point(Game game, int board_width, int board_height,
		  char *buffer, int x, int y)
{
  assert(game >= FIRST_GAME && GAME_IS_SUPPORTED(game));
  assert(BOARD_MIN_WIDTH <= board_width && board_width <= BOARD_MAX_WIDTH);
  assert(BOARD_MIN_HEIGHT <= board_height && board_height <= BOARD_MAX_HEIGHT);
  assert(buffer);
  assert(ON_SIZED_GRID(board_width, board_height, x, y));

  *buffer = game_info[game].horizontal_coordinates[x];
  return (1 + utils_ncprintf(buffer + 1, 3, "%d",
			     (game_info[game].reversed_vertical_coordinates
			      ? board_height - y : y + 1)));
}


int
game_format_position_list(Game game, int board_width, int board_height,
			  char *buffer, const BoardPositionList *position_list)
{
  char *buffer_pointer = buffer;
  int k;

  assert(position_list);

  for (k = 0; k < position_list->num_positions; k++) {
    int pos = position_list->positions[k];

    buffer_pointer += game_format_point(game, board_width, board_height,
					buffer_pointer,
					POSITION_X(pos), POSITION_Y(pos));
    *buffer_pointer++ = ' ';
  }

  *--buffer_pointer = 0;
  return buffer_pointer - buffer;
}


int
game_format_move(Game game, int board_width, int board_height,
		 char *buffer, ...)
{
  va_list move;
  int num_characters;

  assert(game >= FIRST_GAME && GAME_IS_SUPPORTED(game));

  va_start(move, buffer);
  num_characters = game_info[game].format_move(board_width, board_height,
					       buffer, move);
  va_end(move);

  buffer[num_characters] = 0;
  return num_characters;
}


int
game_format_move_valist(Game game, int board_width, int board_height,
			char *buffer, va_list move)
{
  int num_characters;

  assert(game >= FIRST_GAME && GAME_IS_SUPPORTED(game));

  num_characters = game_info[game].format_move(board_width, board_height,
					       buffer, move);
  buffer[num_characters] = 0;

  return num_characters;
}


int
game_parse_point(Game game, int board_width, int board_height,
		 const char *point_string, int *x, int *y)
{
  int x_temp;
  char x_normalized;

  assert(game >= FIRST_GAME && GAME_IS_SUPPORTED(game));
  assert(BOARD_MIN_WIDTH <= board_width && board_width <= BOARD_MAX_WIDTH);
  assert(BOARD_MIN_HEIGHT <= board_height && board_height <= BOARD_MAX_HEIGHT);
  assert(point_string);
  assert(x);
  assert(y);

  if ('A' <= *point_string && *point_string < 'Z')
    x_temp = *point_string - 'A';
  else if ('a' <= *point_string && *point_string < 'z')
    x_temp = *point_string - 'a';
  else
    return 0;

  x_normalized = game_info[game].horizontal_coordinates[0] + x_temp;
  if (x_temp < board_width
      && game_info[game].horizontal_coordinates[x_temp] == x_normalized)
    *x = x_temp;
  else if (x_temp - 1 < board_width
	   && (game_info[game].horizontal_coordinates[x_temp - 1]
	       == x_normalized))
    *x = x_temp - 1;
  else
    return 0;

  if ('1' <= *(point_string + 1) && *(point_string + 1) <= '9') {
    int num_characters_eaten;

    sscanf(point_string + 1, "%d%n", y, &num_characters_eaten);
    if (*y <= board_height) {
      if (game_info[game].reversed_vertical_coordinates)
	*y = board_height - *y;
      else
	(*y)--;

      return 1 + num_characters_eaten;
    }      
  }

  return 0;
}


BoardPositionList *
game_parse_position_list(Game game, int board_width, int board_height,
			 const char *positions_string)
{
  assert(positions_string);

  if (*positions_string) {
    BoardPositionList *position_list;
    int num_positions = 0;
    char present_positions[BOARD_GRID_SIZE];
    int x;
    int y;
    int k;
    int pos;

    for (y = 0, pos = POSITION(0, 0); y < board_height; y++) {
      for (x = 0; x < board_width; x++, pos++)
	present_positions[pos] = 0;
      pos += BOARD_MAX_WIDTH + 1 - board_width;
    }

    do {
      int num_characters_eaten = game_parse_point(game,
						  board_width, board_height,
						  positions_string, &x, &y);

      if (!num_characters_eaten)
	return NULL;

      pos = POSITION(x, y);
      if (present_positions[pos])
	return NULL;

      present_positions[pos] = 1;
      num_positions++;

      positions_string += num_characters_eaten;
      if (*positions_string == ' ')
	positions_string++;
    } while (*positions_string);

    position_list = board_position_list_new_empty(num_positions);
    for (y = 0, pos = POSITION(0, 0), k = 0; k < num_positions; y++) {
      for (x = 0; x < board_width; x++, pos++) {
	if (present_positions[pos])
	  position_list->positions[k++] = pos;
      }

      pos += BOARD_MAX_WIDTH + 1 - board_width;
    }

    return position_list;
  }

  return NULL;
}


int
game_parse_move(Game game, int board_width, int board_height,
		const char *move_string,
		int *x, int *y, BoardAbstractMoveData *move_data)
{
  assert(game >= FIRST_GAME && GAME_IS_SUPPORTED(game));
  assert(BOARD_MIN_WIDTH <= board_width && board_width <= BOARD_MAX_WIDTH);
  assert(BOARD_MIN_HEIGHT <= board_height && board_height <= BOARD_MAX_HEIGHT);
  assert(move_string);
  assert(x);
  assert(y);

  return game_info[game].parse_move(board_width, board_height, move_string,
				    x, y, move_data);
}


/* Determine default setup for the given game and board dimensions.
 * If the default setup is not empty, nonzero is returned and zero
 * otherwise.
 */
inline int
game_get_default_setup(Game game, int width, int height,
		       BoardPositionList **black_stones,
		       BoardPositionList **white_stones)
{
  assert(game >= FIRST_GAME && GAME_IS_SUPPORTED(game));
  assert(BOARD_MIN_WIDTH <= width && width <= BOARD_MAX_WIDTH);
  assert(BOARD_MIN_HEIGHT <= height && height <= BOARD_MAX_HEIGHT);
  assert(black_stones);
  assert(white_stones);

  return (game_info[game].get_default_setup
	  && game_info[game].get_default_setup(width, height,
					       black_stones, white_stones));
}



#define DO_FILL_GRID(grid, width, height, value)			\
  do {									\
    int x;								\
    int y;								\
    int pos;								\
    assert(grid);							\
    assert(BOARD_MIN_WIDTH <= (width)\
	   && (width) <= BOARD_MAX_WIDTH);				\
    assert(BOARD_MIN_HEIGHT <= (height)					\
	   && (height) <= BOARD_MAX_HEIGHT);				\
    for (y = 0, pos = POSITION(0, 0); y < (height); y++) {		\
      for (x = 0; x < (width); x++, pos++)				\
	(grid) [pos] = (value);						\
      pos += BOARD_MAX_WIDTH + 1 - (width);				\
    }									\
  } while (0)


void
grid_fill(char grid[BOARD_GRID_SIZE], int width, int height, char value)
{
  DO_FILL_GRID(grid, width, height, value);
}


void
int_grid_fill(int grid[BOARD_GRID_SIZE], int width, int height, int value)
{
  DO_FILL_GRID(grid, width, height, value);
}


void
pointer_grid_fill(void *grid[BOARD_GRID_SIZE], int width, int height,
		  void *value)
{
  DO_FILL_GRID(grid, width, height, value);
}


#define DO_COPY_GRID(destination, source, type, width, height)		\
  do {									\
    int pos;								\
    assert(destination);						\
    assert(source);							\
    assert(BOARD_MIN_WIDTH <= (width)					\
	   && (width) <= BOARD_MAX_WIDTH);				\
    assert(BOARD_MIN_HEIGHT <= (height)					\
	   && (height) <= BOARD_MAX_HEIGHT);				\
    for (pos = POSITION(0, 0); pos < POSITION(0, (height));		\
	 pos = SOUTH(pos)) {						\
      memcpy((destination) + pos, (source) + pos,			\
	     (width) * sizeof(type));					\
    }									\
  } while (0)


void
grid_copy(char destination[BOARD_GRID_SIZE],
	  const char source[BOARD_GRID_SIZE],
	  int width, int height)
{
  DO_COPY_GRID((destination), (source), char, (width), (height));
}


void
int_grid_copy(int destination[BOARD_GRID_SIZE],
	      const int source[BOARD_GRID_SIZE],
	      int width, int height)
{
  DO_COPY_GRID((destination), (source), int, (width), (height));
}


void
pointer_grid_copy(void *destination[BOARD_GRID_SIZE],
		  const void *source[BOARD_GRID_SIZE],
		  int width, int height)
{
  DO_COPY_GRID((destination), (source), const void *, (width), (height));
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
