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


#include "gui-back-end.h"
#include "gtk-thread-interface.h"
#include "gtk-tile-set-interface.h"
#include "gtk-control-center.h"
#include "gtk-parser-interface.h"
#include "gtk-configuration.h"
#include "gtk-preferences.h"
#include "quarry-stock.h"
#include "configuration.h"
#include "utils.h"

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdlib.h>
#include <assert.h>


static void	 initialize_main_loop(void);

static void	 run_main_loop(void);
static gboolean	 run_cleanup_tasks(gpointer data);


const char     *user_real_name;

static gchar   *configuration_file;



int
gui_back_end_init(int *argc, char **argv[])
{
#if THREADS_SUPPORTED
  /* gdk_threads_init() is not called because only one (main) thread
   * deals with GUI.
   */
  g_thread_init(NULL);
#endif

  gtk_rc_add_default_file(PACKAGE_DATA_DIR "/gtkrc");

  return gtk_init_check(argc, argv);
}


int
gui_back_end_main_default(void)
{
  initialize_main_loop();

  gtk_control_center_present();

  run_main_loop();

  return 0;
}


int
gui_back_end_main_open_files(int num_files, char **filenames)
{
  int k;

  initialize_main_loop();

  for (k = 0; k < num_files; k++)
    gtk_parse_sgf_file(filenames[k], NULL);

  run_main_loop();

  return 0;
}



void *
gui_back_end_image_new(const unsigned char *pixel_data, int width, int height)
{
  GdkPixbuf *pixbuf =
    gdk_pixbuf_new_from_data(pixel_data, GDK_COLORSPACE_RGB,
			     TRUE, 8, width, height,
			     width * 4 * sizeof(unsigned char),
			     (GdkPixbufDestroyNotify) utils_free, NULL);

  assert(pixbuf);
  return (void *) pixbuf;
}


void
gui_back_end_image_delete(void *image)
{
  assert(image);
  g_object_unref(image);
}


#if THREADS_SUPPORTED



static gboolean  thread_event_callback(gpointer data);

static gboolean	 thread_events_prepare(GSource *source, gint *timeout);
static gboolean	 thread_events_check(GSource *source);
static gboolean	 thread_events_dispatch(GSource *source, GSourceFunc callback,
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
thread_event_callback(gpointer data)
{
  ThreadEventData *thread_completion_data = (ThreadEventData *) data;

  thread_completion_data->callback(thread_completion_data->result);

  return TRUE;
}


static gboolean
thread_events_prepare(GSource *source, gint *timeout)
{
  UNUSED(source);

  *timeout = -1;
  return g_async_queue_length(thread_events_queue) > 0;
}


static gboolean
thread_events_check(GSource *source)
{
  UNUSED(source);

  return g_async_queue_length(thread_events_queue) > 0;
}


static gboolean
thread_events_dispatch(GSource *source, GSourceFunc callback,
		       gpointer user_data)
{
  gpointer data = g_async_queue_pop(thread_events_queue);
  gboolean result = callback(data);

  UNUSED(source);
  UNUSED(user_data);  

  g_free(data);

  return result;
}


#endif /* THREADS_SUPPORTED */



static void
initialize_main_loop(void)
{
#if THREADS_SUPPORTED
  thread_events_queue = g_async_queue_new();

  thread_events = g_source_new(&thread_events_functions, sizeof(GSource));
  g_source_set_callback(thread_events, thread_event_callback, NULL, NULL);
  g_source_attach(thread_events, NULL);
#endif

  quarry_stock_init();

  user_real_name = g_get_real_name();
  configuration_init(gtk_configuration_sections,
		     NUM_GTK_CONFIGURATION_SECTIONS);

  configuration_file = g_build_path(G_DIR_SEPARATOR_S,
				    g_get_home_dir(), ".quarry", NULL);
  configuration_read_from_file(gtk_configuration_sections,
			       NUM_GTK_CONFIGURATION_SECTIONS,
			       configuration_file);

  gtk_preferences_init();
}


static void
run_main_loop(void)
{
  g_timeout_add_full(G_PRIORITY_LOW, 10000, run_cleanup_tasks, NULL, NULL);

  gtk_main();

  /* Ensure that dumped tile sets are all deleted. */
  tile_set_dump_recycle(0);
  tile_set_dump_recycle(0);

#if THREADS_SUPPORTED
  g_source_destroy(thread_events);
  g_async_queue_unref(thread_events_queue);
#endif

  gtk_preferences_finalize();

  configuration_write_to_file(gtk_configuration_sections,
			      NUM_GTK_CONFIGURATION_SECTIONS,
			      configuration_file);
  g_free(configuration_file);

  configuration_dispose(gtk_configuration_sections,
			NUM_GTK_CONFIGURATION_SECTIONS);
}


static gboolean
run_cleanup_tasks(gpointer data)
{
  UNUSED(data);

  tile_set_dump_recycle(0);

  return TRUE;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
