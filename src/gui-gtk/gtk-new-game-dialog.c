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


typedef struct _NewGameDialogData	NewGameDialogData;

struct _NewGameDialogData {
  GtkAssistant	   *assistant;

  GtkToggleButton  *game_radio_buttons[NUM_SUPPORTED_GAMES];
  GtkWidget	   *game_supported_icons[NUM_SUPPORTED_GAMES];

  GtkToggleButton  *player_radio_buttons[NUM_COLORS][2];
  GtkEntry	   *human_name_entries[NUM_COLORS];
  GtkWidget	   *engine_selectors[NUM_COLORS];

  GtkNotebook	   *notebook;

  GtkAdjustment	   *board_sizes[NUM_SUPPORTED_GAMES];
  GtkToggleButton  *handicap_toggle_buttons[2];
  GtkAdjustment	   *handicaps[2];
  GtkAdjustment	   *komi;

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

static gboolean	     instantiate_players(NewGameDialogData *data);
static void	     begin_game(GtkEnginesInstantiationStatus status,
				gpointer user_data);


void
gtk_new_game_dialog_present(void)
{
  static const gchar *hint_text
    = ("Note that most GTP engines support only one game. Hence certain "
       "game / computer player combinations may be not possible to play.");

  static const gchar *handicap_radio_button_labels[2] = { "Fi_xed handicap:",
							  "_Free handicap:" };

  NewGameDialogData *data = g_malloc(sizeof(NewGameDialogData));
  gint game_index = gtk_games_name_to_index(new_game_configuration.game_name,
					    FALSE);
  GtkWidget *assistant;
  GtkWidget *vbox1;
  GtkWidget **radio_buttons;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *named_vbox;
  GtkWidget *hboxes[2];
  GtkWidget *player_vboxes[NUM_COLORS];
  GtkWidget *button;
  GtkWidget *vbox2;
  GtkWidget *game_and_players_page;
  GtkWidget *notebook;
  GtkWidget *game_specific_rules[NUM_SUPPORTED_GAMES];
  GtkWidget *board_size_labels[NUM_SUPPORTED_GAMES];
  GtkWidget *board_size_spin_buttons[NUM_SUPPORTED_GAMES];
  GtkWidget *handicap_spin_buttons[NUM_SUPPORTED_GAMES];
  GtkWidget *komi_spin_button;
  GtkWidget *rules_page;
  GtkRadioButton *last_radio_button = NULL;
  int k;

  /* Create assistant dialog that will hold two pages. */
  assistant = gtk_assistant_new("New Game", data);
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
  label = gtk_label_new(hint_text);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);

  /* Pack the radio buttons and the hint next to each other. */
  hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
			       vbox1, GTK_UTILS_FILL,
			       label, GTK_UTILS_PACK_DEFAULT, NULL);

  /* And pack everything into a box with caption saying "Game". */
  named_vbox = gtk_named_vbox_new("Game", FALSE, QUARRY_SPACING_SMALL);
  gtk_box_pack_start_defaults(GTK_BOX(named_vbox), hbox);

  /* Create identical set of controls for each of the players. */
  for (k = 0; k < NUM_COLORS; k++) {
    static const char *radio_labels[NUM_COLORS][2]
      = { { "H_uman", "Compu_ter" }, { "_Human", "Com_puter" } };

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
    entry = gtk_utils_create_entry(new_game_configuration.player_names[k]);
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
				   ? "Black Player" : "White Player"));
  }

  /* Make sure the radio buttons line up nicely. */
  gtk_utils_create_size_group(GTK_SIZE_GROUP_HORIZONTAL,
			      data->player_radio_buttons[WHITE_INDEX][0],
			      data->player_radio_buttons[WHITE_INDEX][1],
			      data->player_radio_buttons[BLACK_INDEX][0],
			      data->player_radio_buttons[BLACK_INDEX][1],
			      vbox1, NULL);

  /* Button which opens "Preferences" dialog at "GTP Engines" page. */
  button = gtk_button_new_with_mnemonic("_Manage Engine List");
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
  button = gtk_button_new_with_mnemonic("_Swap");
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
			 GTK_STOCK_REFRESH, "Game &amp; Players", NULL, NULL);
  gtk_widget_show_all(game_and_players_page);

  /* "Game Rules" page. */

  notebook = gtk_notebook_new();
  data->notebook = GTK_NOTEBOOK(notebook);
  gtk_notebook_set_show_tabs(data->notebook, FALSE);
  gtk_notebook_set_show_border(data->notebook, FALSE);

  data->board_sizes[GTK_GAME_GO]
    = ((GtkAdjustment *)
       gtk_adjustment_new(new_game_configuration.go_board_size,
			  GTK_MIN_BOARD_SIZE, GTK_MAX_BOARD_SIZE, 1, 2, 0));
  data->board_sizes[GTK_GAME_AMAZONS]
    = ((GtkAdjustment *)
       gtk_adjustment_new(new_game_configuration.amazons_board_size,
			  GTK_MIN_BOARD_SIZE, GTK_MAX_BOARD_SIZE, 1, 2, 0));
  data->board_sizes[GTK_GAME_OTHELLO]
    = (GtkAdjustment *)
    gtk_adjustment_new(new_game_configuration.othello_board_size,
		       ROUND_UP(GTK_MIN_BOARD_SIZE, 2),
		       ROUND_DOWN(GTK_MAX_BOARD_SIZE, 2),
		       2, 4, 0);

  for (k = 0; k < NUM_SUPPORTED_GAMES; k++) {
    char named_vbox_title[64];

    sprintf(named_vbox_title, "%s Rules", game_info[index_to_game[k]].name);
    game_specific_rules[k] = gtk_named_vbox_new(named_vbox_title, FALSE,
						QUARRY_SPACING_SMALL);
    gtk_notebook_append_page(data->notebook, game_specific_rules[k], NULL);

    board_size_labels[k] = gtk_label_new_with_mnemonic("Board _size:");
    gtk_misc_set_alignment(GTK_MISC(board_size_labels[k]), 0.0, 0.5);

    board_size_spin_buttons[k]
      = gtk_utils_create_spin_button(data->board_sizes[k], 0.0, 0, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(board_size_labels[k]),
				  board_size_spin_buttons[k]);

    hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				 board_size_labels[k], 0,
				 board_size_spin_buttons[k], GTK_UTILS_FILL,
				 NULL);
    gtk_box_pack_start(GTK_BOX(game_specific_rules[k]), hbox, FALSE, TRUE, 0);
  }

  radio_buttons = (GtkWidget **) data->handicap_toggle_buttons;
  gtk_utils_create_radio_chain(radio_buttons, handicap_radio_button_labels, 2);
  if (!new_game_configuration.handicap_is_fixed)
    gtk_toggle_button_set_active(data->handicap_toggle_buttons[1], TRUE);

  for (k = 0; k < 2; k++) {
    data->handicaps[k]
      = ((GtkAdjustment *)
	 gtk_adjustment_new((k == 0
			     ? new_game_configuration.fixed_handicap
			     : new_game_configuration.free_handicap),
			    0, GTK_MAX_BOARD_SIZE * GTK_MAX_BOARD_SIZE,
			    1, 2, 0));

    handicap_spin_buttons[k] = gtk_utils_create_spin_button(data->handicaps[k],
							    0.0, 0, TRUE);
    gtk_utils_set_sensitive_on_toggle(data->handicap_toggle_buttons[k],
				      handicap_spin_buttons[k]);

    hboxes[k] = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				      radio_buttons[k], 0,
				      handicap_spin_buttons[k], GTK_UTILS_FILL,
				      NULL);
  }

  /* FIXME: Implement free handicap and remove this line. */
  gtk_widget_set_sensitive(hboxes[1], FALSE);

  vbox1 = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				hboxes[0], GTK_UTILS_FILL,
				hboxes[1], GTK_UTILS_FILL, NULL);
  gtk_box_pack_start(GTK_BOX(game_specific_rules[GTK_GAME_GO]),
		     vbox1, FALSE, TRUE, QUARRY_SPACING_SMALL);

  label = gtk_label_new_with_mnemonic("_Komi:");
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

  data->komi = ((GtkAdjustment *)
		gtk_adjustment_new(new_game_configuration.komi, -999.5, 999.5,
				   1.0, 5.0, 0.0));
  komi_spin_button = gtk_utils_create_spin_button(data->komi, 0.0, 1, FALSE);
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), komi_spin_button);

  hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
			       label, 0, komi_spin_button, GTK_UTILS_FILL,
			       NULL);
  gtk_box_pack_start(GTK_BOX(game_specific_rules[GTK_GAME_GO]), hbox,
		     FALSE, TRUE, 0);

  gtk_utils_create_size_group(GTK_SIZE_GROUP_HORIZONTAL,
			      board_size_labels[GTK_GAME_GO],
			      radio_buttons[0], radio_buttons[1], label, NULL);
  gtk_utils_create_size_group(GTK_SIZE_GROUP_HORIZONTAL,
			      board_size_spin_buttons[GTK_GAME_GO],
			      handicap_spin_buttons[0],
			      handicap_spin_buttons[1], komi_spin_button,
			      NULL);

  set_handicap_adjustment_limits(data->board_sizes[GTK_GAME_GO], data);
  g_signal_connect(data->board_sizes[GTK_GAME_GO], "value-changed",
		   G_CALLBACK(set_handicap_adjustment_limits), data);

  rules_page = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				     notebook, GTK_UTILS_FILL, NULL);
  gtk_assistant_add_page(data->assistant, rules_page,
			 GTK_STOCK_PREFERENCES, "Game Rules",
			 ((GtkAssistantPageShownCallback)
			  show_game_specific_rules),
			 ((GtkAssistantPageAcceptableCallback)
			  instantiate_players));
  gtk_widget_show_all(rules_page);

  if (game_index != -1)
    gtk_toggle_button_set_active(data->game_radio_buttons[game_index], TRUE);

  update_game_and_players_page(NULL, data);

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
  gtk_notebook_set_current_page(data->notebook, get_selected_game(data));
}


static void
set_handicap_adjustment_limits(GtkAdjustment *board_size_adjustment,
			       NewGameDialogData *data)
{
  gint board_size = gtk_adjustment_get_value(board_size_adjustment);
  int max_fixed_handicap = go_get_max_fixed_handicap(board_size, board_size);

  g_object_set(data->handicaps[0],
	       "upper", (gdouble) max_fixed_handicap, NULL);
  g_object_set(data->handicaps[1],
	       "upper", (gdouble) board_size * board_size, NULL);
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
  static const ConfigurationSection *new_game_dialog_section
    = &gtk_configuration_sections[SECTION_NEW_GAME_DIALOG];

  NewGameDialogData *data = (NewGameDialogData *) user_data;
  GtkGameIndex game_index = get_selected_game(data);
  Game game = index_to_game[game_index];
  int board_size = gtk_adjustment_get_value(data->board_sizes[game_index]);
  gboolean player_is_computer[NUM_COLORS];
  const char *human_names[NUM_COLORS];
  const char *engine_screen_names[NUM_COLORS];
  SgfGameTree *game_tree;
  SgfCollection *sgf_collection;
  GtkWidget *goban_window;
  int k;

  assert(status == ENGINES_INSTANTIATED);

  game_tree = sgf_game_tree_new_with_root(game, board_size, board_size, 1);

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
						      : human_names[k]));
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
    sgf_node_add_text_property(game_tree->current_node, game_tree, SGF_KOMI,
			       utils_duplicate_string(format_double(komi)));

    configuration_set_section_values(new_game_dialog_section,
				     NEW_GAME_DIALOG_GO_BOARD_SIZE,
				     board_size,
				     NEW_GAME_DIALOG_HANDICAP_IS_FIXED,
				     handicap_is_fixed,
				     (handicap_is_fixed
				      ? NEW_GAME_DIALOG_FIXED_HANDICAP
				      : NEW_GAME_DIALOG_FREE_HANDICAP),
				     handicap,
				     NEW_GAME_DIALOG_KOMI,
				     komi, -1);
  }
  else if (game == GAME_AMAZONS) {
    configuration_set_section_values(new_game_dialog_section,
				     NEW_GAME_DIALOG_AMAZONS_BOARD_SIZE,
				     board_size, -1);
  }
  else if (game == GAME_OTHELLO) {
    configuration_set_section_values(new_game_dialog_section,
				     NEW_GAME_DIALOG_OTHELLO_BOARD_SIZE,
				     board_size, -1);
  }

  sgf_collection = sgf_collection_new();
  sgf_collection_add_game_tree(sgf_collection, game_tree);

  configuration_set_section_values(new_game_dialog_section,
				   NEW_GAME_DIALOG_GAME,
				   game_info[game].name,
				   NEW_GAME_DIALOG_WHITE_PLAYER,
				   human_names[WHITE_INDEX],
				   NEW_GAME_DIALOG_WHITE_IS_COMPUTER,
				   player_is_computer[WHITE_INDEX],
				   NEW_GAME_DIALOG_ENGINE_WHITE,
				   engine_screen_names[WHITE_INDEX],
				   NEW_GAME_DIALOG_BLACK_PLAYER,
				   human_names[BLACK_INDEX],
				   NEW_GAME_DIALOG_BLACK_IS_COMPUTER,
				   player_is_computer[BLACK_INDEX],
				   NEW_GAME_DIALOG_ENGINE_BLACK,
				   engine_screen_names[BLACK_INDEX],
				   -1);

  gtk_widget_destroy(GTK_WIDGET(data->assistant));

  goban_window = gtk_goban_window_new(sgf_collection, NULL);
  gtk_goban_window_enter_game_mode(GTK_GOBAN_WINDOW(goban_window),
				   data->players[BLACK_INDEX],
				   data->players[WHITE_INDEX]);
  gtk_widget_show(goban_window);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
