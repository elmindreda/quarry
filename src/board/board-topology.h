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


#ifndef QUARRY_BOARD_TOPOLOGY_H
#define QUARRY_BOARD_TOPOLOGY_H


#include "quarry.h"


#define BOARD_MIN_WIDTH		5
#define BOARD_MIN_HEIGHT	5
#define BOARD_MAX_WIDTH		25
#define BOARD_MAX_HEIGHT	25

#define POSITION(x, y)		((1 + (y)) * (1 + BOARD_MAX_WIDTH) + (1 + (x)))
#define POINT_TO_POSITION(point)		\
  POSITION((point).x, (point).y)

#define POSITION_X(pos)		(((pos) % (1 + BOARD_MAX_WIDTH)) - 1)
#define POSITION_Y(pos)		(((pos) / (1 + BOARD_MAX_WIDTH)) - 1)

#define SOUTH(pos)		((pos) + (1 + BOARD_MAX_WIDTH))
#define WEST(pos)		((pos) - 1)
#define NORTH(pos)		((pos) - (1 + BOARD_MAX_WIDTH))
#define EAST(pos)		((pos) + 1)

#define BOARD_MAX_POSITIONS	(BOARD_MAX_WIDTH * BOARD_MAX_HEIGHT)
#define BOARD_GRID_SIZE		(POSITION(BOARD_MAX_WIDTH - 1,		\
					  BOARD_MAX_HEIGHT - 1) + 1)
#define BOARD_FULL_GRID_SIZE	(POSITION(BOARD_MAX_WIDTH,		\
					  BOARD_MAX_HEIGHT) + 1)

#define ON_SIZED_GRID(width, height, x, y)		\
  ((unsigned int) (x) < (unsigned int) (width)		\
   && (unsigned int) (y) < (unsigned int) (height))
#define ON_BOARD(board, x, y)					\
  ON_SIZED_GRID((board)->width, (board)->height, (x), (y))

#define NULL_X			(-1)
#define NULL_Y			(-1)
#define NULL_POSITION		POSITION(NULL_X, NULL_Y)
#define IS_NULL_POINT(x, y)	((x) == NULL_X && (y) == NULL_Y)

/* These values has no meaning for the board code, but they cannot be
 * mistook for normal coordinates.  Higher-level modules can treat
 * them in a special way.
 */
#define SPECIAL_X		(-2)
#define SPECIAL_Y		(-2)
#define SPECIAL_POSITION	POSITION(SPECIAL_X, SPECIAL_Y)


#endif /* QUARRY_BOARD_TOPOLOGY_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
