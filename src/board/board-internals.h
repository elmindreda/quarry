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


#ifndef QUARRY_BOARD_INTERNALS_H
#define QUARRY_BOARD_INTERNALS_H


#include "board.h"
#include "quarry.h"


/* When set to 1, the board is validated after each change (playing a
 * move, adding/removing stones or undo).  This includes validating of
 * grid contents (i.e. only stones or "empty" values are allowed on
 * grid for Go), consistency of internal data (for Go: string numbers
 * and number of liberties for each string).
 *
 * When set to 2, it is additionally verified that board_undo() does
 * indeed revert grid to previous state and not something else.
 *
 * Zero means that no automated validation is ever done.
 */
#define BOARD_VALIDATION_LEVEL	0


#define SOUTH(pos)		((pos) + (1 + BOARD_MAX_WIDTH))
#define WEST(pos)		((pos) - 1)
#define NORTH(pos)		((pos) - (1 + BOARD_MAX_WIDTH))
#define EAST(pos)		((pos) + 1)

#define ON_GRID(grid, pos)	((grid) [pos] != OFF_GRID)


/* Cast expressions are not allowed as lvalues by ISO C and may be
 * frowned upon by strict compilers, hence the tricks below.  Must be
 * optimized away in any case.
 */

#define ALLOCATE_MOVE_STACK_ENTRY(board, MoveStackEntryType)		\
  (((board)->move_stack_pointer == (board)->move_stack_end		\
    ? board_increase_move_stack_size (board)				\
    : (void) 0),							\
   (board)->move_stack_pointer						\
     = (MoveStackEntryType *) (board)->move_stack_pointer + 1,		\
   (MoveStackEntryType *) (board)->move_stack_pointer - 1)

#define POP_MOVE_STACK_ENTRY(board, MoveStackEntryType)			\
  ((MoveStackEntryType *)						\
   ((board)->move_stack_pointer						\
    = (MoveStackEntryType *) (board)->move_stack_pointer - 1))


typedef struct _BoardStackEntry		BoardStackEntry;

struct _BoardStackEntry {
#if BOARD_VALIDATION_LEVEL == 2
  /* Causes problems with very deep branches, since stack is allocated
   * in one memory chunk.
   */
  char		grid_copy[BOARD_GRID_SIZE];
#endif

  int		move_number;
};


struct _BoardChangeStackEntry {
  short		position;
  char		contents;
};


inline void	board_undo_changes (Board *board, int num_undos);

int		determine_position_delta (int delta_x, int delta_y);

void		board_increase_move_stack_size (Board *board);


extern const int	delta[8];


#endif /* QUARRY_BOARD_INTERNALS_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
