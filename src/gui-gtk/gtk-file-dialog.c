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

/* Interface for hiding differences between GtkFileChooser and
 * GtkFileSelection.  Since I personally don't like the former (not
 * very convenient for keyboard users), there is a preferences options
 * to use GtkFileSelection even on newer GTK+ versions.
 */


#include "gtk-file-dialog.h"

#include "gtk-configuration.h"
#include "gtk-control-center.h"
#include "gtk-utils.h"
#include "quarry-message-dialog.h"
#include "quarry-stock.h"

#include <gtk/gtk.h>
#include <string.h>


#if GTK_2_4_OR_LATER
static void	 file_chooser_save_response (GtkWidget *dialog,
					     gint response_id);
#endif

static void	 file_selection_open_response (GtkWidget *dialog,
					       gint response_id);
static void	 file_selection_save_response (GtkWidget *dialog,
					       gint response_id);

static gboolean	 check_if_directory_selected (GtkWidget *dialog,
					      const gchar *filename);
static gboolean	 check_if_overwriting_file (GtkWidget *dialog,
					    const gchar *filename);

static void	 overwrite_confirmation (GtkWidget *confirmation_dialog,
					 gint response_id);


GtkWidget *
gtk_file_dialog_new (const gchar *title, GtkWindow *parent,
		     gboolean for_opening,
		     const gchar *affirmative_button_text)
{
  GtkWidget *dialog;

  g_return_val_if_fail (title, NULL);
  g_return_val_if_fail (!parent || GTK_IS_WINDOW (parent), NULL);
  g_return_val_if_fail (affirmative_button_text, NULL);

#if GTK_2_4_OR_LATER

  if (gtk_ui_configuration.use_gtk_file_chooser) {
    dialog
      = gtk_file_chooser_dialog_new (title, parent,
				     (for_opening
				      ? GTK_FILE_CHOOSER_ACTION_OPEN
				      : GTK_FILE_CHOOSER_ACTION_SAVE),
				     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     affirmative_button_text, GTK_RESPONSE_OK,
				     NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    if (!for_opening) {
      g_signal_connect (dialog, "response",
			G_CALLBACK (file_chooser_save_response), NULL);
    }
  }
  else {

#endif /* GTK_2_4_OR_LATER */

    dialog = gtk_file_selection_new (title);

    if (parent) {
      gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
    }

    g_signal_connect (dialog, "response",
		      G_CALLBACK (for_opening
				  ? file_selection_open_response
				  : file_selection_save_response),
		      NULL);

#if GTK_2_4_OR_LATER
  }
#endif

  return dialog;
}


gchar *
gtk_file_dialog_get_filename (GtkWidget *dialog)
{
#if GTK_2_4_OR_LATER

  if (GTK_IS_FILE_CHOOSER (dialog))
    return gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

#endif

  if (GTK_IS_FILE_SELECTION (dialog)) {
    return g_strdup (gtk_file_selection_get_filename
		     (GTK_FILE_SELECTION (dialog)));
  }

  g_assert_not_reached ();
}


/* Set current filename in the file selection dialog.  `filename'
 * should be in UTF-8 rather than in file system encoding.
 */
void
gtk_file_dialog_set_filename (GtkWidget *dialog, const gchar *filename)
{
#if GTK_2_4_OR_LATER

  if (GTK_IS_FILE_CHOOSER (dialog)) {
    if (g_path_is_absolute (filename)) {
      if (g_file_test (filename, G_FILE_TEST_EXISTS))
	gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), filename);
      else {
	gchar *base_name = g_path_get_basename (filename);

	if (strcmp (base_name, G_DIR_SEPARATOR_S) != 0
	    && strcmp (base_name, ".") != 0) {
	  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog),
					     base_name);
	}

	g_free (base_name);
      }
    }
    else
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);

    return;
  }

#endif

  if (GTK_IS_FILE_SELECTION (dialog)) {
    gchar *file_system_filename = g_filename_from_utf8 (filename, -1,
							NULL, NULL, NULL);

    gtk_file_selection_set_filename (GTK_FILE_SELECTION (dialog),
				     file_system_filename);
    g_free (file_system_filename);

    return;
  }

  g_assert_not_reached ();
}


void
gtk_file_dialog_set_current_name (GtkWidget *dialog, const gchar *filename)
{
#if GTK_2_4_OR_LATER

  if (GTK_IS_FILE_CHOOSER (dialog)) {
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);
    return;
  }

#endif

  /* For GtkFileSelection there is no distinction. */
  gtk_file_dialog_set_filename (dialog, filename);
}


#if GTK_2_4_OR_LATER


static void
file_chooser_save_response (GtkWidget *dialog, gint response_id)
{
  if (response_id == GTK_RESPONSE_OK) {
    gchar *filename
      = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    gboolean confirmation_asked = check_if_overwriting_file (dialog, filename);

    g_free (filename);

    if (confirmation_asked)
      g_signal_stop_emission_by_name (dialog, "response");
  }
}


#endif /* GTK_2_4_OR_LATER */


static void
file_selection_open_response (GtkWidget *dialog, gint response_id)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *filename
      = gtk_file_selection_get_filename (GTK_FILE_SELECTION (dialog));

    if (check_if_directory_selected (dialog, filename))
      g_signal_stop_emission_by_name (dialog, "response");
  }
}


static void
file_selection_save_response (GtkWidget *dialog, gint response_id)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *filename
      = gtk_file_selection_get_filename (GTK_FILE_SELECTION (dialog));

    if (check_if_directory_selected (dialog, filename)) {
      g_signal_stop_emission_by_name (dialog, "response");
      return;
    }

    if (check_if_overwriting_file (dialog, filename))
      g_signal_stop_emission_by_name (dialog, "response");
  }
}


static gboolean
check_if_directory_selected (GtkWidget *dialog, const gchar *filename)
{
  if (*filename && g_file_test (filename, G_FILE_TEST_IS_DIR)) {
    /* Don't try to read directory, just browse into it. */
    if (filename[strlen (filename) - 1] != G_DIR_SEPARATOR) {
      gchar *directory_name = g_strconcat (filename, G_DIR_SEPARATOR_S,
					   NULL);

      gtk_file_selection_set_filename (GTK_FILE_SELECTION (dialog),
				       directory_name);
      g_free (directory_name);
    }
    else
      gtk_file_selection_set_filename (GTK_FILE_SELECTION (dialog), filename);

    return TRUE;
  }

  return FALSE;
}


static gboolean
check_if_overwriting_file (GtkWidget *dialog, const gchar *filename)
{
  if (*filename && g_file_test (filename, G_FILE_TEST_EXISTS)) {
    static const gchar *hint
      = N_("Note that all information in the existing file will be lost "
	   "permanently if you choose to overwrite it.");
    static const gchar *message_format_string
      = N_("File named \342\200\230%s\342\200\231 already exists. "
	   "Do you want to overwrite it with the one you are saving?");

    gchar *filename_in_utf8 = g_filename_to_utf8 (filename, -1,
						  NULL, NULL, NULL);
    GtkWidget *confirmation_dialog
      = quarry_message_dialog_new (GTK_WINDOW (dialog),
				   GTK_BUTTONS_NONE,
				   GTK_STOCK_DIALOG_WARNING,
				   _(hint),
				   _(message_format_string),
				   filename_in_utf8);

    g_free (filename_in_utf8);

    gtk_dialog_add_buttons (GTK_DIALOG (confirmation_dialog),
			    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			    QUARRY_STOCK_OVERWRITE, GTK_RESPONSE_YES, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (confirmation_dialog),
				     GTK_RESPONSE_CANCEL);
    gtk_window_set_modal (GTK_WINDOW (confirmation_dialog), TRUE);

    g_signal_connect (confirmation_dialog, "response",
		      G_CALLBACK (overwrite_confirmation), NULL);

    gtk_window_present (GTK_WINDOW (confirmation_dialog));

    return TRUE;
  }

  return FALSE;
}


static void
overwrite_confirmation (GtkWidget *confirmation_dialog, gint response_id)
{
  GtkDialog *file_dialog = GTK_DIALOG (gtk_window_get_transient_for
				       (GTK_WINDOW (confirmation_dialog)));

  gtk_widget_destroy (confirmation_dialog);

  if (response_id == GTK_RESPONSE_YES) {
    /* Disconnect our `response' handlers so that they don't pop
     * confirmation dialog up again.
     */

#if GTK_2_4_OR_LATER

    if (GTK_IS_FILE_CHOOSER (file_dialog)) {
      g_signal_handlers_disconnect_by_func (file_dialog,
					    file_chooser_save_response, NULL);
    }
    else {
#endif

      g_signal_handlers_disconnect_by_func (file_dialog,
					    file_selection_save_response, NULL);

#if GTK_2_4_OR_LATER
    }
#endif

    gtk_dialog_response (file_dialog, GTK_RESPONSE_OK);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
