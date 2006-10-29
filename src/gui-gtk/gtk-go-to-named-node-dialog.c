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


#include "gtk-go-to-named-node-dialog.h"


#ifdef GTK_TYPE_GO_TO_NAMED_NODE_DIALOG


#include "gtk-preferences.h"
#include "gtk-utils.h"
#include "gui-back-end.h"
#include "quarry-stock.h"
#include "time-control.h"
#include "sgf.h"

#include <gtk/gtk.h>
#include <string.h>


enum {
  NODE_NAME,
  NODE_OBJECT,
  NUM_COLUMNS
};


static void	 gtk_go_to_named_node_dialog_class_init
		   (GtkGoToNamedNodeDialogClass *class);
static void	 gtk_go_to_named_node_dialog_init
		   (GtkGoToNamedNodeDialog *dialog);

static void	 gtk_go_to_named_node_dialog_finalize (GObject *object);
static void	 free_node_list (const char *node_name, GSList *node_list);

static void	 entered_node_name_changed (GtkEntry *node_name_entry,
					    GtkGoToNamedNodeDialog *dialog);
static gboolean	 match_selected (GtkGoToNamedNodeDialog *dialog,
				 GtkTreeModel *tree_model,
				 GtkTreeIter *iterator);

static void	 set_selected_node (GtkGoToNamedNodeDialog *dialog,
				    SgfNode *sgf_node);


static GtkDialogClass	*parent_class;

static GtkTextTag	*special_comment_tag;
static GtkTextTagTable	*dialog_tag_table;


GType
gtk_go_to_named_node_dialog_get_type (void)
{
  static GType go_to_named_node_dialog_type = 0;

  if (!go_to_named_node_dialog_type) {
    static GTypeInfo go_to_named_node_dialog_info = {
      sizeof (GtkGoToNamedNodeDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_go_to_named_node_dialog_class_init,
      NULL,
      NULL,
      sizeof (GtkGoToNamedNodeDialog),
      0,
      (GInstanceInitFunc) gtk_go_to_named_node_dialog_init,
      NULL
    };

    go_to_named_node_dialog_type
      = g_type_register_static (GTK_TYPE_DIALOG, "GtkGoToNamedNodeDialog",
				&go_to_named_node_dialog_info, 0);
  }

  return go_to_named_node_dialog_type;
}


static void
gtk_go_to_named_node_dialog_class_init (GtkGoToNamedNodeDialogClass *class)
{
  parent_class = g_type_class_peek_parent (class);

  G_OBJECT_CLASS (class)->finalize = gtk_go_to_named_node_dialog_finalize;

  /* Create the text tag and tag table that will be used in all
   * dialogs.  The tag is used to emphasize "There is no such node"
   * text.
   */

  special_comment_tag = gtk_text_tag_new (NULL);
  g_object_set (special_comment_tag, "style", PANGO_STYLE_ITALIC, NULL);

  dialog_tag_table = gtk_text_tag_table_new ();
  gui_back_end_register_object_to_finalize (dialog_tag_table);

  gtk_text_tag_table_add (dialog_tag_table, special_comment_tag);
  g_object_unref (special_comment_tag);
}


static void
gtk_go_to_named_node_dialog_init (GtkGoToNamedNodeDialog *dialog)
{
  GtkWidget *entry;
  GtkWidget *name_label;
  GtkWidget *hbox;
  GtkWidget *comment_label;
  GtkWidget *text_view_widget;
  GtkTextView *text_view;
  GtkWidget *scrolled_window;
  GtkWidget *vbox1;
  GtkWidget *vbox2;

  gtk_window_set_title (GTK_WINDOW (dialog), _("Go to Named Node"));

  entry = gtk_utils_create_entry (NULL, RETURN_ACTIVATES_DEFAULT);
  gtk_entry_set_width_chars (GTK_ENTRY (entry), 30);

  dialog->entry_changed_handler_id
    = g_signal_connect (entry, "changed",
			G_CALLBACK (entered_node_name_changed), dialog);

  dialog->entry_completion = gtk_entry_completion_new ();
  gtk_entry_set_completion (GTK_ENTRY (entry), dialog->entry_completion);

  g_signal_connect_swapped (dialog->entry_completion, "match-selected",
			    G_CALLBACK (match_selected), dialog);

  name_label = gtk_utils_create_mnemonic_label (_("Node _name:"), entry);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				name_label, GTK_UTILS_FILL,
				entry, GTK_UTILS_PACK_DEFAULT, NULL);

  comment_label = gtk_label_new (_("Node comment:"));
  gtk_utils_create_size_group (GTK_SIZE_GROUP_HORIZONTAL,
			       name_label, comment_label, NULL);

  dialog->comment_buffer = gtk_text_buffer_new (dialog_tag_table);

  text_view_widget = gtk_text_view_new_with_buffer (dialog->comment_buffer);
  text_view = GTK_TEXT_VIEW (text_view_widget);

  gtk_text_view_set_editable (text_view, FALSE);
  gtk_text_view_set_left_margin (text_view, QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_right_margin (text_view, QUARRY_SPACING_VERY_SMALL);
  gtk_text_view_set_wrap_mode (text_view, GTK_WRAP_WORD);

  scrolled_window = gtk_utils_make_widget_scrollable (text_view_widget,
						      GTK_POLICY_AUTOMATIC,
						      GTK_POLICY_AUTOMATIC);

  vbox1 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				 gtk_utils_align_widget (comment_label,
							 0.0, 0.0),
				 GTK_UTILS_FILL,
				 scrolled_window, GTK_UTILS_PACK_DEFAULT,
				 NULL);
  dialog->comment_widgets = vbox1;

  vbox2 = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING,
				 hbox, GTK_UTILS_FILL,
				 vbox1, GTK_UTILS_PACK_DEFAULT, NULL);

  gtk_widget_show_all (vbox2);

  gtk_utils_standardize_dialog (&dialog->dialog, vbox2);

  gtk_dialog_add_buttons (&dialog->dialog,
			  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			  QUARRY_STOCK_GO_TO_NODE, GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response (&dialog->dialog, GTK_RESPONSE_OK);

  gtk_dialog_set_response_sensitive (&dialog->dialog, GTK_RESPONSE_OK, FALSE);

  gtk_dialog_set_has_separator (&dialog->dialog, FALSE);

  set_selected_node (dialog, NULL);
}


GtkWidget *
gtk_go_to_named_node_dialog_new (SgfGameTree *sgf_tree)
{
  SgfNode *sgf_node;
  GTree *completion_tree;
  GtkListStore *completions = gtk_list_store_new (NUM_COLUMNS,
						  G_TYPE_STRING,
						  G_TYPE_POINTER);

  g_return_val_if_fail (sgf_tree, NULL);

  /* Note: cannot use value destructor here, because we build values
   * ``incrementally'' with g_tree_replace() below.
   */
  completion_tree = g_tree_new ((GCompareFunc) strcmp);

  for (sgf_node = sgf_game_tree_traverse_forward (sgf_tree);
       sgf_node; sgf_node = sgf_node_traverse_forward (sgf_node)) {
    const char *node_name = sgf_node_get_text_property_value (sgf_node,
							      SGF_NODE_NAME);

    if (node_name) {
      GtkTreeIter iterator;
      GSList *node_list;

      node_list = g_tree_lookup (completion_tree, node_name);
      g_tree_replace (completion_tree, (gpointer) node_name,
		      g_slist_append (node_list, sgf_node));

      gtk_list_store_append (completions, &iterator);
      gtk_list_store_set (completions, &iterator,
			  NODE_NAME, node_name, NODE_OBJECT, sgf_node, -1);
    }
  }

  if (g_tree_nnodes (completion_tree) > 0) {
    GObject *dialog_object = g_object_new (GTK_TYPE_GO_TO_NAMED_NODE_DIALOG,
					   NULL);
    GtkGoToNamedNodeDialog *dialog
      = GTK_GO_TO_NAMED_NODE_DIALOG (dialog_object);

    dialog->completion_tree = completion_tree;

    gtk_entry_completion_set_model (dialog->entry_completion,
				    GTK_TREE_MODEL (completions));
    gtk_entry_completion_set_text_column (dialog->entry_completion, NODE_NAME);

    return GTK_WIDGET (dialog);
  }
  else {
    g_tree_destroy (completion_tree);
    g_object_unref (completions);

    return NULL;
  }
}


static void
gtk_go_to_named_node_dialog_finalize (GObject *object)
{
  GTree *completion_tree
    = GTK_GO_TO_NAMED_NODE_DIALOG (object)->completion_tree;

  g_tree_foreach (completion_tree, (GTraverseFunc) free_node_list, NULL);
  g_tree_destroy (completion_tree);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
free_node_list (const char *node_name, GSList *node_list)
{
  UNUSED (node_name);

  g_slist_free (node_list);
}


static void
entered_node_name_changed (GtkEntry *node_name_entry,
			   GtkGoToNamedNodeDialog *dialog)
{
  const gchar *node_name = gtk_entry_get_text (node_name_entry);
  GSList *node_list = (GSList *) g_tree_lookup (dialog->completion_tree,
						node_name);

  set_selected_node (dialog, node_list ? (SgfNode *) node_list->data : NULL);
}


static gboolean
match_selected (GtkGoToNamedNodeDialog *dialog, GtkTreeModel *tree_model,
		GtkTreeIter *iterator)
{
  GtkWidget *entry = gtk_entry_completion_get_entry (dialog->entry_completion);
  gchar *node_name;
  SgfNode *sgf_node;

  gtk_tree_model_get (tree_model, iterator,
		      NODE_NAME, &node_name, NODE_OBJECT, &sgf_node, -1);
  set_selected_node (dialog, sgf_node);

  /* Replace entry text ourselves, allows to black
   * entered_node_name_changed() hook.
   */
  g_signal_handler_block (entry, dialog->entry_changed_handler_id);
  gtk_entry_set_text (GTK_ENTRY (entry), node_name);
  gtk_editable_set_position (GTK_EDITABLE (entry), -1);
  g_signal_handler_unblock (entry, dialog->entry_changed_handler_id);

  g_free (node_name);

  return TRUE;
}


static void
set_selected_node (GtkGoToNamedNodeDialog *dialog, SgfNode *sgf_node)
{
  const char *node_comment = NULL;
  const char *comment_text;

  dialog->selected_node = sgf_node;

  if (sgf_node) {
    node_comment = sgf_node_get_text_property_value (sgf_node, SGF_COMMENT);
    comment_text = (node_comment ? node_comment : Q_("comment|Empty"));
  }
  else
    comment_text = _("There is no such node");

  gtk_utils_set_text_buffer_text (dialog->comment_buffer, comment_text);

  if (!node_comment) {
    GtkTextIter start_iterator;
    GtkTextIter end_iterator;

    gtk_text_buffer_get_bounds (dialog->comment_buffer,
				&start_iterator, &end_iterator);
    gtk_text_buffer_apply_tag (dialog->comment_buffer, special_comment_tag,
			       &start_iterator, &end_iterator);
  }

  gtk_widget_set_sensitive (dialog->comment_widgets, node_comment != NULL);
  gtk_dialog_set_response_sensitive (&dialog->dialog, GTK_RESPONSE_OK,
				     sgf_node != NULL);
}


#endif /* GTK_TYPE_GO_TO_NAMED_NODE_DIALOG */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
