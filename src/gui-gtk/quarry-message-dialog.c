/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005 Paul Pogonyshev.                 *
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


#include "quarry-message-dialog.h"

#include "gtk-utils.h"

#include <assert.h>
#include <gtk/gtk.h>
#include <stdarg.h>


static void	 quarry_message_dialog_init (QuarryMessageDialog *dialog);


GType
quarry_message_dialog_get_type (void)
{
  static GType quarry_message_dialog_type = 0;

  if (!quarry_message_dialog_type) {
    static const GTypeInfo quarry_message_dialog_info = {
      sizeof (QuarryMessageDialogClass),
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      sizeof (QuarryMessageDialog),
      1,
      (GInstanceInitFunc) quarry_message_dialog_init,
      NULL
    };

    quarry_message_dialog_type
      = g_type_register_static (GTK_TYPE_DIALOG, "QuarryMessageDialog",
				&quarry_message_dialog_info, 0);
  }

  return quarry_message_dialog_type;
}


static void
quarry_message_dialog_init (QuarryMessageDialog *dialog)
{
  GtkWindow *window = GTK_WINDOW (dialog);
  GtkWidget *primary_text_label;
  GtkWidget *secondary_text_label;
  GtkWidget *vbox;
  GtkWidget *hbox;

  gtk_window_set_title (window, "");
  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_skip_pager_hint (window, TRUE);

  /* Note: disabled because KDE window manager doesn't give focus to
   * message dialog after this call (is it a bug or what?)
   */
#if 0
  gtk_window_set_skip_taskbar_hint (window, TRUE);
#endif

  gtk_dialog_set_has_separator (&dialog->dialog, FALSE);

  dialog->image = gtk_image_new ();
  gtk_misc_set_alignment (GTK_MISC (dialog->image), 0.5, 0.0);

  primary_text_label	     = gtk_label_new (NULL);
  dialog->primary_text_label = GTK_LABEL (primary_text_label);

  gtk_label_set_line_wrap (dialog->primary_text_label, TRUE);
  gtk_label_set_selectable (dialog->primary_text_label, TRUE);
  gtk_misc_set_alignment (GTK_MISC (dialog->primary_text_label), 0.0, 0.0);

  gtk_widget_show (primary_text_label);

  secondary_text_label	       = gtk_label_new (NULL);
  dialog->secondary_text_label = GTK_LABEL (secondary_text_label);

  gtk_label_set_line_wrap (dialog->secondary_text_label, TRUE);
  gtk_label_set_selectable (dialog->secondary_text_label, TRUE);
  gtk_misc_set_alignment (GTK_MISC (dialog->secondary_text_label), 0.0, 0.0);

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, 0,
				primary_text_label, GTK_UTILS_FILL,
				secondary_text_label, GTK_UTILS_FILL, NULL);
  gtk_widget_show (vbox);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				dialog->image, GTK_UTILS_FILL,
				vbox, GTK_UTILS_FILL, NULL);
  gtk_widget_show (hbox);

  gtk_utils_standardize_dialog (&dialog->dialog, hbox);
  gtk_box_set_spacing (GTK_BOX (dialog->dialog.vbox),
		       QUARRY_SPACING_VERY_BIG);
}


GtkWidget *
quarry_message_dialog_new (GtkWindow *parent,
			   GtkButtonsType buttons, const gchar *icon_stock_id,
			   const gchar *secondary_text,
			   const gchar *primary_text_format_string, ...)
{
  GtkWidget *dialog;
  va_list arguments;

  va_start (arguments, primary_text_format_string);
  dialog = quarry_message_dialog_new_valist (parent, buttons, icon_stock_id,
					     secondary_text,
					     primary_text_format_string,
					     arguments);
  va_end (arguments);

  return dialog;
}


GtkWidget *
quarry_message_dialog_new_valist (GtkWindow *parent,
				  GtkButtonsType buttons,
				  const gchar *icon_stock_id,
				  const gchar *secondary_text,
				  const gchar *primary_text_format_string,
				  va_list arguments)
{
  QuarryMessageDialog *dialog = g_object_new (QUARRY_TYPE_MESSAGE_DIALOG,
					      NULL);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  switch (buttons) {
  case GTK_BUTTONS_NONE:
    break;

  case GTK_BUTTONS_OK:
    /* To enable Escape key. */
    gtk_dialog_add_button (&dialog->dialog, GTK_STOCK_OK, GTK_RESPONSE_CANCEL);
    break;

  case GTK_BUTTONS_CLOSE:
    gtk_dialog_add_button (&dialog->dialog,
			   GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
    break;

  case GTK_BUTTONS_CANCEL:
    gtk_dialog_add_button (&dialog->dialog,
			   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    break;

  case GTK_BUTTONS_YES_NO:
    gtk_dialog_add_buttons (&dialog->dialog,
			    GTK_STOCK_NO, GTK_RESPONSE_NO,
			    GTK_STOCK_YES, GTK_RESPONSE_YES, NULL);
    break;

  case GTK_BUTTONS_OK_CANCEL:
    gtk_dialog_add_buttons (&dialog->dialog,
			    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			    GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

  default:
    assert (0);
  }

  quarry_message_dialog_set_icon (dialog, icon_stock_id);
  quarry_message_dialog_format_primary_text_valist (dialog,
						    primary_text_format_string,
						    arguments);
  quarry_message_dialog_set_secondary_text (dialog, secondary_text);

  return GTK_WIDGET (dialog);
}


void
quarry_message_dialog_set_icon (QuarryMessageDialog *dialog,
				const gchar *icon_stock_id)
{
  assert (QUARRY_IS_MESSAGE_DIALOG (dialog));

  gtk_image_set_from_stock (GTK_IMAGE (dialog->image),
			    icon_stock_id, GTK_ICON_SIZE_DIALOG);
  if (icon_stock_id)
    gtk_widget_show (dialog->image);
  else
    gtk_widget_hide (dialog->image);
}


void
quarry_message_dialog_set_primary_text (QuarryMessageDialog *dialog,
					const gchar *primary_text)
{
  gchar *escaped_text;
  gchar *marked_up_text;

  assert (QUARRY_IS_MESSAGE_DIALOG (dialog));

  escaped_text	 = g_markup_escape_text (primary_text, -1);
  marked_up_text = g_strconcat ("<span weight=\"bold\" size=\"larger\">",
				escaped_text, "</span>", NULL);

  gtk_label_set_markup (dialog->primary_text_label, marked_up_text);

  g_free (marked_up_text);
  g_free (escaped_text);
}


void
quarry_message_dialog_format_primary_text
  (QuarryMessageDialog *dialog, const gchar *primary_text_format_string, ...)
{
  va_list arguments;

  va_start (arguments, primary_text_format_string);
  quarry_message_dialog_format_primary_text_valist (dialog,
						    primary_text_format_string,
						    arguments);
  va_end (arguments);
}


void
quarry_message_dialog_format_primary_text_valist
  (QuarryMessageDialog *dialog,
   const gchar *primary_text_format_string, va_list arguments)
{
  gchar *primary_text = g_strdup_vprintf (primary_text_format_string,
					  arguments);

  quarry_message_dialog_set_primary_text (dialog, primary_text);
  g_free (primary_text);
}


void
quarry_message_dialog_set_secondary_text (QuarryMessageDialog *dialog,
					  const gchar *secondary_text)
{
  assert (QUARRY_IS_MESSAGE_DIALOG (dialog));

  if (secondary_text && *secondary_text) {
    gchar *secondary_text_with_newline = g_strconcat ("\n", secondary_text,
						      NULL);

    gtk_label_set_text (dialog->secondary_text_label,
			secondary_text_with_newline);
    g_free (secondary_text_with_newline);

    gtk_widget_show (GTK_WIDGET (dialog->secondary_text_label));
  }
  else {
    gtk_label_set_text (dialog->secondary_text_label, NULL);
    gtk_widget_hide (GTK_WIDGET (dialog->secondary_text_label));
  }
}


void
quarry_message_dialog_format_secondary_text
  (QuarryMessageDialog *dialog, const gchar *secondary_text_format_string, ...)
{
  va_list arguments;

  va_start (arguments, secondary_text_format_string);
  quarry_message_dialog_format_secondary_text_valist
    (dialog, secondary_text_format_string, arguments);
  va_end (arguments);
}


void
quarry_message_dialog_format_secondary_text_valist
  (QuarryMessageDialog *dialog,
   const gchar *secondary_text_format_string, va_list arguments)
{
  gchar *secondary_text = g_strdup_vprintf (secondary_text_format_string,
					    arguments);

  quarry_message_dialog_set_secondary_text (dialog, secondary_text);
  g_free (secondary_text);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
