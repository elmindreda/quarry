/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005, 2006 Paul Pogonyshev.           *
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


#ifndef QUARRY_GAME_INFO_H
#define QUARRY_GAME_INFO_H


#include "games.h"
#include "board-internals.h"
#include "utils.h"
#include "quarry.h"


#define GAME_IS_SUPPORTED(game)			\
  ((game) <= LAST_GAME && game_info[game].default_board_size != 0)


typedef struct _GameInfo	GameInfo;

struct _GameInfo {
  const char			*name;

  int				 default_board_size;
  const char			*standard_board_sizes;

  int				 color_to_play_first;

  int (* adjust_color_to_play)  (const Board *board, BoardRuleSet rule_set,
				 int color);
  int (* is_game_over)		(const Board *board, BoardRuleSet rule_set,
				 int color_to_play);

  const char			*horizontal_coordinates;
  int				 reversed_vertical_coordinates;

  int (* get_default_setup)	(int width, int height,
				 BoardPositionList **black_stones,
				 BoardPositionList **white_stones);

  void (* reset_game_data)	(Board *board, int forced_reset);

  BoardIsLegalMoveFunction	 is_legal_move;
  BoardPlayMoveFunction		 play_move;
  BoardUndoFunction		 undo;

  void (* apply_changes)	(Board *board, int num_changes);
  void (* add_dummy_move_entry)	(Board *board);

  void (* format_move)		(int board_width, int board_height,
				 StringBuffer *buffer, va_list move);
  int (* parse_move)		(int board_width, int board_height,
				 const char *move_string,
				 int *x, int *y,
				 BoardAbstractMoveData *move_data);

  void (* validate_board)	(const Board *board);
  void (* dump_board)		(const Board *board);

  int				 stack_entry_size;
  float				 relative_num_moves_per_game;
};


extern const GameInfo  game_info[];


#endif /* QUARRY_GAME_INFO_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
