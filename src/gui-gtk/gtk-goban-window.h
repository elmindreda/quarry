/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003 Paul Pogonyshev.                             *
 * Copyright (C) 2004 Paul Pogonyshev and Martin Holters.          *
 * Copyright (C) 2005 Paul Pogonyshev                              *
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


#include "gtk-clock.h"
#include "gtk-game-info-dialog.h"
#include "gtk-goban.h"
#include "gtk-sgf-tree-view.h"
#include "gtk-progress-dialog.h"
#include "time-control.h"
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


typedef struct _GtkGobanWindow		GtkGobanWindow;
typedef struct _GtkGobanWindowClass	GtkGobanWindowClass;

struct _GtkGobanWindow {
  GtkWindow		   window;

  GtkItemFactory	  *item_factory;
  GtkToolbar		  *main_toolbar;
  GtkToolbar		  *navigation_toolbar;
  GtkGoban		  *goban;
  GtkLabel		  *player_labels[NUM_COLORS];
  GtkLabel		  *game_specific_info[NUM_COLORS];
  GtkClock		  *clocks[NUM_COLORS];
  GtkLabel		  *move_information_label;
  GtkWidget		  *pass_button;
  GtkWidget		  *resign_button;
  GtkWidget		  *mode_information_vbox;
  GtkLabel		  *mode_hint_label;
  GtkWidget		  *done_button;
  GtkWidget		  *cancel_button;
  GtkTextView		  *text_view;
  GtkTextBuffer		  *text_buffer;
  GtkPaned		  *vpaned;
  GtkSgfTreeView	  *sgf_tree_view;
  gboolean		   sgf_tree_view_visibility_locked;

  Board			  *board;
  SgfBoardState		   sgf_board_state;

  SgfGameTreeState	   game_position;
  SgfBoardState		   game_position_board_state_holder;

  gboolean		   in_game_mode;
  gint			   pending_free_handicap;
  gint			   num_handicap_stones_placed;
  GtpClient		  *players[NUM_COLORS];
  gboolean		   player_initialization_step[NUM_COLORS];
  TimeControl		  *time_controls[NUM_COLORS];

  int			   amazons_move_stage;
  int			   amazons_to_x;
  int			   amazons_to_y;
  BoardAmazonsMoveData     amazons_move;

  int			   black_variations[BOARD_GRID_SIZE];
  int			   white_variations[BOARD_GRID_SIZE];
  char			   sgf_markup[BOARD_GRID_SIZE];

  char			  *dead_stones;
  BoardPositionList	  *dead_stones_list;
  int			  scoring_engine_player;
  gboolean		  engine_scoring_cancelled;

  SgfCollection		  *sgf_collection;
  SgfGameTree		  *current_tree;

  /* FIXME: Temporary, this should be tracked in SGF module together
   *	    with proper undo history.
   */
  gboolean		   sgf_collection_is_modified;

  /* NOTE: In file system encoding! */
  char			  *filename;

  GtkWidget		  *save_as_dialog;
  gboolean		   adjourning_game;

  SgfNode		  *last_displayed_node;
  SgfNode		  *last_game_info_node;

  int			   switching_x;
  int			   switching_y;
  SgfDirection		   switching_direction;
  SgfNode		  *node_to_switch_to;

  GtkDialog		  *find_dialog;
  GtkEntry		  *search_for_entry;
  GtkToggleButton	  *case_sensitive_toggle_button;
  GtkToggleButton	  *wrap_around_toggle_button;
  GtkToggleButton	  *whole_words_only_toggle_button;
  GtkToggleButton	  *search_whole_game_tree_toggle_button;
  GtkToggleButton	  *close_automatically_toggle_button;

  gchar			  *text_to_find;
  gboolean		   case_sensitive;
  gboolean		   whole_words_only;
  gboolean		   wrap_around;
  gboolean		   search_whole_game_tree;

  GtkGameInfoDialog	  *game_info_dialog;
  GtkProgressDialog	  *scoring_progress_dialog;
};

struct _GtkGobanWindowClass {
  GtkWindowClass	   parent_class;
};


GtkType		gtk_goban_window_get_type(void);

GtkWidget *	gtk_goban_window_new(SgfCollection *sgf_collection,
				     const char *filename);

void		gtk_goban_window_enter_game_record_mode
		  (GtkGobanWindow *goban_window);
void		gtk_goban_window_enter_game_mode
		  (GtkGobanWindow *goban_window,
		   GtpClient *black_player, GtpClient *white_player,
		   TimeControl *time_control);
void		gtk_goban_window_resume_game
		  (GtkGobanWindow *goban_window,
		   GtpClient *black_player, GtpClient *white_player);


#endif /* QUARRY_GTK_GOBAN_WINDOW_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
