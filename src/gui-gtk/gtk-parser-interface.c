/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004 Paul Pogonyshev.                       *
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
 * Software Foundation, Inc., 59 Temple Place - Suite 330,         *
 * Boston, MA 02111-1307, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* GTK+ wrapper module around SGF parser.  Contains code for file
 * selection, creating GtkGobanWindow's on successful parsing and
 * popping message dialogs otherwise.  File selection is optional,
 * i.e. this module can be entered to parse immediatly.
 *
 * When threads are supported by GLib, this module shows progress
 * dialog during parsing of huge files, thus providing feedback and a
 * way to cancel parsing.  And you can actually do anything with
 * Quarry while it parses a file in another thread.
 */


#include "gtk-parser-interface.h"
#include "gtk-thread-interface.h"
#include "gtk-goban-window.h"
#include "gtk-control-center.h"
#include "gtk-utils.h"
#include "sgf.h"
#include "utils.h"

#include <gtk/gtk.h>
#include <assert.h>

/* For chdir() function. */
#if HAVE_UNISTD_H
#include <unistd.h>
#elif defined (G_OS_WIN32)

/* FIXME: verify that it works. */
#include <windows.h>

#endif


static const gchar *reading_error = N_("Error reading file `%s'.");
static const gchar *not_sgf_file_error =
  N_("File `%s' doesn't appear to be a valid SGF file.");

static const gchar *reading_error_hint =
  N_("Please check that the file exists "
     "and its permission allow you to read it.");

static const gchar *not_sgf_file_error_hint =
  N_("Quarry uses SGF file format for storing game records. SGF files would "
     "normally have `sgf' extension, but that's not necessarily true. Please "
     "make sure you select a proper SGF file.");


static void	 open_file_response (GtkFileSelection *dialog,
				     gint response_id);


void
gtk_parser_interface_present (void)
{
  GtkWidget *file_selection = gtk_file_selection_new (_("Open SGF File..."));

  gtk_control_center_window_created (GTK_WINDOW (file_selection));

  gtk_utils_add_file_selection_response_handlers (file_selection, FALSE,
						  (G_CALLBACK
						   (open_file_response)),
						  NULL);
  g_signal_connect (file_selection, "destroy",
		    G_CALLBACK (gtk_control_center_window_destroyed), NULL);

  gtk_widget_show (file_selection);
}


static void
open_file_response (GtkFileSelection *dialog, gint response_id)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *filename = gtk_file_selection_get_filename (dialog);

    gtk_parse_sgf_file (filename, GTK_WINDOW (dialog));
  }
  else if (response_id == GTK_RESPONSE_CANCEL)
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


#if THREADS_SUPPORTED


#include "gtk-progress-dialog.h"


static gpointer	 thread_wrapped_sgf_parse_file (ParsingThreadData *data);

static gboolean	 update_progress_bar (GtkProgressDialog *progress_dialog,
				      ParsingThreadData *data);
static gboolean	 cancel_parsing (GtkProgressDialog *progress_dialog,
				 ParsingThreadData *data);

static void	 analyze_parsed_data (void *result);


void
gtk_parse_sgf_file (const char *filename, GtkWindow *parent)
{
  ParsingThreadData *data;
  gchar *label_text;

  data = g_malloc (sizeof (ParsingThreadData));
  data->filename	  = g_strdup (filename);
  data->file_size	  = 0;
  data->bytes_parsed	  = 0;
  data->cancellation_flag = 0;
  data->parent		  = (parent ? GTK_WIDGET (parent) : NULL);

  label_text = g_strdup_printf (_("Parsing file `%s'..."), filename);
  data->progress_dialog
    = gtk_progress_dialog_new (parent, "Quarry", label_text,
			       (GtkProgressDialogCallback) update_progress_bar,
			       (GtkProgressDialogCallback) cancel_parsing,
			       data);
  g_object_ref (data->progress_dialog);
  g_free (label_text);

  gtk_control_center_new_reason_to_live ();

  g_thread_create ((GThreadFunc) thread_wrapped_sgf_parse_file,
		   data, FALSE, NULL);
}


static gpointer
thread_wrapped_sgf_parse_file (ParsingThreadData *data)
{
  ThreadEventData *event_data;

  data->result = sgf_parse_file (data->filename,
				 &data->sgf_collection, &data->error_list,
				 &sgf_parser_defaults,
				 &data->file_size, &data->bytes_parsed,
				 &data->cancellation_flag);

  event_data = g_malloc (sizeof (ThreadEventData));
  event_data->callback = analyze_parsed_data;
  event_data->result   = data;

  g_async_queue_push (thread_events_queue, event_data);
  g_main_context_wakeup (NULL);

  return NULL;
}


static gboolean
update_progress_bar (GtkProgressDialog *progress_dialog,
		     ParsingThreadData *data)
{
  if (!GTK_WIDGET_VISIBLE (progress_dialog)) {
    gtk_widget_show (GTK_WIDGET (progress_dialog));
    if (data->parent)
      gtk_widget_hide (data->parent);
  }

  if (data->file_size > 0) {
    gtk_progress_dialog_set_fraction (progress_dialog,
				      (((gdouble) data->bytes_parsed)
				       / data->file_size),
				      "Quarry");
  }

  return TRUE;
}


static gboolean
cancel_parsing (GtkProgressDialog *progress_dialog, ParsingThreadData *data)
{
  UNUSED (progress_dialog);

  data->cancellation_flag = 1;
  return TRUE;
}


static void
analyze_parsed_data (void *result)
{
  ParsingThreadData *data = (ParsingThreadData *) result;

  gtk_widget_destroy (data->progress_dialog);

  if (data->result == SGF_PARSED) {
    GtkWidget *goban_window = gtk_goban_window_new (data->sgf_collection,
						    data->filename);

    gtk_widget_show (goban_window);

    if (data->error_list)
      string_list_delete (data->error_list);

    if (data->parent) {
      gchar *directory = g_path_get_dirname (data->filename);

      chdir (directory);
      g_free (directory);

      gtk_widget_destroy (data->parent);
    }
  }
  else if (data->result == SGF_PARSING_CANCELLED) {
    if (data->parent)
      gtk_widget_destroy (data->parent);
  }
  else {
    GtkProgressDialog *progress_dialog
      = GTK_PROGRESS_DIALOG (data->progress_dialog);
    GtkWidget *message_dialog;

    gtk_progress_dialog_recover_parent (progress_dialog);

    message_dialog
      = (gtk_utils_create_message_dialog
	 (data->parent ? GTK_WINDOW (data->parent) : NULL,
	  GTK_STOCK_DIALOG_ERROR,
	  (GTK_UTILS_BUTTONS_OK | GTK_UTILS_DESTROY_ON_RESPONSE
	   | (data->parent ? 0 : GTK_UTILS_NON_MODAL_WINDOW)),
	  (data->result == SGF_ERROR_READING_FILE
	   ? _(reading_error_hint) : _(not_sgf_file_error_hint)),
	  (data->result == SGF_ERROR_READING_FILE
	   ? _(reading_error) : _(not_sgf_file_error)),
	  data->filename));

    if (!data->parent) {
      gtk_control_center_window_created (GTK_WINDOW (message_dialog));
      g_signal_connect (message_dialog, "destroy",
			G_CALLBACK (gtk_control_center_window_destroyed),
			NULL);
    }
  }

  g_object_unref (data->progress_dialog);

  g_free (data->filename);
  g_free (data);

  gtk_control_center_lost_reason_to_live ();
}


#else /* not THREADS_SUPPORTED */


void
gtk_parse_sgf_file (const char *filename, GtkWindow *parent)
{
  SgfCollection *sgf_collection;
  SgfErrorList *error_list;
  int result;

  result = sgf_parse_file (filename, &sgf_collection, &error_list,
			   &sgf_parser_defaults, NULL, NULL, NULL);

  if (result == SGF_PARSED) {
    GtkWidget *goban_window = gtk_goban_window_new (sgf_collection, filename);

    gtk_widget_show (goban_window);

    if (error_list)
      string_list_delete (error_list);

    if (parent) {
      gchar *directory = g_path_get_dirname (filename);

      chdir (directory);
      g_free (directory);

      gtk_widget_destroy (GTK_WIDGET (parent));
    }
  }
  else {
    GtkWidget *message_dialog
      = (gtk_utils_create_message_dialog
	 (data->parent ? GTK_WINDOW (data->parent) : NULL,
	  GTK_STOCK_DIALOG_ERROR,
	  (GTK_UTILS_BUTTONS_OK | GTK_UTILS_DESTROY_ON_RESPONSE
	   | (data->parent ? 0 : GTK_UTILS_NON_MODAL_WINDOW)),
	  (data->result == SGF_ERROR_READING_FILE
	   ? _(reading_error_hint) : _(not_sgf_file_error_hint)),
	  (data->result == SGF_ERROR_READING_FILE
	   ? _(reading_error) : _(not_sgf_file_error)),
	  data->filename));

    if (!data->parent) {
      gtk_control_center_window_created (GTK_WINDOW (message_dialog));
      g_signal_connect (message_dialog, "destroy",
			G_CALLBACK (gtk_control_center_window_destroyed),
			NULL);
    }
  }
}


#endif


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
