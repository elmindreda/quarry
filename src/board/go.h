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


#ifndef QUARRY_GO_H
#define QUARRY_GO_H


#include "board-internals.h"
#include "quarry.h"

#include <stdarg.h>


#define ALLOCATE_GO_MOVE_STACK_ENTRY(board)		\
  ALLOCATE_MOVE_STACK_ENTRY ((board), GoMoveStackEntry)

#define POP_GO_MOVE_STACK_ENTRY(board)			\
  POP_MOVE_STACK_ENTRY ((board), GoMoveStackEntry)


typedef struct _GoMoveStackEntry	GoMoveStackEntry;

struct _GoMoveStackEntry {
  BoardStackEntry  common;

  char		   type;
  char		   contents;
  char		   suicide_or_pass_color;
  int		   position;
  char		   status[4];

  union {
    int		   liberties;
    int		   changes;
  } num;

  int		   ko_master;
  int		   ko_position;
  int		   prisoners[NUM_COLORS];
};


void		go_reset_game_data (Board *board, int forced_reset);

int		go_is_game_over (const Board *board, BoardRuleSet rule_set,
				 int color_to_play);

int		go_is_legal_move (const Board *board, BoardRuleSet rule_set,
				  int color, va_list move);

void		go_play_move (Board *board, int color, va_list move);
void		go_undo (Board *board);

void		go_apply_changes (Board *board, int num_changes);
void		go_add_dummy_move_entry (Board *board);

int		go_format_move (int board_width, int board_height,
				char *buffer, va_list move);
int		go_parse_move (int board_width, int board_height,
			       const char *move_string,
			       int *x, int *y,
			       BoardAbstractMoveData *move_data);

void		go_validate_board (const Board *board);
void		go_dump_board (const Board *board);


#endif /* QUARRY_GO_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
