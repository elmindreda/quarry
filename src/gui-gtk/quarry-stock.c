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


#include "quarry-stock.h"

#include <gtk/gtk.h>


typedef struct _IconMap	 IconMap;

struct _IconMap {
  const gchar  *quarry_stock_id;
  const gchar  *gtk_stock_id;
};


static const IconMap quarry_icon_map[] = {
  { QUARRY_STOCK_NEW_GAME,	    GTK_STOCK_NEW },
  { QUARRY_STOCK_OPEN_GAME_RECORD,  GTK_STOCK_OPEN },

  { QUARRY_STOCK_NEXT,		    GTK_STOCK_GO_FORWARD },
  { QUARRY_STOCK_PLAY,		    GTK_STOCK_OK },

  { QUARRY_STOCK_MOVE_UP,	    GTK_STOCK_GO_UP },
  { QUARRY_STOCK_MOVE_DOWN,	    GTK_STOCK_GO_DOWN },
  { QUARRY_STOCK_BROWSE,	    GTK_STOCK_OPEN },
  { QUARRY_STOCK_OVERWRITE,	    GTK_STOCK_SAVE }
};

static const GtkStockItem quarry_stock_items[] = {
  { QUARRY_STOCK_NEW_GAME,	    "_New Game",	  0, 0, NULL },
  { QUARRY_STOCK_OPEN_GAME_RECORD,  "_Open Game Record",  0, 0, NULL },

  { QUARRY_STOCK_NEXT,		    "_Next",		  0, 0, NULL },
  { QUARRY_STOCK_PLAY,		    "_Play",		  0, 0, NULL },

  { QUARRY_STOCK_MOVE_UP,	    "Move _Up",		  0, 0, NULL },
  { QUARRY_STOCK_MOVE_DOWN,	    "Move _Down",	  0, 0, NULL },
  { QUARRY_STOCK_BROWSE,	    "_Browse...",	  0, 0, NULL },
  { QUARRY_STOCK_OVERWRITE,	    "_Overwrite",	  0, 0, NULL }
};


void
quarry_stock_init(void)
{
  GtkIconFactory *icon_factory = gtk_icon_factory_new();
  int k;

  for (k = 0; k < (int) (sizeof(quarry_icon_map) / sizeof(IconMap)); k++) {
    GtkIconSet *icon_set
      = gtk_icon_factory_lookup_default(quarry_icon_map[k].gtk_stock_id);

    gtk_icon_factory_add(icon_factory, quarry_icon_map[k].quarry_stock_id,
			 icon_set);
  }

  gtk_icon_factory_add_default(icon_factory);

  gtk_stock_add_static(quarry_stock_items,
		       sizeof(quarry_stock_items) / sizeof(GtkStockItem));
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
