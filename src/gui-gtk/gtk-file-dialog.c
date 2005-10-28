/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005 Paul Pogonyshev                              *
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
#include "quarry-message-dialog.h"
#include "quarry-stock.h"

#include <assert.h>
#include <gtk/gtk.h>
#include <string.h>

typedef void (* GtkFileDialogResponseCallback)
  (GtkWidget *file_dialog, gint response_id, gpointer user_data);

typedef struct _GtkFileDialogData	GtkFileDialogData;

struct _GtkFileDialogData {
  GtkFileDialogResponseCallback	  response_callback;
  gpointer			  user_data;
};


#if GTK_2_4_OR_LATER
static void	 file_chooser_save_response (GtkWidget *dialog,
					     gint response_id,
					     GtkFileDialogData *data);
#endif

static void	 file_selection_open_response (GtkWidget *dialog,
					       gint response_id,
					       GtkFileDialogData *data);
static void	 file_selection_save_response (GtkWidget *dialog,
					       gint response_id,
					       GtkFileDialogData *data);

static gboolean	 check_if_directory_selected (GtkWidget *dialog,
					      const gchar *filename);
static gboolean	 check_if_overwriting_file (GtkWidget *dialog,
					    const gchar *filename,
					    GtkFileDialogData *data);

static void	 overwrite_confirmation (GtkWidget *confirmation_dialog,
					 gint response_id,
					 GtkFileDialogData *data);


GtkWidget *
gtk_file_dialog_new (const gchar *title, GtkWindow *parent,
		     gboolean for_opening,
		     const gchar *affirmative_button_text,
		     GCallback response_callback, gpointer user_data)
{
  GtkWidget *dialog;
  GtkFileDialogData *data;

  assert (title);
  assert (!parent || GTK_IS_WINDOW (parent));
  assert (affirmative_button_text);
  assert (response_callback);

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

    if (for_opening)
      g_signal_connect (dialog, "response", response_callback, user_data);
    else {
      data = g_malloc (sizeof (GtkFileDialogData));

      data->response_callback = ((GtkFileDialogResponseCallback)
				 response_callback);
      data->user_data	      = user_data;

      g_signal_connect (dialog, "response",
			G_CALLBACK (file_chooser_save_response), data);
      g_signal_connect_swapped (dialog, "destroy", G_CALLBACK (g_free), data);
    }
  }
  else {

#endif /* GTK_2_4_OR_LATER */

    dialog = gtk_file_selection_new (title);

    if (parent) {
      gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
    }

    data = g_malloc (sizeof (GtkFileDialogData));

    data->response_callback = ((GtkFileDialogResponseCallback)
			       response_callback);
    data->user_data	    = user_data;

    g_signal_connect (dialog, "response",
		      G_CALLBACK (for_opening
				  ? file_selection_open_response
				  : file_selection_save_response),
		      data);
    g_signal_connect_swapped (dialog, "destroy", G_CALLBACK (g_free), data);

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

  assert (0);
}


/* Set current filename in the file selection dialog.  `filename'
 * should be in UTF-8 rather than in file system encoding.
 */
void
gtk_file_dialog_set_filename (GtkWidget *dialog, const gchar *filename)
{
#if GTK_2_4_OR_LATER

  if (GTK_IS_FILE_CHOOSER (dialog)) {
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), filename);
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

  assert (0);
}


#if GTK_2_4_OR_LATER


static void
file_chooser_save_response (GtkWidget *dialog, gint response_id,
			    GtkFileDialogData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    gchar *filename
      = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    gboolean confirmation_asked = check_if_overwriting_file (dialog, filename,
							     data);

    g_free (filename);

    if (confirmation_asked)
      return;
  }

  data->response_callback (dialog, response_id, data->user_data);
}


#endif /* GTK_2_4_OR_LATER */


static void
file_selection_open_response (GtkWidget *dialog, gint response_id,
			      GtkFileDialogData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *filename
      = gtk_file_selection_get_filename (GTK_FILE_SELECTION (dialog));

    if (check_if_directory_selected (dialog, filename))
      return;
  }

  data->response_callback (dialog, response_id, data->user_data);
}


static void
file_selection_save_response (GtkWidget *dialog, gint response_id,
			      GtkFileDialogData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *filename
      = gtk_file_selection_get_filename (GTK_FILE_SELECTION (dialog));

    if (check_if_directory_selected (dialog, filename))
      return;

    if (check_if_overwriting_file (dialog, filename, data))
      return;
  }

  data->response_callback (dialog, response_id, data->user_data);
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
check_if_overwriting_file (GtkWidget *dialog, const gchar *filename,
			   GtkFileDialogData *data)
{
  if (*filename && g_file_test (filename, G_FILE_TEST_EXISTS)) {
    static const gchar *hint
      = N_("Note that all information in the existing file will be lost "
	   "permanently if you choose to overwrite it.");
    static const gchar *message_format_string
      = N_("File named `%s' already exists. "
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
		      G_CALLBACK (overwrite_confirmation), data);

    gtk_window_present (GTK_WINDOW (confirmation_dialog));

    return TRUE;
  }

  return FALSE;
}


static void
overwrite_confirmation (GtkWidget *confirmation_dialog, gint response_id,
			GtkFileDialogData *data)
{
  if (response_id == GTK_RESPONSE_YES) {
    data->response_callback (GTK_WIDGET (gtk_window_get_transient_for
					 (GTK_WINDOW (confirmation_dialog))),
			     GTK_RESPONSE_OK, data->user_data);
  }

  gtk_widget_destroy (confirmation_dialog);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
