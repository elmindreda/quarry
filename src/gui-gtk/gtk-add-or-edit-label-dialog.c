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


#include "gtk-add-or-edit-label-dialog.h"

#include "gtk-utils.h"

#include <gtk/gtk.h>
#include <assert.h>


enum {
  NODE_NAME,
  NODE_OBJECT,
  NUM_COLUMNS
};


static void	 gtk_add_or_edit_label_dialog_init
		   (GtkAddOrEditLabelDialog *dialog);


GType
gtk_add_or_edit_label_dialog_get_type (void)
{
  static GType add_or_edit_label_dialog_type = 0;

  if (!add_or_edit_label_dialog_type) {
    static GTypeInfo add_or_edit_label_dialog_info = {
      sizeof (GtkAddOrEditLabelDialogClass),
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      sizeof (GtkAddOrEditLabelDialog),
      0,
      (GInstanceInitFunc) gtk_add_or_edit_label_dialog_init,
      NULL
    };

    add_or_edit_label_dialog_type
      = g_type_register_static (GTK_TYPE_DIALOG, "GtkAddOrEditLabelDialog",
				&add_or_edit_label_dialog_info, 0);
  }

  return add_or_edit_label_dialog_type;
}


static void
gtk_add_or_edit_label_dialog_init (GtkAddOrEditLabelDialog *dialog)
{
  static const gchar *hint_text
    = N_("Although board labels are not limited in length, it is better to "
	 "use only one or two characters long labels. Longer labels will "
	 "not be visible immediately or at all in most clients.");

  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *vbox;

  gtk_utils_make_window_only_horizontally_resizable (GTK_WINDOW (dialog));

  entry = gtk_utils_create_entry (NULL, RETURN_ACTIVATES_DEFAULT);
  dialog->label_entry = GTK_ENTRY (entry);

  label = gtk_utils_create_mnemonic_label (_("Label _text:"), entry);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				label, GTK_UTILS_FILL,
				entry, GTK_UTILS_PACK_DEFAULT, NULL);

  label = gtk_utils_create_left_aligned_label (_(hint_text));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				hbox, GTK_UTILS_FILL, label, GTK_UTILS_FILL,
				NULL);

  gtk_widget_show_all (vbox);

  gtk_utils_standardize_dialog (&dialog->dialog, vbox);

  gtk_dialog_add_buttons (&dialog->dialog,
			  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			  GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response (&dialog->dialog, GTK_RESPONSE_OK);
}


GtkWidget *
gtk_add_or_edit_label_dialog_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_ADD_OR_EDIT_LABEL_DIALOG, NULL));
}


void
gtk_add_or_edit_label_dialog_set_label_text (GtkAddOrEditLabelDialog *dialog,
					     const gchar *label_text)
{
  assert (GTK_IS_ADD_OR_EDIT_LABEL_DIALOG (dialog));

  gtk_entry_set_text (dialog->label_entry, label_text);
}


const gchar *
gtk_add_or_edit_label_dialog_get_label_text (GtkAddOrEditLabelDialog *dialog)
{
  assert (GTK_IS_ADD_OR_EDIT_LABEL_DIALOG (dialog));

  return gtk_entry_get_text (dialog->label_entry);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
