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


#ifndef QUARRY_GTK_GOBAN_WINDOW_H
#define QUARRY_GTK_GOBAN_WINDOW_H


#include "gtk-goban.h"
#include "gtp-client.h"
#include "sgf.h"
#include "board.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_GOBAN_WINDOW	(gtk_goban_window_get_type())
#define GTK_GOBAN_WINDOW(obj)						\
  (GTK_CHECK_CAST((obj),  GTK_TYPE_GOBAN_WINDOW, GtkGobanWindow))
#define GTK_GOBAN_WINDOW_CLASS(klass)					\
  (GTK_CHECK_CLASS_CAST((klass), GTK_TYPE_GOBAN_WINDOW,			\
			GtkGobanWindowClass))

#define GTK_IS_GOBAN_WINDOW(obj)					\
  (GTK_CHECK_TYPE((obj), GTK_TYPE_GOBAN_WINDOW))
#define GTK_IS_GOBAN_WINDOW_CLASS(klass)				\
  (GTK_CHECK_CLASS_TYPE((klass), GTK_TYPE_GOBAN_WINDOW))

#define GTK_GOBAN_WINDOW_GET_CLASS(obj)					\
  (GTK_CHECK_GET_CLASS((obj), GTK_TYPE_GOBAN_WINDOW,			\
		       GtkGobanWindowClass))


enum {
  SELECTING_QUEEN,
  MOVING_QUEEN,
  SHOOTING_ARROW
};


typedef struct _GtkPlayerInformation	GtkPlayerInformation;

typedef struct _GtkGobanWindow		GtkGobanWindow;
typedef struct _GtkGobanWindowClass	GtkGobanWindowClass;

struct _GtkPlayerInformation {
  GtkLabel		  *player_label;
};

struct _GtkGobanWindow {
  GtkWindow		   window;

  GtkGoban		  *goban;
  GtkWidget		  *player_table_alignment;
  GtkPlayerInformation	   player_information[NUM_COLORS];
  GtkWidget		  *hseparator;
  GtkWidget		  *move_info_label;
  GtkTextBuffer		  *text_buffer;


  Board			  *board;
  SgfBoardState		   sgf_board_state;

  gboolean		   in_game_mode;
  GtpClient		  *players[NUM_COLORS];
  gboolean		   player_initialized[NUM_COLORS];

  int			   amazons_move_stage;
  int			   amazons_to_x;
  int			   amazons_to_y;
  BoardAmazonsMoveData     amazons_move;

  int			   black_variations[BOARD_GRID_SIZE];
  int			   white_variations[BOARD_GRID_SIZE];
  char			   sgf_markup[BOARD_GRID_SIZE];

  SgfCollection		  *sgf_collection;
  SgfGameTree		  *current_tree;

  char			  *filename;

  const SgfNode		  *last_displayed_node;
  const SgfNode		  *last_game_info_node;

  int			   switching_x;
  int			   switching_y;
  SgfDirection		   switching_direction;
  SgfNode		  *node_to_switch_to;
};

struct _GtkGobanWindowClass {
  GtkWindowClass	   parent_class;

  void (* navigate_goban) (GtkGobanWindow *goban_window,
			   GtkGobanNavigationCommand command);
};


GtkType		gtk_goban_window_get_type(void);

GtkWidget *	gtk_goban_window_new(SgfCollection *sgf_collection,
				     const char *filename);

void		gtk_goban_window_enter_game_mode(GtkGobanWindow *goban_window,
						 GtpClient *black_player,
						 GtpClient *white_player);


#endif /* QUARRY_GTK_GOBAN_WINDOW_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
