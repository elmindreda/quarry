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

#include <gtk/gtk.h>
#include <string.h>


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

  guint			      state_index_before;
  guint			      state_index_after;
};

struct _QuarryTextBufferOperation {
  QuarryTextBufferOperation  *next;
  QuarryTextBufferOperation  *previous;

  OperationType		      type;
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
static void	 quarry_text_buffer_modified_changed
		   (GtkTextBuffer *text_buffer);

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

  text_buffer_class->modified_changed  = quarry_text_buffer_modified_changed;

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
  buffer->current_undo_entry	     = NULL;
  buffer->state_index		     = 0;
  buffer->last_assigned_state_index  = 0;
  buffer->unmodified_state_index     = 0;

  buffer->last_modification_time     = 0;
  buffer->previous_modification_time = 0;
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

  buffer->state_index = ++buffer->last_assigned_state_index;
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

  buffer->state_index = ++buffer->last_assigned_state_index;
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
quarry_text_buffer_modified_changed (GtkTextBuffer *text_buffer)
{
  QuarryTextBuffer *buffer = QUARRY_TEXT_BUFFER (text_buffer);

  if (gtk_text_buffer_get_modified (text_buffer)) {
    if (buffer->state_index == buffer->unmodified_state_index) {
      /* The current state is explicitly set to `modified.'  Assume
       * there is no unmodified state at all.
       */
      buffer->unmodified_state_index = G_MAXUINT;
    }
  }
  else {
    /* The current state is declared to be unmodified.  Or maybe we
     * just returned to an unmodified state (in this case the below
     * operator does nothing anyway.)
     */
    buffer->unmodified_state_index = buffer->state_index;
  }

  if (parent_class->modified_changed)
    parent_class->modified_changed (text_buffer);
}


static void
quarry_text_buffer_begin_user_action (GtkTextBuffer *text_buffer)
{
  QuarryTextBuffer *buffer = QUARRY_TEXT_BUFFER (text_buffer);
  GtkTextIter cursor_iterator;
  GTimeVal current_time;

  if (!buffer->current_undo_entry) {
    gtk_text_buffer_get_iter_at_mark
      (text_buffer, &cursor_iterator,
       gtk_text_buffer_get_insert (text_buffer));

    buffer->current_undo_entry = quarry_text_buffer_undo_entry_new ();

    buffer->current_undo_entry->cursor_offset_before
      = gtk_text_iter_get_offset (&cursor_iterator);
    buffer->current_undo_entry->state_index_before = buffer->state_index;

    g_get_current_time (&current_time);

    buffer->previous_modification_time = buffer->last_modification_time;
    buffer->last_modification_time     = current_time.tv_sec;
  }
  else
    g_critical ("inconsistent text buffer state...");

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
      buffer->current_undo_entry->state_index_after = buffer->state_index;

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
  guint last_assigned_state_index_copy;

  g_return_if_fail (QUARRY_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (!buffer->current_undo_entry);
  g_return_if_fail (undo_entry);
  g_return_if_fail (undo_entry->state_index_after == buffer->state_index);

  /* Not really necessary.  Just save a few states indices. */
  last_assigned_state_index_copy = buffer->last_assigned_state_index;

  for (operation = undo_entry->last; operation;
       operation = operation->previous)
    operations[operation->type].undo (operation, buffer);

  buffer->state_index		    = undo_entry->state_index_before;
  buffer->last_assigned_state_index = last_assigned_state_index_copy;

  gtk_text_buffer_get_iter_at_offset (&buffer->text_buffer, &cursor_iterator,
				      undo_entry->cursor_offset_before);
  gtk_text_buffer_place_cursor (&buffer->text_buffer, &cursor_iterator);

  if (buffer->state_index == buffer->unmodified_state_index)
    gtk_text_buffer_set_modified (&buffer->text_buffer, FALSE);

  /* This a semi-hackish way to prevent undo entry combinations after
   * an undo operation, see quarry_text_buffer_combine_undo_entries().
   */
  buffer->last_modification_time = 0;
}


void
quarry_text_buffer_redo (QuarryTextBuffer *buffer,
			 const QuarryTextBufferUndoEntry *undo_entry)
{
  GtkTextIter cursor_iterator;
  const QuarryTextBufferOperation *operation;
  guint last_assigned_state_index_copy;

  g_return_if_fail (QUARRY_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (!buffer->current_undo_entry);
  g_return_if_fail (undo_entry);
  g_return_if_fail (undo_entry->state_index_before == buffer->state_index);

  /* Not really necessary.  Just save a few states indices. */
  last_assigned_state_index_copy = buffer->last_assigned_state_index;

  for (operation = undo_entry->first; operation; operation = operation->next)
    operations[operation->type].redo (operation, buffer);

  buffer->state_index		    = undo_entry->state_index_after;
  buffer->last_assigned_state_index = last_assigned_state_index_copy;

  gtk_text_buffer_get_iter_at_offset (&buffer->text_buffer, &cursor_iterator,
				      undo_entry->cursor_offset_after);
  gtk_text_buffer_place_cursor (&buffer->text_buffer, &cursor_iterator);

  if (buffer->state_index == buffer->unmodified_state_index)
    gtk_text_buffer_set_modified (&buffer->text_buffer, FALSE);

  /* This a semi-hackish way to prevent undo entry combinations after
   * a redo operation, see quarry_text_buffer_combine_undo_entries().
   */
  buffer->last_modification_time = 0;
}


gboolean
quarry_text_buffer_combine_undo_entries
  (QuarryTextBuffer *buffer,
   QuarryTextBufferUndoEntry *previous_undo_entry,
   QuarryTextBufferUndoEntry *current_undo_entry)
{
  QuarryTextBufferTextOperation *current_operation;
  QuarryTextBufferTextOperation *previous_operation;

  g_return_val_if_fail (QUARRY_IS_TEXT_BUFFER (buffer), FALSE);

  /* Don't combine if the user made a significant pause.  Sometimes we
   * also specifically set things up for this check to fail to inhibit
   * undo entry combination.
   */
  if (buffer->last_modification_time - buffer->previous_modification_time
      >= 15)
    return FALSE;

  /* Basic checks. */
  if (!previous_undo_entry || !current_undo_entry
      || quarry_text_buffer_undo_entry_is_empty (current_undo_entry)
      || current_undo_entry->first  != current_undo_entry->last
      || quarry_text_buffer_undo_entry_is_empty (previous_undo_entry)
      || previous_undo_entry->first != previous_undo_entry->last)
    return FALSE;

  /* State checks: can only combine the two latest undo entries. */
  if (current_undo_entry->state_index_after != buffer->state_index
      || (previous_undo_entry->state_index_after
	  != current_undo_entry->state_index_before))
    return FALSE;

  current_operation
    = (QuarryTextBufferTextOperation *) current_undo_entry->first;
  previous_operation
    = (QuarryTextBufferTextOperation *) previous_undo_entry->first;

  /* Operation type checks: only text deletion or insertion and must
   * be the same for both entries.
   */
  if (current_operation->operation.type != previous_operation->operation.type
      || (current_operation->operation.type != OPERATION_TEXT_DELETION
	  && current_operation->operation.type != OPERATION_TEXT_INSERTION))
    return FALSE;

  /* Only work with single character operations.  Ideally, should only
   * count _typed/backspaced/deleted_ characters, but I don't see a
   * way to determine that.
   */
  if (!*current_operation->text || *g_utf8_next_char (current_operation->text))
    return FALSE;

  /* Probably not ideal, but it seems gedit behaves like this.  Don't
   * combine on the edge of whitespace/non-whitespace.  Also, never
   * combine if either of the characters is `special.'
   */

  switch (g_unichar_type (g_utf8_get_char (current_operation->text))) {
  case G_UNICODE_CONTROL:
  case G_UNICODE_FORMAT:
  case G_UNICODE_UNASSIGNED:
  case G_UNICODE_PRIVATE_USE:
  case G_UNICODE_SURROGATE:
  case G_UNICODE_COMBINING_MARK:
  case G_UNICODE_ENCLOSING_MARK:
  case G_UNICODE_NON_SPACING_MARK:
  case G_UNICODE_LINE_SEPARATOR:
  case G_UNICODE_PARAGRAPH_SEPARATOR:
    /* Treat tab as space separator even though it is a control
     * character in Unicode.
     */
    if (*current_operation->text != '\t')
      return FALSE;

  case G_UNICODE_SPACE_SEPARATOR:
    break;

  default:
    {
      const gchar *previous_character
	= g_utf8_prev_char (previous_operation->text
			    + strlen (previous_operation->text));

      switch (g_unichar_type (g_utf8_get_char (previous_character))) {
      case G_UNICODE_CONTROL:
      case G_UNICODE_FORMAT:
      case G_UNICODE_UNASSIGNED:
      case G_UNICODE_PRIVATE_USE:
      case G_UNICODE_SURROGATE:
      case G_UNICODE_COMBINING_MARK:
      case G_UNICODE_ENCLOSING_MARK:
      case G_UNICODE_NON_SPACING_MARK:
      case G_UNICODE_LINE_SEPARATOR:
      case G_UNICODE_PARAGRAPH_SEPARATOR:
	return FALSE;

      case G_UNICODE_SPACE_SEPARATOR:
	return FALSE;

      default:
	break;
      }
    }
  }

  if (current_operation->operation.type == OPERATION_TEXT_DELETION) {
    gchar *combined_text;

    if (current_operation->offset == previous_operation->offset) {
      /* Deleting with the Delete key. */
      combined_text = g_strconcat (previous_operation->text,
				   current_operation->text, NULL);
    }
    else if (current_operation->offset == previous_operation->offset - 1) {
      /* Deleting with the Backspace key. */
      previous_operation->offset--;
      combined_text = g_strconcat (current_operation->text,
				   previous_operation->text, NULL);
    }
    else {
      /* Distant changes, don't combine. */
      return FALSE;
    }

    g_free (previous_operation->text);
    previous_operation->text = combined_text;
  }
  else {
    gchar *combined_text;

    if (current_operation->offset
	!= (previous_operation->offset
	    + g_utf8_strlen (previous_operation->text, -1))) {
      /* Distant changes, don't combine.*/
      return FALSE;
    }

    combined_text = g_strconcat (previous_operation->text,
				 current_operation->text, NULL);
    g_free (previous_operation->text);
    previous_operation->text = combined_text;
  }

  previous_undo_entry->cursor_offset_after
    = current_undo_entry->cursor_offset_after;
  previous_undo_entry->state_index_after
    = current_undo_entry->state_index_after;

  quarry_text_buffer_undo_entry_delete (current_undo_entry);

  return TRUE;
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

  g_return_if_fail (undo_entry);

  operation = undo_entry->first;
  while (operation) {
    QuarryTextBufferOperation *next_operation = operation->next;

    if (operations[operation->type].free_data)
      operations[operation->type].free_data (operation);

    g_free (operation);

    operation = next_operation;
  }

  g_free (undo_entry);
}


inline gboolean
quarry_text_buffer_undo_entry_is_empty
  (const QuarryTextBufferUndoEntry *undo_entry)
{
  g_return_val_if_fail (undo_entry, TRUE);
  return undo_entry->first == NULL;
}



static QuarryTextBufferOperation *
add_operation (QuarryTextBuffer *buffer,
	       OperationType operation_type, gint operation_structure_size)
{
  QuarryTextBufferOperation *operation = g_malloc (operation_structure_size);

  operation->next     = NULL;
  operation->previous = buffer->current_undo_entry->last;
  operation->type     = operation_type;

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
