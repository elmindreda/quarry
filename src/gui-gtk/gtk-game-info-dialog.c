/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004, 2005, 2006 Paul Pogonyshev.                 *
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


#include "gtk-game-info-dialog.h"

#include "gtk-freezable-spin-button.h"
#include "gtk-games.h"
#include "gtk-help.h"
#include "gtk-named-vbox.h"
#include "gtk-utils.h"
#include "quarry-marshal.h"
#include "quarry-text-view.h"
#include "sgf.h"
#include "board.h"
#include "utils.h"

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>


enum {
  GTK_GAME_INFO_DIALOG_RESPONSE_UNDO,
  GTK_GAME_INFO_DIALOG_RESPONSE_REDO
};


typedef struct _SgfPropertyChangeData	SgfPropertyChangeData;

struct _SgfPropertyChangeData {
  GtkGameInfoDialog  *dialog;
  SgfType	      sgf_property_type;
};


static void	   gtk_game_info_dialog_class_init
		     (GtkGameInfoDialogClass *class);
static void	   gtk_game_info_dialog_init (GtkGameInfoDialog *dialog);

static void	   update_title (GtkGameInfoDialog *dialog);

static GtkEntry *  create_and_pack_game_info_entry (const gchar *label_text,
						    SgfType sgf_property_type,
						    GtkWidget **hbox,
						    GtkGameInfoDialog *dialog);

static void	   gtk_game_info_dialog_response (GtkDialog *dialog,
						  gint response_id);

static void	   gtk_game_info_dialog_undo_history_action
		     (GtkGameInfoDialog *dialog, gboolean undo);

static void	   gtk_game_info_dialog_finalize (GObject *object);


static void *	   get_field (GtkGameInfoDialog *dialog,
			      SgfType sgf_property_type);
static gint	   get_page_index (SgfType sgf_property_type);
static SgfType	   get_property_type (GtkGameInfoDialog *dialog, void *field);
static char *	   get_field_text (void *field);
static void	   set_field_text (GtkGameInfoDialog *dialog,
				   SgfType sgf_property_type);
static void	   do_set_field_text (void *field, const gchar *value);


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

static void	   enable_simple_undo (void *field, GtkGameInfoDialog *dialog);

static void	   undo_or_redo_availability_changed
		     (SgfUndoHistory *undo_history, void *user_data);

static void	   update_property (GtkGameInfoDialog *dialog,
				    SgfType sgf_property_type,
				    char *new_value);

static void	   property_has_changed (SgfPropertyChangeData *data);


static GtkDialogClass  *parent_class;


enum {
  PROPERTY_CHANGED,
  UNDO_HISTORY_ACTION,
  NUM_SIGNALS
};

static guint	game_info_dialog_signals[NUM_SIGNALS];


static const SgfCustomUndoHistoryEntryData  property_change_undo_entry_data
  = { (SgfCustomOperationEntryFunction) property_has_changed,
      (SgfCustomOperationEntryFunction) property_has_changed,
      (SgfCustomOperationEntryFunction) utils_free };


GType
gtk_game_info_dialog_get_type (void)
{
  static GType game_info_dialog_type = 0;

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
  static GtkUtilsBindingInfo undo_redo_bindings[]
    = { { GDK_Z,	GDK_CONTROL_MASK,			TRUE  },
	{ GDK_Z,	GDK_CONTROL_MASK | GDK_SHIFT_MASK,	FALSE } };

  GtkBindingSet *binding_set;

  parent_class = g_type_class_peek_parent (class);

  G_OBJECT_CLASS (class)->finalize   = gtk_game_info_dialog_finalize;

  GTK_DIALOG_CLASS (class)->response = gtk_game_info_dialog_response;

  class->property_changed    = NULL;
  class->undo_history_action = gtk_game_info_dialog_undo_history_action;

  game_info_dialog_signals[PROPERTY_CHANGED]
    = g_signal_new ("property-changed",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkGameInfoDialogClass, property_changed),
		    NULL, NULL,
		    quarry_marshal_VOID__INT,
		    G_TYPE_NONE, 1, G_TYPE_INT);

  game_info_dialog_signals[UNDO_HISTORY_ACTION]
    = g_signal_new ("undo-history-action",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		    G_STRUCT_OFFSET (GtkGameInfoDialogClass,
				     undo_history_action),
		    NULL, NULL,
		    quarry_marshal_VOID__BOOLEAN,
		    G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  binding_set = gtk_binding_set_by_class (class);
  gtk_utils_add_similar_bindings (binding_set, "undo-history-action",
				  undo_redo_bindings,
				  (sizeof undo_redo_bindings
				   / sizeof (GtkUtilsBindingInfo)));
}


static void
gtk_game_info_dialog_init (GtkGameInfoDialog *dialog)
{
  GtkWidget *notebook = gtk_notebook_new ();
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

  dialog->pages = GTK_NOTEBOOK (notebook);

  for (k = 0; k < NUM_COLORS; k++) {
    dialog->player_names[k]
      = create_and_pack_game_info_entry ((k == BLACK_INDEX
					  ? _("N_ame:") : _("_Name:")),
					 (k == BLACK_INDEX
					  ? SGF_PLAYER_BLACK
					  : SGF_PLAYER_WHITE),
					 &hbox1, dialog);

    dialog->player_teams[k]
      = create_and_pack_game_info_entry (_("Team:"),
					 (k == BLACK_INDEX
					  ? SGF_BLACK_TEAM : SGF_WHITE_TEAM),
					 &hbox2, dialog);

    dialog->player_ranks[k]
      = create_and_pack_game_info_entry (_("Rank:"),
					 (k == BLACK_INDEX
					  ? SGF_BLACK_RANK : SGF_WHITE_RANK),
					 &hbox3, dialog);

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
				       &hbox1, dialog);
  dialog->place
    = create_and_pack_game_info_entry (_("_Place:"), SGF_PLACE, &hbox2,
				       dialog);

  dialog->date
    = create_and_pack_game_info_entry (_("Da_te:"), SGF_DATE, &hbox3, dialog);

  dialog->event
    = create_and_pack_game_info_entry (_("_Event:"), SGF_EVENT, &hbox4,
				       dialog);

  dialog->round
    = create_and_pack_game_info_entry (_("R_ound:"), SGF_ROUND, &hbox5,
				       dialog);

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
    = create_and_pack_game_info_entry (_("Rule _set:"), SGF_RULE_SET, &hbox1,
				       dialog);

  dialog->handicap
    = ((GtkAdjustment *)
       gtk_adjustment_new (0, 0, GTK_MAX_BOARD_SIZE * GTK_MAX_BOARD_SIZE,
			   1, 2, 0));
  handicap_spin_button
    = gtk_utils_create_freezable_spin_button (dialog->handicap, 0.0, 0, TRUE);
  dialog->handicap_spin_button
    = GTK_FREEZABLE_SPIN_BUTTON (handicap_spin_button);
  gtk_utils_freeze_on_empty_input (dialog->handicap_spin_button);

  g_signal_connect (handicap_spin_button, "value-changed",
		    G_CALLBACK (game_info_spin_button_value_changed), dialog);
  g_signal_connect (handicap_spin_button, "changed",
		    G_CALLBACK (enable_simple_undo), dialog);

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
  g_signal_connect (komi_spin_button, "changed",
		    G_CALLBACK (enable_simple_undo), dialog);
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
  g_signal_connect (main_time_spin_button, "changed",
		    G_CALLBACK (enable_simple_undo), dialog);
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
    = create_and_pack_game_info_entry (_("O_vertime:"), SGF_OVERTIME, &hbox5,
				       dialog);

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

  gtk_notebook_append_page (dialog->pages, page, gtk_label_new (_("General")));

  dialog->result
    = create_and_pack_game_info_entry (_("Game re_sult:"), SGF_RESULT, &hbox1,
				       dialog);

  dialog->opening
    = create_and_pack_game_info_entry (_("_Opening played:"), SGF_OPENING,
				       &hbox2, dialog);

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				hbox1, GTK_UTILS_FILL, hbox2, GTK_UTILS_FILL,
				NULL);
  gtk_utils_align_left_widgets (GTK_CONTAINER (vbox), NULL);

  alignment = gtk_alignment_new (0.0, 0.0, 0.4, 1.0);
  gtk_container_add (GTK_CONTAINER (alignment), vbox);

  text_view = gtk_text_view_new ();
  dialog->game_comment_text_view = text_view;

  dialog->game_comment = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text_view),
				 QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (text_view),
				  QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), GTK_WRAP_WORD);

  g_signal_connect (dialog->game_comment, "changed",
		    G_CALLBACK (enable_simple_undo), dialog);
  g_signal_connect_swapped (text_view, "focus-out-event",
			    G_CALLBACK (game_comment_focus_out_event), dialog);

  scrolled_window = gtk_utils_make_widget_scrollable (text_view,
						      GTK_POLICY_AUTOMATIC,
						      GTK_POLICY_AUTOMATIC);

  label = gtk_utils_create_mnemonic_label (_("Comment / _description:"),
					   text_view);

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				label, GTK_UTILS_FILL,
				scrolled_window, GTK_UTILS_PACK_DEFAULT, NULL);

  page = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				alignment, GTK_UTILS_FILL,
				vbox, GTK_UTILS_PACK_DEFAULT, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (page), QUARRY_SPACING);
  gtk_widget_show_all (page);

  gtk_notebook_append_page (dialog->pages, page,
			    gtk_label_new (_("Description & Result")));

  dialog->copyright
    = create_and_pack_game_info_entry (_("Co_pyright string:"), SGF_COPYRIGHT,
				       &hbox1, dialog);

  dialog->annotator
    = create_and_pack_game_info_entry (_("_Annotator:"), SGF_ANNOTATOR,
				       &hbox2, dialog);

  dialog->source
    = create_and_pack_game_info_entry (_("_Source:"), SGF_SOURCE, &hbox3,
				       dialog);

  dialog->user
    = create_and_pack_game_info_entry (_("_Entered by:"), SGF_USER, &hbox4,
				       dialog);

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

  gtk_notebook_append_page (dialog->pages, page,
			    gtk_label_new (_("Game Record")));

  gtk_widget_show (notebook);
  gtk_utils_standardize_dialog (&dialog->dialog, notebook);
  gtk_dialog_set_has_separator (&dialog->dialog, FALSE);

  gtk_dialog_add_buttons (&dialog->dialog,
			  GTK_STOCK_UNDO, GTK_GAME_INFO_DIALOG_RESPONSE_UNDO,
			  GTK_STOCK_REDO, GTK_GAME_INFO_DIALOG_RESPONSE_REDO,
			  GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
  gtk_utils_add_help_button (&dialog->dialog);

  dialog->sgf_tree	    = NULL;
  dialog->sgf_node	    = NULL;
  dialog->sgf_undo_history  = NULL;

  dialog->simple_undo_field = NULL;
  dialog->simple_redo_field = NULL;
  dialog->simple_redo_value = NULL;

  update_title (dialog);
}


static void
update_title (GtkGameInfoDialog *dialog)
{
  if (dialog->sgf_node
      && sgf_node_get_text_property_value (dialog->sgf_node, SGF_GAME_NAME)) {
    gchar* title
      = g_strdup_printf ("%s - %s",
			 sgf_node_get_text_property_value (dialog->sgf_node,
							   SGF_GAME_NAME),
			 _("Game Information"));

    gtk_window_set_title (GTK_WINDOW (dialog), title);
    g_free (title);
  }
  else
    gtk_window_set_title (GTK_WINDOW (dialog), _("Game Information"));
}


static GtkEntry *
create_and_pack_game_info_entry (const gchar *label_text,
				 SgfType sgf_property_type, GtkWidget **hbox,
				 GtkGameInfoDialog *dialog)
{
  GtkWidget *entry = gtk_utils_create_entry (NULL, RETURN_ADVANCES_FOCUS);
  GtkWidget *label = gtk_utils_create_mnemonic_label (label_text, entry);

  *hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				 label, 0, entry, GTK_UTILS_PACK_DEFAULT,
				 NULL);

  g_signal_connect (entry, "changed", G_CALLBACK (enable_simple_undo), dialog);
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
  else if (response_id	  == GTK_GAME_INFO_DIALOG_RESPONSE_UNDO
	   || response_id == GTK_GAME_INFO_DIALOG_RESPONSE_REDO) {
    g_signal_emit (dialog, game_info_dialog_signals[UNDO_HISTORY_ACTION],
		   0, response_id == GTK_GAME_INFO_DIALOG_RESPONSE_UNDO);
  }
  else if (response_id == GTK_RESPONSE_HELP)
    gtk_help_display ("game-information-dialog");
}


static void
gtk_game_info_dialog_undo_history_action (GtkGameInfoDialog *dialog,
					  gboolean undo)
{
  SgfGameTree *sgf_tree;
  SgfUndoHistory *saved_undo_history;
  void *field;

  g_return_if_fail (GTK_IS_GAME_INFO_DIALOG (dialog));

  sgf_tree	     = dialog->sgf_tree;
  saved_undo_history = sgf_tree->undo_history;

  sgf_tree->undo_history = dialog->sgf_undo_history;

  if (undo) {
    if (!dialog->simple_undo_field)
      sgf_utils_undo (sgf_tree);
    else {
      dialog->simple_redo_field = dialog->simple_undo_field;
      dialog->simple_redo_value = get_field_text (dialog->simple_undo_field);

      field = dialog->simple_undo_field;
      set_field_text (dialog, get_property_type (dialog, field));

      dialog->simple_undo_field = NULL;
      undo_or_redo_availability_changed (dialog->sgf_undo_history, dialog);

      goto undone_or_redone;
    }
  }
  else {
    if (!dialog->simple_redo_field)
      sgf_utils_redo (sgf_tree);
    else {
      field = dialog->simple_redo_field;
      do_set_field_text (field, dialog->simple_redo_value);

      dialog->simple_undo_field = dialog->simple_redo_field;
      dialog->simple_redo_field = NULL;

      g_free (dialog->simple_redo_value);
      dialog->simple_redo_value = NULL;
      undo_or_redo_availability_changed (dialog->sgf_undo_history, dialog);

      goto undone_or_redone;
    }
  }

  set_field_text (dialog, dialog->modified_property_type);

  gtk_notebook_set_current_page
    (dialog->pages, get_page_index (dialog->modified_property_type));

  field = get_field (dialog, dialog->modified_property_type);

  g_signal_emit (dialog, game_info_dialog_signals[PROPERTY_CHANGED], 0,
		 dialog->modified_property_type);

 undone_or_redone:

  sgf_tree->undo_history = saved_undo_history;

  if (GTK_IS_ENTRY (field))
    gtk_widget_grab_focus (GTK_WIDGET (field));
  else
    gtk_widget_grab_focus (dialog->game_comment_text_view);
}


static void
gtk_game_info_dialog_finalize (GObject *object)
{
  GtkGameInfoDialog *dialog = GTK_GAME_INFO_DIALOG (object);

  if (dialog->sgf_undo_history)
    sgf_undo_history_delete (dialog->sgf_undo_history, dialog->sgf_tree);

  g_free (dialog->simple_redo_value);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


void
gtk_game_info_dialog_set_node (GtkGameInfoDialog *dialog,
			       SgfGameTree *sgf_tree, SgfNode *sgf_node)
{
  g_return_if_fail (GTK_IS_GAME_INFO_DIALOG (dialog));
  g_return_if_fail (sgf_tree);
  g_return_if_fail (sgf_node);

  dialog->sgf_tree	   = sgf_tree;
  dialog->sgf_node	   = sgf_node;
  dialog->sgf_undo_history = sgf_undo_history_new (sgf_tree);

  sgf_undo_history_set_notification_callback
    (dialog->sgf_undo_history, undo_or_redo_availability_changed, dialog);

  /* Set the initial state. */
  undo_or_redo_availability_changed (dialog->sgf_undo_history, dialog);

  /* Initializing "General" page. */

  set_field_text (dialog, SGF_PLAYER_WHITE);
  set_field_text (dialog, SGF_WHITE_TEAM);
  set_field_text (dialog, SGF_WHITE_RANK);

  set_field_text (dialog, SGF_PLAYER_BLACK);
  set_field_text (dialog, SGF_BLACK_TEAM);
  set_field_text (dialog, SGF_BLACK_RANK);

  set_field_text (dialog, SGF_GAME_NAME);
  set_field_text (dialog, SGF_PLACE);
  set_field_text (dialog, SGF_DATE);
  set_field_text (dialog, SGF_EVENT);
  set_field_text (dialog, SGF_ROUND);

  set_field_text (dialog, SGF_RULE_SET);
  set_field_text (dialog, SGF_HANDICAP);
  set_field_text (dialog, SGF_KOMI);
  set_field_text (dialog, SGF_TIME_LIMIT);
  set_field_text (dialog, SGF_OVERTIME);

  gtk_widget_set_sensitive (dialog->handicap_box, sgf_tree->game == GAME_GO);
  gtk_widget_set_sensitive (dialog->komi_box, sgf_tree->game == GAME_GO);

  /* Initializing "Description & Result" page. */
  set_field_text (dialog, SGF_RESULT);
  set_field_text (dialog, SGF_OPENING);
  set_field_text (dialog, SGF_GAME_COMMENT);

  /* Initializing "Game Record" page. */
  set_field_text (dialog, SGF_COPYRIGHT);
  set_field_text (dialog, SGF_ANNOTATOR);
  set_field_text (dialog, SGF_SOURCE);
  set_field_text (dialog, SGF_USER);
}


static void *
get_field (GtkGameInfoDialog *dialog, SgfType sgf_property_type)
{
  switch (sgf_property_type) {
  case SGF_PLAYER_BLACK:
  case SGF_PLAYER_WHITE:
    return dialog->player_names[sgf_property_type == SGF_PLAYER_BLACK
				? BLACK_INDEX : WHITE_INDEX];

  case SGF_BLACK_TEAM:
  case SGF_WHITE_TEAM:
    return dialog->player_teams[sgf_property_type == SGF_BLACK_TEAM
				? BLACK_INDEX : WHITE_INDEX];

  case SGF_BLACK_RANK:
  case SGF_WHITE_RANK:
    return dialog->player_ranks[sgf_property_type == SGF_BLACK_RANK
				? BLACK_INDEX : WHITE_INDEX];

  case SGF_GAME_NAME:
    return dialog->game_name;

  case SGF_PLACE:
    return dialog->place;

  case SGF_DATE:
    return dialog->date;

  case SGF_EVENT:
    return dialog->event;

  case SGF_ROUND:
    return dialog->round;

  case SGF_RULE_SET:
    return dialog->rule_set;

  case SGF_HANDICAP:
    return dialog->handicap_spin_button;

  case SGF_KOMI:
    return dialog->komi_spin_button;

  case SGF_TIME_LIMIT:
    return dialog->main_time_spin_button;

  case SGF_OVERTIME:
    return dialog->overtime;

  case SGF_RESULT:
    return dialog->result;

  case SGF_OPENING:
    return dialog->opening;

  case SGF_GAME_COMMENT:
    return dialog->game_comment;

  case SGF_COPYRIGHT:
    return dialog->copyright;

  case SGF_ANNOTATOR:
    return dialog->annotator;

  case SGF_SOURCE:
    return dialog->source;

  case SGF_USER:
    return dialog->user;

  default:
    g_assert_not_reached ();
    return NULL;
  }
}


static gint
get_page_index (SgfType sgf_property_type)
{
  switch (sgf_property_type) {
  case SGF_PLAYER_BLACK:
  case SGF_PLAYER_WHITE:
  case SGF_BLACK_TEAM:
  case SGF_WHITE_TEAM:
  case SGF_BLACK_RANK:
  case SGF_WHITE_RANK:
  case SGF_GAME_NAME:
  case SGF_PLACE:
  case SGF_DATE:
  case SGF_EVENT:
  case SGF_ROUND:
  case SGF_RULE_SET:
  case SGF_HANDICAP:
  case SGF_KOMI:
  case SGF_TIME_LIMIT:
  case SGF_OVERTIME:
    return 0;

  case SGF_RESULT:
  case SGF_OPENING:
  case SGF_GAME_COMMENT:
    return 1;

  case SGF_COPYRIGHT:
  case SGF_ANNOTATOR:
  case SGF_SOURCE:
  case SGF_USER:
    return 2;

  default:
    g_assert_not_reached ();
    return -1;
  }
}


static SgfType
get_property_type (GtkGameInfoDialog *dialog, void *field)
{
  if (field == dialog->player_names[BLACK_INDEX])
    return SGF_PLAYER_BLACK;

  if (field == dialog->player_names[WHITE_INDEX])
    return SGF_PLAYER_WHITE;

  if (field == dialog->player_teams[BLACK_INDEX])
    return SGF_BLACK_TEAM;

  if (field == dialog->player_teams[WHITE_INDEX])
    return SGF_WHITE_TEAM;

  if (field == dialog->player_ranks[BLACK_INDEX])
    return SGF_BLACK_RANK;

  if (field == dialog->player_ranks[WHITE_INDEX])
    return SGF_WHITE_RANK;

  if (field == dialog->game_name)
    return SGF_GAME_NAME;

  if (field == dialog->place)
    return SGF_PLACE;

  if (field == dialog->date)
    return SGF_DATE;

  if (field == dialog->event)
    return SGF_EVENT;

  if (field == dialog->round)
    return SGF_ROUND;

  if (field == dialog->rule_set)
    return SGF_RULE_SET;

  if (field == dialog->handicap_spin_button)
    return SGF_HANDICAP;

  if (field == dialog->komi_spin_button)
    return SGF_KOMI;

  if (field == dialog->main_time_spin_button)
    return SGF_TIME_LIMIT;

  if (field == dialog->overtime)
    return SGF_OVERTIME;

  if (field == dialog->result)
    return SGF_RESULT;

  if (field == dialog->opening)
    return SGF_OPENING;

  if (field == dialog->game_comment)
    return SGF_GAME_COMMENT;

  if (field == dialog->copyright)
    return SGF_COPYRIGHT;

  if (field == dialog->annotator)
    return SGF_ANNOTATOR;

  if (field == dialog->source)
    return SGF_SOURCE;

  if (field == dialog->user)
    return SGF_USER;

  g_assert_not_reached ();
  return SGF_UNKNOWN;
}


static char *
get_field_text (void *field)
{
  /* Note that GtkEntry is a more generic type and must be checked for
   * after GtkFreezableSpinButton.
   */
  if (GTK_IS_ENTRY (field))
    return g_strdup (gtk_entry_get_text (GTK_ENTRY (field)));
  else if (GTK_IS_TEXT_BUFFER (field)) {
    GtkTextIter start_iterator;
    GtkTextIter end_iterator;

    gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (field),
				&start_iterator, &end_iterator);
    return gtk_text_iter_get_text (&start_iterator, &end_iterator);
  }
  else {
    g_assert_not_reached ();
    return NULL;
  }
}


static void
set_field_text (GtkGameInfoDialog *dialog, SgfType sgf_property_type)
{
  void *field = get_field (dialog, sgf_property_type);

  g_signal_handlers_block_by_func (field, G_CALLBACK (enable_simple_undo),
				   dialog);

  if (sgf_property_type == SGF_HANDICAP) {
    if (dialog->sgf_tree->game == GAME_GO) {
      int handicap = sgf_node_get_handicap (dialog->sgf_node);

      if (handicap >= 0) {
	gtk_adjustment_set_value (dialog->handicap, (gdouble) handicap);
	goto text_set;
      }
    }
  }
  else if (sgf_property_type == SGF_KOMI) {
    double komi;

    if (dialog->sgf_tree->game == GAME_GO
	&& sgf_node_get_komi (dialog->sgf_node, &komi)) {
      gtk_adjustment_set_value (dialog->komi, komi);
      goto text_set;
    }
  }
  else if (sgf_property_type == SGF_TIME_LIMIT) {
    double time_limit;

    if (sgf_node_get_time_limit (dialog->sgf_node, &time_limit)) {
      gtk_adjustment_set_value (dialog->main_time, time_limit);
      goto text_set;
    }
  }
  else if (sgf_property_type == SGF_GAME_NAME)
    update_title (dialog);

  do_set_field_text (field,
		     sgf_node_get_text_property_value (dialog->sgf_node,
						       sgf_property_type));

 text_set:

  g_signal_handlers_unblock_by_func (field, G_CALLBACK (enable_simple_undo),
				     dialog);
}


static void
do_set_field_text (void *field, const gchar *value)
{
  if (!value)
    value = "";

  gtk_utils_block_signal_handlers (field, G_CALLBACK (enable_simple_undo));

  /* Note that GtkEntry is a more generic type and must be checked for
   * after GtkFreezableSpinButton.
   */
  if (GTK_IS_FREEZABLE_SPIN_BUTTON (field)) {
    gtk_freezable_spin_button_freeze (GTK_FREEZABLE_SPIN_BUTTON (field),
				      value);
  }
  else if (GTK_IS_ENTRY (field))
    gtk_entry_set_text (GTK_ENTRY (field), value);
  else if (GTK_IS_TEXT_BUFFER (field)) {
    gtk_utils_set_text_buffer_text (GTK_TEXT_BUFFER (field), value);
    gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (field), FALSE);
  }
  else
    g_assert_not_reached ();

  gtk_utils_unblock_signal_handlers (field, G_CALLBACK (enable_simple_undo));
}



static gboolean
game_info_entry_focus_out_event (GtkEntry *entry, GdkEventFocus *event,
				 gpointer sgf_property_type)
{
  GtkWidget *top_level_widget;
  const gchar *entry_text = gtk_entry_get_text (entry);
  char *normalized_text = sgf_utils_normalize_text (entry_text, 1);

  UNUSED (event);

  /* If the normalized text is different, set it as the entry text. */
  if (normalized_text != NULL
      ? strcmp (normalized_text, entry_text) != 0
      : *entry_text)
    gtk_entry_set_text (entry, normalized_text ? normalized_text : "");

  top_level_widget = gtk_widget_get_toplevel (GTK_WIDGET (entry));
  update_property (GTK_GAME_INFO_DIALOG (top_level_widget),
		   GPOINTER_TO_INT (sgf_property_type), normalized_text);

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
    update_property (dialog, SGF_GAME_COMMENT, normalized_text);

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

      update_property (dialog, SGF_HANDICAP, utils_cprintf ("%d", handicap));
    }
    else if (dialog->komi_spin_button == freezable_spin_button) {
      double komi = gtk_adjustment_get_value (dialog->komi);

      update_property (dialog, SGF_KOMI, utils_cprintf ("%.f", komi));
    }
    else if (dialog->main_time_spin_button == freezable_spin_button) {
      double main_time = gtk_adjustment_get_value (dialog->main_time);

      update_property (dialog, SGF_TIME_LIMIT,
		       utils_cprintf ("%.f", main_time));
    }
    else
      g_assert_not_reached ();
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
      else {
	g_assert_not_reached ();
	return FALSE;
      }

      update_property (dialog, sgf_property_type, NULL);
    }
  }

  return FALSE;
}


static void
enable_simple_undo (void *field, GtkGameInfoDialog *dialog)
{
  if (dialog->simple_undo_field)
    return;

  if (GTK_IS_FREEZABLE_SPIN_BUTTON (field)
      && (gtk_freezable_spin_button_get_freezing_string
	  (GTK_FREEZABLE_SPIN_BUTTON (field))))
    return;

  dialog->simple_undo_field = field;

  sgf_undo_history_delete_redo_entries (dialog->sgf_undo_history,
					dialog->sgf_tree);
  dialog->simple_redo_field = NULL;
  g_free (dialog->simple_redo_value);
  dialog->simple_redo_value = NULL;

  undo_or_redo_availability_changed (dialog->sgf_undo_history, dialog);
}


static void
undo_or_redo_availability_changed (SgfUndoHistory *undo_history,
				   void *user_data)
{
  GtkGameInfoDialog *dialog = GTK_GAME_INFO_DIALOG (user_data);
  SgfUndoHistory *saved_undo_history = dialog->sgf_tree->undo_history;
  gboolean can_undo;
  gboolean can_redo;

  UNUSED (undo_history);

  dialog->sgf_tree->undo_history = dialog->sgf_undo_history;

  can_undo = (dialog->simple_undo_field
	      || sgf_utils_can_undo (dialog->sgf_tree));
  can_redo = (dialog->simple_redo_field
	      || sgf_utils_can_redo (dialog->sgf_tree));

  dialog->sgf_tree->undo_history = saved_undo_history;

  gtk_dialog_set_response_sensitive (&dialog->dialog,
				     GTK_GAME_INFO_DIALOG_RESPONSE_UNDO,
				     can_undo);
  gtk_dialog_set_response_sensitive (&dialog->dialog,
				     GTK_GAME_INFO_DIALOG_RESPONSE_REDO,
				     can_redo);
}


static void
update_property (GtkGameInfoDialog *dialog, SgfType sgf_property_type,
		 char *new_value)
{
  SgfGameTree *sgf_tree = dialog->sgf_tree;

  if (sgf_tree) {
    SgfUndoHistory *saved_undo_history = sgf_tree->undo_history;

    sgf_tree->undo_history = dialog->sgf_undo_history;

    sgf_utils_begin_action (sgf_tree);

    if (sgf_utils_set_text_property (dialog->sgf_node, sgf_tree,
				     sgf_property_type, new_value, 1)) {
      SgfPropertyChangeData *data
	= utils_malloc (sizeof (SgfPropertyChangeData));

      data->dialog	      = dialog;
      data->sgf_property_type = sgf_property_type;

      sgf_utils_apply_custom_undo_entry (sgf_tree,
					 &property_change_undo_entry_data,
					 data, NULL);

      g_signal_emit (dialog, game_info_dialog_signals[PROPERTY_CHANGED], 0,
		     sgf_property_type);

      dialog->simple_undo_field = NULL;

      if (sgf_property_type == SGF_GAME_NAME)
	update_title (dialog);
    }

    sgf_utils_end_action (sgf_tree);

    sgf_tree->undo_history = saved_undo_history;
  }
}


static void
property_has_changed (SgfPropertyChangeData *data)
{
  data->dialog->modified_property_type = data->sgf_property_type;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
