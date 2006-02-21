/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005, 2006 Paul Pogonyshev.           *
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

#include <gtk/gtk.h>
#include <stdarg.h>
#include <string.h>


static void	quarry_message_dialog_class_init
		  (QuarryMessageDialogClass *class);
static void	quarry_message_dialog_init (QuarryMessageDialog *dialog);

static void	quarry_message_dialog_finalize (GObject *object);

static void	set_label_text (QuarryMessageDialog *dialog);


static GtkDialogClass  *parent_class;


GType
quarry_message_dialog_get_type (void)
{
  static GType message_dialog_type = 0;

  if (!message_dialog_type) {
    static const GTypeInfo message_dialog_info = {
      sizeof (QuarryMessageDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) quarry_message_dialog_class_init,
      NULL,
      NULL,
      sizeof (QuarryMessageDialog),
      1,
      (GInstanceInitFunc) quarry_message_dialog_init,
      NULL
    };

    message_dialog_type
      = g_type_register_static (GTK_TYPE_DIALOG, "QuarryMessageDialog",
				&message_dialog_info, 0);
  }

  return message_dialog_type;
}


static void
quarry_message_dialog_class_init (QuarryMessageDialogClass *class)
{
  parent_class = g_type_class_peek_parent (class);

  G_OBJECT_CLASS (class)->finalize = quarry_message_dialog_finalize;
}


static void
quarry_message_dialog_init (QuarryMessageDialog *dialog)
{
  GtkWindow *window = GTK_WINDOW (dialog);
  GtkWidget *label;
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

  label		= gtk_label_new (NULL);
  dialog->label = GTK_LABEL (label);

  gtk_label_set_line_wrap (dialog->label, TRUE);
  gtk_label_set_selectable (dialog->label, TRUE);
  gtk_misc_set_alignment (GTK_MISC (dialog->label), 0.0, 0.0);

  gtk_widget_show (label);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				dialog->image, GTK_UTILS_FILL,
				label, GTK_UTILS_FILL, NULL);
  gtk_widget_show (hbox);

  gtk_utils_standardize_dialog (&dialog->dialog, hbox);
  gtk_box_set_spacing (GTK_BOX (dialog->dialog.vbox),
		       QUARRY_SPACING_VERY_BIG);

  dialog->primary_text	 = NULL;
  dialog->secondary_text = NULL;
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
    g_critical ("unhandled button set type %d", buttons);
  }

  quarry_message_dialog_set_icon (dialog, icon_stock_id);
  quarry_message_dialog_format_primary_text_valist (dialog,
						    primary_text_format_string,
						    arguments);
  quarry_message_dialog_set_secondary_text (dialog, secondary_text);

  return GTK_WIDGET (dialog);
}


void
quarry_message_dialog_finalize (GObject *object)
{
  QuarryMessageDialog *dialog = QUARRY_MESSAGE_DIALOG (object);

  g_free (dialog->primary_text);
  g_free (dialog->secondary_text);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


void
quarry_message_dialog_set_icon (QuarryMessageDialog *dialog,
				const gchar *icon_stock_id)
{
  g_return_if_fail (QUARRY_IS_MESSAGE_DIALOG (dialog));

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
  g_return_if_fail (QUARRY_IS_MESSAGE_DIALOG (dialog));

  if (primary_text != NULL
      ? (dialog->primary_text == NULL
	 || strcmp (dialog->primary_text, primary_text) != 0)
      : dialog->primary_text != NULL) {
    g_free (dialog->primary_text);
    dialog->primary_text = g_strdup (primary_text);

    set_label_text (dialog);
  }
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
  g_return_if_fail (QUARRY_IS_MESSAGE_DIALOG (dialog));

  if (secondary_text != NULL
      ? (dialog->secondary_text == NULL
	 || strcmp (dialog->secondary_text, secondary_text) != 0)
      : dialog->secondary_text != NULL) {
    g_free (dialog->secondary_text);
    dialog->secondary_text = g_strdup (secondary_text);

    set_label_text (dialog);
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


static void
set_label_text (QuarryMessageDialog *dialog)
{
  if (dialog->primary_text) {
    gchar *escaped_primary_text = g_markup_escape_text (dialog->primary_text,
							-1);
    gchar *marked_up_text;

    if (dialog->secondary_text) {
      gchar *escaped_secondary_text
	= g_markup_escape_text (dialog->secondary_text, -1);

      marked_up_text = g_strconcat ("<span weight=\"bold\" size=\"larger\">",
				    escaped_primary_text, "</span>\n\n",
				    escaped_secondary_text, NULL);
      g_free (escaped_secondary_text);
    }
    else {
      marked_up_text = g_strconcat ("<span weight=\"bold\" size=\"larger\">",
				    escaped_primary_text, "</span>", NULL);
    }

    gtk_label_set_markup (dialog->label, marked_up_text);

    g_free (marked_up_text);
    g_free (escaped_primary_text);
  }
  else
    gtk_label_set_text (dialog->label, dialog->secondary_text);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
