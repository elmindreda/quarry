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

#include "gtk-control-center.h"
#include "gtk-file-dialog.h"
#include "gtk-goban-window.h"
#include "gtk-thread-interface.h"
#include "gtk-utils.h"
#include "sgf.h"
#include "utils.h"

#include <assert.h>
#include <gtk/gtk.h>

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


static void	 open_file_response (GtkWidget *file_dialog,
				     gint response_id,
				     GtkHandleParsedData callback);

static void	 open_game_record (SgfCollection *sgf_collection,
				   SgfErrorList *sgf_error_list,
				   const gchar *filename);


/* For hooking up as a callback. */
void
gtk_parser_interface_present_default (void)
{
  gtk_parser_interface_present (NULL, NULL, NULL);
}


void
gtk_parser_interface_present (GtkWindow **dialog_window, const gchar *title,
			      GtkHandleParsedData callback)
{
  static GtkWindow *open_sgf_file_dialog = NULL;

  if (dialog_window == NULL)
    dialog_window = &open_sgf_file_dialog;

  if (*dialog_window == NULL) {
    GtkWidget *file_dialog
      = gtk_file_dialog_new (title ? title : _("Open SGF File..."),
			     NULL, TRUE, GTK_STOCK_OPEN,
			     G_CALLBACK (open_file_response), callback);

    *dialog_window = GTK_WINDOW (file_dialog);
    gtk_control_center_window_created (*dialog_window);
    gtk_utils_null_pointer_on_destroy (dialog_window, TRUE);
  }

  gtk_window_present (*dialog_window);
}


static void
open_file_response (GtkWidget *file_dialog, gint response_id,
		    GtkHandleParsedData callback)
{
  if (response_id == GTK_RESPONSE_OK) {
    gchar *filename = gtk_file_dialog_get_filename (file_dialog);

    gtk_parse_sgf_file (filename, GTK_WINDOW (file_dialog), callback);
    g_free (filename);
  }
  else if (response_id == GTK_RESPONSE_CANCEL)
    gtk_widget_destroy (file_dialog);
}


static void
open_game_record (SgfCollection *sgf_collection, SgfErrorList *sgf_error_list,
		  const gchar *filename)
{
  GtkWidget *goban_window = gtk_goban_window_new (sgf_collection, filename);

  gtk_goban_window_enter_game_record_mode (GTK_GOBAN_WINDOW (goban_window));
  gtk_window_present (GTK_WINDOW (goban_window));

  if (sgf_error_list)
    string_list_delete (sgf_error_list);
}


#if THREADS_SUPPORTED


#include "gtk-progress-dialog.h"


static gpointer	 thread_wrapped_sgf_parse_file (ParsingThreadData *data);

static gboolean	 update_progress_bar (GtkProgressDialog *progress_dialog,
				      ParsingThreadData *data);
static gboolean	 cancel_parsing (GtkProgressDialog *progress_dialog,
				 ParsingThreadData *data);

static void	 analyze_parsed_data (void *result);


/* NOTE: `filename' must be in file system encoding! */
void
gtk_parse_sgf_file (const char *filename, GtkWindow *parent,
		    GtkHandleParsedData callback)
{
  ParsingThreadData *data;
  gchar *label_text;
  gchar *absolute_filename;
  gchar *filename_in_utf8;

  assert (filename);

  if (g_path_is_absolute (filename))
    absolute_filename = g_strdup (filename);
  else {
    gchar *current_directory = g_get_current_dir ();

    absolute_filename = g_build_filename (current_directory, filename, NULL);
    g_free (current_directory);
  }

  data = g_malloc (sizeof (ParsingThreadData));
  data->filename	  = absolute_filename;
  data->file_size	  = 0;
  data->bytes_parsed	  = 0;
  data->cancellation_flag = 0;
  data->parent		  = (parent ? GTK_WIDGET (parent) : NULL);
  data->callback	  = callback ? callback : open_game_record;

  filename_in_utf8 = g_filename_to_utf8 (absolute_filename, -1,
					 NULL, NULL, NULL);
  label_text = g_strdup_printf (_("Parsing file `%s'..."), filename_in_utf8);
  g_free (filename_in_utf8);

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
    if (data->parent) {
      gchar *directory = g_path_get_dirname (data->filename);

      chdir (directory);
      g_free (directory);

      gtk_widget_destroy (data->parent);
    }

    data->callback (data->sgf_collection, data->error_list, data->filename);
  }
  else if (data->result == SGF_PARSING_CANCELLED) {
    if (data->parent)
      gtk_widget_destroy (data->parent);
  }
  else {
    GtkProgressDialog *progress_dialog
      = GTK_PROGRESS_DIALOG (data->progress_dialog);
    GtkWidget *message_dialog;
    gchar *filename_in_utf8 = g_filename_to_utf8 (data->filename, -1,
						  NULL, NULL, NULL);

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
	  filename_in_utf8));

    g_free (filename_in_utf8);

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
gtk_parse_sgf_file (const char *filename, GtkWindow *parent,
		    GtkHandleParsedData callback)
{
  gchar *absolute_filename;
  SgfCollection *sgf_collection;
  SgfErrorList *error_list;
  int result;

  if (g_path_is_absolute (filename))
    absolute_filename = g_strdup (filename);
  else {
    gchar *current_directory = g_get_current_dir ();

    absolute_filename = g_build_filename (current_directory, filename, NULL);
    g_free (current_directory);
  }

  result = sgf_parse_file (absolute_filename, &sgf_collection, &error_list,
			   &sgf_parser_defaults, NULL, NULL, NULL);

  if (result == SGF_PARSED) {
    if (parent) {
      gchar *directory = g_path_get_dirname (absolute_filename);

      chdir (directory);
      g_free (directory);

      gtk_widget_destroy (GTK_WIDGET (parent));
    }

    if (!callback)
      callback = open_game_record;

    callback (sgf_collection, error_list, absolute_filename);
  }
  else {
    gchar *filename_in_utf8 = g_filename_to_utf8 (absolute_filename, -1,
						  NULL, NULL, NULL);
    GtkWidget *message_dialog
      = (gtk_utils_create_message_dialog
	 (parent ? GTK_WINDOW (parent) : NULL,
	  GTK_STOCK_DIALOG_ERROR,
	  (GTK_UTILS_BUTTONS_OK | GTK_UTILS_DESTROY_ON_RESPONSE
	   | (parent ? 0 : GTK_UTILS_NON_MODAL_WINDOW)),
	  (result == SGF_ERROR_READING_FILE
	   ? _(reading_error_hint) : _(not_sgf_file_error_hint)),
	  (result == SGF_ERROR_READING_FILE
	   ? _(reading_error) : _(not_sgf_file_error)),
	  filename_in_utf8));

    g_free (filename_in_utf8);

    if (!parent) {
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
