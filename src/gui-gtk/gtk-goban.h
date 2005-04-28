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


#ifndef QUARRY_GTK_GOBAN_H
#define QUARRY_GTK_GOBAN_H


#include "gtk-goban-base.h"
#include "gtk-tile-set.h"
#include "sgf.h"
#include "board.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_GOBAN		gtk_goban_get_type ()

#define GTK_GOBAN(object)						\
  GTK_CHECK_CAST ((object), GTK_TYPE_GOBAN, GtkGoban)

#define GTK_GOBAN_CLASS(class)						\
  GTK_CHECK_CLASS_CAST ((class), GTK_TYPE_GOBAN, GtkGobanClass)

#define GTK_IS_GOBAN(object)						\
  GTK_CHECK_TYPE ((object), GTK_TYPE_GOBAN)

#define GTK_IS_GOBAN_CLASS(class)					\
  GTK_CHECK_CLASS_TYPE ((class), GTK_TYPE_GOBAN)

#define GTK_GOBAN_GET_CLASS(object)					\
  (GTK_CHECK_GET_CLASS ((object), GTK_TYPE_GOBAN, GtkGobanClass)


typedef struct _GtkGobanPointerData	GtkGobanPointerData;
typedef struct _GtkGobanClickData	GtkGobanClickData;

struct _GtkGobanPointerData {
  gint		      x;
  gint		      y;
  BoardPositionList  *feedback_position_list;

  GdkModifierType     modifiers;
  gint		      button;
  gint		      press_x;
  gint		      press_y;
};

struct _GtkGobanClickData {
  gint		      x;
  gint		      y;
  gint		      feedback_tile;
  GdkModifierType     modifiers;
  gint		      button;
};

typedef enum {
  GOBAN_FEEDBACK_NONE,

  GOBAN_FEEDBACK_FORCE_TILE_NONE,

  GOBAN_FEEDBACK_OPAQUE,
  GOBAN_FEEDBACK_BLACK_OPAQUE	     = GOBAN_FEEDBACK_OPAQUE + BLACK_INDEX,
  GOBAN_FEEDBACK_WHITE_OPAQUE	     = GOBAN_FEEDBACK_OPAQUE + WHITE_INDEX,

  GOBAN_FEEDBACK_GHOST,
  GOBAN_FEEDBACK_BLACK_GHOST	     = GOBAN_FEEDBACK_GHOST + BLACK_INDEX,
  GOBAN_FEEDBACK_WHITE_GHOST	     = GOBAN_FEEDBACK_GHOST + WHITE_INDEX,

  GOBAN_FEEDBACK_THICK_GHOST,
  GOBAN_FEEDBACK_THICK_BLACK_GHOST   = (GOBAN_FEEDBACK_THICK_GHOST
					+ BLACK_INDEX),
  GOBAN_FEEDBACK_THICK_WHITE_GHOST   = (GOBAN_FEEDBACK_THICK_GHOST
					+ WHITE_INDEX),

  GOBAN_FEEDBACK_PRESS_DEFAULT,

  GOBAN_FEEDBACK_MOVE,
  GOBAN_FEEDBACK_BLACK_MOVE	     = GOBAN_FEEDBACK_MOVE + BLACK_INDEX,
  GOBAN_FEEDBACK_WHITE_MOVE	     = GOBAN_FEEDBACK_MOVE + WHITE_INDEX,

  GOBAN_FEEDBACK_ADD_OR_REMOVE,
  GOBAN_FEEDBACK_ADD_BLACK_OR_REMOVE = (GOBAN_FEEDBACK_ADD_OR_REMOVE
					+ BLACK_INDEX),
  GOBAN_FEEDBACK_ADD_WHITE_OR_REMOVE = (GOBAN_FEEDBACK_ADD_OR_REMOVE
					+ WHITE_INDEX),
  GOBAN_FEEDBACK_SPECIAL,

  /* Must be more than enough. */
  GOBAN_FEEDBACK_MARKUP_FACTOR = 1 << 8,
  GOBAN_FEEDBACK_GRID_MASK = GOBAN_FEEDBACK_MARKUP_FACTOR - 1
} GtkGobanPointerFeedback;

/* Note that there _must not_ be zero enumeration element.  These
 * constants are used in `gtk-goban-window.c' in `callback_action'
 * field of GtkItemFactoryEntry and zero has special meaning in that
 * context.
 */
typedef enum {
  GOBAN_NAVIGATE_BACK = 1,
  GOBAN_NAVIGATE_BACK_FAST,
  GOBAN_NAVIGATE_FORWARD,
  GOBAN_NAVIGATE_FORWARD_FAST,
  GOBAN_NAVIGATE_PREVIOUS_VARIATION,
  GOBAN_NAVIGATE_NEXT_VARIATION,
  GOBAN_NAVIGATE_ROOT,
  GOBAN_NAVIGATE_VARIATION_END
} GtkGobanNavigationCommand;

/* Goban markup flags controlling appearance of stones in marked
 * positions.  Tiles are enumerated in `gui-utils/tile-set.h'.
 */
#if NUM_TILES >= 1 << 4
#error Too many tiles, need to adjust GOBAN_MARKUP_* flags.
#endif

enum {
  GOBAN_TILE_DONT_CHANGE	 = NUM_TILES,

  GOBAN_MARKUP_GHOSTIFY		 = 1 << 4,
  GOBAN_MARKUP_GHOSTIFY_SLIGHTLY = 1 << 5,

  GOBAN_MARKUP_FLAGS_MASK	 = (GOBAN_MARKUP_GHOSTIFY
				    | GOBAN_MARKUP_GHOSTIFY_SLIGHTLY),
  GOBAN_MARKUP_TILE_MASK	 = ~GOBAN_MARKUP_FLAGS_MASK
};


#define NUM_OVERLAYS		4
#define FEEDBACK_OVERLAY	(NUM_OVERLAYS - 1)


typedef struct _GtkGoban		GtkGoban;
typedef struct _GtkGobanClass		GtkGobanClass;

struct _GtkGoban {
  GtkGobanBase		 base;

  int			 width;
  int			 height;

  char			 grid[BOARD_GRID_SIZE];
  char			 goban_markup[BOARD_GRID_SIZE];
  char			 sgf_markup[BOARD_GRID_SIZE];
  gchar			*sgf_labels[BOARD_GRID_SIZE];

  BoardPositionList	*overlay_positon_lists[NUM_OVERLAYS];
  char			*overlay_contents[NUM_OVERLAYS];

  int			 last_move_pos;

  gint			 font_size;
  gint			 character_height;
  gint			 digit_width;

  gint			 left_margin;
  gint			 right_margin;
  gint			 top_margin;
  gint			 bottom_margin;
  gint			 first_cell_center_x;
  gint			 first_cell_center_y;

  gint			 coordinates_x_left;
  gint			 coordinates_x_right;
  gint			 coordinates_y_side;
  gint			 coordinates_y_top;
  gint			 coordinates_y_bottom;

  int			 num_hoshi_points;
  BoardPoint		 hoshi_points[9];

  GtkMainTileSet	*small_tile_set;
  GObject		*checkerboard_pattern_object;

  int			 pointer_x;
  int			 pointer_y;
  int			 feedback_tile;

  GdkModifierType	 modifiers;
  unsigned int		 button_pressed;
  gint			 press_x;
  gint			 press_y;
  GdkModifierType	 press_modifiers;
  int			 feedback_tile_at_press;
};

struct _GtkGobanClass {
  GtkGobanBaseClass	 parent_class;

  GtkGobanPointerFeedback (* pointer_moved) (GtkGoban *goban,
					     GtkGobanPointerData *data);
  void (* goban_clicked) (GtkGoban *goban, GtkGobanClickData *data);

  void (* navigate) (GtkGoban *goban, GtkGobanNavigationCommand command);
};


#define KEEP_SGF_LABELS		((const SgfLabelList *) -1)


GType	 	gtk_goban_get_type (void);

GtkWidget *	gtk_goban_new (void);

void		gtk_goban_set_parameters (GtkGoban *goban, Game game,
					  int width, int height);

void		gtk_goban_update (GtkGoban *goban,
				  const char grid[BOARD_GRID_SIZE],
				  const char goban_markup[BOARD_GRID_SIZE],
				  const char sgf_markup[BOARD_GRID_SIZE],
				  const SgfLabelList *sgf_label_list,
				  int last_move_x, int last_move_y);
void		gtk_goban_force_feedback_poll (GtkGoban *goban);

void		gtk_goban_set_overlay_data (GtkGoban *goban, int overlay_index,
					    BoardPositionList *position_list,
					    int tile, int goban_markup_tile);

void		gtk_goban_set_contents (GtkGoban *goban,
					BoardPositionList *position_list,
					int grid_contents,
					int goban_markup_contents);
int		gtk_goban_get_grid_contents (GtkGoban *goban, int x, int y);
void		gtk_goban_diff_against_grid
		  (GtkGoban *goban, const char *grid,
		   BoardPositionList *position_lists[NUM_ON_GRID_VALUES]);

gint		gtk_goban_negotiate_width (GtkWidget *widget, gint height);
gint		gtk_goban_negotiate_height (GtkWidget *widget, gint width);


#endif /* QUARRY_GTK_GOBAN_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
