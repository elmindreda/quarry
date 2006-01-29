/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004, 2005, 2006 Paul Pogonyshev.                 *
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


#include "gtk-configuration.h"
#include "gtk-goban-base.h"
#include "gtk-preferences.h"
#include "gtk-tile-set.h"
#include "gtk-utils.h"
#include "quarry-marshal.h"
#include "board.h"
#include "utils.h"

#include <assert.h>
#include <gtk/gtk.h>


static void	gtk_goban_base_class_init (GtkGobanBaseClass *class);
static void	gtk_goban_base_init (GtkGobanBase *goban_base);

static void	gtk_goban_base_realize (GtkWidget *widget);
static void	gtk_goban_base_unrealize (GtkWidget *widget);

static void	gtk_goban_base_allocate_screen_resources
		  (GtkGobanBase *goban_base);
static void	gtk_goban_base_free_screen_resources
		  (GtkGobanBase *goban_base);

static void	gtk_goban_base_finalize (GObject *object);


static void	set_background (GtkGobanBase *goban_base);



static GtkWidgetClass  *parent_class;

static GSList	       *all_goban_bases;


enum {
  ALLOCATE_SCREEN_RESOURCES,
  FREE_SCREEN_RESOURCES,
  NUM_SIGNALS
};

static guint		goban_base_signals[NUM_SIGNALS];


GType
gtk_goban_base_get_type (void)
{
  static GType goban_base_type = 0;

  if (!goban_base_type) {
    static GTypeInfo goban_base_info = {
      sizeof (GtkGobanBaseClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_goban_base_class_init,
      NULL,
      NULL,
      sizeof (GtkGobanBase),
      0,
      (GInstanceInitFunc) gtk_goban_base_init,
      NULL
    };

    goban_base_type = g_type_register_static (GTK_TYPE_WIDGET, "GtkGobanBase",
					      &goban_base_info,
					      G_TYPE_FLAG_ABSTRACT);
  }

  return goban_base_type;
}


static void
gtk_goban_base_class_init (GtkGobanBaseClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  G_OBJECT_CLASS (class)->finalize = gtk_goban_base_finalize;

  widget_class->realize	  = gtk_goban_base_realize;
  widget_class->unrealize = gtk_goban_base_unrealize;

  class->allocate_screen_resources = gtk_goban_base_allocate_screen_resources;
  class->free_screen_resources	   = gtk_goban_base_free_screen_resources;

  goban_base_signals[ALLOCATE_SCREEN_RESOURCES]
    = g_signal_new ("allocate-screen-resources",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkGobanBaseClass,
				     allocate_screen_resources),
		    NULL, NULL,
		    quarry_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);

  goban_base_signals[FREE_SCREEN_RESOURCES]
    = g_signal_new ("free-screen-resources",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkGobanBaseClass,
				     free_screen_resources),
		    NULL, NULL,
		    quarry_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
}


static void
gtk_goban_base_init (GtkGobanBase *goban_base)
{
  goban_base->game = GAME_DUMMY;

  goban_base->cell_size = 0;

  goban_base->font_description
    = pango_font_description_copy (gtk_widget_get_default_style ()->font_desc);

  all_goban_bases = g_slist_prepend (all_goban_bases, goban_base);
}


static void
gtk_goban_base_realize (GtkWidget *widget)
{
  assert (GTK_GOBAN_BASE (widget)->cell_size != 0);

  g_signal_emit (widget, goban_base_signals[ALLOCATE_SCREEN_RESOURCES], 0);
}


static void
gtk_goban_base_unrealize (GtkWidget *widget)
{
  g_signal_emit (widget, goban_base_signals[FREE_SCREEN_RESOURCES], 0);

  parent_class->unrealize (widget);
}


static void
gtk_goban_base_allocate_screen_resources (GtkGobanBase *goban_base)
{
  gint main_tile_size = (goban_base->cell_size
			 - (goban_base->game == GAME_GO ? 0 : 1));

  goban_base->main_tile_set
    = gtk_main_tile_set_create_or_reuse (main_tile_size, goban_base->game);

  goban_base->sgf_markup_tile_set
    = gtk_sgf_markup_tile_set_create_or_reuse (main_tile_size,
					       goban_base->game);
}


static void
gtk_goban_base_free_screen_resources (GtkGobanBase *goban_base)
{
  object_cache_unreference_object (&gtk_main_tile_set_cache,
				   goban_base->main_tile_set);
  object_cache_unreference_object (&gtk_sgf_markup_tile_set_cache,
				   goban_base->sgf_markup_tile_set);
}


static void
gtk_goban_base_finalize (GObject *object)
{
  GtkGobanBase *goban_base = GTK_GOBAN_BASE (object);

  pango_font_description_free (goban_base->font_description);

  all_goban_bases = g_slist_remove (all_goban_bases, goban_base);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}



void
gtk_goban_base_update_appearance (Game game)
{
  GSList *item;

  assert (game >= FIRST_GAME && GAME_IS_SUPPORTED (game));

  for (item = all_goban_bases; item; item = item->next) {
    GtkGobanBase *goban_base = (GtkGobanBase *) (item->data);

    if (goban_base->game == game) {
      set_background (goban_base);

      if (GTK_WIDGET_REALIZED (GTK_WIDGET (goban_base))) {
	g_signal_emit (goban_base,
		       goban_base_signals[FREE_SCREEN_RESOURCES], 0);
	g_signal_emit (goban_base,
		       goban_base_signals[ALLOCATE_SCREEN_RESOURCES], 0);
      }
    }
  }
}



/* The following non-static functions are meant to be protected (in
 * C++ sense.)  They should only be called by descendant widgets.
 */

void
gtk_goban_base_set_game (GtkGobanBase *goban_base, Game game)
{
  assert (GTK_IS_GOBAN_BASE (goban_base));
  assert (GAME_IS_SUPPORTED (game));

  if (goban_base->game != game) {
    goban_base->game = game;
    set_background (goban_base);
  }
}


void
gtk_goban_base_set_cell_size (GtkGobanBase *goban_base, gint cell_size)
{
  assert (GTK_IS_GOBAN_BASE (goban_base));
  assert (cell_size > 0);

  if (goban_base->cell_size != cell_size) {
    goban_base->cell_size = cell_size;

    if (GTK_WIDGET_REALIZED (GTK_WIDGET (goban_base))) {
      g_signal_emit (goban_base, goban_base_signals[FREE_SCREEN_RESOURCES], 0);
      g_signal_emit (goban_base,
		     goban_base_signals[ALLOCATE_SCREEN_RESOURCES], 0);
    }
  }
}


static void
set_background (GtkGobanBase *goban_base)
{
  const BoardAppearance *board_appearance
    = game_to_board_appearance_structure (goban_base->game);
  GtkRcStyle *rc_style = gtk_rc_style_new ();

  rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_FG | GTK_RC_BG;
  gtk_utils_set_gdk_color (&rc_style->fg[GTK_STATE_NORMAL],
			   board_appearance->grid_and_labels_color);
  gtk_utils_set_gdk_color (&rc_style->bg[GTK_STATE_NORMAL],
			   board_appearance->background_color);

  if (board_appearance->use_background_texture) {
    rc_style->bg_pixmap_name[GTK_STATE_NORMAL]
      = g_strdup (board_appearance->background_texture);
  }

  gtk_widget_modify_style (&goban_base->widget, rc_style);
  g_object_unref (rc_style);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
