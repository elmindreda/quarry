/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005, 2006 Paul Pogonyshev.           *
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


#include "gtk-new-game-dialog.h"

#include "gtk-assistant.h"
#include "gtk-configuration.h"
#include "gtk-control-center.h"
#include "gtk-games.h"
#include "gtk-goban-window.h"
#include "gtk-gtp-client-interface.h"
#include "gtk-named-vbox.h"
#include "gtk-preferences.h"
#include "gtk-utils.h"
#include "quarry-stock.h"
#include "time-control.h"
#include "sgf.h"
#include "board.h"
#include "game-info.h"

#include <gtk/gtk.h>


static void	      gtk_new_game_dialog_init (GtkNewGameDialog *dialog);


static void	      update_game_and_players_page (GtkWidget *widget,
						    gpointer user_data);
static void	      swap_players (GtkNewGameDialog *dialog);
static GtkGameIndex   get_selected_game (GtkNewGameDialog *dialog);

static void	      show_game_specific_rules (GtkNewGameDialog *dialog);
static void	      set_handicap_adjustment_limits
			(GtkAdjustment *board_size_adjustment,
			 GtkNewGameDialog *dialog);

static void	      time_control_type_changed (GtkWidget *selector,
						 GtkNotebook *notebook);

static const gchar *  get_game_rules_help_link_id (GtkNewGameDialog *dialog);

static gboolean	      instantiate_players (GtkNewGameDialog *dialog);
static void	      begin_game (GtkEnginesInstantiationStatus status,
				  gpointer user_data);


GType
gtk_new_game_dialog_get_type (void)
{
  static GType new_game_dialog_type = 0;

  if (!new_game_dialog_type) {
    static GTypeInfo new_game_dialog_info = {
      sizeof (GtkNewGameDialogClass),
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      sizeof (GtkNewGameDialog),
      1,
      (GInstanceInitFunc) gtk_new_game_dialog_init,
      NULL
    };

    new_game_dialog_type = g_type_register_static (GTK_TYPE_ASSISTANT,
						   "GtkNewGameDialog",
						   &new_game_dialog_info, 0);
  }

  return new_game_dialog_type;
}


static void
gtk_new_game_dialog_init (GtkNewGameDialog *dialog)
{
  static const gchar *hint_text
    = N_("Note that most GTP engines support only one game. Hence certain "
	 "game / computer player combinations may be not possible to play.");

  static const gchar *handicap_radio_button_labels[2]
    = { N_("Fi_xed handicap:"), N_("_Free handicap:") };

  static const gchar *time_control_types[]
    = { N_("No limit"), N_("Limited time for entire game"),
	N_("Limited time per move"), N_("Canadian overtime") };

  gint game_index = gtk_games_name_to_index (new_game_configuration.game_name,
					     FALSE);
  GtkWidget *vbox1;
  GtkWidget **radio_buttons;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *named_vbox;
  GtkWidget *hboxes[3];
  GtkWidget *player_vboxes[NUM_COLORS];
  GtkWidget *button;
  GtkWidget *vbox2;
  GtkWidget *game_and_players_page;
  GtkWidget *games_notebook;
  GtkRadioButton *last_radio_button = NULL;
  int k;

  gtk_window_set_title (GTK_WINDOW (dialog), _("New Game"));
  gtk_utils_make_window_only_horizontally_resizable (GTK_WINDOW (dialog));

  gtk_assistant_set_user_data (&dialog->assistant, dialog);
  gtk_assistant_set_finish_button (&dialog->assistant, QUARRY_STOCK_PLAY);

  /* "Game & Players" page. */

  /* Vertical box for game radio buttons. */
  vbox1 = gtk_vbox_new (FALSE, QUARRY_SPACING_SMALL);

  /* Create a radio button for each of the supported games. */
  for (k = 0; k < NUM_SUPPORTED_GAMES; k++) {
    GtkWidget *radio_button;

    label = gtk_label_new_with_mnemonic (_(game_labels[k]));
    dialog->game_supported_icons[k]
      = gtk_image_new_from_stock (GTK_STOCK_YES, GTK_ICON_SIZE_MENU);

    hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING_SMALL,
				  label, GTK_UTILS_PACK_DEFAULT,
				  dialog->game_supported_icons[k], 0, NULL);

    radio_button = gtk_radio_button_new_from_widget (last_radio_button);
    last_radio_button = GTK_RADIO_BUTTON (radio_button);
    dialog->game_radio_buttons[k] = GTK_TOGGLE_BUTTON (radio_button);
    gtk_container_add (GTK_CONTAINER (radio_button), hbox);
    gtk_box_pack_start (GTK_BOX (vbox1), radio_button, FALSE, TRUE, 0);

    g_signal_connect (radio_button, "toggled",
		      G_CALLBACK (update_game_and_players_page), dialog);
  }

  /* Hint about engine/game (non-)compatibility. */
  label = gtk_utils_create_left_aligned_label (_(hint_text));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  /* Pack the radio buttons and the hint next to each other. */
  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				vbox1, GTK_UTILS_FILL,
				label, GTK_UTILS_PACK_DEFAULT, NULL);

  /* And pack everything into a box with caption saying "Game". */
  named_vbox = gtk_named_vbox_new (_("Game"), FALSE, QUARRY_SPACING_SMALL);
  gtk_box_pack_start_defaults (GTK_BOX (named_vbox), hbox);

  /* Create identical set of controls for each of the players. */
  for (k = 0; k < NUM_COLORS; k++) {
    static const char *radio_labels[NUM_COLORS][2]
      = { { N_("Hu_man"), N_("Com_puter") },
	  { N_("H_uman"), N_("C_omputer") } };

    GtkWidget *entry;
    const GtpEngineListItem *engine_data
      = (gtk_preferences_guess_engine_by_name
	 (new_game_configuration.engine_names[k], GTK_GAME_ANY));

    /* Two radio buttons for selecting who is controlling this
     * player--human or computer.
     */
    radio_buttons = (GtkWidget **) dialog->player_radio_buttons[k];
    gtk_utils_create_radio_chain (radio_buttons, radio_labels[k], 2);
    if (new_game_configuration.player_is_computer[k])
      gtk_toggle_button_set_active (dialog->player_radio_buttons[k][1], TRUE);

    /* Text entry for human's name. */
    entry = gtk_utils_create_entry (new_game_configuration.player_names[k],
				    RETURN_ACTIVATES_DEFAULT);
    dialog->human_name_entries[k] = GTK_ENTRY (entry);
    gtk_utils_set_sensitive_on_toggle (dialog->player_radio_buttons[k][0],
				       entry, FALSE);

    /* Pack it together with corresponding radio button. */
    hboxes[0] = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				       radio_buttons[0], 0,
				       entry, GTK_UTILS_PACK_DEFAULT, NULL);

    /* Selector (combo box/option menu) for computer player. */
    dialog->engine_selectors[k]
      = gtk_preferences_create_engine_selector (GTK_GAME_GO, FALSE,
						engine_data,
						update_game_and_players_page,
						dialog);
    gtk_utils_set_sensitive_on_toggle (dialog->player_radio_buttons[k][1],
				       dialog->engine_selectors[k], FALSE);
    g_signal_connect (radio_buttons[1], "toggled",
		      G_CALLBACK (update_game_and_players_page), dialog);

    /* Horizontal box with a radio button and the selector. */
    hboxes[1] = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				       radio_buttons[1], 0,
				       dialog->engine_selectors[k],
				       GTK_UTILS_PACK_DEFAULT,
				       NULL);

    /* And pack everything into a box with corresponding caption. */
    player_vboxes[k] = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
					      QUARRY_SPACING_SMALL,
					      hboxes[0], GTK_UTILS_FILL,
					      hboxes[1], GTK_UTILS_FILL, NULL);
    gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (player_vboxes[k]),
				   (k == BLACK_INDEX
				    ? _("Black Player") : _("White Player")));
  }

  /* Make sure the radio buttons line up nicely. */
  gtk_utils_create_size_group (GTK_SIZE_GROUP_HORIZONTAL,
			       dialog->player_radio_buttons[WHITE_INDEX][0],
			       dialog->player_radio_buttons[WHITE_INDEX][1],
			       dialog->player_radio_buttons[BLACK_INDEX][0],
			       dialog->player_radio_buttons[BLACK_INDEX][1],
			       vbox1, NULL);

  /* Button which opens "Preferences" dialog at "GTP Engines" page. */
  button = gtk_button_new_with_mnemonic (_("Manage _Engine List"));
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_preferences_dialog_present),
			    GINT_TO_POINTER (PREFERENCES_PAGE_GTP_ENGINES));

  /* Pack boxes for the two players and the button vertically.  In
   * GUI, White always comes before Black.
   */
  vbox1 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				 player_vboxes[WHITE_INDEX], GTK_UTILS_FILL,
				 player_vboxes[BLACK_INDEX], GTK_UTILS_FILL,
				 gtk_utils_align_widget (button, 1.0, 0.5),
				 GTK_UTILS_FILL,
				 NULL);

  /* Top padding for the "Swap" button below. */
  label = gtk_label_new (NULL);
  gtk_utils_create_size_group (GTK_SIZE_GROUP_VERTICAL, button, label, NULL);

  /* A button that allows to quickly swap colors of the players. */
  button = gtk_button_new_with_mnemonic (_("_Swap"));
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (swap_players), dialog);

  /* Pack the button into a box with top and bottom padding so it ends
   * up just between two players' control sets.
   */
  vbox2 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				 gtk_label_new (NULL), 0,
				 button, GTK_UTILS_EXPAND,
				 label, (QUARRY_SPACING_BIG
					 - QUARRY_SPACING_SMALL) / 2,
				 NULL);

  /* Pack players' control sets and the "Swap" button box just to the
   * right of them.
   */
  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				vbox1, GTK_UTILS_PACK_DEFAULT,
				vbox2, GTK_UTILS_FILL, NULL);

  /* Finally pack everything into a vertical box and add it as the
   * assistant dialog page.
   */
  game_and_players_page
    = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
			     named_vbox, GTK_UTILS_FILL,
			     hbox, GTK_UTILS_FILL, NULL);
  gtk_assistant_add_page (&dialog->assistant, game_and_players_page,
			  GTK_STOCK_REFRESH, _("Game & Players"), NULL, NULL);
  gtk_assistant_set_page_help_link_id (&dialog->assistant,
				       game_and_players_page,
				       "new-game-dialog-game-and-players");
  gtk_widget_show_all (game_and_players_page);

  if (game_index != GTK_GAME_UNSUPPORTED) {
    gtk_toggle_button_set_active (dialog->game_radio_buttons[game_index],
				  TRUE);
  }

  update_game_and_players_page (NULL, dialog);

  /* "Game Rules" page. */

  /* Notebook with a page for each game. */
  games_notebook = gtk_utils_create_invisible_notebook ();
  dialog->games_notebook = GTK_NOTEBOOK (games_notebook);

  for (k = 0; k < NUM_SUPPORTED_GAMES; k++) {
    NewGameDialogTimeControlData *const time_control_data
      = dialog->time_control_data + k;
    const TimeControlConfiguration *time_control_configuration = NULL;
    gint initial_board_size = 0;

    GtkWidget *rules_vbox_widget;
    GtkBox *rules_vbox;
    GtkWidget *board_size_spin_button;
    GtkWidget *time_control_type;
    GtkWidget *time_control_notebook;
    GtkWidget *check_button;
    GtkWidget *game_time_limit_spin_button;
    GtkWidget *move_time_limit_spin_button;
    GtkWidget *main_time_spin_button;
    GtkWidget *overtime_period_spin_button;
    GtkWidget *moves_per_overtime_spin_button;
    GtkWidget *time_control_named_vbox;
    GtkSizeGroup *spin_buttons_size_group;

    if (k == GTK_GAME_GO) {
      time_control_configuration = &new_go_game_configuration.time_control;
      initial_board_size	 = new_go_game_configuration.board_size;
    }
    else if (k == GTK_GAME_AMAZONS) {
      time_control_configuration
	= &new_amazons_game_configuration.time_control;
      initial_board_size = new_amazons_game_configuration.board_size;
    }
    else if (k == GTK_GAME_REVERSI) {
      time_control_configuration
	= &new_reversi_game_configuration.time_control;
      initial_board_size = new_reversi_game_configuration.board_size;
    }

    /* "Game Rules" named vertical box. */
    /* FIXME: i18n. */
    rules_vbox_widget = gtk_named_vbox_new (_(game_rules_labels[k]), FALSE,
					    QUARRY_SPACING_SMALL);
    rules_vbox = GTK_BOX (rules_vbox_widget);

    /* Board size spin button and a label for it. */
    dialog->board_sizes[k] =
      gtk_games_create_board_size_adjustment (k, initial_board_size);
    board_size_spin_button
      = gtk_utils_create_spin_button (dialog->board_sizes[k], 0.0, 0, TRUE);

    label = gtk_utils_create_mnemonic_label (_("Board _size:"),
					     board_size_spin_button);

    /* Pack them into a box and add to "Game Rules" box. */
    hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				  label, 0,
				  board_size_spin_button, GTK_UTILS_FILL,
				  NULL);
    gtk_box_pack_start (rules_vbox, hbox, FALSE, TRUE, 0);

    /* A combo box (option menu for pre-2.4) for selecting time
     * control type and a label for it..
     */
    time_control_type = gtk_utils_create_selector (time_control_types,
						   (sizeof time_control_types
						    / sizeof (const gchar *)),
						   -1);
    label = gtk_utils_create_mnemonic_label (_("Time control _type:"),
					     time_control_type);

    /* Pack the selector and the label together. */
    hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				  label, 0,
				  time_control_type, GTK_UTILS_FILL, NULL);

    /* A notebook with a page for each type of time control. */
    time_control_notebook = gtk_utils_create_invisible_notebook ();
    time_control_data->notebook = GTK_NOTEBOOK (time_control_notebook);

    g_signal_connect (time_control_type, "changed",
		      G_CALLBACK (time_control_type_changed),
		      time_control_notebook);

    /* Pack time control widgets together in a named box. */
    time_control_named_vbox
      = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX, QUARRY_SPACING,
			       hbox, GTK_UTILS_FILL,
			       time_control_notebook, GTK_UTILS_FILL, NULL);
    gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (time_control_named_vbox),
				   _("Time Limit"));

    check_button
      = gtk_check_button_new_with_mnemonic (_("K_eep track of total time"));
    time_control_data->track_total_time_button
      = GTK_TOGGLE_BUTTON (check_button);
    if (time_control_configuration->track_total_time) {
      gtk_toggle_button_set_active (time_control_data->track_total_time_button,
				    TRUE);
    }

    gtk_notebook_append_page (time_control_data->notebook,
			      gtk_utils_align_widget (check_button, 0.0, 0.0),
			      NULL);

    time_control_data->game_time_limit
      = ((GtkAdjustment *)
	 gtk_adjustment_new (time_control_configuration->game_time_limit,
			     1.0, 3600000.0 - 1.0, 60.0, 300.0, 0.0));

    game_time_limit_spin_button
      = gtk_utils_create_time_spin_button (time_control_data->game_time_limit,
					   60.0);
    label = gtk_utils_create_mnemonic_label (_("Time _limit for game:"),
					     game_time_limit_spin_button);

    hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				  label, GTK_UTILS_FILL,
				  game_time_limit_spin_button, GTK_UTILS_FILL,
				  gtk_label_new (_("(per player)")),
				  GTK_UTILS_FILL,
				  NULL);
    gtk_notebook_append_page (time_control_data->notebook,
			      gtk_utils_align_widget (hbox, 0.0, 0.0), NULL);

    time_control_data->move_time_limit
      = ((GtkAdjustment *)
	 gtk_adjustment_new (time_control_configuration->move_time_limit,
			     1.0, 360000.0 - 1.0, 5.0, 30.0, 0.0));

    move_time_limit_spin_button
      = gtk_utils_create_time_spin_button (time_control_data->move_time_limit,
					   5.0);
    label = gtk_utils_create_mnemonic_label (_("Time _limit for move:"),
					     move_time_limit_spin_button);

    hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				  label, GTK_UTILS_FILL,
				  move_time_limit_spin_button, GTK_UTILS_FILL,
				  NULL);
    gtk_notebook_append_page (time_control_data->notebook,
			      gtk_utils_align_widget (hbox, 0.0, 0.0), NULL);

    time_control_data->main_time
      = ((GtkAdjustment *)
	 gtk_adjustment_new (time_control_configuration->main_time,
			     0.0, 3600000.0 - 1.0, 60.0, 300.0, 0.0));

    main_time_spin_button
      = gtk_utils_create_time_spin_button (time_control_data->main_time, 60.0);
    label = gtk_utils_create_mnemonic_label (_("_Main time:"),
					     main_time_spin_button);

    hboxes[0] = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				       label, GTK_UTILS_FILL,
				       main_time_spin_button, GTK_UTILS_FILL,
				       NULL);

    time_control_data->overtime_period
      = ((GtkAdjustment *)
	 gtk_adjustment_new
	 (time_control_configuration->overtime_period_length,
	  1.0, 3600000.0 - 1.0, 60.0, 300.0, 0.0));

    overtime_period_spin_button
      = gtk_utils_create_time_spin_button (time_control_data->overtime_period,
					   60.0);
    label = gtk_utils_create_mnemonic_label (_("_Overtime period length:"),
					     overtime_period_spin_button);

    hboxes[1] = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				       label, GTK_UTILS_FILL,
				       overtime_period_spin_button,
				       GTK_UTILS_FILL,
				       NULL);

    time_control_data->moves_per_overtime
      = ((GtkAdjustment *)
	 gtk_adjustment_new (time_control_configuration->moves_per_overtime,
			     1.0, 999.0, 1.0, 5.0, 0.0));

    moves_per_overtime_spin_button
      = gtk_utils_create_spin_button (time_control_data->moves_per_overtime,
				      0.0, 0, TRUE);
    label = gtk_utils_create_mnemonic_label (_("Mo_ves per overtime:"),
					     moves_per_overtime_spin_button);

    hboxes[2] = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				       label, GTK_UTILS_FILL,
				       moves_per_overtime_spin_button,
				       GTK_UTILS_FILL,
				       NULL);

    vbox1 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				   hboxes[0], GTK_UTILS_FILL,
				   hboxes[1], GTK_UTILS_FILL,
				   hboxes[2], GTK_UTILS_FILL, NULL);
    gtk_notebook_append_page (time_control_data->notebook, vbox1, NULL);

    /* Select the last used time control type. */
    if (0 <= time_control_configuration->type
	&& time_control_configuration->type <= (sizeof time_control_types
						/ sizeof (const gchar *))) {
      gtk_widget_show_all (time_control_notebook);
      gtk_utils_set_selector_active_item_index (time_control_type,
						(time_control_configuration
						 ->type));
    }

    /* Align spin buttons nicely. */
    spin_buttons_size_group
      = gtk_utils_create_size_group (GTK_SIZE_GROUP_HORIZONTAL,
				     board_size_spin_button,
				     game_time_limit_spin_button,
				     move_time_limit_spin_button,
				     main_time_spin_button,
				     overtime_period_spin_button,
				     moves_per_overtime_spin_button, NULL);

    if (k == GTK_GAME_GO) {
      GtkWidget *handicap_spin_buttons[2];
      GtkWidget *komi_spin_button;
      int i;

      /* Two radio buttons for choosing fixed (Japanese) or free
       * (Chinese) handicaps.
       */
      radio_buttons = (GtkWidget **) dialog->handicap_toggle_buttons;
      gtk_utils_create_radio_chain (radio_buttons,
				    handicap_radio_button_labels, 2);
      if (!new_go_game_configuration.handicap_is_fixed) {
	gtk_toggle_button_set_active (dialog->handicap_toggle_buttons[1],
				      TRUE);
      }

      /* Handicap spin buttons and boxes to pack them together with
       * the radio buttons.
       */
      for (i = 0; i < 2; i++) {
	dialog->handicaps[i] = (gtk_games_create_handicap_adjustment
				(i == 0
				 ? new_go_game_configuration.fixed_handicap
				 : new_go_game_configuration.free_handicap));

	handicap_spin_buttons[i]
	  = gtk_utils_create_spin_button (dialog->handicaps[i], 0.0, 0, TRUE);
	gtk_utils_set_sensitive_on_toggle (dialog->handicap_toggle_buttons[i],
					   handicap_spin_buttons[i], FALSE);

	hboxes[i] = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
					   radio_buttons[i], 0,
					   handicap_spin_buttons[i],
					   GTK_UTILS_FILL,
					   NULL);

	/* Force proper alignment of handicap spin buttons. */
	gtk_size_group_add_widget (spin_buttons_size_group,
				   handicap_spin_buttons[i]);
      }

      /* Pack all handicap controls in a vertical box and add it to
       * the rules box with a small padding.
       */
      vbox1 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				     hboxes[0], GTK_UTILS_FILL,
				     hboxes[1], GTK_UTILS_FILL, NULL);
      gtk_box_pack_start (rules_vbox, vbox1,
			  FALSE, TRUE, QUARRY_SPACING_SMALL);

      set_handicap_adjustment_limits (dialog->board_sizes[GTK_GAME_GO],
				      dialog);
      g_signal_connect (dialog->board_sizes[GTK_GAME_GO], "value-changed",
			G_CALLBACK (set_handicap_adjustment_limits), dialog);

      /* Komi spin button and label. */
      dialog->komi
	= gtk_games_create_komi_adjustmet (new_go_game_configuration.komi);
      komi_spin_button = gtk_utils_create_spin_button (dialog->komi,
						       0.0, 1, FALSE);

      label = gtk_utils_create_mnemonic_label (_("_Komi:"), komi_spin_button);

      /* Pack the spin button and label together and add them to the
       * rules box.
       */
      hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				    label, 0, komi_spin_button, GTK_UTILS_FILL,
				    NULL);
      gtk_box_pack_start (rules_vbox, hbox, FALSE, TRUE, 0);

      gtk_size_group_add_widget (spin_buttons_size_group, komi_spin_button);
    }

    /* Pack game-specific rules box and time control box together and
     * add them to the games notebook.
     */
    vbox1 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				   rules_vbox_widget, GTK_UTILS_FILL,
				   time_control_named_vbox, GTK_UTILS_FILL,
				   NULL);
    gtk_notebook_append_page (dialog->games_notebook, vbox1, NULL);

    /* Align all labels in one call. */
    gtk_utils_align_left_widgets (GTK_CONTAINER (vbox1), NULL);
  }

  /* Finally, add the game rules notebook as an assistant page. */
  gtk_assistant_add_page (&dialog->assistant, games_notebook,
			  GTK_STOCK_PREFERENCES, _("Game Rules"),
			  ((GtkAssistantPageShownCallback)
			   show_game_specific_rules),
			  ((GtkAssistantPageAcceptableCallback)
			   instantiate_players));
  gtk_assistant_set_page_help_link_id_callback
    (&dialog->assistant, games_notebook,
     (GtkAssistantPageHelpLinkIDCallback) get_game_rules_help_link_id);
  gtk_widget_show_all (games_notebook);
}


void
gtk_new_game_dialog_present (void)
{
  static GtkWindow *new_game_dialog = NULL;

  if (!new_game_dialog) {
    new_game_dialog = GTK_WINDOW (g_object_new (GTK_TYPE_NEW_GAME_DIALOG,
						NULL));

    gtk_control_center_window_created (new_game_dialog);
    gtk_utils_null_pointer_on_destroy (&new_game_dialog, TRUE);
  }

  gtk_window_present (new_game_dialog);
}


static void
update_game_and_players_page (GtkWidget *widget, gpointer user_data)
{
  GtkNewGameDialog *dialog   = GTK_NEW_GAME_DIALOG (user_data);
  GtkGameIndex selected_game = get_selected_game (dialog);
  GtpEngineListItem *engine_datum[NUM_COLORS] = { NULL, NULL };
  int k;

  UNUSED (widget);

  for (k = 0; k < NUM_COLORS; k++) {
    GtkWidget *selector = dialog->engine_selectors[k];

    if (gtk_preferences_have_non_hidden_gtp_engine ()) {
      if (GTK_WIDGET_IS_SENSITIVE (selector)) {
	engine_datum[k]
	  = gtk_preferences_get_engine_selector_selection (selector);
      }

      gtk_widget_set_sensitive
	(GTK_WIDGET (dialog->player_radio_buttons[k][1]), TRUE);
    }
    else {
      gtk_toggle_button_set_active (dialog->player_radio_buttons[k][0], TRUE);
      gtk_widget_set_sensitive
	(GTK_WIDGET (dialog->player_radio_buttons[k][1]), FALSE);
    }

    gtk_preferences_set_engine_selector_game_index (selector, selected_game);
  }

  for (k = 0; k < NUM_SUPPORTED_GAMES; k++) {
    gboolean game_is_supported
      = (gtk_games_engine_supports_game (engine_datum[BLACK_INDEX], k)
	 && gtk_games_engine_supports_game (engine_datum[WHITE_INDEX], k));

    if (gtk_toggle_button_get_active (dialog->game_radio_buttons[k])) {
      gtk_widget_set_sensitive (dialog->assistant.next_button,
				game_is_supported);
    }

    gtk_image_set_from_stock (GTK_IMAGE (dialog->game_supported_icons[k]),
			      game_is_supported ? GTK_STOCK_YES : GTK_STOCK_NO,
			      GTK_ICON_SIZE_MENU);
  }
}


static void
swap_players (GtkNewGameDialog *dialog)
{
  gboolean player_is_human[NUM_COLORS];
  gchar *human_names[NUM_COLORS];
  GtpEngineListItem *engine_datum[NUM_COLORS];
  int k;

  for (k = 0; k < NUM_COLORS; k++) {
    GtkWidget *selector = dialog->engine_selectors[k];

    player_is_human[k]
      = gtk_toggle_button_get_active (dialog->player_radio_buttons[k][0]);

    human_names[k]
      = g_strdup (gtk_entry_get_text (dialog->human_name_entries[k]));
    engine_datum[k] = gtk_preferences_get_engine_selector_selection (selector);
  }

  for (k = 0; k < NUM_COLORS; k++) {
    int other = OTHER_INDEX (k);

    gtk_entry_set_text (dialog->human_name_entries[other], human_names[k]);
    gtk_preferences_set_engine_selector_selection
      (dialog->engine_selectors[other], engine_datum[k]);

    gtk_toggle_button_set_active
      (dialog->player_radio_buttons[other][player_is_human[k] ? 0 : 1], TRUE);

    g_free (human_names[k]);
  }
}


static GtkGameIndex
get_selected_game (GtkNewGameDialog *dialog)
{
  gboolean selected_game;

  for (selected_game = 0; ; selected_game++) {
    if (gtk_toggle_button_get_active
	(dialog->game_radio_buttons[selected_game]))
      break;
  }

  return selected_game;
}


static void
show_game_specific_rules (GtkNewGameDialog *dialog)
{
  gtk_notebook_set_current_page (dialog->games_notebook,
				 get_selected_game (dialog));
}


static void
set_handicap_adjustment_limits (GtkAdjustment *board_size_adjustment,
				GtkNewGameDialog *dialog)
{
  gint board_size = gtk_adjustment_get_value (board_size_adjustment);

  gtk_games_set_handicap_adjustment_limits (board_size, board_size,
					    dialog->handicaps[0],
					    dialog->handicaps[1]);
}


static void
time_control_type_changed (GtkWidget *selector, GtkNotebook *notebook)
{
  gint time_control_type = gtk_utils_get_selector_active_item_index (selector);

  gtk_notebook_set_current_page (notebook, time_control_type);
}


static const gchar *
get_game_rules_help_link_id (GtkNewGameDialog *dialog)
{
  switch (get_selected_game (dialog)) {
  case GTK_GAME_GO:
    return "new-game-dialog-go-rules";

  case GTK_GAME_AMAZONS:
    return "new-game-dialog-amazons-rules";

  case GTK_GAME_REVERSI:
    return "new-game-dialog-reversi-rules";

  default:
    g_warning ("unhandled game %s",
	       INDEX_TO_GAME_NAME (get_selected_game (dialog)));
    return NULL;
  }
}


static gboolean
instantiate_players (GtkNewGameDialog *dialog)
{
  int k;
  GtkWindow *window = GTK_WINDOW (dialog);
  GtkEngineChain *engine_chain
    = gtk_preferences_create_engines_instantiation_chain (window,
							  begin_game, dialog);

  for (k = 0; k < NUM_COLORS; k++) {
    if (GTK_WIDGET_IS_SENSITIVE (dialog->engine_selectors[k])) {
      gtk_preferences_instantiate_selected_engine (engine_chain,
						   dialog->engine_selectors[k],
						   &dialog->players[k]);
    }
    else
      dialog->players[k] = NULL;
  }

  gtk_preferences_do_instantiate_engines (engine_chain);

  return FALSE;
}


static void
begin_game (GtkEnginesInstantiationStatus status, gpointer user_data)
{
  GtkNewGameDialog *dialog;
  GtkGameIndex game_index;
  Game game;
  int board_size;
  gboolean player_is_computer[NUM_COLORS];
  const char *human_names[NUM_COLORS];
  const char *engine_screen_names[NUM_COLORS];
  NewGameDialogTimeControlData * time_control_data;
  TimeControlConfiguration *time_control_configuration;
  TimeControl *time_control;
  SgfGameTree *game_tree;
  SgfCollection *sgf_collection;
  GtkWidget *goban_window;
  int k;

  if (status != ENGINES_INSTANTIATED)
    return;

  dialog     = GTK_NEW_GAME_DIALOG (user_data);
  game_index = get_selected_game (dialog);
  game	     = index_to_game[game_index];
  board_size = gtk_adjustment_get_value (dialog->board_sizes[game_index]);

  time_control_data	     = dialog->time_control_data + game_index;
  time_control_configuration = NULL;
  time_control		     = NULL;

  game_tree = sgf_game_tree_new_with_root (game, board_size, board_size, 1);
  game_tree->char_set = utils_duplicate_string ("UTF-8");

  sgf_node_add_text_property (game_tree->root, game_tree,
			      SGF_RESULT, utils_duplicate_string ("Void"), 0);

  for (k = 0; k < NUM_COLORS; k++) {
    GtpEngineListItem *engine_data =
      (gtk_preferences_get_engine_selector_selection
       (dialog->engine_selectors[k]));
    char *player_name;

    player_is_computer[k]
      = GTK_WIDGET_IS_SENSITIVE (dialog->engine_selectors[k]);
    human_names[k] = gtk_entry_get_text (dialog->human_name_entries[k]);
    engine_screen_names[k] = (engine_data ? engine_data->screen_name : NULL);

    player_name = sgf_utils_normalize_text ((player_is_computer[k]
					     ? engine_screen_names[k]
					     : human_names[k]),
					    1);

    if (player_name) {
      sgf_node_add_text_property (game_tree->root, game_tree,
				  (k == BLACK_INDEX
				   ? SGF_PLAYER_BLACK : SGF_PLAYER_WHITE),
				  player_name, 0);
    }
  }

  if (game == GAME_GO) {
    gboolean handicap_is_fixed
      = gtk_toggle_button_get_active (dialog->handicap_toggle_buttons[0]);
    gint handicap
      = gtk_adjustment_get_value (dialog->handicaps[handicap_is_fixed
						    ? 0 : 1]);
    gdouble komi = gtk_adjustment_get_value (dialog->komi);

    /* Don't bother user with handicap subtleties. */
    if (handicap == 1)
      handicap = 0;

    sgf_utils_set_handicap (game_tree, handicap, handicap_is_fixed);
    sgf_node_add_text_property (game_tree->current_node, game_tree,
				SGF_KOMI, utils_cprintf ("%.f", komi), 0);

    new_go_game_configuration.board_size	= board_size;
    new_go_game_configuration.handicap_is_fixed = handicap_is_fixed;

    if (handicap_is_fixed)
      new_go_game_configuration.fixed_handicap	= handicap;
    else
      new_go_game_configuration.free_handicap	= handicap;

    new_go_game_configuration.komi		= komi;

    time_control_configuration = &new_go_game_configuration.time_control;
  }
  else if (game == GAME_AMAZONS) {
    new_amazons_game_configuration.board_size	= board_size;

    time_control_configuration = &new_amazons_game_configuration.time_control;
  }
  else if (game == GAME_REVERSI) {
    new_reversi_game_configuration.board_size	= board_size;

    time_control_configuration = &new_reversi_game_configuration.time_control;
  }

  time_control_configuration->type
    = gtk_notebook_get_current_page (time_control_data->notebook);

  if (time_control_configuration->type == 0) {
    GtkToggleButton *track_total_time_button
      = time_control_data->track_total_time_button;

    time_control_configuration->track_total_time
      = gtk_toggle_button_get_active (track_total_time_button);
    if (time_control_configuration->track_total_time)
      time_control = time_control_new (0, 0, 0);
  }
  else {
    switch (time_control_configuration->type) {
      /* FIXME: Should be an enumeration, not hardwired constants. */
    case 1:
      time_control_configuration->game_time_limit
	= gtk_adjustment_get_value (time_control_data->game_time_limit);

      time_control
	= time_control_new (time_control_configuration->game_time_limit, 0, 0);

      break;

    case 2:
      time_control_configuration->move_time_limit
	= gtk_adjustment_get_value (time_control_data->move_time_limit);

      time_control
	= time_control_new (0, time_control_configuration->move_time_limit, 1);

      break;

    case 3:
      time_control_configuration->main_time
	= gtk_adjustment_get_value (time_control_data->main_time);
      time_control_configuration->overtime_period_length
	= gtk_adjustment_get_value (time_control_data->overtime_period);
      time_control_configuration->moves_per_overtime
	= gtk_adjustment_get_value (time_control_data->moves_per_overtime);

      time_control
	= time_control_new (time_control_configuration->main_time,
			    time_control_configuration->overtime_period_length,
			    time_control_configuration->moves_per_overtime);
      break;

    default:
      g_assert_not_reached ();
    }

    time_control_save_settings_in_sgf_node (time_control,
					    game_tree->current_node,
					    game_tree);
  }

  sgf_collection = sgf_collection_new ();
  sgf_collection_add_game_tree (sgf_collection, game_tree);

  configuration_set_string_value (&new_game_configuration.game_name,
				  game_info[game].name);

  for (k = 0; k < NUM_COLORS; k++) {
    configuration_set_string_value (&new_game_configuration.player_names[k],
				    human_names[k]);

    new_game_configuration.player_is_computer[k] = player_is_computer[k];
    configuration_set_string_value (&new_game_configuration.engine_names[k],
				    engine_screen_names[k]);
  }

  goban_window = gtk_goban_window_new (sgf_collection, NULL);
  gtk_goban_window_enter_game_mode (GTK_GOBAN_WINDOW (goban_window),
				    dialog->players[BLACK_INDEX],
				    dialog->players[WHITE_INDEX],
				    time_control);
  gtk_widget_show (goban_window);

  gtk_widget_destroy (GTK_WIDGET (dialog));
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
