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
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,     *
 * Boston, MA 02110-1301, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include "amazons.h"
#include "board-internals.h"

#include <stdio.h>
#include <assert.h>


static inline int  amazon_has_any_legal_move
		     (const char grid[BOARD_FULL_GRID_SIZE],
		      int pos);


/* Just handle weird situations when there are no amazons of either
 * color on board.  Even if a player has no legal move, he is still to
 * play according to Amazons rules (and thus loses the game).
 */
int
amazons_adjust_color_to_play (const Board *board, BoardRuleSet rule_set,
			      int color)
{
  int pos;
  int have_other_color = 0;

  UNUSED (rule_set);

  for (pos = POSITION (0, 0); ON_GRID (board->grid, pos);
       pos += (BOARD_MAX_WIDTH + 1) - board->width) {
    for (; ON_GRID (board->grid, pos); pos++) {
      if (board->grid[pos] == color)
	return color;
      else if (board->grid[pos] == OTHER_COLOR (color))
	have_other_color = 1;
    }
  }

  return have_other_color ? OTHER_COLOR (color) : EMPTY;
}


/* Determine if Amazons game is over, i.e. if there is no legal move
 * for `color_to_play'.
 */
int
amazons_is_game_over (const Board *board, BoardRuleSet rule_set,
		      int color_to_play)
{
  int pos;

  assert (rule_set < NUM_AMAZONS_RULE_SETS);

  for (pos = POSITION (0, 0); ON_GRID (board->grid, pos);
       pos += (BOARD_MAX_WIDTH + 1) - board->width) {
    for (; ON_GRID (board->grid, pos); pos++) {
      if (board->grid[pos] == color_to_play
	  && amazon_has_any_legal_move (board->grid, pos))
	return 0;
    }
  }

  return 1;
}


/* Get the default setup for Amazons. */
int
amazons_get_default_setup (int width, int height,
			   BoardPositionList **black_stones,
			   BoardPositionList **white_stones)
{
  int x1 = (width - 1) / 3;
  int x2 = (width - 1) - x1;
  int y1 = (height - 1) / 3;
  int y2 = (height - 1) - y1;

  *black_stones = board_position_list_new_empty (4);
  (*black_stones)->positions[0] = POSITION (x1, 0);
  (*black_stones)->positions[1] = POSITION (x2, 0);
  (*black_stones)->positions[2] = POSITION (0, y1);
  (*black_stones)->positions[3] = POSITION (width - 1, y1);

  *white_stones = board_position_list_new_empty (4);
  (*white_stones)->positions[0] = POSITION (0, y2);
  (*white_stones)->positions[1] = POSITION (width - 1, y2);
  (*white_stones)->positions[2] = POSITION (x1, height - 1);
  (*white_stones)->positions[3] = POSITION (x2, height - 1);

  return 1;
}


/* Determine if a move is legal according to specified rule set.  If
 * "to" point is a null point, determine if there is any legal move
 * for the amazon at "from".  Likewise, if the "shoot-arrow-to" point
 * is a null point, determine if there is any legal move involving
 * given "from" and "to" points (there is always at least one).
 */
int
amazons_is_legal_move (const Board *board, BoardRuleSet rule_set,
		       int color, va_list move)
{
  const char *grid = board->grid;
  int pos;
  int to_x;
  int to_y;
  int to_pos;
  BoardAmazonsMoveData move_data;
  int from_pos;
  int shoot_arrow_to_pos;
  int direction;

  assert (rule_set < NUM_AMAZONS_RULE_SETS);

  to_x	    = va_arg (move, int);
  to_y	    = va_arg (move, int);
  move_data = va_arg (move, BoardAmazonsMoveData);

  assert (ON_BOARD (board, move_data.from.x, move_data.from.y));

  from_pos = POINT_TO_POSITION (move_data.from);
  if (grid[from_pos] != color)
    return 0;

  if (rule_set == AMAZONS_RULE_SET_SGF)
    return 1;

  if (IS_NULL_POINT (to_x, to_y))
    return amazon_has_any_legal_move (grid, from_pos);

  assert (ON_BOARD (board, to_x, to_y));

  direction = determine_position_delta (to_x - move_data.from.x,
					to_y - move_data.from.y);
  if (!direction)
    return 0;

  to_pos = POSITION (to_x, to_y);
  for (pos = from_pos; pos != to_pos; ) {
    pos += direction;
    if (grid[pos] != EMPTY)
      return 0;
  }

  if (IS_NULL_POINT (move_data.shoot_arrow_to.x, move_data.shoot_arrow_to.y))
    return 1;

  assert (ON_BOARD (board,
		    move_data.shoot_arrow_to.x, move_data.shoot_arrow_to.y));

  direction = determine_position_delta (move_data.shoot_arrow_to.x - to_x,
					move_data.shoot_arrow_to.y - to_y);
  if (!direction)
    return 0;

  shoot_arrow_to_pos = POINT_TO_POSITION (move_data.shoot_arrow_to);
  do {
    pos += direction;
    if (grid[pos] != EMPTY && pos != from_pos)
      return 0;
  } while (pos != shoot_arrow_to_pos);

  return 1;
}


static inline int
amazon_has_any_legal_move (const char grid[BOARD_FULL_GRID_SIZE], int pos)
{
  int k;

  for (k = 0; k < 8; k++) {
    if (grid[pos + delta[k]] == EMPTY)
      return 1;
  }

  return 0;
}


void
amazons_play_move (Board *board, int color, va_list move)
{
  char *grid = board->grid;
  int to_x;
  int to_y;
  AmazonsMoveStackEntry *stack_entry
    = ALLOCATE_AMAZONS_MOVE_STACK_ENTRY (board);
  BoardAmazonsMoveData move_data;

  to_x	    = va_arg (move, int);
  to_y	    = va_arg (move, int);
  move_data = va_arg (move, BoardAmazonsMoveData);

  assert (ON_BOARD (board, move_data.from.x, move_data.from.y));
  assert (ON_BOARD (board, to_x, to_y));
  assert (ON_BOARD (board,
		    move_data.shoot_arrow_to.x, move_data.shoot_arrow_to.y));

  assert (grid[POINT_TO_POSITION (move_data.from)] == color);

  stack_entry->from		   = POINT_TO_POSITION (move_data.from);
  stack_entry->to		   = POSITION (to_x, to_y);
  stack_entry->misc.shoot_arrow_to
    = POINT_TO_POSITION (move_data.shoot_arrow_to);

  stack_entry->to_contents = grid[stack_entry->to];
  stack_entry->shoot_arrow_to_contents
    = grid[stack_entry->misc.shoot_arrow_to];

  grid[stack_entry->from]		 = EMPTY;
  grid[stack_entry->to]			 = color;
  grid[stack_entry->misc.shoot_arrow_to] = ARROW;

  stack_entry->move_number = board->move_number++;
}


void
amazons_undo (Board *board)
{
  AmazonsMoveStackEntry *stack_entry = POP_AMAZONS_MOVE_STACK_ENTRY (board);

  if (stack_entry->from != NULL_POSITION) {
    board->grid[stack_entry->from]	     = board->grid[stack_entry->to];
    board->grid[stack_entry->to]	     = stack_entry->to_contents;
    board->grid[stack_entry->misc.shoot_arrow_to]
      = stack_entry->shoot_arrow_to_contents;
  }
  else if (stack_entry->misc.num_changes > 0)
    board_undo_changes (board, stack_entry->misc.num_changes);

  board->move_number = stack_entry->move_number;
}


void
amazons_apply_changes (Board *board, int num_changes)
{
  AmazonsMoveStackEntry *stack_entry
    = ALLOCATE_AMAZONS_MOVE_STACK_ENTRY (board);

  stack_entry->from		= NULL_POSITION;
  stack_entry->misc.num_changes = num_changes;
  stack_entry->move_number	= board->move_number;
}


void
amazons_add_dummy_move_entry (Board *board)
{
  amazons_apply_changes (board, 0);
}


void
amazons_validate_board (const Board *board)
{
  UNUSED (board);
}


void
amazons_dump_board (const Board *board)
{
  static const char contents[5] = {'.', '@', 'O', '=', '?'};
  static const char coordinates[] =
    " A B C D E F G H I J K L M N O P Q R S T U V W X Y";

  int x;
  int y;

  fprintf (stderr, "   %.*s\n", board->width * 2, coordinates);

  for (y = 0; y < board->height; y++) {
    fprintf (stderr, "%3d", board->height - y);

    for (x = 0; x < board->width; x++)
      fprintf (stderr, " %c", contents[(int) board->grid[POSITION (x, y)]]);

    fprintf (stderr, "%3d\n", board->height - y);
  }

  fprintf (stderr, "   %.*s\n", board->width * 2, coordinates);
}


int
amazons_format_move (int board_width, int board_height,
		     char *buffer, va_list move)
{
  int x = va_arg (move, int);
  int y = va_arg (move, int);
  BoardAmazonsMoveData move_data = va_arg (move, BoardAmazonsMoveData);
  char *pointer = buffer;

  pointer += game_format_point (GAME_AMAZONS, board_width, board_height,
				pointer, move_data.from.x, move_data.from.y);
  *pointer++ = '-';
  pointer += game_format_point (GAME_AMAZONS, board_width, board_height,
				pointer, x, y);
  *pointer++ = '-';
  pointer += game_format_point (GAME_AMAZONS, board_width, board_height,
				pointer,
				move_data.shoot_arrow_to.x,
				move_data.shoot_arrow_to.y);

  return pointer - buffer;
}


/* Parse an Amazons move.  Quarry understands three commonly used (and
 * similar to each other) formats: A1-B2-C3, A1-B2/C3 and A1-B2xC3.
 * This is of course only half of the work, as when speaking GTP, the
 * other side has to understand us too (we speak the first vartiant.)
 */
int
amazons_parse_move (int board_width, int board_height, const char *move_string,
		    int *x, int *y, BoardAbstractMoveData *move_data)
{
  int x_temp;
  int y_temp;
  int num_characters_eaten;

  assert (move_data);

  num_characters_eaten = game_parse_point (GAME_AMAZONS,
					   board_width, board_height,
					   move_string, &x_temp, &y_temp);
  if (num_characters_eaten > 0 && move_string[num_characters_eaten] == '-') {
    const char *pointer = move_string + num_characters_eaten + 1;

    move_data->amazons.from.x = x_temp;
    move_data->amazons.from.y = y_temp;

    num_characters_eaten = game_parse_point (GAME_AMAZONS,
					     board_width, board_height,
					     pointer, x, y);
    if (num_characters_eaten > 0
	&& (pointer[num_characters_eaten] == '-'
	    || pointer[num_characters_eaten] == '/'
	    || pointer[num_characters_eaten] == 'x')) {
      pointer += num_characters_eaten + 1;

      num_characters_eaten = game_parse_point (GAME_AMAZONS,
					       board_width, board_height,
					       pointer, &x_temp, &y_temp);
      if (num_characters_eaten) {
	move_data->amazons.shoot_arrow_to.x = x_temp;
	move_data->amazons.shoot_arrow_to.y = y_temp;

	return (pointer - move_string) + num_characters_eaten;
      }
    }
  }

  return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
