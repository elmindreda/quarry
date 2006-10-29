/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
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


#include "gtk-resume-game-dialog.h"

#include "gtk-configuration.h"
#include "gtk-control-center.h"
#include "gtk-games.h"
#include "gtk-goban-window.h"
#include "gtk-help.h"
#include "gtk-named-vbox.h"
#include "gtk-parser-interface.h"
#include "gtk-preferences.h"
#include "gtk-utils.h"
#include "quarry-message-dialog.h"
#include "time-control.h"
#include "sgf.h"
#include "board.h"
#include "game-info.h"

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <string.h>


typedef struct _GameRecordData		GameRecordData;
typedef struct _ResumeGameDialogData	ResumeGameDialogData;

struct _GameRecordData {
  SgfCollection	   *sgf_collection;
  gchar		   *filename;
};

struct _ResumeGameDialogData {
  GtkWidget	   *dialog;

  SgfCollection	   *sgf_collection;
  gchar		   *filename;

  GtkEntry	   *name_entries[NUM_COLORS];
  GtkToggleButton  *player_radio_buttons[NUM_COLORS][2];
  GtkWidget	   *engine_selectors[NUM_COLORS];

  GtpClient	   *players[NUM_COLORS];
};


static void	analyze_game_to_be_resumed (SgfCollection *sgf_collection,
					    SgfErrorList *sgf_error_list,
					    const gchar *filename);
static void	maybe_open_game_record (GtkWidget *dialog, gint response_id,
					GameRecordData *data);

static void	gtk_resume_game_dialog_present (SgfCollection *sgf_collection,
						const gchar *filename);

static void	set_computer_opponent_sensitivity (GtkWidget *selector,
						   ResumeGameDialogData *data);

static void	gtk_resume_game_dialog_response (GtkWidget *dialog,
						 gint response_id,
						 ResumeGameDialogData *data);
static void	do_resume_game (GtkEnginesInstantiationStatus status,
				gpointer user_data);


void
gtk_resume_game (void)
{
  static GtkWindow *resume_game_dialog = NULL;

  gtk_parser_interface_present (&resume_game_dialog, _("Resume Game..."),
				analyze_game_to_be_resumed);
}


static void
analyze_game_to_be_resumed (SgfCollection *sgf_collection,
			    SgfErrorList *sgf_error_list,
			    const gchar *filename)
{
  gchar *filename_in_utf8 = g_filename_to_utf8 (filename, -1,
						NULL, NULL, NULL);
  const SgfGameTree *const sgf_tree = sgf_collection->first_tree;
  SgfResult game_result;

  if (sgf_error_list)
    string_list_delete (sgf_error_list);

  if (!GAME_IS_SUPPORTED (sgf_tree->game)) {
    GtkWidget *error_dialog
      = quarry_message_dialog_new (NULL, GTK_BUTTONS_OK,
				   GTK_STOCK_DIALOG_ERROR,
				   NULL,
				   _("The game stored in file `%s' (%s) "
				     "is not supported by Quarry"),
				   filename_in_utf8,
				   _(game_info[sgf_tree->game].name));

    g_free (filename_in_utf8);
    gtk_utils_show_and_forget_dialog (GTK_DIALOG (error_dialog));

    sgf_collection_delete (sgf_collection);

    return;
  }

  game_result = sgf_node_get_result (sgf_tree->root, NULL);

  if (game_result != SGF_RESULT_NOT_SET
      && game_result != SGF_RESULT_VOID
      && game_result != SGF_RESULT_UNKNOWN) {
    GameRecordData *data = g_malloc (sizeof (GameRecordData));
    GtkWidget *error_dialog
      = quarry_message_dialog_new (NULL, GTK_BUTTONS_NONE,
				   GTK_STOCK_DIALOG_ERROR,
				   NULL,
				   _("This game appears to have been "
				     "finished. Open it for viewing "
				     "and/or editing instead?"));

    g_free (filename_in_utf8);

    gtk_dialog_add_buttons (GTK_DIALOG (error_dialog),
			    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			    GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
				     GTK_RESPONSE_OK);

    data->sgf_collection = sgf_collection;
    data->filename	 = g_strdup (filename);
    g_signal_connect (error_dialog, "response",
		      G_CALLBACK (maybe_open_game_record), data);

    gtk_window_present (GTK_WINDOW (error_dialog));

    return;
  }

  gtk_resume_game_dialog_present (sgf_collection, filename);
}


static void
maybe_open_game_record (GtkWidget *dialog, gint response_id,
			GameRecordData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    GtkWidget *goban_window = gtk_goban_window_new (data->sgf_collection,
						    data->filename);

    gtk_window_present (GTK_WINDOW (goban_window));
  }
  else
    sgf_collection_delete (data->sgf_collection);

  gtk_widget_destroy (dialog);

  g_free (data->filename);
  g_free (data);
}


/* FIXME: Show information about game time control and remaining time
 *	  for both players.
 */
static void
gtk_resume_game_dialog_present (SgfCollection *sgf_collection,
				const gchar *filename)
{
  static gchar buffer[64];
  gchar *filename_in_utf8 = g_filename_to_utf8 (filename, -1,
						NULL, NULL, NULL);
  gchar *base_name = g_path_get_basename (filename_in_utf8);
  const SgfGameTree *const sgf_tree = sgf_collection->first_tree;
  GtkGameIndex game_index = gtk_games_get_game_index (sgf_tree->game);
  ResumeGameDialogData *data = g_malloc (sizeof (ResumeGameDialogData));
  GtkWidget *dialog;
  GtkWidget *game_information_vbox;
  GtkBox *game_information_box;
  GtkWidget *field_name_label;
  GtkWidget *field_value_label;
  GtkWidget *hbox1;
  GtkWidget *hbox2;
  GtkWidget *vbox;
  GtkWidget *player_vboxes[NUM_COLORS];
  int k;

  data->sgf_collection = sgf_collection;
  data->filename       = g_strdup (filename);

  dialog = gtk_dialog_new_with_buttons (_("Resuming Game"), NULL, 0,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					_("_Resume"), GTK_RESPONSE_OK,
					GTK_STOCK_HELP, GTK_RESPONSE_HELP,
					NULL);
  data->dialog = dialog;

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_utils_make_window_only_horizontally_resizable (GTK_WINDOW (dialog));
  gtk_control_center_window_created (GTK_WINDOW (dialog));

  g_signal_connect (dialog, "response",
		    G_CALLBACK (gtk_resume_game_dialog_response), data);
  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (gtk_control_center_window_destroyed), NULL);

  game_information_vbox = gtk_named_vbox_new (_("Short Game Information"),
					      FALSE, QUARRY_SPACING_SMALL);
  game_information_box = GTK_BOX (game_information_vbox);

  field_name_label  = gtk_utils_create_left_aligned_label (_("Game:"));
  field_value_label = (gtk_utils_create_left_aligned_label
		       (gtk_games_get_capitalized_name (sgf_tree->game)));

  hbox1 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				 field_name_label, GTK_UTILS_FILL,
				 field_value_label, GTK_UTILS_FILL, NULL);

  sprintf (buffer, "%d", sgf_tree->board_width);
  field_name_label  = gtk_utils_create_left_aligned_label (_("Board size:"));
  field_value_label = gtk_utils_create_left_aligned_label (buffer);

  hbox2 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				 field_name_label, GTK_UTILS_FILL,
				 field_value_label, GTK_UTILS_FILL, NULL);

  if (sgf_tree->game == GAME_GO) {
    GtkWidget *vbox2;
    const gchar *handicap = sgf_node_get_text_property_value (sgf_tree->root,
							      SGF_HANDICAP);
    const gchar *komi = sgf_node_get_text_property_value (sgf_tree->root,
							  SGF_KOMI);

    vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				  hbox1, GTK_UTILS_FILL, hbox2, GTK_UTILS_FILL,
				  NULL);

    field_name_label  = gtk_utils_create_left_aligned_label (_("Handicap:"));
    field_value_label
      = gtk_utils_create_left_aligned_label (handicap
					     ? handicap
					     : Q_("handicap|Not set"));

    hbox1 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				   field_name_label, GTK_UTILS_FILL,
				   field_value_label, GTK_UTILS_FILL, NULL);

    field_name_label  = gtk_utils_create_left_aligned_label (_("Komi:"));
    field_value_label
      = gtk_utils_create_left_aligned_label (komi
					     ? komi : Q_("komi|Not set"));

    hbox2 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				   field_name_label, GTK_UTILS_FILL,
				   field_value_label, GTK_UTILS_FILL, NULL);

    vbox2 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				   hbox1, GTK_UTILS_FILL,
				   hbox2, GTK_UTILS_FILL, NULL);

    hbox1 = gtk_utils_pack_in_box (GTK_TYPE_HBOX,
				   QUARRY_SPACING_VERY_BIG,
				   vbox, GTK_UTILS_FILL,
				   vbox2, GTK_UTILS_FILL, NULL);
    gtk_box_set_homogeneous (GTK_BOX (hbox1), TRUE);

    gtk_box_pack_start (game_information_box, hbox1, FALSE, TRUE, 0);
  }
  else {
    gtk_box_pack_start (game_information_box, hbox1, FALSE, TRUE, 0);
    gtk_box_pack_start (game_information_box, hbox2, FALSE, TRUE, 0);
  }

  field_name_label  = gtk_utils_create_left_aligned_label (_("Filename:"));
  field_value_label = gtk_utils_create_left_aligned_label (base_name);
  g_free (base_name);
  g_free (filename_in_utf8);

  hbox1 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				 field_name_label, GTK_UTILS_FILL,
				 field_value_label, GTK_UTILS_FILL, NULL);
  gtk_box_pack_start (game_information_box, hbox1, FALSE, TRUE, 0);

  for (k = 0; k < NUM_COLORS; k++) {
    /* Same as in New Game dialog. */
    static const char *radio_labels[NUM_COLORS][2]
      = { { N_("Hu_man"), N_("Compu_ter") },
	  { N_("H_uman"), N_("Com_puter") } };
    GtkWidget **radio_buttons = (GtkWidget **) data->player_radio_buttons[k];
    GtkWidget *entry;
    GtkWidget *hbox3;
    const gchar *player_name
      = sgf_node_get_text_property_value (sgf_tree->root,
					  (k == WHITE_INDEX
					   ? SGF_PLAYER_WHITE
					   : SGF_PLAYER_BLACK));
    const GtpEngineListItem *engine_data
      = gtk_preferences_guess_engine_by_name (player_name, game_index);

    entry = gtk_utils_create_entry (player_name, RETURN_ACTIVATES_DEFAULT);
    data->name_entries[k] = GTK_ENTRY (entry);

    field_name_label = gtk_utils_create_mnemonic_label (k == WHITE_INDEX
							? _("_Name:")
							: _("N_ame:"),
							entry);

    hbox1 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				   field_name_label, GTK_UTILS_FILL,
				   entry, GTK_UTILS_PACK_DEFAULT, NULL);

    gtk_utils_create_radio_chain (radio_buttons, radio_labels[k], 2);

    hbox2 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				   radio_buttons[0], 0, NULL);

    data->engine_selectors[k]
      = (gtk_preferences_create_engine_selector
	 (game_index, TRUE, engine_data,
	  (GtkEngineChanged) set_computer_opponent_sensitivity, data));

    if (engine_data)
      gtk_toggle_button_set_active (data->player_radio_buttons[k][1], TRUE);

    gtk_utils_set_sensitive_on_toggle (data->player_radio_buttons[k][1],
				       data->engine_selectors[k], FALSE);

    hbox3 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				   radio_buttons[1], GTK_UTILS_FILL,
				   data->engine_selectors[k],
				   GTK_UTILS_PACK_DEFAULT,
				   NULL);

    vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				  hbox2, GTK_UTILS_FILL,
				  hbox3, GTK_UTILS_FILL, NULL);

    player_vboxes[k] = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
					      QUARRY_SPACING,
					      hbox1, GTK_UTILS_FILL,
					      vbox, GTK_UTILS_FILL);
    gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (player_vboxes[k]),
				   (k == BLACK_INDEX
				    ? _("Black Player") : _("White Player")));
  }

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				game_information_vbox, GTK_UTILS_FILL,
				player_vboxes[WHITE_INDEX], GTK_UTILS_FILL,
				player_vboxes[BLACK_INDEX], GTK_UTILS_FILL,
				NULL);
  gtk_utils_align_left_widgets (GTK_CONTAINER (vbox), NULL);

  gtk_widget_show_all (vbox);
  gtk_utils_standardize_dialog (GTK_DIALOG (dialog), vbox);

  gtk_window_present (GTK_WINDOW (dialog));
}


static void
set_computer_opponent_sensitivity (GtkWidget *selector,
				   ResumeGameDialogData *data)
{
  UNUSED (selector);

  if (gtk_preferences_have_non_hidden_gtp_engine ()) {
    gtk_widget_set_sensitive
      (GTK_WIDGET (data->player_radio_buttons[BLACK_INDEX][1]), TRUE);
    gtk_widget_set_sensitive
      (GTK_WIDGET (data->player_radio_buttons[WHITE_INDEX][1]), TRUE);
  }
  else {
    gtk_toggle_button_set_active (data->player_radio_buttons[BLACK_INDEX][0],
				  TRUE);
    gtk_toggle_button_set_active (data->player_radio_buttons[WHITE_INDEX][0],
				  TRUE);

    gtk_widget_set_sensitive
      (GTK_WIDGET (data->player_radio_buttons[BLACK_INDEX][1]), FALSE);
    gtk_widget_set_sensitive
      (GTK_WIDGET (data->player_radio_buttons[WHITE_INDEX][1]), FALSE);
  }
}


static void
gtk_resume_game_dialog_response (GtkWidget *dialog, gint response_id,
				 ResumeGameDialogData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    int k;
    GtkWindow *window = GTK_WINDOW (dialog);
    GtkEngineChain *engine_chain
      = gtk_preferences_create_engines_instantiation_chain (window,
							    do_resume_game,
							    data);

    for (k = 0; k < NUM_COLORS; k++) {
      if (GTK_WIDGET_IS_SENSITIVE (data->engine_selectors[k])) {
	gtk_preferences_instantiate_selected_engine (engine_chain,
						     data->engine_selectors[k],
						     &data->players[k]);
      }
      else
	data->players[k] = NULL;
    }

    gtk_preferences_do_instantiate_engines (engine_chain);
  }
  else if (response_id == GTK_RESPONSE_HELP)
    gtk_help_display ("resuming-game-dialog");
  else {
    sgf_collection_delete (data->sgf_collection);

    g_free (data->filename);
    g_free (data);

    gtk_widget_destroy (dialog);
  }
}


static void
do_resume_game (GtkEnginesInstantiationStatus status, gpointer user_data)
{
  ResumeGameDialogData *data;
  GtkWidget *goban_window;

  if (status != ENGINES_INSTANTIATED)
    return;

  data = (ResumeGameDialogData *) user_data;

  goban_window = gtk_goban_window_new (data->sgf_collection, data->filename);
  gtk_goban_window_resume_game (GTK_GOBAN_WINDOW (goban_window),
				data->players[BLACK_INDEX],
				data->players[WHITE_INDEX]);

  gtk_window_present (GTK_WINDOW (goban_window));

  gtk_widget_destroy (data->dialog);

  g_free (data->filename);
  g_free (data);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
