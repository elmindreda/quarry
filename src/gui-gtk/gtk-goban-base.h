/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004 Paul Pogonyshev.                             *
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


#ifndef QUARRY_GTK_GOBAN_BASE_H
#define QUARRY_GTK_GOBAN_BASE_H


#include "gtk-tile-set.h"
#include "board.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_GOBAN_BASE	gtk_goban_base_get_type ()

#define GTK_GOBAN_BASE(object)						\
  GTK_CHECK_CAST ((object), GTK_TYPE_GOBAN_BASE, GtkGobanBase)

#define GTK_GOBAN_BASE_CLASS(class)					\
  GTK_CHECK_CLASS_CAST ((class), GTK_TYPE_GOBAN_BASE,			\
			GtkGobanBaseClass)

#define GTK_IS_GOBAN_BASE(object)					\
  GTK_CHECK_TYPE ((object), GTK_TYPE_GOBAN_BASE)

#define GTK_IS_GOBAN_BASE_CLASS(class)					\
  GTK_CHECK_CLASS_TYPE ((class), GTK_TYPE_GOBAN_BASE)

#define GTK_GOBAN_BASE_GET_CLASS(object)				\
  (GTK_CHECK_GET_CLASS ((object), GTK_TYPE_GOBAN_BASE,			\
			GtkGobanBaseClass)



typedef struct _GtkGobanBase		GtkGobanBase;
typedef struct _GtkGobanBaseClass	GtkGobanBaseClass;

struct _GtkGobanBase {
  GtkWidget		 widget;

  Game			 game;

  gint			 cell_size;

  GtkMainTileSet	*main_tile_set;
  GtkSgfMarkupTileSet	*sgf_markup_tile_set;

  PangoFontDescription  *font_description;
};

struct _GtkGobanBaseClass {
  GtkWidgetClass	 parent_class;

  void (* allocate_screen_resources) (GtkGobanBase *goban_base);
  void (* free_screen_resources) (GtkGobanBase *goban_base);
};


GtkType 	gtk_goban_base_get_type (void);


void		gtk_goban_base_update_appearance (Game game);

void		gtk_goban_base_set_game (GtkGobanBase *goban_base, Game game);
void		gtk_goban_base_set_cell_size (GtkGobanBase *goban_base,
					      gint cell_size);


#endif /* QUARRY_GTK_GOBAN_BASE_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
