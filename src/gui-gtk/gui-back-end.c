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


#include "gtk-configuration.h"
#include "gtk-control-center.h"
#include "gtk-parser-interface.h"
#include "gtk-preferences.h"
#include "gtk-thread-interface.h"
#include "gtk-tile-set.h"
#include "gtk-utils.h"
#include "gui-back-end.h"
#include "quarry-stock.h"
#include "configuration.h"
#include "time-control.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>


static void	 initialize_main_loop (void);

static void	 run_main_loop (void);
static gboolean	 run_cleanup_tasks (gpointer data);


const char	  *user_real_name;

static gchar	  *configuration_file;

static GPtrArray  *objects_to_finalize = NULL;
static GPtrArray  *pointers_to_free    = NULL;



int
gui_back_end_init (int *argc, char **argv[])
{
#if THREADS_SUPPORTED
  /* gdk_threads_init() is not called because only one (main) thread
   * deals with GUI.
   */
  g_thread_init (NULL);
#endif

  gtk_rc_add_default_file (PACKAGE_DATA_DIR "/gtkrc");

  return gtk_init_check (argc, argv);
}


int
gui_back_end_main_default (void)
{
  initialize_main_loop ();

  gtk_control_center_present ();

  run_main_loop ();

  return 0;
}


int
gui_back_end_main_open_files (int num_files, char **filenames)
{
  int k;

  initialize_main_loop ();

  for (k = 0; k < num_files; k++)
    gtk_parse_sgf_file (filenames[k], NULL, NULL);

  run_main_loop ();

  return 0;
}


void
gui_back_end_register_object_to_finalize (void *object)
{
  g_return_if_fail (G_IS_OBJECT (object));

  if (!objects_to_finalize)
    objects_to_finalize = g_ptr_array_new ();

  g_ptr_array_add (objects_to_finalize, object);
}


void
gui_back_end_register_pointer_to_free (void *pointer)
{
  if (pointer) {
    if (!pointers_to_free)
      pointers_to_free = g_ptr_array_new ();

    g_ptr_array_add (pointers_to_free, pointer);
  }
}



void *
gui_back_end_timer_restart (void *timer_object)
{
  if (timer_object) {
    g_timer_start ((GTimer *) timer_object);
    return timer_object;
  }

  return g_timer_new ();
}


void
gui_back_end_timer_delete (void *timer_object)
{
  g_timer_destroy ((GTimer *) timer_object);
}


double
gui_back_end_timer_get_seconds_elapsed (void *timer_object)
{
  return g_timer_elapsed ((GTimer *) timer_object, NULL);
}


#if THREADS_SUPPORTED



static gboolean  thread_event_callback (gpointer data);

static gboolean	 thread_events_prepare (GSource *source, gint *timeout);
static gboolean	 thread_events_check (GSource *source);
static gboolean	 thread_events_dispatch (GSource *source, GSourceFunc callback,
					 gpointer user_data);


static GSourceFuncs thread_events_functions = {
  thread_events_prepare,
  thread_events_check,
  thread_events_dispatch,
  NULL,
};


static GSource	*thread_events;
GAsyncQueue	*thread_events_queue;


static gboolean
thread_event_callback (gpointer data)
{
  ThreadEventData *thread_completion_data = (ThreadEventData *) data;

  thread_completion_data->callback (thread_completion_data->result);

  return TRUE;
}


static gboolean
thread_events_prepare (GSource *source, gint *timeout)
{
  UNUSED (source);

  *timeout = -1;
  return g_async_queue_length (thread_events_queue) > 0;
}


static gboolean
thread_events_check (GSource *source)
{
  UNUSED (source);

  return g_async_queue_length (thread_events_queue) > 0;
}


static gboolean
thread_events_dispatch (GSource *source, GSourceFunc callback,
			gpointer user_data)
{
  gpointer data = g_async_queue_pop (thread_events_queue);
  gboolean result = callback (data);

  UNUSED (source);
  UNUSED (user_data);

  g_free (data);

  return result;
}


#endif /* THREADS_SUPPORTED */



static void
initialize_main_loop (void)
{
  GtpEngineList site_configuration_engines;

#if THREADS_SUPPORTED
  thread_events_queue = g_async_queue_new ();

  thread_events = g_source_new (&thread_events_functions, sizeof (GSource));
  g_source_set_callback (thread_events, thread_event_callback, NULL, NULL);
  g_source_attach (thread_events, NULL);
#endif

  quarry_stock_init ();

  user_real_name = g_get_real_name ();
  configuration_init (gtk_configuration_sections,
		      NUM_GTK_CONFIGURATION_SECTIONS);

  /* Try to read site configuration file.  Later,
   * configuration_write_to_file() will effectively copy all
   * site-default settings to user's configuration, with any changes
   * she has made.
   *
   * Site configuration file is meant for distributions, to
   * e.g. automatically register GNU Go with Quarry.
   */
  configuration_read_from_file (gtk_configuration_sections,
				NUM_GTK_CONFIGURATION_SECTIONS,
				PACKAGE_DATA_DIR "/quarry.cfg");

  gtp_engine_list_init (&site_configuration_engines);
  string_list_steal_items (&site_configuration_engines, &gtp_engines);

  configuration_file = g_build_path (G_DIR_SEPARATOR_S,
				     g_get_home_dir (), ".quarry", NULL);
  configuration_read_from_file (gtk_configuration_sections,
				NUM_GTK_CONFIGURATION_SECTIONS,
				configuration_file);

  configuration_combine_string_lists (&gtp_engines,
				      &site_configuration_engines,
				      (G_STRUCT_OFFSET
				       (GtpEngineListItem,
					site_configuration_name)));

  gtk_preferences_init ();

#if GTK_2_2_OR_LATER
  gtk_window_set_default_icon_from_file (DATA_DIR "/pixmaps/quarry.png", NULL);
#endif
}


static void
run_main_loop (void)
{
  int k;

  g_timeout_add_full (G_PRIORITY_LOW, 10000, run_cleanup_tasks, NULL, NULL);

  gtk_main ();

  object_cache_free (&gtk_main_tile_set_cache);
  object_cache_free (&gtk_sgf_markup_tile_set_cache);

#if THREADS_SUPPORTED
  g_source_destroy (thread_events);
  g_async_queue_unref (thread_events_queue);
#endif

  if (objects_to_finalize) {
    /* FIXME: Use g_ptr_array_foreach() when we drop support for
     * pre-2.4 GTK+.
     */
    for (k = 0; k < objects_to_finalize->len; k++)
      g_object_unref (g_ptr_array_index (objects_to_finalize, k));

    g_ptr_array_free (objects_to_finalize, TRUE);
  }

  if (pointers_to_free) {
    /* FIXME: Likewise. */
    for (k = 0; k < pointers_to_free->len; k++)
      g_free (g_ptr_array_index (pointers_to_free, k));

    g_ptr_array_free (pointers_to_free, TRUE);
  }

  configuration_write_to_file (gtk_configuration_sections,
			       NUM_GTK_CONFIGURATION_SECTIONS,
			       configuration_file);
  g_free (configuration_file);

  configuration_dispose (gtk_configuration_sections,
			 NUM_GTK_CONFIGURATION_SECTIONS);
}


static gboolean
run_cleanup_tasks (gpointer data)
{
  UNUSED (data);

  object_cache_recycle_dump (&gtk_main_tile_set_cache, 0);
  object_cache_recycle_dump (&gtk_sgf_markup_tile_set_cache, 0);

  return TRUE;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
