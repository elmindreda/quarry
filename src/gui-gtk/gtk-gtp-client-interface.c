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


#include "gtk-gtp-client-interface.h"
#include "gtp-client.h"
#include "quarry-main.h"

#include <assert.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>


typedef struct _GtkGtpClientData	GtkGtpClientData;

struct _GtkGtpClientData {
  GtpClient			*client;

  GIOChannel			*engine_input;
  GIOChannel			*engine_output;
  GIOChannel			*engine_stderr;
  guint				 input_event_source_id;
  guint				 stderr_input_event_source_id;

  FILE				*log_file;

  GtpClientInitializedCallback	 initialized_callback;
  GtkGtpClientDeletedCallback	 deleted_callback;
  void				*user_data;

  GError			*shutdown_reason;
  gboolean			 deletion_scheduled;
};


static gboolean	 do_delete_client (gpointer client);

static gboolean	 handle_engine_output (GIOChannel *read_output_from,
				       GIOCondition condition,
				       GtkGtpClientData *data);
static gboolean	 handle_engine_stderr (GIOChannel *read_output_from,
				       GIOCondition condition,
				       GtkGtpClientData *data);
static void	 send_to_engine (const char *command, GtkGtpClientData *data);

static void	 client_initialized (GtpClient *client,
				     GtkGtpClientData *data);

static void	 log_gtp_stream (const char *line, int is_command,
				 int internal_index, GtkGtpClientData *data);
static void	 log_gtp_stream_error (GtpError error, int command_id,
				       GtkGtpClientData *data);

static void	 client_deleted (GtpClient *client, GtkGtpClientData *data);


static GSList  *clients;


GtpClient *
gtk_create_gtp_client (const gchar *command_line,
		       GtpClientInitializedCallback initialized_callback,
		       GtkGtpClientDeletedCallback deleted_callback,
		       void *user_data,
		       GError **error)
{
  GtpClient *client = NULL;
  gchar **argv;

  assert (command_line);
  assert (error == NULL || *error == NULL);

  if (g_shell_parse_argv (command_line, NULL, &argv, error)) {
    gint child_pid;
    gint standard_input;
    gint standard_output;
    gint standard_error;

    if (g_spawn_async_with_pipes
	(NULL, argv, NULL,
	 (G_SPAWN_SEARCH_PATH
	  | (option_gtp_show_stderr ? 0 : G_SPAWN_STDERR_TO_DEV_NULL)),
	 NULL, NULL, &child_pid,
	 &standard_input, &standard_output,
	 (option_gtp_show_stderr && option_gtp_log_base_name
	  ? &standard_error : NULL),
	 error)) {
      GtkGtpClientData *data = g_malloc (sizeof (GtkGtpClientData));

      client = gtp_client_new ((GtpClientSendToEngine) send_to_engine,
			       (GtpClientLineCallback) log_gtp_stream,
			       (GtpClientErrorCallback) log_gtp_stream_error,
			       (initialized_callback
				? ((GtpClientInitializedCallback)
				   client_initialized)
				: NULL),
			       (GtpClientDeletedCallback) client_deleted,
			       data);
      data->client = client;

      data->engine_input  = g_io_channel_unix_new (standard_input);

      g_io_channel_set_encoding (data->engine_input, NULL, NULL);

      /* Since commands always consist of one string only, no
       * buffering is ever needed (and this saves from flushing the
       * channel constantly).
       */
      g_io_channel_set_buffered (data->engine_input, FALSE);

      data->engine_output = g_io_channel_unix_new (standard_output);
      g_io_channel_set_encoding (data->engine_output, NULL, NULL);
      g_io_channel_set_flags (data->engine_output, G_IO_FLAG_NONBLOCK, NULL);

      data->input_event_source_id
	= g_io_add_watch (data->engine_output, G_IO_IN | G_IO_HUP,
			  (GIOFunc) handle_engine_output, data);

      if (option_gtp_show_stderr && option_gtp_log_base_name) {
	data->engine_stderr = g_io_channel_unix_new (standard_error);
	g_io_channel_set_encoding (data->engine_stderr, NULL, NULL);
	g_io_channel_set_flags (data->engine_stderr, G_IO_FLAG_NONBLOCK, NULL);

	data->stderr_input_event_source_id
	  = g_io_add_watch (data->engine_stderr, G_IO_IN | G_IO_HUP,
			    (GIOFunc) handle_engine_stderr, data);
      }
      else
	data->engine_stderr = NULL;

      if (option_gtp_log_base_name) {
	if (strcmp (option_gtp_log_base_name, "-") != 0) {
	  gchar *log_file_name
	    = g_strdup_printf ("%s.%d", option_gtp_log_base_name, child_pid);

	  data->log_file = fopen (log_file_name, "wb");
	  if (!data->log_file) {
	    g_print (("Warning: unable to open file `%s' "
		      "for logging GTP stream.\n"
		      "Warning: logging to stdout instead.\n"),
		     log_file_name);

	    data->log_file = stdout;
	  }

	  g_free (log_file_name);
	}
	else
	  data->log_file = stdout;

	gtp_client_echo_on (client);
      }
      else
	data->log_file = NULL;

      data->initialized_callback = initialized_callback;
      data->deleted_callback	 = deleted_callback;
      data->user_data		 = user_data;

      data->shutdown_reason = NULL;
      data->deletion_scheduled = FALSE;

      clients = g_slist_prepend (clients, data);
    }

    g_strfreev (argv);
  }

  return client;
}


/* Schedule deletion of a GTP client.  The client must have been
 * created by a call to gtk_create_gtp_client() and all such clients
 * must be deleted with this function.  This allows not to track if
 * the client is still alive because it's guaranteed it will be
 * deleted in a separate GTK+/GLib event loop iteration.
 */
gboolean
gtk_schedule_gtp_client_deletion (const GtpClient *client)
{
  GSList *item = clients;

  while (1) {
    GtkGtpClientData *data;

    assert (item);

    data = (GtkGtpClientData *) (item->data);
    if (data->client == client) {
      if (!data->deletion_scheduled) {
	data->deletion_scheduled = TRUE;
	g_idle_add_full (G_PRIORITY_HIGH, do_delete_client, data->client,
			 NULL);

	return TRUE;
      }

      return FALSE;
    }

    item = item->next;
  }
}


/* Delete the client and the idle event source (which is not needed
 * anymore).
 */
static gboolean
do_delete_client (gpointer client)
{
  gtp_client_delete ((GtpClient *) client);

  return FALSE;
}


static gboolean
handle_engine_output (GIOChannel *read_output_from, GIOCondition condition,
		      GtkGtpClientData *data)
{
  if (!data->deletion_scheduled) {
    gchar buffer[0x1000];
    gint bytes_read;

    /* Check that the channel is readable.  This loop throws
     * responses at the client and it can decide to delete itself
     * (on response to `quit').  Then the channel is closed and is
     * not readable even if there is still some bogus response.
     */
    while (g_io_channel_get_flags (read_output_from)
	   & G_IO_FLAG_IS_READABLE) {
      g_io_channel_read_chars (read_output_from,
			       buffer, sizeof buffer, &bytes_read,
			       &data->shutdown_reason);

      if (data->shutdown_reason) {
	/* Since this is happening in a separate event loop, we can
	 * just delete the client without scheduling.  We set
	 * `deletion_scheduled' flag to avoid assertion failure in the
	 * deletion callback though.
	 */
	data->deletion_scheduled = TRUE;
	gtp_client_delete (data->client);

	break;
      }

      if (bytes_read > 0)
	gtp_client_grab_response (data->client, buffer, bytes_read);

      if (bytes_read < sizeof buffer) {
	/* Certainly nothing more to read. */
	break;
      }
    }

    if (condition == G_IO_HUP && g_slist_find (clients, data)) {
      /* We read everything left in the input channel, but the client
       * still hasn't been deleted.  This means the engine hung up
       * before replying to `quit' command (if it was sent at all.)
       */
      g_set_error (&data->shutdown_reason, 0, 0, _("Engine hung up"));

      /* It is OK not to schedule deletion (see comments above.) */
      data->deletion_scheduled = TRUE;
      gtp_client_delete (data->client);
    }
  }

  return TRUE;
}


/* See handle_engine_output() for details. */
static gboolean
handle_engine_stderr (GIOChannel *read_output_from, GIOCondition condition,
		      GtkGtpClientData *data)
{
  if (!data->deletion_scheduled) {
    gchar buffer[0x1000];
    gint bytes_read;

    while (g_io_channel_get_flags (read_output_from)
	   & G_IO_FLAG_IS_READABLE) {
      GError *error = NULL;

      g_io_channel_read_chars (read_output_from,
			       buffer, sizeof buffer, &bytes_read, &error);

      if (error) {
	g_io_channel_shutdown (read_output_from, TRUE, NULL);
	g_io_channel_unref (read_output_from);

	data->engine_stderr = NULL;

	return FALSE;
      }

      if (bytes_read > 0) {
	assert (data->log_file);

	/* FIXME: Handle partial lines nicely.  Prepend some character
	 *	  (maybe '!') to indicate stderr output.
	 */
	fwrite (buffer, bytes_read, 1, data->log_file);
      }

      if (bytes_read < sizeof buffer)
	break;
    }

    if (condition == G_IO_HUP && g_slist_find (clients, data)) {
      g_io_channel_shutdown (read_output_from, TRUE, NULL);
      g_io_channel_unref (read_output_from);

      data->engine_stderr = NULL;

      return FALSE;
    }
  }

  return TRUE;
}


static void
send_to_engine (const char *command, GtkGtpClientData *data)
{
  if (!data->deletion_scheduled) {
    g_io_channel_write_chars (data->engine_input, command, -1, NULL,
			      &data->shutdown_reason);

    if (data->shutdown_reason) {
      /* `send_to_engine' callback is invoked from within GTP client
       * code, so we cannot just delete the client.  Deletion is
       * scheduled instead.
       */
      gtk_schedule_gtp_client_deletion (data->client);
    }
  }
}


static void
client_initialized (GtpClient *client, GtkGtpClientData *data)
{
  data->initialized_callback (client, data->user_data);
}


static void
log_gtp_stream (const char *line, int is_command, int internal_index,
		GtkGtpClientData *data)
{
  UNUSED (internal_index);

  assert (data->log_file);

  fprintf (data->log_file, "%c %s", (is_command ? '>' : ' '), line);
}


static void
log_gtp_stream_error (GtpError error, int command_id, GtkGtpClientData *data)
{
  assert (data->log_file);

  switch (error) {
  case GTP_ERROR_WRONG_RESPONSE_FORMAT:
    fputs (_("* ERROR: wrong response format\n"), data->log_file);
    break;

  case GTP_WARNING_UNEXPECTED_OUTPUT:
    fputs (_("* WARNING: unexpected response:\n"), data->log_file);
    break;

  case GTP_ERROR_UNRECOGNIZED_RESPONSE:
    fputs (_("* ERROR: unrecognized response line:\n"), data->log_file);
    break;

  case GTP_ERROR_MISMATCHED_ID:
    fprintf (data->log_file,
	     _("* ERROR: mismatched ID (%d expected):\n"), command_id);
    break;

  case GTP_ERROR_UNEXPECTED_ID:
    fputs (_("* ERROR: unexpected ID in response (the command had none):\n"),
	   data->log_file);
    break;

  case GTP_WARNING_FUTURE_GTP_VERSION:
    fputs (_("* WARNING: future GTP version, I don't know about it....\n"),
	   data->log_file);
    break;

  default:
    assert (0);
  }
}


static void
client_deleted (GtpClient *client, GtkGtpClientData *data)
{
  /* Forbid explicit gtp_client_delete() from outside this module.
   * Other code must use gtk_gtp_client_schedule_deletion().
   */
  assert (client->operation_stage == GTP_CLIENT_QUIT
	  || data->deletion_scheduled);

  clients = g_slist_remove (clients, data);

  if (data->deleted_callback)
    data->deleted_callback (client, data->shutdown_reason, data->user_data);

  if (data->shutdown_reason)
    g_error_free (data->shutdown_reason);

  g_source_remove (data->input_event_source_id);

  g_io_channel_shutdown (data->engine_input, TRUE, NULL);
  g_io_channel_unref (data->engine_input);

  g_io_channel_shutdown (data->engine_output, TRUE, NULL);
  g_io_channel_unref (data->engine_output);

  if (data->engine_stderr) {
    g_source_remove (data->stderr_input_event_source_id);

    g_io_channel_shutdown (data->engine_stderr, TRUE, NULL);
    g_io_channel_unref (data->engine_stderr);
  }

  if (data->log_file && data->log_file != stdout)
    fclose (data->log_file);

  g_free (data);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
