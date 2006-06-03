/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2006 Paul Pogonyshev.                 *
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


#ifndef QUARRY_QUARRY_ASSISTANT_H
#define QUARRY_QUARRY_ASSISTANT_H


#include "quarry.h"

#include <gtk/gtk.h>


#define QUARRY_TYPE_ASSISTANT	(quarry_assistant_get_type ())
#define QUARRY_ASSISTANT(object)					\
  GTK_CHECK_CAST ((object), QUARRY_TYPE_ASSISTANT, QuarryAssistant)
#define QUARRY_ASSISTANT_CLASS(class)					\
  GTK_CHECK_CLASS_CAST ((class), QUARRY_TYPE_ASSISTANT,			\
			QuarryAssistantClass)

#define QUARRY_IS_ASSISTANT(object)					\
  GTK_CHECK_TYPE ((object), QUARRY_TYPE_ASSISTANT)
#define QUARRY_IS_ASSISTANT_CLASS(class)				\
  GTK_CHECK_CLASS_TYPE ((class), QUARRY_TYPE_ASSISTANT)

#define QUARRY_ASSISTANT_GET_CLASS(object)				\
  GTK_CHECK_GET_CLASS ((object), QUARRY_TYPE_ASSISTANT,			\
		       QuarryAssistantClass)


typedef struct _QuarryAssistant		QuarryAssistant;
typedef struct _QuarryAssistantClass	QuarryAssistantClass;

struct _QuarryAssistant {
  GtkDialog	   dialog;

  GtkNotebook	  *notebook;
  GtkWidget	  *back_button;
  GtkWidget	  *next_button;
  GtkWidget	  *finish_button;
  GtkWidget	  *help_button;

  gpointer	   user_data;
};

struct _QuarryAssistantClass {
  GtkDialogClass   parent_class;
};


typedef void (* QuarryAssistantPageShownCallback) (gpointer user_data);
typedef gboolean (* QuarryAssistantPageAcceptableCallback)
		   (gpointer user_data);
typedef const gchar * (* QuarryAssistantPageHelpLinkIDCallback)
			(gpointer user_data);


GType		quarry_assistant_get_type (void);

GtkWidget *	quarry_assistant_new (const gchar *title, gpointer user_data);

void		quarry_assistant_set_user_data (QuarryAssistant *assistant,
						gpointer user_data);

void		quarry_assistant_add_page
		  (QuarryAssistant *assistant, GtkWidget *widget,
		   const gchar *icon_stock_id, const gchar *title,
		   QuarryAssistantPageShownCallback shown_callback,
		   QuarryAssistantPageAcceptableCallback acceptable_callback);

void		quarry_assistant_set_page_help_link_id
		  (QuarryAssistant *assistant, GtkWidget *page,
		   const gchar *help_link_id);
void		quarry_assistant_set_page_help_link_id_callback
		  (QuarryAssistant *assistant, GtkWidget *page,
		   QuarryAssistantPageHelpLinkIDCallback callback);

void		quarry_assistant_set_finish_button (QuarryAssistant *assistant,
						    const gchar *stock_id);


#endif /* QUARRY_QUARRY_ASSISTANT_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
