/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004 Paul Pogonyshev.                             *
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

/* FIXME: Find a way to display text/nothing in spin buttons
 *	  (i.e. when handicap is not specified, handicap spin button
 *	  should display nothing; when time limit is incorrectly set
 *	  to "3h", then spin button should display "3h").
 */


#include "gtk-game-info-dialog.h"
#include "gtk-games.h"
#include "gtk-named-vbox.h"
#include "gtk-utils.h"
#include "sgf.h"
#include "board.h"

#include <assert.h>
#include <gtk/gtk.h>


static void	gtk_game_info_dialog_class_init(GtkGameInfoDialogClass *class);
static void	gtk_game_info_dialog_init(GtkGameInfoDialog *dialog);

static void	gtk_game_info_dialog_response(GtkDialog *dialog,
					      gint response_id);


GtkType
gtk_game_info_dialog_get_type(void)
{
  static GtkType game_info_dialog_type = 0;

  if (!game_info_dialog_type) {
    static GTypeInfo game_info_dialog_info = {
      sizeof(GtkGameInfoDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_game_info_dialog_class_init,
      NULL,
      NULL,
      sizeof(GtkGameInfoDialog),
      1,
      (GInstanceInitFunc) gtk_game_info_dialog_init,
      NULL
    };

    game_info_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
						   "GtkGameInfoDialog",
						   &game_info_dialog_info, 0);
  }

  return game_info_dialog_type;
}


static void
gtk_game_info_dialog_class_init(GtkGameInfoDialogClass *class)
{
  GTK_DIALOG_CLASS(class)->response = gtk_game_info_dialog_response;
}


static void
gtk_game_info_dialog_init(GtkGameInfoDialog *dialog)
{
  GtkWidget *notebook_widget = gtk_notebook_new();
  GtkNotebook *notebook = GTK_NOTEBOOK(notebook_widget);
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *handicap_spin_button;
  GtkWidget *komi_spin_button;
  GtkWidget *time_spin_button;
  GtkWidget *hbox1;
  GtkWidget *hbox2;
  GtkWidget *hbox3;
  GtkWidget *hbox4;
  GtkWidget *hbox5;
  GtkWidget *large_hbox1;
  GtkWidget *large_hbox2;
  GtkWidget *named_vbox1;
  GtkWidget *named_vbox2;
  GtkWidget *player_vboxes[NUM_COLORS];
  GtkWidget *vbox;
  GtkWidget *alignment;
  GtkWidget *text_view;
  GtkWidget *scrolled_window;
  GtkWidget *page;
  GtkSizeGroup *size_group;
  int k;

  /* FIXME: Game name in title (if present). */
  gtk_window_set_title(GTK_WINDOW(dialog),
		       "Game Information (viewing only for now)");

  for (k = 0; k < NUM_COLORS; k++) {
    entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
    dialog->player_names[k] = GTK_ENTRY(entry);

    label = gtk_utils_create_mnemonic_label((k == BLACK_INDEX
					     ? _("N_ame:") : _("_Name:")),
					    entry);

    hbox1 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				  label, 0, entry, GTK_UTILS_PACK_DEFAULT,
				  NULL);

    entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
    dialog->player_teams[k] = GTK_ENTRY(entry);

    label = gtk_utils_create_mnemonic_label(_("Team:"), entry);

    hbox2 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				  label, 0, entry, GTK_UTILS_PACK_DEFAULT,
				  NULL);

    entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
    dialog->player_ranks[k] = GTK_ENTRY(entry);

    gtk_entry_set_width_chars(GTK_ENTRY(entry), 10);
    label = gtk_utils_create_mnemonic_label(_("Rank:"), entry);

    hbox3 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				  label, 0, entry, 0, NULL);

    player_vboxes[k] = gtk_utils_pack_in_box(GTK_TYPE_NAMED_VBOX,
					     QUARRY_SPACING_SMALL,
					     hbox1, GTK_UTILS_FILL,
					     hbox2, GTK_UTILS_FILL,
					     hbox3, GTK_UTILS_FILL, NULL);
    gtk_named_vbox_set_label_text(GTK_NAMED_VBOX(player_vboxes[k]),
				  (k == BLACK_INDEX
				   ? _("Black Player") : _("White Player")));
  }

  large_hbox1 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				      player_vboxes[1], GTK_UTILS_PACK_DEFAULT,
				      player_vboxes[0], GTK_UTILS_PACK_DEFAULT,
				      NULL);
  gtk_box_set_homogeneous(GTK_BOX(large_hbox1), TRUE);
  size_group = gtk_utils_align_left_widgets(GTK_CONTAINER(large_hbox1), NULL);

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->game_name = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("_Game name:"), entry);

  hbox1 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->place = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("_Place:"), entry);

  hbox2 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->date = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("_Date:"), entry);

  hbox3 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->event = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("_Event:"), entry);

  hbox4 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->round = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("_Round:"), entry);

  hbox5 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  named_vbox1 = gtk_utils_pack_in_box(GTK_TYPE_NAMED_VBOX,
				      QUARRY_SPACING_SMALL,
				      hbox1, GTK_UTILS_FILL,
				      hbox2, GTK_UTILS_FILL,
				      hbox3, GTK_UTILS_FILL,
				      hbox4, GTK_UTILS_FILL,
				      hbox5, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text(GTK_NAMED_VBOX(named_vbox1),
				_("Game Information"));

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->rule_set = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("Rule _set:"), entry);

  hbox1 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  dialog->handicap =
    ((GtkAdjustment *)
     gtk_adjustment_new(0, 0, GTK_MAX_BOARD_SIZE * GTK_MAX_BOARD_SIZE,
			1, 2, 0));
  handicap_spin_button = gtk_utils_create_spin_button(dialog->handicap,
						      0.0, 0, TRUE);

  label = gtk_utils_create_mnemonic_label(_("_Handicap:"),
					  handicap_spin_button);

  hbox2 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, handicap_spin_button, GTK_UTILS_FILL,
				NULL);
  dialog->handicap_box = hbox2;

  dialog->komi = ((GtkAdjustment *)
		  gtk_adjustment_new(0.0, -999.5, 999.5, 1.0, 5.0, 0.0));
  komi_spin_button = gtk_utils_create_spin_button(dialog->komi, 0.0, 1, FALSE);

  label = gtk_utils_create_mnemonic_label(_("_Komi:"), komi_spin_button);

  hbox3 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, komi_spin_button, GTK_UTILS_FILL,
				NULL);
  dialog->komi_box = hbox3;

  dialog->main_time = ((GtkAdjustment *)
		       gtk_adjustment_new(0.0, 0.0, 3600000.0 - 1.0,
					  60.0, 300.0, 0.0));
  time_spin_button = gtk_utils_create_time_spin_button(dialog->main_time, 0.0);

  label = gtk_utils_create_mnemonic_label(_("_Main time:"), time_spin_button);

  hbox4 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, time_spin_button, GTK_UTILS_FILL,
				NULL);

  gtk_utils_create_size_group(GTK_SIZE_GROUP_HORIZONTAL,
			      handicap_spin_button, komi_spin_button,
			      time_spin_button, NULL);

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->overtime = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("_Overtime:"), entry);

  hbox5 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  named_vbox2 = gtk_utils_pack_in_box(GTK_TYPE_NAMED_VBOX,
				      QUARRY_SPACING_SMALL,
				      hbox1, GTK_UTILS_FILL,
				      hbox2, GTK_UTILS_FILL,
				      hbox3, GTK_UTILS_FILL,
				      hbox4, GTK_UTILS_FILL,
				      hbox5, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text(GTK_NAMED_VBOX(named_vbox2),
				_("Game Rules"));

  large_hbox2 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				      named_vbox1, GTK_UTILS_PACK_DEFAULT,
				      named_vbox2, GTK_UTILS_PACK_DEFAULT,
				      NULL);
  gtk_box_set_homogeneous(GTK_BOX(large_hbox2), TRUE);
  gtk_utils_align_left_widgets(GTK_CONTAINER(large_hbox2), size_group);

  page = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
			       large_hbox1, GTK_UTILS_FILL,
			       large_hbox2, GTK_UTILS_FILL, NULL);
  gtk_container_set_border_width(GTK_CONTAINER(page), QUARRY_SPACING);
  gtk_widget_show_all(page);

  gtk_notebook_append_page(notebook, page, gtk_label_new(_("General")));

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->result = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label("Game _result:", entry);

  hbox1 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->opening = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label("_Opening played:", entry);

  hbox2 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  vbox = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
			       hbox1, GTK_UTILS_FILL, hbox2, GTK_UTILS_FILL,
			       NULL);
  gtk_utils_align_left_widgets(GTK_CONTAINER(vbox), NULL);

  alignment = gtk_alignment_new(0.0, 0.0, 0.4, 1.0);
  gtk_container_add(GTK_CONTAINER(alignment), vbox);

  text_view = gtk_text_view_new();
  dialog->game_comment = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view),
				QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view),
				 QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);

  scrolled_window = gtk_utils_make_widget_scrollable(text_view,
						     GTK_POLICY_AUTOMATIC,
						     GTK_POLICY_AUTOMATIC);

  label = gtk_utils_create_mnemonic_label(_("Comment / _description:"),
					  scrolled_window);

  vbox = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
			       label, GTK_UTILS_FILL,
			       scrolled_window, GTK_UTILS_PACK_DEFAULT, NULL);

  page = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
			       alignment, GTK_UTILS_FILL,
			       vbox, GTK_UTILS_PACK_DEFAULT, NULL);
  gtk_container_set_border_width(GTK_CONTAINER(page), QUARRY_SPACING);
  gtk_widget_show_all(page);

  gtk_notebook_append_page(notebook, page,
			   gtk_label_new(_("Description & Result")));

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->copyright = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("Co_pyright string:"), entry);

  hbox1 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->annotator = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("_Annotator:"), entry);

  hbox2 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->source = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("_Source:"), entry);

  hbox3 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  entry = gtk_utils_create_entry(NULL, RETURN_ADVANCES_FOCUS);
  dialog->user = GTK_ENTRY(entry);

  label = gtk_utils_create_mnemonic_label(_("_Entered by:"), entry);

  hbox4 = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, entry, GTK_UTILS_PACK_DEFAULT, NULL);

  named_vbox1 = gtk_utils_pack_in_box(GTK_TYPE_NAMED_VBOX,
				      QUARRY_SPACING_SMALL,
				      hbox1, GTK_UTILS_FILL,
				      hbox2, GTK_UTILS_FILL,
				      hbox3, GTK_UTILS_FILL,
				      hbox4, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text(GTK_NAMED_VBOX(named_vbox1),
				_("Copyright & Credits"));

  alignment = gtk_alignment_new(0.0, 0.0, 0.4, 1.0);
  gtk_container_add(GTK_CONTAINER(alignment), named_vbox1);

  page = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
			       alignment, GTK_UTILS_FILL,
			       /* named_vbox2, GTK_UTILS_FILL,  */NULL);
  gtk_container_set_border_width(GTK_CONTAINER(page), QUARRY_SPACING);
  gtk_utils_align_left_widgets(GTK_CONTAINER(page), NULL);
  gtk_widget_show_all(page);

  gtk_notebook_append_page(notebook, page, gtk_label_new(_("Game Record")));

  gtk_widget_show(notebook_widget);
  gtk_utils_standardize_dialog(&dialog->dialog, notebook_widget);
  gtk_dialog_set_has_separator(&dialog->dialog, FALSE);

  gtk_dialog_add_button(&dialog->dialog, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

  dialog->sgf_tree = NULL;
  dialog->sgf_node = NULL;
}


GtkWidget *
gtk_game_info_dialog_new(void)
{
  return GTK_WIDGET(g_object_new(GTK_TYPE_GAME_INFO_DIALOG, NULL));
}


static void
gtk_game_info_dialog_response(GtkDialog *dialog, gint response_id)
{
  if (response_id == GTK_RESPONSE_CLOSE)
    gtk_widget_destroy(GTK_WIDGET(dialog));
}


void
gtk_game_info_dialog_set_node(GtkGameInfoDialog *dialog,
			      SgfGameTree *sgf_tree, SgfNode *sgf_node)
{
  const char *text;
  int k;

  assert(GTK_IS_GAME_INFO_DIALOG(dialog));
  assert(sgf_tree);
  assert(sgf_node);

  dialog->sgf_tree = sgf_tree;
  dialog->sgf_node = sgf_node;

  /* Initializing "General" page. */

  for (k = 0; k < NUM_COLORS; k++) {
    const char *text;

    text = sgf_node_get_text_property_value(sgf_node, (k == BLACK_INDEX
						       ? SGF_PLAYER_BLACK
						       : SGF_PLAYER_WHITE));
    gtk_entry_set_text(dialog->player_names[k], text ? text : "");

    text = sgf_node_get_text_property_value(sgf_node, (k == BLACK_INDEX
						       ? SGF_BLACK_TEAM
						       : SGF_WHITE_TEAM));
    gtk_entry_set_text(dialog->player_teams[k], text ? text : "");

    text = sgf_node_get_text_property_value(sgf_node, (k == BLACK_INDEX
						       ? SGF_BLACK_RANK
						       : SGF_WHITE_RANK));
    gtk_entry_set_text(dialog->player_ranks[k], text ? text : "");
  }

  text = sgf_node_get_text_property_value(sgf_node, SGF_GAME_NAME);
  gtk_entry_set_text(dialog->game_name, text ? text : "");

  text = sgf_node_get_text_property_value(sgf_node, SGF_PLACE);
  gtk_entry_set_text(dialog->place, text ? text : "");

  text = sgf_node_get_text_property_value(sgf_node, SGF_DATE);
  gtk_entry_set_text(dialog->date, text ? text : "");

  text = sgf_node_get_text_property_value(sgf_node, SGF_EVENT);
  gtk_entry_set_text(dialog->event, text ? text : "");

  text = sgf_node_get_text_property_value(sgf_node, SGF_ROUND);
  gtk_entry_set_text(dialog->round, text ? text : "");

  text = sgf_node_get_text_property_value(sgf_node, SGF_RULE_SET);
  gtk_entry_set_text(dialog->rule_set, text ? text : "");

  if (sgf_tree->game == GAME_GO) {
    int handicap = sgf_node_get_handicap(sgf_node);
    double komi;
    double time_limit;

    dialog->handicap->upper =
      (gdouble) (sgf_tree->board_width * sgf_tree->board_height - 1);
    gtk_adjustment_changed(dialog->handicap);

    if (handicap >= 0)
      gtk_adjustment_set_value(dialog->handicap, (gdouble) handicap);

    if (sgf_node_get_komi(sgf_node, &komi))
      gtk_adjustment_set_value(dialog->komi, komi);

    if (sgf_node_get_time_limit(sgf_node, &time_limit))
      gtk_adjustment_set_value(dialog->main_time, time_limit);

    gtk_widget_set_sensitive(dialog->handicap_box, TRUE);
    gtk_widget_set_sensitive(dialog->komi_box, TRUE);
  }
  else {
    gtk_widget_set_sensitive(dialog->handicap_box, FALSE);
    gtk_widget_set_sensitive(dialog->komi_box, FALSE);
  }

  text = sgf_node_get_text_property_value(sgf_node, SGF_OVERTIME);
  gtk_entry_set_text(dialog->overtime, text ? text : "");

  /* Initializing "Description & Result" page. */

  text = sgf_node_get_text_property_value(sgf_node, SGF_RESULT);
  gtk_entry_set_text(dialog->result, text ? text : "");

  text = sgf_node_get_text_property_value(sgf_node, SGF_OPENING);
  gtk_entry_set_text(dialog->opening, text ? text : "");

  text= sgf_node_get_text_property_value(sgf_node, SGF_GAME_COMMENT);
  if (text) {
    GtkTextIter start_iterator;
    GtkTextIter end_iterator;

    gtk_text_buffer_set_text(dialog->game_comment, text, -1);

    /* Add newline at the end of the comment, to ease editing. */
    gtk_text_buffer_get_bounds(dialog->game_comment,
			       &start_iterator, &end_iterator);
    gtk_text_buffer_place_cursor(dialog->game_comment, &start_iterator);
    gtk_text_buffer_insert(dialog->game_comment, &end_iterator, "\n", 1);

  }
  else
    gtk_text_buffer_set_text(dialog->game_comment, "", 0);

  /* Initializing "Game Record" page. */

  text = sgf_node_get_text_property_value(sgf_node, SGF_COPYRIGHT);
  gtk_entry_set_text(dialog->copyright, text ? text : "");

  text = sgf_node_get_text_property_value(sgf_node, SGF_ANNOTATOR);
  gtk_entry_set_text(dialog->annotator, text ? text : "");

  text = sgf_node_get_text_property_value(sgf_node, SGF_SOURCE);
  gtk_entry_set_text(dialog->source, text ? text : "");

  text = sgf_node_get_text_property_value(sgf_node, SGF_USER);
  gtk_entry_set_text(dialog->user, text ? text : "");
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
