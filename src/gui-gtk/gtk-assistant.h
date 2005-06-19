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
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,     *
 * Boston, MA 02110-1301, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef QUARRY_GTK_ASSISTANT_H
#define QUARRY_GTK_ASSISTANT_H


#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_ASSISTANT	(gtk_assistant_get_type ())
#define GTK_ASSISTANT(obj)						\
  GTK_CHECK_CAST ((obj), GTK_TYPE_ASSISTANT, GtkAssistant)
#define GTK_ASSISTANT_CLASS(klass)					\
  GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_ASSISTANT,			\
			GtkAssistantClass)

#define GTK_IS_ASSISTANT(obj)						\
  GTK_CHECK_TYPE ((obj), GTK_TYPE_ASSISTANT)
#define GTK_IS_ASSISTANT_CLASS(klass)					\
  GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ASSISTANT)

#define GTK_ASSISTANT_GET_CLASS(obj)					\
  GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_ASSISTANT, GtkAssistantClass)


typedef struct _GtkAssistant		GtkAssistant;
typedef struct _GtkAssistantClass	GtkAssistantClass;

struct _GtkAssistant {
  GtkDialog	   dialog;

  GtkNotebook	  *notebook;
  GtkWidget	  *back_button;
  GtkWidget	  *next_button;
  GtkWidget	  *finish_button;
  GtkWidget	  *help_button;

  gpointer	   user_data;
};

struct _GtkAssistantClass {
  GtkDialogClass   parent_class;
};


typedef void (* GtkAssistantPageShownCallback) (gpointer user_data);
typedef gboolean (* GtkAssistantPageAcceptableCallback) (gpointer user_data);
typedef const gchar * (* GtkAssistantPageHelpLinkIDCallback)
  (gpointer user_data);


GType		gtk_assistant_get_type (void);

GtkWidget *	gtk_assistant_new (const gchar *title, gpointer user_data);

void		gtk_assistant_set_user_data (GtkAssistant *assistant,
					     gpointer user_data);

void		gtk_assistant_add_page
		  (GtkAssistant *assistant, GtkWidget *widget,
		   const gchar *icon_stock_id, const gchar *title,
		   GtkAssistantPageShownCallback shown_callback,
		   GtkAssistantPageAcceptableCallback acceptable_callback);

void		gtk_assistant_set_page_help_link_id
		  (GtkAssistant *assistant, GtkWidget *page,
		   const gchar *help_link_id);
void		gtk_assistant_set_page_help_link_id_callback
		  (GtkAssistant *assistant, GtkWidget *page,
		   GtkAssistantPageHelpLinkIDCallback callback);

void		gtk_assistant_set_finish_button (GtkAssistant *assistant,
						 const gchar *stock_id);


#endif /* QUARRY_GTK_ASSISTANT_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
