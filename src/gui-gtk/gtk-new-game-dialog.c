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


#include "gtk-new-game-dialog.h"

#include "gtk-assistant.h"
#include "gtk-configuration.h"
#include "gtk-games.h"
#include "gtk-goban-window.h"
#include "gtk-gtp-client-interface.h"
#include "gtk-named-vbox.h"
#include "gtk-preferences.h"
#include "gtk-utils.h"
#include "quarry-stock.h"
#include "board.h"
#include "game-info.h"

#include <gtk/gtk.h>
#include <assert.h>


typedef struct _TimeControlData		TimeControlData;
typedef struct _NewGameDialogData	NewGameDialogData;

struct _TimeControlData {
  GtkNotebook	   *notebook;
  GtkToggleButton  *track_total_time_button;

  GtkAdjustment    *game_time_limit;
  GtkAdjustment    *move_time_limit;
  GtkAdjustment	   *main_time;
  GtkAdjustment	   *overtime_period;
  GtkAdjustment	   *moves_per_overtime;
};

struct _NewGameDialogData {
  GtkAssistant	   *assistant;

  GtkToggleButton  *game_radio_buttons[NUM_SUPPORTED_GAMES];
  GtkWidget	   *game_supported_icons[NUM_SUPPORTED_GAMES];

  GtkToggleButton  *player_radio_buttons[NUM_COLORS][2];
  GtkEntry	   *human_name_entries[NUM_COLORS];
  GtkWidget	   *engine_selectors[NUM_COLORS];

  GtkNotebook	   *games_notebook;

  GtkAdjustment	   *board_sizes[NUM_SUPPORTED_GAMES];
  GtkToggleButton  *handicap_toggle_buttons[2];
  GtkAdjustment	   *handicaps[2];
  GtkAdjustment	   *komi;

  TimeControlData   time_control_data[NUM_SUPPORTED_GAMES];

  GtpClient	   *players[NUM_COLORS];
};


static void	     update_game_and_players_page(GtkWidget *widget,
						  gpointer user_data);
static void	     swap_players(NewGameDialogData *data);
static GtkGameIndex  get_selected_game(NewGameDialogData *data);

static void	     show_game_specific_rules(NewGameDialogData *data);
static void	     set_handicap_adjustment_limits
		       (GtkAdjustment *board_size_adjustment,
			NewGameDialogData *data);

static void	     time_control_type_changed(GtkWidget *selector,
					       GtkNotebook *notebook);

static gboolean	     instantiate_players(NewGameDialogData *data);
static void	     begin_game(GtkEnginesInstantiationStatus status,
				gpointer user_data);


void
gtk_new_game_dialog_present(void)
{
  static const gchar *hint_text
    = N_("Note that most GTP engines support only one game. Hence certain "
	 "game / computer player combinations may be not possible to play.");

  static const gchar *handicap_radio_button_labels[2]
    = { N_("Fi_xed handicap:"), N_("_Free handicap:") };

  static const gchar *time_control_types[]
    = { N_("No limit"), N_("Limited time for entire game"),
	N_("Limited time per move"), N_("Canadian overtime") };

  NewGameDialogData *data = g_malloc(sizeof(NewGameDialogData));
  gint game_index = gtk_games_name_to_index(new_game_configuration.game_name,
					    FALSE);
  GtkWidget *assistant;
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

  /* Create assistant dialog that will hold two pages. */
  assistant = gtk_assistant_new(_("New Game"), data);
  data->assistant = GTK_ASSISTANT(assistant);
  gtk_assistant_set_finish_button(data->assistant, QUARRY_STOCK_PLAY);
  gtk_utils_make_window_only_horizontally_resizable(GTK_WINDOW(assistant));

  /* "Game & Players" page. */

  /* Vertical box for game radio buttons. */
  vbox1 = gtk_vbox_new(FALSE, QUARRY_SPACING_SMALL);

  /* Create a radio button for each of the supported games. */
  for (k = 0; k < NUM_SUPPORTED_GAMES; k++) {
    GtkWidget *radio_button;

    label = gtk_label_new_with_mnemonic(game_labels[k]);
    data->game_supported_icons[k]
       = gtk_image_new_from_stock(GTK_STOCK_YES, GTK_ICON_SIZE_MENU);

    hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING_SMALL,
				 label, GTK_UTILS_PACK_DEFAULT,
				 data->game_supported_icons[k], 0, NULL);

    radio_button = gtk_radio_button_new_from_widget(last_radio_button);
    last_radio_button = GTK_RADIO_BUTTON(radio_button);
    data->game_radio_buttons[k] = GTK_TOGGLE_BUTTON(radio_button);
    gtk_container_add(GTK_CONTAINER(radio_button), hbox);
    gtk_box_pack_start(GTK_BOX(vbox1), radio_button, FALSE, TRUE, 0);

    g_signal_connect(radio_button, "toggled",
		     G_CALLBACK(update_game_and_players_page), data);
  }

  /* Hint about engine/game (non-)compatibility. */
  label = gtk_utils_create_left_aligned_label(_(hint_text));
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

  /* Pack the radio buttons and the hint next to each other. */
  hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
			       vbox1, GTK_UTILS_FILL,
			       label, GTK_UTILS_PACK_DEFAULT, NULL);

  /* And pack everything into a box with caption saying "Game". */
  named_vbox = gtk_named_vbox_new(_("Game"), FALSE, QUARRY_SPACING_SMALL);
  gtk_box_pack_start_defaults(GTK_BOX(named_vbox), hbox);

  /* Create identical set of controls for each of the players. */
  for (k = 0; k < NUM_COLORS; k++) {
    static const char *radio_labels[NUM_COLORS][2]
      = { { N_("H_uman"), N_("Compu_ter") },
	  { N_("_Human"), N_("Com_puter") } };

    GtkWidget *entry;
    const char *engine_name = new_game_configuration.engine_names[k];

    /* Two radio buttons for selecting who is controlling this
     * player--human or computer.
     */
    radio_buttons = (GtkWidget **) data->player_radio_buttons[k];
    gtk_utils_create_radio_chain(radio_buttons, radio_labels[k], 2);
    if (new_game_configuration.player_is_computer[k])
      gtk_toggle_button_set_active(data->player_radio_buttons[k][1], TRUE);

    /* Text entry for human's name. */
    entry = gtk_utils_create_entry(new_game_configuration.player_names[k],
				   RETURN_ACTIVATES_DEFAULT);
    data->human_name_entries[k] = GTK_ENTRY(entry);
    gtk_utils_set_sensitive_on_toggle(data->player_radio_buttons[k][0], entry);

    /* Pack it together with corresponding radio button. */
    hboxes[0] = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				      radio_buttons[0], 0,
				      entry, GTK_UTILS_PACK_DEFAULT, NULL);

    /* Selector (combo box/option menu) for computer player. */
    data->engine_selectors[k]
      = gtk_preferences_create_engine_selector(GTK_GAME_GO, engine_name,
					       update_game_and_players_page,
					       data);
    gtk_utils_set_sensitive_on_toggle(data->player_radio_buttons[k][1],
				      data->engine_selectors[k]);
    g_signal_connect(radio_buttons[1], "toggled",
		     G_CALLBACK(update_game_and_players_page), data);

    /* Horizontal box with a radio button and the selector. */
    hboxes[1] = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				      radio_buttons[1], 0,
				      data->engine_selectors[k],
				      GTK_UTILS_PACK_DEFAULT,
				      NULL);

    /* And pack everything into a box with corresponding caption. */
    player_vboxes[k] = gtk_utils_pack_in_box(GTK_TYPE_NAMED_VBOX,
					     QUARRY_SPACING_SMALL,
					     hboxes[0], GTK_UTILS_FILL,
					     hboxes[1], GTK_UTILS_FILL, NULL);
    gtk_named_vbox_set_label_text(GTK_NAMED_VBOX(player_vboxes[k]),
				  (k == BLACK_INDEX
				   ? _("Black Player") : _("White Player")));
  }

  /* Make sure the radio buttons line up nicely. */
  gtk_utils_create_size_group(GTK_SIZE_GROUP_HORIZONTAL,
			      data->player_radio_buttons[WHITE_INDEX][0],
			      data->player_radio_buttons[WHITE_INDEX][1],
			      data->player_radio_buttons[BLACK_INDEX][0],
			      data->player_radio_buttons[BLACK_INDEX][1],
			      vbox1, NULL);

  /* Button which opens "Preferences" dialog at "GTP Engines" page. */
  button = gtk_button_new_with_mnemonic(_("_Manage Engine List"));
  g_signal_connect_swapped(button, "clicked",
			   G_CALLBACK(gtk_preferences_dialog_present),
			   GINT_TO_POINTER(PREFERENCES_PAGE_GTP_ENGINES));

  /* Pack boxes for the two players and the button vertically.  In
   * GUI, White always comes before Black.
   */
  vbox1 = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				player_vboxes[WHITE_INDEX], GTK_UTILS_FILL,
				player_vboxes[BLACK_INDEX], GTK_UTILS_FILL,
				gtk_utils_align_widget(button, 1.0, 0.5),
				GTK_UTILS_FILL,
				NULL);

  /* Top padding for the "Swap" button below. */
  label = gtk_label_new(NULL);
  gtk_utils_create_size_group(GTK_SIZE_GROUP_VERTICAL, button, label, NULL);

  /* A button that allows to quickly swap colors of the players. */
  button = gtk_button_new_with_mnemonic(_("_Swap"));
  g_signal_connect_swapped(button, "clicked", G_CALLBACK(swap_players), data);

  /* Pack the button into a box with top and bottom padding so it ends
   * up just between two players' control sets.
   */
  vbox2 = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				gtk_label_new(NULL), 0,
				button, GTK_UTILS_EXPAND,
				label, (QUARRY_SPACING_BIG
					  - QUARRY_SPACING_SMALL) / 2,
				NULL);

  /* Pack players' control sets and the "Swap" button box just to the
   * right of them.
   */
  hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
			       vbox1, GTK_UTILS_PACK_DEFAULT,
			       vbox2, GTK_UTILS_FILL, NULL);

  /* Finally pack everything into a vertical box and add it as the
   * assistant dialog page.
   */
  game_and_players_page
    = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
			    named_vbox, GTK_UTILS_FILL,
			    hbox, GTK_UTILS_FILL, NULL);
  gtk_assistant_add_page(data->assistant, game_and_players_page,
			 GTK_STOCK_REFRESH, _("Game &amp; Players"),
			 NULL, NULL);
  gtk_widget_show_all(game_and_players_page);

  if (game_index != -1)
    gtk_toggle_button_set_active(data->game_radio_buttons[game_index], TRUE);

  update_game_and_players_page(NULL, data);

  /* "Game Rules" page. */

  /* Notebook with a page for each game. */
  games_notebook = gtk_utils_create_invisible_notebook();
  data->games_notebook = GTK_NOTEBOOK(games_notebook);

  /* Board size adjustments for each of the games. */
  data->board_sizes[GTK_GAME_GO]
    = ((GtkAdjustment *)
       gtk_adjustment_new(new_go_game_configuration.board_size,
			  GTK_MIN_BOARD_SIZE, GTK_MAX_BOARD_SIZE, 1, 2, 0));
  data->board_sizes[GTK_GAME_AMAZONS]
    = ((GtkAdjustment *)
       gtk_adjustment_new(new_amazons_game_configuration.board_size,
			  GTK_MIN_BOARD_SIZE, GTK_MAX_BOARD_SIZE, 1, 2, 0));
  data->board_sizes[GTK_GAME_OTHELLO]
    = ((GtkAdjustment *)
       gtk_adjustment_new(new_othello_game_configuration.board_size,
			  ROUND_UP(GTK_MIN_BOARD_SIZE, 2),
			  ROUND_DOWN(GTK_MAX_BOARD_SIZE, 2),
			  2, 4, 0));

  for (k = 0; k < NUM_SUPPORTED_GAMES; k++) {
    TimeControlData *const time_control_data = data->time_control_data + k;
    const TimeControlConfiguration *time_control_configuration = NULL;

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
    char rules_vbox_title[64];

    if (k == GTK_GAME_GO)
      time_control_configuration = &new_go_game_configuration.time_control;
    else if (k == GTK_GAME_AMAZONS) {
      time_control_configuration
	= &new_amazons_game_configuration.time_control;
    }
    else if (k == GTK_GAME_OTHELLO) {
      time_control_configuration
	= &new_othello_game_configuration.time_control;
    }

    /* "Game Rules" named vertical box. */
    /* FIXME: i18n. */
    sprintf(rules_vbox_title, "%s Rules", game_info[index_to_game[k]].name);
    rules_vbox_widget = gtk_named_vbox_new(rules_vbox_title, FALSE,
					   QUARRY_SPACING_SMALL);
    rules_vbox = GTK_BOX(rules_vbox_widget);

    /* Board size spin button and a label for it. */
    board_size_spin_button = gtk_utils_create_spin_button(data->board_sizes[k],
							  0.0, 0, TRUE);
    label = gtk_utils_create_mnemonic_label(_("Board _size:"),
					    board_size_spin_button);

    /* Pack them into a box and add to "Game Rules" box. */
    hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				 label, 0,
				 board_size_spin_button, GTK_UTILS_FILL, NULL);
    gtk_box_pack_start(rules_vbox, hbox, FALSE, TRUE, 0);

    /* A combo box (option menu for pre-2.4) for selecting time
     * control type and a label for it..
     */
    time_control_type = gtk_utils_create_selector(time_control_types,
						  (sizeof(time_control_types)
						   / sizeof(const gchar *)),
						  -1);
    label = gtk_utils_create_mnemonic_label(_("Time control _type:"),
					    time_control_type);

    /* Pack the selector and the label together. */
    hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				 label, 0,
				 time_control_type, GTK_UTILS_FILL, NULL);

    /* A notebook with a page for each type of time control. */
    time_control_notebook = gtk_utils_create_invisible_notebook();
    time_control_data->notebook = GTK_NOTEBOOK(time_control_notebook);

    g_signal_connect(time_control_type, "changed",
		     G_CALLBACK(time_control_type_changed),
		     time_control_notebook);

    /* Pack time control widgets together in a named box. */
    time_control_named_vbox
      = gtk_utils_pack_in_box(GTK_TYPE_NAMED_VBOX, QUARRY_SPACING,
			      hbox, GTK_UTILS_FILL,
			      time_control_notebook, GTK_UTILS_FILL, NULL);
    gtk_named_vbox_set_label_text(GTK_NAMED_VBOX(time_control_named_vbox),
				  _("Time Limit"));

    check_button
      = gtk_check_button_new_with_mnemonic(_("K_eep track of total time"));
    time_control_data->track_total_time_button
      = GTK_TOGGLE_BUTTON(check_button);
    if (time_control_configuration->track_total_time) {
      gtk_toggle_button_set_active(time_control_data->track_total_time_button,
				   TRUE);
    }

    gtk_notebook_append_page(time_control_data->notebook,
			     gtk_utils_align_widget(check_button, 0.0, 0.0),
			     NULL);

    time_control_data->game_time_limit
      = ((GtkAdjustment *)
	 gtk_adjustment_new(time_control_configuration->game_time_limit,
			    1.0, 3600000.0 - 1.0, 60.0, 300.0, 0.0));

    game_time_limit_spin_button
      = gtk_utils_create_time_spin_button(time_control_data->game_time_limit,
					  60.0);
    label = gtk_utils_create_mnemonic_label(_("Time _limit for game:"),
					    game_time_limit_spin_button);

    hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				 label, GTK_UTILS_FILL,
				 game_time_limit_spin_button, GTK_UTILS_FILL,
				 gtk_label_new(_("(per player)")),
				 GTK_UTILS_FILL,
				 NULL);
    gtk_notebook_append_page(time_control_data->notebook,
			     gtk_utils_align_widget(hbox, 0.0, 0.0), NULL);

    time_control_data->move_time_limit
      = ((GtkAdjustment *)
	 gtk_adjustment_new(time_control_configuration->move_time_limit,
			    1.0, 360000.0 - 1.0, 5.0, 30.0, 0.0));

    move_time_limit_spin_button
      = gtk_utils_create_time_spin_button(time_control_data->move_time_limit,
					  5.0);
    label = gtk_utils_create_mnemonic_label(_("Time _limit for move:"),
					    move_time_limit_spin_button);

    hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				 label, GTK_UTILS_FILL,
				 move_time_limit_spin_button, GTK_UTILS_FILL,
				 NULL);
    gtk_notebook_append_page(time_control_data->notebook,
			     gtk_utils_align_widget(hbox, 0.0, 0.0), NULL);

    time_control_data->main_time
      = ((GtkAdjustment *)
	 gtk_adjustment_new(time_control_configuration->main_time,
			    0.0, 3600000.0 - 1.0, 60.0, 300.0, 0.0));

    main_time_spin_button
      = gtk_utils_create_time_spin_button(time_control_data->main_time, 60.0);
    label = gtk_utils_create_mnemonic_label(_("_Main time:"),
					    main_time_spin_button);

    hboxes[0] = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				      label, GTK_UTILS_FILL,
				      main_time_spin_button, GTK_UTILS_FILL,
				      NULL);

    time_control_data->overtime_period
      = ((GtkAdjustment *)
	 gtk_adjustment_new(time_control_configuration->overtime_period_length,
			    1.0, 3600000.0 - 1.0, 60.0, 300.0, 0.0));

    overtime_period_spin_button
      = gtk_utils_create_time_spin_button(time_control_data->overtime_period,
					  60.0);
    label = gtk_utils_create_mnemonic_label(_("_Overtime period length:"),
					    overtime_period_spin_button);

    hboxes[1] = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				      label, GTK_UTILS_FILL,
				      overtime_period_spin_button,
				      GTK_UTILS_FILL,
				      NULL);

    time_control_data->moves_per_overtime
      = ((GtkAdjustment *)
	 gtk_adjustment_new(time_control_configuration->moves_per_overtime,
			    1.0, 999.0, 1.0, 5.0, 0.0));

    moves_per_overtime_spin_button
      = gtk_utils_create_spin_button(time_control_data->moves_per_overtime,
				     0.0, 0, TRUE);
    label = gtk_utils_create_mnemonic_label(_("Mo_ves per overtime:"),
					    moves_per_overtime_spin_button);

    hboxes[2] = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				      label, GTK_UTILS_FILL,
				      moves_per_overtime_spin_button,
				      GTK_UTILS_FILL,
				      NULL);

    vbox1 = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				  hboxes[0], GTK_UTILS_FILL,
				  hboxes[1], GTK_UTILS_FILL,
				  hboxes[2], GTK_UTILS_FILL, NULL);
    gtk_notebook_append_page(time_control_data->notebook, vbox1, NULL);

    /* Select the last used time control type. */
    if (0 <= time_control_configuration->type
	&& time_control_configuration->type <= (sizeof(time_control_types)
						/ sizeof(const gchar *))) {
      gtk_widget_show_all(time_control_notebook);
      gtk_utils_set_selector_active_item_index(time_control_type,
					       (time_control_configuration
						->type));
    }

    /* Align spin buttons nicely. */
    spin_buttons_size_group
      = gtk_utils_create_size_group(GTK_SIZE_GROUP_HORIZONTAL,
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
      radio_buttons = (GtkWidget **) data->handicap_toggle_buttons;
      gtk_utils_create_radio_chain(radio_buttons, handicap_radio_button_labels,
				   2);
      if (!new_go_game_configuration.handicap_is_fixed)
	gtk_toggle_button_set_active(data->handicap_toggle_buttons[1], TRUE);

      /* Handicap spin buttons and boxes to pack them together with
       * the radio buttons.
       */
      for (i = 0; i < 2; i++) {
	data->handicaps[i]
	  = ((GtkAdjustment *)
	     gtk_adjustment_new((i == 0
				 ? new_go_game_configuration.fixed_handicap
				 : new_go_game_configuration.free_handicap),
				0, GTK_MAX_BOARD_SIZE * GTK_MAX_BOARD_SIZE,
				1, 2, 0));

	handicap_spin_buttons[i]
	  = gtk_utils_create_spin_button(data->handicaps[i], 0.0, 0, TRUE);
	gtk_utils_set_sensitive_on_toggle(data->handicap_toggle_buttons[i],
					  handicap_spin_buttons[i]);

	hboxes[i] = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
					  radio_buttons[i], 0,
					  handicap_spin_buttons[i],
					  GTK_UTILS_FILL,
					  NULL);

	/* Force proper alignment of handicap spin buttons. */
	gtk_size_group_add_widget(spin_buttons_size_group,
				  handicap_spin_buttons[i]);
      }

      /* Pack all handicap controls in a vertical box and add it to
       * the rules box with a small padding.
       */
      vbox1 = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				    hboxes[0], GTK_UTILS_FILL,
				    hboxes[1], GTK_UTILS_FILL, NULL);
      gtk_box_pack_start(rules_vbox, vbox1, FALSE, TRUE, QUARRY_SPACING_SMALL);

      set_handicap_adjustment_limits(data->board_sizes[GTK_GAME_GO], data);
      g_signal_connect(data->board_sizes[GTK_GAME_GO], "value-changed",
		       G_CALLBACK(set_handicap_adjustment_limits), data);

      /* Komi spin button and label. */
      data->komi = ((GtkAdjustment *)
		    gtk_adjustment_new(new_go_game_configuration.komi,
				       -999.5, 999.5, 1.0, 5.0, 0.0));
      komi_spin_button = gtk_utils_create_spin_button(data->komi,
						      0.0, 1, FALSE);

      label = gtk_utils_create_mnemonic_label(_("_Komi:"), komi_spin_button);

      /* Pack the spin button and label together and add them to the
       * rules box.
       */
      hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				   label, 0, komi_spin_button, GTK_UTILS_FILL,
				   NULL);
      gtk_box_pack_start(rules_vbox, hbox, FALSE, TRUE, 0);

      gtk_size_group_add_widget(spin_buttons_size_group, komi_spin_button);
    }

    /* Pack game-specific rules box and time control box together and
     * add them to the games notebook.
     */
    vbox1 = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				  rules_vbox_widget, GTK_UTILS_FILL,
				  time_control_named_vbox, GTK_UTILS_FILL,
				  NULL);
    gtk_notebook_append_page(data->games_notebook, vbox1, NULL);

    /* Align all labels in one call. */
    gtk_utils_align_left_widgets(GTK_CONTAINER(vbox1), NULL);
  }

  /* Finally, add the game rules notebook as an assistant page. */
  gtk_assistant_add_page(data->assistant, games_notebook,
			 GTK_STOCK_PREFERENCES, _("Game Rules"),
			 ((GtkAssistantPageShownCallback)
			  show_game_specific_rules),
			 ((GtkAssistantPageAcceptableCallback)
			  instantiate_players));
  gtk_widget_show_all(games_notebook);

  gtk_widget_show(assistant);
}


static void
update_game_and_players_page(GtkWidget *widget, gpointer user_data)
{
  NewGameDialogData *data = (NewGameDialogData *) user_data;
  GtkGameIndex selected_game = get_selected_game(data);
  GtpEngineListItem *engine_datum[NUM_COLORS] = { NULL, NULL };
  int k;

  UNUSED(widget);

  for (k = 0; k < NUM_COLORS; k++) {
    GtkWidget *selector = data->engine_selectors[k];

    if (!string_list_is_empty(&gtp_engines)) {
      if (GTK_WIDGET_IS_SENSITIVE(selector)) {
	engine_datum[k]
	  = gtk_preferences_get_engine_selector_selection(selector);
      }

      gtk_widget_set_sensitive(GTK_WIDGET(data->player_radio_buttons[k][1]),
			       TRUE);
    }
    else {
      gtk_toggle_button_set_active(data->player_radio_buttons[k][0], TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(data->player_radio_buttons[k][1]),
			       FALSE);
    }

    gtk_preferences_set_engine_selector_game_index(selector, selected_game);
  }

  for (k = 0; k < NUM_SUPPORTED_GAMES; k++) {
    gboolean game_is_supported
      = (gtk_games_engine_supports_game(engine_datum[BLACK_INDEX], k)
	 && gtk_games_engine_supports_game(engine_datum[WHITE_INDEX], k));

    if (gtk_toggle_button_get_active(data->game_radio_buttons[k])) {
      gtk_widget_set_sensitive(data->assistant->next_button,
			       game_is_supported);
    }

    gtk_image_set_from_stock(GTK_IMAGE(data->game_supported_icons[k]),
			     game_is_supported ? GTK_STOCK_YES : GTK_STOCK_NO,
			     GTK_ICON_SIZE_MENU);
  }
}


static void
swap_players(NewGameDialogData *data)
{
  gboolean player_is_human[NUM_COLORS];
  gchar *human_names[NUM_COLORS];
  GtpEngineListItem *engine_datum[NUM_COLORS];
  int k;

  for (k = 0; k < NUM_COLORS; k++) {
    GtkWidget *selector = data->engine_selectors[k];

    player_is_human[k]
      = gtk_toggle_button_get_active(data->player_radio_buttons[k][0]);

    human_names[k] = g_strdup(gtk_entry_get_text(data->human_name_entries[k]));
    engine_datum[k] = gtk_preferences_get_engine_selector_selection(selector);
  }

  for (k = 0; k < NUM_COLORS; k++) {
    int other = OTHER_INDEX(k);

    gtk_entry_set_text(data->human_name_entries[other], human_names[k]);
    gtk_preferences_set_engine_selector_selection(data->engine_selectors[other],
						  engine_datum[k]);

    gtk_toggle_button_set_active(data->player_radio_buttons
				   [other][player_is_human[k] ? 0 : 1],
				 TRUE);

    g_free(human_names[k]);
  }
}


static GtkGameIndex
get_selected_game(NewGameDialogData *data)
{
  gboolean selected_game;

  for (selected_game = 0; ; selected_game++) {
    if (gtk_toggle_button_get_active(data->game_radio_buttons[selected_game]))
      break;
  }

  return selected_game;
}


static void
show_game_specific_rules(NewGameDialogData *data)
{
  gtk_notebook_set_current_page(data->games_notebook, get_selected_game(data));
}


static void
set_handicap_adjustment_limits(GtkAdjustment *board_size_adjustment,
			       NewGameDialogData *data)
{
  gint board_size = gtk_adjustment_get_value(board_size_adjustment);
  int max_fixed_handicap = go_get_max_fixed_handicap(board_size, board_size);

  data->handicaps[0]->upper = (gdouble) max_fixed_handicap;
  gtk_adjustment_changed(data->handicaps[0]);

  data->handicaps[1]->upper = (gdouble) (board_size * board_size - 1);
  gtk_adjustment_changed(data->handicaps[1]);
}


static void
time_control_type_changed(GtkWidget *selector, GtkNotebook *notebook)
{
  gint time_control_type = gtk_utils_get_selector_active_item_index(selector);

  gtk_notebook_set_current_page(notebook, time_control_type);
}


static gboolean
instantiate_players(NewGameDialogData *data)
{
  int k;
  GtkWindow *window = GTK_WINDOW(data->assistant);
  GtkEngineChain *engine_chain
    = gtk_preferences_create_engines_instantiation_chain(window,
							 begin_game, data);

  for (k = 0; k < NUM_COLORS; k++) {
    if (GTK_WIDGET_IS_SENSITIVE(data->engine_selectors[k])) {
      gtk_preferences_instantiate_selected_engine(engine_chain,
						  data->engine_selectors[k],
						  &data->players[k]);
    }
    else
      data->players[k] = NULL;
  }

  gtk_preferences_do_instantiate_engines(engine_chain);

  return FALSE;
}


static void
begin_game(GtkEnginesInstantiationStatus status, gpointer user_data)
{
  NewGameDialogData *data = (NewGameDialogData *) user_data;
  GtkGameIndex game_index = get_selected_game(data);
  Game game = index_to_game[game_index];
  int board_size = gtk_adjustment_get_value(data->board_sizes[game_index]);
  gboolean player_is_computer[NUM_COLORS];
  const char *human_names[NUM_COLORS];
  const char *engine_screen_names[NUM_COLORS];
  TimeControlData *const time_control_data = (data->time_control_data
					      + game_index);
  TimeControlConfiguration *time_control_configuration = NULL;
  TimeControl *black_time_control = NULL;
  TimeControl *white_time_control = NULL;
  SgfGameTree *game_tree;
  SgfCollection *sgf_collection;
  GtkWidget *goban_window;
  int k;

  assert(status == ENGINES_INSTANTIATED);

  game_tree = sgf_game_tree_new_with_root(game, board_size, board_size, 1);
  game_tree->char_set = utils_duplicate_string("UTF-8");

  for (k = 0; k < NUM_COLORS; k++) {
    GtpEngineListItem *engine_data =
      gtk_preferences_get_engine_selector_selection(data->engine_selectors[k]);

    player_is_computer[k] = GTK_WIDGET_IS_SENSITIVE(data->engine_selectors[k]);
    human_names[k] = gtk_entry_get_text(data->human_name_entries[k]);
    engine_screen_names[k] = (engine_data ? engine_data->screen_name : NULL);

    sgf_node_add_text_property(game_tree->root, game_tree,
			       (k == BLACK_INDEX
				? SGF_PLAYER_BLACK : SGF_PLAYER_WHITE),
			       utils_duplicate_string(player_is_computer[k]
						      ? engine_screen_names[k]
						      : human_names[k]),
			       0);
  }

  if (game == GAME_GO) {
    gboolean handicap_is_fixed
      = gtk_toggle_button_get_active(data->handicap_toggle_buttons[0]);
    gint handicap = gtk_adjustment_get_value(data->handicaps[handicap_is_fixed
							     ? 0 : 1]);
    gdouble komi = gtk_adjustment_get_value(data->komi);

    /* Don't bother user with handicap subtleties. */
    if (handicap == 1)
      handicap = 0;

    sgf_utils_set_handicap(game_tree, handicap, handicap_is_fixed);
    sgf_node_add_text_property(game_tree->current_node, game_tree,
			       SGF_KOMI, utils_cprintf("%.f", komi), 0);

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
  else if (game == GAME_OTHELLO) {
    new_othello_game_configuration.board_size	= board_size;

    time_control_configuration = &new_othello_game_configuration.time_control;
  }

  time_control_configuration->type
    = gtk_notebook_get_current_page(time_control_data->notebook);

  switch (time_control_configuration->type) {
  case 0:
    {
      GtkToggleButton *track_total_time_button =
	time_control_data->track_total_time_button;

      time_control_configuration->track_total_time
	= gtk_toggle_button_get_active(track_total_time_button);
      if (time_control_configuration->track_total_time) {
	black_time_control = time_control_new(0, 0, 0);
	white_time_control = time_control_new(0, 0, 0);
      }
    }

    break;

  case 1:
    time_control_configuration->game_time_limit
      = gtk_adjustment_get_value(time_control_data->game_time_limit);

    black_time_control
      = time_control_new(time_control_configuration->game_time_limit, 0, 0);
    white_time_control
      = time_control_new(time_control_configuration->game_time_limit, 0, 0);

    break;

  case 2:
    time_control_configuration->move_time_limit
      = gtk_adjustment_get_value(time_control_data->move_time_limit);

    black_time_control
      = time_control_new(0, time_control_configuration->move_time_limit, 1);
    white_time_control
      = time_control_new(0, time_control_configuration->move_time_limit, 1);

    break;

  case 3:
    time_control_configuration->main_time
      = gtk_adjustment_get_value(time_control_data->main_time);
    time_control_configuration->overtime_period_length
      = gtk_adjustment_get_value(time_control_data->overtime_period);
    time_control_configuration->moves_per_overtime
      = gtk_adjustment_get_value(time_control_data->moves_per_overtime);

    black_time_control
      = time_control_new(time_control_configuration->main_time,
			 time_control_configuration->overtime_period_length,
			 time_control_configuration->moves_per_overtime);
    white_time_control
      = time_control_new(time_control_configuration->main_time,
			 time_control_configuration->overtime_period_length,
			 time_control_configuration->moves_per_overtime);

    break;

  default:
    assert(0);
  }

  sgf_collection = sgf_collection_new();
  sgf_collection_add_game_tree(sgf_collection, game_tree);

  configuration_set_string_value(&new_game_configuration.game_name,
				 game_info[game].name);

  for (k = 0; k < NUM_COLORS; k++) {
    configuration_set_string_value(&new_game_configuration.player_names[k],
				   human_names[k]);

    new_game_configuration.player_is_computer[k] = player_is_computer[k];
    configuration_set_string_value(&new_game_configuration.engine_names[k],
				   engine_screen_names[k]);
  }

  goban_window = gtk_goban_window_new(sgf_collection, NULL);
  gtk_goban_window_enter_game_mode(GTK_GOBAN_WINDOW(goban_window),
				   data->players[BLACK_INDEX],
				   data->players[WHITE_INDEX],
				   black_time_control, white_time_control);
  gtk_widget_show(goban_window);

  gtk_widget_destroy(GTK_WIDGET(data->assistant));
  g_free(data);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
