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


#include "gui-utils.h"
#include "board.h"

#include <assert.h>


void
gui_utils_mark_variations_on_grid(char grid[BOARD_GRID_SIZE],
				  const Board *board,
				  int black_variations[BOARD_GRID_SIZE],
				  int white_variations[BOARD_GRID_SIZE],
				  char black_variations_mark,
				  char white_variations_mark,
				  char mixed_variations_mark)
{
  int x;
  int y;

  assert(grid);
  assert(board);
  assert(black_variations);
  assert(white_variations);

  for (y = 0; y < board->height; y++) {
    for (x = 0; x < board->width; x++) {
      int pos = POSITION(x, y);

      if (board->grid[pos] == EMPTY) {
	if (black_variations[pos] > 0) {
	  if (white_variations[pos] > 0)
	    grid[pos] = mixed_variations_mark;
	  else
	    grid[pos] = black_variations_mark;
	}
	else {
	  if (white_variations[pos] > 0)
	    grid[pos] = white_variations_mark;
	}
      }
    }
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
