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


#include "gtk-thread-interface.h"


#if THREADS_SUPPORTED


#include "gtk-progress-dialog.h"
#include "gtk-control-center.h"
#include "gtk-utils.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>


static void	 gtk_progress_dialog_class_init(GtkProgressDialogClass *class);
static void	 gtk_progress_dialog_init(GtkProgressDialog *progress_dialog);

static gboolean  gtk_progress_dialog_update(GtkProgressDialog *progress_dialog);

static void	 gtk_progress_dialog_response(GtkDialog *dialog,
					      gint response_id);

static gboolean	 gtk_progress_dialog_delete_event(GtkWidget *widget,
						  GdkEventAny *event);
static void	 gtk_progress_dialog_destroy(GtkObject *object);


static GtkDialogClass  *parent_class;


GtkType
gtk_progress_dialog_get_type(void)
{
  static GtkType progress_dialog_type = 0;

  if (!progress_dialog_type) {
    static GTypeInfo progress_dialog_info = {
      sizeof(GtkProgressDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_progress_dialog_class_init,
      NULL,
      NULL,
      sizeof(GtkProgressDialog),
      1,
      (GInstanceInitFunc) gtk_progress_dialog_init,
      NULL
    };

    progress_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
						  "GtkProgressDialog",
						  &progress_dialog_info, 0);
  }

  return progress_dialog_type;
}


static void
gtk_progress_dialog_class_init(GtkProgressDialogClass *class)
{
  parent_class = g_type_class_peek_parent(class);

  GTK_OBJECT_CLASS(class)->destroy = gtk_progress_dialog_destroy;

  GTK_WIDGET_CLASS(class)->delete_event = gtk_progress_dialog_delete_event;

  GTK_DIALOG_CLASS(class)->response = gtk_progress_dialog_response;
}


static void
gtk_progress_dialog_init(GtkProgressDialog *progress_dialog)
{
  GtkWindow *window = GTK_WINDOW(progress_dialog);
  GtkWidget *label;
  GtkWidget *progress_bar;
  GtkWidget *contents;

  gtk_control_center_window_created(window);

  gtk_window_set_resizable(window, FALSE);
  gtk_window_set_skip_pager_hint(window, TRUE);

  label = gtk_label_new(NULL);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  progress_dialog->label = GTK_LABEL(label);
  gtk_widget_show(label);

  progress_bar = gtk_progress_bar_new();
  progress_dialog->progress_bar = GTK_PROGRESS_BAR(progress_bar);

  contents = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				   label, GTK_UTILS_FILL,
				   progress_bar, GTK_UTILS_FILL, NULL);
  gtk_utils_standardize_dialog(&progress_dialog->dialog, contents);
  gtk_widget_show(contents);

  gtk_dialog_set_has_separator(&progress_dialog->dialog, FALSE);

  progress_dialog->last_displayed_percentage = 0;
}


GtkWidget *
gtk_progress_dialog_new(GtkWindow *parent,
			const gchar *title, const gchar *label_text,
			GtkProgressDialogCallback update_callback,
			GtkProgressDialogCallback cancel_callback,
			gpointer user_data)
{
  GtkWidget *widget = GTK_WIDGET(g_object_new(GTK_TYPE_PROGRESS_DIALOG, NULL));
  GtkProgressDialog *progress_dialog = GTK_PROGRESS_DIALOG(widget);

  assert(!parent || GTK_IS_WINDOW(parent));

  progress_dialog->parent = GTK_WIDGET(parent);
  if (progress_dialog->parent) {
    g_object_ref(progress_dialog->parent);
    gtk_widget_set_sensitive(progress_dialog->parent, FALSE);

    g_signal_connect(progress_dialog->parent, "delete-event",
		     G_CALLBACK(gtk_true), NULL);
  }

  if (title)
    gtk_window_set_title(GTK_WINDOW(progress_dialog), title);

  if (label_text)
    gtk_label_set_text(progress_dialog->label, label_text);

  if (cancel_callback) {
    gtk_dialog_add_button(&progress_dialog->dialog,
			  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  }

  progress_dialog->update_callback = update_callback;
  progress_dialog->cancel_callback = cancel_callback;
  progress_dialog->user_data	   = user_data;

  progress_dialog->timeout_handler_id
    = g_timeout_add(200, (GSourceFunc) gtk_progress_dialog_update,
		    progress_dialog);

  return widget;
}


static gboolean
gtk_progress_dialog_update(GtkProgressDialog *progress_dialog)
{
  gboolean keep_timeout;

  if (progress_dialog->update_callback) {
    keep_timeout
      = progress_dialog->update_callback(progress_dialog,
					 progress_dialog->user_data);
  }
  else {
    if (!GTK_WIDGET_VISIBLE(progress_dialog)) {
      gtk_widget_show(GTK_WIDGET(progress_dialog));
      if (progress_dialog->parent)
	gtk_widget_hide(progress_dialog->parent);
    }

    keep_timeout = FALSE;
  }

  if (!keep_timeout)
    progress_dialog->timeout_handler_id = -1;

  return keep_timeout;
}


static void
gtk_progress_dialog_response(GtkDialog *dialog, gint response_id)
{
  GtkProgressDialog *progress_dialog = GTK_PROGRESS_DIALOG(dialog);

  UNUSED(response_id);

  if (progress_dialog->cancel_callback
      && progress_dialog->cancel_callback(progress_dialog,
					  progress_dialog->user_data))
    gtk_widget_destroy(GTK_WIDGET(dialog));
}


static gboolean
gtk_progress_dialog_delete_event(GtkWidget *widget, GdkEventAny *event)
{
  GtkProgressDialog *progress_dialog = GTK_PROGRESS_DIALOG(widget);

  UNUSED(event);

  return (progress_dialog->cancel_callback
	  && progress_dialog->cancel_callback(progress_dialog,
					      progress_dialog->user_data)
	  ? TRUE : FALSE);
}


static void
gtk_progress_dialog_destroy(GtkObject *object)
{
  GtkProgressDialog *progress_dialog = GTK_PROGRESS_DIALOG(object);

  if (gtk_control_center_window_destroyed(GTK_WINDOW(object))) {
    if (progress_dialog->parent)
      g_object_unref(progress_dialog->parent);

    if (progress_dialog->timeout_handler_id >= 0)
      g_source_remove(progress_dialog->timeout_handler_id);
  }

  GTK_OBJECT_CLASS(parent_class)->destroy(object);
}


void
gtk_progress_dialog_set_fraction (GtkProgressDialog *progress_dialog,
				  gdouble fraction, const gchar *title_part)
{
  assert(GTK_IS_PROGRESS_DIALOG(progress_dialog));

  gtk_progress_bar_set_fraction(progress_dialog->progress_bar, fraction);
  if (!GTK_WIDGET_VISIBLE(progress_dialog->progress_bar))
    gtk_widget_show(GTK_WIDGET(progress_dialog->progress_bar));

  if (title_part) {
    int percentage = floor(fraction * 100.0);

    if (percentage != progress_dialog->last_displayed_percentage) {
      gchar *full_title;

      full_title = g_strdup_printf("[%d%%]%s%s", percentage,
				   *title_part ? " " : "", title_part);
      gtk_window_set_title(GTK_WINDOW(progress_dialog), full_title);
      g_free(full_title);

      progress_dialog->last_displayed_percentage = percentage;
    }
  }
}


void
gtk_progress_dialog_recover_parent(GtkProgressDialog *progress_dialog)
{
  assert(GTK_IS_PROGRESS_DIALOG(progress_dialog));

  if (progress_dialog->parent) {
    g_signal_handlers_disconnect_by_func(progress_dialog->parent,
					 gtk_true, NULL);
    gtk_widget_set_sensitive(progress_dialog->parent, TRUE);
    gtk_widget_show(progress_dialog->parent);
  }
}


#endif /* THREADS_SUPPORTED */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
