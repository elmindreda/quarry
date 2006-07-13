/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003 Paul Pogonyshev.                             *
 * Copyright (C) 2004 Paul Pogonyshev and Martin Holters.          *
 * Copyright (C) 2005, 2006 Paul Pogonyshev.                       *
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


/* FIXME: Certain parts can be simplified by switching from explicit
 *	  function calls to hooking to the new GtkSgfTreeSignalProxy
 *	  signals.
 *
 * FIXME: Current synchronization between the "Tools" menu in the main
 *	  window menu and the menu available from the editing toolbar
 *	  is hackish and ugly.  Should be possible to improve when we
 *	  drop support for pre-2.4 GTK+.
 *
 * FIXME: This file should be split up, it is too large.
 */


#include "gtk-goban-window.h"

#include "gtk-add-or-edit-label-dialog.h"
#include "gtk-clock.h"
#include "gtk-configuration.h"
#include "gtk-control-center.h"
#include "gtk-file-dialog.h"
#include "gtk-game-info-dialog.h"
#include "gtk-goban.h"
#include "gtk-go-to-named-node-dialog.h"
#include "gtk-gtp-client-interface.h"
#include "gtk-help.h"
#include "gtk-named-vbox.h"
#include "gtk-new-game-dialog.h"
#include "gtk-new-game-record-dialog.h"
#include "gtk-parser-interface.h"
#include "gtk-preferences.h"
#include "gtk-qhbox.h"
#include "gtk-qvbox.h"
#include "gtk-resume-game-dialog.h"
#include "gtk-sgf-tree-signal-proxy.h"
#include "gtk-sgf-tree-view.h"
#include "gtk-utils.h"
#include "quarry-find-dialog.h"
#include "quarry-marshal.h"
#include "quarry-message-dialog.h"
#include "quarry-move-number-dialog.h"
#include "quarry-save-confirmation-dialog.h"
#include "quarry-stock.h"
#include "quarry-text-buffer.h"
#include "gui-utils.h"
#include "time-control.h"
#include "gtp-client.h"
#include "sgf.h"
#include "board.h"
#include "game-info.h"
#include "utils.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <string.h>


#define NAVIGATE_FAST_NUM_MOVES	10

#define IS_DISPLAYING_GAME_NODE(goban_window)				\
  ((goban_window)->game_position.board_state				\
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
			    ->game_position.board_state			\
			    ->color_to_play)]))


#define IS_IN_SPECIAL_MODE(goban_window)				\
  GTK_WIDGET_VISIBLE ((goban_window)->mode_information_vbox)


enum {
  INITIALIZATION_NOT_STARTED,
  INITIALIZATION_GAME_SET,
  INITIALIZATION_BOARD_SIZE_SET,
  INITIALIZATION_BOARD_CLEARED,
  INITIALIZATION_TIME_LIMITS_SET,
  INITIALIZATION_FIXED_HANDICAP_SET,
  INITIALIZATION_FREE_HANDICAP_PLACED,
  INITIALIZATION_HANDICAP_SET,
  INITIALIZATION_COMPLETE
};


typedef void (* SpecialModeButtonClicked) (GtkGobanWindow *goban_window);


enum {
  GTK_GOBAN_WINDOW_SAVE = 1,
  GTK_GOBAN_WINDOW_SAVE_AS,
  GTK_GOBAN_WINDOW_ADJOURN
};


enum {
  GTK_GOBAN_WINDOW_NON_LABELS_MODE,
  GTK_GOBAN_WINDOW_TEXT_LABELS_MODE,
  GTK_GOBAN_WINDOW_NUMERIC_LABELS_MODE
};


enum {
  GTK_GOBAN_WINDOW_HIDE_CHILD = FALSE,
  GTK_GOBAN_WINDOW_SHOW_CHILD = TRUE,
  GTK_GOBAN_WINDOW_TOGGLE_CHILD
};


typedef struct _GtkGobanWindowUndoEntryData	GtkGobanWindowUndoEntryData;
typedef struct _GtkGobanWindowStateData		GtkGobanWindowStateData;

struct _GtkGobanWindowUndoEntryData {
  GtkGobanWindow	     *goban_window;
  QuarryTextBufferUndoEntry  *undo_entry;
};

struct _GtkGobanWindowStateData {
  GtkGobanWindow	     *goban_window;
  QuarryTextBufferState	      state_before;
  QuarryTextBufferState	      state_after;
  const SgfNode		     *sgf_node_after;
};


typedef struct _SgfCopyData			SgfCopyData;

struct _SgfCopyData {
  char			     *sgf;
  int			      sgf_length;
};


static void	 gtk_goban_window_class_init (GtkGobanWindowClass *class);
static void	 gtk_goban_window_init (GtkGobanWindow *goban_window);

static void	 gtk_goban_window_destroy (GtkObject *object);
static void	 gtk_goban_window_finalize (GObject *object);

static void	 force_minimal_width (GtkWidget *label,
				      GtkRequisition *requisition);

static void	 do_enter_game_mode (GtkGobanWindow *goban_window);
static void	 leave_game_mode (GtkGobanWindow *goban_window);

static void	 collection_modification_state_changed
		   (SgfCollection *sgf_collection, void *user_data);
static void	 undo_or_redo_availability_changed
		   (SgfUndoHistory *undo_history, void *user_data);

static void	 gtk_goban_window_save (GtkGobanWindow *goban_window,
					guint callback_action);
static char *	 suggest_filename (GtkGobanWindow *goban_window);
static void	 save_file_as_response (GtkWidget *file_dialog,
					gint response_id,
					GtkGobanWindow *goban_window);
static gboolean  do_save_collection (GtkGobanWindow *goban_window,
				     const gchar *filename);
static void	 game_has_been_adjourned (GtkGobanWindow *goban_window);

static void	 export_ascii_diagram (GtkGobanWindow *goban_window);
static void	 export_senseis_library_diagram (GtkGobanWindow *goban_window);
static void	 do_export_diagram (GtkGobanWindow *goban_window,
				    char *diagram_string,
				    const gchar *message);

static void	 gtk_goban_window_close (GtkGobanWindow *goban_window);

static void	 show_find_dialog (GtkGobanWindow *goban_window);
static void	 find_dialog_response (QuarryFindDialog *find_dialog,
				       gint response_id,
				       GtkGobanWindow *goban_window);
static gboolean	 do_find_text (GtkGobanWindow *goban_window,
			       QuarryFindDialogSearchDirection direction);

static void	 show_game_information_dialog (GtkGobanWindow *goban_window);
static void	 game_info_dialog_property_changed
		   (GtkGobanWindow *goban_window, SgfType sgf_property_type);

static void	 show_preferences_dialog (void);

static void	 show_or_hide_main_toolbar (GtkGobanWindow *goban_window);
static void	 show_or_hide_editing_toolbar (GtkGobanWindow *goban_window);
static void	 show_or_hide_navigation_toolbar
		   (GtkGobanWindow *goban_window);
static void	 show_or_hide_game_action_buttons
		   (GtkGobanWindow *goban_window);
static void	 show_or_hide_sgf_tree_view (GtkGobanWindow *goban_window,
					     guint callback_action);
static void	 recenter_sgf_tree_view (GtkGobanWindow *goban_window);

static void	 show_sgf_tree_view_automatically
		   (GtkGobanWindow *goban_window, const SgfNode *sgf_node);

#ifdef GTK_TYPE_GO_TO_NAMED_NODE_DIALOG
static void	 show_go_to_named_node_dialog (GtkGobanWindow *goban_window);
#endif

static void	 show_about_dialog (void);
static void	 show_help_contents (void);

static void	 tools_option_menu_changed
		   (GtkOptionMenu *option_menu,
		    const GtkGobanWindow *goban_window);
static void	 synchronize_tools_menus (const GtkGobanWindow *goban_window);

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
static void	 go_scoring_mode_cancel (GtkGobanWindow *goban_window);
static void	 handle_go_scoring_results (GtkGobanWindow *goban_window);

static void	 free_handicap_mode_done (GtkGobanWindow *goban_window);


static void	 play_pass_move (GtkGobanWindow *goban_window);
static void	 resign_game (GtkGobanWindow *goban_window);
static void	 do_resign_game (GtkGobanWindow *goban_window);


static void	 set_current_tree (GtkGobanWindow *goban_window,
				   SgfGameTree *sgf_tree);
static void	 set_time_controls (GtkGobanWindow *goban_window,
				    TimeControl *time_control);
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
		 setup_mode_pointer_moved (GtkGobanWindow *goban_window,
					   GtkGobanPointerData *data);
static void	 setup_mode_goban_clicked (GtkGobanWindow *goban_window,
					   GtkGobanClickData *data);

static GtkGobanPointerFeedback
		 markup_mode_pointer_moved (GtkGobanWindow *goban_window,
					    GtkGobanPointerData *data);
static void	 markup_mode_goban_clicked (GtkGobanWindow *goban_window,
					    GtkGobanClickData *data);

static GtkGobanPointerFeedback
		 label_mode_pointer_moved (GtkGobanWindow *goban_window,
					   GtkGobanPointerData *data);
static void	 label_mode_goban_clicked (GtkGobanWindow *goban_window,
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

static void	 delete_drawn_position_list (GtkGobanWindow *goban_window);
static void	 navigate_goban (GtkGobanWindow *goban_window,
				 GtkGobanNavigationCommand command);
static void	 switch_to_given_node (GtkGobanWindow *goban_window,
				       SgfNode *sgf_node);

static int	 find_variation_to_switch_to (GtkGobanWindow *goban_window,
					      int x, int y,
					      SgfDirection direction);

static void	 update_children_for_new_node (GtkGobanWindow *goban_window,
					       gboolean forced,
					       gboolean text_handled);
static void	 set_comment_and_node_name (GtkGobanWindow *goban_window,
					    const SgfNode *sgf_node);

static void	 update_game_information (GtkGobanWindow *goban_window);
static void	 update_window_title (GtkGobanWindow *goban_window);
static void	 update_player_information (GtkGobanWindow *goban_window,
					    int player_color);
static void	 update_game_specific_information
		   (const GtkGobanWindow *goban_window);
static void	 update_move_information (const GtkGobanWindow *goban_window);
static void	 update_commands_sensitivity
		   (const GtkGobanWindow *goban_window);
static void	 update_set_player_to_move_commands
		   (GtkGobanWindow *goban_window);

static void	 insert_node_name (GtkGobanWindow *goban_window,
				   const gchar *node_name);
static void	 select_node_name (GtkGobanWindow *goban_window);

static void	 text_buffer_insert_text (GtkTextBuffer *text_buffer,
					  GtkTextIter *insertion_iterator,
					  const gchar *text, guint length,
					  GtkGobanWindow *goban_window);
static void	 text_buffer_after_insert_text
		   (GtkTextBuffer *text_buffer,
		    GtkTextIter *insertion_iterator,
		    const gchar *text, guint length,
		    GtkGobanWindow *goban_window);
static void	 text_buffer_mark_set (GtkTextBuffer *text_buffer,
				       GtkTextIter *new_position_iterator,
				       GtkTextMark *mark,
				       GtkGobanWindow *goban_window);
static void	 text_buffer_end_user_action (GtkTextBuffer *text_buffer,
					      GtkGobanWindow *goban_window);
static gboolean	 text_buffer_receive_undo_entry
		   (QuarryTextBuffer *text_buffer,
		    QuarryTextBufferUndoEntry *undo_entry,
		    GtkGobanWindow *goban_window);

static void	 text_buffer_undo
		   (GtkGobanWindowUndoEntryData *undo_entry_data);
static void	 text_buffer_redo
		   (GtkGobanWindowUndoEntryData *undo_entry_data);

static void	 text_buffer_set_state_undo
		   (GtkGobanWindowStateData *state_data);
static void	 text_buffer_set_state_redo
		   (GtkGobanWindowStateData *state_data);

static QuarryTextBufferState *
		 update_comment_and_node_name_if_needed
		   (GtkGobanWindow *goban_window, gboolean for_current_node);
static gboolean	 fetch_comment_and_node_name (GtkGobanWindow *goban_window,
					      gboolean for_current_node);

static void	 set_move_number (GtkGobanWindow *goban_window);
static void	 set_player_to_move (GtkGobanWindow *goban_window,
				     guint callback_action,
				     GtkCheckMenuItem *menu_item);

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

static void	 undo_operation (GtkGobanWindow *goban_window);
static void	 redo_operation (GtkGobanWindow *goban_window);

static void	 cut_operation (GtkGobanWindow *goban_window);
static void	 copy_operation (GtkGobanWindow *goban_window);
static void	 paste_operation (GtkGobanWindow *goban_window);

static void	 get_copied_sgf (GtkClipboard *clipboard,
				 GtkSelectionData *selection_data,
				 guint format, gpointer user_data);
static void	 delete_copied_sgf (GtkClipboard *clipboard,
				    gpointer user_data);
static void	 receive_copied_sgf (GtkClipboard *clipboard,
				     GtkSelectionData *selection_data,
				     gpointer user_data);

static void	 append_empty_variation (GtkGobanWindow *goban_window);

static void	 swap_adjacent_branches (GtkGobanWindow *goban_window,
					 guint use_previous_branch);

static void	 delete_current_node (GtkGobanWindow *goban_window);
static void	 delete_current_node_children (GtkGobanWindow *goban_window);

static void	 activate_move_tool (GtkGobanWindow *goban_window,
				     guint callback_action,
				     GtkCheckMenuItem *menu_item);
static void	 activate_setup_tool (GtkGobanWindow *goban_window,
				      guint callback_action,
				      GtkCheckMenuItem *menu_item);
static void	 activate_scoring_tool (GtkGobanWindow *goban_window,
					guint callback_action,
					GtkCheckMenuItem *menu_item);
static void	 activate_markup_tool (GtkGobanWindow *goban_window,
				       gint sgf_markup_type,
				       GtkCheckMenuItem *menu_item);
static void	 activate_label_tool (GtkGobanWindow *goban_window,
				      gint labels_mode,
				      GtkCheckMenuItem *menu_item);


static GtkUtilsToolbarEntry toolbar_open = {
  N_("Open"),	N_("Open a game record"),		GTK_STOCK_OPEN,
  (GtkUtilsToolbarEntryCallback) gtk_parser_interface_present_default, 0
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


static GtkUtilsToolbarEntry editing_toolbar_undo = {
  N_("Undo"),	N_("Undo the last action"),		GTK_STOCK_UNDO,
  (GtkUtilsToolbarEntryCallback) undo_operation, 0
};

static GtkUtilsToolbarEntry editing_toolbar_redo = {
  N_("Redo"),	N_("Redo the last undone action"),	GTK_STOCK_REDO,
  (GtkUtilsToolbarEntryCallback) redo_operation, 0
};

static GtkUtilsToolbarEntry editing_toolbar_delete = {
  N_("Delete"),	N_("Delete the current node"),		GTK_STOCK_DELETE,
  (GtkUtilsToolbarEntryCallback) delete_current_node, 0
};


static GtkUtilsToolbarEntry navigation_toolbar_root = {
  N_("Root"),	N_("Go to the root node"),		GTK_STOCK_GOTO_FIRST,
  (GtkUtilsToolbarEntryCallback) navigate_goban, GOBAN_NAVIGATE_ROOT
};

static GtkUtilsToolbarEntry navigation_toolbar_back = {
  N_("Back"),	N_("Go to the previous node"),		GTK_STOCK_GO_BACK,
  (GtkUtilsToolbarEntryCallback) navigate_goban, GOBAN_NAVIGATE_BACK
};

static GtkUtilsToolbarEntry navigation_toolbar_forward = {
  N_("Forward"), N_("Go to the next node"),		GTK_STOCK_GO_FORWARD,
  (GtkUtilsToolbarEntryCallback) navigate_goban, GOBAN_NAVIGATE_FORWARD
};

static GtkUtilsToolbarEntry navigation_toolbar_variation_end = {
  N_("End"),	N_("Go to the current variation's last node"),
  GTK_STOCK_GOTO_LAST,
  (GtkUtilsToolbarEntryCallback) navigate_goban, GOBAN_NAVIGATE_VARIATION_END
};

static GtkUtilsToolbarEntry navigation_toolbar_previous_variation = {
  N_("Previous"), N_("Switch to the previous variation"), GTK_STOCK_GO_UP,
  (GtkUtilsToolbarEntryCallback) navigate_goban,
  GOBAN_NAVIGATE_PREVIOUS_VARIATION
};

static GtkUtilsToolbarEntry navigation_toolbar_next_variation = {
  N_("Next"),	N_("Switch to the next variation"),	GTK_STOCK_GO_DOWN,
  (GtkUtilsToolbarEntryCallback) navigate_goban, GOBAN_NAVIGATE_NEXT_VARIATION
};


static GtkWindowClass	*parent_class;

static guint		 clicked_signal_id;
static guint		 pointer_moved_signal_id;
static guint		 click_canceled_signal_id;
static guint		 goban_clicked_signal_id;


static GtkWindow	*about_dialog = NULL;


static GtkTextTag	*node_name_tag;
static GtkTextTag	*separator_tag;
static GtkTextTagTable	*node_name_tag_table;


static const SgfCustomUndoHistoryEntryData  quarry_text_buffer_undo_entry_data
  = { (SgfCustomOperationEntryFunction) text_buffer_undo,
      (SgfCustomOperationEntryFunction) text_buffer_redo,
      (SgfCustomOperationEntryFunction) g_free };

static const SgfCustomUndoHistoryEntryData  quarry_text_buffer_state_data
  = { (SgfCustomOperationEntryFunction) text_buffer_set_state_undo,
      (SgfCustomOperationEntryFunction) text_buffer_set_state_redo,
      (SgfCustomOperationEntryFunction) g_free };


GType
gtk_goban_window_get_type (void)
{
  static GType goban_window_type = 0;

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

  G_OBJECT_CLASS (class)->finalize	 = gtk_goban_window_finalize;

  GTK_OBJECT_CLASS (class)->destroy	 = gtk_goban_window_destroy;

  GTK_WIDGET_CLASS (class)->delete_event
    = ((gboolean (*) (GtkWidget *, GdkEventAny *))
       gtk_goban_window_stops_closing);

  /* Create the text tags and tag table we will be using in all
   * windows.  The tags will be used to highlight node name.
   */

  node_name_tag = gtk_text_tag_new (NULL);
  g_object_set (node_name_tag,
		"justification",      GTK_JUSTIFY_CENTER,
		"pixels-below-lines", QUARRY_SPACING,
		"underline",	      TRUE,
		"weight",	      PANGO_WEIGHT_BOLD,
		NULL);

  separator_tag = gtk_text_tag_new (NULL);
  g_object_set (separator_tag,
		"editable", FALSE,
		NULL);

  node_name_tag_table = gtk_text_tag_table_new ();
  gui_back_end_register_object_to_finalize (node_name_tag_table);

  gtk_text_tag_table_add (node_name_tag_table, node_name_tag);
  g_object_unref (node_name_tag);

  gtk_text_tag_table_add (node_name_tag_table, separator_tag);
  g_object_unref (separator_tag);
}


static void
gtk_goban_window_init (GtkGobanWindow *goban_window)
{
  static GtkItemFactoryEntry menu_entries[] = {
    { N_("/_File"), NULL, NULL, 0, "<Branch>" },
    { N_("/File/_New Game..."),		"<ctrl>N",
      gtk_new_game_dialog_present,	0,
      "<StockItem>",			GTK_STOCK_NEW },
    { N_("/File/Ne_w Game Record..."),	"<shift><ctrl>N",
      gtk_new_game_record_dialog_present, 0,
      "<Item>" },
    { N_("/File/"), NULL, NULL, 0, "<Separator>" },

    { N_("/File/_Open..."),		"<ctrl>O",
      gtk_parser_interface_present_default, 0,
      "<StockItem>",			GTK_STOCK_OPEN },
    { N_("/File/_Resume Game..."),	"",
      gtk_resume_game,			0,
      "<Item>" },

    { N_("/File/_Export..."), NULL, NULL, 0, "<Branch>" },
    { N_("/File/Export.../_ASCII Diagram"), NULL,
      export_ascii_diagram,		0,
      "<Item>" },
    { N_("/File/Export.../_Sensei's Library Diagram"), NULL,
      export_senseis_library_diagram,		0,
      "<Item>" },

    { N_("/File/"), NULL, NULL, 0, "<Separator>" },

    { N_("/File/_Save"),		"<ctrl>S",
      gtk_goban_window_save,		GTK_GOBAN_WINDOW_SAVE,
      "<StockItem>",			GTK_STOCK_SAVE },
    { N_("/File/Save _As..."),		"<shift><ctrl>S",
      gtk_goban_window_save,		GTK_GOBAN_WINDOW_SAVE_AS,
      "<StockItem>",			GTK_STOCK_SAVE_AS },
    { N_("/File/"), NULL, NULL, 0, "<Separator>" },

    { N_("/File/_Close"),		"<ctrl>W",
      gtk_goban_window_close,		0,
      "<StockItem>",			GTK_STOCK_CLOSE },
    { N_("/File/_Quit"),		"<ctrl>Q",
      gtk_control_center_quit,		0,
      "<StockItem>",			GTK_STOCK_QUIT },


    { N_("/_Edit"), NULL, NULL, 0, "<Branch>" },
    { N_("/Edit/_Undo"),		"<ctrl>Z",
      undo_operation,			0,
      "<StockItem>",			GTK_STOCK_UNDO },
    { N_("/Edit/_Redo"),		"<shift><ctrl>Z",
      redo_operation,			0,
      "<StockItem>",			GTK_STOCK_REDO },
    { N_("/Edit/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/Cu_t"),			"",
      cut_operation,			0,
      "<StockItem>",			GTK_STOCK_CUT },
    { N_("/Edit/_Copy"),		"",
      copy_operation,			0,
      "<StockItem>",			GTK_STOCK_COPY },
    { N_("/Edit/_Paste"),		"",
      paste_operation,			0,
      "<StockItem>",			GTK_STOCK_PASTE },
    { N_("/Edit/_Delete Node"),		"<alt>Delete",
      delete_current_node,		0,
      "<StockItem>",			GTK_STOCK_DELETE },
    { N_("/Edit/Delete Node's C_hildren"), "<shift><alt>Delete",
      delete_current_node_children,	0,
      "<Item>" },
    { N_("/Edit/"), NULL, NULL, 0, "<Separator>" },


    { N_("/Edit/T_ools"), NULL, NULL, 0, "<Branch>" },
    { N_("/Edit/Tools/_Move Tool"),	"<ctrl>M",
      activate_move_tool,		0,
      "<RadioItem>" },
    { N_("/Edit/Tools/_Setup Tool"),	"<ctrl><alt>S",
      activate_setup_tool,		0,
      "/Edit/Tools/Move Tool" },
    { N_("/Edit/Tools/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/Tools/C_ross Markup"),	"<ctrl>1",
      activate_markup_tool,		SGF_MARKUP_CROSS,
      "/Edit/Tools/Setup Tool" },
    { N_("/Edit/Tools/_Circle Markup"), "<ctrl>2",
      activate_markup_tool,		SGF_MARKUP_CIRCLE,
      "/Edit/Tools/Cross Markup" },
    { N_("/Edit/Tools/S_quare Markup"), "<ctrl>3",
      activate_markup_tool,		SGF_MARKUP_SQUARE,
      "/Edit/Tools/Circle Markup" },
    { N_("/Edit/Tools/_Triangle Markup"), "<ctrl>4",
      activate_markup_tool,		SGF_MARKUP_TRIANGLE,
      "/Edit/Tools/Square Markup" },
    { N_("/Edit/Tools/S_elected Markup"), "<ctrl>5",
      activate_markup_tool,		SGF_MARKUP_SELECTED,
      "/Edit/Tools/Triangle Markup" },
    { N_("/Edit/Tools/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/Tools/_Label Tool"),	"<ctrl>6",
      activate_label_tool,		GTK_GOBAN_WINDOW_TEXT_LABELS_MODE,
      "/Edit/Tools/Selected Markup" },
    { N_("/Edit/Tools/_Number Tool"),	"<ctrl>7",
      activate_label_tool,		GTK_GOBAN_WINDOW_NUMERIC_LABELS_MODE,
      "/Edit/Tools/Label Tool" },
    { N_("/Edit/Tools/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/Tools/Scori_ng Tool"),	NULL,
      activate_scoring_tool,		0,
      "/Edit/Tools/Number Tool" },

    { N_("/Edit/_Add Empty Node"),	NULL,
      append_empty_variation,		0,
      "<Item>" },
    { N_("/Edit/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/Move _Branch Up"),	NULL,
      swap_adjacent_branches,		TRUE,
      "<Item>" },
    { N_("/Edit/Move Branch Do_wn"),	NULL,
      swap_adjacent_branches,		FALSE,
      "<Item>" },
    { N_("/Edit/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/Edit Node _Name"),	"<ctrl><alt>N",
      select_node_name,			0,
      "<Item>" },
    { N_("/Edit/Set _Move Number"),	NULL,
      set_move_number,			0,
      "<Item>" },

    { N_("/Edit/P_layer to Move"), NULL, NULL, 0, "<Branch>" },
    { N_("/Edit/Player to Move/_White"), NULL,
      set_player_to_move,		WHITE,
      "<RadioItem>" },
    { N_("/Edit/Player to Move/_Black"), NULL,
      set_player_to_move,		BLACK,
      "/Edit/Player to Move/White" },
    { N_("/Edit/Player to Move/By Game _Rules"), NULL,
      set_player_to_move,		EMPTY,
      "/Edit/Player to Move/Black" },

    { N_("/Edit/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/_Find"),		"<ctrl>F",
      show_find_dialog,			0,
      "<StockItem>",			GTK_STOCK_FIND },
    { N_("/Edit/Find Ne_xt"),		"<ctrl>G",
      (GtkItemFactoryCallback) do_find_text, QUARRY_FIND_DIALOG_FIND_NEXT,
      "<Item>" },
    { N_("/Edit/Find Pre_vious"),	"<shift><ctrl>G",
      (GtkItemFactoryCallback) do_find_text, QUARRY_FIND_DIALOG_FIND_PREVIOUS,
      "<Item>" },
    { N_("/Edit/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/Game _Information"),	"<alt>Return",
      show_game_information_dialog,	0,
      "<StockItem>",			GTK_STOCK_PROPERTIES },
    { N_("/Edit/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Edit/Pr_eferences"),		"",
      show_preferences_dialog,		0,
      "<StockItem>",			GTK_STOCK_PREFERENCES },


    { N_("/_View"), NULL, NULL, 0, "<Branch>" },
    { N_("/View/_Main Toolbar"),	NULL,
      show_or_hide_main_toolbar,	0,
      "<CheckItem>" },
    { N_("/View/_Editing Toolbar"),	NULL,
      show_or_hide_editing_toolbar,	0,
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
    { N_("/View/_Recenter on Current Node"), "<ctrl><alt>C",
      recenter_sgf_tree_view,		0,
      "<Item>" },
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
      resign_game,			0,
      "<Item>" },
    { N_("/Play/"), NULL, NULL, 0, "<Separator>" },

    { N_("/Play/_Adjourn Game"),	"",
      gtk_goban_window_save,		GTK_GOBAN_WINDOW_ADJOURN,
      "<StockItem>",			GTK_STOCK_SAVE },


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

#ifdef GTK_TYPE_GO_TO_NAMED_NODE_DIALOG
    { N_("/Go/"), NULL, NULL, 0, "<Separator>" },
    { N_("/Go/_Go to Named Node..."),	"",
      show_go_to_named_node_dialog,	0,
      "<StockItem>",			GTK_STOCK_JUMP_TO },
#endif


    { N_("/_Help"), NULL, NULL, 0, "<Branch>" },
    { N_("/Help/_Contents"),		"F1",
      show_help_contents,		0,
      "<StockItem>",			GTK_STOCK_HELP },
    { N_("/Help/_About"),		NULL,
      show_about_dialog,		0,
      QUARRY_STOCK_MENU_ITEM_ABOUT }
  };

  static GtkItemFactoryEntry tools_menu_entries[] = {
    { N_("/Move Tool"),		"<ctrl>M",	NULL, 0, "<Item>" },
    { N_("/Setup Tool"),	"<ctrl><alt>S",	NULL, 0, "<Item>" },
    { "/", NULL, NULL, 0, "<Separator>" },

    { N_("/Cross Markup"),	"<ctrl>1",	NULL, 0, "<Item>" },
    { N_("/Circle Markup"),	"<ctrl>2",	NULL, 0, "<Item>" },
    { N_("/Square Markup"),	"<ctrl>3",	NULL, 0, "<Item>" },
    { N_("/Triangle Markup"),	"<ctrl>4",	NULL, 0, "<Item>" },
    { N_("/Selected Markup"),	"<ctrl>5",	NULL, 0, "<Item>" },
    { "/", NULL, NULL, 0, "<Separator>" },

    { N_("/Label Tool"),	"<ctrl>6",	NULL, 0, "<Item>" },
    { N_("/Number Tool"),	"<ctrl>7",	NULL, 0, "<Item>" },
    { "/", NULL, NULL, 0, "<Separator>" },

    { N_("/Scoring Tool"),	NULL,		NULL, 0, "<Item>" },
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
  GtkWidget *editing_toolbar;
  GtkWidget *editing_handle_box;
  GtkWidget *tools_option_menu;
  GtkWidget *navigation_toolbar;
  GtkWidget *navigation_handle_box;
  GtkAccelGroup *accel_group;
  int k;

  gtk_control_center_window_created (GTK_WINDOW (goban_window));

  /* Goban, the main thing in the window.  Signal handlers are
   * connected later.
   */
  goban = gtk_goban_new ();
  goban_window->goban = GTK_GOBAN (goban);

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
  goban_window->pass_button = gtk_button_new_with_label (_("Pass"));
  GTK_WIDGET_UNSET_FLAGS (goban_window->pass_button, GTK_CAN_FOCUS);

  g_signal_connect_swapped (goban_window->pass_button, "clicked",
			    G_CALLBACK (play_pass_move), goban_window);

  goban_window->resign_button = gtk_button_new_with_label (_("Resign"));
  GTK_WIDGET_UNSET_FLAGS (goban_window->resign_button, GTK_CAN_FOCUS);

  g_signal_connect_swapped (goban_window->resign_button, "clicked",
			    G_CALLBACK (resign_game), goban_window);

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

  /* Multipurpose text view and its text buffer. */
  goban_window->text_buffer = quarry_text_buffer_new (node_name_tag_table);

  g_signal_connect (goban_window->text_buffer, "insert-text",
		    G_CALLBACK (text_buffer_insert_text), goban_window);
  g_signal_connect_after (goban_window->text_buffer, "insert-text",
			  G_CALLBACK (text_buffer_after_insert_text),
			  goban_window);
  g_signal_connect (goban_window->text_buffer, "mark-set",
		    G_CALLBACK (text_buffer_mark_set), goban_window);
  g_signal_connect (goban_window->text_buffer, "end-user-action",
		    G_CALLBACK (text_buffer_end_user_action), goban_window);
  g_signal_connect (goban_window->text_buffer, "receive-undo-entry",
		    G_CALLBACK (text_buffer_receive_undo_entry), goban_window);

  text_view = gtk_text_view_new_with_buffer (goban_window->text_buffer);
  goban_window->text_view = GTK_TEXT_VIEW (text_view);
  gtk_text_view_set_left_margin (goban_window->text_view,
				 QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_right_margin (goban_window->text_view,
				  QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_wrap_mode (goban_window->text_view, GTK_WRAP_WORD);

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

#if ENABLE_NLS
  gtk_item_factory_set_translate_func (goban_window->item_factory,
				       (GtkTranslateFunc) gettext, NULL, NULL);
#endif

  gtk_item_factory_create_items (goban_window->item_factory,
				 (sizeof menu_entries
				  / sizeof (GtkItemFactoryEntry)),
				 menu_entries,
				 goban_window);
  menu_bar = gtk_item_factory_get_widget (goban_window->item_factory,
					  "<QuarryGobanWindowMenu>");

  gtk_window_add_accel_group (GTK_WINDOW (goban_window), accel_group);

  /* Tools menu, used with the editing toolbar below. */
  goban_window->tools_item_factory
    = gtk_item_factory_new (GTK_TYPE_MENU, "<QuarryToolsMenu>", NULL);

#if ENABLE_NLS
  gtk_item_factory_set_translate_func (goban_window->tools_item_factory,
				       (GtkTranslateFunc) gettext, NULL, NULL);
#endif

  gtk_item_factory_create_items (goban_window->tools_item_factory,
				 (sizeof tools_menu_entries
				  / sizeof (GtkItemFactoryEntry)),
				 tools_menu_entries, NULL);

  /* Main toolbar and a handle box for it. */
  main_toolbar = gtk_toolbar_new ();
  goban_window->main_toolbar = GTK_TOOLBAR (main_toolbar);
  gtk_preferences_register_main_toolbar (goban_window->main_toolbar);

  /* Else the toolbar collapses to the arrow alone on GTK+ 2.4+. */
  gtk_toolbar_set_show_arrow (goban_window->main_toolbar, FALSE);

  gtk_utils_append_toolbar_button (goban_window->main_toolbar,
				   &toolbar_open, GTK_UTILS_IS_IMPORTANT,
				   goban_window);
  gtk_utils_append_toolbar_button (goban_window->main_toolbar,
				   &toolbar_save, 0, goban_window);
  gtk_utils_append_toolbar_space (goban_window->main_toolbar);

  gtk_utils_append_toolbar_button (goban_window->main_toolbar,
				   &toolbar_find, 0, goban_window);
  gtk_utils_append_toolbar_space (goban_window->main_toolbar);

  gtk_utils_append_toolbar_button (goban_window->main_toolbar,
				   &toolbar_game_information, 0, goban_window);

  main_handle_box = gtk_handle_box_new ();
  gtk_container_add (GTK_CONTAINER (main_handle_box), main_toolbar);

  /* Editing toolbar and a handle box for it. */
  editing_toolbar = gtk_toolbar_new ();
  goban_window->editing_toolbar = GTK_TOOLBAR (editing_toolbar);
  gtk_preferences_register_editing_toolbar (goban_window->editing_toolbar);

  /* Else the toolbar collapses to the arrow alone on GTK+ 2.4+. */
  gtk_toolbar_set_show_arrow (goban_window->editing_toolbar, FALSE);

  gtk_utils_append_toolbar_button (goban_window->editing_toolbar,
				   &editing_toolbar_undo,
				   GTK_UTILS_IS_IMPORTANT,
				   goban_window);
  gtk_utils_append_toolbar_button (goban_window->editing_toolbar,
				   &editing_toolbar_redo, 0,
				   goban_window);
  gtk_utils_append_toolbar_space (goban_window->editing_toolbar);

  gtk_utils_append_toolbar_button (goban_window->editing_toolbar,
				   &editing_toolbar_delete,
				   GTK_UTILS_IS_IMPORTANT,
				   goban_window);
  gtk_utils_append_toolbar_space (goban_window->editing_toolbar);

  tools_option_menu = gtk_option_menu_new ();
  goban_window->tools_option_menu = GTK_OPTION_MENU (tools_option_menu);

  gtk_option_menu_set_menu (goban_window->tools_option_menu,
			    (gtk_item_factory_get_widget
			     (goban_window->tools_item_factory,
			      "<QuarryToolsMenu>")));

  g_signal_connect (tools_option_menu, "changed",
		    G_CALLBACK (tools_option_menu_changed), goban_window);

#if GTK_2_4_OR_LATER

  {
    GtkToolItem *tool_item = gtk_tool_item_new ();

    gtk_container_add (GTK_CONTAINER (tool_item),
		       gtk_utils_align_widget (tools_option_menu, 0.5, 0.5));
    gtk_toolbar_insert (goban_window->editing_toolbar, tool_item, -1);
  }

#else /* not GTK_2_4_OR_LATER */

  gtk_toolbar_append_widget (goban_window->editing_toolbar,
			     tools_option_menu, NULL, NULL);

#endif /* not GTK_2_4_OR_LATER */

  editing_handle_box = gtk_handle_box_new ();
  gtk_container_add (GTK_CONTAINER (editing_handle_box), editing_toolbar);

  /* Navigation toolbar and a handle box for it. */
  navigation_toolbar = gtk_toolbar_new ();
  goban_window->navigation_toolbar = GTK_TOOLBAR (navigation_toolbar);
  gtk_preferences_register_navigation_toolbar
    (goban_window->navigation_toolbar);

  gtk_toolbar_set_show_arrow (goban_window->navigation_toolbar, TRUE);

  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_root,
				   GTK_UTILS_IS_IMPORTANT, goban_window);
  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_back, 0,
				   goban_window);
  gtk_utils_append_toolbar_space (goban_window->navigation_toolbar);

  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_forward,
				   GTK_UTILS_IS_IMPORTANT, goban_window);
  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_variation_end, 0,
				   goban_window);
  gtk_utils_append_toolbar_space (goban_window->navigation_toolbar);

  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_previous_variation, 0,
				   goban_window);
  gtk_utils_append_toolbar_button (goban_window->navigation_toolbar,
				   &navigation_toolbar_next_variation, 0,
				   goban_window);

  navigation_handle_box = gtk_handle_box_new ();
  gtk_container_add (GTK_CONTAINER (navigation_handle_box),
		     navigation_toolbar);

  /* Horizontal box with the toolbars. */
  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, 0,
				main_handle_box, GTK_UTILS_FILL,
				editing_handle_box, GTK_UTILS_FILL,
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

  /* Starting with GTK+ 2.4 toolbars can grab focus, so we need to
   * explicitly transfer it to `goban'.
   */
  gtk_widget_grab_focus (goban);

  /* Show everything but the window itself.  Show toolbars' handle
   * boxes and game action buttons' box dependent on configuration.
   */
  gtk_widget_show_all (vbox);

  gtk_widget_hide (main_handle_box);
  if (gtk_ui_configuration.show_main_toolbar)
    show_or_hide_main_toolbar (goban_window);

  gtk_widget_hide (editing_handle_box);
  if (gtk_ui_configuration.show_editing_toolbar)
    show_or_hide_editing_toolbar (goban_window);

  gtk_widget_hide (navigation_handle_box);
  if (gtk_ui_configuration.show_navigation_toolbar)
    show_or_hide_navigation_toolbar (goban_window);

  gtk_widget_hide (game_action_buttons_hbox);
  if (gtk_ui_configuration.show_game_action_buttons)
    show_or_hide_game_action_buttons (goban_window);

  if (game_tree_view.show_game_tree == SHOW_GAME_TREE_ALWAYS)
    show_or_hide_sgf_tree_view (goban_window, GTK_GOBAN_WINDOW_SHOW_CHILD);
  else {
    gtk_utils_set_menu_items_sensitive (goban_window->item_factory, FALSE,
					"/View/Recenter on Current Node",
					NULL);
  }

  /* Look up here when the classes are certainly loaded. */
  clicked_signal_id	   = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
  pointer_moved_signal_id  = g_signal_lookup ("pointer-moved", GTK_TYPE_GOBAN);
  click_canceled_signal_id = g_signal_lookup ("click-canceled",
					      GTK_TYPE_GOBAN);
  goban_clicked_signal_id  = g_signal_lookup ("goban-clicked", GTK_TYPE_GOBAN);

  /* set_goban_signal_handlers() uses `click_canceled_signal_id', so
   * we call it only now.
   */
  set_goban_signal_handlers (goban_window,
			     G_CALLBACK (playing_mode_pointer_moved),
			     G_CALLBACK (playing_mode_goban_clicked));
  g_signal_connect_swapped (goban, "click-canceled",
			    G_CALLBACK (delete_drawn_position_list),
			    goban_window);
  g_signal_connect_swapped (goban, "navigate",
			    G_CALLBACK (navigate_goban), goban_window);

  /* But hide special mode section again. */
  leave_special_mode (goban_window);

  goban_window->board			     = NULL;

  goban_window->in_game_mode		     = FALSE;
  goban_window->pending_free_handicap	     = 0;
  goban_window->players[BLACK_INDEX]	     = NULL;
  goban_window->players[WHITE_INDEX]	     = NULL;
  goban_window->time_controls[BLACK_INDEX]   = NULL;
  goban_window->time_controls[WHITE_INDEX]   = NULL;

  goban_window->drawn_position_list	     = NULL;

  goban_window->updating_set_player_commands = FALSE;

  goban_window->next_sgf_label		     = NULL;
  goban_window->labels_mode
    = GTK_GOBAN_WINDOW_NON_LABELS_MODE;

  goban_window->time_of_first_modification   = 0;

  goban_window->filename		     = NULL;
  goban_window->save_as_dialog		     = NULL;

  goban_window->last_displayed_node	     = NULL;
  goban_window->last_game_info_node	     = NULL;

  goban_window->find_dialog		     = NULL;
  goban_window->text_to_find		     = NULL;

  goban_window->game_info_dialog	     = NULL;
}


/* NOTE: `filename' must be in file system encoding and absolute path,
 * if non-NULL!
 */
GtkWidget *
gtk_goban_window_new (SgfCollection *sgf_collection, const char *filename)
{
  GtkWidget *widget = GTK_WIDGET (g_object_new (GTK_TYPE_GOBAN_WINDOW, NULL));
  GtkGobanWindow *goban_window = GTK_GOBAN_WINDOW (widget);

  g_return_val_if_fail (sgf_collection, NULL);
  g_return_val_if_fail (!filename || g_path_is_absolute (filename), NULL);

  goban_window->board = NULL;

  goban_window->dead_stones_list = NULL;

  goban_window->sgf_collection = sgf_collection;

  if (filename) {
    goban_window->filename = g_strdup (filename);

    gtk_utils_set_menu_items_sensitive (goban_window->item_factory, FALSE,
					"/File/Save", NULL);
    gtk_utils_set_toolbar_buttons_sensitive (goban_window->main_toolbar, FALSE,
					     &toolbar_save, NULL);
  }

  sgf_collection_set_notification_callback
    (sgf_collection, collection_modification_state_changed, goban_window);

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


gboolean
gtk_goban_window_stops_closing (GtkGobanWindow *goban_window)
{
  g_return_val_if_fail (GTK_IS_GOBAN_WINDOW (goban_window), FALSE);

  if (sgf_collection_is_modified (goban_window->sgf_collection)) {
    GtkWidget *question_dialog;
    gint response_id;

    if (goban_window->filename) {
      gchar *filename_in_utf8 = g_filename_to_utf8 (goban_window->filename, -1,
						    NULL, NULL, NULL);

      question_dialog
	= (quarry_save_confirmation_dialog_new
	   (GTK_WINDOW (goban_window),
	    goban_window->time_of_first_modification,
	    _("Save changes to game record `%s' before closing?"),
	    filename_in_utf8));
      g_free (filename_in_utf8);
    }
    else {
      question_dialog
	= (quarry_save_confirmation_dialog_new
	   (GTK_WINDOW (goban_window),
	    goban_window->time_of_first_modification,
	    _("Save changes to the untitled game record before closing?")));
    }

    response_id = gtk_dialog_run (GTK_DIALOG (question_dialog));
    gtk_widget_destroy (question_dialog);

    switch (response_id) {
    case GTK_RESPONSE_NO:
      /* So do nothing. */
      break;

    case GTK_RESPONSE_YES:
      gtk_goban_window_save (goban_window, GTK_GOBAN_WINDOW_SAVE);

      if (!goban_window->save_as_dialog)
	break;

      response_id = gtk_dialog_run (GTK_DIALOG (goban_window->save_as_dialog));
      gtk_widget_destroy (goban_window->save_as_dialog);

      if (response_id == GTK_RESPONSE_OK)
	break;

    default:
    case GTK_RESPONSE_CANCEL:
      /* Don't allow closing then. */
      return TRUE;
    }
  }

  return FALSE;
}


static void
gtk_goban_window_finalize (GObject *object)
{
  GtkGobanWindow *goban_window = GTK_GOBAN_WINDOW (object);

  g_object_unref (goban_window->item_factory);
  g_object_unref (goban_window->tools_item_factory);

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
    gtk_widget_destroy (goban_window->save_as_dialog);

  g_free (goban_window->text_to_find);
  if (goban_window->find_dialog)
    gtk_widget_destroy (GTK_WIDGET (goban_window->find_dialog));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
force_minimal_width (GtkWidget *widget, GtkRequisition *requisition)
{
  /* TRANSLATORS: Width of this string in default font is used to
     determine the minimal width of the right board window side. */
  static const gchar *string = N_("A good width for the right side to have.");

  PangoLayout *layout = gtk_widget_create_pango_layout (widget, _(string));
  gint width;

  pango_layout_get_pixel_size (layout, &width, NULL);
  if (width > requisition->width)
    requisition->width = width;

  g_object_unref (layout);
}


void
gtk_goban_window_enter_game_record_mode (GtkGobanWindow *goban_window)
{
  g_return_if_fail (GTK_IS_GOBAN_WINDOW (goban_window));

  if (!goban_window->current_tree->undo_history) {
    goban_window->current_tree->undo_history
      = sgf_undo_history_new (goban_window->current_tree);

    sgf_undo_history_set_notification_callback
      (goban_window->current_tree->undo_history,
       undo_or_redo_availability_changed, goban_window);
  }
}


void
gtk_goban_window_enter_game_mode (GtkGobanWindow *goban_window,
				  GtpClient *black_player,
				  GtpClient *white_player,
				  TimeControl *time_control)
{
  const SgfGameTree *game_tree;
  int handicap = -1;

  g_return_if_fail (GTK_IS_GOBAN_WINDOW (goban_window));
  g_return_if_fail (!goban_window->in_game_mode);

  goban_window->in_game_mode		   = TRUE;
  goban_window->players[BLACK_INDEX]	   = black_player;
  goban_window->players[WHITE_INDEX]	   = white_player;

  set_time_controls (goban_window, time_control);

  goban_window->game_position.board_state = &goban_window->sgf_board_state;

  game_tree = goban_window->current_tree;
  handicap = sgf_node_get_handicap (game_tree->current_node);

  if (handicap > 0
      && !sgf_node_get_list_of_point_property_value (game_tree->current_node,
						     SGF_ADD_BLACK)) {
    goban_window->pending_free_handicap = handicap;
    goban_window->num_handicap_stones_placed = 0;

    if (!black_player) {
      /* TRANSLATORS: It can never be 1 stone, always at least 2. */
      gchar *hint = g_strdup_printf (ngettext ("Please set up %d (or less)\n"
					       "stone of free handicap",
					       "Please set up %d (or less)\n"
					       "stones of free handicap",
					       handicap),
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

  do_enter_game_mode (goban_window);
}


/* FIXME: Get remaining time from the SGF tree and adjust time
 *	  controls accordingly.
 */
void
gtk_goban_window_resume_game (GtkGobanWindow *goban_window,
			      GtpClient *black_player, GtpClient *white_player)
{
  SgfBoardState *const board_state = &goban_window->sgf_board_state;

  g_return_if_fail (GTK_IS_GOBAN_WINDOW (goban_window));
  g_return_if_fail (!goban_window->in_game_mode);

  goban_window->in_game_mode		   = TRUE;
  goban_window->players[BLACK_INDEX]	   = black_player;
  goban_window->players[WHITE_INDEX]	   = white_player;

  goban_window->game_position.board_state = board_state;

  /* Resuming, skip'em played moves! */
  sgf_utils_go_down_in_tree (goban_window->current_tree, -1);

  if (goban_window->time_controls[BLACK_INDEX]) {
    time_control_set_state (goban_window->time_controls[BLACK_INDEX],
			    board_state->time_left[BLACK_INDEX],
			    board_state->moves_left[BLACK_INDEX]);
  }

  if (goban_window->time_controls[WHITE_INDEX]) {
    time_control_set_state (goban_window->time_controls[WHITE_INDEX],
			    board_state->time_left[WHITE_INDEX],
			    board_state->moves_left[WHITE_INDEX]);
  }

  do_enter_game_mode (goban_window);
}


static void
do_enter_game_mode (GtkGobanWindow *goban_window)
{
  if (goban_window->players[BLACK_INDEX]) {
    goban_window->player_initialization_step[BLACK_INDEX]
      = INITIALIZATION_NOT_STARTED;
    initialize_gtp_player (goban_window->players[BLACK_INDEX], 1,
			   goban_window);
  }

  if (goban_window->players[WHITE_INDEX]) {
    goban_window->player_initialization_step[WHITE_INDEX]
      = INITIALIZATION_NOT_STARTED;
    initialize_gtp_player (goban_window->players[WHITE_INDEX], 1,
			   goban_window);
  }

  if (goban_window->time_controls[BLACK_INDEX]
      && goban_window->time_controls[WHITE_INDEX]) {
    gtk_clock_use_time_control (goban_window->clocks[BLACK_INDEX],
				goban_window->time_controls[BLACK_INDEX],
				((GtkClockOutOfTimeCallback)
				 player_is_out_of_time),
				goban_window);
    gtk_clock_use_time_control (goban_window->clocks[WHITE_INDEX],
				goban_window->time_controls[WHITE_INDEX],
				((GtkClockOutOfTimeCallback)
				 player_is_out_of_time),
				goban_window);
  }
  else {
    g_assert (!goban_window->time_controls[BLACK_INDEX]
	      && !goban_window->time_controls[WHITE_INDEX]);
  }

  goban_window->last_displayed_node = NULL;
  update_children_for_new_node (goban_window, TRUE, FALSE);

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
  }

  if (goban_window->time_controls[WHITE_INDEX]) {
    gtk_clock_use_time_control (goban_window->clocks[WHITE_INDEX], NULL,
				NULL, NULL);
  }

  gtk_goban_window_enter_game_record_mode (goban_window);
  update_commands_sensitivity (goban_window);
}


static void
collection_modification_state_changed (SgfCollection *sgf_collection,
				       void *user_data)
{
  GtkGobanWindow *goban_window = GTK_GOBAN_WINDOW (user_data);
  int is_modified	    = sgf_collection_is_modified (sgf_collection);
  int is_modified_effective = (is_modified || !goban_window->filename);

  if (is_modified && goban_window->time_of_first_modification == 0) {
    GTimeVal current_time;

    g_get_current_time (&current_time);
    goban_window->time_of_first_modification = current_time.tv_sec;
  }

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      is_modified_effective,
				      "/File/Save", NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->main_toolbar,
					   is_modified_effective,
					   &toolbar_save, NULL);

  update_window_title (goban_window);
}


static void
undo_or_redo_availability_changed (SgfUndoHistory *undo_history,
				   void *user_data)
{
  GtkGobanWindow *goban_window = GTK_GOBAN_WINDOW (user_data);

  gboolean can_undo = sgf_utils_can_undo (goban_window->current_tree);
  gboolean can_redo = sgf_utils_can_redo (goban_window->current_tree);

  UNUSED (undo_history);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory, can_undo,
				      "/Edit/Undo", NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->editing_toolbar,
					   can_undo,
					   &editing_toolbar_undo, NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory, can_redo,
				      "/Edit/Redo", NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->editing_toolbar,
					   can_redo,
					   &editing_toolbar_redo, NULL);
}


static void
gtk_goban_window_save (GtkGobanWindow *goban_window, guint callback_action)
{
  if (callback_action != GTK_GOBAN_WINDOW_SAVE_AS && goban_window->filename) {
    if (do_save_collection (goban_window, goban_window->filename)
	&& callback_action == GTK_GOBAN_WINDOW_ADJOURN)
      game_has_been_adjourned (goban_window);
  }
  else {
    /* "Save as..." command invoked or we don't have a filename. */
    gboolean adjourning_game = (callback_action == GTK_GOBAN_WINDOW_ADJOURN);

    if (!goban_window->save_as_dialog
	|| goban_window->adjourning_game != adjourning_game) {
      if (goban_window->save_as_dialog)
	gtk_widget_destroy (goban_window->save_as_dialog);

      goban_window->save_as_dialog
	= gtk_file_dialog_new ((adjourning_game
				? _("Adjourn & Save As...") : _("Save As...")),
			       GTK_WINDOW (goban_window),
			       FALSE, GTK_STOCK_SAVE);

      g_signal_connect (goban_window->save_as_dialog, "response",
			G_CALLBACK (save_file_as_response), goban_window);

      if (goban_window->filename) {
	gtk_file_dialog_set_filename (goban_window->save_as_dialog,
				      goban_window->filename);
      }
      else {
	char *filename = suggest_filename (goban_window);

	gtk_file_dialog_set_current_name (goban_window->save_as_dialog,
					  filename);
	utils_free (filename);
      }

      goban_window->adjourning_game = adjourning_game;
      gtk_control_center_window_created
	(GTK_WINDOW (goban_window->save_as_dialog));
      gtk_utils_null_pointer_on_destroy (((GtkWindow **)
					  &goban_window->save_as_dialog),
					 TRUE);
    }

    gtk_window_present (GTK_WINDOW (goban_window->save_as_dialog));
  }
}


/* Returns a suggested filename for the current game record in UTF-8
 * encoding.  The return value must be utils_free'd.
 */
static char *
suggest_filename (GtkGobanWindow *goban_window)
{
  const SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;
  char *suggested_filename = NULL;

  if (goban_window->filename)
    return utils_duplicate_string (goban_window->filename);

  if (game_info_node) {
    const char *white_player
      = sgf_node_get_text_property_value (game_info_node, SGF_PLAYER_WHITE);
    const char *black_player
      = sgf_node_get_text_property_value (game_info_node, SGF_PLAYER_BLACK);

    if (white_player && black_player) {
      suggested_filename = utils_cat_strings (NULL,
					      white_player,
					      " vs ", black_player, ".sgf",
					      NULL);
    }
    else {
      const char *game_name = sgf_node_get_text_property_value (game_info_node,
								SGF_GAME_NAME);

      if (game_name)
	suggested_filename = utils_cat_strings (NULL, game_name, ".sgf", NULL);
    }

    if (suggested_filename) {
      char *scan;

      /* For Windows spaces in names are standard anyway. */
#ifndef G_OS_WIN32

      for (scan = suggested_filename; *scan; scan++) {
	/* It's difficult to provide sane alternative for other
	 * characters shells have to escape in filenames, so we keep
	 * those characters as they are.
	 */
	if (*scan == ' ')
	  *scan = '-';
      }

#endif

      return suggested_filename;
    }
  }

  return NULL;
}


static void
save_file_as_response (GtkWidget *file_dialog, gint response_id,
		       GtkGobanWindow *goban_window)
{
  if (response_id == GTK_RESPONSE_OK) {
    gchar *filename = gtk_file_dialog_get_filename (file_dialog);

    if (do_save_collection (goban_window, filename)) {
      g_free (goban_window->filename);
      goban_window->filename = filename;

      if (goban_window->adjourning_game) {
	game_has_been_adjourned (goban_window);
	return;
      }
    }
    else
      g_free (filename);
  }

  if (!gtk_window_get_modal (GTK_WINDOW (file_dialog))
      && (response_id == GTK_RESPONSE_OK
	  || response_id == GTK_RESPONSE_CANCEL))
    gtk_widget_destroy (file_dialog);
}


static gboolean
do_save_collection (GtkGobanWindow *goban_window, const gchar *filename)
{
  int result;
  SgfGameTree *sgf_tree = goban_window->current_tree;
  SgfUndoHistory *undo_history = sgf_tree->undo_history;

  /* Fetch comment and node name, but ensure that SGF properties are
   * changed only for the call to sgf_write_file(), by undo history
   * swapping.
   */

  sgf_tree->undo_history = sgf_undo_history_new (sgf_tree);
  fetch_comment_and_node_name (goban_window, TRUE);

  result = sgf_write_file (filename, goban_window->sgf_collection,
			   sgf_configuration.force_utf8);

  if (sgf_utils_can_undo (sgf_tree))
    sgf_utils_undo (sgf_tree);

  sgf_undo_history_delete (sgf_tree->undo_history, sgf_tree);
  sgf_tree->undo_history = undo_history;

  if (result) {
    sgf_collection_set_unmodified (goban_window->sgf_collection);
    goban_window->time_of_first_modification = 0;
  }

  return result;
}


static void
game_has_been_adjourned (GtkGobanWindow *goban_window)
{
  static const gchar *hint
    = N_("You can later resume the game by pressing the `Resume Game' button "
	 "in Quarry Control Center, or selecting `Resume Game' item from "
	 "the `File' menu.");

  gchar *filename_in_utf8 = g_filename_to_utf8 (goban_window->filename, -1,
						NULL, NULL, NULL);
  gchar *message = g_strdup_printf (_("The game is adjourned and "
				      "saved in file `%s'"),
				    filename_in_utf8);
  GtkWidget *information_dialog
    = quarry_message_dialog_new (NULL, GTK_BUTTONS_OK, GTK_STOCK_DIALOG_INFO,
				 _(hint), message);

  gtk_utils_show_and_forget_dialog (GTK_DIALOG (information_dialog));

  g_free (filename_in_utf8);

  gtk_widget_destroy (GTK_WIDGET (goban_window));
}


static void
export_ascii_diagram (GtkGobanWindow *goban_window)
{
  static const gchar *message
    = N_("ASCII diagram has been exported to clipboard");

  do_export_diagram
    (goban_window,
     sgf_utils_export_position_as_ascii (goban_window->current_tree),
     _(message));
}


static void
export_senseis_library_diagram (GtkGobanWindow *goban_window)
{
  static const gchar *message
    = N_("Sensei's Library diagram has been exported to clipboard");

  do_export_diagram
    (goban_window,
     (sgf_utils_export_position_as_senseis_library_diagram
      (goban_window->current_tree)),
     _(message));
}


static void
do_export_diagram (GtkGobanWindow *goban_window, char *diagram_string,
		   const gchar *message)
{
  static const gchar *hint
    = N_("You can usually paste the diagram in another application using "
	 "`Ctrl+C' key combination or by selecting appropriate menu item.");

  int diagram_string_length = strlen (diagram_string);
  GtkWidget *message_dialog;

  gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
			  diagram_string, diagram_string_length);
  gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
			  diagram_string, diagram_string_length);
  utils_free (diagram_string);

  message_dialog = quarry_message_dialog_new (GTK_WINDOW (goban_window),
					      GTK_BUTTONS_OK,
					      GTK_STOCK_DIALOG_INFO,
					      _(hint), message);
  gtk_utils_show_and_forget_dialog (GTK_DIALOG (message_dialog));
}


static void
gtk_goban_window_close (GtkGobanWindow *goban_window)
{
  if (!gtk_goban_window_stops_closing (goban_window))
    gtk_widget_destroy (GTK_WIDGET (goban_window));
}




static void
show_find_dialog (GtkGobanWindow *goban_window)
{
  if (!goban_window->find_dialog) {
    goban_window->find_dialog = QUARRY_FIND_DIALOG (quarry_find_dialog_new
						    (_("Find")));
    gtk_utils_null_pointer_on_destroy (((GtkWindow **)
					&goban_window->find_dialog),
				       FALSE);

    gtk_window_set_transient_for (GTK_WINDOW (goban_window->find_dialog),
				  GTK_WINDOW (goban_window));
    gtk_window_set_destroy_with_parent (GTK_WINDOW (goban_window->find_dialog),
					TRUE);

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
      goban_window->search_in = find_dialog_configuration.search_in;
      goban_window->close_automatically
	= find_dialog_configuration.close_automatically;
    }
    else {
      quarry_find_dialog_set_text_to_find (goban_window->find_dialog,
					   goban_window->text_to_find);
    }

    quarry_find_dialog_set_search_history
      (goban_window->find_dialog,
       &find_dialog_configuration.latest_search_strings);

    quarry_find_dialog_set_case_sensitive (goban_window->find_dialog,
					   goban_window->case_sensitive);
    quarry_find_dialog_set_whole_words_only (goban_window->find_dialog,
					     goban_window->whole_words_only);
    quarry_find_dialog_set_wrap_around (goban_window->find_dialog,
					goban_window->wrap_around);
    quarry_find_dialog_set_search_whole_game_tree
      (goban_window->find_dialog, goban_window->search_whole_game_tree);
    quarry_find_dialog_set_search_in (goban_window->find_dialog,
				      goban_window->search_in);
    quarry_find_dialog_set_close_automatically
      (goban_window->find_dialog, goban_window->close_automatically);

    g_signal_connect (goban_window->find_dialog, "response",
		      G_CALLBACK (find_dialog_response), goban_window);
  }

  gtk_window_present (GTK_WINDOW (goban_window->find_dialog));
}


static void
find_dialog_response (QuarryFindDialog *find_dialog, gint response_id,
		      GtkGobanWindow *goban_window)
{
  if (response_id    == QUARRY_FIND_DIALOG_FIND_NEXT
      || response_id == QUARRY_FIND_DIALOG_FIND_PREVIOUS) {
    g_free (goban_window->text_to_find);
    goban_window->text_to_find = g_strdup (quarry_find_dialog_get_text_to_find
					   (find_dialog));

    goban_window->case_sensitive
      = quarry_find_dialog_get_case_sensitive (find_dialog);
    find_dialog_configuration.case_sensitive = goban_window->case_sensitive;

    goban_window->whole_words_only
      = quarry_find_dialog_get_whole_words_only (find_dialog);
    find_dialog_configuration.whole_words_only
      = goban_window->whole_words_only;

    goban_window->wrap_around
      = quarry_find_dialog_get_wrap_around (find_dialog);
    find_dialog_configuration.wrap_around = goban_window->wrap_around;

    goban_window->search_whole_game_tree
      = quarry_find_dialog_get_search_whole_game_tree (find_dialog);
    find_dialog_configuration.search_whole_game_tree
      = goban_window->search_whole_game_tree;

    goban_window->search_in
      = quarry_find_dialog_get_search_in (find_dialog);
    find_dialog_configuration.search_in = goban_window->search_in;

    goban_window->close_automatically
      = quarry_find_dialog_get_close_automatically (find_dialog);
    find_dialog_configuration.close_automatically
      = goban_window->close_automatically;

    if (!do_find_text (goban_window, response_id)
	|| !find_dialog_configuration.close_automatically) {
      /* Don't close the dialog. */
      return;
    }
  }

  gtk_widget_destroy (GTK_WIDGET (find_dialog));
}


static gboolean
do_find_text (GtkGobanWindow *goban_window,
	      QuarryFindDialogSearchDirection direction)
{
  if (!goban_window->text_to_find) {
    /* Don't have text to find yet. */
    show_find_dialog (goban_window);
    return FALSE;
  }

  if (quarry_find_text (goban_window->text_to_find, direction,
			goban_window->case_sensitive,
			goban_window->whole_words_only,
			goban_window->wrap_around,
			goban_window->search_whole_game_tree,
			goban_window->search_in,
			goban_window->text_buffer,
			goban_window->node_name_inserted,
			goban_window->current_tree,
			((QuarryFindDialogSwitchToGivenNode)
			 switch_to_given_node),
			goban_window)) {
    gtk_text_view_scroll_to_mark
      (goban_window->text_view,
       gtk_text_buffer_get_insert (goban_window->text_buffer),
       0.1, FALSE, 0.0, 0.0);

    StringListItem *item
      = string_list_find (&find_dialog_configuration.latest_search_strings,
			  goban_window->text_to_find);

    if (item) {
      string_list_delete_item
	(&find_dialog_configuration.latest_search_strings, item);
    }
    else {
      /* Ten (maximal history size) minus one.  Account for the item
       * to be added!
       */
      string_list_clamp_size (&find_dialog_configuration.latest_search_strings,
			      9);
    }

    string_list_prepend (&find_dialog_configuration.latest_search_strings,
			 utils_duplicate_string (goban_window->text_to_find));

    if (goban_window->find_dialog) {
      quarry_find_dialog_set_search_history
	(goban_window->find_dialog,
	 &find_dialog_configuration.latest_search_strings);
    }

    return TRUE;
  }
  else {
    if (goban_window->find_dialog) {
      if (goban_window->wrap_around
	  || direction == QUARRY_FIND_DIALOG_FIND_NEXT) {
	gtk_dialog_set_response_sensitive
	  (GTK_DIALOG (goban_window->find_dialog),
	   QUARRY_FIND_DIALOG_FIND_NEXT, FALSE);
      }

      if (goban_window->wrap_around
	  || direction == QUARRY_FIND_DIALOG_FIND_PREVIOUS) {
	gtk_dialog_set_response_sensitive
	  (GTK_DIALOG (goban_window->find_dialog),
	   QUARRY_FIND_DIALOG_FIND_PREVIOUS, FALSE);
      }
    }

    return FALSE;
  }
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

    gtk_window_set_transient_for (GTK_WINDOW (goban_window->game_info_dialog),
				  GTK_WINDOW (goban_window));

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
  switch (sgf_property_type) {
  case SGF_GAME_NAME:
    update_window_title (goban_window);
    break;

  case SGF_PLAYER_BLACK:
  case SGF_BLACK_RANK:
  case SGF_BLACK_TEAM:
    update_player_information (goban_window, BLACK);
    break;

  case SGF_PLAYER_WHITE:
  case SGF_WHITE_RANK:
  case SGF_WHITE_TEAM:
    update_player_information (goban_window, WHITE);
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

    break;

  default:
    /* Silence warnings. */
    break;
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
				   "/View/Main Toolbar");

  gtk_ui_configuration.show_main_toolbar
    = !GTK_WIDGET_VISIBLE (toolbar_handle_box);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
				  gtk_ui_configuration.show_main_toolbar);
  gtk_utils_set_widgets_visible (gtk_ui_configuration.show_main_toolbar,
				 toolbar_handle_box, NULL);
}


static void
show_or_hide_editing_toolbar (GtkGobanWindow *goban_window)
{
  GtkWidget *toolbar_handle_box
    = gtk_widget_get_parent (GTK_WIDGET (goban_window->editing_toolbar));
  GtkWidget *menu_item
    = gtk_item_factory_get_widget (goban_window->item_factory,
				   "/View/Editing Toolbar");

  gtk_ui_configuration.show_editing_toolbar
    = !GTK_WIDGET_VISIBLE (toolbar_handle_box);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
				  gtk_ui_configuration.show_editing_toolbar);
  gtk_utils_set_widgets_visible (gtk_ui_configuration.show_editing_toolbar,
				 toolbar_handle_box, NULL);
}


static void
show_or_hide_navigation_toolbar (GtkGobanWindow *goban_window)
{
  GtkWidget *toolbar_handle_box
    = gtk_widget_get_parent (GTK_WIDGET (goban_window->navigation_toolbar));
  GtkWidget *menu_item
    = gtk_item_factory_get_widget (goban_window->item_factory,
				   "/View/Navigation Toolbar");

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
				   "/View/Game Action Buttons");

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
				     "/View/Game Tree");

    g_signal_handlers_block_by_func (menu_item, show_or_hide_sgf_tree_view,
				     goban_window);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
				    show_sgf_tree_view);
    g_signal_handlers_unblock_by_func (menu_item, show_or_hide_sgf_tree_view,
				       goban_window);

    gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
					show_sgf_tree_view,
					"/View/Recenter on Current Node",
					NULL);

    if (show_sgf_tree_view) {
      gtk_paned_pack2 (goban_window->vpaned, sgf_tree_view_parent,
		       TRUE, FALSE);

      /* Force reallocation of `sgf_tree_view' now: recentering
       * algorithm needs to know the widget size to find the center.
       */
      gtk_container_check_resize (GTK_CONTAINER (goban_window->vpaned));

      gtk_sgf_tree_view_center_on_current_node (goban_window->sgf_tree_view);
    }
    else {
      gtk_container_remove (GTK_CONTAINER (goban_window->vpaned),
			    sgf_tree_view_parent);
    }
  }
}


static void
recenter_sgf_tree_view (GtkGobanWindow *goban_window)
{
  gtk_sgf_tree_view_center_on_current_node (goban_window->sgf_tree_view);
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


#ifdef GTK_TYPE_GO_TO_NAMED_NODE_DIALOG


static void
show_go_to_named_node_dialog (GtkGobanWindow *goban_window)
{
  GtkWidget *dialog
    = gtk_go_to_named_node_dialog_new (goban_window->current_tree);

  if (dialog == NULL) {
    dialog = quarry_message_dialog_new (NULL, GTK_BUTTONS_OK,
					GTK_STOCK_DIALOG_INFO,
					NULL,
					_("Sorry, there are no named "
					  "nodes in this game tree"));
  }

  gtk_window_set_transient_for (GTK_WINDOW (dialog),
				GTK_WINDOW (goban_window));

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK
      && GTK_IS_GO_TO_NAMED_NODE_DIALOG (dialog)) {
    switch_to_given_node (goban_window,
			  GTK_GO_TO_NAMED_NODE_DIALOG (dialog)->selected_node);
  }

  gtk_widget_destroy (dialog);
}


#endif /* GTK_TYPE_GO_TO_NAMED_NODE_DIALOG */


static void
show_about_dialog (void)
{
  static const char *description_string
    = N_("A GUI program for Go, Amazons and Reversi board games");
  static const char *copyright_string
    = N_("Copyright (C) 2003, 2004, 2005, 2006 Paul Pogonyshev and others");

  if (!about_dialog) {
#if GTK_2_6_OR_LATER

    GtkAboutDialog *dialog = GTK_ABOUT_DIALOG (gtk_about_dialog_new ());

    gtk_about_dialog_set_name (dialog, PACKAGE_NAME);
    gtk_about_dialog_set_version (dialog, PACKAGE_VERSION);
    gtk_about_dialog_set_copyright (dialog, _(copyright_string));
    gtk_about_dialog_set_comments (dialog, _(description_string));

    about_dialog = GTK_WINDOW (dialog);

#else /* not GTK_2_6_OR_LATER */

    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("About Quarry"),
						     NULL, 0,
						     GTK_STOCK_CLOSE,
						     GTK_RESPONSE_CANCEL,
						     NULL);
    GtkWidget *quarry_label;
    GtkWidget *description_label;
    GtkWidget *copyright_label;
    GtkWidget *vbox;
    gchar *copyright_markup = g_strconcat ("<small>", _(copyright_string),
					   "</small>", NULL);

    about_dialog = GTK_WINDOW (dialog);
    gtk_utils_null_pointer_on_destroy (&about_dialog, FALSE);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

    g_signal_connect (dialog, "response",
		      G_CALLBACK (gtk_widget_destroy), NULL);

    quarry_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (quarry_label),
			  ("<span size=\"larger\" weight=\"bold\">"
			   PACKAGE_STRING "</span>"));

    description_label = gtk_label_new (_(description_string));

    copyright_label = gtk_label_new (NULL);
    gtk_label_set_justify (GTK_LABEL (copyright_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_markup (GTK_LABEL (copyright_label), copyright_markup);
    g_free (copyright_markup);

    vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING,
				  quarry_label, GTK_UTILS_FILL,
				  description_label, GTK_UTILS_FILL,
				  copyright_label, GTK_UTILS_FILL, NULL);

    gtk_utils_standardize_dialog (GTK_DIALOG (dialog), vbox);
    gtk_widget_show_all (vbox);

#endif /* not GTK_2_6_OR_LATER */
  }

  gtk_window_present (about_dialog);
}


static void
show_help_contents (void)
{
  gtk_help_display (NULL);
}


static void
tools_option_menu_changed (GtkOptionMenu *option_menu,
			   const GtkGobanWindow *goban_window)
{
  gint tool_index = gtk_option_menu_get_history (option_menu);
  GtkMenuShell *tools_menu
    = GTK_MENU_SHELL (gtk_item_factory_get_widget (goban_window->item_factory,
						   "/Edit/Tools"));
  GtkCheckMenuItem *menu_item
    = GTK_CHECK_MENU_ITEM (g_list_nth_data (tools_menu->children, tool_index));

  gtk_check_menu_item_set_active (menu_item, TRUE);
}


static void
synchronize_tools_menus (const GtkGobanWindow *goban_window)
{
  GtkMenuShell *main_tools_menu
    = GTK_MENU_SHELL (gtk_item_factory_get_widget (goban_window->item_factory,
						   "/Edit/Tools"));
  GtkMenuShell *toolbar_tools_menu
    = GTK_MENU_SHELL (gtk_option_menu_get_menu
		      (goban_window->tools_option_menu));
  GList *main_menu_child;
  GList *toolbar_menu_child;
  int k;

  for (main_menu_child = main_tools_menu->children,
	 toolbar_menu_child = toolbar_tools_menu->children, k = 0;
       main_menu_child && toolbar_menu_child;
       main_menu_child = main_menu_child->next,
	 toolbar_menu_child = toolbar_menu_child->next, k++) {
    gtk_widget_set_sensitive (GTK_WIDGET (toolbar_menu_child->data),
			      GTK_WIDGET_SENSITIVE (main_menu_child->data));

    if (GTK_IS_CHECK_MENU_ITEM (main_menu_child->data)
	&& (gtk_check_menu_item_get_active
	    (GTK_CHECK_MENU_ITEM (main_menu_child->data))))
      gtk_option_menu_set_history (goban_window->tools_option_menu, k);
  }
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
  BoardPositionList *difference_lists[NUM_ON_GRID_VALUES];

  grid_diff (goban_window->board->grid, goban_window->goban->grid,
	     goban_window->board->width, goban_window->board->height,
	     difference_lists);
  g_assert ((difference_lists[BLACK]->num_positions
	     == goban_window->num_handicap_stones_placed)
	    && difference_lists[WHITE] == NULL
	    && difference_lists[EMPTY] == NULL
	    && difference_lists[SPECIAL_ON_GRID_VALUE] == NULL);

  leave_special_mode (goban_window);
  set_goban_signal_handlers (goban_window,
			     G_CALLBACK (playing_mode_pointer_moved),
			     G_CALLBACK (playing_mode_goban_clicked));

  free_handicap_has_been_placed (goban_window, difference_lists[BLACK]);
}


static void
go_scoring_mode_done (GtkGobanWindow *goban_window)
{
  handle_go_scoring_results (goban_window);

  if (goban_window->in_game_mode) {
    set_goban_signal_handlers (goban_window,
			       G_CALLBACK (playing_mode_pointer_moved),
			       G_CALLBACK (playing_mode_goban_clicked));
  }
  else {
    GtkWidget *move_tool_menu_item
      = gtk_item_factory_get_widget (goban_window->item_factory,
				     "/Edit/Tools/Move Tool");

    /* Activate move tool. */
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (move_tool_menu_item),
				    TRUE);
  }

  reenter_current_node (goban_window);
}


static void
go_scoring_mode_cancel (GtkGobanWindow *goban_window)
{
  GtkWidget *move_tool_menu_item
    = gtk_item_factory_get_widget (goban_window->item_factory,
				   "/Edit/Tools/Move Tool");

  /* Activate move tool. */
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (move_tool_menu_item),
				  TRUE);

  reenter_current_node (goban_window);
}


static void
handle_go_scoring_results (GtkGobanWindow *goban_window)
{
  SgfGameTree *current_tree = goban_window->current_tree;
  SgfNode *game_info_node   = current_tree->board_state->game_info_node;
  double komi = 0.0;
  double score;
  StringBuffer detailed_score;
  BoardPositionList *black_territory;
  BoardPositionList *white_territory;

  sgf_node_get_komi (game_info_node, &komi);

  string_buffer_init (&detailed_score, 0x400, 0x200);
  go_score_game (current_tree->board, goban_window->dead_stones, komi,
		 &score, &detailed_score, &black_territory, &white_territory);

  sgf_utils_begin_action (current_tree);

  fetch_comment_and_node_name (goban_window, TRUE);

  sgf_utils_append_text_property (current_tree->current_node, current_tree,
				  SGF_COMMENT,
				  string_buffer_steal_string (&detailed_score),
				  "\n\n----------------\n\n", 0);
  set_comment_and_node_name (goban_window, current_tree->current_node);

  sgf_utils_set_list_of_point_property (current_tree->current_node,
					current_tree,
					SGF_BLACK_TERRITORY, black_territory,
					0);
  sgf_utils_set_list_of_point_property (current_tree->current_node,
					current_tree,
					SGF_WHITE_TERRITORY, white_territory,
					0);

  sgf_utils_set_score_result (game_info_node, current_tree, score, 1);

  sgf_utils_end_action (current_tree);

  g_free (goban_window->dead_stones);
  goban_window->dead_stones = NULL;
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
   *
   * FIXME: Doesn't really work currently.  Not important yet, since
   *	    we don't support switching the current tree anyway.
   */
  update_comment_and_node_name_if_needed (goban_window, TRUE);

  gtk_sgf_tree_signal_proxy_attach (sgf_tree);

  goban_window->current_tree = sgf_tree;
  sgf_utils_enter_tree (sgf_tree, goban_window->board,
			&goban_window->sgf_board_state);

  gtk_goban_set_parameters (goban_window->goban, sgf_tree->game,
			    sgf_tree->board_width, sgf_tree->board_height);

  set_time_controls (goban_window,
		     time_control_new_from_sgf_node (sgf_tree->root));

  update_game_information (goban_window);
  update_children_for_new_node (goban_window, TRUE, FALSE);
  undo_or_redo_availability_changed (NULL, goban_window);

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
set_time_controls (GtkGobanWindow *goban_window, TimeControl *time_control)
{
  if (goban_window->time_controls[BLACK_INDEX])
    time_control_delete (goban_window->time_controls[BLACK_INDEX]);

  if (goban_window->time_controls[WHITE_INDEX])
    time_control_delete (goban_window->time_controls[WHITE_INDEX]);

  goban_window->time_controls[BLACK_INDEX] = time_control;
  goban_window->time_controls[WHITE_INDEX]
    = (time_control ? time_control_duplicate (time_control) : NULL);
}


static void
reenter_current_node (GtkGobanWindow *goban_window)
{
  SgfGameTree *current_tree = goban_window->current_tree;

  if (current_tree->current_node->parent) {
    sgf_utils_go_up_in_tree (current_tree, 1);
    sgf_utils_go_down_in_tree (current_tree, 1);
  }
  else {
    sgf_utils_enter_tree (current_tree, goban_window->board,
			  &goban_window->sgf_board_state);
  }

  /* `text_handled' is set to TRUE to avoid spoiling undo/redo history
   * and text buffer contents.
   */
  update_children_for_new_node (goban_window, TRUE, TRUE);
}


/* This function must be called before changing to ``arbitrary'' in
 * some sense node.  More exactly, when the node we are going to
 * switch to is not being added as a result of playing a move.
 */
static void
about_to_change_node (GtkGobanWindow *goban_window)
{
  g_signal_emit (goban_window->goban, click_canceled_signal_id, 0);

  if (goban_window->in_game_mode && IS_DISPLAYING_GAME_NODE (goban_window)) {
    /* Goban window is going to display something other than the game
     * position node.
     */
    sgf_game_tree_get_state (goban_window->current_tree,
			     &goban_window->game_position);
    goban_window->game_position.board
      = board_duplicate_without_stacks (goban_window->game_position.board);

    goban_window->game_position.board_state
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
    goban_window->game_position.board_state = &goban_window->sgf_board_state;
  }
}


static void
play_pass_move (GtkGobanWindow *goban_window)
{
  g_assert (goban_window->board->game == GAME_GO
	    && USER_CAN_PLAY_MOVES (goban_window));

  sgf_utils_append_variation (goban_window->current_tree,
			      goban_window->sgf_board_state.color_to_play,
			      PASS_X, PASS_Y);

  if (goban_window->in_game_mode && IS_DISPLAYING_GAME_NODE (goban_window))
    move_has_been_played (goban_window);

  update_children_for_new_node (goban_window, TRUE, FALSE);
}


static void
resign_game (GtkGobanWindow *goban_window)
{
  GtkWidget *question_dialog
    = quarry_message_dialog_new (GTK_WINDOW (goban_window),
				 GTK_BUTTONS_NONE, GTK_STOCK_DIALOG_QUESTION,
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

  sgf_utils_set_text_property (game_info_node, goban_window->current_tree,
			       SGF_RESULT,
			       utils_cprintf ("%c+Resign", other_color_char),
			       1);

  sgf_utils_append_variation (goban_window->current_tree, EMPTY);
  update_children_for_new_node (goban_window, TRUE, FALSE);
}


static void
cancel_amazons_move (GtkGobanWindow *goban_window)
{
  if (goban_window->amazons_move_stage == SHOOTING_ARROW) {
    gtk_goban_set_overlay_data (goban_window->goban, 1, NULL,
				TILE_NONE, TILE_NONE, SGF_MARKUP_NONE);
  }

  if (goban_window->amazons_move_stage != SELECTING_QUEEN) {
    gtk_goban_set_overlay_data (goban_window->goban, 0, NULL,
				TILE_NONE, TILE_NONE, SGF_MARKUP_NONE);
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
  /* Mode switching must cancel whatever is being done with mouse. */
  g_signal_emit (goban_window->goban, click_canceled_signal_id, 0);

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

  gtk_goban_force_feedback_poll (goban_window->goban);
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
    if (data->non_empty_feedback && !(data->modifiers & GDK_SHIFT_MASK)) {
      int color_to_play = goban_window->sgf_board_state.color_to_play;

      if (color_to_play == EMPTY)
	return;

      if (goban_window->board->game != GAME_AMAZONS) {
	sgf_utils_append_variation (goban_window->current_tree,
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
				      GOBAN_TILE_DONT_CHANGE,
				      GOBAN_SGF_MARKUP_TILE_DONT_CHANGE);

	  return;
	}
	else if (goban_window->amazons_move_stage == MOVING_QUEEN) {
	  goban_window->amazons_move_stage  = SHOOTING_ARROW;

	  gtk_goban_set_overlay_data (goban_window->goban, 1,
				      board_position_list_new (&pos, 1),
				      (STONE_25_TRANSPARENT
				       + COLOR_INDEX (color_to_play)),
				      GOBAN_TILE_DONT_CHANGE,
				      GOBAN_SGF_MARKUP_TILE_DONT_CHANGE);

	  return;
	}
	else {
	  sgf_utils_append_variation (goban_window->current_tree,
				      color_to_play,
				      goban_window->amazons_to_x,
				      goban_window->amazons_to_y,
				      goban_window->amazons_move);
	}
      }

      if (goban_window->in_game_mode && IS_DISPLAYING_GAME_NODE (goban_window))
	move_has_been_played (goban_window);

      update_children_for_new_node (goban_window, TRUE, FALSE);
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
					   goban_window->node_to_switch_to);
      just_changed_node (goban_window);

      update_children_for_new_node (goban_window, TRUE, FALSE);
    }
    else
      cancel_amazons_move (goban_window);

    break;
  }
}


static GtkGobanPointerFeedback
setup_mode_pointer_moved (GtkGobanWindow *goban_window,
			  GtkGobanPointerData *data)
{
  Board *const board = goban_window->board;
  int pos = POSITION (data->x, data->y);

  switch (data->button) {
  case 0:
    if (data->modifiers & GDK_CONTROL_MASK
	&& board->game == GAME_GO) {
      if (!IS_STONE (board->grid[pos]))
	break;

      data->feedback_position_list = go_get_string_stones (board,
							   data->x, data->y);
      return GOBAN_FEEDBACK_THICK_GHOST + COLOR_INDEX (board->grid[pos]);
    }

    return (data->modifiers & GDK_SHIFT_MASK
	    ? GOBAN_FEEDBACK_ADD_BLACK_OR_REMOVE
	    : GOBAN_FEEDBACK_ADD_WHITE_OR_REMOVE);

  case 1:
  case 3:
    gtk_goban_disable_anti_slip_mode (goban_window->goban);

    if (data->modifiers & GDK_CONTROL_MASK
	&& board->game == GAME_GO) {
      if (!IS_STONE (board->grid[POSITION (data->press_x, data->press_y)])
	  || !go_is_same_string (board, data->press_x, data->press_y,
				 data->x, data->y))
	break;

      data->feedback_position_list = go_get_string_stones (board,
							   data->x, data->y);
      return GOBAN_FEEDBACK_GHOST + COLOR_INDEX (board->grid[pos]);
    }

    if (!goban_window->drawn_position_list) {
      if (IS_STONE (board->grid[pos]))
	goban_window->drawing_mode = EMPTY;
      else {
	goban_window->drawing_mode = ((data->button == 3
				       || (data->modifiers & GDK_SHIFT_MASK))
				      ? BLACK : WHITE);
      }
    }

    goban_window->drawn_position_list
      = board_position_list_add_position (goban_window->drawn_position_list,
					  pos);
    data->feedback_position_list
      = board_position_list_duplicate (goban_window->drawn_position_list);

    switch (goban_window->drawing_mode) {
    case EMPTY:
      return GOBAN_FEEDBACK_GHOSTIFY;

    case BLACK:
      return GOBAN_FEEDBACK_THICK_BLACK_GHOST;

    case WHITE:
      return GOBAN_FEEDBACK_THICK_WHITE_GHOST;

    default:
      g_assert_not_reached ();
    }
  }

  return GOBAN_FEEDBACK_NONE;
}


static void
setup_mode_goban_clicked (GtkGobanWindow *goban_window,
			  GtkGobanClickData *data)
{
  if (data->button != 1 && data->button != 3)
    return;

  if (data->modifiers & GDK_CONTROL_MASK
      && goban_window->board->game == GAME_GO) {
    BoardPositionList *string_stones;

    if (!data->non_empty_feedback)
      return;

    string_stones = go_get_string_stones (goban_window->board,
					  data->x, data->y);

    gtk_goban_set_contents (goban_window->goban, string_stones,
			    EMPTY, GOBAN_TILE_DONT_CHANGE,
			    GOBAN_SGF_MARKUP_TILE_DONT_CHANGE);
    board_position_list_delete (string_stones);
  }
  else {
    gtk_goban_set_contents (goban_window->goban,
			    goban_window->drawn_position_list,
			    goban_window->drawing_mode,
			    GOBAN_TILE_DONT_CHANGE,
			    GOBAN_SGF_MARKUP_TILE_DONT_CHANGE);

    board_position_list_delete (goban_window->drawn_position_list);
    goban_window->drawn_position_list = NULL;
  }

  if (sgf_utils_apply_setup_changes (goban_window->current_tree,
				     goban_window->goban->grid, 0))
    update_children_for_new_node (goban_window, TRUE, FALSE);
}


static GtkGobanPointerFeedback
markup_mode_pointer_moved (GtkGobanWindow *goban_window,
			   GtkGobanPointerData *data)
{
  GtkGoban *const goban = goban_window->goban;
  Board *const board = goban_window->board;
  int pos = POSITION (data->x, data->y);

  switch (data->button) {
  case 0:
    {
      int is_over_same_existing_markup
	= (gtk_goban_get_sgf_markup_contents (goban, data->x, data->y)
	   == goban_window->sgf_markup_type);

      if (data->modifiers & GDK_CONTROL_MASK
	  && board->game == GAME_GO) {
	if (!IS_STONE (board->grid[pos]))
	  break;

	data->feedback_position_list = go_get_string_stones (board,
							     data->x, data->y);

	if (is_over_same_existing_markup) {
	  return (GOBAN_FEEDBACK_SGF_GHOSTIFY_SLIGHTLY
		  * GOBAN_FEEDBACK_SGF_FACTOR);
	}
      }

      return (((is_over_same_existing_markup
		? GOBAN_FEEDBACK_SGF_THICK_GHOST : GOBAN_FEEDBACK_SGF_GHOST)
	       + goban_window->sgf_markup_type)
	      * GOBAN_FEEDBACK_SGF_FACTOR);
    }

  case 1:
  case 3:
    gtk_goban_disable_anti_slip_mode (goban_window->goban);

    if (gtk_goban_get_sgf_markup_contents (goban, data->press_x, data->press_y)
	== goban_window->sgf_markup_type)
      goban_window->drawing_mode = SGF_MARKUP_NONE;
    else
      goban_window->drawing_mode = goban_window->sgf_markup_type;

    if (data->modifiers & GDK_CONTROL_MASK
	&& board->game == GAME_GO) {
      if (!IS_STONE (board->grid[POSITION (data->press_x, data->press_y)])
	  || !go_is_same_string (board, data->press_x, data->press_y,
				 data->x, data->y))
	break;

      data->feedback_position_list = go_get_string_stones (board,
							   data->x, data->y);
    }
    else {
      goban_window->drawn_position_list
	= board_position_list_add_position (goban_window->drawn_position_list,
					    pos);
      data->feedback_position_list
	= board_position_list_duplicate (goban_window->drawn_position_list);
    }

    if (goban_window->drawing_mode == SGF_MARKUP_NONE)
      return GOBAN_FEEDBACK_SGF_GHOSTIFY * GOBAN_FEEDBACK_SGF_FACTOR;
    else {
      return ((GOBAN_FEEDBACK_SGF_THICK_GHOST + goban_window->drawing_mode)
	      * GOBAN_FEEDBACK_SGF_FACTOR);
    }
  }

  return GOBAN_FEEDBACK_NONE;
}


static void
markup_mode_goban_clicked (GtkGobanWindow *goban_window,
			   GtkGobanClickData *data)
{
  if (data->button != 1 && data->button != 3)
    return;

  if (data->modifiers & GDK_CONTROL_MASK
      && goban_window->board->game == GAME_GO) {
    BoardPositionList *string_stones;

    if (!data->non_empty_feedback)
      return;

    string_stones = go_get_string_stones (goban_window->board,
					  data->x, data->y);

    gtk_goban_set_contents (goban_window->goban, string_stones,
			    GOBAN_TILE_DONT_CHANGE, GOBAN_TILE_DONT_CHANGE,
			    goban_window->drawing_mode);
    board_position_list_delete (string_stones);
  }
  else {
    gtk_goban_set_contents (goban_window->goban,
			    goban_window->drawn_position_list,
			    GOBAN_TILE_DONT_CHANGE,
			    GOBAN_TILE_DONT_CHANGE,
			    goban_window->drawing_mode);

    board_position_list_delete (goban_window->drawn_position_list);
    goban_window->drawn_position_list = NULL;
  }

  sgf_utils_apply_markup_changes (goban_window->current_tree,
				  goban_window->goban->sgf_markup, 0);
}


static GtkGobanPointerFeedback
label_mode_pointer_moved (GtkGobanWindow *goban_window,
			  GtkGobanPointerData *data)
{
  /* FIXME: Make configurable.  E.g. some people may prefer upper-case
   *	    labels or a different alphabet.
   */
  static const gchar *labels = "abcdefghijklmnopqrstuvwxyz";

  if (data->button == 0 && !goban_window->next_sgf_label) {
    const SgfNode *current_node = goban_window->current_tree->current_node;
    const SgfLabelList *label_list
      = sgf_node_get_list_of_label_property_value (current_node, SGF_LABEL);
    int k;

    switch (goban_window->labels_mode) {
    case GTK_GOBAN_WINDOW_TEXT_LABELS_MODE:
      for (k = 0; k < strlen (labels); k++) {
	goban_window->next_sgf_label = utils_duplicate_as_string (labels + k,
								  1);
	if (!label_list
	    || !sgf_label_list_contains_label (label_list,
					       goban_window->next_sgf_label))
	  break;

	utils_free (goban_window->next_sgf_label);
	goban_window->next_sgf_label = NULL;
      }

      break;

    case GTK_GOBAN_WINDOW_NUMERIC_LABELS_MODE:
      for (k = 1; ; k++) {
	goban_window->next_sgf_label = utils_printf ("%d", k);
	if (!label_list
	    || !sgf_label_list_contains_label (label_list,
					       goban_window->next_sgf_label))
	  break;

	utils_free (goban_window->next_sgf_label);
	goban_window->next_sgf_label = NULL;
      }

      break;

    default:
      g_assert_not_reached ();
    }
  }

  if ((data->button == 1 && (data->modifiers & GDK_SHIFT_MASK))
      || data->button == 3)
    return GOBAN_FEEDBACK_NONE;

  if (data->button == 0 || data->button == 1) {
    const gchar *label	     = goban_window->next_sgf_label;
    int		 ghost_level = (data->button == 0
				? LABEL_50_TRANSPARENT : LABEL_25_TRANSPARENT);
    const SgfNode *current_node = goban_window->current_tree->current_node;
    const SgfLabelList *label_list
      = sgf_node_get_list_of_label_property_value (current_node, SGF_LABEL);

    if (label_list) {
      BoardPoint  point		 = { data->x, data->y };
      const char *existing_label = sgf_label_list_get_label (label_list,
							     point);

      if (existing_label) {
	label	    = existing_label;
	ghost_level = (data->button == 0
		       ? LABEL_25_TRANSPARENT : LABEL_50_TRANSPARENT);
      }
    }

    if (label) {
      gtk_goban_set_label_feedback (goban_window->goban, data->x, data->y,
				    label, ghost_level);
    }
    else {
      gtk_goban_set_label_feedback (goban_window->goban,
				    NULL_X, NULL_Y, NULL, 0);
    }
  }

  return GOBAN_FEEDBACK_NONE;
}


static void
label_mode_goban_clicked (GtkGobanWindow *goban_window,
			  GtkGobanClickData *data)
{
  SgfNode *current_node = goban_window->current_tree->current_node;
  const SgfLabelList *old_label_list;
  const char *existing_label = NULL;
  BoardPoint point = { data->x, data->y };
  SgfLabelList *new_label_list;

  if (data->button != 1 && data->button != 3)
    return;

  old_label_list = sgf_node_get_list_of_label_property_value (current_node,
							      SGF_LABEL);
  if (old_label_list)
    existing_label = sgf_label_list_get_label (old_label_list, point);

  if (data->button == 1 && !(data->modifiers & GDK_SHIFT_MASK)) {
    if (!existing_label) {
      new_label_list = sgf_label_list_set_label (old_label_list, point,
						 goban_window->next_sgf_label);
      goban_window->next_sgf_label = NULL;
    }
    else
      new_label_list = sgf_label_list_set_label (old_label_list, point, NULL);
  }
  else {
    GtkWidget *dialog = gtk_add_or_edit_label_dialog_new ();

    if (existing_label) {
      gtk_add_or_edit_label_dialog_set_label_text
	(GTK_ADD_OR_EDIT_LABEL_DIALOG (dialog), existing_label);
      gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Board Label"));
    }
    else
      gtk_window_set_title (GTK_WINDOW (dialog), _("Add Board Label"));

    gtk_window_set_transient_for (GTK_WINDOW (dialog),
				  GTK_WINDOW (goban_window));

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
      const gchar *new_label_text
	= (gtk_add_or_edit_label_dialog_get_label_text
	   (GTK_ADD_OR_EDIT_LABEL_DIALOG (dialog)));

      if (! *new_label_text)
	new_label_text = NULL;

      new_label_list
	= sgf_label_list_set_label (old_label_list, point,
				    utils_duplicate_string (new_label_text));

      gtk_widget_destroy (dialog);
    }
    else {
      gtk_widget_destroy (dialog);
      return;
    }
  }

  if (sgf_utils_set_list_of_label_property (current_node,
					    goban_window->current_tree,
					    SGF_LABEL, new_label_list, 0))
    update_children_for_new_node (goban_window, TRUE, FALSE);
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
      && data->non_empty_feedback
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
      g_assert_not_reached ();

    gtk_goban_set_contents (goban_window->goban, position_list,
			    contents, GOBAN_TILE_DONT_CHANGE,
			    GOBAN_SGF_MARKUP_TILE_DONT_CHANGE);
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
	int color = goban_window->board->grid[pos];

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
  else if (button_index == 3) {
    if (sgf_node->child) {
      sgf_utils_set_node_is_collapsed (goban_window->current_tree, sgf_node,
				       !sgf_node->is_collapsed);
    }
  }
}


static void
delete_drawn_position_list (GtkGobanWindow *goban_window)
{
  if (goban_window->drawn_position_list) {
    board_position_list_delete (goban_window->drawn_position_list);
    goban_window->drawn_position_list = NULL;
  }
}


static void
navigate_goban (GtkGobanWindow *goban_window,
		GtkGobanNavigationCommand command)
{
  SgfGameTree *current_tree = goban_window->current_tree;
  SgfNode *current_node = current_tree->current_node;

  if (IS_IN_SPECIAL_MODE (goban_window))
    return;

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
    sgf_utils_go_up_in_tree (current_tree, 1);
    break;

  case GOBAN_NAVIGATE_BACK_FAST:
    sgf_utils_go_up_in_tree (current_tree, NAVIGATE_FAST_NUM_MOVES);
    break;

  case GOBAN_NAVIGATE_FORWARD:
    sgf_utils_go_down_in_tree (current_tree, 1);
    break;

  case GOBAN_NAVIGATE_FORWARD_FAST:
    sgf_utils_go_down_in_tree (current_tree, NAVIGATE_FAST_NUM_MOVES);
    break;

  case GOBAN_NAVIGATE_PREVIOUS_VARIATION:
    sgf_utils_switch_to_variation (current_tree, SGF_PREVIOUS);
    break;

  case GOBAN_NAVIGATE_NEXT_VARIATION:
    sgf_utils_switch_to_variation (current_tree, SGF_NEXT);
    break;

  case GOBAN_NAVIGATE_ROOT:
    sgf_utils_go_up_in_tree (current_tree, -1);
    break;

  case GOBAN_NAVIGATE_VARIATION_END:
    sgf_utils_go_down_in_tree (current_tree, -1);
    break;
  }

  just_changed_node (goban_window);
  update_children_for_new_node (goban_window, TRUE, FALSE);
}


static void
switch_to_given_node (GtkGobanWindow *goban_window, SgfNode *sgf_node)
{
  about_to_change_node (goban_window);
  sgf_utils_switch_to_given_node (goban_window->current_tree, sgf_node);
  just_changed_node (goban_window);

  update_children_for_new_node (goban_window, FALSE, FALSE);
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
update_children_for_new_node (GtkGobanWindow *goban_window, gboolean forced,
			      gboolean text_handled)
{
  const SgfBoardState *const board_state = &goban_window->sgf_board_state;
  SgfGameTree *current_tree = goban_window->current_tree;
  SgfNode *current_node = current_tree->current_node;
  QuarryTextBufferState *text_buffer_state = NULL;
  char goban_markup[BOARD_GRID_SIZE];

  if (!forced && current_node == goban_window->last_displayed_node)
    return;

  if (goban_window->last_displayed_node
      && goban_window->last_displayed_node != current_node
      && !text_handled) {
    text_buffer_state = update_comment_and_node_name_if_needed (goban_window,
								FALSE);
  }

  reset_amazons_move_data (goban_window);

  /* FIXME: Probably not the right place for it. */
  utils_free (goban_window->next_sgf_label);
  goban_window->next_sgf_label = NULL;

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
		    board_state->last_move_x, board_state->last_move_y);
  gtk_goban_force_feedback_poll (goban_window->goban);

  if (goban_window->last_game_info_node != board_state->game_info_node)
    update_game_information (goban_window);

  update_game_specific_information (goban_window);
  update_move_information (goban_window);

  if (!text_handled
      && (goban_window->last_displayed_node != current_node
	  || !gtk_text_buffer_get_modified (goban_window->text_buffer)))
    set_comment_and_node_name (goban_window, current_node);

  if (text_buffer_state) {
    quarry_text_buffer_get_state (QUARRY_TEXT_BUFFER (goban_window->text_buffer),
				  text_buffer_state);
  }

  if (!goban_window->in_game_mode) {
    int k;

    for (k = 0; k < NUM_COLORS; k++) {
      if (goban_window->time_controls[k]) {
	double time_left;
	int moves_left;

	time_control_apply_defaults_if_needed (goban_window->time_controls[k],
					       board_state->time_left[k],
					       board_state->moves_left[k],
					       &time_left, &moves_left);

	gtk_clock_set_time (goban_window->clocks[k], time_left, moves_left);
      }
      else {
	gtk_clock_set_time (goban_window->clocks[k],
			    board_state->time_left[k],
			    board_state->moves_left[k]);
      }
    }
  }

  show_sgf_tree_view_automatically (goban_window, current_node);

  update_commands_sensitivity (goban_window);
  update_set_player_to_move_commands (goban_window);

  goban_window->switching_x = NULL_X;
  goban_window->switching_y = NULL_Y;

  goban_window->last_displayed_node = current_node;
}


static void
set_comment_and_node_name (GtkGobanWindow *goban_window,
			   const SgfNode *sgf_node)
{
  const char *comment = sgf_node_get_text_property_value (sgf_node,
							  SGF_COMMENT);
  const char *node_name = sgf_node_get_text_property_value (sgf_node,
							    SGF_NODE_NAME);

  gtk_utils_block_signal_handlers (goban_window->text_buffer,
				   text_buffer_receive_undo_entry);

  goban_window->node_name_inserted = FALSE;
  gtk_utils_set_text_buffer_text (goban_window->text_buffer, comment);

  if (node_name)
    insert_node_name (goban_window, node_name);

  gtk_text_buffer_set_modified (goban_window->text_buffer, FALSE);

  gtk_utils_unblock_signal_handlers (goban_window->text_buffer,
				     text_buffer_receive_undo_entry);
}


static void
update_game_information (GtkGobanWindow *goban_window)
{
  SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;

  update_window_title (goban_window);

  update_player_information (goban_window, BLACK);
  update_player_information (goban_window, WHITE);

  goban_window->last_game_info_node = game_info_node;

  if (goban_window->game_info_dialog) {
    gtk_game_info_dialog_set_node (goban_window->game_info_dialog,
				   goban_window->current_tree, game_info_node);
  }
}


static void
update_window_title (GtkGobanWindow *goban_window)
{
  const SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;
  const char *game_name = NULL;
  char *string_to_free  = NULL;
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

  if (goban_window->filename) {
    gchar *filename_in_utf8 = g_filename_to_utf8 (goban_window->filename, -1,
						  NULL, NULL, NULL);
    char *base_name = g_path_get_basename (filename_in_utf8);

    if (game_name)
      title = utils_cat_strings (NULL, game_name, " (", base_name, ")", NULL);
    else
      title = utils_duplicate_string (base_name);

    g_free (base_name);
    g_free (filename_in_utf8);
  }
  else {
    if (game_name)
      title = utils_duplicate_string (game_name);
    else
      title = utils_duplicate_string (_("Unnamed Game"));
  }

  if (sgf_collection_is_modified (goban_window->sgf_collection)
      || !goban_window->filename)
    title = utils_cat_strings (title, " [", _("modified"), "]", NULL);

  gtk_window_set_title (GTK_WINDOW (goban_window), title);

  utils_free (title);
  g_free (string_to_free);
}


static void
update_player_information (GtkGobanWindow *goban_window, int player_color)
{
  SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;
  const char *name = NULL;
  const char *rank = NULL;
  const char *team = NULL;
  char *label_text;

  if (game_info_node) {
    name = sgf_node_get_text_property_value (game_info_node,
					     (player_color == BLACK
					      ? SGF_PLAYER_BLACK
					      : SGF_PLAYER_WHITE));
    rank = sgf_node_get_text_property_value (game_info_node,
					     (player_color == BLACK
					      ? SGF_BLACK_RANK
					      : SGF_WHITE_RANK));
    team = sgf_node_get_text_property_value (game_info_node,
					     (player_color == BLACK
					      ? SGF_BLACK_TEAM
					      : SGF_WHITE_TEAM));
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

  gtk_label_set_text (goban_window->player_labels[COLOR_INDEX (player_color)],
		      label_text);
  utils_free (label_text);
}


static void
update_game_specific_information (const GtkGobanWindow *goban_window)
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

    white_string
      = g_strdup_printf (ngettext ("%d capture", "%d captures",
				   board->data.go.prisoners[WHITE_INDEX]),
			 board->data.go.prisoners[WHITE_INDEX]);

    if (game_info_node
	&& sgf_node_get_komi (game_info_node, &komi) && komi != 0.0) {
      gchar *full_white_string;

      if ((fabs (fabs (komi) - floor (fabs (komi) + 0.005)) >= 0.005)) {
	full_white_string
	  = g_strdup_printf (_("%s %s %.*f komi"),
			     white_string,
			     (komi >= 0.0 ? "+" : "\xe2\x88\x92"),
			     ((int) floor (komi * 100.0 + 0.5) % 10 == 0
			      ? 1 : 2),
			     fabs (komi));
      }
      else {
	int absolute_integral_komi = (int) floor (fabs (komi) + 0.005);

	full_white_string
	  = g_strdup_printf (ngettext ("%s %s %d komi", "%s %s %d komi",
				       absolute_integral_komi),
			     white_string,
			     (komi >= 0.0 ? "+" : "\xe2\x88\x92"),
			     absolute_integral_komi);
      }

      g_free (white_string);
      white_string = full_white_string;
    }
  }
  else if (board->game == GAME_REVERSI) {
    int num_black_disks;
    int num_white_disks;

    reversi_count_disks (board, &num_black_disks, &num_white_disks);
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
update_move_information (const GtkGobanWindow *goban_window)
{
  const SgfNode *move_node = goban_window->sgf_board_state.last_move_node;
  const SgfNode *game_info_node = goban_window->sgf_board_state.game_info_node;
  SgfResult result = SGF_RESULT_NOT_SET;
  gboolean result_is_final;
  double score;
  StringBuffer buffer;

  string_buffer_init (&buffer, 0x100, 0x100);

  if (move_node) {
    if (goban_window->current_tree->current_node == move_node) {
      string_buffer_printf (&buffer, _("Move %d: "),
			    goban_window->board->move_number);
    }
    else {
      string_buffer_printf (&buffer, _("Last move: %u, "),
			    goban_window->board->move_number);
    }

    sgf_utils_format_node_move (goban_window->current_tree, move_node, &buffer,
				/* TRANSLATORS: This is an
				   abbreviation of `Black'. */
				_("B "),
				/* TRANSLATORS: This is an
				   abbreviation of `White'. */
				_("W "),
				_("pass"));
  }
  else
    string_buffer_cat_string (&buffer, _("Game beginning"));

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
    string_buffer_cat_string (&buffer,
			      ((goban_window->sgf_board_state.color_to_play
				== BLACK)
			       ? _("; black to play") : _("; white to play")));
  }
  else
    string_buffer_cat_string (&buffer, _("; game over"));

  if (result_is_final) {
    string_buffer_add_character (&buffer, '\n');

    switch (result) {
    case SGF_RESULT_BLACK_WIN:
      string_buffer_cat_string (&buffer, _("Black wins"));
      break;
    case SGF_RESULT_WHITE_WIN:
      string_buffer_cat_string (&buffer, _("White wins"));
      break;

    case SGF_RESULT_BLACK_WIN_BY_FORFEIT:
      string_buffer_cat_string (&buffer, _("Black wins by forfeit"));
      break;
    case SGF_RESULT_WHITE_WIN_BY_FORFEIT:
      string_buffer_cat_string (&buffer, _("White wins by forfeit"));
      break;

    case SGF_RESULT_BLACK_WIN_BY_RESIGNATION:
      string_buffer_cat_string (&buffer, _("White resigns"));
      break;
    case SGF_RESULT_WHITE_WIN_BY_RESIGNATION:
      string_buffer_cat_string (&buffer, _("Black resigns"));
      break;

    case SGF_RESULT_BLACK_WIN_BY_SCORE:
    case SGF_RESULT_WHITE_WIN_BY_SCORE:
      game_format_score_difference (goban_window->board->game, &buffer,
				    (result == SGF_RESULT_BLACK_WIN_BY_SCORE
				     ? score : -score));
      break;

    case SGF_RESULT_BLACK_WIN_BY_TIME:
      string_buffer_cat_string (&buffer,
				_("White runs out of time and loses"));
      break;
    case SGF_RESULT_WHITE_WIN_BY_TIME:
      string_buffer_cat_string (&buffer,
				_("Black runs out of time and loses"));
      break;

    case SGF_RESULT_DRAW:
      string_buffer_cat_string (&buffer, _("Game is draw"));
      break;

    default:
      g_assert_not_reached ();
    }
  }

  gtk_label_set_text (goban_window->move_information_label, buffer.string);
  string_buffer_dispose (&buffer);
}


static void
update_commands_sensitivity (const GtkGobanWindow *goban_window)
{
  const SgfGameTree *current_tree = goban_window->current_tree;
  const SgfNode *current_node	  = current_tree->current_node;

  gboolean is_in_special_mode = IS_IN_SPECIAL_MODE (goban_window);
  gboolean pass_sensitive     = (goban_window->board->game == GAME_GO
				 && USER_CAN_PLAY_MOVES (goban_window)
				 && !is_in_special_mode);
  gboolean resign_sensitive   = (goban_window->in_game_mode
				 && IS_DISPLAYING_GAME_NODE (goban_window)
				 && USER_CAN_PLAY_MOVES (goban_window)
				 && !is_in_special_mode);
  gboolean previous_node_sensitive	= (current_node->parent != NULL
					   && !is_in_special_mode);
  gboolean next_node_sensitive		= (current_node->child != NULL
					   && !is_in_special_mode);
  gboolean previous_variation_sensitive = (current_node->parent != NULL
					   && (current_node->parent->child
					       != current_node)
					   && !is_in_special_mode);
  gboolean next_variation_sensitive	= (current_node->next != NULL
					   && !is_in_special_mode);

  /* "File" submenu, */
  gtk_utils_set_menu_items_sensitive
    (goban_window->item_factory, goban_window->current_tree->game == GAME_GO,
     "/File/Export.../Sensei's Library Diagram", NULL);

  /* "Edit" submenu. */
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      (!goban_window->in_game_mode
				       && current_node->parent != NULL),
				      "/Edit/Cut", NULL);				      
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      !goban_window->in_game_mode,
				      "/Edit/Paste", NULL);
				      
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      (USER_CAN_PLAY_MOVES (goban_window)
				       && !is_in_special_mode),
				      "/Edit/Add Empty Node", NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      previous_variation_sensitive,
				      "/Edit/Move Branch Up", NULL);
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      next_variation_sensitive,
				      "/Edit/Move Branch Down", NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      (!goban_window->in_game_mode
				       && current_node->parent != NULL),
				      "/Edit/Delete Node", NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->editing_toolbar,
					   (!goban_window->in_game_mode
					    && current_node->parent != NULL),
					   &editing_toolbar_delete, NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      (!goban_window->in_game_mode
				       && current_node->child != NULL),
				      "/Edit/Delete Node's Children", NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      /* FIXME: Not strictly following
				       *	SGF, but let's not
				       *	care for now.
				       */
				      IS_STONE (current_node->move_color),
				      "/Edit/Set Move Number", NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      !is_in_special_mode,
				      "/Edit/Find", "/Edit/Find Next",
				      "/Edit/Find Previous", NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->main_toolbar,
					   !is_in_special_mode,
					   &toolbar_find, NULL);

  /* "Edit/Tools" submenu. */

  /* Only desensitize when scoring a game. */
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      (!goban_window->in_game_mode
				       || !goban_window->dead_stones),
				      "/Edit/Tools/Move Tool", NULL);

  /* Only desensitize when playing a game. */
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      !goban_window->in_game_mode,
				      "/Edit/Tools/Setup Tool",
				      "/Edit/Tools/Cross Markup",
				      "/Edit/Tools/Circle Markup",
				      "/Edit/Tools/Square Markup",
				      "/Edit/Tools/Triangle Markup",
				      "/Edit/Tools/Selected Markup",
				      "/Edit/Tools/Label Tool",
				      "/Edit/Tools/Number Tool", NULL);

  /* Only desensitize when playing a game and not scoring already. */
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      (!goban_window->in_game_mode
				       || goban_window->dead_stones),
				      "/Edit/Tools/Scoring Tool", NULL);

  synchronize_tools_menus (goban_window);

  /* "Play" submenu. */
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      pass_sensitive, "/Play/Pass", NULL);
  gtk_widget_set_sensitive (goban_window->pass_button, pass_sensitive);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      resign_sensitive, "/Play/Resign", NULL);
  gtk_widget_set_sensitive (goban_window->resign_button, resign_sensitive);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      (goban_window->in_game_mode
				       && !is_in_special_mode),
				      "/Play/Adjourn Game", NULL);

  /* "Go" submenu. */
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      previous_node_sensitive,
				      "/Go/Previous Node",
				      "/Go/Ten Nodes Backward",
				      "/Go/Root Node", NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->navigation_toolbar,
					   previous_node_sensitive,
					   &navigation_toolbar_back,
					   &navigation_toolbar_root, NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      next_node_sensitive,
				      "/Go/Next Node",
				      "/Go/Ten Nodes Forward",
				      "/Go/Variation Last Node", NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->navigation_toolbar,
					   next_node_sensitive,
					   &navigation_toolbar_forward,
					   &navigation_toolbar_variation_end,
					   NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      previous_variation_sensitive,
				      "/Go/Previous Variation", NULL);
  gtk_utils_set_toolbar_buttons_sensitive
    (goban_window->navigation_toolbar,
     previous_variation_sensitive,
     &navigation_toolbar_previous_variation, NULL);

  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      next_variation_sensitive,
				      "/Go/Next Variation", NULL);
  gtk_utils_set_toolbar_buttons_sensitive (goban_window->navigation_toolbar,
					   next_variation_sensitive,
					   &navigation_toolbar_next_variation,
					   NULL);

#ifdef GTK_TYPE_GO_TO_NAMED_NODE_DIALOG
  gtk_utils_set_menu_items_sensitive (goban_window->item_factory,
				      !is_in_special_mode,
				      "/Go/Go to Named Node...", NULL);
#endif
}


static void
update_set_player_to_move_commands (GtkGobanWindow *goban_window)
{
  const SgfGameTree *game_tree = goban_window->current_tree;
  const gchar *menu_item_text;
  GtkWidget *menu_item;

  goban_window->updating_set_player_commands = TRUE;

  switch (game_tree->current_node->to_play_color) {
  case EMPTY:
    menu_item_text = "/Edit/Player to Move/By Game Rules";
    break;

  case BLACK:
    menu_item_text = "/Edit/Player to Move/Black";
    break;

  case WHITE:
    menu_item_text = "/Edit/Player to Move/White";
    break;

  default:
    g_assert_not_reached ();
  }

  menu_item = gtk_item_factory_get_widget (goban_window->item_factory,
					   menu_item_text);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), TRUE);

  menu_item
    = gtk_item_factory_get_widget (goban_window->item_factory,
				   "/Edit/Player to Move/By Game Rules");

  switch (sgf_utils_determine_player_to_move_by_rules (game_tree)) {
  case EMPTY:
    menu_item_text = _("By Game _Rules (Game Over)");
    break;

  case BLACK:
    menu_item_text = _("By Game _Rules (Black)");
    break;

  case WHITE:
    menu_item_text = _("By Game _Rules (White)");
    break;
  }

  gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (menu_item)->child),
				    menu_item_text);

  goban_window->updating_set_player_commands = FALSE;
}


static void
insert_node_name (GtkGobanWindow *goban_window, const gchar *node_name)
{
  static const gchar node_name_and_comment_separator = '\n';

  GtkTextIter start_iterator;

  gtk_text_buffer_begin_user_action (goban_window->text_buffer);

  gtk_text_buffer_get_start_iter (goban_window->text_buffer, &start_iterator);

  gtk_text_buffer_insert_with_tags (goban_window->text_buffer, &start_iterator,
				    node_name, -1, node_name_tag, NULL);
  gtk_text_buffer_insert_with_tags (goban_window->text_buffer, &start_iterator,
				    &node_name_and_comment_separator, 1,
				    node_name_tag, separator_tag, NULL);

  gtk_text_buffer_end_user_action (goban_window->text_buffer);
}


static void
select_node_name (GtkGobanWindow *goban_window)
{
  GtkTextIter start_iterator;
  GtkTextIter second_line_iterator;

  gtk_widget_grab_focus (GTK_WIDGET (goban_window->text_view));

  if (!goban_window->node_name_inserted) {
    /* If it is not inserted, then there is no node name.  Start with
     * a hint so that user knows what to do.
     */
    insert_node_name (goban_window, _("insert node name here"));
  }

  gtk_text_buffer_get_start_iter (goban_window->text_buffer, &start_iterator);
  gtk_text_buffer_get_iter_at_line (goban_window->text_buffer,
				    &second_line_iterator, 1);
  gtk_text_iter_backward_char (&second_line_iterator);

  gtk_text_buffer_select_range (goban_window->text_buffer,
				&second_line_iterator, &start_iterator);
  gtk_text_view_scroll_to_iter (goban_window->text_view, &start_iterator,
				0.0, FALSE, 0.0, 0.0);
}


static void
text_buffer_insert_text (GtkTextBuffer *text_buffer,
			 GtkTextIter *insertion_iterator,
			 const gchar *text, guint length,
			 GtkGobanWindow *goban_window)
{
  gint paragraph_delimiter_index;
  gint next_paragraph_start;
  gchar *new_text;
  gchar *new_text_pointer;
  gboolean first_time = TRUE;

  /* We only need to do anything if there is a node name in the buffer
   * and we are inserting something into it.  Also don't accidentally
   * stop signal emission during undoing or redoing.
   */
  if (!goban_window->node_name_inserted
      || gtk_text_iter_get_line (insertion_iterator) > 0
      || quarry_text_buffer_is_undoing_or_redoing (QUARRY_TEXT_BUFFER
						   (text_buffer)))
    return;

  /* Don't insert paragraph terminators in the node name.  If there
   * are any, replace them with spaces and re-emit this signal.
   */

  pango_find_paragraph_boundary (text, length, &paragraph_delimiter_index,
				 &next_paragraph_start);
  if (paragraph_delimiter_index == length)
    return;

  /* We will not expand the text, only maybe shrink it. */
  new_text	   = g_malloc (length);
  new_text_pointer = new_text;

  do {
    if (paragraph_delimiter_index > 0) {
      memcpy (new_text_pointer, text, paragraph_delimiter_index);
      new_text_pointer += paragraph_delimiter_index;
    }

    /* Insert a space if the paragraph was non-empty, or the
     * paragraph delimiter is at the very beginning/end of the
     * text.
     */
    if (paragraph_delimiter_index > 0
	|| first_time || next_paragraph_start == length)
      *new_text_pointer++ = ' ';

    text   += next_paragraph_start;
    length -= next_paragraph_start;

    first_time = FALSE;
    pango_find_paragraph_boundary (text, length, &paragraph_delimiter_index,
				   &next_paragraph_start);
  } while (paragraph_delimiter_index < length);

  g_signal_stop_emission_by_name (text_buffer, "insert-text");

  /* Block ourselves: there will not be any paragraphs this time. */
  g_signal_handlers_block_by_func (text_buffer, text_buffer_insert_text,
				   goban_window);
  gtk_text_buffer_insert (text_buffer, insertion_iterator,
			  new_text, new_text_pointer - new_text);
  g_signal_handlers_unblock_by_func (text_buffer, text_buffer_insert_text,
				     goban_window);

  g_free (new_text);
}


static void
text_buffer_after_insert_text (GtkTextBuffer *text_buffer,
			       GtkTextIter *insertion_iterator,
			       const gchar *text, guint length,
			       GtkGobanWindow *goban_window)
{
  GtkTextIter start_iterator;

  if (!goban_window->node_name_inserted
      || (gtk_text_iter_get_offset (insertion_iterator)
	  != g_utf8_strlen (text, length))
      || quarry_text_buffer_is_undoing_or_redoing (QUARRY_TEXT_BUFFER
						   (text_buffer)))
    return;

  /* We have inserted text before the node name.  Apply the node name
   * tag to it.  Note that `insertion_iterator' has been moved to
   * inserted text end by now.
   */
  gtk_text_buffer_get_start_iter (text_buffer, &start_iterator);
  gtk_text_buffer_apply_tag (text_buffer, node_name_tag,
			     &start_iterator, insertion_iterator);
}


static void
text_buffer_mark_set (GtkTextBuffer *text_buffer,
		      GtkTextIter *new_position_iterator, GtkTextMark *mark,
		      GtkGobanWindow *goban_window)
{
  GtkTextIter start_iterator;
  GtkTextIter bound_iterator;
  GtkTextIter node_name_end_iterator;

  if (!goban_window->node_name_inserted
      || mark != gtk_text_buffer_get_insert (text_buffer)
      || gtk_text_iter_get_line (new_position_iterator) == 0
      || quarry_text_buffer_is_undoing_or_redoing (QUARRY_TEXT_BUFFER
						   (text_buffer)))
    return;

  gtk_text_buffer_get_start_iter (text_buffer, &start_iterator);
  if (gtk_text_iter_get_char (&start_iterator) != '\n')
    return;

  gtk_text_buffer_get_iter_at_mark (text_buffer, &bound_iterator,
				    (gtk_text_buffer_get_selection_bound
				     (text_buffer)));
  if (gtk_text_iter_get_line (&bound_iterator) == 0)
    return;

  /* We have node name inserted in the text buffer, but it has become
   * empty and the cursor is somewhere else.  Remove the node name
   * from the buffer.
   */

  node_name_end_iterator = start_iterator;
  gtk_text_iter_forward_char (&node_name_end_iterator);

  gtk_text_buffer_delete (text_buffer, &start_iterator,
			  &node_name_end_iterator);
}


static void
text_buffer_end_user_action (GtkTextBuffer *text_buffer,
			     GtkGobanWindow *goban_window)
{
  GtkTextIter start_iterator;
  GSList *tags;

  gtk_text_buffer_get_start_iter (text_buffer, &start_iterator);
  tags = gtk_text_iter_get_tags (&start_iterator);

  goban_window->node_name_inserted = (g_slist_find (tags, node_name_tag)
				      != NULL);

  g_slist_free (tags);
}


static gboolean
text_buffer_receive_undo_entry (QuarryTextBuffer *text_buffer,
				QuarryTextBufferUndoEntry *undo_entry,
				GtkGobanWindow *goban_window)
{
  SgfGameTree *game_tree = goban_window->current_tree;
  SgfNode *node = game_tree->current_node;
  GtkGobanWindowUndoEntryData *undo_entry_data;

  if (game_tree->undo_history
      && (sgf_undo_history_is_last_applied_entry_single
	  (game_tree->undo_history))
      && (sgf_undo_history_check_last_applied_custom_entry_type
	  (game_tree->undo_history, &quarry_text_buffer_undo_entry_data))) {
    undo_entry_data = ((GtkGobanWindowUndoEntryData *)
		       (sgf_undo_history_get_last_applied_custom_entry_data
			(game_tree->undo_history)));

    if (quarry_text_buffer_combine_undo_entries (text_buffer,
						 undo_entry_data->undo_entry,
						 undo_entry)) {
      /* Entries combined.  Return now. */
      return TRUE;
    }
  }

  undo_entry_data = g_malloc (sizeof (GtkGobanWindowUndoEntryData));

  /* Set to NULL so that quarry_text_buffer_redo() is not called this
   * time (it shouldn't.)
   */
  undo_entry_data->goban_window = NULL;
  undo_entry_data->undo_entry   = undo_entry;

  sgf_utils_apply_custom_undo_entry (game_tree,
				     &quarry_text_buffer_undo_entry_data,
				     undo_entry_data, node);

  if (game_tree->undo_history)
    undo_entry_data->goban_window = goban_window;

  return TRUE;
}


static void
text_buffer_undo (GtkGobanWindowUndoEntryData *undo_entry_data)
{
  if (undo_entry_data->goban_window) {
    quarry_text_buffer_undo ((QUARRY_TEXT_BUFFER
			      (undo_entry_data->goban_window->text_buffer)),
			     undo_entry_data->undo_entry);
    undo_entry_data->goban_window->text_buffer_modified = TRUE;
  }
}


static void
text_buffer_redo (GtkGobanWindowUndoEntryData *undo_entry_data)
{
  if (undo_entry_data->goban_window) {
    quarry_text_buffer_redo ((QUARRY_TEXT_BUFFER
			      (undo_entry_data->goban_window->text_buffer)),
			     undo_entry_data->undo_entry);
    undo_entry_data->goban_window->text_buffer_modified = TRUE;
  }
}


static void
text_buffer_set_state_undo (GtkGobanWindowStateData *state_data)
{
  if (state_data->goban_window) {
    GtkTextBuffer *text_buffer = state_data->goban_window->text_buffer;
    GtkTextIter start_iterator;
    GtkTextIter end_iterator;

    gtk_utils_block_signal_handlers (text_buffer,
				     text_buffer_receive_undo_entry);

    gtk_text_buffer_get_bounds (text_buffer, &start_iterator, &end_iterator);
    gtk_text_buffer_delete (text_buffer, &start_iterator, &end_iterator);

    gtk_utils_unblock_signal_handlers (text_buffer,
				       text_buffer_receive_undo_entry);

    quarry_text_buffer_set_state (QUARRY_TEXT_BUFFER (text_buffer),
				  &state_data->state_before);
  }
}


static void
text_buffer_set_state_redo (GtkGobanWindowStateData *state_data)
{
  if (state_data->goban_window) {
    set_comment_and_node_name (state_data->goban_window,
			       state_data->sgf_node_after);

    quarry_text_buffer_set_state
      (QUARRY_TEXT_BUFFER (state_data->goban_window->text_buffer),
       &state_data->state_after);
  }
}


static QuarryTextBufferState *
update_comment_and_node_name_if_needed (GtkGobanWindow *goban_window,
					gboolean for_current_node)
{
  SgfGameTree *current_tree = goban_window->current_tree;
  SgfGameTreeState current_tree_state;
  QuarryTextBufferState *text_buffer_state = NULL;

  if (!current_tree)
    return NULL;

  /* This is needed to suppress node changing signals.  They are not
   * needed here and get in our way when `for_current_node' is FALSE.
   */
  sgf_game_tree_get_state (current_tree, &current_tree_state);
  gtk_sgf_tree_signal_proxy_push_tree_state (current_tree,
					     &current_tree_state);

  /* FIXME: This action is separate from real text activity in the
   *	    buffer and is undone/redone separately too.  It is unclear
   *	    how to fix it now; maybe a ``don't stop on this action''
   *	    flag?
   */

  sgf_utils_begin_action (current_tree);

  if (fetch_comment_and_node_name (goban_window, for_current_node)) {
    GtkGobanWindowStateData *buffer_state
      = g_malloc (sizeof (GtkGobanWindowStateData));
    GtkTextIter start_iterator;
    GtkTextIter end_iterator;

    gtk_text_buffer_get_bounds (goban_window->text_buffer,
				&start_iterator, &end_iterator);
    gtk_text_buffer_begin_user_action (goban_window->text_buffer);
    gtk_text_buffer_delete (goban_window->text_buffer,
			    &start_iterator, &end_iterator);
    gtk_text_buffer_end_user_action (goban_window->text_buffer);

    buffer_state->goban_window = NULL;

    quarry_text_buffer_get_state
      (QUARRY_TEXT_BUFFER (goban_window->text_buffer),
       &buffer_state->state_before);
    buffer_state->sgf_node_after = current_tree->current_node;

    sgf_utils_apply_custom_undo_entry (current_tree,
				       &quarry_text_buffer_state_data,
				       buffer_state, NULL);

    if (current_tree->undo_history) {
      buffer_state->goban_window = goban_window;
      text_buffer_state	         = &buffer_state->state_after;

      sgf_undo_history_hide_last_applied_entry (current_tree->undo_history);
    }
  }

  sgf_utils_end_action (current_tree);

  /* In case we switched to `node' and it is different. */
  sgf_utils_switch_to_given_node (current_tree,
				  current_tree_state.current_node);

  gtk_sgf_tree_signal_proxy_pop_tree_state (current_tree, NULL);

  return text_buffer_state;
}


static gboolean
fetch_comment_and_node_name (GtkGobanWindow *goban_window,
			     gboolean for_current_node)
{
  SgfGameTree *current_tree = goban_window->current_tree;
  SgfNode *node = (for_current_node
		   ? current_tree->current_node
		   : goban_window->last_displayed_node);
  gchar *new_comment;
  gchar *new_node_name = NULL;
  char *normalized_comment;
  char *normalized_node_name;
  GtkTextIter start_iterator;
  GtkTextIter end_iterator;
  gboolean any_changes;

  gtk_text_buffer_get_bounds (goban_window->text_buffer,
			      &start_iterator, &end_iterator);

  if (goban_window->node_name_inserted) {
    GtkTextIter second_line_iterator;

    gtk_text_buffer_get_iter_at_line (goban_window->text_buffer,
				      &second_line_iterator, 1);
    new_comment = gtk_text_iter_get_text (&second_line_iterator,
					  &end_iterator);

    gtk_text_iter_backward_char (&second_line_iterator);
    new_node_name = gtk_text_iter_get_text (&start_iterator,
					    &second_line_iterator);
  }
  else
    new_comment = gtk_text_iter_get_text (&start_iterator, &end_iterator);

  normalized_comment   = sgf_utils_normalize_text (new_comment, 0);
  normalized_node_name = (new_node_name
			  ? sgf_utils_normalize_text (new_node_name, 1)
			  : NULL);

  any_changes
    = (sgf_utils_set_text_property (node, current_tree,
				    SGF_COMMENT, normalized_comment, 0)
       || sgf_utils_set_text_property (node, current_tree,
				       SGF_NODE_NAME, normalized_node_name,
				       0));

  g_free (new_comment);
  g_free (new_node_name);

  return any_changes;
}


static void
set_move_number (GtkGobanWindow *goban_window)
{
  SgfGameTree *game_tree = goban_window->current_tree;
  GtkWidget *dialog = quarry_move_number_dialog_new ();
  QuarryMoveNumberDialog *move_number_dialog
    = QUARRY_MOVE_NUMBER_DIALOG (dialog);
  int move_number;

  gtk_window_set_title (GTK_WINDOW (dialog), _("Set Move Number"));
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
				GTK_WINDOW (goban_window));

  quarry_move_number_dialog_set_sequential_move_number
    (move_number_dialog, sgf_utils_get_sequential_move_number (game_tree));

  if (sgf_node_get_number_property_value (game_tree->current_node,
					  SGF_MOVE_NUMBER, &move_number)) {
    quarry_move_number_dialog_set_specific_move_number (move_number_dialog,
							move_number);
    quarry_move_number_dialog_set_use_sequential_move_number
      (move_number_dialog, FALSE);
  }
  else {
    quarry_move_number_dialog_set_use_sequential_move_number
      (move_number_dialog, TRUE);
  }

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
    int made_changes;

    if (quarry_move_number_dialog_get_use_sequential_move_number
	(move_number_dialog)) {
      made_changes = sgf_utils_delete_property (game_tree->current_node,
						game_tree, SGF_MOVE_NUMBER, 0);
    }
    else {
      move_number = (quarry_move_number_dialog_get_specific_move_number
		     (move_number_dialog));
      made_changes = sgf_utils_set_number_property (game_tree->current_node,
						    game_tree,
						    SGF_MOVE_NUMBER,
						    move_number, 0);
    }

    if (made_changes)
      update_move_information (goban_window);
  }

  gtk_widget_destroy (dialog);
}


static void
set_player_to_move (GtkGobanWindow *goban_window, guint callback_action,
		    GtkCheckMenuItem *menu_item)
{
  if (gtk_check_menu_item_get_active (menu_item)
      && !goban_window->updating_set_player_commands) {
    SgfGameTree *game_tree = goban_window->current_tree;
    SgfNode *node = game_tree->current_node;
    int created_new_node = 0;
    int player_to_move_changed;

    sgf_utils_begin_action (game_tree);

    if (IS_STONE (callback_action)
	&& IS_STONE (game_tree->current_node->move_color)) {
      node = sgf_utils_append_variation (game_tree, EMPTY);
      created_new_node = 1;
    }

    player_to_move_changed = sgf_utils_set_color_property (node, game_tree,
							   SGF_TO_PLAY,
							   callback_action, 0);

    sgf_utils_end_action (game_tree);

    if (created_new_node)
      update_children_for_new_node (goban_window, FALSE, FALSE);
    else if (player_to_move_changed) {
      update_move_information (goban_window);
      update_set_player_to_move_commands (goban_window);
    }
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
  g_assert (successful);

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

    g_assert (game_tree->board_width == game_tree->board_height);
    gtp_client_set_board_size (client,
			       ((GtpClientResponseCallback)
				initialize_gtp_player),
			       goban_window, game_tree->board_width);
    break;

  case INITIALIZATION_BOARD_SIZE_SET:
    *initialization_step = INITIALIZATION_BOARD_CLEARED;
    gtp_client_clear_board (client,
			    (GtpClientResponseCallback) initialize_gtp_player,
			    goban_window);

    break;

  case INITIALIZATION_BOARD_CLEARED:
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
	  if (client_color == BLACK || !goban_window->pending_free_handicap) {
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
	== goban_window->game_position.board_state->color_to_play) {
      generate_move_via_gtp (goban_window);
      start_clock_if_needed (goban_window);
    }

    break;

  default:
    /* Must never happen. */
    g_assert_not_reached ();
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
  g_assert (goban_window->game_position.board_state->color_to_play == WHITE);

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
  /* This may be either the displayed board or an ``off-screen''
   * board.  In either case, it stores the game position.  See
   * move_has_been_generated().
   */
  const Board *board = goban_window->current_tree->board;

  SgfNode *move_node
    = goban_window->game_position.board_state->last_move_node;
  TimeControl *time_control
    = goban_window->time_controls[COLOR_INDEX (move_node->move_color)];
  int color_to_play = goban_window->game_position.board_state->color_to_play;

  if (time_control) {
    time_control_stop (time_control, NULL);
    gtk_clock_time_control_state_changed
      (goban_window->clocks[COLOR_INDEX (move_node->move_color)]);

    time_control_save_state_in_sgf_node (time_control,
					 move_node, goban_window->current_tree,
					 move_node->move_color, 0);
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

  if (!board_is_game_over (board, RULE_SET_DEFAULT, color_to_play)) {
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
      = goban_window->game_position.board_state->game_info_node;

    switch (board->game) {
    case GAME_GO:
      {
	int player;

	goban_window->dead_stones = g_malloc (BOARD_GRID_SIZE * sizeof (char));
	board_fill_grid (board, goban_window->dead_stones, 0);

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

	    /* Note that we skip leave_game_mode() below in this case.
	     * One reason is that we need the game position intact.
	     */
	    return;
	  }
	}

	enter_scoring_mode (goban_window);
      }

      break;

    case GAME_AMAZONS:
      {
	char *result = utils_duplicate_string (move_node->move_color == BLACK
					       ? "B+" : "W+");

	sgf_utils_set_text_property (game_info_node,
				     goban_window->current_tree,
				     SGF_RESULT, result, 1);
      }

      break;

    case GAME_REVERSI:
      {
	int num_black_disks;
	int num_white_disks;

	reversi_count_disks (board, &num_black_disks, &num_white_disks);
	sgf_utils_set_score_result (game_info_node, goban_window->current_tree,
				    num_black_disks - num_white_disks, 1);
      }

      break;

    default:
      g_assert_not_reached ();
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
    g_assert (status == GTP_DEAD);

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
      if (!IS_DISPLAYING_GAME_NODE (goban_window)) {
	switch_to_given_node (goban_window,
			      goban_window->game_position.current_node);
      }

      enter_scoring_mode (goban_window);
    }
    else {
      /* Need to add scoring results to the proper node. */
      if (!IS_DISPLAYING_GAME_NODE (goban_window)) {
	gtk_sgf_tree_signal_proxy_push_tree_state
	  (goban_window->current_tree, &goban_window->game_position);
      }

      handle_go_scoring_results (goban_window);

      if (IS_DISPLAYING_GAME_NODE (goban_window))
	reenter_current_node (goban_window);
      else {
	gtk_sgf_tree_signal_proxy_pop_tree_state (goban_window->current_tree,
						  NULL);
      }
    }

    if (goban_window->dead_stones_list) {
      board_position_list_delete (goban_window->dead_stones_list);
      goban_window->dead_stones_list = NULL;
    }

    leave_game_mode (goban_window);
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

  if (!IS_DISPLAYING_GAME_NODE (goban_window)) {
    switch_to_given_node (goban_window,
			  goban_window->game_position.current_node);
  }

  enter_scoring_mode (goban_window);
  leave_game_mode (goban_window);
}


static void
enter_scoring_mode (GtkGobanWindow *goban_window)
{
  GtkWidget *scoring_tool_menu_item
    = gtk_item_factory_get_widget (goban_window->item_factory,
				   "/Edit/Tools/Scoring Tool");

  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (scoring_tool_menu_item),
				  TRUE);
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

    if (x == RESIGNATION_X && y == RESIGNATION_Y) {
      do_resign_game (goban_window);
      return;
    }

    if (!IS_DISPLAYING_GAME_NODE (goban_window)) {
      gtk_sgf_tree_signal_proxy_push_tree_state (current_tree,
						 &goban_window->game_position);
    }

    /* FIXME: Validate move and alert if it is illegal. */

    if (goban_window->current_tree->game != GAME_AMAZONS)
      sgf_utils_append_variation (current_tree, color, x, y);
    else {
      sgf_utils_append_variation (current_tree,
				  color, x, y, move_data->amazons);
    }

    move_has_been_played (goban_window);

    if (IS_DISPLAYING_GAME_NODE (goban_window))
      update_children_for_new_node (goban_window, TRUE, FALSE);
    else {
      gtk_sgf_tree_signal_proxy_pop_tree_state (current_tree,
						&goban_window->game_position);
      show_sgf_tree_view_automatically
	(goban_window, goban_window->game_position.current_node);
    }
  }
}


static void
generate_move_via_gtp (GtkGobanWindow *goban_window)
{
  int color_to_play = goban_window->game_position.board_state->color_to_play;
  int color_to_play_index = COLOR_INDEX (color_to_play);
  TimeControl *time_control = goban_window->time_controls[color_to_play_index];

  if (time_control) {
    int moves_left;
    double seconds_left = time_control_get_time_left (time_control,
						      &moves_left);

    /* FIXME: OUT_OF_TIME shouldn't really happen here.
     *
     *	      Check if it still happens.  Time control handling has
     *	      been significantly improved since this FIXME was added.
     */
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
    = COLOR_INDEX (goban_window->game_position.board_state->color_to_play);

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

  sgf_utils_set_text_property (game_info_node, goban_window->current_tree,
			       SGF_RESULT,
			       utils_cprintf ("%c+Time", winner_color_char),
			       1);

  sgf_utils_append_variation (goban_window->current_tree, EMPTY);
  update_children_for_new_node (goban_window, TRUE, FALSE);
}



/* Various editing functions. */

static void
undo_operation (GtkGobanWindow *goban_window)
{
  goban_window->text_buffer_modified = FALSE;
  sgf_utils_undo (goban_window->current_tree);

  update_children_for_new_node (goban_window, TRUE,
				goban_window->text_buffer_modified);
}


static void
redo_operation (GtkGobanWindow *goban_window)
{
  goban_window->text_buffer_modified = FALSE;
  sgf_utils_redo (goban_window->current_tree);

  update_children_for_new_node (goban_window, TRUE,
				goban_window->text_buffer_modified);
}


static void
cut_operation (GtkGobanWindow *goban_window)
{
  copy_operation (goban_window);
  delete_current_node (goban_window);
}


static void
copy_operation (GtkGobanWindow *goban_window)
{
  SgfGameTree *sgf_tree = goban_window->current_tree;
  SgfCopyData *sgf_data = g_malloc (sizeof (SgfCopyData));
  static const GtkTargetEntry target = { SGF_MIME_TYPE, 0, 0 };

  sgf_data->sgf = sgf_utils_create_subtree_sgf (sgf_tree,
						sgf_tree->current_node,
						&sgf_data->sgf_length);

  gtk_clipboard_set_with_data (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
			       &target, 1,
			       get_copied_sgf, delete_copied_sgf, sgf_data);
}


static void
paste_operation (GtkGobanWindow *goban_window)
{
  gtk_clipboard_request_contents (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				  gdk_atom_intern (SGF_MIME_TYPE, FALSE),
				  receive_copied_sgf, goban_window);
}


static void
get_copied_sgf (GtkClipboard *clipboard, GtkSelectionData *selection_data,
		guint format, gpointer user_data)
{
  SgfCopyData *sgf_data = (SgfCopyData *) user_data;

  UNUSED (clipboard);
  UNUSED (format);

  gtk_selection_data_set (selection_data, selection_data->target, 8,
			  sgf_data->sgf, sgf_data->sgf_length);
}


static void
delete_copied_sgf (GtkClipboard *clipboard, gpointer user_data)
{
  SgfCopyData *sgf_data = (SgfCopyData *) user_data;

  UNUSED (clipboard);

  utils_free (sgf_data->sgf);
  g_free (sgf_data);
}


static void
receive_copied_sgf (GtkClipboard *clipboard, GtkSelectionData *selection_data,
		    gpointer user_data)
{
  static const gchar *not_clipboard_sgf_message
    = N_("Unable to paste game record fragment from clipboard since it is not "
	 "intended for pasting");
  static const gchar *not_clipboard_sgf_hint
    = N_("Clipboard SGF probably contains more than one game tree or "
	 "not a single child of the root of the only game tree.");

  static const gchar *couldnt_paste_message
    = N_("Unable to paste game record fragment from clipboard");
  static const gchar *couldnt_paste_hint
    = N_("Clipboard game record fragment must be of different game "
	 "or board size.");

  static const gchar *invalid_sgf_message
    = N_("Clipboard contains an invalid game record fragment (SGF)");

  GtkGobanWindow *goban_window = (GtkGobanWindow *) user_data;
  SgfGameTree *sgf_tree = goban_window->current_tree;
  SgfPasteResult result;
  GtkWidget *error_dialog;
  const gchar *message;
  const gchar *hint;

  UNUSED (clipboard);

  if (selection_data->type != gdk_atom_intern (SGF_MIME_TYPE, FALSE))
    return;

  if (selection_data->length <= 0)
    return;

  result = sgf_utils_paste_sgf (sgf_tree, sgf_tree->current_node,
				selection_data->data, selection_data->length);

  switch (result) {
  case SGF_PASTED:
    return;

  case SGF_NOT_CLIPBOARD_SGF:
    message = _(not_clipboard_sgf_message);
    hint    = _(not_clipboard_sgf_hint);
    break;

  case SGF_COULDNT_PASTE:
    message = _(couldnt_paste_message);
    hint    = _(couldnt_paste_hint);
    break;

  case SGF_INVALID_SGF:
    message = _(invalid_sgf_message);
    hint    = NULL;
    break;
  }

  error_dialog = quarry_message_dialog_new (GTK_WINDOW (goban_window),
					    GTK_BUTTONS_OK,
					    GTK_STOCK_DIALOG_ERROR,
					    hint, message);
  gtk_utils_show_and_forget_dialog (GTK_DIALOG (error_dialog));
}


static void
append_empty_variation (GtkGobanWindow *goban_window)
{
  sgf_utils_append_variation (goban_window->current_tree, EMPTY);
  update_children_for_new_node (goban_window, TRUE, FALSE);
}


static void
swap_adjacent_branches (GtkGobanWindow *goban_window,
			guint use_previous_branch)
{
  SgfGameTree *game_tree = goban_window->current_tree;
  SgfNode *swap_with = (use_previous_branch
			? sgf_node_get_previous_node (game_tree->current_node)
			: game_tree->current_node->next);

  sgf_utils_swap_current_node_with (game_tree, swap_with);
  update_commands_sensitivity (goban_window);
}


static void
delete_current_node (GtkGobanWindow *goban_window)
{
  sgf_utils_delete_current_node (goban_window->current_tree);
  update_children_for_new_node (goban_window, TRUE, FALSE);
}


static void
delete_current_node_children (GtkGobanWindow *goban_window)
{
  sgf_utils_delete_current_node_children (goban_window->current_tree);

  update_commands_sensitivity (goban_window);
}


static void
activate_move_tool (GtkGobanWindow *goban_window, guint callback_action,
		    GtkCheckMenuItem *menu_item)
{
  UNUSED (callback_action);

  if (gtk_check_menu_item_get_active (menu_item)) {
    synchronize_tools_menus (goban_window);

    set_goban_signal_handlers (goban_window,
			       G_CALLBACK (playing_mode_pointer_moved),
			       G_CALLBACK (playing_mode_goban_clicked));
  }
}


static void
activate_setup_tool (GtkGobanWindow *goban_window, guint callback_action,
		     GtkCheckMenuItem *menu_item)
{
  UNUSED (callback_action);

  if (gtk_check_menu_item_get_active (menu_item)) {
    synchronize_tools_menus (goban_window);

    set_goban_signal_handlers (goban_window,
			       G_CALLBACK (setup_mode_pointer_moved),
			       G_CALLBACK (setup_mode_goban_clicked));
  }
}


static void
activate_scoring_tool (GtkGobanWindow *goban_window, guint callback_action,
		       GtkCheckMenuItem *menu_item)
{
  UNUSED (callback_action);

  if (gtk_check_menu_item_get_active (menu_item)) {
    synchronize_tools_menus (goban_window);

    if (goban_window->in_game_mode)
      g_assert (goban_window->dead_stones);
    else {
      const SgfNode *current_node = goban_window->current_tree->current_node;
      const BoardPositionList *black_territory
	= sgf_node_get_list_of_point_property_value (current_node,
						     SGF_BLACK_TERRITORY);
      const BoardPositionList *white_territory
	= sgf_node_get_list_of_point_property_value (current_node,
						     SGF_WHITE_TERRITORY);

      g_assert (!goban_window->dead_stones);

      goban_window->dead_stones = g_malloc (BOARD_GRID_SIZE * sizeof (char));
      board_fill_grid (goban_window->board, goban_window->dead_stones, 0);

      go_guess_dead_stones (goban_window->board, goban_window->dead_stones,
			    black_territory, white_territory);
    }

    enter_special_mode (goban_window,
			_("Please select dead stones\nto score the game"),
			go_scoring_mode_done,
			(goban_window->in_game_mode
			 ? NULL : go_scoring_mode_cancel));
    set_goban_signal_handlers (goban_window,
			       G_CALLBACK (go_scoring_mode_pointer_moved),
			       G_CALLBACK (go_scoring_mode_goban_clicked));

    update_territory_markup (goban_window);
    update_commands_sensitivity (goban_window);
  }
  else {
    /* Switching to a different tool, cancel scoring.
     * go_scoring_mode_done() also indirectly calls this function, and
     * in that case we actually leave scoring mode, not cancel it.
     */
    g_free (goban_window->dead_stones);
    goban_window->dead_stones = NULL;

    leave_special_mode (goban_window);
  }
}


static void
activate_markup_tool (GtkGobanWindow *goban_window, gint sgf_markup_type,
		      GtkCheckMenuItem *menu_item)
{
  if (gtk_check_menu_item_get_active (menu_item)) {
    synchronize_tools_menus (goban_window);

    goban_window->sgf_markup_type = sgf_markup_type;
    set_goban_signal_handlers (goban_window,
			       G_CALLBACK (markup_mode_pointer_moved),
			       G_CALLBACK (markup_mode_goban_clicked));
  }
}


static void
activate_label_tool (GtkGobanWindow *goban_window, gint labels_mode,
		     GtkCheckMenuItem *menu_item)
{
  if (gtk_check_menu_item_get_active (menu_item)) {
    if (labels_mode != goban_window->labels_mode) {
      utils_free (goban_window->next_sgf_label);
      goban_window->next_sgf_label = NULL;

      goban_window->labels_mode = labels_mode;

      synchronize_tools_menus (goban_window);

      set_goban_signal_handlers (goban_window,
				 G_CALLBACK (label_mode_pointer_moved),
				 G_CALLBACK (label_mode_goban_clicked));
    }
  }
  else {
    /* Switching to a different tool, cancel label mode. */
    utils_free (goban_window->next_sgf_label);
    goban_window->next_sgf_label = NULL;

    gtk_goban_set_label_feedback (goban_window->goban,
				  NULL_X, NULL_Y, NULL, 0);

    goban_window->labels_mode = GTK_GOBAN_WINDOW_NON_LABELS_MODE;
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
