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


#include "quarry-text-view.h"

#include "gtk-utils.h"
#include "quarry-history-text-buffer.h"
#include "quarry-marshal.h"

#include <assert.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>


static void	quarry_text_view_class_init (QuarryTextViewClass *class);

static void	quarry_text_view_populate_popup (GtkTextView *text_view,
						 GtkMenu *menu);

static void	quarry_text_view_undo_real (QuarryTextView *view);
static void	quarry_text_view_redo_real (QuarryTextView *view);


static GtkTextViewClass  *parent_class;


enum {
  UNDO,
  REDO,
  NUM_SIGNALS
};

static guint		  text_view_signals[NUM_SIGNALS];


GType
quarry_text_view_get_type (void)
{
  static GType quarry_text_view_type = 0;

  if (!quarry_text_view_type) {
    static const GTypeInfo quarry_text_view_info = {
      sizeof (QuarryTextViewClass),
      NULL,
      NULL,
      (GClassInitFunc) quarry_text_view_class_init,
      NULL,
      NULL,
      sizeof (QuarryTextView),
      0,
      NULL,
      NULL
    };

    quarry_text_view_type = g_type_register_static (GTK_TYPE_TEXT_VIEW,
						    "QuarryTextView",
						    &quarry_text_view_info, 0);
  }

  return quarry_text_view_type;
}


static void
quarry_text_view_class_init (QuarryTextViewClass *class)
{
  GtkBindingSet *binding_set;

  parent_class = g_type_class_peek_parent (class);

  GTK_TEXT_VIEW_CLASS (class)->populate_popup
    = quarry_text_view_populate_popup;

  class->undo = quarry_text_view_undo_real;
  class->redo = quarry_text_view_redo_real;

  text_view_signals[UNDO]
    = g_signal_new ("undo",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		    G_STRUCT_OFFSET (QuarryTextViewClass, undo),
		    NULL, NULL,
		    quarry_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);

  text_view_signals[REDO]
    = g_signal_new ("redo",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		    G_STRUCT_OFFSET (QuarryTextViewClass, redo),
		    NULL, NULL,
		    quarry_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (class);

  gtk_binding_entry_add_signal (binding_set, GDK_z, GDK_CONTROL_MASK,
				"undo", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_z, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				"redo", 0);
}


GtkWidget *
quarry_text_view_new (void)
{
  return quarry_text_view_new_with_buffer (quarry_history_text_buffer_new
					   (NULL));
}


GtkWidget *
quarry_text_view_new_with_buffer (GtkTextBuffer *buffer)
{
  GtkWidget *widget = GTK_WIDGET (g_object_new (QUARRY_TYPE_TEXT_VIEW, NULL));

  gtk_text_view_set_buffer (GTK_TEXT_VIEW (widget), buffer);

  return widget;
}


static void
quarry_text_view_populate_popup (GtkTextView *text_view, GtkMenu *menu)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (text_view);

  if (QUARRY_IS_HISTORY_TEXT_BUFFER (buffer)) {
    QuarryHistoryTextBuffer *history_buffer
      = QUARRY_HISTORY_TEXT_BUFFER (buffer);
    GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);
    GtkWidget *menu_item;

    /* `Undo' and `Redo' menu items normally go first, so we are saved
     * from heuristics for finding a place for them.
     */

    menu_item = gtk_separator_menu_item_new ();
    gtk_widget_show (menu_item);
    gtk_menu_shell_prepend (menu_shell, menu_item);

    menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_REDO, NULL);
    gtk_widget_set_sensitive (menu_item,
			      (quarry_history_text_buffer_can_redo
			       (history_buffer)));
    gtk_widget_show (menu_item);
    gtk_menu_shell_prepend (menu_shell, menu_item);

    g_signal_connect_swapped (menu_item, "activate",
			      G_CALLBACK (quarry_text_view_redo), text_view);

    menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_UNDO, NULL);
    gtk_widget_set_sensitive (menu_item,
			      (quarry_history_text_buffer_can_undo
			       (history_buffer)));
    gtk_widget_show (menu_item);
    gtk_menu_shell_prepend (menu_shell, menu_item);

    g_signal_connect_swapped (menu_item, "activate",
			      G_CALLBACK (quarry_text_view_undo), text_view);
  }

  if (parent_class->populate_popup)
    parent_class->populate_popup (text_view, menu);
}


void
quarry_text_view_undo (QuarryTextView *view)
{
  assert (QUARRY_IS_TEXT_VIEW (view));

  g_signal_emit (view, text_view_signals[UNDO], 0);
}


void
quarry_text_view_redo (QuarryTextView *view)
{
  assert (QUARRY_IS_TEXT_VIEW (view));

  g_signal_emit (view, text_view_signals[REDO], 0);
}


static void
quarry_text_view_undo_real (QuarryTextView *view)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

  if (QUARRY_IS_HISTORY_TEXT_BUFFER (buffer)) {
    QuarryHistoryTextBuffer *history_buffer
      = QUARRY_HISTORY_TEXT_BUFFER (buffer);

    if (quarry_history_text_buffer_can_undo (history_buffer))
      quarry_history_text_buffer_undo (history_buffer);
  }
}


static void
quarry_text_view_redo_real (QuarryTextView *view)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

  if (QUARRY_IS_HISTORY_TEXT_BUFFER (buffer)) {
    QuarryHistoryTextBuffer *history_buffer
      = QUARRY_HISTORY_TEXT_BUFFER (buffer);

    if (quarry_history_text_buffer_can_redo (history_buffer))
      quarry_history_text_buffer_redo (history_buffer);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
