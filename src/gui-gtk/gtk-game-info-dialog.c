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


#include "gtk-freezable-spin-button.h"
#include "gtk-game-info-dialog.h"
#include "gtk-games.h"
#include "gtk-named-vbox.h"
#include "gtk-utils.h"
#include "quarry-marshal.h"
#include "sgf.h"
#include "board.h"
#include "utils.h"

#include <assert.h>
#include <string.h>
#include <gtk/gtk.h>


static void	   gtk_game_info_dialog_class_init
		     (GtkGameInfoDialogClass *class);
static void	   gtk_game_info_dialog_init (GtkGameInfoDialog *dialog);

static GtkEntry *  create_and_pack_game_info_entry (const gchar *label_text,
						    SgfType sgf_property_type,
						    GtkWidget **hbox);

static void	   gtk_game_info_dialog_response (GtkDialog *dialog,
						  gint response_id);


static gboolean	   game_info_entry_focus_out_event
		     (GtkEntry *entry, GdkEventFocus *event,
		      gpointer sgf_property_type);
static gboolean	   game_comment_focus_out_event (GtkGameInfoDialog *dialog,
						 GdkEventFocus *event);
static void	   game_info_spin_button_value_changed
		     (GtkFreezableSpinButton *freezable_spin_button,
		      GtkGameInfoDialog *dialog);
static gboolean	   game_info_spin_button_focus_out_event
		     (GtkFreezableSpinButton *freezable_spin_button,
		      GdkEventFocus *event, GtkGameInfoDialog *dialog);

static void	   update_property_if_changed (GtkGameInfoDialog *dialog,
					       SgfType sgf_property_type,
					       char *new_value);


enum {
  PROPERTY_CHANGED,
  NUM_SIGNALS
};

static guint	game_info_dialog_signals[NUM_SIGNALS];


GtkType
gtk_game_info_dialog_get_type (void)
{
  static GtkType game_info_dialog_type = 0;

  if (!game_info_dialog_type) {
    static GTypeInfo game_info_dialog_info = {
      sizeof (GtkGameInfoDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_game_info_dialog_class_init,
      NULL,
      NULL,
      sizeof (GtkGameInfoDialog),
      1,
      (GInstanceInitFunc) gtk_game_info_dialog_init,
      NULL
    };

    game_info_dialog_type = g_type_register_static (GTK_TYPE_DIALOG,
						    "GtkGameInfoDialog",
						    &game_info_dialog_info, 0);
  }

  return game_info_dialog_type;
}


static void
gtk_game_info_dialog_class_init (GtkGameInfoDialogClass *class)
{
  GTK_DIALOG_CLASS (class)->response = gtk_game_info_dialog_response;

  game_info_dialog_signals[PROPERTY_CHANGED]
    = g_signal_new ("property-changed",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkGameInfoDialogClass, property_changed),
		    NULL, NULL,
		    quarry_marshal_VOID__INT,
		    G_TYPE_NONE, 1, G_TYPE_INT);
}


static void
gtk_game_info_dialog_init (GtkGameInfoDialog *dialog)
{
  GtkWidget *notebook_widget = gtk_notebook_new ();
  GtkNotebook *notebook = GTK_NOTEBOOK (notebook_widget);
  GtkWidget *label;
  GtkWidget *handicap_spin_button;
  GtkWidget *komi_spin_button;
  GtkWidget *main_time_spin_button;
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
  gtk_window_set_title (GTK_WINDOW (dialog), _("Game Information"));

  for (k = 0; k < NUM_COLORS; k++) {
    dialog->player_names[k]
      = create_and_pack_game_info_entry ((k == BLACK_INDEX
					  ? _("N_ame:") : _("_Name:")),
					 (k == BLACK_INDEX
					  ? SGF_PLAYER_BLACK
					  : SGF_PLAYER_WHITE),
					 &hbox1);

    dialog->player_teams[k]
      = create_and_pack_game_info_entry (_("Team:"),
					 (k == BLACK_INDEX
					  ? SGF_BLACK_TEAM : SGF_WHITE_TEAM),
					 &hbox2);

    dialog->player_ranks[k]
      = create_and_pack_game_info_entry (_("Rank:"),
					 (k == BLACK_INDEX
					  ? SGF_BLACK_RANK : SGF_WHITE_RANK),
					 &hbox3);

    player_vboxes[k] = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
					      QUARRY_SPACING_SMALL,
					      hbox1, GTK_UTILS_FILL,
					      hbox2, GTK_UTILS_FILL,
					      hbox3, GTK_UTILS_FILL, NULL);
    gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (player_vboxes[k]),
				   (k == BLACK_INDEX
				    ? _("Black Player") : _("White Player")));
  }

  large_hbox1 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				       player_vboxes[1],
				       GTK_UTILS_PACK_DEFAULT,
				       player_vboxes[0],
				       GTK_UTILS_PACK_DEFAULT,
				       NULL);
  gtk_box_set_homogeneous (GTK_BOX (large_hbox1), TRUE);
  size_group = gtk_utils_align_left_widgets (GTK_CONTAINER (large_hbox1),
					     NULL);

  dialog->game_name
    = create_and_pack_game_info_entry (_("_Game name:"), SGF_GAME_NAME,
				       &hbox1);
  dialog->place
    = create_and_pack_game_info_entry (_("_Place:"), SGF_PLACE, &hbox2);

  dialog->date
    = create_and_pack_game_info_entry (_("Da_te:"), SGF_DATE, &hbox3);

  dialog->event
    = create_and_pack_game_info_entry (_("_Event:"), SGF_EVENT, &hbox4);

  dialog->round
    = create_and_pack_game_info_entry (_("_Round:"), SGF_ROUND, &hbox5);

  named_vbox1 = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
				       QUARRY_SPACING_SMALL,
				       hbox1, GTK_UTILS_FILL,
				       hbox2, GTK_UTILS_FILL,
				       hbox3, GTK_UTILS_FILL,
				       hbox4, GTK_UTILS_FILL,
				       hbox5, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (named_vbox1),
				 _("Game Information"));

  dialog->rule_set
    = create_and_pack_game_info_entry (_("Rule _set:"), SGF_RULE_SET, &hbox1);

  dialog->handicap =
    ((GtkAdjustment *)
     gtk_adjustment_new (0, 0, GTK_MAX_BOARD_SIZE * GTK_MAX_BOARD_SIZE,
			 1, 2, 0));
  handicap_spin_button
    = gtk_utils_create_freezable_spin_button (dialog->handicap, 0.0, 0, TRUE);
  dialog->handicap_spin_button
    = GTK_FREEZABLE_SPIN_BUTTON (handicap_spin_button);
  gtk_utils_freeze_on_empty_input (dialog->handicap_spin_button);

  g_signal_connect (handicap_spin_button, "value-changed",
		    G_CALLBACK (game_info_spin_button_value_changed), dialog);

  /* Note: This *must* be `_after' so that `input' signal is sent
   * before our handler is invoked (that signal's handler can set the
   * freezing string.)
   */
  g_signal_connect_after (handicap_spin_button, "focus-out-event",
			  G_CALLBACK (game_info_spin_button_focus_out_event),
			  dialog);

  label = gtk_utils_create_mnemonic_label (_("Han_dicap:"),
					   handicap_spin_button);

  hbox2 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				 label, 0,
				 handicap_spin_button, GTK_UTILS_FILL, NULL);
  dialog->handicap_box = hbox2;

  dialog->komi = ((GtkAdjustment *)
		  gtk_adjustment_new (0.0, -999.5, 999.5, 1.0, 5.0, 0.0));
  komi_spin_button
    = gtk_utils_create_freezable_spin_button (dialog->komi, 0.0, 1, FALSE);
  dialog->komi_spin_button = GTK_FREEZABLE_SPIN_BUTTON (komi_spin_button);
  gtk_utils_freeze_on_empty_input (dialog->komi_spin_button);

  g_signal_connect (komi_spin_button, "value-changed",
		    G_CALLBACK (game_info_spin_button_value_changed), dialog);
  g_signal_connect_after (komi_spin_button, "focus-out-event",
			  G_CALLBACK (game_info_spin_button_focus_out_event),
			  dialog);

  label = gtk_utils_create_mnemonic_label (_("_Komi:"), komi_spin_button);

  hbox3 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				 label, 0, komi_spin_button, GTK_UTILS_FILL,
				 NULL);
  dialog->komi_box = hbox3;

  dialog->main_time = ((GtkAdjustment *)
		       gtk_adjustment_new (0.0, 0.0, 3600000.0 - 1.0,
					   60.0, 300.0, 0.0));
  main_time_spin_button
    = gtk_utils_create_freezable_spin_button (dialog->main_time, 0.0,
					      0, FALSE);
  dialog->main_time_spin_button
    = GTK_FREEZABLE_SPIN_BUTTON (main_time_spin_button);
  gtk_utils_freeze_on_empty_input (dialog->main_time_spin_button);
  gtk_utils_convert_to_time_spin_button
    (GTK_SPIN_BUTTON (main_time_spin_button));

  g_signal_connect (main_time_spin_button, "value-changed",
		    G_CALLBACK (game_info_spin_button_value_changed), dialog);
  g_signal_connect_after (main_time_spin_button, "focus-out-event",
			  G_CALLBACK (game_info_spin_button_focus_out_event),
			  dialog);

  label = gtk_utils_create_mnemonic_label (_("_Main time:"),
					   main_time_spin_button);

  hbox4 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				 label, 0,
				 main_time_spin_button, GTK_UTILS_FILL, NULL);

  gtk_utils_create_size_group (GTK_SIZE_GROUP_HORIZONTAL,
			       handicap_spin_button, komi_spin_button,
			       main_time_spin_button, NULL);

  dialog->overtime
    = create_and_pack_game_info_entry (_("_Overtime:"), SGF_OVERTIME, &hbox5);

  named_vbox2 = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
				       QUARRY_SPACING_SMALL,
				       hbox1, GTK_UTILS_FILL,
				       hbox2, GTK_UTILS_FILL,
				       hbox3, GTK_UTILS_FILL,
				       hbox4, GTK_UTILS_FILL,
				       hbox5, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (named_vbox2),
				 _("Game Rules"));

  large_hbox2 = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				       named_vbox1, GTK_UTILS_PACK_DEFAULT,
				       named_vbox2, GTK_UTILS_PACK_DEFAULT,
				       NULL);
  gtk_box_set_homogeneous (GTK_BOX (large_hbox2), TRUE);
  gtk_utils_align_left_widgets (GTK_CONTAINER (large_hbox2), size_group);

  page = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				large_hbox1, GTK_UTILS_FILL,
				large_hbox2, GTK_UTILS_FILL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (page), QUARRY_SPACING);
  gtk_widget_show_all (page);

  gtk_notebook_append_page (notebook, page, gtk_label_new (_("General")));

  dialog->result
    = create_and_pack_game_info_entry (_("Game _result:"), SGF_RESULT, &hbox1);

  dialog->opening
    = create_and_pack_game_info_entry (_("_Opening played:"), SGF_OPENING,
				       &hbox2);

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				hbox1, GTK_UTILS_FILL, hbox2, GTK_UTILS_FILL,
				NULL);
  gtk_utils_align_left_widgets (GTK_CONTAINER (vbox), NULL);

  alignment = gtk_alignment_new (0.0, 0.0, 0.4, 1.0);
  gtk_container_add (GTK_CONTAINER (alignment), vbox);

  text_view = gtk_text_view_new ();
  dialog->game_comment = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text_view),
				 QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (text_view),
				  QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), GTK_WRAP_WORD);

  g_signal_connect_swapped (text_view, "focus-out-event",
			    G_CALLBACK (game_comment_focus_out_event), dialog);

  scrolled_window = gtk_utils_make_widget_scrollable (text_view,
						      GTK_POLICY_AUTOMATIC,
						      GTK_POLICY_AUTOMATIC);

  label = gtk_utils_create_mnemonic_label (_("Comment / _description:"),
					   scrolled_window);

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				label, GTK_UTILS_FILL,
				scrolled_window, GTK_UTILS_PACK_DEFAULT, NULL);

  page = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				alignment, GTK_UTILS_FILL,
				vbox, GTK_UTILS_PACK_DEFAULT, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (page), QUARRY_SPACING);
  gtk_widget_show_all (page);

  gtk_notebook_append_page (notebook, page,
			    gtk_label_new (_("Description & Result")));

  dialog->copyright
    = create_and_pack_game_info_entry (_("Co_pyright string:"), SGF_COPYRIGHT,
				       &hbox1);

  dialog->annotator
    = create_and_pack_game_info_entry (_("_Annotator:"), SGF_ANNOTATOR,
				       &hbox2);

  dialog->source
    = create_and_pack_game_info_entry (_("_Source:"), SGF_SOURCE, &hbox3);

  dialog->user
    = create_and_pack_game_info_entry (_("_Entered by:"), SGF_USER, &hbox4);

  named_vbox1 = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
				       QUARRY_SPACING_SMALL,
				       hbox1, GTK_UTILS_FILL,
				       hbox2, GTK_UTILS_FILL,
				       hbox3, GTK_UTILS_FILL,
				       hbox4, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (named_vbox1),
				 _("Copyright & Credits"));

  alignment = gtk_alignment_new (0.0, 0.0, 0.4, 1.0);
  gtk_container_add (GTK_CONTAINER (alignment), named_vbox1);

  /* FIXME: Add second box with application etc. */
  page = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				alignment, GTK_UTILS_FILL,
				/* named_vbox2, GTK_UTILS_FILL, */ NULL);
  gtk_container_set_border_width (GTK_CONTAINER (page), QUARRY_SPACING);
  gtk_utils_align_left_widgets (GTK_CONTAINER (page), NULL);
  gtk_widget_show_all (page);

  gtk_notebook_append_page (notebook, page, gtk_label_new (_("Game Record")));

  gtk_widget_show (notebook_widget);
  gtk_utils_standardize_dialog (&dialog->dialog, notebook_widget);
  gtk_dialog_set_has_separator (&dialog->dialog, FALSE);

  gtk_dialog_add_button (&dialog->dialog, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

  dialog->sgf_tree = NULL;
  dialog->sgf_node = NULL;
}


static GtkEntry *
create_and_pack_game_info_entry (const gchar *label_text,
				 SgfType sgf_property_type, GtkWidget **hbox)
{
  GtkWidget *entry = gtk_utils_create_entry (NULL, RETURN_ADVANCES_FOCUS);
  GtkWidget *label = gtk_utils_create_mnemonic_label (label_text, entry);

  *hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				 label, 0, entry, GTK_UTILS_PACK_DEFAULT,
				 NULL);

  g_signal_connect (entry, "focus-out-event",
		    G_CALLBACK (game_info_entry_focus_out_event),
		    GINT_TO_POINTER (sgf_property_type));

  return GTK_ENTRY (entry);
}


GtkWidget *
gtk_game_info_dialog_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_GAME_INFO_DIALOG, NULL));
}


static void
gtk_game_info_dialog_response (GtkDialog *dialog, gint response_id)
{
  if (response_id == GTK_RESPONSE_CLOSE)
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


void
gtk_game_info_dialog_set_node (GtkGameInfoDialog *dialog,
			       SgfGameTree *sgf_tree, SgfNode *sgf_node)
{
  const char *text;
  int k;
  double time_limit;

  assert (GTK_IS_GAME_INFO_DIALOG (dialog));
  assert (sgf_tree);
  assert (sgf_node);

  dialog->sgf_tree = sgf_tree;
  dialog->sgf_node = sgf_node;

  /* Initializing "General" page. */

  for (k = 0; k < NUM_COLORS; k++) {
    const char *text;

    text = sgf_node_get_text_property_value (sgf_node, (k == BLACK_INDEX
							? SGF_PLAYER_BLACK
							: SGF_PLAYER_WHITE));
    gtk_entry_set_text (dialog->player_names[k], text ? text : "");

    text = sgf_node_get_text_property_value (sgf_node, (k == BLACK_INDEX
							? SGF_BLACK_TEAM
							: SGF_WHITE_TEAM));
    gtk_entry_set_text (dialog->player_teams[k], text ? text : "");

    text = sgf_node_get_text_property_value (sgf_node, (k == BLACK_INDEX
							? SGF_BLACK_RANK
							: SGF_WHITE_RANK));
    gtk_entry_set_text (dialog->player_ranks[k], text ? text : "");
  }

  text = sgf_node_get_text_property_value (sgf_node, SGF_GAME_NAME);
  gtk_entry_set_text (dialog->game_name, text ? text : "");

  text = sgf_node_get_text_property_value (sgf_node, SGF_PLACE);
  gtk_entry_set_text (dialog->place, text ? text : "");

  text = sgf_node_get_text_property_value (sgf_node, SGF_DATE);
  gtk_entry_set_text (dialog->date, text ? text : "");

  text = sgf_node_get_text_property_value (sgf_node, SGF_EVENT);
  gtk_entry_set_text (dialog->event, text ? text : "");

  text = sgf_node_get_text_property_value (sgf_node, SGF_ROUND);
  gtk_entry_set_text (dialog->round, text ? text : "");

  text = sgf_node_get_text_property_value (sgf_node, SGF_RULE_SET);
  gtk_entry_set_text (dialog->rule_set, text ? text : "");

  if (sgf_tree->game == GAME_GO) {
    int handicap = sgf_node_get_handicap (sgf_node);
    double komi;

    dialog->handicap->upper =
      (gdouble) (sgf_tree->board_width * sgf_tree->board_height - 1);
    gtk_adjustment_changed (dialog->handicap);

    if (handicap >= 0)
      gtk_adjustment_set_value (dialog->handicap, (gdouble) handicap);
    else {
      const char *handicap_as_string
	= sgf_node_get_text_property_value (sgf_node, SGF_HANDICAP);

      gtk_freezable_spin_button_freeze (dialog->handicap_spin_button,
					(handicap_as_string
					 ? handicap_as_string : ""));
    }

    if (sgf_node_get_komi (sgf_node, &komi))
      gtk_adjustment_set_value (dialog->komi, komi);
    else {
      const char *komi_as_string = sgf_node_get_text_property_value (sgf_node,
								     SGF_KOMI);

      gtk_freezable_spin_button_freeze (dialog->komi_spin_button,
					komi_as_string ? komi_as_string : "");
    }

    gtk_widget_set_sensitive (dialog->handicap_box, TRUE);
    gtk_widget_set_sensitive (dialog->komi_box, TRUE);
  }
  else {
    gtk_widget_set_sensitive (dialog->handicap_box, FALSE);
    gtk_widget_set_sensitive (dialog->komi_box, FALSE);
  }

  if (sgf_node_get_time_limit (sgf_node, &time_limit))
    gtk_adjustment_set_value (dialog->main_time, time_limit);
  else {
    const char *main_time_as_string
      = sgf_node_get_text_property_value (sgf_node, SGF_TIME_LIMIT);

    gtk_freezable_spin_button_freeze (dialog->main_time_spin_button,
				      (main_time_as_string
				       ? main_time_as_string : ""));
  }

  text = sgf_node_get_text_property_value (sgf_node, SGF_OVERTIME);
  gtk_entry_set_text (dialog->overtime, text ? text : "");

  /* Initializing "Description & Result" page. */

  text = sgf_node_get_text_property_value (sgf_node, SGF_RESULT);
  gtk_entry_set_text (dialog->result, text ? text : "");

  text = sgf_node_get_text_property_value (sgf_node, SGF_OPENING);
  gtk_entry_set_text (dialog->opening, text ? text : "");

  text = sgf_node_get_text_property_value (sgf_node, SGF_GAME_COMMENT);
  gtk_utils_set_text_buffer_text (dialog->game_comment, text);

  gtk_text_buffer_set_modified (dialog->game_comment, FALSE);

  /* Initializing "Game Record" page. */

  text = sgf_node_get_text_property_value (sgf_node, SGF_COPYRIGHT);
  gtk_entry_set_text (dialog->copyright, text ? text : "");

  text = sgf_node_get_text_property_value (sgf_node, SGF_ANNOTATOR);
  gtk_entry_set_text (dialog->annotator, text ? text : "");

  text = sgf_node_get_text_property_value (sgf_node, SGF_SOURCE);
  gtk_entry_set_text (dialog->source, text ? text : "");

  text = sgf_node_get_text_property_value (sgf_node, SGF_USER);
  gtk_entry_set_text (dialog->user, text ? text : "");
}



static gboolean
game_info_entry_focus_out_event (GtkEntry *entry, GdkEventFocus *event,
				 gpointer sgf_property_type)
{
  GtkWidget *widget;
  const gchar *entry_text = gtk_entry_get_text (entry);
  char *normalized_text = sgf_utils_normalize_text (entry_text, 1);

  UNUSED (event);

  /* If the normalized text is different, set it as the entry text. */
  if (normalized_text != NULL
      ? strcmp (normalized_text, entry_text) != 0
      : *entry_text)
    gtk_entry_set_text (entry, normalized_text ? normalized_text : "");

  widget = GTK_WIDGET (entry);
  do
    widget = gtk_widget_get_parent (widget);
  while (!GTK_IS_GAME_INFO_DIALOG (widget));

  update_property_if_changed (GTK_GAME_INFO_DIALOG (widget),
			      GPOINTER_TO_INT (sgf_property_type),
			      normalized_text);

  return FALSE;
}


/* Only connected to game comment text view, so `sgf_property_type' is
 * constant and equals to `SGF_GAME_COMMENT'.
 */
static gboolean
game_comment_focus_out_event (GtkGameInfoDialog *dialog, GdkEventFocus *event)
{
  UNUSED (event);

  if (dialog->sgf_tree
      && gtk_text_buffer_get_modified (dialog->game_comment)) {
    GtkTextIter start_iterator;
    GtkTextIter end_iterator;
    gchar *text_buffer_text;
    char *normalized_text;

    gtk_text_buffer_get_bounds (dialog->game_comment,
				&start_iterator, &end_iterator);
    text_buffer_text = gtk_text_iter_get_text (&start_iterator, &end_iterator);

    normalized_text = sgf_utils_normalize_text (text_buffer_text, 0);
    update_property_if_changed (dialog, SGF_GAME_COMMENT, normalized_text);

    g_free (text_buffer_text);

    gtk_text_buffer_set_modified (dialog->game_comment, FALSE);
  }

  return FALSE;
}


static void
game_info_spin_button_value_changed
  (GtkFreezableSpinButton *freezable_spin_button, GtkGameInfoDialog *dialog)
{
  if (gtk_freezable_spin_button_get_freezing_string (freezable_spin_button)
      == NULL) {
    if (dialog->handicap_spin_button == freezable_spin_button) {
      int handicap = gtk_adjustment_get_value (dialog->handicap);

      update_property_if_changed (dialog, SGF_HANDICAP,
				  utils_cprintf ("%d", handicap));
    }
    else if (dialog->komi_spin_button == freezable_spin_button) {
      double komi = gtk_adjustment_get_value (dialog->komi);

      update_property_if_changed (dialog, SGF_KOMI,
				  utils_cprintf ("%.f", komi));
    }
    else if (dialog->main_time_spin_button == freezable_spin_button) {
      double main_time = gtk_adjustment_get_value (dialog->main_time);

      update_property_if_changed (dialog, SGF_TIME_LIMIT,
				  utils_cprintf ("%.f", main_time));
    }
    else
      assert (0);
  }
}


static gboolean
game_info_spin_button_focus_out_event
  (GtkFreezableSpinButton *freezable_spin_button, GdkEventFocus *event,
   GtkGameInfoDialog *dialog)
{
  UNUSED (event);

  if (dialog->sgf_tree) {
    const gchar *freezing_string
      = gtk_freezable_spin_button_get_freezing_string (freezable_spin_button);

    /* There is only one interesting case: when a property value is
     * removed.  Check for it.
     */
    if (freezing_string && ! *freezing_string) {
      SgfType sgf_property_type;

      if (dialog->handicap_spin_button == freezable_spin_button)
	sgf_property_type = SGF_HANDICAP;
      else if (dialog->komi_spin_button == freezable_spin_button)
	sgf_property_type = SGF_KOMI;
      else if (dialog->main_time_spin_button == freezable_spin_button)
	sgf_property_type = SGF_TIME_LIMIT;
      else
	assert (0);

      if (sgf_node_get_text_property_value (dialog->sgf_node,
					    sgf_property_type)) {
	sgf_node_delete_property (dialog->sgf_node, dialog->sgf_tree,
				  sgf_property_type);
	g_signal_emit (dialog, game_info_dialog_signals[PROPERTY_CHANGED], 0,
		       sgf_property_type);
      }
    }
  }

  return FALSE;
}


static void
update_property_if_changed (GtkGameInfoDialog *dialog,
			    SgfType sgf_property_type, char *new_value)
{
  if (dialog->sgf_tree) {
    const char *current_value
      = sgf_node_get_text_property_value (dialog->sgf_node, sgf_property_type);

    if (current_value != NULL
	? new_value == NULL || strcmp (current_value, new_value) != 0
	: new_value != NULL) {
      /* The property value has been changed. */
      if (new_value) {
	sgf_node_add_text_property (dialog->sgf_node, dialog->sgf_tree,
				    sgf_property_type, new_value, 1);
      }
      else {
	sgf_node_delete_property (dialog->sgf_node, dialog->sgf_tree,
				  sgf_property_type);
      }

      g_signal_emit (dialog, game_info_dialog_signals[PROPERTY_CHANGED], 0,
		     sgf_property_type);
    }
    else
      utils_free (new_value);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
