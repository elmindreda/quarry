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


#include "quarry-stock.h"

#include "utils.h"

#include <gtk/gtk.h>


typedef struct _IconMap	 IconMap;

struct _IconMap {
  const gchar  *quarry_stock_id;
  const gchar  *gtk_stock_id;
};


static const IconMap quarry_icon_map[] = {
  { QUARRY_STOCK_NEW_GAME,	    GTK_STOCK_NEW	 },
  { QUARRY_STOCK_OPEN_GAME_RECORD,  GTK_STOCK_OPEN	 },

  { QUARRY_STOCK_BROWSE,	    GTK_STOCK_OPEN	 },
  { QUARRY_STOCK_CREATE,	    GTK_STOCK_NEW	 },
  { QUARRY_STOCK_DONE,		    GTK_STOCK_OK	 },
  { QUARRY_STOCK_GO_TO_NODE,	    GTK_STOCK_JUMP_TO	 },
  { QUARRY_STOCK_MODIFY,	    GTK_STOCK_PROPERTIES },
  { QUARRY_STOCK_MOVE_UP,	    GTK_STOCK_GO_UP	 },
  { QUARRY_STOCK_MOVE_DOWN,	    GTK_STOCK_GO_DOWN	 },
  { QUARRY_STOCK_FIND_NEXT,	    GTK_STOCK_GO_FORWARD },
  { QUARRY_STOCK_OVERWRITE,	    GTK_STOCK_SAVE	 },
  { QUARRY_STOCK_PLAY,		    GTK_STOCK_OK	 },
  { QUARRY_STOCK_FIND_PREVIOUS,	    GTK_STOCK_GO_BACK	 }
};

static GtkStockItem quarry_stock_items[] = {
  { QUARRY_STOCK_NEW_GAME,	    N_("_New Game"),	      0, 0, NULL },
  { QUARRY_STOCK_OPEN_GAME_RECORD,  N_("_Open Game Record"),  0, 0, NULL },

  { QUARRY_STOCK_BROWSE,	    N_("_Browse\342\200\246"), 0, 0, NULL },
  { QUARRY_STOCK_CREATE,	    N_("Crea_te"),	      0, 0, NULL },
  { QUARRY_STOCK_DONE,		    N_("_Done"),	      0, 0, NULL },
  { QUARRY_STOCK_GO_TO_NODE,	    N_("_Go to Node"),	      0, 0, NULL },
  { QUARRY_STOCK_MODIFY,	    N_("_Modify"),	      0, 0, NULL },
  { QUARRY_STOCK_MOVE_UP,	    N_("Move _Up"),	      0, 0, NULL },
  { QUARRY_STOCK_MOVE_DOWN,	    N_("Move _Down"),	      0, 0, NULL },
  { QUARRY_STOCK_FIND_NEXT,	    N_("find|_Next"),	      0, 0, NULL },
  { QUARRY_STOCK_OVERWRITE,	    N_("_Overwrite"),	      0, 0, NULL },
  { QUARRY_STOCK_PLAY,		    N_("_Play"),	      0, 0, NULL },
  { QUARRY_STOCK_FIND_PREVIOUS,	    N_("find|_Previous"),     0, 0, NULL }
};


void
quarry_stock_init (void)
{
  GtkIconFactory *icon_factory = gtk_icon_factory_new ();
  int k;

  for (k = 0; k < (int) (sizeof quarry_icon_map / sizeof (IconMap)); k++) {
    GtkIconSet *icon_set
      = gtk_icon_factory_lookup_default (quarry_icon_map[k].gtk_stock_id);

    gtk_icon_factory_add (icon_factory, quarry_icon_map[k].quarry_stock_id,
			  icon_set);
  }

  gtk_icon_factory_add_default (icon_factory);

  /* Gettextize labels first. */
  for (k = 0; k < (int) (sizeof quarry_stock_items / sizeof (GtkStockItem));
       k++) {
    /* Most labels don't have contexts, but that's not a problem:
     * utils_gettext_with_context() will do the right thing.
     */
    quarry_stock_items[k].label = (gchar *) Q_(quarry_stock_items[k].label);
  }

  gtk_stock_add_static (quarry_stock_items,
			sizeof quarry_stock_items / sizeof (GtkStockItem));
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
