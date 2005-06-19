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


#ifndef QUARRY_QUARRY_STOCK_H
#define QUARRY_QUARRY_STOCK_H


#include "quarry.h"

#include <gtk/gtk.h>


/* Control Center buttons. */
#define QUARRY_STOCK_NEW_GAME		"quarry-new-game"
#define QUARRY_STOCK_OPEN_GAME_RECORD	"quarry-open-game-record"

/* Other buttons. */
#define QUARRY_STOCK_BROWSE		"quarry-browse"
#define QUARRY_STOCK_CREATE		"quarry-create"
#define QUARRY_STOCK_DONE		"quarry-done"
#define QUARRY_STOCK_MODIFY		"quarry-modify"
#define QUARRY_STOCK_MOVE_UP		"quarry-move-up"
#define QUARRY_STOCK_MOVE_DOWN		"quarry-move-down"
#define QUARRY_STOCK_NEXT		"quarry-next"
#define QUARRY_STOCK_OVERWRITE		"quarry-overwrite"
#define QUARRY_STOCK_PLAY		"quarry-play"
#define QUARRY_STOCK_PREVIOUS		"quarry-previous"


/* Backward-compatible wrappers around evolving GTK+ stock.  Fallbacks
 * are either similar icons or no icons.
 *
 * A prefix `ICON' or `MENU_ITEM' is used to show that these are not
 * real stock identifiers.
 */

#if GTK_2_6_OR_LATER


#define QUARRY_STOCK_ICON_DIRECTORY	GTK_STOCK_DIRECTORY
#define QUARRY_STOCK_ICON_FILE		GTK_STOCK_FILE

#define QUARRY_STOCK_MENU_ITEM_ABOUT		\
  "<StockItem>",	GTK_STOCK_ABOUT


#else /* not GTK_2_6_OR_LATER */


#define QUARRY_STOCK_ICON_DIRECTORY	GTK_STOCK_OPEN
#define QUARRY_STOCK_ICON_FILE		GTK_STOCK_NEW

#define QUARRY_STOCK_MENU_ITEM_ABOUT		\
  "<Item>"


#endif /* not GTK_2_6_OR_LATER */


void		quarry_stock_init (void);


#endif /* QUARRY_QUARRY_STOCK_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
