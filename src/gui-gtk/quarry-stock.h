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


#ifndef QUARRY_QUARRY_STOCK_H
#define QUARRY_QUARRY_STOCK_H


#include "quarry.h"

#include <gtk/gtk.h>


/* Control Center buttons. */
#define QUARRY_STOCK_NEW_GAME		"quarry-new-game"
#define QUARRY_STOCK_OPEN_GAME_RECORD	"quarry-open-game-record"

/* Different assistant buttons. */
#define QUARRY_STOCK_NEXT		"quarry-next"
#define QUARRY_STOCK_PLAY		"quarry-play"

/* Other buttons. */
#define QUARRY_STOCK_MOVE_UP		"quarry-move-up"
#define QUARRY_STOCK_MOVE_DOWN		"quarry-move-down"
#define QUARRY_STOCK_BROWSE		"quarry-browse"
#define QUARRY_STOCK_OVERWRITE		"quarry-overwrite"


void		quarry_stock_init(void);


#endif /* QUARRY_QUARRY_STOCK_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
