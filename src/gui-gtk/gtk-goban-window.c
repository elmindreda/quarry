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

#include "gtk-clock.h"
#include "gtk-control-center.h"
#include "gtk-goban.h"
#include "gtk-gtp-client-interface.h"
#include "gtk-named-vbox.h"
#include "gtk-parser-interface.h"
#include "gtk-qhbox.h"
#include "gtk-qvbox.h"
#include "gtk-utils.h"
#include "quarry-marshal.h"
#include "quarry-stock.h"
#include "gui-utils.h"
#include "time-control.h"
#include "gtp-client.h"
#include "sgf.h"
#include "board.h"
#include "game-info.h"
#include "utils.h"

#include <assert.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <string.h>


#define NAVIGATE_FAST_NUM_MOVES	10

#define IS_DISPLAYING_GAME_NODE(goban_window)				\
  ((goban_window)->game_position_board_state				\
   == &(goban_window)->sgf_board_state)

#define USER_CAN_PLAY_MOVES(goban_window)				\
  (!(goban_window)->in_game_mode					\
   || !IS_DISPLAYING_GAME_NODE(goban_window)				\
   || !((goban_window)							\
	->players[COLOR_INDEX((goban_window)				\
			      ->sgf_board_state.color_to_play)]))

#define GTP_ENGINE_CAN_PLAY_MOVES(goban_window, color)			\
  ((goban_window)->players[COLOR_INDEX(color)]				\
   && ((goban_window)->player_initialization_step[COLOR_INDEX(color)]	\
       == INITIALIZATION_COMPLETE))


enum {
  INITIALIZATION_NOT_STARTED,
  INITIALIZATION_GAME_SET,
  INITIALIZATION_BOARD_SIZE_SET,
  INITIALIZATION_TIME_LIMITS_SET,
  INITIALIZATION_FIXED_HANDICAP_SET,
  INITIALIZATION_FREE_HANDICAP_PLACED,
  INITIALIZATION_HANDICAP_SET,
  INITIALIZATION_COMPLETE
};


typedef void (* SpecialModeButtonClicked) (GtkGobanWindow *goban_window);


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

static void	 update_territory_markup(GtkGobanWindow *goban_window);

static void	 enter_special_mode(GtkGobanWindow *goban_window,
				    const gchar *hint,
				    SpecialModeButtonClicked done_callback,
				    SpecialModeButtonClicked cancel_callback);
static void	 leave_special_mode(GtkGobanWindow *goban_window);

static void	 free_handicap_mode_done(GtkGobanWindow *goban_window);
static void	 go_scoring_mode_done(GtkGobanWindow *goban_window);

static void	 play_pass_move(GtkGobanWindow *goban_window);


static void	 set_current_tree(GtkGobanWindow *goban_window,
				  SgfGameTree *sgf_tree);
static void	 reenter_current_node(GtkGobanWindow *goban_window);

static void	 cancel_amazons_move(GtkGobanWindow *goban_window);
static void	 reset_amazons_move_data(GtkGobanWindow *goban_window);

static void	 set_goban_signal_handlers(GtkGobanWindow *goban_window,
					   GCallback pointer_moved_handler,
					   GCallback goban_clicked_handler);

static GtkGobanPointerFeedback
		 playing_mode_pointer_moved(GtkGobanWindow *goban_window,
					    GtkGobanPointerData *data);
static void	 playing_mode_goban_clicked(GtkGobanWindow *goban_window,
					    GtkGobanClickData *data);

static GtkGobanPointerFeedback
		 free_handicap_mode_pointer_moved(GtkGobanWindow *goban_window,
						  GtkGobanPointerData *data);
static void	 free_handicap_mode_goban_clicked(GtkGobanWindow *goban_window,
						  GtkGobanClickData *data);

static GtkGobanPointerFeedback
		 go_scoring_mode_pointer_moved(GtkGobanWindow *goban_window,
					       GtkGobanPointerData *data);
static void	 go_scoring_mode_goban_clicked(GtkGobanWindow *goban_window,
					       GtkGobanClickData *data);

static void	 navigate_goban(GtkGobanWindow *goban_window,
				GtkGobanNavigationCommand command);

static int	 find_variation_to_switch_to(GtkGobanWindow *goban_window,
					     int x, int y,
					     SgfDirection direction);

static void	 update_children_for_new_node(GtkGobanWindow *goban_window);

static void	 update_game_information(GtkGobanWindow *goban_window);
static void	 update_window_title(GtkGobanWindow *goban_window,
				     const SgfNode *game_info_node);
static int	 update_player_information(const SgfNode *game_info_node,
					   GtkLabel *player_label,
					   SgfType name_property,
					   SgfType rank_property,
					   SgfType team_property);

static void	 update_game_specific_information
		   (GtkGobanWindow *goban_window);
static void	 update_move_information(GtkGobanWindow *goban_window);

static void	 initialize_gtp_player(GtpClient *client, int successful,
				       GtkGobanWindow *goban_window, ...);

static void	 free_handicap_has_been_placed
		   (GtkGobanWindow *goban_window,
		    BoardPositionList *handicap_stones);
static void	 move_has_been_played(GtkGobanWindow *goban_window);
static void	 move_has_been_generated(GtpClient *client, int successful,
					 GtkGobanWindow *goban_window,
					 int color, int x, int y,
					 BoardAbstractMoveData *move_data);
static void	 generate_move_via_gtp(GtkGobanWindow *goban_window);
static void	 start_clock_if_needed(GtkGobanWindow *goban_window);


static GtkWindowClass  *parent_class;

static guint		clicked_signal_id;
static guint		pointer_moved_signal_id;
static guint		goban_clicked_signal_id;


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
    { N_("/_File"),		NULL,		  NULL, 0, "<Branch>" },
    { N_("/File/_Open..."),	"<ctrl>O",	  gtk_parser_interface_present,
      0,			    "<StockItem>",  GTK_STOCK_OPEN },
    { N_("/File/"),		NULL,		  NULL, 0, "<Separator>" },
    { N_("/File/_Save"),	"<ctrl>S",	  gtk_goban_window_save,
      GTK_GOBAN_WINDOW_SAVE,	    "<StockItem>",  GTK_STOCK_SAVE },
    { N_("/File/Save _As..."),	"<shift><ctrl>S", gtk_goban_window_save,
      GTK_GOBAN_WINDOW_SAVE_AS,	    "<StockItem>",  GTK_STOCK_SAVE_AS },

    { N_("/_Play"),		NULL,		  NULL, 0, "<Branch>" },
    { N_("/Play/_Pass"),	NULL,		  play_pass_move,
      1,			    "<Item>" },

    { N_("/_Go"),		NULL,		  NULL, 0, "<Branch>" },
    { N_("/Go/_Next Node"),	"<alt>Right",	  navigate_goban,
      GOBAN_NAVIGATE_FORWARD,	    "<StockItem>",  GTK_STOCK_GO_FORWARD },
    { N_("/Go/_Previous Node"),	"<alt>Left",	  navigate_goban,
      GOBAN_NAVIGATE_BACK,	    "<StockItem>",  GTK_STOCK_GO_BACK },
    { N_("/Go/Ten Nodes _Forward"),   "<alt>Page_Down", navigate_goban,
      GOBAN_NAVIGATE_FORWARD_FAST,  "<Item>" },
    { N_("/Go/Ten Nodes _Backward"),  "<alt>Page_Up",	navigate_goban,
      GOBAN_NAVIGATE_BACK_FAST,	    "<Item>" },
    { N_("/Go/"),		NULL,		  NULL, 0, "<Separator>" },
    { N_("/Go/_Root Node"),	"<alt>Home",	  navigate_goban,
      GOBAN_NAVIGATE_ROOT,	    "<StockItem>",  GTK_STOCK_GOTO_FIRST },
    { N_("/Go/Variation _Last Node"), "<alt>End",	navigate_goban,
      GOBAN_NAVIGATE_VARIATION_END, "<StockItem>",  GTK_STOCK_GOTO_LAST },
    { N_("/Go/"),		NULL,		  NULL, 0, "<Separator>" },
    { N_("/Go/Ne_xt Variation"),      "<alt>Down",	navigate_goban,
      GOBAN_NAVIGATE_NEXT_VARIATION, "<StockItem>", GTK_STOCK_GO_DOWN },
    { N_("/Go/Pre_vious Variation"),  "<alt>Up",	navigate_goban,
      GOBAN_NAVIGATE_PREVIOUS_VARIATION, "<StockItem>", GTK_STOCK_GO_UP },
  };

  GtkWidget *goban;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *move_information_label;
  GtkWidget *mode_hint_label;
  GtkWidget *hbox;
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

  set_goban_signal_handlers(goban_window,
			    G_CALLBACK(playing_mode_pointer_moved),
			    G_CALLBACK(playing_mode_goban_clicked));
  g_signal_connect_swapped(goban, "navigate",
			   G_CALLBACK(navigate_goban), goban_window);

  /* Frame to make goban look sunken. */
  frame = gtk_utils_sink_widget(goban);

  /* Table that holds players' information and clocks. */
  table = gtk_table_new(2, 2, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(table), QUARRY_SPACING_SMALL);
  gtk_table_set_col_spacings(GTK_TABLE(table), QUARRY_SPACING_BIG);

  /* Information labels and clocks for each player. */
  for (k = 0; k < NUM_COLORS; k++) {
    GtkWidget *player_label;
    GtkWidget *game_specific_info;
    GtkWidget *clock;
    GtkWidget *named_vbox;

    player_label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(player_label), 0.0, 0.5);
    goban_window->player_labels[k] = GTK_LABEL(player_label);

    game_specific_info = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(game_specific_info), 0.0, 0.5);
    goban_window->game_specific_info[k] = GTK_LABEL(game_specific_info);

    named_vbox = gtk_utils_pack_in_box(GTK_TYPE_NAMED_VBOX,
				       QUARRY_SPACING_VERY_SMALL,
				       player_label, GTK_UTILS_FILL,
				       game_specific_info, GTK_UTILS_FILL,
				       NULL);
    gtk_named_vbox_set_label_text(GTK_NAMED_VBOX(named_vbox), 
				  (k == BLACK_INDEX ?
				   _("Black") : _("White")));
    gtk_table_attach(GTK_TABLE(table), named_vbox,
		     0, 1, OTHER_INDEX(k), OTHER_INDEX(k) + 1,
		     GTK_FILL, 0, 0, 0);

    clock = gtk_clock_new();
    gtk_table_attach(GTK_TABLE(table), gtk_utils_sink_widget(clock),
		     1, 2, OTHER_INDEX(k), OTHER_INDEX(k) + 1,
		     GTK_FILL, 0, 0, 0);

    goban_window->clocks[k] = GTK_CLOCK(clock);
  }

  /* Alignment widget to keep the table centered in sidebar. */
  goban_window->player_table_alignment = gtk_utils_align_widget(table,
								0.5, 0.5);

  /* Separate players' and move informations. */
  goban_window->players_information_hseparator = gtk_hseparator_new();

  /* Move information label. */
  move_information_label = gtk_label_new(NULL);
  goban_window->move_information_label = GTK_LABEL(move_information_label);

  goban_window->mode_information_hseparator = gtk_hseparator_new();

  /* A label with hints about special window modes. */
  mode_hint_label = gtk_label_new(NULL);
  gtk_label_set_line_wrap(GTK_LABEL(mode_hint_label), TRUE);
  gtk_label_set_justify(GTK_LABEL(mode_hint_label), GTK_JUSTIFY_CENTER);
  goban_window->mode_hint_label = GTK_LABEL(mode_hint_label);

  /* Buttons for ending specil window modes. */
  goban_window->done_button = gtk_button_new_from_stock(QUARRY_STOCK_DONE);
  goban_window->cancel_button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

  hbox = gtk_utils_pack_in_box(GTK_TYPE_HBUTTON_BOX, QUARRY_SPACING_SMALL,
			       goban_window->done_button, 0,
			       goban_window->cancel_button, 0, NULL);

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
			       goban_window->players_information_hseparator,
			       GTK_UTILS_FILL,
			       move_information_label, GTK_UTILS_FILL,
			       goban_window->mode_information_hseparator,
			       GTK_UTILS_FILL,
			       goban_window->mode_hint_label,
			       GTK_UTILS_FILL | QUARRY_SPACING_SMALL,
			       gtk_utils_align_widget(hbox, 0.5, 0.5),
			       GTK_UTILS_FILL,
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

  /* Look up here when the classes are certainly loaded. */
  clicked_signal_id	  = g_signal_lookup("clicked", GTK_TYPE_BUTTON);
  pointer_moved_signal_id = g_signal_lookup("pointer-moved", GTK_TYPE_GOBAN);
  goban_clicked_signal_id = g_signal_lookup("goban-clicked", GTK_TYPE_GOBAN);

  /* But hide special mode section again. */
  leave_special_mode(goban_window);

  goban_window->board = NULL;

  goban_window->in_game_mode	      = FALSE;
  goban_window->pending_free_handicap = 0;
  goban_window->players[BLACK_INDEX]  = NULL;
  goban_window->players[WHITE_INDEX]  = NULL;

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
				 GtpClient *white_player,
				 TimeControl *black_time_control,
				 TimeControl *white_time_control)
{
  const SgfGameTree *game_tree;
  int handicap = -1;
  GtkWidget *black_clock_frame;
  GtkWidget *white_clock_frame;

  assert(GTK_IS_GOBAN_WINDOW(goban_window));
  assert(!goban_window->in_game_mode);

  goban_window->in_game_mode		   = TRUE;
  goban_window->players[BLACK_INDEX]	   = black_player;
  goban_window->players[WHITE_INDEX]	   = white_player;
  goban_window->time_controls[BLACK_INDEX] = black_time_control;
  goban_window->time_controls[WHITE_INDEX] = white_time_control;

  goban_window->game_position_board_state = &goban_window->sgf_board_state;

  game_tree = goban_window->current_tree;
  handicap = sgf_node_get_handicap(game_tree->current_node);

  if (handicap > 0
      && !sgf_node_get_list_of_point_property_value(game_tree->current_node,
						    SGF_ADD_BLACK)) {
    goban_window->pending_free_handicap = handicap;
    goban_window->num_handicap_stones_placed = 0;

    if (!black_player) {
      gchar *hint = g_strdup_printf(_("Please set up %d (or less)\n"
				      "stones of free handicap"),
				    handicap);

      gtk_widget_set_sensitive(goban_window->done_button, FALSE);
      enter_special_mode(goban_window, hint, free_handicap_mode_done, NULL);
      g_free(hint);

      set_goban_signal_handlers(goban_window,
				G_CALLBACK(free_handicap_mode_pointer_moved),
				G_CALLBACK(free_handicap_mode_goban_clicked));
    }
  }

  if (black_player) {
    goban_window->player_initialization_step[BLACK_INDEX]
      = INITIALIZATION_NOT_STARTED;
    initialize_gtp_player(black_player, 1, goban_window);
  }

  if (white_player) {
    goban_window->player_initialization_step[WHITE_INDEX]
      = INITIALIZATION_NOT_STARTED;
    initialize_gtp_player(white_player, 1, goban_window);
  }

  if (black_time_control && white_time_control) {
    gtk_clock_initialize_for_time_control(goban_window->clocks[BLACK_INDEX],
					  black_time_control);
    gtk_clock_initialize_for_time_control(goban_window->clocks[WHITE_INDEX],
					  white_time_control);
  }
  else
    assert(!black_time_control && !white_time_control);

  black_clock_frame
    = gtk_widget_get_parent(GTK_WIDGET(goban_window->clocks[BLACK_INDEX]));
  white_clock_frame
    = gtk_widget_get_parent(GTK_WIDGET(goban_window->clocks[WHITE_INDEX]));

  gtk_utils_set_widgets_visible(black_time_control && white_time_control,
				black_clock_frame, white_clock_frame, NULL);

  if (USER_CAN_PLAY_MOVES(goban_window))
    start_clock_if_needed(goban_window);
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

  g_object_unref(goban_window->item_factory);

  if (goban_window->board)
    board_delete(goban_window->board);

  if (goban_window->players[BLACK_INDEX])
    gtk_schedule_gtp_client_deletion(goban_window->players[BLACK_INDEX]);
  if (goban_window->players[WHITE_INDEX])
    gtk_schedule_gtp_client_deletion(goban_window->players[WHITE_INDEX]);

  if (goban_window->time_controls[BLACK_INDEX])
    time_control_delete(goban_window->time_controls[BLACK_INDEX]);
  if (goban_window->time_controls[WHITE_INDEX])
    time_control_delete(goban_window->time_controls[WHITE_INDEX]);

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
  if (callback_action == GTK_GOBAN_WINDOW_SAVE && goban_window->filename) {
    /* NOTE: Not translated, a temporary message anyway. */
    static const gchar *hint_text
      = ("Note that if this file was created with another SGF editor, "
	 "there is a small chance that some information will be lost. "
	 "This warning will be removed when Quarry is more mature.");

    GtkWidget *warning_dialog
      = gtk_utils_create_message_dialog(GTK_WINDOW(goban_window),
					GTK_STOCK_DIALOG_WARNING,
					(GTK_UTILS_NO_BUTTONS
					 | GTK_UTILS_DONT_SHOW),
					hint_text,
					"Save game record back to file `%s'?",
					goban_window->filename);
    gtk_dialog_add_buttons(GTK_DIALOG(warning_dialog),
			   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			   GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(warning_dialog),
				    GTK_RESPONSE_OK);
    if (gtk_dialog_run(GTK_DIALOG(warning_dialog)) == GTK_RESPONSE_OK)
      sgf_write_file(goban_window->filename, goban_window->sgf_collection, 0);

    gtk_widget_destroy(warning_dialog);
  }
  else {
    /* "Save as..." command invoked or we don't have a filename. */
    if (!goban_window->save_as_dialog) {
      GtkWidget *file_selection = gtk_file_selection_new(_("Save As..."));

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

    if (sgf_write_file(filename, goban_window->sgf_collection, 0)) {
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
update_territory_markup(GtkGobanWindow *goban_window)
{
  int x;
  int y;
  char goban_markup[BOARD_GRID_SIZE];

  for (y = 0; y < goban_window->board->height; y++) {
    for (x = 0; x < goban_window->board->width; x++) {
      int pos = POSITION(x, y);

      if (!goban_window->dead_stones[pos])
	goban_markup[pos] = TILE_NONE;
      else
	goban_markup[pos] = GOBAN_MARKUP_GHOSTIFY;
    }
  }

  go_mark_territory_on_grid(goban_window->board,
			    goban_markup, goban_window->dead_stones,
			    BLACK_OPAQUE | GOBAN_MARKUP_GHOSTIFY,
			    WHITE_OPAQUE | GOBAN_MARKUP_GHOSTIFY);

  gtk_goban_update(goban_window->goban, NULL, goban_markup, NULL,
		   goban_window->sgf_board_state.last_move_x,
		   goban_window->sgf_board_state.last_move_y);
}



static void
enter_special_mode(GtkGobanWindow *goban_window, const gchar *hint,
		   SpecialModeButtonClicked done_callback,
		   SpecialModeButtonClicked cancel_callback)
{
  gtk_label_set_text(goban_window->mode_hint_label, hint);

  gtk_widget_show(goban_window->mode_information_hseparator);
  gtk_widget_show(GTK_WIDGET(goban_window->mode_hint_label));

  gtk_widget_show(goban_window->done_button);
  g_signal_connect_swapped(goban_window->done_button, "clicked",
			   G_CALLBACK(done_callback), goban_window);

  if (cancel_callback) {
    gtk_widget_show(goban_window->cancel_button);
    g_signal_connect_swapped(goban_window->cancel_button, "clicked",
			     G_CALLBACK(cancel_callback), goban_window);
  }
}


static void
leave_special_mode(GtkGobanWindow *goban_window)
{
  g_signal_handlers_disconnect_matched(goban_window->done_button,
				       G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
				       clicked_signal_id, 0, NULL, NULL,
				       goban_window);
  g_signal_handlers_disconnect_matched(goban_window->cancel_button,
				       G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
				       clicked_signal_id, 0, NULL, NULL,
				       goban_window);

  gtk_widget_hide(goban_window->mode_information_hseparator);
  gtk_widget_hide(GTK_WIDGET(goban_window->mode_hint_label));
  gtk_widget_hide(goban_window->done_button);
  gtk_widget_hide(goban_window->cancel_button);
}


static void
free_handicap_mode_done(GtkGobanWindow *goban_window)
{
  BoardPositionList *position_lists[NUM_ON_GRID_VALUES];

  gtk_goban_diff_against_grid(goban_window->goban, goban_window->board->grid,
			      position_lists);
  assert((position_lists[BLACK]->num_positions
	  == goban_window->num_handicap_stones_placed)
	 && position_lists[WHITE] == NULL
	 && position_lists[EMPTY] == NULL
	 && position_lists[SPECIAL_ON_GRID_VALUE] == NULL);

  free_handicap_has_been_placed(goban_window, position_lists[BLACK]);

  leave_special_mode(goban_window);
  set_goban_signal_handlers(goban_window,
			    G_CALLBACK(playing_mode_pointer_moved),
			    G_CALLBACK(playing_mode_goban_clicked));
}


static void
go_scoring_mode_done(GtkGobanWindow *goban_window)
{
  SgfGameTree *current_tree = goban_window->current_tree;
  double komi;
  char *detailed_score;
  BoardPositionList *black_territory;
  BoardPositionList *white_territory;

  assert(sgf_node_get_komi(goban_window->sgf_board_state.game_info_node,
			   &komi));
  go_score_game(goban_window->board, goban_window->dead_stones, komi,
		NULL, &detailed_score, &black_territory, &white_territory);

  sgf_node_add_text_property(current_tree->current_node, current_tree,
			     SGF_COMMENT, detailed_score);
  sgf_node_add_list_of_point_property(current_tree->current_node, current_tree,
				      SGF_BLACK_TERRITORY, black_territory);
  sgf_node_add_list_of_point_property(current_tree->current_node, current_tree,
				      SGF_WHITE_TERRITORY, white_territory);

  g_free(goban_window->dead_stones);

  reenter_current_node(goban_window);

  leave_special_mode(goban_window);
  set_goban_signal_handlers(goban_window,
			    G_CALLBACK(playing_mode_pointer_moved),
			    G_CALLBACK(playing_mode_goban_clicked));
}


static void
set_current_tree(GtkGobanWindow *goban_window, SgfGameTree *sgf_tree)
{
  if (!goban_window->board && GAME_IS_SUPPORTED(sgf_tree->game)) {
    GtkLabel **game_specific_info = goban_window->game_specific_info;

    goban_window->board = board_new(sgf_tree->game,
				    sgf_tree->board_width,
				    sgf_tree->board_height);

    gtk_utils_set_widgets_visible(goban_window->board->game != GAME_AMAZONS,
				  game_specific_info[BLACK_INDEX],
				  game_specific_info[WHITE_INDEX], NULL);
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
reenter_current_node(GtkGobanWindow *goban_window)
{
  SgfGameTree *current_tree = goban_window->current_tree;

  if (current_tree->current_node->parent) {
    sgf_utils_go_up_in_tree(current_tree, 1, &goban_window->sgf_board_state);
    sgf_utils_go_down_in_tree(current_tree, 1, &goban_window->sgf_board_state);
  }
  else {
    sgf_utils_enter_tree(current_tree, goban_window->board,
			 &goban_window->sgf_board_state);
  }

  goban_window->last_displayed_node = NULL;
  update_children_for_new_node(goban_window);
}


static void
play_pass_move(GtkGobanWindow *goban_window)
{
  assert(goban_window->board->game == GAME_GO
	 && USER_CAN_PLAY_MOVES(goban_window));

  sgf_utils_append_move_variation(goban_window->current_tree,
				  &goban_window->sgf_board_state,
				  goban_window->sgf_board_state.color_to_play,
				  PASS_X, PASS_Y);

  update_children_for_new_node(goban_window);

  if (goban_window->in_game_mode && IS_DISPLAYING_GAME_NODE(goban_window))
    move_has_been_played(goban_window);
}


static void
cancel_amazons_move(GtkGobanWindow *goban_window)
{
  if (goban_window->amazons_move_stage == SHOOTING_ARROW) {
    gtk_goban_set_overlay_data(goban_window->goban, 1, NULL,
			       TILE_NONE, TILE_NONE);
  }

  if (goban_window->amazons_move_stage != SELECTING_QUEEN) {
    gtk_goban_set_overlay_data(goban_window->goban, 0, NULL,
			       TILE_NONE, TILE_NONE);
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



static void
set_goban_signal_handlers(GtkGobanWindow *goban_window,
			  GCallback pointer_moved_handler,
			  GCallback goban_clicked_handler)
{
  g_signal_handlers_disconnect_matched(goban_window->goban,
				       G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
				       pointer_moved_signal_id, 0, NULL, NULL,
				       goban_window);
  g_signal_handlers_disconnect_matched(goban_window->goban,
				       G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
				       goban_clicked_signal_id, 0, NULL, NULL,
				       goban_window);

  g_signal_connect_swapped(goban_window->goban, "pointer-moved",
			   pointer_moved_handler, goban_window);
  g_signal_connect_swapped(goban_window->goban, "goban-clicked",
			   goban_clicked_handler, goban_window);
}


static GtkGobanPointerFeedback
playing_mode_pointer_moved(GtkGobanWindow *goban_window,
			   GtkGobanPointerData *data)
{
  if (USER_CAN_PLAY_MOVES(goban_window)) {
    int color_to_play = goban_window->sgf_board_state.color_to_play;

    switch (data->button) {
    case 0:
      if (!(data->modifiers & GDK_SHIFT_MASK)) {
	if (color_to_play == EMPTY)
	  break;

	if (goban_window->board->game != GAME_AMAZONS) {
	  if (board_is_legal_move(goban_window->board, RULE_SET_DEFAULT,
				  color_to_play, data->x, data->y))
	    return GOBAN_FEEDBACK_MOVE + COLOR_INDEX(color_to_play);
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
	      return GOBAN_FEEDBACK_MOVE + COLOR_INDEX(color_to_play);
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
    if (data->feedback_tile != GOBAN_TILE_DONT_CHANGE
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
	int pos = POSITION(data->x, data->y);

	if (goban_window->amazons_move_stage == SELECTING_QUEEN) {
	  goban_window->amazons_move_stage  = MOVING_QUEEN;

	  gtk_goban_set_overlay_data(goban_window->goban, 0,
				     board_position_list_new(&pos, 1),
				     (STONE_50_TRANSPARENT
				      + COLOR_INDEX(color_to_play)),
				     GOBAN_TILE_DONT_CHANGE);

	  return;
	}
	else if (goban_window->amazons_move_stage == MOVING_QUEEN) {
	  goban_window->amazons_move_stage  = SHOOTING_ARROW;

	  gtk_goban_set_overlay_data(goban_window->goban, 1,
				     board_position_list_new(&pos, 1),
				     (STONE_25_TRANSPARENT
				      + COLOR_INDEX(color_to_play)),
				     GOBAN_TILE_DONT_CHANGE);

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

      update_children_for_new_node(goban_window);

      if (goban_window->in_game_mode && IS_DISPLAYING_GAME_NODE(goban_window))
	move_has_been_played(goban_window);
    }

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
      update_children_for_new_node(goban_window);
    }
    else
      cancel_amazons_move(goban_window);

    break;
  }
}


static GtkGobanPointerFeedback
free_handicap_mode_pointer_moved(GtkGobanWindow *goban_window,
				 GtkGobanPointerData *data)
{
  switch (data->button) {
  case 0:
    if (!(data->modifiers & GDK_SHIFT_MASK)) {
      int contents = gtk_goban_get_grid_contents(goban_window->goban,
						 data->x, data->y);

      if (contents == BLACK
	  || (contents == EMPTY
	      && (goban_window->num_handicap_stones_placed
		  < goban_window->pending_free_handicap)))
	return GOBAN_FEEDBACK_ADD_BLACK_OR_REMOVE;
    }

    break;

  case 1:
    return GOBAN_FEEDBACK_PRESS_DEFAULT;
  }

  return GOBAN_FEEDBACK_NONE;
}


static void
free_handicap_mode_goban_clicked(GtkGobanWindow *goban_window,
				 GtkGobanClickData *data)
{
  if (data->button == 1
      && data->feedback_tile != GOBAN_TILE_DONT_CHANGE
      && !(data->modifiers & GDK_SHIFT_MASK)) {
    int contents = gtk_goban_get_grid_contents(goban_window->goban,
					       data->x, data->y);
    int pos = POSITION(data->x, data->y);
    BoardPositionList *position_list = board_position_list_new(&pos, 1);

    if (contents == EMPTY) {
      contents = BLACK;
      goban_window->num_handicap_stones_placed++;
    }
    else if (contents == BLACK) {
      contents = EMPTY;
      goban_window->num_handicap_stones_placed--;
    }
    else
      assert(0);

    gtk_goban_set_contents(goban_window->goban, position_list,
			   contents, GOBAN_TILE_DONT_CHANGE);
    board_position_list_delete(position_list);

    gtk_widget_set_sensitive(goban_window->done_button,
			     goban_window->num_handicap_stones_placed >= 2);
  }
}


static GtkGobanPointerFeedback
go_scoring_mode_pointer_moved(GtkGobanWindow *goban_window,
			      GtkGobanPointerData *data)
{
  BoardPositionList * (* const get_stones) (Board *board, int x, int y)
    = (data->modifiers & GDK_SHIFT_MASK
       ? go_get_string_stones : go_get_logically_dead_stones);
  int pos = POSITION(data->x, data->y);

  switch (data->button) {
  case 0:
    {
      BoardPositionList *stones = get_stones(goban_window->board,
					     data->x, data->y);

      if (stones) {
	int color = goban_window->board->grid[pos];

	data->feedback_position_list = stones;

	if (!goban_window->dead_stones[pos]) {
	  return ((GOBAN_FEEDBACK_GHOST + COLOR_INDEX(OTHER_COLOR(color)))
		  * GOBAN_FEEDBACK_MARKUP_FACTOR);
	}
	else {
	  return ((GOBAN_FEEDBACK_THICK_GHOST + COLOR_INDEX(color))
		  + (GOBAN_FEEDBACK_FORCE_TILE_NONE
		     * GOBAN_FEEDBACK_MARKUP_FACTOR));
	}
      }
    }

    break;

  case 1:
    {
      BoardPositionList *stones = get_stones(goban_window->board,
					     data->x, data->y);

      if (stones) {
	int color = goban_window->board->grid[POSITION(data->x, data->y)];

	data->feedback_position_list = stones;

	if (!goban_window->dead_stones[pos]) {
	  return ((GOBAN_FEEDBACK_THICK_GHOST + COLOR_INDEX(color))
		  + ((GOBAN_FEEDBACK_GHOST + COLOR_INDEX(OTHER_COLOR(color)))
		     * GOBAN_FEEDBACK_MARKUP_FACTOR));
	}
	else {
	  return ((GOBAN_FEEDBACK_OPAQUE + COLOR_INDEX(color))
		  + (GOBAN_FEEDBACK_FORCE_TILE_NONE
		     * GOBAN_FEEDBACK_MARKUP_FACTOR));
	}
      }
    }

    break;
  }

  return GOBAN_FEEDBACK_NONE;
}


static void
go_scoring_mode_goban_clicked(GtkGobanWindow *goban_window,
			      GtkGobanClickData *data)
{
  if (data->button == 1) {
    BoardPositionList * (* const get_stones) (Board *board, int x, int y)
      = (data->modifiers & GDK_SHIFT_MASK
	 ? go_get_string_stones : go_get_logically_dead_stones);
    BoardPositionList *stones = get_stones(goban_window->board,
					   data->x, data->y);

    if (stones) {
      int pos = POSITION(data->x, data->y);

      board_position_list_mark_on_grid(stones, goban_window->dead_stones,
				       !goban_window->dead_stones[pos]);
      board_position_list_delete(stones);

      update_territory_markup(goban_window);
    }
  }
}


static void
navigate_goban(GtkGobanWindow *goban_window, GtkGobanNavigationCommand command)
{
  SgfGameTree *current_tree = goban_window->current_tree;
  SgfNode *current_node = current_tree->current_node;

  /* Don't do anything if navigating the goban doesn't change
   * displayed node.
   */
  switch (command) {
  case GOBAN_NAVIGATE_BACK:
  case GOBAN_NAVIGATE_BACK_FAST:
  case GOBAN_NAVIGATE_ROOT:
    if (!current_node->parent)
      return;

    break;

  case GOBAN_NAVIGATE_FORWARD:
  case GOBAN_NAVIGATE_FORWARD_FAST:
  case GOBAN_NAVIGATE_VARIATION_END:
    if (!current_node->child)
      return;

    break;

  case GOBAN_NAVIGATE_PREVIOUS_VARIATION:
    if (!current_node->parent || current_node->parent->child == current_node)
      return;

    break;

  case GOBAN_NAVIGATE_NEXT_VARIATION:
    if (!current_node->next)
      return;

    break;

  default:
    return;
  }

  if (goban_window->in_game_mode && IS_DISPLAYING_GAME_NODE(goban_window)) {
    /* Goban window is going to display something other than the game
     * position node.
     */
    goban_window->game_position_node = current_node;
    goban_window->game_position_board
      = board_duplicate_without_stacks(goban_window->board);

    goban_window->game_position_board_state
      = &goban_window->game_position_board_state_holder;
    memcpy(&goban_window->game_position_board_state_holder,
	   &goban_window->sgf_board_state, sizeof(SgfBoardState));
  }

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
  }

  if (goban_window->in_game_mode
      && !IS_DISPLAYING_GAME_NODE(goban_window)
      && goban_window->game_position_node == current_tree->current_node) {
    /* The goban window displayed something other than the game
     * position node, but has just navigated back to that node.
     */
    board_delete(goban_window->game_position_board);
    goban_window->game_position_board_state = &goban_window->sgf_board_state;
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
				    MIXED_50_TRANSPARENT);
  sgf_utils_mark_territory_on_grid(current_tree, goban_markup,
				   (BLACK_OPAQUE
				    | GOBAN_MARKUP_GHOSTIFY),
				   (WHITE_OPAQUE
				    | GOBAN_MARKUP_GHOSTIFY));

  sgf_utils_get_markup(current_tree, goban_window->sgf_markup);

  gtk_goban_update(goban_window->goban, goban_window->board->grid,
		   goban_markup, goban_window->sgf_markup,
		   goban_window->sgf_board_state.last_move_x,
		   goban_window->sgf_board_state.last_move_y);
  gtk_goban_force_feedback_poll(goban_window->goban);

  if (goban_window->last_game_info_node
      != goban_window->sgf_board_state.game_info_node)
    update_game_information(goban_window);

  update_game_specific_information(goban_window);
  update_move_information(goban_window);

  comment = sgf_node_get_text_property_value(current_node, SGF_COMMENT);
  gtk_text_buffer_set_text(goban_window->text_buffer,
			   comment ? comment : "", -1);

  gtk_utils_set_menu_items_sensitive(goban_window->item_factory,
				     (goban_window->board->game == GAME_GO
				      && USER_CAN_PLAY_MOVES(goban_window)),
				     _("/Play/Pass"), NULL);

  gtk_utils_set_menu_items_sensitive(goban_window->item_factory,
				     current_node->child != NULL,
				     _("/Go/Next Node"),
				     _("/Go/Ten Nodes Forward"),
				     _("/Go/Variation Last Node"), NULL);
  gtk_utils_set_menu_items_sensitive(goban_window->item_factory,
				     current_node->parent != NULL,
				     _("/Go/Previous Node"),
				     _("/Go/Ten Nodes Backward"),
				     _("/Go/Root Node"), NULL);
  gtk_utils_set_menu_items_sensitive(goban_window->item_factory,
				     current_node->next != NULL,
				     _("/Go/Next Variation"), NULL);
  gtk_utils_set_menu_items_sensitive(goban_window->item_factory,
				     (current_node->parent != NULL
				      && (current_node->parent->child
					  != current_node)),
				     _("/Go/Previous Variation"), NULL);

  goban_window->switching_x = NULL_X;
  goban_window->switching_y = NULL_Y;

  goban_window->last_displayed_node = current_node;
}


static void
update_game_information(GtkGobanWindow *goban_window)
{
  const SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;
  int have_any_player_information = (goban_window->board->game
				     != GAME_AMAZONS);

  update_window_title(goban_window, game_info_node);

  if (update_player_information(game_info_node,
				goban_window->player_labels[BLACK_INDEX],
				SGF_PLAYER_BLACK, SGF_BLACK_RANK,
				SGF_BLACK_TEAM))
    have_any_player_information = 1;

  if (update_player_information(game_info_node,
				goban_window->player_labels[WHITE_INDEX],
				SGF_PLAYER_WHITE, SGF_WHITE_RANK,
				SGF_WHITE_TEAM))
    have_any_player_information = 1;

  gtk_utils_set_widgets_visible(have_any_player_information,
				goban_window->player_table_alignment,
				goban_window->players_information_hseparator,
				NULL);

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
			  GtkLabel *player_label,
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

  label_text = utils_duplicate_string(name ? name : _("[unknown]"));

  if (rank)
    label_text = utils_cat_strings(label_text, " ", rank, NULL);

  if (team)
    label_text = utils_cat_strings(label_text, " (", team, ")", NULL);

  gtk_label_set_text(player_label, label_text);
  utils_free(label_text);

  return name || rank || team;
}


static void
update_game_specific_information(GtkGobanWindow *goban_window)
{
  const Board *board = goban_window->board;
  gchar *black_string;
  gchar *white_string;

  if (board->game == GAME_GO) {
    const SgfNode *game_info_node
      = goban_window->sgf_board_state.game_info_node;
    double komi;

    black_string
      = g_strdup_printf(ngettext("%d capture", "%d captures",
				 board->data.go.prisoners[BLACK_INDEX]),
			board->data.go.prisoners[BLACK_INDEX]);

    if (game_info_node
	&& sgf_node_get_komi(game_info_node, &komi) && komi != 0.0) {
      white_string
	= g_strdup_printf(ngettext("%d capture %c %.*f komi",
				   "%d captures %c %.*f komi",
				   board->data.go.prisoners[WHITE_INDEX]),
			  board->data.go.prisoners[WHITE_INDEX],
			  (komi > 0.0 ? '+' : '-'),
			  ((int) floor(komi * 100.0 + 0.5) % 10 == 0 ? 1 : 2),
			  fabs(komi));
    }
    else {
      white_string
	= g_strdup_printf(ngettext("%d capture", "%d captures",
				   board->data.go.prisoners[WHITE_INDEX]),
			  board->data.go.prisoners[WHITE_INDEX]);
    }
  }
  else if (board->game == GAME_OTHELLO) {
    int num_black_disks;
    int num_white_disks;

    othello_count_disks(board, &num_black_disks, &num_white_disks);
    black_string = g_strdup_printf(ngettext("%d disk", "%d disks",
					    num_black_disks),
				   num_black_disks);
    white_string = g_strdup_printf(ngettext("%d disk", "%d disks",
					    num_white_disks),
				   num_white_disks);
  }
  else
    return;

  gtk_label_set_text(goban_window->game_specific_info[BLACK_INDEX],
		     black_string);
  gtk_label_set_text(goban_window->game_specific_info[WHITE_INDEX],
		     white_string);

  g_free(black_string);
  g_free(white_string);
}


static void
update_move_information(GtkGobanWindow *goban_window)
{
  const SgfNode *move_node = goban_window->sgf_board_state.last_move_node;
  char buffer[256];
  int length;

  if (move_node) {
    if (goban_window->current_tree->current_node == move_node) {
      length = sprintf(buffer, _("Move %d: "),
		       goban_window->board->move_number);
    }
    else {
      length = sprintf(buffer, _("Last move: %u, "),
		       goban_window->board->move_number);
    }

    length += sgf_utils_format_node_move(goban_window->current_tree, move_node,
					 buffer + length,
					 _("B "), _("W "), _("pass"));
  }
  else
    length = sprintf(buffer, _("Game beginning"));

  if (!board_is_game_over(goban_window->board, RULE_SET_DEFAULT,
			  goban_window->sgf_board_state.color_to_play)) {
    strcpy(buffer + length,
	    (goban_window->sgf_board_state.color_to_play == BLACK
	     ? _("; black to play") : _("; white to play")));
  }
  else
    strcpy(buffer + length, _("; game over"));

  gtk_label_set_text(goban_window->move_information_label, buffer);
}


static void
initialize_gtp_player(GtpClient *client, int successful,
		      GtkGobanWindow *goban_window, ...)
{
  const SgfGameTree *game_tree = goban_window->current_tree;
  const SgfNode *root_node = game_tree->root;
  int client_color = (client == goban_window->players[BLACK_INDEX]
		      ? BLACK : WHITE);
  int client_color_index = COLOR_INDEX(client_color);
  int *initialization_step = (goban_window->player_initialization_step
			      + client_color_index);

  /* FIXME */
  assert(successful);

  /* These special cases are needed to avoid nasty `goto's in `switch'
   * block below.
   */
  if (*initialization_step == INITIALIZATION_FIXED_HANDICAP_SET) {
    /* FIXME: Validate handicap position. */
    *initialization_step = INITIALIZATION_HANDICAP_SET;
  }
  else if (*initialization_step == INITIALIZATION_FREE_HANDICAP_PLACED) {
    va_list arguments;

    va_start(arguments, goban_window);
    free_handicap_has_been_placed(goban_window,
				  va_arg(arguments, BoardPositionList *));
    va_end(arguments);

    *initialization_step = INITIALIZATION_HANDICAP_SET;
  }

  /* Note that `case' branches here often fall through to next ones.
   * Often certain initializations are just not needed at all.
   */
  switch (*initialization_step) {
  case INITIALIZATION_NOT_STARTED:
    if (gtp_client_is_known_command(client, "set_game")) {
      *initialization_step = INITIALIZATION_GAME_SET;
      gtp_client_set_game(client,
			  (GtpClientResponseCallback) initialize_gtp_player,
			  goban_window, game_tree->game);
      break;
    }

  case  INITIALIZATION_GAME_SET:
    *initialization_step = INITIALIZATION_BOARD_SIZE_SET;

    assert(game_tree->board_width == game_tree->board_height);
    gtp_client_set_board_size(client,
			      ((GtpClientResponseCallback)
			       initialize_gtp_player),
			      goban_window, game_tree->board_width);
    break;

  case INITIALIZATION_BOARD_SIZE_SET:
    {
      TimeControl *time_control =
	goban_window->time_controls[client_color_index];

      if (time_control) {
	*initialization_step = INITIALIZATION_TIME_LIMITS_SET;
	gtp_client_send_time_settings(client,
				      ((GtpClientResponseCallback)
				       initialize_gtp_player),
				      goban_window,
				      time_control->main_time,
				      time_control->overtime_length,
				      time_control->moves_per_overtime);
	break;
      }
    }

  case INITIALIZATION_TIME_LIMITS_SET:
    if (game_tree->game == GAME_GO) {
      int handicap = sgf_node_get_handicap(root_node);

      if (handicap > 0) {
	gboolean is_fixed_handicap = FALSE;
	const BoardPositionList *handicap_stones = NULL;

	handicap_stones
	  = sgf_node_get_list_of_point_property_value(root_node,
						      SGF_ADD_BLACK);
	if (handicap_stones) {
	  BoardPositionList *fixed_handicap_stones
	    = go_get_fixed_handicap_stones(game_tree->board_width,
					   game_tree->board_height,
					   handicap);

	  if (board_position_lists_are_equal(fixed_handicap_stones,
					     handicap_stones))
	    is_fixed_handicap = TRUE;

	  board_position_list_delete(fixed_handicap_stones);
	}

	if (is_fixed_handicap) {
	  *initialization_step = INITIALIZATION_FIXED_HANDICAP_SET;
	  gtp_client_set_fixed_handicap(client,
					((GtpClientResponseCallback)
					 initialize_gtp_player),
					goban_window, handicap);
	  break;
	}
	else {
	  if (client_color == BLACK) {
	    *initialization_step = INITIALIZATION_FREE_HANDICAP_PLACED;
	    gtp_client_place_free_handicap(client,
					   ((GtpClientFreeHandicapCallback)
					    initialize_gtp_player),
					   goban_window, handicap);
	    break;
	  }
	  else {
	    /* Note that if `handicap_stones' are not set, then free
	     * handicap placement has not been performed yet by the
	     * black player (either another engine or the user).  In
	     * this case we just skip this step for now.  When the
	     * placement is determined, handicap will be set for this
	     * client from free_handicap_has_been_placed().
	     */
	    if (handicap_stones) {
	      gtp_client_set_free_handicap(client, NULL, NULL,
					   handicap_stones);
	    }
	  }
	}
      }
    }

  case INITIALIZATION_HANDICAP_SET:
    if (game_tree->game == GAME_GO) {
      double komi;

      if (!sgf_node_get_komi(game_tree->current_node, &komi)) {
	/* Recently suggested default komi is 6.5, but if a game lacks
	 * `KM' property it is more likely to be old.
	 */
	komi = (sgf_node_get_handicap(root_node) > 0 ? 0.5 : 5.5);
      }

      gtp_client_set_komi(client, NULL, NULL, komi);
    }

    *initialization_step = INITIALIZATION_COMPLETE;

    {
      const SgfNode *node;

      for (node = root_node; node; node = node->current_variation) {
	if (IS_STONE(node->move_color)) {
	  gtp_client_play_move_from_sgf_node(client, NULL, NULL,
					     game_tree, node);
	}
      }
    }

    if (client_color
	== goban_window->game_position_board_state->color_to_play) {
      generate_move_via_gtp(goban_window);
      start_clock_if_needed(goban_window);
    }

    break;

  default:
    /* Must never happen. */
    assert(0);
  }
}


static void
free_handicap_has_been_placed(GtkGobanWindow *goban_window,
			      BoardPositionList *handicap_stones)
{
  sgf_utils_add_free_handicap_stones(goban_window->current_tree,
				     handicap_stones);

  reenter_current_node(goban_window);
  assert(goban_window->game_position_board_state->color_to_play == WHITE);

  if (GTP_ENGINE_CAN_PLAY_MOVES(goban_window, WHITE)) {
    /* The engine is initialized, but since free handicap placement
     * only became known at this point, the engine doesn't know about
     * it yet.
     */
    gtp_client_set_free_handicap(goban_window->players[WHITE_INDEX],
				 NULL, NULL, handicap_stones);
    generate_move_via_gtp(goban_window);
  }

  start_clock_if_needed(goban_window);
}


static void
move_has_been_played(GtkGobanWindow *goban_window)
{
  const SgfNode *move_node
    = goban_window->game_position_board_state->last_move_node;
  int color_to_play = goban_window->game_position_board_state->color_to_play;

  if (goban_window->time_controls[COLOR_INDEX(move_node->move_color)])
    gtk_clock_stop(goban_window->clocks[COLOR_INDEX(move_node->move_color)]);

  if (GTP_ENGINE_CAN_PLAY_MOVES(goban_window,
				OTHER_COLOR(move_node->move_color))) {
    GtpClient *other_player
      = goban_window->players[COLOR_INDEX(OTHER_COLOR(move_node->move_color))];

    /* Other player is a GTP engine which is already initialized.
     * Inform it about the move that has just been played.
     */
    gtp_client_play_move_from_sgf_node(other_player, NULL, NULL,
				       goban_window->current_tree, move_node);
  }

  if (!board_is_game_over(goban_window->board, RULE_SET_DEFAULT,
			  color_to_play)) {
    if (GTP_ENGINE_CAN_PLAY_MOVES(goban_window, color_to_play)) {
      /* If the next move is to be played by a GTP engine and the engine
       * is ready, ask for a move now.
       */
      generate_move_via_gtp(goban_window);
    }
    else if (goban_window->players[COLOR_INDEX(color_to_play)])
      return;

    start_clock_if_needed(goban_window);
  }
  else {
    if (goban_window->board->game == GAME_GO) {
      goban_window->dead_stones = g_malloc(BOARD_GRID_SIZE * sizeof(char));
      board_fill_grid(goban_window->board, goban_window->dead_stones, 0);

      enter_special_mode(goban_window,
			 _("Please select dead stones\nto score the game"),
			 go_scoring_mode_done, NULL); 
      set_goban_signal_handlers(goban_window,
				G_CALLBACK(go_scoring_mode_pointer_moved),
				G_CALLBACK(go_scoring_mode_goban_clicked));

      update_territory_markup(goban_window);
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
    SgfGameTree *current_tree = goban_window->current_tree;
    SgfGameTreeState tree_state;

    if (!IS_DISPLAYING_GAME_NODE(goban_window)) {
      sgf_game_tree_set_state(current_tree,
			      goban_window->game_position_board,
			      goban_window->game_position_node,
			      &tree_state);
    }

    /* FIXME: Validate move and alert if it is illegal. */

    if (goban_window->current_tree->game != GAME_AMAZONS) {
      sgf_utils_append_move_variation(current_tree,
				      goban_window->game_position_board_state,
				      color, x, y);
    }
    else {
      sgf_utils_append_move_variation(current_tree,
				      goban_window->game_position_board_state,
				      color, x, y, move_data->amazons);
    }

    if (IS_DISPLAYING_GAME_NODE(goban_window))
      update_children_for_new_node(goban_window);
    else {
      goban_window->game_position_node = current_tree->current_node;
      sgf_game_tree_set_state(current_tree,
			      tree_state.board, tree_state.current_node, NULL);
    }

    move_has_been_played(goban_window);
  }
}


static void
generate_move_via_gtp(GtkGobanWindow *goban_window)
{
  int color_to_play = goban_window->game_position_board_state->color_to_play;
  int color_to_play_index = COLOR_INDEX(color_to_play);
  TimeControl *time_control = goban_window->time_controls[color_to_play_index];

  if (time_control) {
    int moves_left;
    double seconds_left = time_control_get_time_left(time_control,
						     &moves_left);

    /* FIXME: OUT_OF_TIME shouldn't really happen here. */
    if (seconds_left != NO_TIME_LIMITS && seconds_left != OUT_OF_TIME) {
      gtp_client_send_time_left(goban_window->players[color_to_play_index],
				NULL, NULL, color_to_play,
				floor(seconds_left + 0.5), moves_left);
    }
  }

  gtp_client_generate_move(goban_window->players[color_to_play_index],
			   (GtpClientMoveCallback) move_has_been_generated,
			   goban_window, color_to_play);
}


static void
start_clock_if_needed(GtkGobanWindow *goban_window)
{
  int color_to_play_index
    = COLOR_INDEX(goban_window->game_position_board_state->color_to_play);

  if (goban_window->time_controls[color_to_play_index]) {
    gtk_clock_start(goban_window->clocks[color_to_play_index],
		    goban_window->time_controls[color_to_play_index]);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
