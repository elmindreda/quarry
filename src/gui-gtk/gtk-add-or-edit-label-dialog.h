/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005 Paul Pogonyshev                              *
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


#ifndef QUARRY_GTK_ADD_OR_EDIT_LABEL_DIALOG_H
#define QUARRY_GTK_ADD_OR_EDIT_LABEL_DIALOG_H


#include <gtk/gtk.h>


#define GTK_TYPE_ADD_OR_EDIT_LABEL_DIALOG				\
  (gtk_add_or_edit_label_dialog_get_type ())

#define GTK_ADD_OR_EDIT_LABEL_DIALOG(object)				\
  GTK_CHECK_CAST ((object), GTK_TYPE_ADD_OR_EDIT_LABEL_DIALOG,		\
		  GtkAddOrEditLabelDialog)

#define GTK_ADD_OR_EDIT_LABEL_DIALOG_CLASS(class)			\
  GTK_CHECK_CLASS_CAST ((class), GTK_TYPE_ADD_OR_EDIT_LABEL_DIALOG,	\
			GtkAddOrEditLabelDialogClass)

#define GTK_IS_ADD_OR_EDIT_LABEL_DIALOG(object)				\
  GTK_CHECK_TYPE ((object), GTK_TYPE_ADD_OR_EDIT_LABEL_DIALOG)

#define GTK_IS_ADD_OR_EDIT_LABEL_DIALOG_CLASS(class)			\
  GTK_CHECK_CLASS_TYPE ((class), GTK_TYPE_ADD_OR_EDIT_LABEL_DIALOG)

#define GTK_ADD_OR_EDIT_LABEL_DIALOG_GET_CLASS(object)			\
  GTK_CHECK_GET_CLASS ((object), GTK_TYPE_ADD_OR_EDIT_LABEL_DIALOG,	\
		       GtkAddOrEditLabelDialogClass)


typedef struct _GtkAddOrEditLabelDialog		GtkAddOrEditLabelDialog;
typedef struct _GtkAddOrEditLabelDialogClass	GtkAddOrEditLabelDialogClass;

struct _GtkAddOrEditLabelDialog {
  GtkDialog	  dialog;

  GtkEntry	 *label_entry;
};

struct _GtkAddOrEditLabelDialogClass {
  GtkDialogClass  parent_class;
};


GType		gtk_add_or_edit_label_dialog_get_type (void);

GtkWidget *	gtk_add_or_edit_label_dialog_new (void);

void		gtk_add_or_edit_label_dialog_set_label_text
		  (GtkAddOrEditLabelDialog *dialog, const gchar *label_text);
const gchar *	gtk_add_or_edit_label_dialog_get_label_text
		  (GtkAddOrEditLabelDialog *dialog);


#endif /* QUARRY_GTK_ADD_OR_EDIT_LABEL_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
