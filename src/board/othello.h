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


#ifndef QUARRY_OTHELLO_H
#define QUARRY_OTHELLO_H


#include "board-internals.h"
#include "quarry.h"

#include <stdarg.h>


#define ALLOCATE_OTHELLO_MOVE_STACK_ENTRY(board)		\
  ALLOCATE_MOVE_STACK_ENTRY((board), OthelloMoveStackEntry)

#define POP_OTHELLO_MOVE_STACK_ENTRY(board)		\
  POP_MOVE_STACK_ENTRY((board), OthelloMoveStackEntry)


typedef struct _OthelloMoveStackEntry	OthelloMoveStackEntry;

struct _OthelloMoveStackEntry {
#if BOARD_VALIDATION_LEVEL == 2
  /* Causes problems with very deep branches, since stack is allocated
   * in one memory chunk.
   */
  char		grid_copy[BOARD_GRID_SIZE];
#endif

  int		position;
  char		contents;

  union {
    char	flips[8];
    int		changes;
  } num;

  int		move_number;
};


int		othello_adjust_color_to_play(const Board *board,
					     BoardRuleSet rule_set, int color);
int		othello_is_game_over(const Board *board, BoardRuleSet rule_set,
				     int color_to_play);

int		othello_get_default_setup(int width, int height,
					  BoardPositionList **black_stones,
					  BoardPositionList **white_stones);

int		othello_is_legal_move(const Board *board,
				      BoardRuleSet rule_set,
				      int color, va_list move);

void		othello_play_move(Board *board, int color, va_list move);
void		othello_undo(Board *board);

void		othello_apply_changes(Board *board, int num_changes);
void		othello_add_dummy_move_entry(Board *board);

int		othello_format_move(int board_width, int board_height,
				    char *buffer, va_list move);
int		othello_parse_move(int board_width, int board_height,
				   const char *move_string,
				   int *x, int *y,
				   BoardAbstractMoveData *move_data);

void		othello_validate_board(const Board *board);
void		othello_dump_board(const Board *board);


#endif /* QUARRY_OTHELLO_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
