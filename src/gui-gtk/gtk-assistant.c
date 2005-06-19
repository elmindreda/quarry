/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005 Paul Pogonyshev.                 *
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


#include "gtk-assistant.h"
#include "gtk-control-center.h"
#include "gtk-help.h"
#include "gtk-utils.h"
#include "quarry-stock.h"

#include <gtk/gtk.h>
#include <assert.h>


#define ASSISTANT_RESPONSE_NEXT	1
#define ASSISTANT_RESPONSE_BACK	2


typedef struct _GtkAssistantPage	GtkAssistantPage;

struct _GtkAssistantPage {
  GtkAssistantPageShownCallback	       shown_callback;
  GtkAssistantPageAcceptableCallback   acceptable_callback;

  const gchar			      *help_link_id;
  GtkAssistantPageHelpLinkIDCallback   help_link_id_callback;
};


static void	gtk_assistant_class_init (GtkAssistantClass *class);
static void	gtk_assistant_init (GtkAssistant *assistant);

static void	gtk_assistant_update_buttons (GtkAssistant *assistant);
static void	gtk_assistant_response (GtkDialog *dialog, gint response_id);

static void	gtk_assistant_destroy (GtkObject *object);


static GtkDialogClass  *parent_class;

static GQuark		assistant_page_quark;


GType
gtk_assistant_get_type (void)
{
  static GType assistant_type = 0;

  if (!assistant_type) {
    static GTypeInfo assistant_info = {
      sizeof (GtkAssistantClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_assistant_class_init,
      NULL,
      NULL,
      sizeof (GtkAssistant),
      1,
      (GInstanceInitFunc) gtk_assistant_init,
      NULL
    };

    assistant_type = g_type_register_static (GTK_TYPE_DIALOG, "GtkAssistant",
					     &assistant_info, 0);
  }

  return assistant_type;
}


static void
gtk_assistant_class_init (GtkAssistantClass *class)
{
  parent_class = g_type_class_peek_parent (class);

  GTK_OBJECT_CLASS (class)->destroy = gtk_assistant_destroy;

  GTK_DIALOG_CLASS (class)->response = gtk_assistant_response;

  assistant_page_quark = g_quark_from_static_string ("quarry-assistant-page");
}


static void
gtk_assistant_init (GtkAssistant *assistant)
{
  GtkWidget *notebook;

  gtk_control_center_window_created (GTK_WINDOW (assistant));

  notebook = gtk_utils_create_invisible_notebook ();
  assistant->notebook = GTK_NOTEBOOK (notebook);
  gtk_widget_show (notebook);

  gtk_utils_standardize_dialog (&assistant->dialog, notebook);

  g_signal_connect_data (notebook, "switch-page",
			 G_CALLBACK (gtk_assistant_update_buttons), assistant,
			 NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  gtk_dialog_add_button (&assistant->dialog,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  assistant->back_button   = gtk_dialog_add_button (&assistant->dialog,
						    GTK_STOCK_GO_BACK,
						    ASSISTANT_RESPONSE_BACK);
  assistant->next_button   = gtk_dialog_add_button (&assistant->dialog,
						    QUARRY_STOCK_NEXT,
						    ASSISTANT_RESPONSE_NEXT);
  assistant->finish_button = gtk_dialog_add_button (&assistant->dialog,
						    GTK_STOCK_OK,
						    GTK_RESPONSE_OK);
  assistant->help_button   = NULL;

  assistant->user_data = NULL;
}


GtkWidget *
gtk_assistant_new (const char *title, gpointer user_data)
{
  GtkWidget *widget = GTK_WIDGET (g_object_new (GTK_TYPE_ASSISTANT, NULL));

  gtk_window_set_title (GTK_WINDOW (widget), title);

  GTK_ASSISTANT (widget)->user_data = user_data;

  return widget;
}


void
gtk_assistant_set_user_data (GtkAssistant *assistant, gpointer user_data)
{
  assert (GTK_IS_ASSISTANT (assistant));

  assistant->user_data = user_data;
}


static void
gtk_assistant_update_buttons (GtkAssistant *assistant)
{
  gint current_page_index
    = gtk_notebook_get_current_page (assistant->notebook);

  if (current_page_index >= 0) {
    int is_last_page;

    /* Workaround GTK+ 2.0 problem.  Causes annoying warning without
     * this hack in certain cases.
     */
#if !GTK_2_2_OR_LATER
    GtkWidget *current_page = gtk_notebook_get_nth_page (assistant->notebook,
							 current_page_index);
    GtkWidget *window = gtk_widget_get_toplevel (current_page);

    if (!window || !GTK_WIDGET_TOPLEVEL (window))
      return;
#endif

    /* This is not very important for Quarry, but pre-2.2 determining
     * if the page is last is somewhat hackish.  Therefore I think it
     * is better to #if here.
     */
#if GTK_2_2_OR_LATER
    is_last_page = (current_page_index
		    == gtk_notebook_get_n_pages (assistant->notebook) - 1);
#else
    is_last_page = (gtk_notebook_get_nth_page (assistant->notebook,
					       current_page_index + 1)
		    == NULL);
#endif

    if (current_page_index > 0)
      gtk_widget_show (assistant->back_button);
    else
      gtk_widget_hide (assistant->back_button);

    if (!is_last_page) {
      gtk_widget_show (assistant->next_button);
      gtk_widget_hide (assistant->finish_button);

      gtk_dialog_set_default_response (&assistant->dialog,
				       ASSISTANT_RESPONSE_NEXT);
    }
    else {
      gtk_widget_hide (assistant->next_button);
      gtk_widget_show (assistant->finish_button);

      gtk_dialog_set_default_response (&assistant->dialog, GTK_RESPONSE_OK);
    }

    if (assistant->help_button) {
      GtkWidget *current_page = gtk_notebook_get_nth_page (assistant->notebook,
							   current_page_index);
      GtkAssistantPage *page_data
	= g_object_get_qdata (G_OBJECT (current_page), assistant_page_quark);

      gtk_widget_set_sensitive (assistant->help_button,
				(page_data->help_link_id != NULL
				 || page_data->help_link_id_callback != NULL));
    }
  }
  else {
    gtk_widget_hide (assistant->back_button);
    gtk_widget_hide (assistant->next_button);
    gtk_widget_hide (assistant->finish_button);

    if (assistant->help_button)
      gtk_widget_set_sensitive (assistant->help_button, FALSE);
  }
}


static void
gtk_assistant_response (GtkDialog *dialog, gint response_id)
{
  GtkAssistant *assistant = GTK_ASSISTANT (dialog);
  GtkNotebook *notebook = assistant->notebook;
  gint current_page_index;
  GtkWidget *current_page;
  GtkAssistantPage *page_data;

  if (response_id == GTK_RESPONSE_HELP) {
    current_page_index = gtk_notebook_get_current_page (notebook);
    current_page = gtk_notebook_get_nth_page (notebook, current_page_index);
    page_data = g_object_get_qdata (G_OBJECT (current_page),
				    assistant_page_quark);

    assert (page_data->help_link_id || page_data->help_link_id_callback);

    gtk_help_display (page_data->help_link_id
		      ? page_data->help_link_id
		      : (page_data->help_link_id_callback
			 (assistant->user_data)));
  }

  /* Assistant pages might contain spin buttons. */
  gtk_utils_workaround_focus_bug (GTK_WINDOW (assistant));

  if (response_id == ASSISTANT_RESPONSE_NEXT
      || response_id == GTK_RESPONSE_OK) {
    current_page_index = gtk_notebook_get_current_page (notebook);
    current_page = gtk_notebook_get_nth_page (notebook, current_page_index);
    page_data = g_object_get_qdata (G_OBJECT (current_page),
				    assistant_page_quark);

    if (page_data
	&& page_data->acceptable_callback
	&& !page_data->acceptable_callback (assistant->user_data))
      return;
  }

  if (response_id == ASSISTANT_RESPONSE_NEXT
      || response_id == ASSISTANT_RESPONSE_BACK) {
    if (response_id == ASSISTANT_RESPONSE_NEXT)
      gtk_notebook_next_page (notebook);
    else
      gtk_notebook_prev_page (notebook);

    current_page_index = gtk_notebook_get_current_page (notebook);
    current_page = gtk_notebook_get_nth_page (notebook, current_page_index);
    page_data = g_object_get_qdata (G_OBJECT (current_page),
				    assistant_page_quark);

    if (page_data && page_data->shown_callback)
      page_data->shown_callback (assistant->user_data);
  }

  if (response_id ==  GTK_RESPONSE_OK || response_id == GTK_RESPONSE_CANCEL)
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void
gtk_assistant_destroy (GtkObject *object)
{
  gtk_control_center_window_destroyed (GTK_WINDOW (object));

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}



void
gtk_assistant_add_page (GtkAssistant *assistant, GtkWidget *widget,
			const gchar *icon_stock_id, const gchar *title,
			GtkAssistantPageShownCallback shown_callback,
			GtkAssistantPageAcceptableCallback acceptable_callback)
{
  GtkWidget *page = widget;
  GtkAssistantPage *page_data = g_malloc (sizeof (GtkAssistantPage));

  assert (GTK_IS_ASSISTANT (assistant));
  assert (GTK_IS_WIDGET (widget));

  if (icon_stock_id || title)
    page = gtk_utils_create_titled_page (widget, icon_stock_id, title);

  gtk_notebook_append_page (assistant->notebook, page, NULL);
  gtk_widget_show (page);
  gtk_assistant_update_buttons (assistant);

  page_data->shown_callback	   = shown_callback;
  page_data->acceptable_callback   = acceptable_callback;
  page_data->help_link_id	   = NULL;
  page_data->help_link_id_callback = NULL;

  g_object_set_qdata_full (G_OBJECT (page), assistant_page_quark,
			   page_data, g_free);
}


void
gtk_assistant_set_page_help_link_id (GtkAssistant *assistant, GtkWidget *page,
				     const gchar *help_link_id)
{
  GtkAssistantPage *page_data;

  assert (GTK_IS_ASSISTANT (assistant));
  assert (GTK_IS_WIDGET (page));
  assert (help_link_id);

  page_data = g_object_get_qdata (G_OBJECT (page), assistant_page_quark);
  if (!page_data) {
    page_data = g_object_get_qdata (G_OBJECT (gtk_widget_get_parent
					      (GTK_WIDGET (page))),
				    assistant_page_quark);
    assert (page_data);
  }

  page_data->help_link_id = help_link_id;

  if (!assistant->help_button) {
    assistant->help_button = gtk_dialog_add_button (&assistant->dialog,
						    GTK_STOCK_HELP,
						    GTK_RESPONSE_HELP);
  }
}


void
gtk_assistant_set_page_help_link_id_callback
  (GtkAssistant *assistant, GtkWidget *page,
   GtkAssistantPageHelpLinkIDCallback callback)
{
  GtkAssistantPage *page_data;

  assert (GTK_IS_ASSISTANT (assistant));
  assert (GTK_IS_WIDGET (page));
  assert (callback);

  page_data = g_object_get_qdata (G_OBJECT (page), assistant_page_quark);
  if (!page_data) {
    page_data = g_object_get_qdata (G_OBJECT (gtk_widget_get_parent
					      (GTK_WIDGET (page))),
				    assistant_page_quark);
    assert (page_data);
  }

  page_data->help_link_id_callback = callback;

  if (!assistant->help_button) {
    assistant->help_button = gtk_dialog_add_button (&assistant->dialog,
						    GTK_STOCK_HELP,
						    GTK_RESPONSE_HELP);
  }
}


void
gtk_assistant_set_finish_button (GtkAssistant *assistant,
				 const gchar *stock_id)
{
  assert (GTK_IS_ASSISTANT (assistant));
  assert (stock_id);

  gtk_widget_destroy (assistant->finish_button);

  assistant->finish_button = gtk_dialog_add_button (&assistant->dialog,
						    stock_id, GTK_RESPONSE_OK);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
