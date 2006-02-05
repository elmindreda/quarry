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


#ifndef QUARRY_AMAZONS_H
#define QUARRY_AMAZONS_H


#include "board-internals.h"
#include "quarry.h"

#include <stdarg.h>


#define ALLOCATE_AMAZONS_MOVE_STACK_ENTRY(board)		\
  ALLOCATE_MOVE_STACK_ENTRY ((board), AmazonsMoveStackEntry)

#define POP_AMAZONS_MOVE_STACK_ENTRY(board)			\
  POP_MOVE_STACK_ENTRY ((board), AmazonsMoveStackEntry)


typedef struct _AmazonsMoveStackEntry	AmazonsMoveStackEntry;

struct _AmazonsMoveStackEntry {
  BoardStackEntry  common;
  char		   to_contents;
  char		   shoot_arrow_to_contents;
  int		   from;
  int		   to;

  union {
    int		   shoot_arrow_to;
    int		   num_changes;
  } misc;
};


int		amazons_adjust_color_to_play (const Board *board,
					      BoardRuleSet rule_set,
					      int color);
int		amazons_is_game_over (const Board *board,
				      BoardRuleSet rule_set,
				      int color_to_play);

int		amazons_get_default_setup (int width, int height,
					   BoardPositionList **black_stones,
					   BoardPositionList **white_stones);

int		amazons_is_legal_move (const Board *board,
				       BoardRuleSet rule_set,
				       int color, va_list move);

void		amazons_play_move (Board *board, int color, va_list move);
void		amazons_undo (Board *board);

void		amazons_apply_changes (Board *board, int num_changes);
void		amazons_add_dummy_move_entry (Board *board);

int		amazons_format_move (int board_width, int board_height,
				     char *buffer, va_list move);
int		amazons_parse_move (int board_width, int board_height,
				    const char *move_string,
				    int *x, int *y,
				    BoardAbstractMoveData *move_data);

void		amazons_validate_board (const Board *board);
void		amazons_dump_board (const Board *board);


#endif /* QUARRY_AMAZONS_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
