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


#include "gtk-goban-window.h"

#include "gtk-control-center.h"
#include "gtk-goban.h"
#include "gtk-gtp-client-interface.h"
#include "gtk-named-vbox.h"
#include "gtk-parser-interface.h"
#include "gtk-qhbox.h"
#include "gtk-qvbox.h"
#include "gtk-utils.h"
#include "quarry-marshal.h"
#include "gui-utils.h"
#include "gtp-client.h"
#include "sgf.h"
#include "board.h"
#include "game-info.h"
#include "utils.h"

#include <assert.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>


#define NAVIGATE_FAST_NUM_MOVES	10

#define USER_CAN_PLAY_MOVES(goban_window)				\
  (!(goban_window)->in_game_mode					\
   || !((goban_window)							\
	->players[COLOR_INDEX((goban_window)				\
			      ->sgf_board_state.color_to_play)]))


enum {
  GTK_GOBAN_WINDOW_SAVE = 1,
  GTK_GOBAN_WINDOW_SAVE_AS
};


static void	 gtk_goban_window_class_init(GtkGobanWindowClass *class);
static void	 gtk_goban_window_init(GtkGobanWindow *goban_window);

static void	 gtk_goban_window_destroy(GtkObject *object);
static void	 gtk_goban_window_finalize(GObject *object);

static void	 gtk_goban_window_save(GtkGobanWindow *goban_window,
				       guint callback_action);
static void	 save_file_as_response(GtkFileSelection *dialog,
				       gint response_id,
				       GtkGobanWindow *goban_window);
static void	 save_file_as_destroy(GtkGobanWindow *goban_window);

static void	 play_pass_move(GtkGobanWindow *goban_window);


static void	 set_current_tree(GtkGobanWindow *goban_window,
				  SgfGameTree *sgf_tree);

static void	 cancel_amazons_move(GtkGobanWindow *goban_window);
static void	 reset_amazons_move_data(GtkGobanWindow *goban_window);

static GtkGobanPointerFeedback
		 playing_mode_pointer_moved(GtkGobanWindow *goban_window,
					    GtkGobanPointerData *data);
static void	 playing_mode_goban_clicked(GtkGobanWindow *goban_window,
					    GtkGobanClickData *data);
static void	 playing_mode_navigate_goban(GtkGobanWindow *goban_window,
					     GtkGobanNavigationCommand command);

static int	 find_variation_to_switch_to(GtkGobanWindow *goban_window,
					     int x, int y,
					     SgfDirection direction);

static void	 update_children_for_new_node(GtkGobanWindow *goban_window);

static void	 update_game_information(GtkGobanWindow *goban_window);
static void	 update_window_title(GtkGobanWindow *goban_window,
				     const SgfNode *game_info_node);
static int	 update_player_information
		   (const SgfNode *game_info_node,
		    GtkPlayerInformation *player_information,
		    SgfType name_property, SgfType rank_property,
		    SgfType team_property);

static void	 update_move_information(GtkGobanWindow *goban_window);

static void	 initialize_gtp_player(GtkGobanWindow *goban_window,
				       GtpClient *client);
static void	 gtp_player_initialized_for_game(GtpClient *player,
						 int successful,
						 GtkGobanWindow *goban_window);
static void	 move_has_been_played(GtkGobanWindow *goban_window);
static void	 move_has_been_generated(GtpClient *client, int successful,
					 GtkGobanWindow *goban_window,
					 int color, int x, int y,
					 BoardAbstractMoveData *move_data);


static GtkWindowClass  *parent_class;


GtkType
gtk_goban_window_get_type(void)
{
  static GtkType goban_window_type = 0;

  if (!goban_window_type) {
    static GTypeInfo goban_window_info = {
      sizeof(GtkGobanWindowClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_goban_window_class_init,
      NULL,
      NULL,
      sizeof(GtkGobanWindow),
      1,
      (GInstanceInitFunc) gtk_goban_window_init,
      NULL
    };

    goban_window_type = g_type_register_static(GTK_TYPE_WINDOW,
					       "GtkGobanWindow",
					       &goban_window_info, 0);
  }

  return goban_window_type;
}


static void
gtk_goban_window_class_init(GtkGobanWindowClass *class)
{
  parent_class = g_type_class_peek_parent(class);

  G_OBJECT_CLASS(class)->finalize = gtk_goban_window_finalize;

  GTK_OBJECT_CLASS(class)->destroy = gtk_goban_window_destroy;
}


static void
gtk_goban_window_init(GtkGobanWindow *goban_window)
{
  static GtkItemFactoryEntry menu_entries[] = {
    { "/_File",			NULL,		  NULL, 0, "<Branch>" },
    { "/File/_Open...",		"<ctrl>O",	  gtk_parser_interface_present,
      0,			    "<StockItem>",  GTK_STOCK_OPEN },
    { "/File/",			NULL,		  NULL, 0, "<Separator>" },
    { "/File/_Save",		"<ctrl>S",	  gtk_goban_window_save,
      GTK_GOBAN_WINDOW_SAVE,	    "<StockItem>",  GTK_STOCK_SAVE },
    { "/File/Save _As...",	"<shift><ctrl>S", gtk_goban_window_save,
      GTK_GOBAN_WINDOW_SAVE_AS,	    "<StockItem>",  GTK_STOCK_SAVE_AS },

    { "/_Play",			NULL,		  NULL, 0, "<Branch>" },
    { "/Play/_Pass",		NULL,		  play_pass_move,
      1,			    "<Item>" },

    /* FIXME: At some point these should be generalized for not
     *	      playing mode only.
     */
    { "/_Go",			NULL,		  NULL, 0, "<Branch>" },
    { "/Go/_Next Node",		"<alt>Right",	  playing_mode_navigate_goban,
      GOBAN_NAVIGATE_FORWARD,	    "<StockItem>",  GTK_STOCK_GO_FORWARD },
    { "/Go/_Previous Node",	"<alt>Left",	  playing_mode_navigate_goban,
      GOBAN_NAVIGATE_BACK,	    "<StockItem>",  GTK_STOCK_GO_BACK },
    { "/Go/Ten Nodes _Forward",	"<alt>Page_Down", playing_mode_navigate_goban,
      GOBAN_NAVIGATE_FORWARD_FAST,  "<Item>" },
    { "/Go/Ten Nodes _Backward", "<alt>Page_Up",  playing_mode_navigate_goban,
      GOBAN_NAVIGATE_BACK_FAST,	    "<Item>" },
    { "/Go/",			NULL,		  NULL, 0, "<Separator>" },
    { "/Go/_Root Node",		"<alt>Home",	  playing_mode_navigate_goban,
      GOBAN_NAVIGATE_ROOT,	    "<StockItem>",  GTK_STOCK_GOTO_FIRST },
    { "/Go/Variation _Last Node", "<alt>End",	  playing_mode_navigate_goban,
      GOBAN_NAVIGATE_VARIATION_END, "<StockItem>",  GTK_STOCK_GOTO_LAST },
    { "/Go/",			NULL,		  NULL, 0, "<Separator>" },
    { "/Go/Ne_xt Variation",	"<alt>Down",	  playing_mode_navigate_goban,
      GOBAN_NAVIGATE_NEXT_VARIATION, "<StockItem>", GTK_STOCK_GO_DOWN },
    { "/Go/Pre_vious Variation", "<alt>Up",	  playing_mode_navigate_goban,
      GOBAN_NAVIGATE_PREVIOUS_VARIATION, "<StockItem>", GTK_STOCK_GO_UP },
  };

  GtkWidget *goban;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *text_view;
  GtkWidget *scrolled_window;
  GtkWidget *vbox;
  GtkWidget *qhbox;
  GtkWidget *menu_bar;
  GtkAccelGroup *accel_group;
  int k;

  gtk_control_center_window_created(GTK_WINDOW(goban_window));

  /* Goban, the main thing in the window. */
  goban = gtk_goban_new();
  goban_window->goban = GTK_GOBAN(goban);

  g_signal_connect_swapped(goban, "pointer-moved",
			   G_CALLBACK(playing_mode_pointer_moved),
			   goban_window);
  g_signal_connect_swapped(goban, "goban-clicked",
			   G_CALLBACK(playing_mode_goban_clicked),
			   goban_window);
  g_signal_connect_swapped(goban, "navigate",
			   G_CALLBACK(playing_mode_navigate_goban),
			   goban_window);

  /* Frame to make goban look sunken. */
  frame = gtk_utils_sink_widget(goban);

  /* Table that holds players' information and clocks. */
  table = gtk_table_new(2, 2, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(table), QUARRY_SPACING_GOBAN_WINDOW);
  gtk_table_set_col_spacings(GTK_TABLE(table), QUARRY_SPACING);

  /* Information labels and clocks for each player. */
  for (k = 0; k < NUM_COLORS; k++) {
    GtkWidget *named_vbox;
    GtkWidget *label;

    named_vbox = gtk_named_vbox_new(k == BLACK_INDEX ? "Black" : "White",
				    FALSE, QUARRY_SPACING_VERY_SMALL);

    /* In GUI, White is always first. */
    gtk_table_attach(GTK_TABLE(table), named_vbox,
		     0, 1, OTHER_INDEX(k), OTHER_INDEX(k) + 1,
		     GTK_FILL, 0, 0, 0);

    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(named_vbox), label, FALSE, TRUE, 0);
    goban_window->player_information[k].player_label = GTK_LABEL(label);
  }

  /* Alignment widget to keep the table centered in sidebar. */
  goban_window->player_table_alignment = gtk_utils_align_widget(table,
								0.5, 0.5);

  /* Separate players and move information. */
  goban_window->hseparator = gtk_hseparator_new();

  /* Move information label. */
  goban_window->move_info_label = gtk_label_new(NULL);

  /* Multipurpose text view. */
  text_view = gtk_text_view_new();
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view),
				QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view),
				 QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
  goban_window->text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

  /* Scrolled window to keep text view in it. */
  scrolled_window = gtk_utils_make_widget_scrollable(text_view,
						     GTK_POLICY_AUTOMATIC,
						     GTK_POLICY_AUTOMATIC);

  /* Sidebar vertical box. */
  vbox = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_GOBAN_WINDOW,
			       goban_window->player_table_alignment,
			       GTK_UTILS_FILL,
			       goban_window->hseparator, GTK_UTILS_FILL,
			       goban_window->move_info_label, GTK_UTILS_FILL,
			       scrolled_window, GTK_UTILS_PACK_DEFAULT, NULL);

  /* Horizontal box containing goban and sidebar. */
  qhbox = gtk_utils_pack_in_box(GTK_TYPE_QHBOX, QUARRY_SPACING_GOBAN_WINDOW,
				frame, GTK_UTILS_FILL,
				vbox, GTK_UTILS_PACK_DEFAULT, NULL);
  gtk_container_set_border_width(GTK_CONTAINER(qhbox),
				 QUARRY_SPACING_GOBAN_WINDOW);
  gtk_qbox_set_ruling_widget(GTK_QBOX(qhbox), frame,
			     gtk_goban_negotiate_height);

  /* Window menu bar and associated accelerator group. */
  accel_group = gtk_accel_group_new();

  goban_window->item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR,
						    "<QuarryGobanWindowMenu>",
						    accel_group);
  gtk_item_factory_create_items(goban_window->item_factory,
				(sizeof(menu_entries)
				 / sizeof(GtkItemFactoryEntry)),
				menu_entries,
				goban_window);
  menu_bar = gtk_item_factory_get_widget(goban_window->item_factory,
					 "<QuarryGobanWindowMenu>");

  gtk_window_add_accel_group(GTK_WINDOW(goban_window), accel_group);

  /* Vertical box with menu bar and actual window contents. */
  vbox = gtk_utils_pack_in_box(GTK_TYPE_VBOX, 0,
			       menu_bar, GTK_UTILS_FILL,
			       qhbox, GTK_UTILS_PACK_DEFAULT, NULL);
  gtk_container_add(GTK_CONTAINER(goban_window), vbox);

  /* Show everything but the window itself. */
  gtk_widget_show_all(vbox);

  goban_window->board = NULL;

  goban_window->in_game_mode	     = FALSE;
  goban_window->players[BLACK_INDEX] = NULL;
  goban_window->players[WHITE_INDEX] = NULL;

  goban_window->filename = NULL;
  goban_window->save_as_dialog = NULL;

  goban_window->last_displayed_node = NULL;
  goban_window->last_game_info_node = NULL;
}


GtkWidget *
gtk_goban_window_new(SgfCollection *sgf_collection, const char *filename)
{
  GtkWidget *widget = GTK_WIDGET(g_object_new(GTK_TYPE_GOBAN_WINDOW, NULL));
  GtkGobanWindow *goban_window = GTK_GOBAN_WINDOW(widget);

  assert(sgf_collection);

  goban_window->board = NULL;

  goban_window->sgf_collection = sgf_collection;
  if (filename)
    goban_window->filename = g_strdup(filename);

  set_current_tree(goban_window, sgf_collection->first_tree);

  return widget;
}


void
gtk_goban_window_enter_game_mode(GtkGobanWindow *goban_window,
				 GtpClient *black_player,
				 GtpClient *white_player)
{
  assert(GTK_IS_GOBAN_WINDOW(goban_window));
  assert(!goban_window->in_game_mode);

  goban_window->in_game_mode	     = TRUE;
  goban_window->players[BLACK_INDEX] = black_player;
  goban_window->players[WHITE_INDEX] = white_player;

  if (black_player) {
    goban_window->player_initialized[BLACK_INDEX] = FALSE;
    initialize_gtp_player(goban_window, black_player);
  }

  if (white_player) {
    goban_window->player_initialized[WHITE_INDEX] = FALSE;
    initialize_gtp_player(goban_window, white_player);
  }
}


static void
gtk_goban_window_destroy(GtkObject *object)
{
  gtk_control_center_window_destroyed(GTK_WINDOW(object));

  GTK_OBJECT_CLASS(parent_class)->destroy(object);
}


static void
gtk_goban_window_finalize(GObject *object)
{
  GtkGobanWindow *goban_window = GTK_GOBAN_WINDOW(object);

  if (goban_window->board)
    board_delete(goban_window->board);

  if (goban_window->players[BLACK_INDEX])
    gtk_schedule_gtp_client_deletion(goban_window->players[BLACK_INDEX]);
  if (goban_window->players[WHITE_INDEX])
    gtk_schedule_gtp_client_deletion(goban_window->players[WHITE_INDEX]);

  if (goban_window->sgf_collection)
    sgf_collection_delete(goban_window->sgf_collection);

  g_free(goban_window->filename);
  if (goban_window->save_as_dialog)
    gtk_widget_destroy(GTK_WIDGET(goban_window->save_as_dialog));

  G_OBJECT_CLASS(parent_class)->finalize(object);
}


static void
gtk_goban_window_save(GtkGobanWindow *goban_window, guint callback_action)
{
  if (callback_action == GTK_GOBAN_WINDOW_SAVE_AS || !goban_window->filename) {
    if (!goban_window->save_as_dialog) {
      GtkWidget *file_selection = gtk_file_selection_new("Save As...");

      goban_window->save_as_dialog = GTK_WINDOW(file_selection);
      gtk_control_center_window_created(goban_window->save_as_dialog);
      gtk_window_set_transient_for(goban_window->save_as_dialog,
				   GTK_WINDOW(goban_window));
      gtk_window_set_destroy_with_parent(goban_window->save_as_dialog, TRUE);

      gtk_utils_add_file_selection_response_handlers
	(file_selection, TRUE,
	 G_CALLBACK(save_file_as_response), goban_window);
      g_signal_connect_swapped(file_selection, "destroy",
			       G_CALLBACK(save_file_as_destroy),
			       goban_window);
    }

    gtk_window_present(goban_window->save_as_dialog);
  }
}


static void
save_file_as_response(GtkFileSelection *dialog, gint response_id,
		      GtkGobanWindow *goban_window)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *filename = gtk_file_selection_get_filename(dialog);

    if (sgf_write_file(filename, goban_window->sgf_collection)) {
      g_free(goban_window->filename);
      goban_window->filename = g_strdup(filename);
    }
  }

  if (response_id == GTK_RESPONSE_OK || response_id == GTK_RESPONSE_CANCEL)
    gtk_widget_destroy(GTK_WIDGET(dialog));
}


static void
save_file_as_destroy(GtkGobanWindow *goban_window)
{
  gtk_control_center_window_destroyed(goban_window->save_as_dialog);
  goban_window->save_as_dialog = NULL;
}


static void
set_current_tree(GtkGobanWindow *goban_window, SgfGameTree *sgf_tree)
{
  if (!goban_window->board && GAME_IS_SUPPORTED(sgf_tree->game)) {
    goban_window->board = board_new(sgf_tree->game,
				    sgf_tree->board_width,
				    sgf_tree->board_height);
  }

  goban_window->current_tree = sgf_tree;
  sgf_utils_enter_tree(sgf_tree, goban_window->board,
		       &goban_window->sgf_board_state);

  gtk_goban_set_parameters(goban_window->goban, sgf_tree->game,
			   sgf_tree->board_width, sgf_tree->board_height);

  update_game_information(goban_window);
  update_children_for_new_node(goban_window);
}


static void
play_pass_move(GtkGobanWindow *goban_window)
{
  int color_to_play = goban_window->sgf_board_state.color_to_play;

  assert(goban_window->board->game == GAME_GO
	 && USER_CAN_PLAY_MOVES(goban_window));

  sgf_utils_append_move_variation(goban_window->current_tree,
				  &goban_window->sgf_board_state,
				  color_to_play, PASS_X, PASS_Y);
  move_has_been_played(goban_window);

  update_children_for_new_node(goban_window);
}


static void
cancel_amazons_move(GtkGobanWindow *goban_window)
{
  if (goban_window->amazons_move_stage == SHOOTING_ARROW) {
    gtk_goban_set_overlay_data(goban_window->goban, 1,
			       NULL_X, NULL_Y, TILE_NONE);
  }

  if (goban_window->amazons_move_stage != SELECTING_QUEEN) {
    gtk_goban_set_overlay_data(goban_window->goban, 0,
			       NULL_X, NULL_Y, TILE_NONE);
  }

  reset_amazons_move_data(goban_window);
}


static void
reset_amazons_move_data(GtkGobanWindow *goban_window)
{
  goban_window->amazons_move_stage = 0;

  goban_window->amazons_move.from.x	      = NULL_X;
  goban_window->amazons_move.from.y	      = NULL_Y;
  goban_window->amazons_to_x		      = NULL_X;
  goban_window->amazons_to_y		      = NULL_Y;
  goban_window->amazons_move.shoot_arrow_to.x = NULL_X;
  goban_window->amazons_move.shoot_arrow_to.y = NULL_Y;
}


static GtkGobanPointerFeedback
playing_mode_pointer_moved(GtkGobanWindow *goban_window,
			   GtkGobanPointerData *data)
{
  int color_to_play = goban_window->sgf_board_state.color_to_play;

  if (USER_CAN_PLAY_MOVES(goban_window)) {
    switch (data->button) {
    case 0:
      if (!(data->modifiers & GDK_SHIFT_MASK)) {
	if (color_to_play == EMPTY)
	  break;

	if (goban_window->board->game != GAME_AMAZONS) {
	  if (board_is_legal_move(goban_window->board, RULE_SET_DEFAULT,
				  color_to_play, data->x, data->y))
	    return GOBAN_FEEDBACK_MOVE_DEFAULT + COLOR_INDEX(color_to_play);
	}
	else {
	  if (goban_window->amazons_move_stage == SELECTING_QUEEN) {
	    goban_window->amazons_move.from.x = data->x;
	    goban_window->amazons_move.from.y = data->y;
	  }
	  else if (goban_window->amazons_move_stage == MOVING_QUEEN) {
	    goban_window->amazons_to_x = data->x;
	    goban_window->amazons_to_y = data->y;
	  }
	  else {
	    goban_window->amazons_move.shoot_arrow_to.x = data->x;
	    goban_window->amazons_move.shoot_arrow_to.y = data->y;
	  }

	  if (board_is_legal_move(goban_window->board, RULE_SET_DEFAULT,
				  color_to_play,
				  goban_window->amazons_to_x,
				  goban_window->amazons_to_y,
				  goban_window->amazons_move)) {
	    if (goban_window->amazons_move_stage == SELECTING_QUEEN)
	      return GOBAN_FEEDBACK_MOVE_DEFAULT + COLOR_INDEX(color_to_play);
	    else if (goban_window->amazons_move_stage == MOVING_QUEEN)
	      return GOBAN_FEEDBACK_GHOST + COLOR_INDEX(color_to_play);
	    else
	      return GOBAN_FEEDBACK_SPECIAL;
	  }
	}
      }

      break;

    case 1:
      return GOBAN_FEEDBACK_PRESS_DEFAULT;

    case 3:
      if ((goban_window->board->game != GAME_AMAZONS
	   || goban_window->amazons_move_stage == SELECTING_QUEEN)
	  && data->x == data->press_x && data->y == data->press_y
	  && find_variation_to_switch_to(goban_window, data->x, data->y,
					 data->modifiers & GDK_SHIFT_MASK
					 ? SGF_PREVIOUS : SGF_NEXT)) {
	return (GOBAN_FEEDBACK_THICK_GHOST
		+ COLOR_INDEX(goban_window->node_to_switch_to->move_color));
      }
    }
  }

  return GOBAN_FEEDBACK_NONE;
}


static void
playing_mode_goban_clicked(GtkGobanWindow *goban_window,
			   GtkGobanClickData *data)
{
  switch (data->button) {
  case 1:
    if (data->feedback_tile != TILE_NONE
	&& !(data->modifiers & GDK_SHIFT_MASK)) {
      int color_to_play = goban_window->sgf_board_state.color_to_play;

      if (color_to_play == EMPTY)
	return;

      if (goban_window->board->game != GAME_AMAZONS) {
	sgf_utils_append_move_variation(goban_window->current_tree,
					&goban_window->sgf_board_state,
					color_to_play, data->x, data->y);
      }
      else {
	if (goban_window->amazons_move_stage == SELECTING_QUEEN) {
	  goban_window->amazons_move_stage  = MOVING_QUEEN;

	  gtk_goban_set_overlay_data(goban_window->goban, 0,
				     data->x, data->y,
				     (STONE_50_TRANSPARENT
				      + COLOR_INDEX(color_to_play)));

	  return;
	}
	else if (goban_window->amazons_move_stage == MOVING_QUEEN) {
	  goban_window->amazons_move_stage  = SHOOTING_ARROW;

	  gtk_goban_set_overlay_data(goban_window->goban, 1,
				     data->x, data->y,
				     (STONE_25_TRANSPARENT
				      + COLOR_INDEX(color_to_play)));

	  return;
	}
	else {
	  sgf_utils_append_move_variation(goban_window->current_tree,
					  &goban_window->sgf_board_state,
					  color_to_play,
					  goban_window->amazons_to_x,
					  goban_window->amazons_to_y,
					  goban_window->amazons_move);
	}
      }
    }
    else
      return;

    move_has_been_played(goban_window);
    break;

  case 3:
    if (goban_window->board->game != GAME_AMAZONS
	|| goban_window->amazons_move_stage == SELECTING_QUEEN) {
      if (!find_variation_to_switch_to(goban_window, data->x, data->y,
				       data->modifiers & GDK_SHIFT_MASK
				       ? SGF_PREVIOUS : SGF_NEXT))
	return;

      sgf_utils_switch_to_given_variation(goban_window->current_tree,
					  goban_window->node_to_switch_to,
					  &goban_window->sgf_board_state);
    }
    else
      cancel_amazons_move(goban_window);

    break;

  default:
    return;
  }

  update_children_for_new_node(goban_window);
}


static void
playing_mode_navigate_goban(GtkGobanWindow *goban_window,
			    GtkGobanNavigationCommand command)
{
  SgfGameTree *current_tree = goban_window->current_tree;

  switch (command) {
  case GOBAN_NAVIGATE_BACK:
    sgf_utils_go_up_in_tree(current_tree, 1, &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_BACK_FAST:
    sgf_utils_go_up_in_tree(current_tree, NAVIGATE_FAST_NUM_MOVES,
			    &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_FORWARD:
    sgf_utils_go_down_in_tree(current_tree, 1,
			      &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_FORWARD_FAST:
    sgf_utils_go_down_in_tree(current_tree, NAVIGATE_FAST_NUM_MOVES,
			      &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_PREVIOUS_VARIATION:
    sgf_utils_switch_to_variation(current_tree, SGF_PREVIOUS,
				  &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_NEXT_VARIATION:
    sgf_utils_switch_to_variation(current_tree, SGF_NEXT,
				  &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_ROOT:
    sgf_utils_go_up_in_tree(current_tree, -1,
			    &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_VARIATION_END:
    sgf_utils_go_down_in_tree(current_tree, -1,
			      &goban_window->sgf_board_state);
    break;

  default:
    return;
  }

  update_children_for_new_node(goban_window);
}


static int
find_variation_to_switch_to(GtkGobanWindow *goban_window,
			    int x, int y,
			    SgfDirection direction)
{
  if (x != goban_window->switching_x || y != goban_window->switching_y
      || direction != goban_window->switching_direction) {
    const SgfNode *current_node = goban_window->current_tree->current_node;
    int after_node = (IS_STONE(current_node->move_color)
		      && current_node->move_point.x == x
		      && current_node->move_point.y == y);

    goban_window->switching_x	      = x;
    goban_window->switching_y	      = y;
    goban_window->switching_direction = direction;
    goban_window->node_to_switch_to
      = sgf_utils_find_variation_at_position(goban_window->current_tree,
					     x, y, direction, after_node);
  }

  return goban_window->node_to_switch_to != NULL;
}


static void
update_children_for_new_node(GtkGobanWindow *goban_window)
{
  SgfGameTree *current_tree = goban_window->current_tree;
  SgfNode *current_node = current_tree->current_node;
  char goban_markup[BOARD_GRID_SIZE];
  const char *comment;

  if (current_node == goban_window->last_displayed_node)
    return;

  reset_amazons_move_data(goban_window);

  if (!goban_window->last_displayed_node
      || current_node->parent != goban_window->last_displayed_node->parent) {
    sgf_utils_count_variations(current_tree, 1,
			       goban_window->black_variations,
			       goban_window->white_variations, NULL);
  }

  board_fill_grid(goban_window->board, goban_markup, TILE_NONE);
  gui_utils_mark_variations_on_grid(goban_markup, goban_window->board,
				    goban_window->black_variations,
				    goban_window->white_variations,
				    BLACK_50_TRANSPARENT, WHITE_50_TRANSPARENT,
				    TILE_NONE);
  sgf_utils_mark_territory_on_grid(current_tree, goban_markup,
				   (BLACK_OPAQUE
				    | GOBAN_MARKUP_GHOSTIFY_SLIGHTLY),
				   (WHITE_OPAQUE
				    | GOBAN_MARKUP_GHOSTIFY_SLIGHTLY));

  sgf_utils_get_markup(current_tree, goban_window->sgf_markup);

  gtk_goban_update(goban_window->goban, goban_window->board->grid,
		   goban_markup, goban_window->sgf_markup,
		   goban_window->sgf_board_state.last_move_x,
		   goban_window->sgf_board_state.last_move_y);
  gtk_goban_force_feedback_poll(goban_window->goban);

  if (goban_window->last_game_info_node
      != goban_window->sgf_board_state.game_info_node)
    update_game_information(goban_window);

  update_move_information(goban_window);

  comment = sgf_node_get_text_property_value(current_node, SGF_COMMENT);
  gtk_text_buffer_set_text(goban_window->text_buffer,
			   comment ? comment : "", -1);

  gtk_utils_set_menu_items_sensitive(goban_window->item_factory,
				     (goban_window->board->game == GAME_GO
				      && USER_CAN_PLAY_MOVES(goban_window)),
				     "/Play/Pass", NULL);

  gtk_utils_set_menu_items_sensitive(goban_window->item_factory,
				     current_node->child != NULL,
				     "/Go/Next Node", "/Go/Ten Nodes Forward",
				     "/Go/Variation Last Node", NULL);
  gtk_utils_set_menu_items_sensitive(goban_window->item_factory,
				     current_node->parent != NULL,
				     "/Go/Previous Node",
				     "/Go/Ten Nodes Backward",
				     "/Go/Root Node", NULL);
  gtk_utils_set_menu_items_sensitive(goban_window->item_factory,
				     current_node->next != NULL,
				     "/Go/Next Variation", NULL);
  gtk_utils_set_menu_items_sensitive(goban_window->item_factory,
				     (current_node->parent != NULL
				      && (current_node->parent->child
					  != current_node)),
				     "/Go/Previous Variation", NULL);

  goban_window->switching_x = NULL_X;
  goban_window->switching_y = NULL_Y;

  goban_window->last_displayed_node = current_node;
}


static void
update_game_information(GtkGobanWindow *goban_window)
{
  const SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;
  int have_any_player_information = 0;

  update_window_title(goban_window, game_info_node);

  if (update_player_information(game_info_node,
				goban_window->player_information + BLACK_INDEX,
				SGF_PLAYER_BLACK, SGF_BLACK_RANK,
				SGF_BLACK_TEAM))
    have_any_player_information = 1;

  if (update_player_information(game_info_node,
				goban_window->player_information + WHITE_INDEX,
				SGF_PLAYER_WHITE, SGF_WHITE_RANK,
				SGF_WHITE_TEAM))
    have_any_player_information = 1;

  if (have_any_player_information
      && !GTK_WIDGET_VISIBLE(goban_window->player_table_alignment)) {
    gtk_widget_show(goban_window->player_table_alignment);
    gtk_widget_show(goban_window->hseparator);
  }
  else if (!have_any_player_information
	   && GTK_WIDGET_VISIBLE(goban_window->player_table_alignment)) {
    gtk_widget_hide(goban_window->player_table_alignment);
    gtk_widget_hide(goban_window->hseparator);
  }

  goban_window->last_game_info_node = game_info_node;
}


static void
update_window_title(GtkGobanWindow *goban_window, const SgfNode *game_info_node)
{
  const char *game_name = NULL;
  char *basename = (goban_window->filename
		    ? g_path_get_basename(goban_window->filename) : NULL);
  char *title;

  if (game_info_node) {
    game_name = sgf_node_get_text_property_value(game_info_node,
						 SGF_GAME_NAME);
  }

  if (game_name && basename) {
    title = utils_cat_strings(NULL, game_name, " (", basename, ")", NULL);
    gtk_window_set_title(GTK_WINDOW(goban_window), title);
    utils_free(title);
  }
  else if (basename)
    gtk_window_set_title(GTK_WINDOW(goban_window), basename);

  g_free(basename);
}


static int
update_player_information(const SgfNode *game_info_node,
			  GtkPlayerInformation *player_information,
			  SgfType name_property, SgfType rank_property,
			  SgfType team_property)
{
  const char *name = NULL;
  const char *rank = NULL;
  const char *team = NULL;
  char *label_text;

  if (game_info_node) {
    name = sgf_node_get_text_property_value(game_info_node, name_property);
    rank = sgf_node_get_text_property_value(game_info_node, rank_property);
    team = sgf_node_get_text_property_value(game_info_node, team_property);
  }

  label_text = utils_duplicate_string(name ? name : "[unknown]");

  if (rank)
    label_text = utils_cat_strings(label_text, " ", rank, NULL);

  if (team)
    label_text = utils_cat_strings(label_text, " (", team, ")", NULL);

  gtk_label_set_text(player_information->player_label, label_text);
  utils_free(label_text);

  return name || rank || team;
}


static void
update_move_information(GtkGobanWindow *goban_window)
{
  const SgfNode *move_node = goban_window->sgf_board_state.last_move_node;
  char buffer[128];
  int length;

  if (move_node) {
    if (goban_window->current_tree->current_node == move_node)
      length = sprintf(buffer, "Move %d: ", goban_window->board->move_number);
    else {
      length = sprintf(buffer, "Last move: %u, ",
		       goban_window->board->move_number);
    }

    length += sgf_utils_format_node_move(goban_window->current_tree, move_node,
					 buffer + length, "B ", "W ", "Pass");
  }
  else
    length = sprintf(buffer, "Game beginning");

  if (!board_is_game_over(goban_window->board, RULE_SET_DEFAULT,
			  goban_window->sgf_board_state.color_to_play)) {
    sprintf(buffer + length, "; %s to play",
	    (goban_window->sgf_board_state.color_to_play == BLACK
	     ? "black" : "white"));
  }
  else
    strcpy(buffer + length, "; game over");

  gtk_label_set_text(GTK_LABEL(goban_window->move_info_label), buffer);
}


static void
initialize_gtp_player(GtkGobanWindow *goban_window, GtpClient *client)
{
  if (gtp_client_is_known_command(client, "set_game"))
    gtp_client_set_game(client, NULL, NULL, goban_window->current_tree->game);

  gtp_client_set_board_size(client,
			    ((GtpClientResponseCallback)
			     gtp_player_initialized_for_game),
			    goban_window,
			    goban_window->current_tree->board_width);
}


static void
gtp_player_initialized_for_game(GtpClient *player, int successful,
				GtkGobanWindow *goban_window)
{
  const SgfNode *move_node = goban_window->sgf_board_state.last_move_node;
  int color_to_play = goban_window->sgf_board_state.color_to_play;
  int client_color;

  assert(successful);

  if (player == goban_window->players[BLACK_INDEX])
    client_color = BLACK;
  else if (player == goban_window->players[WHITE_INDEX])
    client_color = WHITE;
  else
    assert(0);

  goban_window->player_initialized[COLOR_INDEX(client_color)] = TRUE;

  if (move_node) {
    gtp_client_play_move_from_sgf_node(player, NULL, NULL,
				       goban_window->current_tree, move_node);
  }

  if (client_color == color_to_play) {
    gtp_client_generate_move(player,
			     (GtpClientMoveCallback) move_has_been_generated,
			     goban_window, color_to_play);
  }
}


static void
move_has_been_played(GtkGobanWindow *goban_window)
{
  int color_to_play = goban_window->sgf_board_state.color_to_play;
  GtpClient *player = goban_window->players[COLOR_INDEX(color_to_play)];

  if (player && goban_window->player_initialized[COLOR_INDEX(color_to_play)]) {
    const SgfNode *move_node = goban_window->sgf_board_state.last_move_node;

    gtp_client_play_move_from_sgf_node(player, NULL, NULL,
				       goban_window->current_tree, move_node);

    if (!board_is_game_over(goban_window->board, RULE_SET_DEFAULT,
			    color_to_play)) {
      gtp_client_generate_move(player,
			       (GtpClientMoveCallback) move_has_been_generated,
			       goban_window, color_to_play);
    }
  }
}


static void
move_has_been_generated(GtpClient *client, int successful,
			GtkGobanWindow *goban_window,
			int color, int x, int y,
			BoardAbstractMoveData *move_data)
{
  UNUSED(client);

  if (successful) {
    if (goban_window->current_tree->game != GAME_AMAZONS) {
      sgf_utils_append_move_variation(goban_window->current_tree,
				      &goban_window->sgf_board_state,
				      color, x, y);
    }
    else {
      sgf_utils_append_move_variation(goban_window->current_tree,
				      &goban_window->sgf_board_state,
				      color, x, y, move_data->amazons);
    }

    update_children_for_new_node(goban_window);

    move_has_been_played(goban_window);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
