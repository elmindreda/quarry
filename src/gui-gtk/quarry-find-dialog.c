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


#include "quarry-find-dialog.h"

#include "gtk-configuration.h"
#include "gtk-named-vbox.h"
#include "gtk-utils.h"
#include "quarry-stock.h"
#include "sgf.h"

#include <gtk/gtk.h>
#include <string.h>


static void	 quarry_find_dialog_class_init (QuarryFindDialogClass *class);
static void	 quarry_find_dialog_init (QuarryFindDialog *dialog);

static void	 parameters_changed (QuarryFindDialog *dialog);


static char *	 strstr_whole_word (const char *haystack, const char *needle);
static char *	 strrstr_whole_word (const char *haystack, const char *needle);
static char *	 get_top_buffer_part (GtkTextBuffer *text_buffer,
				      gboolean node_name_inserted,
				      const GtkTextIter *boundary_iterator,
				      gint search_in);
static char *	 get_bottom_buffer_part (GtkTextBuffer *text_buffer,
					 gboolean node_name_inserted,
					 const GtkTextIter *boundary_iterator,
					 gint search_in);
static gboolean  char_is_word_constituent (const gchar *character);
inline static gchar *
		 get_normalized_text (const gchar *text, gint length,
				      gboolean case_sensitive);
static gint	 get_offset_in_original_text (const gchar *text, gint length,
					      gint normalized_text_offset,
					      gboolean case_sensitive,
					      gint first_guess);


static GtkDialog  *parent_class;


GType
quarry_find_dialog_get_type (void)
{
  static GType quarry_find_dialog_type = 0;

  if (!quarry_find_dialog_type) {
    static const GTypeInfo quarry_find_dialog_info = {
      sizeof (QuarryFindDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) quarry_find_dialog_class_init,
      NULL,
      NULL,
      sizeof (QuarryFindDialog),
      0,
      (GInstanceInitFunc) quarry_find_dialog_init,
      NULL
    };

    quarry_find_dialog_type = g_type_register_static (GTK_TYPE_DIALOG,
						      "QuarryFindDialog",
						      &quarry_find_dialog_info,
						      0);
  }

  return quarry_find_dialog_type;
}


static void
quarry_find_dialog_class_init (QuarryFindDialogClass *class)
{
  parent_class = g_type_class_peek_parent (class);
}


static void
quarry_find_dialog_init (QuarryFindDialog *dialog)
{
  static const gchar *tree_scope_radio_button_labels[2]
    = { N_("Whole game _tree"), N_("Current no_de only") };

  static const gchar *properties_scope_radio_button_labels[3]
    = { N_("C_omments & node names"),
	N_("Comm_ents only"), N_("Node na_mes only") };

  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *case_sensitive_check_button;
  GtkWidget *whole_words_only_check_button;
  GtkWidget *wrap_around_check_button;
  GtkWidget *vbox1;
  GtkWidget *close_automatically_check_button;
  GtkWidget *options_named_vbox;
  GtkWidget *tree_scope_radio_buttons[2];
  GtkWidget *properties_scope_radio_buttons[3];
  GtkWidget *vbox2;
  GtkWidget *scope_named_vbox;
  int k;

  gtk_dialog_add_buttons (&dialog->dialog,
			  GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
			  QUARRY_STOCK_FIND_PREVIOUS,
			  QUARRY_FIND_DIALOG_FIND_PREVIOUS,
			  QUARRY_STOCK_FIND_NEXT,
			  QUARRY_FIND_DIALOG_FIND_NEXT, NULL);
  gtk_utils_add_help_button (&dialog->dialog);
  gtk_dialog_set_default_response (&dialog->dialog,
				   QUARRY_FIND_DIALOG_FIND_NEXT);

  gtk_utils_make_window_only_horizontally_resizable (GTK_WINDOW (dialog));

#if GTK_2_4_OR_LATER

  entry = gtk_combo_box_entry_new_text ();
  dialog->search_for_entry = GTK_ENTRY (GTK_BIN (entry)->child);

  gtk_entry_set_activates_default (dialog->search_for_entry, TRUE);

#else  /* not GTK_2_4_OR_LATER */

  entry = gtk_utils_create_entry (NULL, RETURN_ACTIVATES_DEFAULT);
  dialog->search_for_entry = GTK_ENTRY (entry);

#endif /* not GTK_2_4_OR_LATER */

  g_signal_connect_swapped (entry, "changed",
			    G_CALLBACK (parameters_changed), dialog);

  label = gtk_utils_create_mnemonic_label (_("Search _for:"), entry);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				label, GTK_UTILS_FILL,
				entry, GTK_UTILS_PACK_DEFAULT, NULL);

  case_sensitive_check_button
    = gtk_check_button_new_with_mnemonic (_("Case _sensitive"));
  dialog->case_sensitive_toggle_button
    = GTK_TOGGLE_BUTTON (case_sensitive_check_button);

  g_signal_connect_swapped (case_sensitive_check_button, "toggled",
			    G_CALLBACK (parameters_changed), dialog);

  whole_words_only_check_button
    = gtk_check_button_new_with_mnemonic (_("Whole _words only"));
  dialog->whole_words_only_toggle_button
    = GTK_TOGGLE_BUTTON (whole_words_only_check_button);

  g_signal_connect_swapped (whole_words_only_check_button, "toggled",
			    G_CALLBACK (parameters_changed), dialog);

  wrap_around_check_button
    = gtk_check_button_new_with_mnemonic (_("Wrap _around"));
  dialog->wrap_around_toggle_button
    = GTK_TOGGLE_BUTTON (wrap_around_check_button);

  g_signal_connect_swapped (wrap_around_check_button, "toggled",
			    G_CALLBACK (parameters_changed), dialog);

  vbox1 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				 case_sensitive_check_button, GTK_UTILS_FILL,
				 whole_words_only_check_button, GTK_UTILS_FILL,
				 wrap_around_check_button, GTK_UTILS_FILL,
				 NULL);

  close_automatically_check_button
    = gtk_check_button_new_with_mnemonic (_("A_uto-close this dialog"));
  dialog->close_automatically_toggle_button
    = GTK_TOGGLE_BUTTON (close_automatically_check_button);

  options_named_vbox
    = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX, QUARRY_SPACING_BIG,
			     vbox1, GTK_UTILS_FILL,
			     close_automatically_check_button,
			     GTK_UTILS_FILL,
			     NULL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (options_named_vbox),
				 _("Options"));

  gtk_utils_create_radio_chain (tree_scope_radio_buttons,
				tree_scope_radio_button_labels, 2);
  dialog->search_current_node_only_toggle_button
    = GTK_TOGGLE_BUTTON (tree_scope_radio_buttons[1]);

  g_signal_connect_swapped (tree_scope_radio_buttons[0], "toggled",
			    G_CALLBACK (parameters_changed), dialog);

  vbox1 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				 tree_scope_radio_buttons[0], GTK_UTILS_FILL,
				 tree_scope_radio_buttons[1], GTK_UTILS_FILL,
				 NULL);

  /* Let's be over-secure (a compile-time error would have been
   * better...)
   */
  g_assert (SEARCH_EVERYWHERE	    == 0
	    && SEARCH_IN_COMMENTS   == 1
	    && SEARCH_IN_NODE_NAMES == 2);

  gtk_utils_create_radio_chain (properties_scope_radio_buttons,
				properties_scope_radio_button_labels, 3);

  for (k = 0; k < 3; k++) {
    dialog->search_scope_toggle_buttons[k]
      = GTK_TOGGLE_BUTTON (properties_scope_radio_buttons[k]);
  }

  g_signal_connect_swapped (properties_scope_radio_buttons[0], "toggled",
			    G_CALLBACK (parameters_changed), dialog);
  g_signal_connect_swapped (properties_scope_radio_buttons[1], "toggled",
			    G_CALLBACK (parameters_changed), dialog);

  vbox2 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				 properties_scope_radio_buttons[0],
				 GTK_UTILS_FILL,
				 properties_scope_radio_buttons[1],
				 GTK_UTILS_FILL,
				 properties_scope_radio_buttons[2],
				 GTK_UTILS_FILL,
				 NULL);

  scope_named_vbox = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
					    QUARRY_SPACING_BIG,
					    vbox1, GTK_UTILS_FILL,
					    vbox2, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (scope_named_vbox),
				 _("Search Scope"));

  vbox1 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				 hbox, GTK_UTILS_FILL,
				 (gtk_utils_pack_in_box
				  (GTK_TYPE_HBOX, QUARRY_SPACING_VERY_BIG,
				   options_named_vbox, GTK_UTILS_FILL,
				   scope_named_vbox, GTK_UTILS_FILL,
				   NULL)),
				 GTK_UTILS_FILL,
				 NULL);

  gtk_widget_show_all (vbox1);
  gtk_utils_standardize_dialog (&dialog->dialog, vbox1);

  /* Desensitize buttons. */
  parameters_changed (dialog);
}


GtkWidget *
quarry_find_dialog_new (const gchar *title)
{
  GtkWidget *dialog = GTK_WIDGET (g_object_new (QUARRY_TYPE_FIND_DIALOG,
						NULL));

  gtk_window_set_title (GTK_WINDOW (dialog), title);

  return dialog;
}


const gchar *
quarry_find_dialog_get_text_to_find (QuarryFindDialog *dialog)
{
  g_return_val_if_fail (QUARRY_IS_FIND_DIALOG (dialog), NULL);

  return gtk_entry_get_text (dialog->search_for_entry);
}


gboolean
quarry_find_dialog_get_case_sensitive (QuarryFindDialog *dialog)
{
  g_return_val_if_fail (QUARRY_IS_FIND_DIALOG (dialog), FALSE);

  return gtk_toggle_button_get_active (dialog->case_sensitive_toggle_button);
}


gboolean
quarry_find_dialog_get_whole_words_only (QuarryFindDialog *dialog)
{
  g_return_val_if_fail (QUARRY_IS_FIND_DIALOG (dialog), FALSE);

  return gtk_toggle_button_get_active (dialog->whole_words_only_toggle_button);
}


gboolean
quarry_find_dialog_get_wrap_around (QuarryFindDialog *dialog)
{
  g_return_val_if_fail (QUARRY_IS_FIND_DIALOG (dialog), FALSE);

  return gtk_toggle_button_get_active (dialog->wrap_around_toggle_button);
}


gboolean
quarry_find_dialog_get_search_whole_game_tree (QuarryFindDialog *dialog)
{
  g_return_val_if_fail (QUARRY_IS_FIND_DIALOG (dialog), FALSE);

  return !(gtk_toggle_button_get_active
	   (dialog->search_current_node_only_toggle_button));
}


gint
quarry_find_dialog_get_search_in (QuarryFindDialog *dialog)
{
  g_return_val_if_fail (QUARRY_IS_FIND_DIALOG (dialog), SEARCH_EVERYWHERE);

  if (gtk_toggle_button_get_active
      (dialog->search_scope_toggle_buttons[SEARCH_EVERYWHERE]))
    return SEARCH_EVERYWHERE;
  else if (gtk_toggle_button_get_active
	   (dialog->search_scope_toggle_buttons[SEARCH_IN_COMMENTS]))
    return SEARCH_IN_COMMENTS;
  else
    return SEARCH_IN_NODE_NAMES;
}


gboolean
quarry_find_dialog_get_close_automatically (QuarryFindDialog *dialog)
{
  g_return_val_if_fail (QUARRY_IS_FIND_DIALOG (dialog), FALSE);

  return (gtk_toggle_button_get_active
	  (dialog->close_automatically_toggle_button));
}


void
quarry_find_dialog_set_text_to_find (QuarryFindDialog *dialog,
				     const gchar *text_to_find)
{
  g_return_if_fail (QUARRY_IS_FIND_DIALOG (dialog));

  gtk_entry_set_text (dialog->search_for_entry, text_to_find);
}


void
quarry_find_dialog_set_search_history (QuarryFindDialog *dialog,
				       const StringList *strings)
{
#if GTK_2_4_OR_LATER

  GtkComboBox *combo_box;
  GtkTreeModel *tree_model;
  GtkTreeIter dummy_iterator;
  StringListItem *item;

  g_return_if_fail (QUARRY_IS_FIND_DIALOG (dialog));
  g_return_if_fail (strings != NULL);

  combo_box  = GTK_COMBO_BOX (gtk_widget_get_parent
			      (GTK_WIDGET (dialog->search_for_entry)));
  tree_model = gtk_combo_box_get_model (combo_box);

  while (gtk_tree_model_get_iter_first (tree_model, &dummy_iterator))
    gtk_combo_box_remove_text (combo_box, 0);

  for (item = strings->first; item; item = item->next)
    gtk_combo_box_append_text (combo_box, item->text);

#else  /* not GTK_2_4_OR_LATER */

  UNUSED (dialog);
  UNUSED (strings);

#endif /* not GTK_2_4_OR_LATER */
}


void
quarry_find_dialog_set_case_sensitive (QuarryFindDialog *dialog,
				       gboolean case_sensitive)
{
  g_return_if_fail (QUARRY_IS_FIND_DIALOG (dialog));

  gtk_toggle_button_set_active (dialog->case_sensitive_toggle_button,
				case_sensitive);
}


void
quarry_find_dialog_set_whole_words_only (QuarryFindDialog *dialog,
					 gboolean whole_words_only)
{
  g_return_if_fail (QUARRY_IS_FIND_DIALOG (dialog));

  gtk_toggle_button_set_active (dialog->whole_words_only_toggle_button,
				whole_words_only);
}


void
quarry_find_dialog_set_wrap_around (QuarryFindDialog *dialog,
				    gboolean wrap_around)
{
  g_return_if_fail (QUARRY_IS_FIND_DIALOG (dialog));

  gtk_toggle_button_set_active (dialog->wrap_around_toggle_button,
				wrap_around);
}


void
quarry_find_dialog_set_search_whole_game_tree (QuarryFindDialog *dialog,
					       gboolean search_whole_game_tree)
{
  g_return_if_fail (QUARRY_IS_FIND_DIALOG (dialog));

  gtk_toggle_button_set_active (dialog->search_current_node_only_toggle_button,
				!search_whole_game_tree);
}


void
quarry_find_dialog_set_search_in (QuarryFindDialog *dialog, gint search_in)
{
  g_return_if_fail (QUARRY_IS_FIND_DIALOG (dialog));
  g_return_if_fail (search_in    == SEARCH_EVERYWHERE
		    || search_in == SEARCH_IN_COMMENTS
		    || search_in == SEARCH_IN_NODE_NAMES);

  gtk_toggle_button_set_active (dialog->search_scope_toggle_buttons[search_in],
				TRUE);
}


void
quarry_find_dialog_set_close_automatically (QuarryFindDialog *dialog,
					    gboolean close_automatically)
{
  g_return_if_fail (QUARRY_IS_FIND_DIALOG (dialog));

  gtk_toggle_button_set_active (dialog->close_automatically_toggle_button,
				close_automatically);
}


static void
parameters_changed (QuarryFindDialog *dialog)
{
  gboolean sensitive = (* gtk_entry_get_text (dialog->search_for_entry) != 0);

  gtk_dialog_set_response_sensitive (&dialog->dialog,
				     QUARRY_FIND_DIALOG_FIND_NEXT,
				     sensitive);
  gtk_dialog_set_response_sensitive (&dialog->dialog,
				     QUARRY_FIND_DIALOG_FIND_PREVIOUS,
				     sensitive);
}


/* This function finds text occurences in comments of current SGF game
 * tree's nodes.  We don't use GtkTreeBuffer built-in search, because
 * it would have been very inefficient: we would have had to load each
 * comment in a buffer before searching.  Also, GtkTreeBuffer
 * apparently doesn't care much for Unicode character collation etc.
 *
 * FIMXE: Arguments are really many...
 */
gboolean
quarry_find_text (const gchar *text_to_find,
		  QuarryFindDialogSearchDirection direction,
		  gboolean case_sensitive, gboolean whole_words_only,
		  gboolean wrap_around, gboolean search_whole_game_tree,
		  gint search_in,
		  GtkTextBuffer *text_buffer, gboolean node_name_inserted,
		  SgfGameTree *sgf_tree,
		  void (* switch_to_given_node) (void *, SgfNode *),
		  void *user_data)
{
  GtkTextIter selection_iterator;

  gchar *text_to_find_normalized;
  const gchar *text_to_search_in;
  gchar *text_to_search_in_normalized;
  gchar *text_to_free;
  const gchar *occurence;
  gint base_offset = 0;
  SgfType property_type;

  char * (* do_search) (const char *haystack, const char *needle);

  text_to_find_normalized = get_normalized_text (text_to_find,
						 -1, case_sensitive);

  if (direction == QUARRY_FIND_DIALOG_FIND_NEXT)
    do_search = (whole_words_only ? strstr_whole_word : strstr);
  else {
    /* Apparently, g_strrstr() uses naive search, but let's not care. */
    do_search = (whole_words_only ? strrstr_whole_word : g_strrstr);
  }

  /* First search in the right portion of the current node's
   * comments.
   */

  if (direction == QUARRY_FIND_DIALOG_FIND_NEXT) {
    gtk_text_buffer_get_selection_bounds (text_buffer,
					  NULL, &selection_iterator);
    text_to_free = get_bottom_buffer_part (text_buffer, node_name_inserted,
					   &selection_iterator, search_in);
  }
  else {
    gtk_text_buffer_get_selection_bounds (text_buffer,
					  &selection_iterator, NULL);
    text_to_free = get_top_buffer_part (text_buffer, node_name_inserted,
					&selection_iterator, search_in);
  }

  if (text_to_free) {
    text_to_search_in_normalized = get_normalized_text (text_to_free, -1,
							case_sensitive);

    occurence = do_search (text_to_search_in_normalized,
			   text_to_find_normalized);
    if (occurence) {
      text_to_search_in = text_to_free;
      property_type	= SGF_UNKNOWN;

      if (direction == QUARRY_FIND_DIALOG_FIND_NEXT)
	base_offset = gtk_text_iter_get_offset (&selection_iterator);

      goto found;
    }

    g_free (text_to_search_in_normalized);
    g_free (text_to_free);
  }

  /* Next traverse the game tree if requested. */
  if (search_whole_game_tree) {
    SgfNode *occurence_node = sgf_tree->current_node;
    SgfNode * (* do_traverse) (const SgfNode *sgf_node)
      = (direction == QUARRY_FIND_DIALOG_FIND_NEXT
	 ? sgf_node_traverse_forward : sgf_node_traverse_backward);
    SgfType first_property_type;

    switch (search_in) {
    case SEARCH_EVERYWHERE:
      first_property_type = (direction == QUARRY_FIND_DIALOG_FIND_NEXT
			     ? SGF_NODE_NAME : SGF_COMMENT);
      break;

    case SEARCH_IN_COMMENTS:
      first_property_type = SGF_COMMENT;
      break;

    case SEARCH_IN_NODE_NAMES:
      first_property_type = SGF_NODE_NAME;
      break;

    default:
      g_assert_not_reached ();
      return FALSE;
    }

    while (1) {
      occurence_node = do_traverse (occurence_node);

      if (!occurence_node) {
	if (!wrap_around)
	  break;

	if (direction == QUARRY_FIND_DIALOG_FIND_NEXT)
	  occurence_node = sgf_game_tree_traverse_forward (sgf_tree);
	else
	  occurence_node = sgf_game_tree_traverse_backward (sgf_tree);
      }

      /* Can happen when wrapping around. */
      if (occurence_node == sgf_tree->current_node)
	break;

      property_type = first_property_type;

      do {
	text_to_search_in = sgf_node_get_text_property_value (occurence_node,
							      property_type);
	if (text_to_search_in) {
	  text_to_search_in_normalized
	    = get_normalized_text (text_to_search_in, -1, case_sensitive);

	  occurence = do_search (text_to_search_in_normalized,
				 text_to_find_normalized);
	  if (occurence) {
	    switch_to_given_node (user_data, occurence_node);
	    text_to_free = NULL;

	    goto found;
	  }

	  g_free (text_to_search_in_normalized);
	}

	if (search_in != SEARCH_EVERYWHERE)
	  break;

	property_type = (SGF_COMMENT + SGF_NODE_NAME) - property_type;
      } while (property_type != first_property_type);
    }
  }

  /* Finally, wrap around in the current node's comment, if
   * requested.
   */
  if (wrap_around) {
    /* The `selection_iterator' is already set. */
    if (direction == QUARRY_FIND_DIALOG_FIND_NEXT)
      text_to_free = get_top_buffer_part (text_buffer, node_name_inserted,
					  &selection_iterator, search_in);
    else
      text_to_free = get_bottom_buffer_part (text_buffer, node_name_inserted,
					     &selection_iterator, search_in);

    if (text_to_free) {
      text_to_search_in_normalized = get_normalized_text (text_to_free, -1,
							  case_sensitive);

      occurence = do_search (text_to_search_in_normalized,
			     text_to_find_normalized);
      if (occurence) {
	text_to_search_in = text_to_free;
	property_type	  = SGF_UNKNOWN;

	if (direction == QUARRY_FIND_DIALOG_FIND_PREVIOUS)
	  base_offset = gtk_text_iter_get_offset (&selection_iterator);

	goto found;
      }

      g_free (text_to_search_in_normalized);
      g_free (text_to_free);
    }
  }

  /* Nothing found. */
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
    GtkTextIter insertion_iterator;
    GtkTextIter bound_iterator;

    /* If `text_to_search_in' doesn't include node name, while the
     * text buffer does, increase `base_offset' accordingly.
     */
    if (node_name_inserted
	&& (property_type == SGF_COMMENT
	    || (property_type == SGF_UNKNOWN && base_offset == 0
		&& search_in == SEARCH_IN_COMMENTS))) {
      GtkTextIter second_line_iterator;

      gtk_text_buffer_get_iter_at_line (text_buffer, &second_line_iterator, 1);
      base_offset += gtk_text_iter_get_offset (&second_line_iterator);
    }

    if (property_type == SGF_COMMENT) {
      /* First skip all full lines, to speed the things up in case of
       * a very long text.
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

      /* Only search in the next line. */
      remaining_text_length = 0;
      while (last_line[remaining_text_length]
	     && last_line[remaining_text_length] != '\n')
	remaining_text_length++;
    }
    else {
      last_line_normalized  = text_to_search_in_normalized;
      last_line		    = text_to_search_in;
      remaining_text_length = strlen (last_line);
    }

    last_line_offset
      = get_offset_in_original_text (last_line, remaining_text_length,
				     occurence - last_line_normalized,
				     case_sensitive,
				     occurence - last_line_normalized);

    gtk_text_buffer_get_iter_at_offset (text_buffer, &insertion_iterator,
					base_offset + last_line_offset);

    last_line_offset
      = get_offset_in_original_text (last_line, remaining_text_length,
				     ((occurence
				       + strlen (text_to_find_normalized))
				      - last_line_normalized),
				     case_sensitive,
				     last_line_offset + strlen (text_to_find));

    gtk_text_buffer_get_iter_at_offset (text_buffer, &bound_iterator,
					base_offset + last_line_offset);

    gtk_text_buffer_select_range (text_buffer,
				  &insertion_iterator, &bound_iterator);
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


static char *
get_top_buffer_part (GtkTextBuffer *text_buffer, gboolean node_name_inserted,
		     const GtkTextIter *boundary_iterator, gint search_in)
{
  GtkTextIter start_iterator;
  GtkTextIter real_boundary_iterator = *boundary_iterator;

  if (search_in != SEARCH_IN_COMMENTS
      || !node_name_inserted) {
    gtk_text_buffer_get_start_iter (text_buffer,
				    &start_iterator);
  }
  else
    gtk_text_buffer_get_iter_at_line (text_buffer, &start_iterator, 1);

  if (search_in == SEARCH_IN_NODE_NAMES) {
    if (!node_name_inserted)
      return NULL;

    if (gtk_text_iter_get_line (&real_boundary_iterator) > 0) {
      gtk_text_buffer_get_iter_at_line (text_buffer,
					&real_boundary_iterator, 1);
      gtk_text_iter_backward_char (&real_boundary_iterator);
    }
  }

  if (gtk_text_iter_compare (&start_iterator, &real_boundary_iterator) >= 0)
    return NULL;

  return gtk_text_iter_get_text (&start_iterator, &real_boundary_iterator);
}


static char *
get_bottom_buffer_part (GtkTextBuffer *text_buffer,
			gboolean node_name_inserted,
			const GtkTextIter *boundary_iterator, gint search_in)
{
  GtkTextIter real_boundary_iterator = *boundary_iterator;
  GtkTextIter end_iterator;

  if (search_in == SEARCH_IN_COMMENTS
      && node_name_inserted
      && gtk_text_iter_get_line (&real_boundary_iterator) == 0)
    gtk_text_buffer_get_iter_at_line (text_buffer, &real_boundary_iterator, 1);

  if (search_in != SEARCH_IN_NODE_NAMES)
    gtk_text_buffer_get_end_iter (text_buffer, &end_iterator);
  else {
    if (!node_name_inserted)
      return NULL;

    gtk_text_buffer_get_iter_at_line (text_buffer, &end_iterator, 1);
    gtk_text_iter_backward_char (&end_iterator);
  }

  if (gtk_text_iter_compare (&real_boundary_iterator, &end_iterator) >= 0)
    return NULL;

  return gtk_text_iter_get_text (&real_boundary_iterator, &end_iterator);
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


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
