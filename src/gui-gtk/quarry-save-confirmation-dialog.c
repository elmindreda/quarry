/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005 Paul Pogonyshev.                             *
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


#include "quarry-save-confirmation-dialog.h"

#include "quarry-message-dialog.h"

#include <assert.h>
#include <gtk/gtk.h>
#include <stdarg.h>


static void	 quarry_save_confirmation_dialog_class_init
		   (QuarrySaveConfirmationDialogClass *class);
static void	 quarry_save_confirmation_dialog_init
		   (QuarrySaveConfirmationDialog *dialog);

static void	 quarry_save_confirmation_dialog_finalize
		   (GObject *object);

static gboolean	 update_secondary_text
		   (QuarrySaveConfirmationDialog *dialog);


static QuarryMessageDialogClass	 *parent_class;


GType
quarry_save_confirmation_dialog_get_type (void)
{
  static GType quarry_save_confirmation_dialog_type = 0;

  if (!quarry_save_confirmation_dialog_type) {
    static const GTypeInfo quarry_save_confirmation_dialog_info = {
      sizeof (QuarrySaveConfirmationDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) quarry_save_confirmation_dialog_class_init,
      NULL,
      NULL,
      sizeof (QuarrySaveConfirmationDialog),
      0,
      (GInstanceInitFunc) quarry_save_confirmation_dialog_init,
      NULL
    };

    quarry_save_confirmation_dialog_type
      = g_type_register_static (QUARRY_TYPE_MESSAGE_DIALOG,
				"QuarrySaveConfirmationDialog",
				&quarry_save_confirmation_dialog_info, 0);
  }

  return quarry_save_confirmation_dialog_type;
}


static void
quarry_save_confirmation_dialog_class_init
  (QuarrySaveConfirmationDialogClass *class)
{
  parent_class = g_type_class_peek_parent (class);

  G_OBJECT_CLASS (class)->finalize = quarry_save_confirmation_dialog_finalize;
}


static void
quarry_save_confirmation_dialog_init (QuarrySaveConfirmationDialog *dialog)
{
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			  "Close _without Saving", GTK_RESPONSE_NO,
			  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			  GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);

  quarry_message_dialog_set_icon (QUARRY_MESSAGE_DIALOG (dialog),
				  GTK_STOCK_DIALOG_WARNING);

  dialog->timeout_source_id = 0;
}


static void
quarry_save_confirmation_dialog_finalize (GObject *object)
{
  QuarrySaveConfirmationDialog *dialog
    = QUARRY_SAVE_CONFIRMATION_DIALOG (object);

  if (dialog->timeout_source_id)
    g_source_remove (dialog->timeout_source_id);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


GtkWidget *
quarry_save_confirmation_dialog_new (GtkWindow *parent,
				     glong time_of_first_modification,
				     const gchar *primary_text_format_string,
				     ...)
{
  GtkWidget *dialog;
  va_list arguments;

  va_start (arguments, primary_text_format_string);
  dialog
    = quarry_save_confirmation_dialog_new_valist (parent,
						  time_of_first_modification,
						  primary_text_format_string,
						  arguments);
  va_end (arguments);

  return dialog;
}


GtkWidget *
quarry_save_confirmation_dialog_new_valist
  (GtkWindow *parent, glong time_of_first_modification,
   const gchar *primary_text_format_string, va_list arguments)
{
  QuarrySaveConfirmationDialog *dialog
    = g_object_new (QUARRY_TYPE_SAVE_CONFIRMATION_DIALOG, NULL);

  if (parent) {
    gtk_window_present (parent);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
  }

  quarry_message_dialog_format_primary_text_valist
    (QUARRY_MESSAGE_DIALOG (dialog),
     primary_text_format_string, arguments);

  dialog->time_of_first_modification = time_of_first_modification;
  update_secondary_text (dialog);

  return GTK_WIDGET (dialog);
}


static gboolean
update_secondary_text (QuarrySaveConfirmationDialog *dialog)
{
  GTimeVal current_time;
  glong num_seconds_elapsed;
  gchar *secondary_text;

  g_get_current_time (&current_time);
  num_seconds_elapsed = (current_time.tv_sec
			 - dialog->time_of_first_modification);

  /* The ranges are taken from gedit code. */
  if (num_seconds_elapsed < 55) {
    secondary_text
      = g_strdup_printf (ngettext (("If you don't save, changes from the last "
				    "second will be lost permanently."),
				   ("If you don't save, changes from the last "
				    "%d seconds will be lost permanently."),
				   num_seconds_elapsed),
			 (gint) num_seconds_elapsed);
  }
  else if (num_seconds_elapsed < 60 + 15) {
    secondary_text = g_strdup (_("If you don't save, changes from the last "
				 "minute will be discarded."));
  }
  else if (num_seconds_elapsed < 60 + 50) {
    /* TRANSLATORS: Cannot be one minute one second, actually. */
    secondary_text
      = g_strdup_printf (ngettext (("If you don't save, changes from the last "
				    "minute and one second will be lost "
				    "permanently."),
				   ("If you don't save, changes from the last "
				    "minute and %d seconds will be lost "
				    "permanently."),
				   num_seconds_elapsed - 60),
			 (gint) (num_seconds_elapsed - 60));
  }
  else if (num_seconds_elapsed < 60 * 60) {
    secondary_text
      = g_strdup_printf (ngettext (("If you don't save, changes from the last "
				    "minute will be lost permanently."),
				   ("If you don't save, changes from the last "
				    "%d minutes will be lost permanently."),
				   num_seconds_elapsed / 60),
			 (gint) (num_seconds_elapsed / 60));
  }
  else if (60 * 60 + 5 * 60 <= num_seconds_elapsed
	   && num_seconds_elapsed < 2 * 60 * 60) {
    /* Between 1:05:00 and 2:00:00. */
    gint minutes = (num_seconds_elapsed - 60 * 60) / 60;

    /* TRANSLATORS: Cannot be one hour one minute, actually. */
    secondary_text
      = g_strdup_printf (ngettext (("If you don't save, changes from the last "
				    "hour and one minute will be lost "
				    "permanently."),
				   ("If you don't save, changes from the last "
				    "hour and %d minutes will be lost "
				    "permanently."),
				   minutes),
			 minutes);
    
  }
  else {
    secondary_text
      = g_strdup_printf (ngettext (("If you don't save, changes from the last "
				    "hour will be lost permanently."),
				   ("If you don't save, changes from the last "
				    "%d hours will be lost permanently."),
				   num_seconds_elapsed / (60 * 60)),
			 (gint) (num_seconds_elapsed / (60 * 60)));
  }

  quarry_message_dialog_set_secondary_text (QUARRY_MESSAGE_DIALOG (dialog),
					    secondary_text);

  if (dialog->timeout_source_id)
    g_source_remove (dialog->timeout_source_id);

  dialog->timeout_source_id
    = g_timeout_add (1 * 1000, (GSourceFunc) update_secondary_text, dialog);

  return FALSE;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
