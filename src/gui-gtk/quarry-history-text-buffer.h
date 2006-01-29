/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2006 Paul Pogonyshev.                             *
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


#ifndef QUARRY_QUARRY_HISTORY_TEXT_BUFFER_H
#define QUARRY_QUARRY_HISTORY_TEXT_BUFFER_H


#include "quarry-text-buffer.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define QUARRY_TYPE_HISTORY_TEXT_BUFFER					\
  (quarry_history_text_buffer_get_type ())

#define QUARRY_HISTORY_TEXT_BUFFER(object)				\
  GTK_CHECK_CAST ((object), QUARRY_TYPE_HISTORY_TEXT_BUFFER,		\
		  QuarryHistoryTextBuffer)

#define QUARRY_HISTORY_TEXT_BUFFER_CLASS(class)				\
  GTK_CHECK_CLASS_CAST ((class), QUARRY_TYPE_HISTORY_TEXT_BUFFER,	\
			QuarryHistoryTextBufferClass)

#define QUARRY_IS_HISTORY_TEXT_BUFFER(object)				\
  GTK_CHECK_TYPE ((object), QUARRY_TYPE_HISTORY_TEXT_BUFFER)

#define QUARRY_IS_HISTORY_TEXT_BUFFER_CLASS(class)			\
  GTK_CHECK_CLASS_TYPE ((class), QUARRY_TYPE_HISTORY_TEXT_BUFFER)

#define QUARRY_HISTORY_TEXT_BUFFER_GET_CLASS(object)			\
  GTK_CHECK_GET_CLASS ((object), QUARRY_TYPE_HISTORY_TEXT_BUFFER,	\
		       QuarryHistoryTextBufferClass)


typedef struct _QuarryHistoryTextBuffer		QuarryHistoryTextBuffer;
typedef struct _QuarryHistoryTextBufferClass	QuarryHistoryTextBufferClass;

struct _QuarryHistoryTextBuffer {
  QuarryTextBuffer	  text_buffer;

  GList			 *undo_history_begin;
  GList			 *undo_history_end;
  GList			 *last_applied_entry;
};

struct _QuarryHistoryTextBufferClass {
  QuarryTextBufferClass   parent_class;
};


GType		 quarry_history_text_buffer_get_type (void);

GtkTextBuffer *	 quarry_history_text_buffer_new (GtkTextTagTable *tag_table);

gboolean	 quarry_history_text_buffer_can_undo
		   (const QuarryHistoryTextBuffer *text_buffer);
gboolean	 quarry_history_text_buffer_can_redo
		   (const QuarryHistoryTextBuffer *text_buffer);

void		 quarry_history_text_buffer_undo
		   (QuarryHistoryTextBuffer *text_buffer);
void		 quarry_history_text_buffer_redo
		   (QuarryHistoryTextBuffer *text_buffer);


#endif /* QUARRY_QUARRY_HISTORY_TEXT_BUFFER_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
