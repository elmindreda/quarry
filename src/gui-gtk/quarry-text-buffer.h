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


#ifndef QUARRY_QUARRY_TEXT_BUFFER_H
#define QUARRY_QUARRY_TEXT_BUFFER_H


#include "quarry.h"

#include <gtk/gtk.h>


#define QUARRY_TYPE_TEXT_BUFFER	(quarry_text_buffer_get_type ())

#define QUARRY_TEXT_BUFFER(object)					\
  GTK_CHECK_CAST ((object), QUARRY_TYPE_TEXT_BUFFER,			\
		  QuarryTextBuffer)

#define QUARRY_TEXT_BUFFER_CLASS(class)					\
  GTK_CHECK_CLASS_CAST ((class), QUARRY_TYPE_TEXT_BUFFER,		\
			QuarryTextBufferClass)

#define QUARRY_IS_TEXT_BUFFER(object)					\
  GTK_CHECK_TYPE ((object), QUARRY_TYPE_TEXT_BUFFER)

#define QUARRY_IS_TEXT_BUFFER_CLASS(class)				\
  GTK_CHECK_CLASS_TYPE ((class), QUARRY_TYPE_TEXT_BUFFER)

#define QUARRY_TEXT_BUFFER_GET_CLASS(object)				\
  GTK_CHECK_GET_CLASS ((object), QUARRY_TYPE_TEXT_BUFFER,		\
		       QuarryTextBufferClass)


typedef struct _QuarryTextBufferUndoEntry	QuarryTextBufferUndoEntry;


typedef struct _QuarryTextBuffer		QuarryTextBuffer;
typedef struct _QuarryTextBufferClass		QuarryTextBufferClass;

struct _QuarryTextBuffer {
  GtkTextBuffer		      text_buffer;

  QuarryTextBufferUndoEntry  *current_undo_entry;
  guint			      modifications_count;

  glong			      last_modification_time;
  glong			      previous_modification_time;
};

struct _QuarryTextBufferClass {
  GtkTextBufferClass   parent_class;

  gboolean (* receive_undo_entry) (QuarryTextBuffer *buffer,
				   QuarryTextBufferUndoEntry *undo_entry);
};


GType		 quarry_text_buffer_get_type (void);

GtkTextBuffer *	 quarry_text_buffer_new (GtkTextTagTable *tag_table);

void		 quarry_text_buffer_undo
		   (QuarryTextBuffer *buffer,
		    const QuarryTextBufferUndoEntry *undo_entry);
void		 quarry_text_buffer_redo
		   (QuarryTextBuffer *buffer,
		    const QuarryTextBufferUndoEntry *undo_entry);

gboolean	 quarry_text_buffer_combine_undo_entries
		   (QuarryTextBuffer *buffer,
		    QuarryTextBufferUndoEntry *previous_undo_entry,
		    QuarryTextBufferUndoEntry *current_undo_entry);


QuarryTextBufferUndoEntry *
		 quarry_text_buffer_undo_entry_new (void);
void		 quarry_text_buffer_undo_entry_delete
		   (QuarryTextBufferUndoEntry *undo_entry);

inline gboolean	 quarry_text_buffer_undo_entry_is_empty
		   (const QuarryTextBufferUndoEntry *undo_entry);


#endif /* QUARRY_QUARRY_TEXT_BUFFER_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
