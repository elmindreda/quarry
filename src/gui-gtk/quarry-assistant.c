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


#include "quarry-assistant.h"

#include "gtk-control-center.h"
#include "gtk-help.h"
#include "gtk-utils.h"
#include "quarry-stock.h"

#include <gtk/gtk.h>


#define ASSISTANT_RESPONSE_NEXT	1
#define ASSISTANT_RESPONSE_BACK	2


typedef struct _QuarryAssistantPage	QuarryAssistantPage;

struct _QuarryAssistantPage {
  QuarryAssistantPageShownCallback	  shown_callback;
  QuarryAssistantPageAcceptableCallback   acceptable_callback;

  const gchar				 *help_link_id;
  QuarryAssistantPageHelpLinkIDCallback   help_link_id_callback;
};


static void	quarry_assistant_class_init (QuarryAssistantClass *class);
static void	quarry_assistant_init (QuarryAssistant *assistant);

static void	quarry_assistant_update_buttons (QuarryAssistant *assistant);
static void	quarry_assistant_response (GtkDialog *dialog,
					   gint response_id);

static void	quarry_assistant_destroy (GtkObject *object);


static GtkDialogClass  *parent_class;

static GQuark		assistant_page_quark;


GType
quarry_assistant_get_type (void)
{
  static GType assistant_type = 0;

  if (!assistant_type) {
    static GTypeInfo assistant_info = {
      sizeof (QuarryAssistantClass),
      NULL,
      NULL,
      (GClassInitFunc) quarry_assistant_class_init,
      NULL,
      NULL,
      sizeof (QuarryAssistant),
      1,
      (GInstanceInitFunc) quarry_assistant_init,
      NULL
    };

    assistant_type = g_type_register_static (GTK_TYPE_DIALOG,
					     "QuarryAssistant",
					     &assistant_info, 0);
  }

  return assistant_type;
}


static void
quarry_assistant_class_init (QuarryAssistantClass *class)
{
  parent_class = g_type_class_peek_parent (class);

  GTK_OBJECT_CLASS (class)->destroy  = quarry_assistant_destroy;

  GTK_DIALOG_CLASS (class)->response = quarry_assistant_response;

  assistant_page_quark = g_quark_from_static_string ("quarry-assistant-page");
}


static void
quarry_assistant_init (QuarryAssistant *assistant)
{
  GtkWidget *notebook;

  gtk_control_center_window_created (GTK_WINDOW (assistant));

  notebook = gtk_utils_create_invisible_notebook ();
  assistant->notebook = GTK_NOTEBOOK (notebook);
  gtk_widget_show (notebook);

  gtk_utils_standardize_dialog (&assistant->dialog, notebook);

  g_signal_connect_data (notebook, "switch-page",
			 G_CALLBACK (quarry_assistant_update_buttons),
			 assistant,
			 NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  gtk_dialog_add_button (&assistant->dialog,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  assistant->back_button   = gtk_dialog_add_button (&assistant->dialog,
						    GTK_STOCK_GO_BACK,
						    ASSISTANT_RESPONSE_BACK);
  assistant->next_button   = gtk_dialog_add_button (&assistant->dialog,
						    GTK_STOCK_GO_FORWARD,
						    ASSISTANT_RESPONSE_NEXT);
  assistant->finish_button = gtk_dialog_add_button (&assistant->dialog,
						    GTK_STOCK_OK,
						    GTK_RESPONSE_OK);
  assistant->help_button   = NULL;

  assistant->user_data = NULL;
}


GtkWidget *
quarry_assistant_new (const char *title, gpointer user_data)
{
  GtkWidget *widget = GTK_WIDGET (g_object_new (QUARRY_TYPE_ASSISTANT, NULL));

  gtk_window_set_title (GTK_WINDOW (widget), title);

  QUARRY_ASSISTANT (widget)->user_data = user_data;

  return widget;
}


void
quarry_assistant_set_user_data (QuarryAssistant *assistant, gpointer user_data)
{
  g_return_if_fail (QUARRY_IS_ASSISTANT (assistant));

  assistant->user_data = user_data;
}


static void
quarry_assistant_update_buttons (QuarryAssistant *assistant)
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
      QuarryAssistantPage *page_data
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
quarry_assistant_response (GtkDialog *dialog, gint response_id)
{
  QuarryAssistant *assistant = QUARRY_ASSISTANT (dialog);
  GtkNotebook *notebook = assistant->notebook;
  gint current_page_index;
  GtkWidget *current_page;
  QuarryAssistantPage *page_data;

  if (response_id == GTK_RESPONSE_HELP) {
    current_page_index = gtk_notebook_get_current_page (notebook);
    current_page = gtk_notebook_get_nth_page (notebook, current_page_index);
    page_data = g_object_get_qdata (G_OBJECT (current_page),
				    assistant_page_quark);

    g_assert (page_data->help_link_id || page_data->help_link_id_callback);

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
quarry_assistant_destroy (GtkObject *object)
{
  gtk_control_center_window_destroyed (GTK_WINDOW (object));

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}



void
quarry_assistant_add_page
  (QuarryAssistant *assistant, GtkWidget *widget,
   const gchar *icon_stock_id, const gchar *title,
   QuarryAssistantPageShownCallback shown_callback,
   QuarryAssistantPageAcceptableCallback acceptable_callback)
{
  GtkWidget *page = widget;
  QuarryAssistantPage *page_data = g_malloc (sizeof (QuarryAssistantPage));

  g_return_if_fail (QUARRY_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (icon_stock_id || title)
    page = gtk_utils_create_titled_page (widget, icon_stock_id, title);

  gtk_notebook_append_page (assistant->notebook, page, NULL);
  gtk_widget_show (page);
  quarry_assistant_update_buttons (assistant);

  page_data->shown_callback	   = shown_callback;
  page_data->acceptable_callback   = acceptable_callback;
  page_data->help_link_id	   = NULL;
  page_data->help_link_id_callback = NULL;

  g_object_set_qdata_full (G_OBJECT (page), assistant_page_quark,
			   page_data, g_free);
}


void
quarry_assistant_set_page_help_link_id (QuarryAssistant *assistant,
					GtkWidget *page,
					const gchar *help_link_id)
{
  QuarryAssistantPage *page_data;

  g_return_if_fail (QUARRY_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (help_link_id);

  page_data = g_object_get_qdata (G_OBJECT (page), assistant_page_quark);
  if (!page_data) {
    page_data = g_object_get_qdata (G_OBJECT (gtk_widget_get_parent
					      (GTK_WIDGET (page))),
				    assistant_page_quark);
    g_assert (page_data);
  }

  page_data->help_link_id = help_link_id;

  if (!assistant->help_button) {
    assistant->help_button = gtk_dialog_add_button (&assistant->dialog,
						    GTK_STOCK_HELP,
						    GTK_RESPONSE_HELP);
  }
}


void
quarry_assistant_set_page_help_link_id_callback
  (QuarryAssistant *assistant, GtkWidget *page,
   QuarryAssistantPageHelpLinkIDCallback callback)
{
  QuarryAssistantPage *page_data;

  g_return_if_fail (QUARRY_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (callback);

  page_data = g_object_get_qdata (G_OBJECT (page), assistant_page_quark);
  if (!page_data) {
    page_data = g_object_get_qdata (G_OBJECT (gtk_widget_get_parent
					      (GTK_WIDGET (page))),
				    assistant_page_quark);
    g_assert (page_data);
  }

  page_data->help_link_id_callback = callback;

  if (!assistant->help_button) {
    assistant->help_button = gtk_dialog_add_button (&assistant->dialog,
						    GTK_STOCK_HELP,
						    GTK_RESPONSE_HELP);
  }
}


void
quarry_assistant_set_finish_button (QuarryAssistant *assistant,
				    const gchar *stock_id)
{
  g_return_if_fail (QUARRY_IS_ASSISTANT (assistant));
  g_return_if_fail (stock_id);

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
