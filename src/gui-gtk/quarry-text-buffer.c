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


#include "quarry-text-buffer.h"

#include "quarry-marshal.h"

#include <assert.h>
#include <gtk/gtk.h>


/* FIXME: Make typed words be lumped into single undo operations. */


/* NOTE: Keep in sync with operations[]. */
typedef enum {
  OPERATION_TEXT_DELETION,
  OPERATION_TEXT_INSERTION,
  NUM_TEXT_BUFFER_OPERATIONS
} OperationType;


typedef struct _QuarryTextBufferOperation	QuarryTextBufferOperation;
typedef struct _QuarryTextBufferTextOperation	QuarryTextBufferTextOperation;
typedef struct _QuarryTextBufferTagOperation	QuarryTextBufferTagOperation;

struct _QuarryTextBufferUndoEntry {
  QuarryTextBufferOperation  *first;
  QuarryTextBufferOperation  *last;

  gint			      cursor_offset_before;
  gint			      cursor_offset_after;
};

struct _QuarryTextBufferOperation {
  QuarryTextBufferOperation  *next;
  QuarryTextBufferOperation  *previous;

  OperationType		      operation_type;
};

struct _QuarryTextBufferTextOperation {
  QuarryTextBufferOperation  operation;

  gchar			    *text;
  gint			     offset;
};


typedef struct _QuarryTextBufferOperationInfo	QuarryTextBufferOperationInfo;

struct _QuarryTextBufferOperationInfo {
  void (* undo) (const QuarryTextBufferOperation *operation,
		 QuarryTextBuffer *buffer);
  void (* redo) (const QuarryTextBufferOperation *operation,
		 QuarryTextBuffer *buffer);

  void (* free_data) (const QuarryTextBufferOperation *operation);
};


static void	 quarry_text_buffer_class_init (QuarryTextBufferClass *class);
static void	 quarry_text_buffer_init (QuarryTextBuffer *buffer);

static void	 quarry_text_buffer_delete_range (GtkTextBuffer *text_buffer,
						  GtkTextIter *from,
						  GtkTextIter *to);
static void	 quarry_text_buffer_insert_text (GtkTextBuffer *text_buffer,
						 GtkTextIter *where,
						 const gchar *text,
						 gint length);
static void	 quarry_text_buffer_apply_tag (GtkTextBuffer *text_buffer,
					       GtkTextTag *tag,
					       const GtkTextIter *from,
					       const GtkTextIter *to);
static void	 quarry_text_buffer_remove_tag (GtkTextBuffer *text_buffer,
						GtkTextTag *tag,
						const GtkTextIter *from,
						const GtkTextIter *to);

static void	 quarry_text_buffer_begin_user_action
		   (GtkTextBuffer *text_buffer);
static void	 quarry_text_buffer_end_user_action
		   (GtkTextBuffer *text_buffer);

static gboolean	 quarry_text_buffer_receive_undo_entry
		   (QuarryTextBuffer *buffer,
		    QuarryTextBufferUndoEntry *undo_entry);

static void	 quarry_text_buffer_finalize (GObject *object);

static QuarryTextBufferOperation *
		 add_operation (QuarryTextBuffer *buffer,
				OperationType operation_type,
				gint operation_structure_size);


static void	 insert_text (const QuarryTextBufferOperation *operation,
			      QuarryTextBuffer *buffer);
static void	 delete_text (const QuarryTextBufferOperation *operation,
			      QuarryTextBuffer *buffer);
static void	 free_text (const QuarryTextBufferOperation *operation);


/* NOTE: Keep in sync with OperationType.
 *
 * We could use a parser similar to one used with SGF undo histories,
 * but it seems like an overkill in this case.
 */
static const QuarryTextBufferOperationInfo operations[]
  = { { insert_text, delete_text, free_text },
      { delete_text, insert_text, free_text } };


static GtkTextBufferClass  *parent_class;


enum {
  RECEIVE_UNDO_ENTRY,
  NUM_SIGNALS
};

static guint		    text_buffer_signals[NUM_SIGNALS];


GType
quarry_text_buffer_get_type (void)
{
  static GType quarry_text_buffer_type = 0;

  if (!quarry_text_buffer_type) {
    static const GTypeInfo quarry_text_buffer_info = {
      sizeof (QuarryTextBufferClass),
      NULL,
      NULL,
      (GClassInitFunc) quarry_text_buffer_class_init,
      NULL,
      NULL,
      sizeof (QuarryTextBuffer),
      1,
      (GInstanceInitFunc) quarry_text_buffer_init,
      NULL
    };

    quarry_text_buffer_type
      = g_type_register_static (GTK_TYPE_TEXT_BUFFER, "QuarryTextBuffer",
				&quarry_text_buffer_info, 0);
  }

  return quarry_text_buffer_type;
}


static void
quarry_text_buffer_class_init (QuarryTextBufferClass *class)
{
  GtkTextBufferClass *text_buffer_class = GTK_TEXT_BUFFER_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  G_OBJECT_CLASS (class)->finalize     = quarry_text_buffer_finalize;

  text_buffer_class->delete_range      = quarry_text_buffer_delete_range;
  text_buffer_class->insert_text       = quarry_text_buffer_insert_text;

  text_buffer_class->apply_tag	       = quarry_text_buffer_apply_tag;
  text_buffer_class->remove_tag	       = quarry_text_buffer_remove_tag;

  /* FIXME: Is it needed? */
#if 0
  text_buffer_class->mark_deleted      = quarry_text_buffer_mark_deleted;
  text_buffer_class->mark_set	       = quarry_text_buffer_mark_set;
#endif

  text_buffer_class->begin_user_action = quarry_text_buffer_begin_user_action;
  text_buffer_class->end_user_action   = quarry_text_buffer_end_user_action;

  class->receive_undo_entry	       = quarry_text_buffer_receive_undo_entry;

  text_buffer_signals[RECEIVE_UNDO_ENTRY]
    = g_signal_new ("receive-undo-entry",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (QuarryTextBufferClass,
				     receive_undo_entry),
		    g_signal_accumulator_true_handled, NULL,
		    quarry_marshal_BOOLEAN__POINTER,
		    G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
}


static void
quarry_text_buffer_init (QuarryTextBuffer *buffer)
{
  buffer->current_undo_entry = NULL;
}


GtkTextBuffer *
quarry_text_buffer_new (GtkTextTagTable *tag_table)
{
  return GTK_TEXT_BUFFER (g_object_new (QUARRY_TYPE_TEXT_BUFFER,
					"tag-table", tag_table, NULL));
}


static void
quarry_text_buffer_delete_range (GtkTextBuffer *text_buffer,
				 GtkTextIter *from, GtkTextIter *to)
{
  QuarryTextBuffer *buffer = QUARRY_TEXT_BUFFER (text_buffer);

  if (buffer->current_undo_entry) {
    QuarryTextBufferTextOperation *operation
      = ((QuarryTextBufferTextOperation *)
	 add_operation (buffer, OPERATION_TEXT_DELETION,
			sizeof (QuarryTextBufferTextOperation)));

    operation->text   = gtk_text_iter_get_text (from, to);
    operation->offset = gtk_text_iter_get_offset (from);
  }

  parent_class->delete_range (text_buffer, from, to);
}


static void
quarry_text_buffer_insert_text (GtkTextBuffer *text_buffer, GtkTextIter *where,
				const gchar *text, gint length)
{
  QuarryTextBuffer *buffer = QUARRY_TEXT_BUFFER (text_buffer);

  if (buffer->current_undo_entry) {
    QuarryTextBufferTextOperation *operation
      = ((QuarryTextBufferTextOperation *)
	 add_operation (buffer, OPERATION_TEXT_INSERTION,
			sizeof (QuarryTextBufferTextOperation)));

    operation->text   = g_strndup (text, length);
    operation->offset = gtk_text_iter_get_offset (where);
  }

  parent_class->insert_text (text_buffer, where, text, length);
}


static void
quarry_text_buffer_apply_tag (GtkTextBuffer *text_buffer, GtkTextTag *tag,
			      const GtkTextIter *from, const GtkTextIter *to)
{
  /* FIXME: Implement. */
  parent_class->apply_tag (text_buffer, tag, from, to);
}


static void
quarry_text_buffer_remove_tag (GtkTextBuffer *text_buffer, GtkTextTag *tag,
			       const GtkTextIter *from, const GtkTextIter *to)
{
  /* FIXME: Implement. */
  parent_class->remove_tag (text_buffer, tag, from, to);
}


static void
quarry_text_buffer_begin_user_action (GtkTextBuffer *text_buffer)
{
  GtkTextIter cursor_iterator;
  QuarryTextBuffer *buffer = QUARRY_TEXT_BUFFER (text_buffer);

  assert (!buffer->current_undo_entry);

  gtk_text_buffer_get_iter_at_mark (text_buffer, &cursor_iterator,
				    gtk_text_buffer_get_insert (text_buffer));

  buffer->current_undo_entry = quarry_text_buffer_undo_entry_new ();
  buffer->current_undo_entry->cursor_offset_before
    = gtk_text_iter_get_offset (&cursor_iterator);

  if (parent_class->begin_user_action)
    parent_class->begin_user_action (text_buffer);
}


static void
quarry_text_buffer_end_user_action (GtkTextBuffer *text_buffer)
{
  QuarryTextBuffer *buffer = QUARRY_TEXT_BUFFER (text_buffer);

  if (buffer->current_undo_entry) {
    if (0&&quarry_text_buffer_undo_entry_is_empty (buffer->current_undo_entry))
      quarry_text_buffer_undo_entry_delete (buffer->current_undo_entry);
    else {
      GtkTextIter cursor_iterator;

      /* Actually, we know that the result will be TRUE and don't
       * really care, but GLib doesn't allow passing NULL for result
       * value location.
       */
      gboolean result;

      gtk_text_buffer_get_iter_at_mark (text_buffer, &cursor_iterator,
					(gtk_text_buffer_get_insert
					 (text_buffer)));
      buffer->current_undo_entry->cursor_offset_after
	= gtk_text_iter_get_offset (&cursor_iterator);

      g_signal_emit (buffer, text_buffer_signals[RECEIVE_UNDO_ENTRY], 0,
		     buffer->current_undo_entry, &result);
    }

    buffer->current_undo_entry = NULL;
  }

  if (parent_class->end_user_action)
    parent_class->end_user_action (text_buffer);
}


static gboolean
quarry_text_buffer_receive_undo_entry (QuarryTextBuffer *buffer,
				       QuarryTextBufferUndoEntry *undo_entry)
{
  UNUSED (buffer);

  /* If this gets called, it means nobody cared for the entry.  Since
   * we don't maintain an undo history here, just delete the entry.
   */
  quarry_text_buffer_undo_entry_delete (undo_entry);

  return TRUE;
}


static void
quarry_text_buffer_finalize (GObject *object)
{
  QuarryTextBufferUndoEntry *undo_entry =
    QUARRY_TEXT_BUFFER (object)->current_undo_entry;

  if (undo_entry)
    quarry_text_buffer_undo_entry_delete (undo_entry);
}


void
quarry_text_buffer_undo (QuarryTextBuffer *buffer,
			 const QuarryTextBufferUndoEntry *undo_entry)
{
  GtkTextIter cursor_iterator;
  const QuarryTextBufferOperation *operation;

  assert (QUARRY_IS_TEXT_BUFFER (buffer));
  assert (!buffer->current_undo_entry);
  assert (undo_entry);

  for (operation = undo_entry->last; operation;
       operation = operation->previous)
    operations[operation->operation_type].undo (operation, buffer);

  gtk_text_buffer_get_iter_at_offset (&buffer->text_buffer, &cursor_iterator,
				      undo_entry->cursor_offset_before);
  gtk_text_buffer_place_cursor (&buffer->text_buffer, &cursor_iterator);
}


void
quarry_text_buffer_redo (QuarryTextBuffer *buffer,
			 const QuarryTextBufferUndoEntry *undo_entry)
{
  GtkTextIter cursor_iterator;
  const QuarryTextBufferOperation *operation;

  assert (QUARRY_IS_TEXT_BUFFER (buffer));
  assert (!buffer->current_undo_entry);
  assert (undo_entry);

  for (operation = undo_entry->first; operation; operation = operation->next)
    operations[operation->operation_type].redo (operation, buffer);

  gtk_text_buffer_get_iter_at_offset (&buffer->text_buffer, &cursor_iterator,
				      undo_entry->cursor_offset_after);
  gtk_text_buffer_place_cursor (&buffer->text_buffer, &cursor_iterator);
}




QuarryTextBufferUndoEntry *
quarry_text_buffer_undo_entry_new (void)
{
  QuarryTextBufferUndoEntry *undo_entry
    = g_malloc (sizeof (QuarryTextBufferUndoEntry));

  undo_entry->first = NULL;
  undo_entry->last  = NULL;

  return undo_entry;
}


void
quarry_text_buffer_undo_entry_delete (QuarryTextBufferUndoEntry *undo_entry)
{
  QuarryTextBufferOperation *operation;

  assert (undo_entry);

  operation = undo_entry->first;
  while (operation) {
    QuarryTextBufferOperation *next_operation = operation->next;

    if (operations[operation->operation_type].free_data)
      operations[operation->operation_type].free_data (operation);

    g_free (operation);

    operation = next_operation;
  }

  g_free (undo_entry);
}


inline gboolean
quarry_text_buffer_undo_entry_is_empty
  (const QuarryTextBufferUndoEntry *undo_entry)
{
  assert (undo_entry);
  return undo_entry->first == NULL;
}



static QuarryTextBufferOperation *
add_operation (QuarryTextBuffer *buffer,
	       OperationType operation_type, gint operation_structure_size)
{
  QuarryTextBufferOperation *operation = g_malloc (operation_structure_size);

  operation->next	    = NULL;
  operation->previous	    = buffer->current_undo_entry->last;
  operation->operation_type = operation_type;

  if (buffer->current_undo_entry->first == NULL)
    buffer->current_undo_entry->first = operation;
  else
    buffer->current_undo_entry->last->next = operation;

  buffer->current_undo_entry->last = operation;

  return operation;
}




static void
insert_text (const QuarryTextBufferOperation *operation,
	     QuarryTextBuffer *buffer)
{
  const QuarryTextBufferTextOperation *text_operation
    = (const QuarryTextBufferTextOperation *) operation;
  GtkTextIter iterator;

  gtk_text_buffer_get_iter_at_offset (&buffer->text_buffer, &iterator,
				      text_operation->offset);
  gtk_text_buffer_insert (&buffer->text_buffer, &iterator,
			  text_operation->text, -1);
}


static void
delete_text (const QuarryTextBufferOperation *operation,
	     QuarryTextBuffer *buffer)
{
  const QuarryTextBufferTextOperation *text_operation
    = (const QuarryTextBufferTextOperation *) operation;
  int end_offset = (text_operation->offset
		    + g_utf8_strlen (text_operation->text, -1));
  GtkTextIter start_iterator;
  GtkTextIter end_iterator;

  gtk_text_buffer_get_iter_at_offset (&buffer->text_buffer, &start_iterator,
				      text_operation->offset);
  gtk_text_buffer_get_iter_at_offset (&buffer->text_buffer, &end_iterator,
				      end_offset);
  gtk_text_buffer_delete (&buffer->text_buffer,
			  &start_iterator, &end_iterator);
}


static void
free_text (const QuarryTextBufferOperation *operation)
{
  g_free (((QuarryTextBufferTextOperation *) operation)->text);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
