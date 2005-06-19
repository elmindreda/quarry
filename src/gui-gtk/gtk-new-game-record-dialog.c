/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005 Paul Pogonyshev.                             *
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

/* TODO: Save last created game record parameters in configuration.
 *	 Think of what to do with the handicap and komi, which can be
 *	 either numbers or empty strings.
 */


#include "gtk-new-game-record-dialog.h"

#include "gtk-configuration.h"
#include "gtk-control-center.h"
#include "gtk-freezable-spin-button.h"
#include "gtk-games.h"
#include "gtk-goban-window.h"
#include "gtk-named-vbox.h"
#include "gtk-utils.h"
#include "quarry-stock.h"
#include "sgf.h"
#include "board.h"
#include "game-info.h"

#include <gtk/gtk.h>
#include <assert.h>


static void	gtk_new_game_record_dialog_class_init
		  (GtkNewGameRecordDialogClass *class);
static void	gtk_new_game_record_dialog_init
		  (GtkNewGameRecordDialog *dialog);

static void	gtk_new_game_record_dialog_response (GtkDialog *dialog,
						     gint response_id);


static void	change_rules_notebook_page (GtkRadioButton *radio_button,
					    GtkNotebook *rules_notebook);
static void	set_handicap_adjustment_limit
		  (GtkAdjustment *board_size_adjustment,
		   GtkAdjustment *handicap_adjustment);
static void	set_place_stones_check_button_sensitivity
		  (GtkNewGameRecordDialog *dialog);


GType
gtk_new_game_record_dialog_get_type (void)
{
  static GType new_game_record_dialog_type = 0;

  if (!new_game_record_dialog_type) {
    static GTypeInfo new_game_record_dialog_info = {
      sizeof (GtkNewGameRecordDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_new_game_record_dialog_class_init,
      NULL,
      NULL,
      sizeof (GtkNewGameRecordDialog),
      1,
      (GInstanceInitFunc) gtk_new_game_record_dialog_init,
      NULL
    };

    new_game_record_dialog_type
      = g_type_register_static (GTK_TYPE_DIALOG, "GtkNewGameRecordDialog",
				&new_game_record_dialog_info, 0);
  }

  return new_game_record_dialog_type;
}


static void
gtk_new_game_record_dialog_class_init (GtkNewGameRecordDialogClass *class)
{
  GTK_DIALOG_CLASS (class)->response = gtk_new_game_record_dialog_response;
}


static void
gtk_new_game_record_dialog_init (GtkNewGameRecordDialog *dialog)
{
  GtkWidget *radio_buttons[NUM_SUPPORTED_GAMES];
  GtkWidget *game_named_vbox;
  GtkWidget *rules_notebook;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *white_player_hbox;
  GtkWidget *black_player_hbox;
  GtkWidget *game_name_hbox;
  GtkWidget *game_info_named_vbox;
  GtkWidget *vbox;
  GtkSizeGroup *height_size_group
    = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  GtkSizeGroup *labels_size_group
    = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  GtkSizeGroup *spin_button_size_group
    = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  int k;

  gtk_window_set_title (GTK_WINDOW (dialog), _("New Game Record"));
  gtk_utils_make_window_only_horizontally_resizable (GTK_WINDOW (dialog));

  gtk_utils_create_radio_chain (radio_buttons, game_labels,
				NUM_SUPPORTED_GAMES);
  dialog->game_radio_button_group
    = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_buttons[0]));

  game_named_vbox = gtk_utils_pack_array_in_box (GTK_TYPE_NAMED_VBOX,
						 QUARRY_SPACING_SMALL,
						 radio_buttons,
						 NUM_SUPPORTED_GAMES,
						 GTK_UTILS_FILL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (game_named_vbox), _("Game"));

  rules_notebook = gtk_utils_create_invisible_notebook ();

  for (k = 0; k < NUM_SUPPORTED_GAMES; k++) {
    GtkWidget *spin_button;
    GtkWidget *rules_named_vbox;

    gtk_size_group_add_widget (height_size_group, radio_buttons[k]);

    g_signal_connect (radio_buttons[k], "toggled",
		      G_CALLBACK (change_rules_notebook_page), rules_notebook);

    dialog->board_sizes[k] = gtk_games_create_board_size_adjustment (k, 0);

    spin_button = gtk_utils_create_spin_button (dialog->board_sizes[k],
						0.0, 0, TRUE);
    gtk_size_group_add_widget (spin_button_size_group, spin_button);

    label = gtk_utils_create_mnemonic_label (_("Board _size:"), spin_button);

    hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				  label, 0, spin_button, GTK_UTILS_FILL, NULL);
    gtk_size_group_add_widget (height_size_group, hbox);

    rules_named_vbox = gtk_named_vbox_new (_(game_rules_labels[k]), FALSE,
					   QUARRY_SPACING_SMALL);
    gtk_box_pack_start (GTK_BOX (rules_named_vbox), hbox, FALSE, TRUE, 0);

    if (k == GTK_GAME_GO) {
      GtkAdjustment *handicap_adjustment
	= gtk_games_create_handicap_adjustment (0);
      GtkAdjustment *komi_adjustment
	= gtk_games_create_komi_adjustmet (0.0);
      GtkWidget *place_stones;

      spin_button
	= gtk_utils_create_freezable_spin_button (handicap_adjustment,
						  0.0, 0, TRUE);
      dialog->handicap_spin_button = GTK_FREEZABLE_SPIN_BUTTON (spin_button);
      gtk_utils_freeze_on_empty_input (dialog->handicap_spin_button);
      gtk_freezable_spin_button_freeze
	(GTK_FREEZABLE_SPIN_BUTTON (spin_button), "");

      gtk_size_group_add_widget (spin_button_size_group, spin_button);

      label = gtk_utils_create_mnemonic_label (_("Han_dicap:"), spin_button);

      place_stones = gtk_check_button_new_with_mnemonic (_("_Place stones"));
      dialog->place_stones = GTK_TOGGLE_BUTTON (place_stones);
      gtk_toggle_button_set_active (dialog->place_stones, TRUE);

      set_handicap_adjustment_limit (dialog->board_sizes[k],
				     handicap_adjustment);
      g_signal_connect (dialog->board_sizes[k], "value-changed",
			G_CALLBACK (set_handicap_adjustment_limit),
			handicap_adjustment);

      set_place_stones_check_button_sensitivity (dialog);
      g_signal_connect_swapped (dialog->board_sizes[k], "value-changed",
				(G_CALLBACK
				 (set_place_stones_check_button_sensitivity)),
				dialog);
      g_signal_connect_swapped (handicap_adjustment, "value-changed",
				(G_CALLBACK
				 (set_place_stones_check_button_sensitivity)),
				dialog);

      hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				    label, 0, spin_button, GTK_UTILS_FILL,
				    place_stones, GTK_UTILS_PACK_DEFAULT,
				    NULL);
      gtk_size_group_add_widget (height_size_group, hbox);

      gtk_box_pack_start (GTK_BOX (rules_named_vbox), hbox, FALSE, TRUE, 0);

      spin_button = gtk_utils_create_freezable_spin_button (komi_adjustment,
							    0.0, 1, FALSE);
      dialog->komi_spin_button = GTK_FREEZABLE_SPIN_BUTTON (spin_button);
      gtk_utils_freeze_on_empty_input (dialog->komi_spin_button);
      gtk_freezable_spin_button_freeze
	(GTK_FREEZABLE_SPIN_BUTTON (spin_button), "");

      gtk_size_group_add_widget (spin_button_size_group, spin_button);

      label = gtk_utils_create_mnemonic_label (_("_Komi:"), spin_button);

      hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				    label, 0, spin_button, GTK_UTILS_FILL,
				    NULL);
      gtk_size_group_add_widget (height_size_group, hbox);

      gtk_box_pack_start (GTK_BOX (rules_named_vbox), hbox, FALSE, TRUE, 0);
    }

    gtk_utils_align_left_widgets (GTK_CONTAINER (rules_named_vbox),
				  labels_size_group);

    gtk_notebook_append_page (GTK_NOTEBOOK (rules_notebook),
			      rules_named_vbox, NULL);
  }

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				game_named_vbox, GTK_UTILS_FILL,
				rules_notebook, GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry (NULL, RETURN_ACTIVATES_DEFAULT);
  dialog->white_player = GTK_ENTRY (entry);

  label = gtk_utils_create_mnemonic_label (_("_White player:"), entry);

  white_player_hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
					     label, 0, entry,
					     GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry (NULL, RETURN_ACTIVATES_DEFAULT);
  dialog->black_player = GTK_ENTRY (entry);

  label = gtk_utils_create_mnemonic_label (_("_Black player:"), entry);

  black_player_hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
					     label, 0, entry,
					     GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry (NULL, RETURN_ACTIVATES_DEFAULT);
  dialog->game_name = GTK_ENTRY (entry);

  label = gtk_utils_create_mnemonic_label (_("Game _name:"), entry);

  game_name_hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
					  label, 0, entry,
					  GTK_UTILS_PACK_DEFAULT, NULL);

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				white_player_hbox, GTK_UTILS_FILL,
				black_player_hbox, GTK_UTILS_FILL,
				game_name_hbox, GTK_UTILS_FILL, NULL);
  gtk_utils_align_left_widgets (GTK_CONTAINER (vbox), NULL);

  label = (gtk_utils_create_left_aligned_label
	   (_("You can always change and expand game information later. "
	      "Just choose `Edit/Game Information' menu item or press "
	      "`Info' button on the main toolbar.")));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  game_info_named_vbox = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
						QUARRY_SPACING,
						vbox, GTK_UTILS_FILL,
						label, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (game_info_named_vbox),
				 _("Short Game Information"));

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				hbox, GTK_UTILS_FILL,
				game_info_named_vbox, GTK_UTILS_FILL, NULL);

  gtk_widget_show_all (vbox);
  gtk_utils_standardize_dialog (&dialog->dialog, vbox);

  gtk_dialog_add_buttons (&dialog->dialog,
			  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			  QUARRY_STOCK_CREATE, GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response (&dialog->dialog, GTK_RESPONSE_OK);
}


void
gtk_new_game_record_dialog_present (void)
{
  static GtkWindow *new_game_record_dialog = NULL;

  if (!new_game_record_dialog) {
    new_game_record_dialog
      = GTK_WINDOW (g_object_new (GTK_TYPE_NEW_GAME_RECORD_DIALOG, NULL));

    gtk_control_center_window_created (new_game_record_dialog);
    gtk_utils_null_pointer_on_destroy (&new_game_record_dialog, TRUE);
  }

  gtk_window_present (new_game_record_dialog);
}


static void
gtk_new_game_record_dialog_response (GtkDialog *dialog, gint response_id)
{
  if (response_id == GTK_RESPONSE_OK) {
    GtkNewGameRecordDialog *data = GTK_NEW_GAME_RECORD_DIALOG (dialog);
    gint game_index
      = gtk_utils_get_selected_radio_index (data->game_radio_button_group);
    gint board_size = gtk_adjustment_get_value (data->board_sizes[game_index]);
    const gchar *white_player = gtk_entry_get_text (data->white_player);
    const gchar *black_player = gtk_entry_get_text (data->black_player);
    const gchar *game_name    = gtk_entry_get_text (data->game_name);
    SgfGameTree *sgf_tree;
    SgfNode *root;
    SgfCollection *sgf_collection;
    GtkWidget *goban_window;

    sgf_tree = sgf_game_tree_new_with_root (index_to_game[game_index],
					    board_size, board_size, 1);
    root     = sgf_tree->root;

    if (white_player) {
      sgf_node_add_text_property (root, sgf_tree, SGF_PLAYER_WHITE,
				  utils_duplicate_string (white_player), 0);
    }

    if (black_player) {
      sgf_node_add_text_property (root, sgf_tree, SGF_PLAYER_BLACK,
				  utils_duplicate_string (black_player), 0);
    }

    if (game_name) {
      sgf_node_add_text_property (root, sgf_tree, SGF_GAME_NAME,
				  utils_duplicate_string (game_name), 0);
    }

    if ((gtk_freezable_spin_button_get_freezing_string
	 (data->handicap_spin_button))
	== NULL) {
      gint handicap = (gtk_spin_button_get_value
		       (GTK_SPIN_BUTTON (data->handicap_spin_button)));
      gint place_handicap
	= (GTK_WIDGET_SENSITIVE (data->place_stones)
	   && gtk_toggle_button_get_active (data->place_stones));

      /* Don't bother user with handicap subtleties. */
      if (handicap == 1)
	handicap = 0;

      sgf_utils_set_handicap (sgf_tree, handicap, place_handicap);
    }

    if (gtk_freezable_spin_button_get_freezing_string (data->komi_spin_button)
	== NULL) {
      gdouble komi = gtk_spin_button_get_value (GTK_SPIN_BUTTON
						(data->komi_spin_button));

      sgf_node_add_text_property (root, sgf_tree, SGF_KOMI,
				  utils_cprintf ("%.f", komi), 0);
    }

    sgf_collection = sgf_collection_new ();
    sgf_collection_add_game_tree (sgf_collection, sgf_tree);

    goban_window = gtk_goban_window_new (sgf_collection, NULL);

    gtk_goban_window_enter_game_record_mode (GTK_GOBAN_WINDOW (goban_window));
    gtk_window_present (GTK_WINDOW (goban_window));
  }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void
change_rules_notebook_page (GtkRadioButton *radio_button,
			    GtkNotebook *rules_notebook)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_button))) {
    GSList *radio_button_group = gtk_radio_button_get_group (radio_button);
    gint page_index = gtk_utils_get_selected_radio_index (radio_button_group);

    gtk_notebook_set_current_page (rules_notebook, page_index);
  }
}


static void
set_handicap_adjustment_limit (GtkAdjustment *board_size_adjustment,
			       GtkAdjustment *handicap_adjustment)
{
  gint board_size = gtk_adjustment_get_value (board_size_adjustment);

  gtk_games_set_handicap_adjustment_limits (board_size, board_size,
					    NULL, handicap_adjustment);
}


static void
set_place_stones_check_button_sensitivity (GtkNewGameRecordDialog *dialog)
{
  gint handicap = gtk_spin_button_get_value (GTK_SPIN_BUTTON
					     (dialog->handicap_spin_button));
  gint board_size
    = gtk_adjustment_get_value (dialog->board_sizes[GTK_GAME_GO]);
  gint max_fixed_handicap = go_get_max_fixed_handicap (board_size, board_size);

  gtk_widget_set_sensitive (GTK_WIDGET (dialog->place_stones),
			    2 <= handicap && handicap <= max_fixed_handicap);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
