/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005 Paul Pogonyshev.                             *
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


#ifndef QUARRY_QUARRY_SAVE_CONFIRMATION_DIALOG_H
#define QUARRY_QUARRY_SAVE_CONFIRMATION_DIALOG_H


#include "quarry-message-dialog.h"
#include "quarry.h"

#include <gtk/gtk.h>
#include <stdarg.h>


#define QUARRY_TYPE_SAVE_CONFIRMATION_DIALOG				\
  (quarry_save_confirmation_dialog_get_type ())

#define QUARRY_SAVE_CONFIRMATION_DIALOG(object)				\
  GTK_CHECK_CAST ((object), QUARRY_TYPE_SAVE_CONFIRMATION_DIALOG,	\
		  QuarrySaveConfirmationDialog)

#define QUARRY_SAVE_CONFIRMATION_DIALOG_CLASS(class)			\
  GTK_CHECK_CLASS_CAST ((class), QUARRY_TYPE_SAVE_CONFIRMATION_DIALOG,	\
			QuarrySaveConfirmationDialogClass)

#define QUARRY_IS_SAVE_CONFIRMATION_DIALOG(object)			\
  GTK_CHECK_TYPE ((object), QUARRY_TYPE_SAVE_CONFIRMATION_DIALOG)

#define QUARRY_IS_SAVE_CONFIRMATION_DIALOG_CLASS(class)			\
  GTK_CHECK_CLASS_TYPE ((class), QUARRY_TYPE_SAVE_CONFIRMATION_DIALOG)

#define QUARRY_SAVE_CONFIRMATION_DIALOG_GET_CLASS(object)		\
  GTK_CHECK_GET_CLASS ((object), QUARRY_TYPE_SAVE_CONFIRMATION_DIALOG,	\
		       QuarrySaveConfirmationDialogClass)


typedef struct _QuarrySaveConfirmationDialog
		QuarrySaveConfirmationDialog;
typedef struct _QuarrySaveConfirmationDialogClass
		QuarrySaveConfirmationDialogClass;

struct _QuarrySaveConfirmationDialog {
  QuarryMessageDialog	     dialog;

  glong			     time_of_first_modification;
  guint			     timeout_source_id;
};

struct _QuarrySaveConfirmationDialogClass {
  QuarryMessageDialogClass   parent_class;
};


GType		quarry_save_confirmation_dialog_get_type (void);

GtkWidget *	quarry_save_confirmation_dialog_new
		  (GtkWindow *parent, glong time_of_first_modification,
		   const gchar *primary_text_format_string, ...);
GtkWidget *	quarry_save_confirmation_dialog_new_valist
		  (GtkWindow *parent, glong time_of_first_modification,
		   const gchar *primary_text_format_string, va_list arguments);


#endif /* QUARRY_QUARRY_SAVE_CONFIRMATION_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
