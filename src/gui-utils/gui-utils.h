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


#ifndef QUARRY_GUI_UTILS_H
#define QUARRY_GUI_UTILS_H


#include "board.h"
#include "quarry.h"


int		gui_utils_enumerate_themes(void);
void		gui_utils_discard_theme_lists(void);


void		gui_utils_mark_variations_on_grid
		  (char grid[BOARD_GRID_SIZE], const Board *board,
		   int black_variations[BOARD_GRID_SIZE],
		   int white_variations[BOARD_GRID_SIZE],
		   char black_variations_mark, char white_variations_mark,
		   char mixed_variations_mark);


#endif /* QUARRY_GUI_UTILS_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
