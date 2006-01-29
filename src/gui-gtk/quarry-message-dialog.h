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


#ifndef QUARRY_QUARRY_MESSAGE_DIALOG_H
#define QUARRY_QUARRY_MESSAGE_DIALOG_H


#include "quarry.h"

#include <gtk/gtk.h>
#include <stdarg.h>


#define QUARRY_TYPE_MESSAGE_DIALOG					\
  (quarry_message_dialog_get_type ())

#define QUARRY_MESSAGE_DIALOG(object)					\
  GTK_CHECK_CAST ((object), QUARRY_TYPE_MESSAGE_DIALOG,			\
		  QuarryMessageDialog)

#define QUARRY_MESSAGE_DIALOG_CLASS(class)				\
  GTK_CHECK_CLASS_CAST ((class), QUARRY_TYPE_MESSAGE_DIALOG,		\
			QuarryMessageDialogClass)

#define QUARRY_IS_MESSAGE_DIALOG(object)				\
  GTK_CHECK_TYPE ((object), QUARRY_TYPE_MESSAGE_DIALOG)

#define QUARRY_IS_MESSAGE_DIALOG_CLASS(class)				\
  GTK_CHECK_CLASS_TYPE ((class), QUARRY_TYPE_MESSAGE_DIALOG)

#define QUARRY_MESSAGE_DIALOG_GET_CLASS(object)				\
  GTK_CHECK_GET_CLASS ((object), QUARRY_TYPE_MESSAGE_DIALOG,		\
		       QuarryMessageDialogClass)


typedef struct _QuarryMessageDialog		QuarryMessageDialog;
typedef struct _QuarryMessageDialogClass	QuarryMessageDialogClass;

struct _QuarryMessageDialog {
  GtkDialog	   dialog;

  GtkWidget	  *image;
  GtkLabel	  *primary_text_label;
  GtkLabel	  *secondary_text_label;
};

struct _QuarryMessageDialogClass {
  GtkDialogClass   parent_class;
};


GType		quarry_message_dialog_get_type (void);

GtkWidget *	quarry_message_dialog_new
		  (GtkWindow *parent,
		   GtkButtonsType buttons, const gchar *icon_stock_id,
		   const gchar *secondary_text,
		   const gchar *primary_text_format_string, ...);
GtkWidget *	quarry_message_dialog_new_valist
		  (GtkWindow *parent,
		   GtkButtonsType buttons, const gchar *icon_stock_id,
		   const gchar *secondary_text,
		   const gchar *primary_text_format_string, va_list arguments);


void		quarry_message_dialog_set_icon (QuarryMessageDialog *dialog,
						const gchar *icon_stock_id);

void		quarry_message_dialog_set_primary_text
		  (QuarryMessageDialog *dialog, const gchar *primary_text);
void		quarry_message_dialog_format_primary_text
		  (QuarryMessageDialog *dialog,
		   const gchar *primary_text_format_string, ...);
void		quarry_message_dialog_format_primary_text_valist
		  (QuarryMessageDialog *dialog,
		   const gchar *primary_text_format_string, va_list arguments);

void		quarry_message_dialog_set_secondary_text
		  (QuarryMessageDialog *dialog, const gchar *secondary_text);
void		quarry_message_dialog_format_secondary_text
		  (QuarryMessageDialog *dialog,
		   const gchar *secondary_text_format_string, ...);
void		quarry_message_dialog_format_secondary_text_valist
		  (QuarryMessageDialog *dialog,
		   const gchar *secondary_text_format_string,
		   va_list arguments);


#endif /* QUARRY_QUARRY_MESSAGE_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
