/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2006 Paul Pogonyshev.                             *
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


#include "quarry-move-number-dialog.h"

#include "gtk-utils.h"

#include <gtk/gtk.h>


static void	quarry_move_number_dialog_init
		  (QuarryMoveNumberDialog *dialog);


GType
quarry_move_number_dialog_get_type (void)
{
  static GType move_number_dialog_type = 0;

  if (!move_number_dialog_type) {
    static const GTypeInfo move_number_dialog_info = {
      sizeof (QuarryMoveNumberDialogClass),
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      sizeof (QuarryMoveNumberDialog),
      0,
      (GInstanceInitFunc) quarry_move_number_dialog_init,
      NULL
    };

    move_number_dialog_type
      = g_type_register_static (GTK_TYPE_DIALOG, "QuarryMoveNumberDialog",
				&move_number_dialog_info, 0);
  }

  return move_number_dialog_type;
}


static void
quarry_move_number_dialog_init (QuarryMoveNumberDialog *dialog)
{
  static const gchar *radio_labels[2]
    = { N_("Use se_quential move number"), N_("_Set specific move number:") };

  GtkWidget **radio_buttons = (GtkWidget **) dialog->toggle_buttons;
  GtkWidget *spin_button;
  GtkWidget *hbox;
  GtkWidget *vbox;

  gtk_utils_create_radio_chain (radio_buttons, radio_labels, 2);

  dialog->move_number_adjustment
    = ((GtkAdjustment *)
       gtk_adjustment_new (1.0, 1.0, G_MAXINT, 1.0, 10.0, 0.0));
  spin_button = gtk_utils_create_spin_button (dialog->move_number_adjustment,
					      0.0, 0, TRUE);

  gtk_utils_set_sensitive_on_toggle (dialog->toggle_buttons[1], spin_button,
				     FALSE);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				radio_buttons[1], GTK_UTILS_FILL,
				spin_button, GTK_UTILS_FILL, NULL);
  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				radio_buttons[0], GTK_UTILS_FILL,
				hbox, GTK_UTILS_FILL, NULL);

  gtk_widget_show_all (vbox);
  gtk_utils_standardize_dialog (&dialog->dialog, vbox);

  gtk_dialog_add_buttons (&dialog->dialog,
			  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			  GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response (&dialog->dialog, GTK_RESPONSE_OK);

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  dialog->sequential_move_number = 1;
}


GtkWidget *
quarry_move_number_dialog_new (void)
{
  return GTK_WIDGET (g_object_new (QUARRY_TYPE_MOVE_NUMBER_DIALOG, NULL));
}


gboolean
quarry_move_number_dialog_get_use_sequential_move_number
  (const QuarryMoveNumberDialog *dialog)
{
  g_return_val_if_fail (QUARRY_IS_MOVE_NUMBER_DIALOG (dialog), TRUE);

  return gtk_toggle_button_get_active (dialog->toggle_buttons[0]);
}


gint
quarry_move_number_dialog_get_specific_move_number
  (const QuarryMoveNumberDialog *dialog)
{
  g_return_val_if_fail (QUARRY_IS_MOVE_NUMBER_DIALOG (dialog), 1);
  g_return_val_if_fail ((gtk_toggle_button_get_active
			 (dialog->toggle_buttons[1])),
			dialog->sequential_move_number);

  return gtk_adjustment_get_value (dialog->move_number_adjustment);
}


void
quarry_move_number_dialog_set_sequential_move_number
  (QuarryMoveNumberDialog *dialog, gint move_number)
{
  GtkLabel *label;
  gchar *text;

  g_return_if_fail (QUARRY_IS_MOVE_NUMBER_DIALOG (dialog));
  g_return_if_fail (move_number >= 0);

  dialog->sequential_move_number = move_number;

  text = g_strdup_printf ("%s (%d)",
			  _("Use se_quential move number"), move_number);

  label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (dialog->toggle_buttons[0])));
  gtk_label_set_text_with_mnemonic (label, text);

  g_free (text);
}


void
quarry_move_number_dialog_set_specific_move_number
  (QuarryMoveNumberDialog *dialog, gint move_number)
{
  g_return_if_fail (QUARRY_IS_MOVE_NUMBER_DIALOG (dialog));
  g_return_if_fail (move_number > 0);

  gtk_adjustment_set_value (dialog->move_number_adjustment, move_number);
}


void
quarry_move_number_dialog_set_use_sequential_move_number
  (QuarryMoveNumberDialog *dialog, gboolean use_sequential_move_number)
{
  g_return_if_fail (QUARRY_IS_MOVE_NUMBER_DIALOG (dialog));

  if (use_sequential_move_number) {
    gtk_adjustment_set_value (dialog->move_number_adjustment,
			      dialog->sequential_move_number);
    gtk_toggle_button_set_active (dialog->toggle_buttons[0], TRUE);
  }
  else
    gtk_toggle_button_set_active (dialog->toggle_buttons[1], TRUE);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
