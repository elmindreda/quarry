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


#include "quarry-history-text-buffer.h"

#include <assert.h>
#include <gtk/gtk.h>


static void	 quarry_history_text_buffer_class_init
		   (QuarryHistoryTextBufferClass *class);
static void	 quarry_history_text_buffer_init
		   (QuarryHistoryTextBuffer *buffer);

static gboolean	 quarry_history_text_buffer_receive_undo_entry
		   (QuarryTextBuffer *text_buffer,
		    QuarryTextBufferUndoEntry *undo_entry);

static void	 quarry_history_text_buffer_finalize (GObject *object);


GType
quarry_history_text_buffer_get_type (void)
{
  static GType quarry_history_text_buffer_type = 0;

  if (!quarry_history_text_buffer_type) {
    static const GTypeInfo quarry_history_text_buffer_info = {
      sizeof (QuarryHistoryTextBufferClass),
      NULL,
      NULL,
      (GClassInitFunc) quarry_history_text_buffer_class_init,
      NULL,
      NULL,
      sizeof (QuarryHistoryTextBuffer),
      0,
      (GInstanceInitFunc) quarry_history_text_buffer_init,
      NULL
    };

    quarry_history_text_buffer_type
      = g_type_register_static (QUARRY_TYPE_TEXT_BUFFER,
				"QuarryHistoryTextBuffer",
				&quarry_history_text_buffer_info, 0);
  }

  return quarry_history_text_buffer_type;
}


static void
quarry_history_text_buffer_class_init (QuarryHistoryTextBufferClass *class)
{
  G_OBJECT_CLASS (class)->finalize = quarry_history_text_buffer_finalize;

  QUARRY_TEXT_BUFFER_CLASS (class)->receive_undo_entry
    = quarry_history_text_buffer_receive_undo_entry;
}


static void
quarry_history_text_buffer_init (QuarryHistoryTextBuffer *buffer)
{
  buffer->undo_history_begin = NULL;
  buffer->undo_history_end   = NULL;
  buffer->last_applied_entry = NULL;
}


GtkTextBuffer *
quarry_history_text_buffer_new (GtkTextTagTable *tag_table)
{
  return GTK_TEXT_BUFFER (g_object_new (QUARRY_TYPE_HISTORY_TEXT_BUFFER,
					"tag-table", tag_table, NULL));
}


static gboolean
quarry_history_text_buffer_receive_undo_entry
  (QuarryTextBuffer *text_buffer, QuarryTextBufferUndoEntry *undo_entry)
{
  QuarryHistoryTextBuffer *buffer = QUARRY_HISTORY_TEXT_BUFFER (text_buffer);
  GList *undo_history_tail = (buffer->last_applied_entry
			      ? buffer->last_applied_entry->next
			      : buffer->undo_history_begin);

  if (undo_history_tail) {
    buffer->undo_history_end = undo_history_tail->prev;
    undo_history_tail->prev  = NULL;

    if (buffer->undo_history_end)
      buffer->undo_history_end->next = NULL;
    else
      buffer->undo_history_begin = NULL;

    g_list_foreach (undo_history_tail,
		    (GFunc) quarry_text_buffer_undo_entry_delete, NULL);
    g_list_free (undo_history_tail);
  }
  else if (buffer->last_applied_entry) {
    QuarryTextBufferUndoEntry *last_undo_entry
      = (QuarryTextBufferUndoEntry *) buffer->last_applied_entry->data;

    if (quarry_text_buffer_combine_undo_entries
	(text_buffer, last_undo_entry, undo_entry)) {
      /* The new entry is merged into the previous, we are done. */
      return TRUE;
    }
  }

  if (buffer->undo_history_end) {
    buffer->undo_history_end
      = g_list_append (buffer->undo_history_end, undo_entry)->next;
  }
  else {
    buffer->undo_history_begin = g_list_append (NULL, undo_entry);
    buffer->undo_history_end   = buffer->undo_history_begin;
  }

  buffer->last_applied_entry = buffer->undo_history_end;

  return TRUE;
}


static void
quarry_history_text_buffer_finalize (GObject *object)
{
  quarry_history_text_buffer_reset_history (QUARRY_HISTORY_TEXT_BUFFER
					    (object));
}


gboolean
quarry_history_text_buffer_can_undo (const QuarryHistoryTextBuffer *buffer)
{
  assert (QUARRY_IS_HISTORY_TEXT_BUFFER (buffer));

  return buffer->last_applied_entry != NULL;
}


gboolean
quarry_history_text_buffer_can_redo (const QuarryHistoryTextBuffer *buffer)
{
  assert (QUARRY_IS_HISTORY_TEXT_BUFFER (buffer));

  return buffer->last_applied_entry != buffer->undo_history_end;
}


void
quarry_history_text_buffer_undo (QuarryHistoryTextBuffer *buffer)
{
  assert (quarry_history_text_buffer_can_undo (buffer));

  quarry_text_buffer_undo (&buffer->text_buffer,
			   ((const QuarryTextBufferUndoEntry *)
			    buffer->last_applied_entry->data));
  buffer->last_applied_entry = buffer->last_applied_entry->prev;
}


void
quarry_history_text_buffer_redo (QuarryHistoryTextBuffer *buffer)
{
  assert (quarry_history_text_buffer_can_redo (buffer));

  buffer->last_applied_entry = (buffer->last_applied_entry
				? buffer->last_applied_entry->next
				: buffer->undo_history_begin);

  quarry_text_buffer_redo (&buffer->text_buffer,
			   ((const QuarryTextBufferUndoEntry *)
			    buffer->last_applied_entry->data));
}


void
quarry_history_text_buffer_reset_history (QuarryHistoryTextBuffer *buffer)
{
  assert (QUARRY_IS_HISTORY_TEXT_BUFFER (buffer));

  g_list_foreach (buffer->undo_history_begin,
		  (GFunc) quarry_text_buffer_undo_entry_delete, NULL);
  g_list_free (buffer->undo_history_begin);

  buffer->undo_history_begin = NULL;
  buffer->undo_history_end   = NULL;
  buffer->last_applied_entry = NULL;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
