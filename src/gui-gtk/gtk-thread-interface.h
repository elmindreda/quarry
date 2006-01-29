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


#ifndef QUARRY_GTK_THREAD_INTERFACE_H
#define QUARRY_GTK_THREAD_INTERFACE_H


#include "quarry.h"

#include <glib.h>


#define THREADS_SUPPORTED	(defined(G_THREADS_ENABLED)		\
				 && !defined(G_THREADS_IMPL_NONE))

#if THREADS_SUPPORTED


#include "gtk-parser-interface.h"
#include "sgf.h"

#include <gtk/gtk.h>


typedef void (* ThreadEventCallback) (void *result);

typedef struct _ThreadEventData	ThreadEventData;

struct _ThreadEventData {
  ThreadEventCallback	callback;
  void		       *result;
};


typedef struct _ParsingThreadData	ParsingThreadData;

struct _ParsingThreadData {
  char		       *filename;

  SgfCollection	       *sgf_collection;
  SgfErrorList	       *error_list;

  int			file_size;
  int			bytes_parsed;
  int			cancellation_flag;

  int			result;

  GtkWidget	       *parent;
  GtkWidget	       *progress_dialog;

  GtkHandleParsedData	callback;
};


extern GAsyncQueue     *thread_events_queue;


#endif /* THREADS_SUPPORTED */


#endif /* QUARRY_GTK_THREAD_INTERFACE_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
