/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005 Paul Pogonyshev                  *
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
#include "gtk-configuration.h"
#include "gtk-control-center.h"
#include "gtk-game-info-dialog.h"
#include "gtk-goban.h"
#include "gtk-gtp-client-interface.h"
#include "gtk-help.h"
#include "gtk-named-vbox.h"
#include "gtk-new-game-dialog.h"
#include "gtk-parser-interface.h"
#include "gtk-preferences.h"
#include "gtk-qhbox.h"
#include "gtk-qvbox.h"
#include "gtk-sgf-tree-view.h"
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
   || !IS_DISPLAYING_GAME_NODE (goban_window)				\
   || !((goban_window)							\
	->players[COLOR_INDEX ((goban_window)				\
			       ->sgf_board_state.color_to_play)]))

#define GTP_ENGINE_CAN_PLAY_MOVES(goban_window, color)			\
  ((goban_window)->players[COLOR_INDEX (color)]				\
   && ((goban_window)->player_initialization_step[COLOR_INDEX (color)]	\
       == INITIALIZATION_COMPLETE))

#define USER_IS_TO_PLAY(goban_window)					\
  (!((goban_window)							\
     ->players[COLOR_INDEX ((goban_window)				\
			    ->game_position_board_state			\
			    ->color_to_play)]))


#define IS_IN_SPECIAL_MODE(goban_window)				\
  GTK_WIDGET_VISIBLE ((goban_window)->mode_information_vbox)


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


enum {
  GTK_GOBAN_WINDOW_FIND_NEXT = 1,
  GTK_GOBAN_WINDOW_FIND_PREVIOUS
};


enum {
  GTK_GOBAN_WINDOW_HIDE_CHILD = FALSE,
  GTK_GOBAN_WINDOW_SHOW_CHILD = TRUE,
  GTK_GOBAN_WINDOW_TOGGLE_CHILD
};


static void	 gtk_goban_window_class_init (GtkGobanWindowClass *class);
static void	 gtk_goban_window_init (GtkGobanWindow *goban_window);

static void	 gtk_goban_window_destroy (GtkObject *object);
static void	 gtk_goban_window_finalize (GObject *object);

static void	 force_minimal_width (GtkWidget *label,
				      GtkRequisition *requisition);

static void	 leave_game_mode (GtkGobanWindow *goban_window);

static void	 gtk_goban_window_save (GtkGobanWindow *goban_window,
					guint callback_action);
static void	 save_file_as_response (GtkFileSelection *dialog,
					gint response_id,
					GtkGobanWindow *goban_window);

static void	 show_find_dialog (GtkGobanWindow *goban_window);
static void	 find_dialog_response (GtkGobanWindow *goban_window,
				       gint response_id);
static void	 find_dialog_parameters_changed (GtkGobanWindow *goban_window);
static gboolean	 do_find_text (GtkGobanWindow *goban_window,
			       guint callback_action);
static char *	 strstr_whole_word (const char *haystack, const char *needle);
static char *	 strrstr_whole_word (const char *haystack, const char *needle);
static gboolean  char_is_word_constituent (const gchar *character);
inline static gchar *
		 get_normalized_text (const gchar *text, gint length,
				      gboolean case_sensitive);
static gint	 get_offset_in_original_text (const gchar *text, gint length,
					      gint normalized_text_offset,
					      gboolean case_sensitive,
					      gint first_guess);

static void	 show_game_information_dialog (GtkGobanWindow *goban_window);
static void	 game_info_dialog_property_changed
		   (GtkGobanWindow *goban_window, SgfType sgf_property_type);

static void	 show_preferences_dialog (void);

static void	 show_or_hide_main_toolbar (GtkGobanWindow *goban_window);
static void	 show_or_hide_navigation_toolbar
		   (GtkGobanWindow *goban_window);
static void	 show_or_hide_game_action_buttons
		   (GtkGobanWindow *goban_window);
static void	 show_or_hide_sgf_tree_view (GtkGobanWindow *goban_window,
					     guint callback_action);

static void	 show_sgf_tree_view_automatically
		   (GtkGobanWindow *goban_window, const SgfNode *sgf_node);

static void	 show_about_dialog (void);
static void	 show_help_contents (void);

static void	 update_territory_markup (GtkGobanWindow *goban_window);

static void	 enter_special_mode (GtkGobanWindow *goban_window,
				     const gchar *hint,
				     SpecialModeButtonClicked done_callback,
				     SpecialModeButtonClicked cancel_callback);
static void	 leave_special_mode (GtkGobanWindow *goban_window);

static void	 engine_has_scored (GtpClient *client, int successful,
				    GtkGobanWindow *goban_window,
				    GtpStoneStatus status,
				    BoardPositionList *dead_stones);
static void	 cancel_scoring (GtkProgressDialog *progress_dialog,
				 GtkGobanWindow *goban_window);
static void	 enter_scoring_mode (GtkGobanWindow *goban_window);

static void	 go_scoring_mode_done (GtkGobanWindow *goban_window);
static void	 free_handicap_mode_done (GtkGobanWindow *goban_window);


static void	 play_pass_move (GtkGobanWindow *goban_window);
static void	 play_resign (GtkGobanWindow *goban_window);
static void	 do_resign_game (GtkGobanWindow *goban_window);


static void	 set_current_tree (GtkGobanWindow *goban_window,
				   SgfGameTree *sgf_tree);
static void	 reenter_current_node (GtkGobanWindow *goban_window);
static void	 about_to_change_node (GtkGobanWindow *goban_window);
static void	 just_changed_node (GtkGobanWindow *goban_window);

static void	 cancel_amazons_move (GtkGobanWindow *goban_window);
static void	 reset_amazons_move_data (GtkGobanWindow *goban_window);

static void	 set_goban_signal_handlers (GtkGobanWindow *goban_window,
					    GCallback pointer_moved_handler,
					    GCallback goban_clicked_handler);

static GtkGobanPointerFeedback
		 playing_mode_pointer_moved (GtkGobanWindow *goban_window,
					     GtkGobanPointerData *data);
static void	 playing_mode_goban_clicked (GtkGobanWindow *goban_window,
					     GtkGobanClickData *data);

static GtkGobanPointerFeedback
		 free_handicap_mode_pointer_moved
		   (GtkGobanWindow *goban_window, GtkGobanPointerData *data);
static void	 free_handicap_mode_goban_clicked
		   (GtkGobanWindow *goban_window, GtkGobanClickData *data);

static GtkGobanPointerFeedback
		 go_scoring_mode_pointer_moved (GtkGobanWindow *goban_window,
						GtkGobanPointerData *data);
static void	 go_scoring_mode_goban_clicked (GtkGobanWindow *goban_window,
						GtkGobanClickData *data);

static void	 sgf_tree_view_clicked (GtkGobanWindow *goban_window,
					SgfNode *sgf_node, gint button_index);

static void	 navigate_goban (GtkGobanWindow *goban_window,
				 GtkGobanNavigationCommand command);
static void	 switch_to_given_node (GtkGobanWindow *goban_window,
				       SgfNode *sgf_node);

static int	 find_variation_to_switch_to (GtkGobanWindow *goban_window,
					      int x, int y,
					      SgfDirection direction);

static void	 update_children_for_new_node (GtkGobanWindow *goban_window);

static void	 update_game_information (GtkGobanWindow *goban_window);
static void	 update_window_title (GtkGobanWindow *goban_window,
				      const SgfNode *game_info_node);
static void	 update_player_information (const SgfNode *game_info_node,
					    GtkLabel *player_label,
					    SgfType name_property,
					    SgfType rank_property,
					    SgfType team_property);

static void	 update_game_specific_information
		   (GtkGobanWindow *goban_window);
static void	 update_move_information (GtkGobanWindow *goban_window);

static void	 fetch_comment_if_changed (GtkGobanWindow *goban_window,
					   gboolean for_current_node);

static int	 initialize_gtp_player (GtpClient *client, int successful,
					GtkGobanWindow *goban_window, ...);

static void	 free_handicap_has_been_placed
		   (GtkGobanWindow *goban_window,
		    BoardPositionList *handicap_stones);
static void	 move_has_been_played (GtkGobanWindow *goban_window);
static void	 move_has_been_generated (GtpClient *client, int successful,
					  GtkGobanWindow *goban_window,
					  int color, int x, int y,
					  BoardAbstractMoveData *move_data);
static void	 generate_move_via_gtp (GtkGobanWindow *goban_window);

static void	 start_clock_if_needed (GtkGobanWindow *goban_window);
static void	 player_is_out_of_time (GtkClock *clock,
					GtkGobanWindow *goban_window);


static GtkUtilsToolbarEntry toolbar_open = {
  N_("Open"),	N_("Open a game record"),		GTK_STOCK_OPEN,
  (GtkUtilsToolbarEntryCallback)  gtk_parser_interface_present, 0
};

static GtkUtilsToolbarEntry toolbar_save = {
  N_("Save"),	N_("Save the current file"),		GTK_STOCK_SAVE,
  (GtkUtilsToolbarEntryCallback) gtk_goban_window_save, GTK_GOBAN_WINDOW_SAVE
};

static GtkUtilsToolbarEntry toolbar_find = {
  N_("Find"),	N_("Search for a string in comments"),	GTK_STOCK_FIND,
  (GtkUtilsToolbarEntryCallback) show_find_dialog, 0
};

static GtkUtilsToolbarEntry toolbar_game_information = {
  N_("Info"),	N_("View and edit game information"),	GTK_STOCK_PROPERTIES,
  (GtkUtilsToolbarEntryCallback) show_game_information_dialog, 0
};


static GtkUtilsToolbarEntry navigation_toolbar_root = {
  NULL,		N_("Go to root node"),			GTK_STOCK_GOTO_FIRST,
  (GtkUtilsToolbarEntryCallback) navigate_goban, GOBAN_NAVIGATE_ROOT
};

static GtkUtilsToolbarEntry navigation_toolbar_back = {
  NULL,		N_("Go to previous node"),		GTK_STOCK_GO_BACK,
  (GtkUtilsToolbarEntryCallback) navigate_goban, GOBAN_NAVIGATE_BACK
};

static GtkUtilsToolbarEntry navigation_toolbar_forward = {
  NULL,		N_("Go to next node"),			GTK_STOCK_GO_FORWARD,
  (GtkUtilsToolbarEntryCallback) navigate_goban, GOBAN_NAVIGATE_FORWARD
};

static GtkUtilsToolbarEntry navigation_toolbar_variation_end = {
  NULL,		N_("Go to current variation's last node"), GTK_STOCK_GOTO_LAST,
  (GtkUtilsToolbarEntryCallback) navigate_goban, GOBAN_NAVIGATE_VARIATION_END
};

static GtkUtilsToolbarEntry navigation_toolbar_previous_variation = {
  NULL,		N_("Switch to previous variation"),	GTK_STOCK_GO_UP,
  (GtkUtilsToolbarEntryCallback) navigate_goban,
  GOBAN_NAVIGATE_PREVIOUS_VARIATION
};

static GtkUtilsToolbarEntry navigation_toolbar_next_variation = {
  NULL,		N_("Switch to next variation"),		GTK_STOCK_GO_DOWN,
  (GtkUtilsToolbarEntryCallback) navigate_goban, GOBAN_NAVIGATE_NEXT_VARIATION
};


static GtkWindowClass  *parent_class;

static guint		clicked_signal_id;
static guint		pointer_moved_signal_id;
static guint		goban_clicked_signal_id;


static GtkWindow       *about_dialog = NULL;


GtkType
gtk_goban_window_get_type (void)
{
  static GtkType goban_window_type = 0;

  if (!goban_window_type) {
    static GTypeInfo goban_window_info = {
      sizeof (GtkGobanWindowClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_goban_window_class_init,
      NULL,
      NULL,
      sizeof (GtkGobanWindow),
      1,
      (GInstanceInitFunc) gtk_goban_window_init,
      NULL
    };

    goban_window_type = g_type_register_static (GTK_TYPE_WINDOW,
						"GtkGobanWindow",
						&goban_window_info, 0);
  }

  return goban_window_type;
}


static void
gtk_goban_window_class_init (GtkGobanWindowClass *class)
{
  parent_class = g_type_class_peek_parent (class);

  G_OBJECT_CLASS (class)->finalize = gtk_goban_window_finalize;

  GTK_OBJECT_CLASS (class)->destroy = gtk_goban_window_destroy;
}


static void
gtk_goban_window_init (GtkGobanWindow *goban_window)
{
  static GtkItemFactoryEntry menu_entries[] = {
    { N_("/_File"), NULL, NULL, 0, "<Branch>" },
    { N_("/File/New _Game..."),		"",
      gtk_new_game_dialog_present,	0,
      "<StockItem>",			GTK_STOCK_NEW },
    { N_("/File/"), NULL, NULL, 0, "<Separator>" },

    { N_("/File/_Open..."),		"<ctrl>O",
      gtk_parser_interface_present,	0,
      "<StockItem>",			GTK_STOCK_OPEN },
    { N_("/File/"), NULL, NULL, 0, "<Separator>" },

    { N_("/File/_Save"),		"<ctrl>S",
      gtk_goban_window_save,		GTK_GOBAN_WINDOW_SAVE,
      "<StockItem>",			GTK_STOCK_SAVE },
    { N_("/File/Save _As..."),		"<shift><ctrl>S",
      gtk_goban_window_save,		GTK_GOBAN_WINDOW_SAVE_AS,
      "<StockItem>",			GTK_STOCK_SAVE_AS },


    { N_("/_Edit"), NULL, NULL, 0, "<Branch>" },
    { N_("/Edit/_Find"),		"<ctrl>F",
      show_find_dialog,			0,
      "<StockItem>",			GTK_STOCK_FIND },
    { N_("/Edit/Find Ne_xt"),		"<ctrl>G",
      (GtkItemFactoryCallback) do_find_text, GTK_GOBAN_WINDOW_FIND_NEXT,
      "<Item>" },
    { N_("/Edit/Find Pre_vious"),	"<shift><ctrl>G",
      (GtkItemFactoryCallback) do_find_text, GTK_GOBAN_WINDOW_FIND_PREVIOUS,
      "<Item>" },
    { N_("/Edit/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/Game _Information"),	"<alt>Return",
      show_game_information_dialog,	0,
      "<StockItem>",			GTK_STOCK_PROPERTIES },
    { N_("/Edit/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/_Preferences"),		NULL,
      show_preferences_dialog,		0,
      "<StockItem>",			GTK_STOCK_PREFERENCES },


    { N_("/_View"), NULL, NULL, 0, "<Branch>" },
    { N_("/View/_Main Toolbar"),	NULL,
      show_or_hide_main_toolbar,	0,
      "<CheckItem>" },
    { N_("/View/_Navigation Toolbar"),	NULL,
      show_or_hide_navigation_toolbar,	0,
      "<CheckItem>" },
    { N_("/View/"), NULL, NULL, 0, "<Separator>" },

    { N_("/View/_Game Action Buttons"),	NULL,
      show_or_hide_game_action_buttons,	0,
      "<CheckItem>" },
    { N_("/View/"), NULL, NULL, 0, "<Separator>" },

    { N_("/View/Game _Tree"),		NULL,
      show_or_hide_sgf_tree_view,	GTK_GOBAN_WINDOW_TOGGLE_CHILD,
      "<CheckItem>" },
    { N_("/View/"), NULL, NULL, 0, "<Separator>" },

    { N_("/View/_Control Center"),	NULL,
      gtk_control_center_present,	0,
      "<Item>" },


    { N_("/_Play"), NULL, NULL, 0, "<Branch>" },
    { N_("/Play/_Pass"),		NULL,
      play_pass_move,			0,
      "<Item>" },
    { N_("/Play/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Play/_Resign"),		NULL,
      play_resign,			0,
      "<Item>" },


    { N_("/_Go"), NULL, NULL, 0, "<Branch>" },
    { N_("/Go/_Previous Node"),		"<alt>Left",
      navigate_goban,			GOBAN_NAVIGATE_BACK,
      "<StockItem>",			GTK_STOCK_GO_BACK },
    { N_("/Go/_Next Node"),		"<alt>Right",
      navigate_goban,			GOBAN_NAVIGATE_FORWARD,
      "<StockItem>",			GTK_STOCK_GO_FORWARD },
    { N_("/Go/Ten Nodes _Backward"),	"<alt>Page_Up",
      navigate_goban,			GOBAN_NAVIGATE_BACK_FAST,
      "<Item>" },
    { N_("/Go/Ten Nodes _Forward"),	"<alt>Page_Down",
      navigate_goban,			GOBAN_NAVIGATE_FORWARD_FAST,
      "<Item>" },
    { N_("/Go/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Go/_Root Node"),		"<alt>Home",
      navigate_goban,			GOBAN_NAVIGATE_ROOT,
      "<StockItem>",			GTK_STOCK_GOTO_FIRST },
    { N_("/Go/Variation _Last Node"),	"<alt>End",
      navigate_goban,			 GOBAN_NAVIGATE_VARIATION_END,
      "<StockItem>",			GTK_STOCK_GOTO_LAST },
    { N_("/Go/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Go/Pre_vious Variation"),	"<alt>Up",
      navigate_goban,			GOBAN_NAVIGATE_PREVIOUS_VARIATION,
      "<StockItem>",			GTK_STOCK_GO_UP },
    { N_("/Go/Ne_xt Variation"),	"<alt>Down",
      navigate_goban,			GOBAN_NAVIGATE_NEXT_VARIATION,
      "<StockItem>",			GTK_STOCK_GO_DOWN },


    { N_("/_Help"), NULL, NULL, 0, "<Branch>" },
    { N_("/Help/_Contents"),		"F1",
      show_help_contents,		0,
      "<StockItem>",			GTK_STOCK_HELP },
    { N_("/Help/_About"),		NULL,
      show_about_dialog,		0,
      "<Item>" }
  };

  GtkWidget *goban;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *move_information_label;
  GtkWidget *mode_hint_label;
  GtkWidget *game_action_buttons_hbox;
  GtkWidget *hbox;
  GtkWidget *text_view;
  GtkWidget *scrolled_window;
  GtkWidget *vpaned;
  GtkWidget *sgf_tree_view;
  GtkWidget *vbox;
  GtkWidget *qhbox;
  GtkWidget *menu_bar;
  GtkWidget *main_toolbar;
  GtkWidget *main_handle_box;
  GtkWidget *navigation_toolbar;
  GtkWidget *navigation_handle_box;
  GtkAccelGroup *accel_group;
  int k;

  gtk_control_center_window_created (GTK_WINDOW (goban_window));

  /* Goban, the main thing in the window. */
  goban = gtk_goban_new ();
  goban_window->goban = GTK_GOBAN (goban);

  set_goban_signal_handlers (goban_window,
			     G_CALLBACK (playing_mode_pointer_moved),
			     G_CALLBACK (playing_mode_goban_clicked));
  g_signal_connect_swapped (goban, "navigate",
			    G_CALLBACK (navigate_goban), goban_window);

  /* Frame to make goban look sunken. */
  frame = gtk_utils_sink_widget (goban);

  /* Table that holds players' information and clocks. */
  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), QUARRY_SPACING_SMALL);
  gtk_table_set_col_spacings (GTK_TABLE (table), QUARRY_SPACING_BIG);

  /* Information labels and clocks for each player. */
  for (k = 0; k < NUM_COLORS; k++) {
    GtkWidget *player_label;
    GtkWidget *game_specific_info;
    GtkWidget *clock;
    GtkWidget *named_vbox;

    player_label = gtk_label_new (NULL);
    gtk_misc_set_alignment (GTK_MISC (player_label), 0.0, 0.5);
    goban_window->player_labels[k] = GTK_LABEL (player_label);

    game_specific_info = gtk_label_new (NULL);
    gtk_misc_set_alignment (GTK_MISC (game_specific_info), 0.0, 0.5);
    goban_window->game_specific_info[k] = GTK_LABEL (game_specific_info);

    named_vbox = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
					QUARRY_SPACING_VERY_SMALL,
					player_label, GTK_UTILS_FILL,
					game_specific_info, GTK_UTILS_FILL,
					NULL);
    gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (named_vbox),
				   (k == BLACK_INDEX ?
				    _("Black") : _("White")));
    gtk_table_attach (GTK_TABLE (table), named_vbox,
		      0, 1, OTHER_INDEX (k), OTHER_INDEX (k) + 1,
		      GTK_FILL, 0, 0, 0);

    clock = gtk_clock_new ();
    gtk_table_attach (GTK_TABLE (table), gtk_utils_sink_widget (clock),
		      1, 2, OTHER_INDEX (k), OTHER_INDEX (k) + 1,
		      GTK_FILL, 0, 0, 0);

    goban_window->clocks[k] = GTK_CLOCK (clock);
  }

  /* Pack the table together with a separator (which separates the
   * table and move information below.)
   */
  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				gtk_utils_align_widget (table, 0.5, 0.5),
				GTK_UTILS_FILL,
				gtk_hseparator_new (), GTK_UTILS_FILL, NULL);

  /* Move information label. */
  move_information_label = gtk_label_new (NULL);
  goban_window->move_information_label = GTK_LABEL (move_information_label);
  gtk_label_set_justify (goban_window->move_information_label,
			 GTK_JUSTIFY_CENTER);

  /* Game action buttons: "Pass" and "Resign". */
  goban_window->pass_button = gtk_button_new_with_label ("Pass");
  GTK_WIDGET_UNSET_FLAGS (goban_window->pass_button, GTK_CAN_FOCUS);

  g_signal_connect_swapped (goban_window->pass_button, "clicked",
			    G_CALLBACK (play_pass_move), goban_window);

  goban_window->resign_button = gtk_button_new_with_label ("Resign");
  GTK_WIDGET_UNSET_FLAGS (goban_window->resign_button, GTK_CAN_FOCUS);

  g_signal_connect_swapped (goban_window->resign_button, "clicked",
			    G_CALLBACK (play_resign), goban_window);

  game_action_buttons_hbox
    = gtk_utils_pack_in_box (GTK_TYPE_HBUTTON_BOX, QUARRY_SPACING_SMALL,
			     goban_window->pass_button, 0,
			     goban_window->resign_button, 0, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (game_action_buttons_hbox),
				  QUARRY_SPACING_SMALL);

  /* A label with hints about special window modes. */
  mode_hint_label = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (mode_hint_label), TRUE);
  gtk_label_set_justify (GTK_LABEL (mode_hint_label), GTK_JUSTIFY_CENTER);
  goban_window->mode_hint_label = GTK_LABEL (mode_hint_label);

  /* Buttons for ending special window modes. */
  goban_window->done_button = gtk_button_new_from_stock (QUARRY_STOCK_DONE);
  goban_window->cancel_button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBUTTON_BOX, QUARRY_SPACING_SMALL,
				goban_window->done_button, 0,
				goban_window->cancel_button, 0, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), QUARRY_SPACING_SMALL);

  /* Pack all mode information widgets together with a separator. */
  goban_window->mode_information_vbox
    = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
			     gtk_hseparator_new (), GTK_UTILS_FILL,
			     mode_hint_label, GTK_UTILS_FILL,
			     gtk_utils_align_widget (hbox, 0.5, 0.5),
			     GTK_UTILS_FILL,
			     NULL);

  /* Multipurpose text view. */
  text_view = gtk_text_view_new ();
  goban_window->text_view = GTK_TEXT_VIEW (text_view);
  gtk_text_view_set_left_margin (goban_window->text_view,
				 QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_right_margin (goban_window->text_view,
				  QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_wrap_mode (goban_window->text_view, GTK_WRAP_WORD);
  goban_window->text_buffer
    = gtk_text_view_get_buffer (goban_window->text_view);

  /* Scrolled window to keep text view in it. */
  scrolled_window = gtk_utils_make_widget_scrollable (text_view,
						      GTK_POLICY_AUTOMATIC,
						      GTK_POLICY_AUTOMATIC);

  /* Paned control for text view and SGF tree view. */
  vpaned = gtk_vpaned_new ();
  goban_window->vpaned = GTK_PANED (vpaned);

  gtk_paned_pack1 (goban_window->vpaned, scrolled_window, TRUE, FALSE);

  /* SGF tree view. */
  sgf_tree_view = gtk_sgf_tree_view_new ();
  goban_window->sgf_tree_view = GTK_SGF_TREE_VIEW (sgf_tree_view);
  goban_window->sgf_tree_view_visibility_locked = FALSE;

  g_signal_connect_swapped (sgf_tree_view, "sgf-tree-view-clicked",
			    G_CALLBACK (sgf_tree_view_clicked), goban_window);

  /* Make it scrollable.  Note that we don't pack it in the `vpaned'
   * widget now, this is done only by show_or_hide_sgf_tree_view().
   * Unlike most other containers, GtkPaned doesn't like hidden
   * children, so to show/hide `sgf_tree_view' we need to add/remove
   * it to the `vpaned'.
   */
  scrolled_window = gtk_utils_make_widget_scrollable (sgf_tree_view,
						      GTK_POLICY_AUTOMATIC,
						      GTK_POLICY_AUTOMATIC);

  g_object_ref (scrolled_window);
  gtk_object_sink (GTK_OBJECT (scrolled_window));

  /* Sidebar vertical box. */
  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, 0,
				vbox, GTK_UTILS_FILL,
				move_information_label,
				GTK_UTILS_FILL | QUARRY_SPACING_SMALL,
				(gtk_utils_align_widget
				 (game_action_buttons_hbox, 0.5, 0.5)),
				GTK_UTILS_FILL,
				goban_window->mode_information_vbox,
				GTK_UTILS_FILL,
				vpaned, GTK_UTILS_PACK_DEFAULT, NULL);

  g_signal_connect (vbox, "size-request",
		    G_CALLBACK (force_minimal_width), NULL);

  /* Horizontal box containing goban and sidebar. */
  qhbox = gtk_utils_pack_in_box (GTK_TYPE_QHBOX, QUARRY_SPACING_GOBAN_WINDOW,
				 frame, GTK_UTILS_FILL,
				 vbox, GTK_UTILS_PACK_DEFAULT, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (qhbox),
				  QUARRY_SPACING_GOBAN_WINDOW);
  gtk_qbox_set_ruling_widget (GTK_QBOX (qhbox), frame,
			      gtk_goban_negotiate_height);

  /* Window menu bar and associated accelerator group. */
  accel_group = gtk_accel_group_new ();

  goban_window->item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR,
						     "<QuarryGobanWindowMenu>",
						     accel_group);
  gtk_item_factory_create_items (goban_window->item_factory,
				 (sizeof menu_entries
				  / sizeof (GtkItemFactoryEntry)),
				 menu_entries,
				 goban_window);
  menu_bar = gtk_item_factory_get_widget (goban_window->item_factory,
					  "<QuarryGobanWindowMenu>");

  gtk_window_add_accel_group (GTK_WINDOW (goban_window), accel_group);

  /* Main toolbar and a handle box for it. */
  main_toolbar = gtk_toolbar_new ();
  goban_window->main_toolbar = GTK_TOOLBAR (main_toolbar);

  gtk_utils_append_toolbar_button (goban_window->main_toolbar,
				   &toolbar_open, goban_window);
  gtk_utils_append_toolbar_button (goban_window->main_toolbar,
				   &toolbar_save, goban_window);
  gtk_toolbar_append_space (goban_window->main_toolbar);

  gtk_utils_append_toolbar_button (goban_window->main_toolbar,
				   &toolbar_find, goban_window);
  gtk_toolbar_append_space (goban_window->main_toolbar);

  gtk_utils_append_toolbar_button (goban_window->main_toolbar,
				   &toolbar_game_information, goban_window);

  main_handle_box = gtk_handle_box_new ();
  gtk_container_add (GTK_CONTAINER (main_handle_box), main_toolbar);

  /* Navigation toolbar and a handle box for it. */
  navigation_toolbar = gtk_toolbar_new ();
  goban_window->navigation_toolbar = GTK_TOOLBAR (navigation_toolbar);

  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_root, goban_window);
  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_back, goban_window);
  gtk_toolbar_append_space (goban_window->navigation_toolbar);

  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_forward, goban_window);
  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_variation_end,
				   goban_window);
  gtk_toolbar_append_space (goban_window->navigation_toolbar);

  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_previous_variation,
				   goban_window);
  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_next_variation,
				   goban_window);

  navigation_handle_box = gtk_handle_box_new ();
  gtk_container_add (GTK_CONTAINER (navigation_handle_box),
		     navigation_toolbar);

  /* Horizontal box with the toolbars. */
  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, 0,
				main_handle_box, GTK_UTILS_FILL,
				navigation_handle_box, GTK_UTILS_PACK_DEFAULT,
				NULL);

  /* Vertical box with menu bar, toolbars and actual window
   * contents.
   */
  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, 0,
				menu_bar, GTK_UTILS_FILL,
				hbox, GTK_UTILS_FILL,
				qhbox, GTK_UTILS_PACK_DEFAULT, NULL);
  gtk_container_add (GTK_CONTAINER (goban_window), vbox);

  /* Show everything but the window itself.  Show toolbars' handle
   * boxes and game action buttons' box dependent on configuration.
   */
  gtk_widget_show_all (vbox);

  gtk_widget_hide (main_handle_box);
  if (gtk_ui_configuration.show_main_toolbar)
    show_or_hide_main_toolbar (goban_window);

  gtk_widget_hide (navigation_handle_box);
  if (gtk_ui_configuration.show_navigation_toolbar)
    show_or_hide_navigation_toolbar (goban_window);

  gtk_widget_hide (game_action_buttons_hbox);
  if (gtk_ui_configuration.show_game_action_buttons)
    show_or_hide_game_action_buttons (goban_window);

  if (game_tree_view.show_game_tree == SHOW_GAME_TREE_ALWAYS)
    show_or_hide_sgf_tree_view (goban_window, GTK_GOBAN_WINDOW_SHOW_CHILD);

  /* Look up here when the classes are certainly loaded. */
  clicked_signal_id	  = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
  pointer_moved_signal_id = g_signal_lookup ("pointer-moved", GTK_TYPE_GOBAN);
  goban_clicked_signal_id = g_signal_lookup ("goban-clicked", GTK_TYPE_GOBAN);

  /* But hide special mode section again. */
  leave_special_mode (goban_window);

  gtk_window_maximize (GTK_WINDOW (goban_window));

  goban_window->board = NULL;

  goban_window->in_game_mode	      = FALSE;
  goban_window->pending_free_handicap = 0;
  goban_window->players[BLACK_INDEX]  = NULL;
  goban_window->players[WHITE_INDEX]  = NULL;

  goban_window->filename = NULL;
  goban_window->save_as_dialog = NULL;

  goban_window->last_displayed_node = NULL;
  goban_window->last_game_info_node = NULL;

  goban_window->find_dialog = NULL;
  goban_window->text_to_find = NULL;

  goban_window->game_info_dialog = NULL;
}


GtkWidget *
gtk_goban_window_new (SgfCollection *sgf_collection, const char *filename)
{
  GtkWidget *widget = GTK_WIDGET (g_object_new (GTK_TYPE_GOBAN_WINDOW, NULL));
  GtkGobanWindow *goban_window = GTK_GOBAN_WINDOW (widget);

  assert (sgf_collection);

  goban_window->board = NULL;

  goban_window->dead_stones_list = NULL;

  goban_window->sgf_collection = sgf_collection;
  if (filename)
    goban_window->filename = g_strdup (filename);

  set_current_tree (goban_window, sgf_collection->first_tree);

  return widget;
}


static void
gtk_goban_window_destroy (GtkObject *object)
{
  GtkGobanWindow *goban_window = GTK_GOBAN_WINDOW (object);

  if (goban_window->game_info_dialog)
    gtk_widget_destroy (GTK_WIDGET (goban_window->game_info_dialog));

  if (gtk_control_center_window_destroyed (GTK_WINDOW (object))) {
    g_object_unref
      (gtk_widget_get_parent (GTK_WIDGET (goban_window->sgf_tree_view)));
  }

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


static void
gtk_goban_window_finalize (GObject *object)
{
  GtkGobanWindow *goban_window = GTK_GOBAN_WINDOW (object);

  g_object_unref (goban_window->item_factory);

  if (goban_window->board)
    board_delete (goban_window->board);

  if (goban_window->players[BLACK_INDEX])
    gtk_schedule_gtp_client_deletion (goban_window->players[BLACK_INDEX]);
  if (goban_window->players[WHITE_INDEX])
    gtk_schedule_gtp_client_deletion (goban_window->players[WHITE_INDEX]);

  if (goban_window->time_controls[BLACK_INDEX])
    time_control_delete (goban_window->time_controls[BLACK_INDEX]);
  if (goban_window->time_controls[WHITE_INDEX])
    time_control_delete (goban_window->time_controls[WHITE_INDEX]);

  if (goban_window->sgf_collection)
    sgf_collection_delete (goban_window->sgf_collection);

  g_free (goban_window->filename);
  if (goban_window->save_as_dialog)
    gtk_widget_destroy (GTK_WIDGET (goban_window->save_as_dialog));

  g_free (goban_window->text_to_find);
  if (goban_window->find_dialog)
    gtk_widget_destroy (GTK_WIDGET (goban_window->find_dialog));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
force_minimal_width (GtkWidget *widget, GtkRequisition *requisition)
{
  /* Different languages might have longer words. */
  static const gchar *string = N_("A good width for the right side to have.");

  PangoLayout *layout = gtk_widget_create_pango_layout (widget, _(string));
  gint width;

  pango_layout_get_pixel_size (layout, &width, NULL);
  if (width > requisition->width)
    requisition->width = width;

  g_object_unref (layout);
}


void
gtk_goban_window_enter_game_mode (GtkGobanWindow *goban_window,
				  GtpClient *black_player,
				  GtpClient *white_player,
				  TimeControl *black_time_control,
				  TimeControl *white_time_control)
{
  const SgfGameTree *game_tree;
  int handicap = -1;

  assert (GTK_IS_GOBAN_WINDOW (goban_window));
  assert (!goban_window->in_game_mode);

  goban_window->in_game_mode		   = TRUE;
  goban_window->players[BLACK_INDEX]	   = black_player;
  goban_window->players[WHITE_INDEX]	   = white_player;
  goban_window->time_controls[BLACK_INDEX] = black_time_control;
  goban_window->time_controls[WHITE_INDEX] = white_time_control;

  goban_window->game_position_board_state = &goban_window->sgf_board_state;

  game_tree = goban_window->current_tree;
  handicap = sgf_node_get_handicap (game_tree->current_node);

  if (handicap > 0
      && !sgf_node_get_list_of_point_property_value (game_tree->current_node,
						     SGF_ADD_BLACK)) {
    goban_window->pending_free_handicap = handicap;
    goban_window->num_handicap_stones_placed = 0;

    if (!black_player) {
      gchar *hint = g_strdup_printf (_("Please set up %d (or less)\n"
				       "stones of free handicap"),
				     handicap);

      gtk_widget_set_sensitive (goban_window->done_button, FALSE);
      enter_special_mode (goban_window, hint, free_handicap_mode_done, NULL);
      g_free (hint);

      set_goban_signal_handlers
	(goban_window,
	 G_CALLBACK (free_handicap_mode_pointer_moved),
	 G_CALLBACK (free_handicap_mode_goban_clicked));
    }
  }

  if (black_player) {
    goban_window->player_initialization_step[BLACK_INDEX]
      = INITIALIZATION_NOT_STARTED;
    initialize_gtp_player (black_player, 1, goban_window);
  }

  if (white_player) {
    goban_window->player_initialization_step[WHITE_INDEX]
      = INITIALIZATION_NOT_STARTED;
    initialize_gtp_player (white_player, 1, goban_window);
  }

  if (black_time_control && white_time_control) {
    gtk_clock_use_time_control (goban_window->clocks[BLACK_INDEX],
				black_time_control,
				((GtkClockOutOfTimeCallback)
				 player_is_out_of_time),
				goban_window);
    gtk_clock_use_time_control (goban_window->clocks[WHITE_INDEX],
				white_time_control,
				((GtkClockOutOfTimeCallback)
				 player_is_out_of_time),
				goban_window);
  }
  else
    assert (!black_time_control && !white_time_control);

  goban_window->last_displayed_node = NULL;
  update_children_for_new_node (goban_window);

  if (USER_IS_TO_PLAY (goban_window) && !goban_window->pending_free_handicap)
    start_clock_if_needed (goban_window);
}


static void
leave_game_mode (GtkGobanWindow *goban_window)
{
  goban_window->in_game_mode = FALSE;

  if (goban_window->time_controls[BLACK_INDEX]) {
    gtk_clock_use_time_control (goban_window->clocks[BLACK_INDEX], NULL,
				NULL, NULL);

    time_control_delete (goban_window->time_controls[BLACK_INDEX]);
    goban_window->time_controls[BLACK_INDEX] = NULL;
  }

  if (goban_window->time_controls[WHITE_INDEX]) {
    gtk_clock_use_time_control (goban_window->clocks[WHITE_INDEX], NULL,
				NULL, NULL);
    time_control_delete (goban_window->time_controls[WHITE_INDEX]);
    goban_window->time_controls[BLACK_INDEX] = NULL;
  }
}


static void
gtk_goban_window_save (GtkGobanWindow *goban_window, guint callback_action)
{
  if (callback_action == GTK_GOBAN_WINDOW_SAVE && goban_window->filename) {
    fetch_comment_if_changed (goban_window, TRUE);
    sgf_write_file (goban_window->filename, goban_window->sgf_collection,
		    sgf_configuration.force_utf8);
  }
  else {
    /* "Save as..." command invoked or we don't have a filename. */
    if (!goban_window->save_as_dialog) {
      GtkWidget *file_selection = gtk_file_selection_new (_("Save As..."));

      goban_window->save_as_dialog = GTK_WINDOW (file_selection);
      gtk_control_center_window_created (goban_window->save_as_dialog);
      gtk_utils_null_pointer_on_destroy (&goban_window->save_as_dialog, TRUE);

      gtk_window_set_transient_for (goban_window->save_as_dialog,
				    GTK_WINDOW (goban_window));
      gtk_window_set_destroy_with_parent (goban_window->save_as_dialog, TRUE);

      gtk_utils_add_file_selection_response_handlers
	(file_selection, TRUE,
	 G_CALLBACK (save_file_as_response), goban_window);
    }

    gtk_window_present (goban_window->save_as_dialog);
  }
}


static void
save_file_as_response (GtkFileSelection *dialog, gint response_id,
		       GtkGobanWindow *goban_window)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *filename = gtk_file_selection_get_filename (dialog);

    fetch_comment_if_changed (goban_window, TRUE);

    if (sgf_write_file (filename, goban_window->sgf_collection,
			sgf_configuration.force_utf8)) {
      gboolean need_window_title_update = (!goban_window->filename
					   || (strcmp (goban_window->filename,
						       filename)
					       != 0));

      g_free (goban_window->filename);
      goban_window->filename = g_strdup (filename);

      if (need_window_title_update) {
	update_window_title (goban_window,
			     goban_window->sgf_board_state.game_info_node);
      }
    }
  }

  if (response_id == GTK_RESPONSE_OK || response_id == GTK_RESPONSE_CANCEL)
    gtk_widget_destroy (GTK_WIDGET (dialog));
}



/* FIXME: Move find dialog to another file? */

static void
show_find_dialog (GtkGobanWindow *goban_window)
{
  static const gchar *radio_button_labels[2]
    = { N_("Search current no_de"),  N_("Search whole game _tree") };

  if (!goban_window->find_dialog) {
    GtkWidget *dialog
      = gtk_dialog_new_with_buttons (_("Find"), GTK_WINDOW (goban_window),
				     GTK_DIALOG_DESTROY_WITH_PARENT,
				     GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
				     QUARRY_STOCK_PREVIOUS,
				     GTK_GOBAN_WINDOW_FIND_PREVIOUS,
				     QUARRY_STOCK_NEXT,
				     GTK_GOBAN_WINDOW_FIND_NEXT, NULL);
    GtkWidget *entry;
    GtkWidget *label;
    GtkWidget *hbox1;
    GtkWidget *case_sensitive_check_button;
    GtkWidget *whole_words_only_check_button;
    GtkWidget *wrap_around_check_button;
    GtkWidget *vbox1;
    GtkWidget *radio_buttons[2];
    GtkWidget *vbox2;
    GtkWidget *hbox2;
    GtkWidget *close_automatically_check_button;

    if (!goban_window->text_to_find) {
      /* The dialog is opened for the first time for this
       * `goban_window'.  Initialize local configuration.
       */
      goban_window->case_sensitive
	= find_dialog_configuration.case_sensitive;
      goban_window->whole_words_only
	= find_dialog_configuration.whole_words_only;
      goban_window->wrap_around
	= find_dialog_configuration.wrap_around;
      goban_window->search_whole_game_tree
	= find_dialog_configuration.search_whole_game_tree;
    }

    goban_window->find_dialog = GTK_DIALOG (dialog);
    gtk_utils_null_pointer_on_destroy (((GtkWindow **)
					&goban_window->find_dialog),
				       FALSE);

    g_signal_connect_swapped (dialog, "response",
			      G_CALLBACK (find_dialog_response), goban_window);

    entry = gtk_utils_create_entry (goban_window->text_to_find,
				    RETURN_ACTIVATES_DEFAULT);
    goban_window->search_for_entry = GTK_ENTRY (entry);

    /* FIXME: Implement history, maybe only for GTK+ 2.4. */

    g_signal_connect_swapped (entry, "changed",
			      G_CALLBACK (find_dialog_parameters_changed),
			      goban_window);

    label = gtk_utils_create_mnemonic_label (_("Search _for:"), entry);

    hbox1 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				   label, GTK_UTILS_FILL,
				   entry, GTK_UTILS_PACK_DEFAULT, NULL);

    case_sensitive_check_button
      = gtk_check_button_new_with_mnemonic (_("Case _sensitive"));
    goban_window->case_sensitive_toggle_button
      = GTK_TOGGLE_BUTTON (case_sensitive_check_button);

    if (goban_window->case_sensitive) {
      gtk_toggle_button_set_active (goban_window->case_sensitive_toggle_button,
				    TRUE);
    }

    g_signal_connect_swapped (case_sensitive_check_button, "toggled",
			      G_CALLBACK (find_dialog_parameters_changed),
			      goban_window);

    whole_words_only_check_button
      = gtk_check_button_new_with_mnemonic (_("Whole _words only"));
    goban_window->whole_words_only_toggle_button
      = GTK_TOGGLE_BUTTON (whole_words_only_check_button);

    if (goban_window->whole_words_only) {
      gtk_toggle_button_set_active
	(goban_window->whole_words_only_toggle_button, TRUE);
    }

    g_signal_connect_swapped (whole_words_only_check_button, "toggled",
			      G_CALLBACK (find_dialog_parameters_changed),
			      goban_window);

    wrap_around_check_button
      = gtk_check_button_new_with_mnemonic (_("Wrap _around"));
    goban_window->wrap_around_toggle_button
      = GTK_TOGGLE_BUTTON (wrap_around_check_button);

    if (goban_window->wrap_around) {
      gtk_toggle_button_set_active (goban_window->wrap_around_toggle_button,
				    TRUE);
    }

    g_signal_connect_swapped (wrap_around_check_button, "toggled",
			      G_CALLBACK (find_dialog_parameters_changed),
			      goban_window);

    vbox1 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				   case_sensitive_check_button, GTK_UTILS_FILL,
				   whole_words_only_check_button,
				   GTK_UTILS_FILL,
				   wrap_around_check_button, GTK_UTILS_FILL,
				   NULL);

    gtk_utils_create_radio_chain (radio_buttons, radio_button_labels, 2);
    goban_window->search_whole_game_tree_toggle_button
      = GTK_TOGGLE_BUTTON (radio_buttons[1]);

    if (goban_window->search_whole_game_tree) {
      gtk_toggle_button_set_active
	(goban_window->search_whole_game_tree_toggle_button, TRUE);
    }

    g_signal_connect_swapped (radio_buttons[0], "toggled",
			      G_CALLBACK (find_dialog_parameters_changed),
			      goban_window);

    vbox2 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				   radio_buttons[0], GTK_UTILS_FILL,
				   radio_buttons[1], GTK_UTILS_FILL, NULL);

    hbox2 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				   vbox1, GTK_UTILS_PACK_DEFAULT,
				   vbox2, GTK_UTILS_PACK_DEFAULT, NULL);

    close_automatically_check_button
      = (gtk_check_button_new_with_mnemonic
	 (_("Close this dialog a_utomatically")));
    goban_window->close_automatically_toggle_button
      = GTK_TOGGLE_BUTTON (close_automatically_check_button);

    if (find_dialog_configuration.close_automatically) {
      gtk_toggle_button_set_active
	(goban_window->close_automatically_toggle_button, TRUE);
    }

    vbox1 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				   hbox1, GTK_UTILS_FILL,
				   hbox2, GTK_UTILS_FILL,
				   close_automatically_check_button,
				   GTK_UTILS_FILL,
				   NULL);

    gtk_widget_show_all (vbox1);
    gtk_utils_standardize_dialog (goban_window->find_dialog, vbox1);

    gtk_dialog_set_default_response (goban_window->find_dialog,
				     GTK_GOBAN_WINDOW_FIND_NEXT);

    /* Desensitize buttons if the entry is empty. */
    find_dialog_parameters_changed (goban_window);
  }

  gtk_window_present (GTK_WINDOW (goban_window->find_dialog));
}


static void
find_dialog_response (GtkGobanWindow *goban_window, gint response_id)
{
  if (response_id == GTK_GOBAN_WINDOW_FIND_NEXT
      || response_id == GTK_GOBAN_WINDOW_FIND_PREVIOUS) {
    g_free (goban_window->text_to_find);
    goban_window->text_to_find
      = g_strdup (gtk_entry_get_text (goban_window->search_for_entry));

    goban_window->case_sensitive
      = (gtk_toggle_button_get_active
	 (goban_window->case_sensitive_toggle_button));
    find_dialog_configuration.case_sensitive = goban_window->case_sensitive;

    goban_window->whole_words_only
      = (gtk_toggle_button_get_active
	 (goban_window->whole_words_only_toggle_button));
    find_dialog_configuration.whole_words_only
      = goban_window->whole_words_only;

    goban_window->wrap_around
      = gtk_toggle_button_get_active (goban_window->wrap_around_toggle_button);
    find_dialog_configuration.wrap_around = goban_window->wrap_around;

    goban_window->search_whole_game_tree
      = (gtk_toggle_button_get_active
	 (goban_window->search_whole_game_tree_toggle_button));
    find_dialog_configuration.search_whole_game_tree
      = goban_window->search_whole_game_tree;

    find_dialog_configuration.close_automatically
      = (gtk_toggle_button_get_active
	 (goban_window->close_automatically_toggle_button));

    if (!do_find_text (goban_window, response_id)
	|| !find_dialog_configuration.close_automatically) {
      /* Don't close the dialog. */
      return;
    }
  }

  gtk_widget_destroy (GTK_WIDGET (goban_window->find_dialog));
}


static void
find_dialog_parameters_changed (GtkGobanWindow *goban_window)
{
  gboolean sensitive = (* gtk_entry_get_text (goban_window->search_for_entry)
			!= 0);

  gtk_dialog_set_response_sensitive (goban_window->find_dialog,
				     GTK_GOBAN_WINDOW_FIND_NEXT, sensitive);
  gtk_dialog_set_response_sensitive (goban_window->find_dialog,
				     GTK_GOBAN_WINDOW_FIND_PREVIOUS,
				     sensitive);
}


/* This function finds text occurences in comments of current SGF game
 * tree's nodes.  We don't use GtkTreeBuffer built-in search, because
 * it would have been very inefficient: we would have had to load each
 * comment in a buffer before searching.  Also, GtkTreeBuffer
 * apparently doesn't care much for Unicode character collation etc.
 */
static gboolean
do_find_text (GtkGobanWindow *goban_window, guint callback_action)
{
  GtkTextBuffer *text_buffer = goban_window->text_buffer;
  GtkTextIter start_iterator;
  GtkTextIter selection_iterator;
  GtkTextIter end_iterator;

  gboolean case_sensitive = goban_window->case_sensitive;

  gchar *text_to_find_normalized;
  const gchar *text_to_search_in;
  gchar *text_to_search_in_normalized;
  gchar *text_to_free;
  const gchar *occurence;
  gint base_offset = 0;

  char * (* do_search) (const char *haystack, const char *needle);

  assert (goban_window->text_to_find);

  text_to_find_normalized = get_normalized_text (goban_window->text_to_find,
						 -1, case_sensitive);

  if (callback_action == GTK_GOBAN_WINDOW_FIND_NEXT)
    do_search = (goban_window->whole_words_only ? strstr_whole_word : strstr);
  else {
    /* Apparently, g_strrstr() uses naive search, but let's not care. */
    do_search = (goban_window->whole_words_only
		 ? strrstr_whole_word : g_strrstr);
  }

  /* First search in the right portion of the current node's
   * comments.
   */

  if (callback_action == GTK_GOBAN_WINDOW_FIND_NEXT) {
    gtk_text_buffer_get_selection_bounds (text_buffer,
					  NULL, &selection_iterator);
    gtk_text_buffer_get_end_iter (text_buffer, &end_iterator);

    text_to_free = gtk_text_iter_get_text (&selection_iterator, &end_iterator);
  }
  else {
    gtk_text_buffer_get_start_iter (text_buffer, &start_iterator);
    gtk_text_buffer_get_selection_bounds (text_buffer,
					  &selection_iterator, NULL);

    text_to_free = gtk_text_iter_get_text (&start_iterator,
					   &selection_iterator);
  }

  text_to_search_in_normalized = get_normalized_text (text_to_free, -1,
						      case_sensitive);

  occurence = do_search (text_to_search_in_normalized,
			 text_to_find_normalized);
  if (occurence) {
    text_to_search_in = text_to_free;
    if (callback_action == GTK_GOBAN_WINDOW_FIND_NEXT)
      base_offset = gtk_text_iter_get_offset (&selection_iterator);

    goto found;
  }

  g_free (text_to_search_in_normalized);
  g_free (text_to_free);

  /* Next traverse the game tree if requested. */
  if (goban_window->search_whole_game_tree) {
    SgfNode *occurence_node = goban_window->current_tree->current_node;
    SgfNode * (* do_traverse) (const SgfNode *sgf_node)
      = (callback_action == GTK_GOBAN_WINDOW_FIND_NEXT
	 ? sgf_node_traverse_forward : sgf_node_traverse_backward);

    while (1) {
      occurence_node = do_traverse (occurence_node);
      if (!occurence_node) {
	if (!goban_window->wrap_around)
	  break;

	if (callback_action == GTK_GOBAN_WINDOW_FIND_NEXT) {
	  occurence_node
	      = sgf_game_tree_traverse_forward (goban_window->current_tree);
	}
	else {
	  occurence_node
	    = sgf_game_tree_traverse_backward (goban_window->current_tree);
	}
      }

      /* Can happen when wrapping around. */
      if (occurence_node == goban_window->current_tree->current_node)
	break;

      text_to_search_in = sgf_node_get_text_property_value (occurence_node,
							    SGF_COMMENT);
      if (text_to_search_in) {
	text_to_search_in_normalized = get_normalized_text (text_to_search_in,
							    -1,
							    case_sensitive);

	occurence = do_search (text_to_search_in_normalized,
			       text_to_find_normalized);
	if (occurence) {
	  switch_to_given_node (goban_window, occurence_node);

	  text_to_free = NULL;
	  goto found;
	}

	g_free (text_to_search_in_normalized);
      }
    }
  }

  /* Finally, wrap around in the current node's comment, if
   * requested.
   */
  if (goban_window->wrap_around) {
    if (callback_action == GTK_GOBAN_WINDOW_FIND_NEXT) {
      gtk_text_buffer_get_start_iter (text_buffer, &start_iterator);

      /* The `selection_iterator' is already set. */
      text_to_free = gtk_text_iter_get_text (&start_iterator,
					     &selection_iterator);
    }
    else {
      gtk_text_buffer_get_end_iter (text_buffer, &end_iterator);

      /* The `selection_iterator' is already set. */
      text_to_free = gtk_text_iter_get_text (&selection_iterator,
					     &end_iterator);
    }

    text_to_search_in_normalized = get_normalized_text (text_to_free, -1,
							case_sensitive);

    occurence = do_search (text_to_search_in_normalized,
			   text_to_find_normalized);
    if (occurence) {
      text_to_search_in = text_to_free;
      if (callback_action == GTK_GOBAN_WINDOW_FIND_PREVIOUS)
	base_offset = gtk_text_iter_get_offset (&selection_iterator);

      goto found;
    }

    g_free (text_to_search_in_normalized);
    g_free (text_to_free);
  }

  /* Nothing found. */

  if (goban_window->find_dialog) {
    if (goban_window->wrap_around
	|| callback_action == GTK_GOBAN_WINDOW_FIND_NEXT) {
      gtk_dialog_set_response_sensitive (goban_window->find_dialog,
					 GTK_GOBAN_WINDOW_FIND_NEXT, FALSE);
    }

    if (goban_window->wrap_around
	|| callback_action == GTK_GOBAN_WINDOW_FIND_PREVIOUS) {
      gtk_dialog_set_response_sensitive (goban_window->find_dialog,
					 GTK_GOBAN_WINDOW_FIND_PREVIOUS,
					 FALSE);
    }
  }

  g_free (text_to_find_normalized);

  return FALSE;

 found:

  {
    /* We need to find the boundaries of the found string to adjust
     * selection in the `text_buffer'.
     *
     * The main problem is that g_utf8_normalize() can change the
     * length of string in characters (not the case with simple ASCII
     * strings, but we at Unicode handling here.)  So, we find how
     * `text_to_search_in' maps onto `text_to_search_in_normalized'
     * here.
     */
    const gchar *last_line;
    const gchar *last_line_normalized;
    const gchar *scan;
    gint num_lines;
    gint remaining_text_length;
    gint last_line_offset;

    /* First skip all full lines, to speed the things up in case of a
     * very long text.
     */

    for (last_line_normalized = text_to_search_in_normalized,
	   scan = text_to_search_in_normalized, num_lines = 0;
	 scan < occurence; scan++) {
      if (*scan == '\n') {
	num_lines++;
	last_line_normalized = scan + 1;
      }
    }

    for (last_line = text_to_search_in; num_lines > 0; last_line++) {
      if (IS_UTF8_STARTER (*last_line)) {
	/* Also adjust `base_offset' as we scan the text. */
	base_offset++;

	if (*last_line == '\n')
	  num_lines--;
      }
    }

    remaining_text_length = strlen (last_line);
    last_line_offset
      = get_offset_in_original_text (last_line, remaining_text_length,
				     occurence - last_line_normalized,
				     case_sensitive,
				     occurence - last_line_normalized);

    gtk_text_buffer_get_iter_at_offset (text_buffer, &selection_iterator,
					base_offset + last_line_offset);
    gtk_text_buffer_move_mark_by_name (text_buffer, "insert",
				       &selection_iterator);

    last_line_offset
      = get_offset_in_original_text (last_line, remaining_text_length,
				     ((occurence
				       + strlen (text_to_find_normalized))
				      - last_line_normalized),
				     case_sensitive,
				     (last_line_offset
				      + strlen (goban_window->text_to_find)));

    gtk_text_buffer_get_iter_at_offset (text_buffer, &selection_iterator,
					base_offset + last_line_offset);
    gtk_text_buffer_move_mark_by_name (text_buffer,
				       "selection_bound", &selection_iterator);

    gtk_text_view_scroll_to_iter (goban_window->text_view, &selection_iterator,
				  0.1, FALSE, 0.0, 0.0);
  }

  g_free (text_to_find_normalized);
  g_free (text_to_search_in_normalized);
  g_free (text_to_free);

  return TRUE;
}


static char *
strstr_whole_word (const char *haystack, const char *needle)
{
  gchar *occurence = strstr (haystack, needle);

  if (occurence) {
    gint needle_length = strlen (needle);
    gboolean first_char_is_word_constituent
      = char_is_word_constituent (needle);
    gboolean last_char_is_word_constituent
      = char_is_word_constituent (g_utf8_prev_char (needle + needle_length));

    do {
      if ((!first_char_is_word_constituent
	   || occurence == haystack
	   || !char_is_word_constituent (g_utf8_prev_char (occurence)))
	  && (!last_char_is_word_constituent
	      || *(occurence + needle_length) == 0
	      || !char_is_word_constituent (occurence + needle_length)))
	return occurence;

      occurence = strstr (occurence + 1, needle);
    } while (occurence);
  }

  return NULL;
}


static char *
strrstr_whole_word (const char *haystack, const char *needle)
{
  gchar *occurence = g_strrstr (haystack, needle);

  if (occurence) {
    gint needle_length = strlen (needle);
    gboolean first_char_is_word_constituent
      = char_is_word_constituent (needle);
    gboolean last_char_is_word_constituent
      = char_is_word_constituent (g_utf8_prev_char (needle + needle_length));

    do {
      if ((!first_char_is_word_constituent
	   || occurence == haystack
	   || !char_is_word_constituent (g_utf8_prev_char (occurence)))
	  && (!last_char_is_word_constituent
	      || *(occurence + needle_length) == 0
	      || !char_is_word_constituent (occurence + needle_length)))
	return occurence;

      occurence = g_strrstr_len (haystack,
				 occurence + (needle_length - 1) - haystack,
				 needle);
    } while (occurence);
  }

  return NULL;
}


static gboolean
char_is_word_constituent (const gchar *character)
{
  GUnicodeType character_type = g_unichar_type (g_utf8_get_char (character));

  return (character_type == G_UNICODE_LOWERCASE_LETTER
	  || character_type == G_UNICODE_MODIFIER_LETTER
	  || character_type == G_UNICODE_OTHER_LETTER
	  || character_type == G_UNICODE_TITLECASE_LETTER
	  || character_type == G_UNICODE_UPPERCASE_LETTER
	  || character_type == G_UNICODE_DECIMAL_NUMBER
	  || character_type == G_UNICODE_LETTER_NUMBER
	  || character_type == G_UNICODE_OTHER_NUMBER);
}


inline static gchar *
get_normalized_text (const gchar *text, gint length, gboolean case_sensitive)
{
  gchar *normalized_text;

  if (case_sensitive)
    normalized_text = g_utf8_normalize (text, length, G_NORMALIZE_ALL_COMPOSE);
  else {
    gchar *case_folded_text = g_utf8_casefold (text, length);

    normalized_text = g_utf8_normalize (case_folded_text, -1,
					G_NORMALIZE_ALL_COMPOSE);
    g_free (case_folded_text);
  }

  return normalized_text;
}


static gint
get_offset_in_original_text (const gchar *text, gint length,
			     gint normalized_text_offset,
			     gboolean case_sensitive, gint first_guess)
{
  gint byte_offset = MIN (first_guess, length - 1);
  gint offset;
  const gchar *scan;

  if (normalized_text_offset == 0)
    return 0;

  while (byte_offset < length && !IS_UTF8_STARTER (text[byte_offset]))
    byte_offset++;

  while (1) {
    gchar *normalized_text = get_normalized_text (text, byte_offset,
						  case_sensitive);
    gint length = strlen (normalized_text);

    g_free (normalized_text);

    if (length == normalized_text_offset)
      break;

    /* FIXME: Maybe implement something more efficient than one UTF-8
     *	      character at a time.
     */
    if (length < normalized_text_offset) {
      do
	byte_offset++;
      while (byte_offset < length && !IS_UTF8_STARTER (text[byte_offset]));

      if (byte_offset == length)
	break;
    }
    else {
      do
	byte_offset--;
      while (byte_offset > 0 && !IS_UTF8_STARTER (text[byte_offset]));

      if (byte_offset == 0)
	return 0;
    }
  }

  for (offset = 0, scan = text; scan < text + byte_offset; scan++) {
    if (IS_UTF8_STARTER (*scan))
      offset++;
  }

  return offset;
}



static void
show_game_information_dialog (GtkGobanWindow *goban_window)
{
  if (!goban_window->game_info_dialog) {
    goban_window->game_info_dialog
      = GTK_GAME_INFO_DIALOG (gtk_game_info_dialog_new ());
    gtk_utils_null_pointer_on_destroy (((GtkWindow **)
					&goban_window->game_info_dialog),
				       FALSE);

    g_signal_connect_swapped (goban_window->game_info_dialog,
			      "property-changed",
			      G_CALLBACK (game_info_dialog_property_changed),
			      goban_window);

    gtk_game_info_dialog_set_node (goban_window->game_info_dialog,
				   goban_window->current_tree,
				   goban_window->last_game_info_node);
  }

  gtk_window_present (GTK_WINDOW (goban_window->game_info_dialog));
}


static void
game_info_dialog_property_changed (GtkGobanWindow *goban_window,
				   SgfType sgf_property_type)
{
  SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;

  switch (sgf_property_type) {
  case SGF_PLAYER_BLACK:
  case SGF_BLACK_RANK:
  case SGF_BLACK_TEAM:
    update_player_information (game_info_node,
			       goban_window->player_labels[BLACK_INDEX],
			       SGF_PLAYER_BLACK, SGF_BLACK_RANK,
			       SGF_BLACK_TEAM);
    break;

  case SGF_PLAYER_WHITE:
  case SGF_WHITE_RANK:
  case SGF_WHITE_TEAM:
    update_player_information (game_info_node,
			       goban_window->player_labels[WHITE_INDEX],
			       SGF_PLAYER_WHITE, SGF_WHITE_RANK,
			       SGF_WHITE_TEAM);
    break;

  case SGF_KOMI:
    update_game_specific_information (goban_window);
    break;

  case SGF_RESULT:
    if (!goban_window->current_tree->current_node->child
	&& !goban_window->sgf_board_state.last_main_variation_node) {
      /* The result is being displayed now, update the display. */
      update_move_information (goban_window);
    }

  default:
    /* Silence warnings. */
    return;
  }
}


static void
show_preferences_dialog (void)
{
  gtk_preferences_dialog_present (GINT_TO_POINTER (-1));
}


static void
show_or_hide_main_toolbar (GtkGobanWindow *goban_window)
{
  GtkWidget *toolbar_handle_box
    = gtk_widget_get_parent (GTK_WIDGET (goban_window->main_toolbar));
  GtkWidget *menu_item
    = gtk_item_factory_get_widget (goban_window->item_factory,
				   _("/View/Main Toolbar"));

  gtk_ui_configuration.show_main_toolbar
    = !GTK_WIDGET_VISIBLE (toolbar_handle_box);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
				  gtk_ui_configuration.show_main_toolbar);
  gtk_utils_set_widgets_visible (gtk_ui_configuration.show_main_toolbar,
				 toolbar_handle_box, NULL);
}


static void
show_or_hide_navigation_toolbar (GtkGobanWindow *goban_window)
{
  GtkWidget *toolbar_handle_box
    = gtk_widget_get_parent (GTK_WIDGET (goban_window->navigation_toolbar));
  GtkWidget *menu_item
    = gtk_item_factory_get_widget (goban_window->item_factory,
				   _("/View/Navigation Toolbar"));

  gtk_ui_configuration.show_navigation_toolbar
    = !GTK_WIDGET_VISIBLE (toolbar_handle_box);
  gtk_check_menu_item_set_active
    (GTK_CHECK_MENU_ITEM (menu_item),
     gtk_ui_configuration.show_navigation_toolbar);
  gtk_utils_set_widgets_visible (gtk_ui_configuration.show_navigation_toolbar,
				 toolbar_handle_box, NULL);
}


static void
show_or_hide_game_action_buttons (GtkGobanWindow *goban_window)
{
  GtkWidget *game_action_buttons_box
    = gtk_widget_get_parent (goban_window->pass_button);
  GtkWidget *menu_item
    = gtk_item_factory_get_widget (goban_window->item_factory,
				   _("/View/Game Action Buttons"));

  gtk_ui_configuration.show_game_action_buttons
    = !GTK_WIDGET_VISIBLE (game_action_buttons_box);
  gtk_check_menu_item_set_active
    (GTK_CHECK_MENU_ITEM (menu_item),
     gtk_ui_configuration.show_game_action_buttons);
  gtk_utils_set_widgets_visible (gtk_ui_configuration.show_game_action_buttons,
				 game_action_buttons_box, NULL);
}


static void
show_or_hide_sgf_tree_view (GtkGobanWindow *goban_window,
			    guint callback_action)
{
  GtkWidget *sgf_tree_view_parent
    = gtk_widget_get_parent (GTK_WIDGET (goban_window->sgf_tree_view));
  gboolean sgf_tree_view_is_visible
    = (gtk_widget_get_parent (sgf_tree_view_parent) != NULL);
  gboolean show_sgf_tree_view;


  if (callback_action == GTK_GOBAN_WINDOW_TOGGLE_CHILD) {
    show_sgf_tree_view = !sgf_tree_view_is_visible;
    goban_window->sgf_tree_view_visibility_locked = TRUE;
  }
  else
    show_sgf_tree_view = callback_action;

  if (show_sgf_tree_view != sgf_tree_view_is_visible) {
    GtkWidget *menu_item
      = gtk_item_factory_get_widget (goban_window->item_factory,
				     _("/View/Game Tree"));

    g_signal_handlers_block_by_func (menu_item, show_or_hide_sgf_tree_view,
				     goban_window);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
				    show_sgf_tree_view);
    g_signal_handlers_unblock_by_func (menu_item, show_or_hide_sgf_tree_view,
				       goban_window);

    if (show_sgf_tree_view) {
      gtk_paned_pack2 (goban_window->vpaned, sgf_tree_view_parent,
		       TRUE, FALSE);
    }
    else {
      gtk_container_remove (GTK_CONTAINER (goban_window->vpaned),
			    sgf_tree_view_parent);
    }

  }
}


static void
show_sgf_tree_view_automatically (GtkGobanWindow *goban_window,
				  const SgfNode *sgf_node)
{
  if (game_tree_view.show_game_tree == SHOW_GAME_TREE_AUTOMATICALLY
      && !goban_window->sgf_tree_view_visibility_locked
      && sgf_node
      && (sgf_node->next
	  || (sgf_node->parent && sgf_node->parent->child != sgf_node)))
    show_or_hide_sgf_tree_view (goban_window, GTK_GOBAN_WINDOW_SHOW_CHILD);
}


static void
show_about_dialog (void)
{
  if (!about_dialog) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("About Quarry"),
						     NULL, 0,
						     GTK_STOCK_CLOSE,
						     GTK_RESPONSE_CANCEL,
						     NULL);
    GtkWidget *quarry_label;
    GtkWidget *description_label;
    GtkWidget *copyright_label;
    GtkWidget *vbox;

    about_dialog = GTK_WINDOW (dialog);
    gtk_utils_null_pointer_on_destroy (&about_dialog, FALSE);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

    g_signal_connect (dialog, "response",
		      G_CALLBACK (gtk_widget_destroy), NULL);

    quarry_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (quarry_label),
			  ("<span size=\"larger\" weight=\"bold\">"
			   PACKAGE_STRING "</span>"));

    description_label = gtk_label_new (_("A GUI program for Go, Amazons "
					 "and Othello board games"));

    copyright_label = gtk_label_new (NULL);
    gtk_label_set_justify (GTK_LABEL (copyright_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_markup (GTK_LABEL (copyright_label),
			  _("<small>Copyright \xc2\xa9 2003 Paul Pogonyshev\n"
			    "Copyright \xc2\xa9 2004 "
			    "Paul Pogonyshev and Martin Holters</small>"));

    vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING,
				  quarry_label, GTK_UTILS_FILL,
				  description_label, GTK_UTILS_FILL,
				  copyright_label, GTK_UTILS_FILL, NULL);

    gtk_utils_standardize_dialog (GTK_DIALOG (dialog), vbox);
    gtk_widget_show_all (vbox);
  }

  gtk_window_present (about_dialog);
}


static void
show_help_contents (void)
{
  gtk_help_display (NULL);
}



static void
update_territory_markup (GtkGobanWindow *goban_window)
{
  int x;
  int y;
  char goban_markup[BOARD_GRID_SIZE];

  for (y = 0; y < goban_window->board->height; y++) {
    for (x = 0; x < goban_window->board->width; x++) {
      int pos = POSITION (x, y);

      if (!goban_window->dead_stones[pos])
	goban_markup[pos] = TILE_NONE;
      else
	goban_markup[pos] = GOBAN_MARKUP_GHOSTIFY;
    }
  }

  go_mark_territory_on_grid (goban_window->board,
			     goban_markup, goban_window->dead_stones,
			     BLACK_OPAQUE | GOBAN_MARKUP_GHOSTIFY,
			     WHITE_OPAQUE | GOBAN_MARKUP_GHOSTIFY);

  gtk_goban_update (goban_window->goban, NULL, goban_markup, NULL,
		    KEEP_SGF_LABELS,
		    goban_window->sgf_board_state.last_move_x,
		    goban_window->sgf_board_state.last_move_y);
}



static void
enter_special_mode (GtkGobanWindow *goban_window, const gchar *hint,
		    SpecialModeButtonClicked done_callback,
		    SpecialModeButtonClicked cancel_callback)
{
  gtk_label_set_text (goban_window->mode_hint_label, hint);

  gtk_widget_show (goban_window->mode_information_vbox);

  gtk_widget_show (goban_window->done_button);
  g_signal_connect_swapped (goban_window->done_button, "clicked",
			    G_CALLBACK (done_callback), goban_window);

  if (cancel_callback) {
    gtk_widget_show (goban_window->cancel_button);
    g_signal_connect_swapped (goban_window->cancel_button, "clicked",
			      G_CALLBACK (cancel_callback), goban_window);
  }
  else
    gtk_widget_hide (goban_window->cancel_button);
}


static void
leave_special_mode (GtkGobanWindow *goban_window)
{
  g_signal_handlers_disconnect_matched (goban_window->done_button,
					(G_SIGNAL_MATCH_ID
					 | G_SIGNAL_MATCH_DATA),
					clicked_signal_id, 0, NULL, NULL,
					goban_window);
  g_signal_handlers_disconnect_matched (goban_window->cancel_button,
					(G_SIGNAL_MATCH_ID
					 | G_SIGNAL_MATCH_DATA),
					clicked_signal_id, 0, NULL, NULL,
					goban_window);

  gtk_widget_hide (goban_window->mode_information_vbox);
}


static void
free_handicap_mode_done (GtkGobanWindow *goban_window)
{
  BoardPositionList *position_lists[NUM_ON_GRID_VALUES];

  gtk_goban_diff_against_grid (goban_window->goban, goban_window->board->grid,
			       position_lists);
  assert ((position_lists[BLACK]->num_positions
	   == goban_window->num_handicap_stones_placed)
	  && position_lists[WHITE] == NULL
	  && position_lists[EMPTY] == NULL
	  && position_lists[SPECIAL_ON_GRID_VALUE] == NULL);

  free_handicap_has_been_placed (goban_window, position_lists[BLACK]);

  leave_special_mode (goban_window);
  set_goban_signal_handlers (goban_window,
			     G_CALLBACK (playing_mode_pointer_moved),
			     G_CALLBACK (playing_mode_goban_clicked));
}


static void
go_scoring_mode_done (GtkGobanWindow *goban_window)
{
  SgfGameTree *current_tree = goban_window->current_tree;
  SgfNode *game_info_node
    = goban_window->game_position_board_state->game_info_node;
  double komi;
  double score;
  char *detailed_score;
  BoardPositionList *black_territory;
  BoardPositionList *white_territory;

  assert (sgf_node_get_komi (goban_window->sgf_board_state.game_info_node,
			     &komi));
  go_score_game (goban_window->board, goban_window->dead_stones, komi,
		 &score, &detailed_score, &black_territory, &white_territory);

  sgf_node_append_text_property (current_tree->current_node, current_tree,
				 SGF_COMMENT, detailed_score,
				 "\n\n----------------\n\n");

  sgf_node_add_list_of_point_property (current_tree->current_node,
				       current_tree,
				       SGF_BLACK_TERRITORY, black_territory,
				       1);
  sgf_node_add_list_of_point_property (current_tree->current_node,
				       current_tree,
				       SGF_WHITE_TERRITORY, white_territory,
				       1);

  sgf_node_add_score_result (game_info_node, goban_window->current_tree,
			     score, 1);

  g_free (goban_window->dead_stones);

  reenter_current_node (goban_window);

  leave_special_mode (goban_window);
  set_goban_signal_handlers (goban_window,
			     G_CALLBACK (playing_mode_pointer_moved),
			     G_CALLBACK (playing_mode_goban_clicked));
}


static void
set_current_tree (GtkGobanWindow *goban_window, SgfGameTree *sgf_tree)
{
  if (!goban_window->board && GAME_IS_SUPPORTED (sgf_tree->game)) {
    GtkLabel **game_specific_info = goban_window->game_specific_info;

    goban_window->board = board_new (sgf_tree->game,
				     sgf_tree->board_width,
				     sgf_tree->board_height);

    gtk_utils_set_widgets_visible (goban_window->board->game != GAME_AMAZONS,
				   game_specific_info[BLACK_INDEX],
				   game_specific_info[WHITE_INDEX], NULL);
    gtk_utils_set_widgets_visible (goban_window->board->game == GAME_GO,
				   goban_window->pass_button, NULL);
  }

  /* Won't work from update_children_for_new_node() below, because the
   * tree is being changed.
   */
  fetch_comment_if_changed (goban_window, TRUE);

  goban_window->current_tree = sgf_tree;
  sgf_utils_enter_tree (sgf_tree, goban_window->board,
			&goban_window->sgf_board_state);

  gtk_goban_set_parameters (goban_window->goban, sgf_tree->game,
			    sgf_tree->board_width, sgf_tree->board_height);

  update_game_information (goban_window);
  update_children_for_new_node (goban_window);

  gtk_sgf_tree_view_set_sgf_tree (goban_window->sgf_tree_view, sgf_tree);

  if (game_tree_view.show_game_tree == SHOW_GAME_TREE_AUTOMATICALLY
      && !goban_window->sgf_tree_view_visibility_locked) {
    const SgfNode *sgf_node;

    for (sgf_node = sgf_tree->root->child; sgf_node;
	 sgf_node = sgf_node->child) {
      if (sgf_node->next)
	break;
    }

    show_or_hide_sgf_tree_view (goban_window,
				(sgf_node
				 ? GTK_GOBAN_WINDOW_SHOW_CHILD
				 : GTK_GOBAN_WINDOW_HIDE_CHILD));
  }
}


static void
reenter_current_node (GtkGobanWindow *goban_window)
{
  SgfGameTree *current_tree = goban_window->current_tree;

  if (current_tree->current_node->parent) {
    sgf_utils_go_up_in_tree (current_tree, 1, &goban_window->sgf_board_state);
    sgf_utils_go_down_in_tree (current_tree, 1,
			       &goban_window->sgf_board_state);
  }
  else {
    sgf_utils_enter_tree (current_tree, goban_window->board,
			  &goban_window->sgf_board_state);
  }

  goban_window->last_displayed_node = NULL;
  update_children_for_new_node (goban_window);
}


/* This function must be called before changing to ``arbitrary'' in
 * some sense node.  More exactly, when the node we are going to
 * switch to is not being added as a result of playing a move.
 */
static void
about_to_change_node (GtkGobanWindow *goban_window)
{
  if (goban_window->in_game_mode && IS_DISPLAYING_GAME_NODE (goban_window)) {
    /* Goban window is going to display something other than the game
     * position node.
     */
    sgf_game_tree_get_state (goban_window->current_tree,
			     &goban_window->game_position);
    goban_window->game_position.board
      = board_duplicate_without_stacks (goban_window->game_position.board);

    goban_window->game_position_board_state
      = &goban_window->game_position_board_state_holder;
    memcpy (&goban_window->game_position_board_state_holder,
	    &goban_window->sgf_board_state, sizeof (SgfBoardState));
  }
}


/* Similar to about_to_change_node(), but has to be called after the
 * current node is changed.
 */
static void
just_changed_node (GtkGobanWindow *goban_window)
{
  if (goban_window->in_game_mode
      && !IS_DISPLAYING_GAME_NODE (goban_window)
      && (goban_window->game_position.current_node
	  == goban_window->current_tree->current_node)) {
    /* The goban window displayed something other than the game
     * position node, but has just navigated back to that node.
     */
    board_delete (goban_window->game_position.board);
    goban_window->game_position_board_state = &goban_window->sgf_board_state;
  }
}


static void
play_pass_move (GtkGobanWindow *goban_window)
{
  assert (goban_window->board->game == GAME_GO
	  && USER_CAN_PLAY_MOVES (goban_window));

  sgf_utils_append_variation (goban_window->current_tree,
			      &goban_window->sgf_board_state,
			      goban_window->sgf_board_state.color_to_play,
			      PASS_X, PASS_Y);

  update_children_for_new_node (goban_window);

  if (goban_window->in_game_mode && IS_DISPLAYING_GAME_NODE (goban_window))
    move_has_been_played (goban_window);
}


static void
play_resign (GtkGobanWindow *goban_window)
{
  GtkWidget *question_dialog
    = gtk_utils_create_message_dialog (GTK_WINDOW (goban_window),
				       GTK_STOCK_DIALOG_QUESTION,
				       (GTK_UTILS_NO_BUTTONS
					| GTK_UTILS_DONT_SHOW),
				       NULL, _("Really resign this game?"));
  gtk_dialog_add_buttons (GTK_DIALOG (question_dialog),
			  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			  _("_Resign"), GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (question_dialog),
				   GTK_RESPONSE_OK);
  if (gtk_dialog_run (GTK_DIALOG (question_dialog)) == GTK_RESPONSE_OK)
    do_resign_game (goban_window);

  gtk_widget_destroy (question_dialog);
}


static void
do_resign_game (GtkGobanWindow *goban_window)
{
  int color = goban_window->sgf_board_state.color_to_play;
  char other_color_char = (color == BLACK ? 'W' : 'B');
  SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;

  leave_game_mode (goban_window);

  sgf_node_add_text_property (game_info_node, goban_window->current_tree,
			      SGF_RESULT,
			      utils_cprintf ("%c+Resign", other_color_char),
			      1);

  sgf_utils_append_variation (goban_window->current_tree,
			      &goban_window->sgf_board_state, EMPTY);
  update_children_for_new_node (goban_window);
}


static void
cancel_amazons_move (GtkGobanWindow *goban_window)
{
  if (goban_window->amazons_move_stage == SHOOTING_ARROW) {
    gtk_goban_set_overlay_data (goban_window->goban, 1, NULL,
				TILE_NONE, TILE_NONE);
  }

  if (goban_window->amazons_move_stage != SELECTING_QUEEN) {
    gtk_goban_set_overlay_data (goban_window->goban, 0, NULL,
				TILE_NONE, TILE_NONE);
  }

  reset_amazons_move_data (goban_window);
}


static void
reset_amazons_move_data (GtkGobanWindow *goban_window)
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
set_goban_signal_handlers (GtkGobanWindow *goban_window,
			   GCallback pointer_moved_handler,
			   GCallback goban_clicked_handler)
{
  g_signal_handlers_disconnect_matched (goban_window->goban,
					(G_SIGNAL_MATCH_ID
					 | G_SIGNAL_MATCH_DATA),
					pointer_moved_signal_id, 0, NULL, NULL,
					goban_window);
  g_signal_handlers_disconnect_matched (goban_window->goban,
					(G_SIGNAL_MATCH_ID
					 | G_SIGNAL_MATCH_DATA),
					goban_clicked_signal_id, 0, NULL, NULL,
					goban_window);

  g_signal_connect_swapped (goban_window->goban, "pointer-moved",
			    pointer_moved_handler, goban_window);
  g_signal_connect_swapped (goban_window->goban, "goban-clicked",
			    goban_clicked_handler, goban_window);
}


static GtkGobanPointerFeedback
playing_mode_pointer_moved (GtkGobanWindow *goban_window,
			    GtkGobanPointerData *data)
{
  if (USER_CAN_PLAY_MOVES (goban_window)) {
    int color_to_play = goban_window->sgf_board_state.color_to_play;

    switch (data->button) {
    case 0:
      if (!(data->modifiers & GDK_SHIFT_MASK)) {
	if (color_to_play == EMPTY)
	  break;

	if (goban_window->board->game != GAME_AMAZONS) {
	  if (board_is_legal_move (goban_window->board, RULE_SET_DEFAULT,
				   color_to_play, data->x, data->y))
	    return GOBAN_FEEDBACK_MOVE + COLOR_INDEX (color_to_play);
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

	  if (board_is_legal_move (goban_window->board, RULE_SET_DEFAULT,
				   color_to_play,
				   goban_window->amazons_to_x,
				   goban_window->amazons_to_y,
				   goban_window->amazons_move)) {
	    if (goban_window->amazons_move_stage == SELECTING_QUEEN)
	      return GOBAN_FEEDBACK_MOVE + COLOR_INDEX (color_to_play);
	    else if (goban_window->amazons_move_stage == MOVING_QUEEN)
	      return GOBAN_FEEDBACK_GHOST + COLOR_INDEX (color_to_play);
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
	  && find_variation_to_switch_to (goban_window, data->x, data->y,
					  data->modifiers & GDK_SHIFT_MASK
					  ? SGF_PREVIOUS : SGF_NEXT)) {
	return (GOBAN_FEEDBACK_THICK_GHOST
		+ COLOR_INDEX (goban_window->node_to_switch_to->move_color));
      }
    }
  }

  return GOBAN_FEEDBACK_NONE;
}


static void
playing_mode_goban_clicked (GtkGobanWindow *goban_window,
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
	sgf_utils_append_variation (goban_window->current_tree,
				    &goban_window->sgf_board_state,
				    color_to_play, data->x, data->y);
      }
      else {
	int pos = POSITION (data->x, data->y);

	if (goban_window->amazons_move_stage == SELECTING_QUEEN) {
	  goban_window->amazons_move_stage  = MOVING_QUEEN;

	  gtk_goban_set_overlay_data (goban_window->goban, 0,
				      board_position_list_new (&pos, 1),
				      (STONE_50_TRANSPARENT
				       + COLOR_INDEX (color_to_play)),
				      GOBAN_TILE_DONT_CHANGE);

	  return;
	}
	else if (goban_window->amazons_move_stage == MOVING_QUEEN) {
	  goban_window->amazons_move_stage  = SHOOTING_ARROW;

	  gtk_goban_set_overlay_data (goban_window->goban, 1,
				      board_position_list_new (&pos, 1),
				      (STONE_25_TRANSPARENT
				       + COLOR_INDEX (color_to_play)),
				      GOBAN_TILE_DONT_CHANGE);

	  return;
	}
	else {
	  sgf_utils_append_variation (goban_window->current_tree,
				      &goban_window->sgf_board_state,
				      color_to_play,
				      goban_window->amazons_to_x,
				      goban_window->amazons_to_y,
				      goban_window->amazons_move);
	}
      }

      update_children_for_new_node (goban_window);

      if (goban_window->in_game_mode && IS_DISPLAYING_GAME_NODE (goban_window))
	move_has_been_played (goban_window);
    }

    break;

  case 3:
    if (goban_window->board->game != GAME_AMAZONS
	|| goban_window->amazons_move_stage == SELECTING_QUEEN) {
      if (!find_variation_to_switch_to (goban_window, data->x, data->y,
					data->modifiers & GDK_SHIFT_MASK
					? SGF_PREVIOUS : SGF_NEXT))
	return;

      about_to_change_node (goban_window);
      sgf_utils_switch_to_given_variation (goban_window->current_tree,
					   goban_window->node_to_switch_to,
					   &goban_window->sgf_board_state);
      just_changed_node (goban_window);
      update_children_for_new_node (goban_window);
    }
    else
      cancel_amazons_move (goban_window);

    break;
  }
}


static GtkGobanPointerFeedback
free_handicap_mode_pointer_moved (GtkGobanWindow *goban_window,
				  GtkGobanPointerData *data)
{
  switch (data->button) {
  case 0:
    if (!(data->modifiers & GDK_SHIFT_MASK)) {
      int contents = gtk_goban_get_grid_contents (goban_window->goban,
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
free_handicap_mode_goban_clicked (GtkGobanWindow *goban_window,
				  GtkGobanClickData *data)
{
  if (data->button == 1
      && data->feedback_tile != GOBAN_TILE_DONT_CHANGE
      && !(data->modifiers & GDK_SHIFT_MASK)) {
    int contents = gtk_goban_get_grid_contents (goban_window->goban,
						data->x, data->y);
    int pos = POSITION (data->x, data->y);
    BoardPositionList *position_list = board_position_list_new (&pos, 1);

    if (contents == EMPTY) {
      contents = BLACK;
      goban_window->num_handicap_stones_placed++;
    }
    else if (contents == BLACK) {
      contents = EMPTY;
      goban_window->num_handicap_stones_placed--;
    }
    else
      assert (0);

    gtk_goban_set_contents (goban_window->goban, position_list,
			    contents, GOBAN_TILE_DONT_CHANGE);
    board_position_list_delete (position_list);

    gtk_widget_set_sensitive (goban_window->done_button,
			      goban_window->num_handicap_stones_placed >= 2);
  }
}


static GtkGobanPointerFeedback
go_scoring_mode_pointer_moved (GtkGobanWindow *goban_window,
			       GtkGobanPointerData *data)
{
  BoardPositionList * (* const get_stones) (Board *board, int x, int y)
    = (data->modifiers & GDK_SHIFT_MASK
       ? go_get_string_stones : go_get_logically_dead_stones);
  int pos = POSITION (data->x, data->y);

  switch (data->button) {
  case 0:
    {
      BoardPositionList *stones = get_stones (goban_window->board,
					      data->x, data->y);

      if (stones) {
	int color = goban_window->board->grid[pos];

	data->feedback_position_list = stones;

	if (!goban_window->dead_stones[pos]) {
	  return ((GOBAN_FEEDBACK_GHOST + COLOR_INDEX (OTHER_COLOR (color)))
		  * GOBAN_FEEDBACK_MARKUP_FACTOR);
	}
	else {
	  return ((GOBAN_FEEDBACK_THICK_GHOST + COLOR_INDEX (color))
		  + (GOBAN_FEEDBACK_FORCE_TILE_NONE
		     * GOBAN_FEEDBACK_MARKUP_FACTOR));
	}
      }
    }

    break;

  case 1:
    {
      BoardPositionList *stones = get_stones (goban_window->board,
					      data->x, data->y);

      if (stones) {
	int color = goban_window->board->grid[POSITION (data->x, data->y)];

	data->feedback_position_list = stones;

	if (!goban_window->dead_stones[pos]) {
	  return ((GOBAN_FEEDBACK_THICK_GHOST + COLOR_INDEX (color))
		  + ((GOBAN_FEEDBACK_GHOST + COLOR_INDEX (OTHER_COLOR (color)))
		     * GOBAN_FEEDBACK_MARKUP_FACTOR));
	}
	else {
	  return ((GOBAN_FEEDBACK_OPAQUE + COLOR_INDEX (color))
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
go_scoring_mode_goban_clicked (GtkGobanWindow *goban_window,
			       GtkGobanClickData *data)
{
  if (data->button == 1) {
    BoardPositionList * (* const get_stones) (Board *board, int x, int y)
      = (data->modifiers & GDK_SHIFT_MASK
	 ? go_get_string_stones : go_get_logically_dead_stones);
    BoardPositionList *stones = get_stones (goban_window->board,
					    data->x, data->y);

    if (stones) {
      int pos = POSITION (data->x, data->y);

      board_position_list_mark_on_grid (stones, goban_window->dead_stones,
					!goban_window->dead_stones[pos]);
      board_position_list_delete (stones);

      update_territory_markup (goban_window);
    }
  }
}


static void
sgf_tree_view_clicked (GtkGobanWindow *goban_window, SgfNode *sgf_node,
		       gint button_index)
{
  if (button_index == 1)
    switch_to_given_node (goban_window, sgf_node);
  else {
    assert (button_index == 3);

    if (sgf_node->child) {
      sgf_utils_set_node_is_collapsed (goban_window->current_tree, sgf_node,
				       !sgf_node->is_collapsed);
      gtk_sgf_tree_view_update_view_port (goban_window->sgf_tree_view);
    }
  }
}


static void
navigate_goban (GtkGobanWindow *goban_window,
		GtkGobanNavigationCommand command)
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

  about_to_change_node (goban_window);

  switch (command) {
  case GOBAN_NAVIGATE_BACK:
    sgf_utils_go_up_in_tree (current_tree, 1, &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_BACK_FAST:
    sgf_utils_go_up_in_tree (current_tree, NAVIGATE_FAST_NUM_MOVES,
			     &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_FORWARD:
    sgf_utils_go_down_in_tree (current_tree, 1,
			       &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_FORWARD_FAST:
    sgf_utils_go_down_in_tree (current_tree, NAVIGATE_FAST_NUM_MOVES,
			       &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_PREVIOUS_VARIATION:
    sgf_utils_switch_to_variation (current_tree, SGF_PREVIOUS,
				   &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_NEXT_VARIATION:
    sgf_utils_switch_to_variation (current_tree, SGF_NEXT,
				   &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_ROOT:
    sgf_utils_go_up_in_tree (current_tree, -1,
			     &goban_window->sgf_board_state);
    break;

  case GOBAN_NAVIGATE_VARIATION_END:
    sgf_utils_go_down_in_tree (current_tree, -1,
			       &goban_window->sgf_board_state);
    break;
  }

  just_changed_node (goban_window);
  update_children_for_new_node (goban_window);
}


static void
switch_to_given_node (GtkGobanWindow *goban_window, SgfNode *sgf_node)
{
  about_to_change_node (goban_window);
  sgf_utils_switch_to_given_node (goban_window->current_tree, sgf_node,
				  &goban_window->sgf_board_state);
  just_changed_node (goban_window);

  update_children_for_new_node (goban_window);
}


static int
find_variation_to_switch_to (GtkGobanWindow *goban_window,
			     int x, int y,
			     SgfDirection direction)
{
  if (x != goban_window->switching_x || y != goban_window->switching_y
      || direction != goban_window->switching_direction) {
    const SgfNode *current_node = goban_window->current_tree->current_node;
    int after_node = (IS_STONE (current_node->move_color)
		      && current_node->move_point.x == x
		      && current_node->move_point.y == y);

    goban_window->switching_x	      = x;
    goban_window->switching_y	      = y;
    goban_window->switching_direction = direction;
    goban_window->node_to_switch_to
      = sgf_utils_find_variation_at_position (goban_window->current_tree,
					      x, y, direction, after_node);
  }

  return goban_window->node_to_switch_to != NULL;
}


static void
update_children_for_new_node (GtkGobanWindow *goban_window)
{
  SgfGameTree *current_tree = goban_window->current_tree;
  SgfNode *current_node = current_tree->current_node;
  char goban_markup[BOARD_GRID_SIZE];
  const char *comment;
  int pass_sensitive;
  int resign_sensitive;

  if (current_node == goban_window->last_displayed_node)
    return;

  fetch_comment_if_changed (goban_window, FALSE);

  reset_amazons_move_data (goban_window);

  if (!goban_window->last_displayed_node
      || current_node->parent != goban_window->last_displayed_node->parent) {
    sgf_utils_count_variations (current_tree, 1,
				goban_window->black_variations,
				goban_window->white_variations, NULL);
  }

  board_fill_grid (goban_window->board, goban_markup, TILE_NONE);
  gui_utils_mark_variations_on_grid (goban_markup, goban_window->board,
				     goban_window->black_variations,
				     goban_window->white_variations,
				     BLACK_50_TRANSPARENT,
				     WHITE_50_TRANSPARENT,
				     MIXED_50_TRANSPARENT);
  sgf_utils_mark_territory_on_grid (current_tree, goban_markup,
				    (BLACK_OPAQUE
				     | GOBAN_MARKUP_GHOSTIFY),
				    (WHITE_OPAQUE
				     | GOBAN_MARKUP_GHOSTIFY));

  sgf_utils_get_markup (current_tree, goban_window->sgf_markup);

  gtk_goban_update (goban_window->goban, goban_window->board->grid,
		    goban_markup, goban_window->sgf_markup,
		    sgf_node_get_list_of_label_property_value (current_node,
							       SGF_LABEL),
		    goban_window->sgf_board_state.last_move_x,
		    goban_window->sgf_board_state.last_move_y);
  gtk_goban_force_feedback_poll (goban_window->goban);

  if (goban_window->last_game_info_node
      != goban_window->sgf_board_state.game_info_node)
    update_game_information (goban_window);

  update_game_specific_information (goban_window);
  update_move_information (goban_window);

  comment = sgf_node_get_text_property_value (current_node, SGF_COMMENT);
  gtk_utils_set_text_buffer_text (goban_window->text_buffer, comment);

  gtk_sgf_tree_view_update_view_port (goban_window->sgf_tree_view);
  show_sgf_tree_view_automatically (goban_window, current_node);

  pass_sensitive = (goban_window->board->game == GAME_GO
		    && USER_CAN_PLAY_MOVES (goban_window)
		    && !IS_IN_SPECIAL_MODE (goban_window));
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      pass_sensitive,
				      _("/Play/Pass"), NULL);
  gtk_widget_set_sensitive (goban_window->pass_button, pass_sensitive);

  resign_sensitive = (goban_window->in_game_mode
		      && IS_DISPLAYING_GAME_NODE (goban_window)
		      && USER_CAN_PLAY_MOVES (goban_window)
		      && !IS_IN_SPECIAL_MODE (goban_window));
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      resign_sensitive,
				      _("/Play/Resign"), NULL);
  gtk_widget_set_sensitive (goban_window->resign_button, resign_sensitive);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      current_node->parent != NULL,
				      _("/Go/Previous Node"),
				      _("/Go/Ten Nodes Backward"),
				      _("/Go/Root Node"), NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->navigation_toolbar,
					   current_node->parent != NULL,
					   &navigation_toolbar_back,
					   &navigation_toolbar_root, NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      current_node->child != NULL,
				      _("/Go/Next Node"),
				      _("/Go/Ten Nodes Forward"),
				      _("/Go/Variation Last Node"), NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->navigation_toolbar,
					   current_node->child != NULL,
					   &navigation_toolbar_forward,
					   &navigation_toolbar_variation_end,
					   NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      (current_node->parent != NULL
				       && (current_node->parent->child
					   != current_node)),
				      _("/Go/Previous Variation"), NULL);
  gtk_utils_set_toolbar_buttons_sensitive
    (goban_window->navigation_toolbar,
     (current_node->parent != NULL
      && current_node->parent->child != current_node),
     &navigation_toolbar_previous_variation, NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      current_node->next != NULL,
				      _("/Go/Next Variation"), NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->navigation_toolbar,
					   current_node->next != NULL,
					   &navigation_toolbar_next_variation,
					   NULL);

  goban_window->switching_x = NULL_X;
  goban_window->switching_y = NULL_Y;

  goban_window->last_displayed_node = current_node;
}


static void
update_game_information (GtkGobanWindow *goban_window)
{
  SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;

  update_window_title (goban_window, game_info_node);

  update_player_information (game_info_node,
			     goban_window->player_labels[BLACK_INDEX],
			     SGF_PLAYER_BLACK, SGF_BLACK_RANK, SGF_BLACK_TEAM);
  update_player_information (game_info_node,
			     goban_window->player_labels[WHITE_INDEX],
			     SGF_PLAYER_WHITE, SGF_WHITE_RANK, SGF_WHITE_TEAM);

  goban_window->last_game_info_node = game_info_node;

  if (goban_window->game_info_dialog) {
    gtk_game_info_dialog_set_node (goban_window->game_info_dialog,
				   goban_window->current_tree, game_info_node);
  }
}


static void
update_window_title (GtkGobanWindow *goban_window,
		     const SgfNode *game_info_node)
{
  const char *game_name = NULL;
  char *string_to_free = NULL;
  char *base_name = (goban_window->filename
		     ? g_path_get_basename (goban_window->filename) : NULL);
  char *title;

  if (game_info_node) {
    game_name = sgf_node_get_text_property_value (game_info_node,
						  SGF_GAME_NAME);

    if (!game_name) {
      const char *white_player
	= sgf_node_get_text_property_value (game_info_node, SGF_PLAYER_WHITE);
      const char *black_player
	= sgf_node_get_text_property_value (game_info_node, SGF_PLAYER_BLACK);

      if (white_player && black_player) {
	string_to_free = g_strdup_printf (_("%s (W) vs. %s"),
					  white_player, black_player);
	game_name = string_to_free;
      }
    }
  }

  if (game_name) {
    if (base_name) {
      title = utils_cat_strings (NULL, game_name, " (", base_name, ")", NULL);
      gtk_window_set_title (GTK_WINDOW (goban_window), title);
      utils_free (title);
    }
    else
      gtk_window_set_title (GTK_WINDOW (goban_window), game_name);
  }
  else if (base_name)
    gtk_window_set_title (GTK_WINDOW (goban_window), base_name);

  g_free (string_to_free);
  g_free (base_name);
}


static void
update_player_information (const SgfNode *game_info_node,
			   GtkLabel *player_label,
			   SgfType name_property, SgfType rank_property,
			   SgfType team_property)
{
  const char *name = NULL;
  const char *rank = NULL;
  const char *team = NULL;
  char *label_text;

  if (game_info_node) {
    name = sgf_node_get_text_property_value (game_info_node, name_property);
    rank = sgf_node_get_text_property_value (game_info_node, rank_property);
    team = sgf_node_get_text_property_value (game_info_node, team_property);
  }

  label_text = utils_duplicate_string (name ? name : _("[unknown]"));

  if (rank) {
    /* Heuristic: add a comma if the rank doesn't begin with a number
     * (i.e. "Honinbo".)
     */
    label_text = utils_cat_strings (label_text,
				    '0' <= *rank && *rank <= '9' ? " " : ", ",
				    rank, NULL);
  }

  if (team)
    label_text = utils_cat_strings (label_text, " (", team, ")", NULL);

  gtk_label_set_text (player_label, label_text);
  utils_free (label_text);
}


static void
update_game_specific_information (GtkGobanWindow *goban_window)
{
  const Board *board = goban_window->board;
  gchar *black_string;
  gchar *white_string;

  if (board->game == GAME_GO) {
    const SgfNode *game_info_node
      = goban_window->sgf_board_state.game_info_node;
    double komi;

    black_string
      = g_strdup_printf (ngettext ("%d capture", "%d captures",
				   board->data.go.prisoners[BLACK_INDEX]),
			 board->data.go.prisoners[BLACK_INDEX]);

    if (game_info_node
	&& sgf_node_get_komi (game_info_node, &komi) && komi != 0.0) {
      white_string
	= g_strdup_printf (ngettext ("%d capture %c %.*f komi",
				     "%d captures %c %.*f komi",
				     board->data.go.prisoners[WHITE_INDEX]),
			   board->data.go.prisoners[WHITE_INDEX],
			   (komi > 0.0 ? '+' : '-'),
			   ((int) floor (komi * 100.0 + 0.5) % 10 == 0
			    ? 1 : 2),
			   fabs (komi));
    }
    else {
      white_string
	= g_strdup_printf (ngettext ("%d capture", "%d captures",
				     board->data.go.prisoners[WHITE_INDEX]),
			   board->data.go.prisoners[WHITE_INDEX]);
    }
  }
  else if (board->game == GAME_OTHELLO) {
    int num_black_disks;
    int num_white_disks;

    othello_count_disks (board, &num_black_disks, &num_white_disks);
    black_string = g_strdup_printf (ngettext ("%d disk", "%d disks",
					      num_black_disks),
				    num_black_disks);
    white_string = g_strdup_printf (ngettext ("%d disk", "%d disks",
					      num_white_disks),
				    num_white_disks);
  }
  else
    return;

  gtk_label_set_text (goban_window->game_specific_info[BLACK_INDEX],
		      black_string);
  gtk_label_set_text (goban_window->game_specific_info[WHITE_INDEX],
		      white_string);

  g_free (black_string);
  g_free (white_string);
}


static void
update_move_information (GtkGobanWindow *goban_window)
{
  const SgfNode *move_node = goban_window->sgf_board_state.last_move_node;
  const SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;
  SgfResult result = SGF_RESULT_NOT_SET;
  gboolean result_is_final;
  double score;
  char buffer[512];
  int length;

  if (move_node) {
    if (goban_window->current_tree->current_node == move_node) {
      length = sprintf (buffer, _("Move %d: "),
			goban_window->board->move_number);
    }
    else {
      length = sprintf (buffer, _("Last move: %u, "),
			goban_window->board->move_number);
    }

    length += sgf_utils_format_node_move (goban_window->current_tree,
					  move_node, buffer + length,
					  _("B "), _("W "), _("pass"));
  }
  else
    length = sprintf (buffer, _("Game beginning"));

  if (game_info_node
      && !goban_window->current_tree->current_node->child
      && !goban_window->sgf_board_state.last_main_variation_node)
    result = sgf_node_get_result (game_info_node, &score);

  result_is_final = (result != SGF_RESULT_NOT_SET
		     && result != SGF_RESULT_UNKNOWN
		     && result != SGF_RESULT_VOID
		     && result != SGF_RESULT_INVALID);

  if (!result_is_final
      && !board_is_game_over (goban_window->board, RULE_SET_DEFAULT,
			      goban_window->sgf_board_state.color_to_play)) {
    strcpy (buffer + length,
	    (goban_window->sgf_board_state.color_to_play == BLACK
	     ? _("; black to play") : _("; white to play")));
  }
  else
    strcpy (buffer + length, _("; game over"));

  if (result_is_final) {
    length += strlen (buffer + length);
    *(buffer + length++) = '\n';

    switch (result) {
    case SGF_RESULT_BLACK_WIN:
      strcpy (buffer + length, _("Black wins"));
      break;
    case SGF_RESULT_WHITE_WIN:
      strcpy (buffer + length, _("White wins"));
      break;

    case SGF_RESULT_BLACK_WIN_BY_FORFEIT:
      strcpy (buffer + length, _("Black wins by forfeit"));
      break;
    case SGF_RESULT_WHITE_WIN_BY_FORFEIT:
      strcpy (buffer + length, _("White wins by forfeit"));
      break;

    case SGF_RESULT_BLACK_WIN_BY_RESIGNATION:
      strcpy (buffer + length, _("White resigns"));
      break;
    case SGF_RESULT_WHITE_WIN_BY_RESIGNATION:
      strcpy (buffer + length, _("Black resigns"));
      break;

    case SGF_RESULT_BLACK_WIN_BY_SCORE:
    case SGF_RESULT_WHITE_WIN_BY_SCORE:
      {
	int num_digits
	  = (fabs (score - floor (score + 0.005)) >= 0.005
	     ? ((score + 0.005) * 10 - floor ((score + 0.005) * 10) >= 0.1
		? 2 : 1)
	     : 0);

	sprintf (buffer + length,
		 (result == SGF_RESULT_BLACK_WIN_BY_SCORE
		  ? _("Black wins by %.*f") : _("White wins by %.*f")),
		 num_digits, score);
      }

      break;

    case SGF_RESULT_BLACK_WIN_BY_TIME:
      strcpy (buffer + length, _("White runs out of time and loses"));
      break;
    case SGF_RESULT_WHITE_WIN_BY_TIME:
      strcpy (buffer + length, _("Black runs out of time and loses"));
      break;

    case SGF_RESULT_DRAW:
      strcpy (buffer + length, _("Game is draw"));
      break;

    default:
      assert (0);
    }
  }

  gtk_label_set_text (goban_window->move_information_label, buffer);
}


static void
fetch_comment_if_changed (GtkGobanWindow *goban_window,
			  gboolean for_current_node)
{
  if (gtk_text_buffer_get_modified (goban_window->text_buffer)) {
    SgfNode *node = (for_current_node
		     ? goban_window->current_tree->current_node
		     : goban_window->last_displayed_node);
    GtkTextIter start_iterator;
    GtkTextIter end_iterator;
    gchar *new_comment;
    char *normalized_comment;

    gtk_text_buffer_get_bounds (goban_window->text_buffer,
				&start_iterator, &end_iterator);
    new_comment = gtk_text_iter_get_text (&start_iterator, &end_iterator);

    /* FIXME: When we track changes in SGF trees for undo history and
     *	      modified-flag, we need to only do this when the comment
     *	      is really changed.
     */
    normalized_comment = sgf_utils_normalize_text (new_comment, 0);
    if (normalized_comment) {
      sgf_node_add_text_property (node, goban_window->current_tree,
				  SGF_COMMENT, normalized_comment, 1);
    }
    else
      sgf_node_delete_property (node, goban_window->current_tree, SGF_COMMENT);

    g_free (new_comment);

    gtk_text_buffer_set_modified (goban_window->text_buffer, FALSE);
  }
}



static int
initialize_gtp_player (GtpClient *client, int successful,
		       GtkGobanWindow *goban_window, ...)
{
  const SgfGameTree *game_tree = goban_window->current_tree;
  const SgfNode *root_node = game_tree->root;
  int client_color = (client == goban_window->players[BLACK_INDEX]
		      ? BLACK : WHITE);
  int client_color_index = COLOR_INDEX (client_color);
  int *initialization_step = (goban_window->player_initialization_step
			      + client_color_index);

  /* FIXME */
  assert (successful);

  /* These special cases are needed to avoid nasty `goto's in `switch'
   * block below.
   */
  if (*initialization_step == INITIALIZATION_FIXED_HANDICAP_SET) {
    /* FIXME: Validate handicap position. */
    *initialization_step = INITIALIZATION_HANDICAP_SET;
  }
  else if (*initialization_step == INITIALIZATION_FREE_HANDICAP_PLACED) {
    va_list arguments;

    va_start (arguments, goban_window);
    free_handicap_has_been_placed (goban_window,
				   va_arg (arguments, BoardPositionList *));
    va_end (arguments);

    *initialization_step = INITIALIZATION_HANDICAP_SET;
  }

  /* Note that `case' branches here often fall through to next ones.
   * Often certain initializations are just not needed at all.
   */
  switch (*initialization_step) {
  case INITIALIZATION_NOT_STARTED:
    if (gtp_client_is_known_command (client, "set_game")) {
      *initialization_step = INITIALIZATION_GAME_SET;
      gtp_client_set_game (client,
			   (GtpClientResponseCallback) initialize_gtp_player,
			   goban_window, game_tree->game);
      break;
    }

  case  INITIALIZATION_GAME_SET:
    *initialization_step = INITIALIZATION_BOARD_SIZE_SET;

    assert (game_tree->board_width == game_tree->board_height);
    gtp_client_set_board_size (client,
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
	gtp_client_send_time_settings (client,
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
      int handicap = sgf_node_get_handicap (root_node);

      if (handicap > 0) {
	gboolean is_fixed_handicap = FALSE;
	const BoardPositionList *handicap_stones
	  = sgf_node_get_list_of_point_property_value (root_node,
						       SGF_ADD_BLACK);

	if (handicap_stones
	    && (handicap_stones->num_positions
		<= go_get_max_fixed_handicap (game_tree->board_width,
					      game_tree->board_height))) {
	  BoardPositionList *fixed_handicap_stones
	    = go_get_fixed_handicap_stones (game_tree->board_width,
					    game_tree->board_height,
					    handicap);

	  if (board_position_lists_are_equal (fixed_handicap_stones,
					      handicap_stones))
	    is_fixed_handicap = TRUE;

	  board_position_list_delete (fixed_handicap_stones);
	}

	if (is_fixed_handicap) {
	  *initialization_step = INITIALIZATION_FIXED_HANDICAP_SET;
	  gtp_client_set_fixed_handicap (client,
					 ((GtpClientResponseCallback)
					  initialize_gtp_player),
					 goban_window, handicap);
	  break;
	}
	else {
	  if (client_color == BLACK) {
	    *initialization_step = INITIALIZATION_FREE_HANDICAP_PLACED;
	    gtp_client_place_free_handicap (client,
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
	      gtp_client_set_free_handicap (client, NULL, NULL,
					    handicap_stones);
	    }
	  }
	}
      }
    }

  case INITIALIZATION_HANDICAP_SET:
    if (game_tree->game == GAME_GO) {
      double komi;

      if (!sgf_node_get_komi (game_tree->current_node, &komi)) {
	/* Recently suggested default komi is 6.5, but if a game lacks
	 * `KM' property it is more likely to be old.
	 */
	komi = (sgf_node_get_handicap (root_node) > 0 ? 0.5 : 5.5);
      }

      gtp_client_set_komi (client, NULL, NULL, komi);
    }

    *initialization_step = INITIALIZATION_COMPLETE;

    {
      const SgfNode *node;

      for (node = root_node; node; node = node->current_variation) {
	if (IS_STONE (node->move_color)) {
	  gtp_client_play_move_from_sgf_node (client, NULL, NULL,
					      game_tree, node);
	}
      }
    }

    if (client_color
	== goban_window->game_position_board_state->color_to_play) {
      generate_move_via_gtp (goban_window);
      start_clock_if_needed (goban_window);
    }

    break;

  default:
    /* Must never happen. */
    assert (0);
  }

  return 1;
}


static void
free_handicap_has_been_placed (GtkGobanWindow *goban_window,
			       BoardPositionList *handicap_stones)
{
  sgf_utils_add_free_handicap_stones (goban_window->current_tree,
				      handicap_stones);

  reenter_current_node (goban_window);
  assert (goban_window->game_position_board_state->color_to_play == WHITE);

  if (GTP_ENGINE_CAN_PLAY_MOVES (goban_window, WHITE)) {
    /* The engine is initialized, but since free handicap placement
     * only became known at this point, the engine doesn't know about
     * it yet.
     */
    gtp_client_set_free_handicap (goban_window->players[WHITE_INDEX],
				  NULL, NULL, handicap_stones);
    generate_move_via_gtp (goban_window);
  }

  if (USER_IS_TO_PLAY (goban_window)
      || GTP_ENGINE_CAN_PLAY_MOVES (goban_window, WHITE))
    start_clock_if_needed (goban_window);
}


static void
move_has_been_played (GtkGobanWindow *goban_window)
{
  SgfNode *move_node
    = goban_window->game_position_board_state->last_move_node;
  TimeControl *time_control
    = goban_window->time_controls[COLOR_INDEX (move_node->move_color)];
  int color_to_play = goban_window->game_position_board_state->color_to_play;

  if (time_control) {
    gint moves_left;
    double seconds_left = time_control_stop (time_control, &moves_left);

    gtk_clock_time_control_state_changed
      (goban_window->clocks[COLOR_INDEX (move_node->move_color)]);

    sgf_node_add_real_property (move_node, goban_window->current_tree,
				(move_node->move_color == BLACK
				 ? SGF_TIME_LEFT_4BLACK
				 : SGF_TIME_LEFT_4WHITE),
				floor (seconds_left * 1000.0 + 0.5) / 1000.0,
				0);

    if (moves_left) {
      sgf_node_add_number_property (move_node, goban_window->current_tree,
				    (move_node->move_color == BLACK
				     ? SGF_MOVES_LEFT_4BLACK
				     : SGF_MOVES_LEFT_4WHITE),
				    moves_left, 0);
    }
  }

  if (GTP_ENGINE_CAN_PLAY_MOVES (goban_window,
				 OTHER_COLOR (move_node->move_color))) {
    GtpClient *other_player
      = goban_window->players[COLOR_INDEX (OTHER_COLOR
					   (move_node->move_color))];

    /* Other player is a GTP engine which is already initialized.
     * Inform it about the move that has just been played.
     */
    gtp_client_play_move_from_sgf_node (other_player, NULL, NULL,
					goban_window->current_tree, move_node);
  }

  if (!board_is_game_over (goban_window->board, RULE_SET_DEFAULT,
			   color_to_play)) {
    if (GTP_ENGINE_CAN_PLAY_MOVES (goban_window, color_to_play)) {
      /* If the next move is to be played by a GTP engine and the engine
       * is ready, ask for a move now.
       */
      generate_move_via_gtp (goban_window);
    }
    else if (goban_window->players[COLOR_INDEX (color_to_play)])
      return;

    start_clock_if_needed (goban_window);
  }
  else {
    SgfNode *game_info_node
      = goban_window->game_position_board_state->game_info_node;

    switch (goban_window->board->game) {
    case GAME_GO:
      {
	int player;

	goban_window->dead_stones = g_malloc (BOARD_GRID_SIZE * sizeof (char));
	board_fill_grid (goban_window->board, goban_window->dead_stones, 0);

	goban_window->scoring_engine_player = -1;

	for (player = 0; player < NUM_COLORS; player++) {
	  if (goban_window->players[player]) {
	    goban_window->dead_stones_list = NULL;
	    goban_window->engine_scoring_cancelled = FALSE;
	    goban_window->scoring_progress_dialog
	      = ((GtkProgressDialog *)
		 gtk_progress_dialog_new (NULL,
					  "Quarry",
					  _("GTP engine is scoring..."),
					  NULL,
					  ((GtkProgressDialogCallback)
					   cancel_scoring),
					  goban_window));
	    gtk_progress_dialog_set_fraction ((goban_window
					       ->scoring_progress_dialog),
					      0.0, NULL);
	    goban_window->scoring_engine_player = player;
	    gtp_client_final_status_list (goban_window->players[player],
					  (GtpClientFinalStatusListCallback)
					  engine_has_scored,
					  goban_window,
					  GTP_DEAD);
	    break;
	  }
	}

	if (goban_window->scoring_engine_player == -1)
	  enter_scoring_mode (goban_window);
      }

      break;

    case GAME_AMAZONS:
      {
	char *result = utils_duplicate_string (move_node->move_color == BLACK
					       ? "B+" : "W+");

	sgf_node_add_text_property (game_info_node, goban_window->current_tree,
				    SGF_RESULT, result, 1);
      }

      break;

    case GAME_OTHELLO:
      {
	int num_black_disks;
	int num_white_disks;

	othello_count_disks (goban_window->board,
			     &num_black_disks, &num_white_disks);
	sgf_node_add_score_result (game_info_node, goban_window->current_tree,
				   num_black_disks - num_white_disks, 1);
      }

      break;

    default:
      assert (0);
    }

    leave_game_mode (goban_window);
  }
}


static void
engine_has_scored (GtpClient *client, int successful,
		   GtkGobanWindow *goban_window,
		   GtpStoneStatus status, BoardPositionList *dead_stones)
{
  int player;

  UNUSED (client);

  if (goban_window->engine_scoring_cancelled)
    return;

  if (successful) {
    assert (status == GTP_DEAD);

    board_position_list_mark_on_grid (dead_stones,
				      goban_window->dead_stones, 1);
  }

  for (player = goban_window->scoring_engine_player + 1;
       player < NUM_COLORS;
       player++) {
    if (goban_window->players[player]) {
      /* Store dead stone list of first engine and let second engine
       * score.
       */
      goban_window->dead_stones_list
	= board_position_list_duplicate (dead_stones);
      gtk_progress_dialog_set_fraction (goban_window->scoring_progress_dialog,
					0.5, NULL);
      goban_window->scoring_engine_player = player;
      gtp_client_final_status_list (goban_window->players[player],
				    ((GtpClientFinalStatusListCallback)
				     engine_has_scored),
				    goban_window,
				    GTP_DEAD);
      break;
    }
  }

  if (goban_window->scoring_engine_player != player) {
    gtk_widget_destroy ((GtkWidget*) goban_window->scoring_progress_dialog);

    if (!goban_window->dead_stones_list
	|| !board_position_lists_are_equal (goban_window->dead_stones_list,
					    dead_stones)) {
      /* Either one human player or engines disagree. */
      enter_scoring_mode (goban_window);
    }
    else
      go_scoring_mode_done (goban_window);

    if (goban_window->dead_stones_list) {
      board_position_list_delete (goban_window->dead_stones_list);
      goban_window->dead_stones_list = NULL;
    }
  }
}


static void
cancel_scoring (GtkProgressDialog *progress_dialog,
		GtkGobanWindow *goban_window)
{
  /* TODO: Would be nice to tell the GTP client about the
   *	   cancellation.  /mh
   *
   *       With GTP 2 it is not possible to tell the engine to cancel
   *       a command execution.  Or do you mean to have the client not
   *       invoke engine_has_scored() callback?  That might be a good
   *       idea.  /pp
   */
  goban_window->engine_scoring_cancelled = TRUE;
  gtk_widget_destroy (GTK_WIDGET (progress_dialog));
  enter_scoring_mode (goban_window);
}


static void
enter_scoring_mode (GtkGobanWindow *goban_window)
{
  enter_special_mode (goban_window,
		      _("Please select dead stones\nto score the game"),
		      go_scoring_mode_done, NULL);
  set_goban_signal_handlers (goban_window,
			     G_CALLBACK (go_scoring_mode_pointer_moved),
			     G_CALLBACK (go_scoring_mode_goban_clicked));

  update_territory_markup (goban_window);
}


static void
move_has_been_generated (GtpClient *client, int successful,
			 GtkGobanWindow *goban_window,
			 int color, int x, int y,
			 BoardAbstractMoveData *move_data)
{
  UNUSED (client);

  /* If the engine has run out of time, we are not in game mode by
   * now.
   */
  if (goban_window->in_game_mode && successful) {
    SgfGameTree *current_tree = goban_window->current_tree;
    SgfGameTreeState tree_state;

    if (x == RESIGNATION_X && y == RESIGNATION_Y) {
      do_resign_game (goban_window);
      return;
    }

    if (!IS_DISPLAYING_GAME_NODE (goban_window)) {
      sgf_game_tree_get_state (current_tree, &tree_state);
      sgf_game_tree_set_state (current_tree, &goban_window->game_position);
    }

    /* FIXME: Validate move and alert if it is illegal. */

    if (goban_window->current_tree->game != GAME_AMAZONS) {
      sgf_utils_append_variation (current_tree,
				  goban_window->game_position_board_state,
				  color, x, y);
    }
    else {
      sgf_utils_append_variation (current_tree,
				  goban_window->game_position_board_state,
				  color, x, y, move_data->amazons);
    }

    if (IS_DISPLAYING_GAME_NODE (goban_window))
      update_children_for_new_node (goban_window);
    else {
      goban_window->game_position.current_node = current_tree->current_node;
      sgf_game_tree_set_state (current_tree, &tree_state);

      gtk_sgf_tree_view_update_view_port (goban_window->sgf_tree_view);
      show_sgf_tree_view_automatically
	(goban_window, goban_window->game_position.current_node);
    }

    move_has_been_played (goban_window);
  }
}


static void
generate_move_via_gtp (GtkGobanWindow *goban_window)
{
  int color_to_play = goban_window->game_position_board_state->color_to_play;
  int color_to_play_index = COLOR_INDEX (color_to_play);
  TimeControl *time_control = goban_window->time_controls[color_to_play_index];

  if (time_control) {
    int moves_left;
    double seconds_left = time_control_get_time_left (time_control,
						      &moves_left);

    /* FIXME: OUT_OF_TIME shouldn't really happen here. */
    if (seconds_left != NO_TIME_LIMITS && seconds_left != OUT_OF_TIME) {
      gtp_client_send_time_left (goban_window->players[color_to_play_index],
				 NULL, NULL, color_to_play,
				 floor (seconds_left + 0.5), moves_left);
    }
  }

  gtp_client_generate_move (goban_window->players[color_to_play_index],
			    (GtpClientMoveCallback) move_has_been_generated,
			    goban_window, color_to_play);
}


static void
start_clock_if_needed (GtkGobanWindow *goban_window)
{
  int color_to_play_index
    = COLOR_INDEX (goban_window->game_position_board_state->color_to_play);

  if (goban_window->time_controls[color_to_play_index]) {
    time_control_start (goban_window->time_controls[color_to_play_index]);
    gtk_clock_time_control_state_changed
      (goban_window->clocks[color_to_play_index]);
  }
}


/* FIXME; Almost identical to do_resign_game(). */
static void
player_is_out_of_time (GtkClock *clock, GtkGobanWindow *goban_window)
{
  int winner_color_char = (goban_window->clocks[BLACK_INDEX] == clock
			   ? 'W' : 'B');
  SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;

  leave_game_mode (goban_window);

  sgf_node_add_text_property (game_info_node, goban_window->current_tree,
			      SGF_RESULT,
			      utils_cprintf ("%c+Time", winner_color_char), 1);

  sgf_utils_append_variation (goban_window->current_tree,
			      &goban_window->sgf_board_state, EMPTY);
  update_children_for_new_node (goban_window);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
