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


#ifndef QUARRY_GTK_PROGRESS_DIALOG_H
#define QUARRY_GTK_PROGRESS_DIALOG_H


#include "gtk-thread-interface.h"
#include "quarry.h"


#if THREADS_SUPPORTED


#include <gtk/gtk.h>


#define GTK_TYPE_PROGRESS_DIALOG	(gtk_progress_dialog_get_type())
#define GTK_PROGRESS_DIALOG(obj)					\
  (GTK_CHECK_CAST((obj), GTK_TYPE_PROGRESS_DIALOG, GtkProgressDialog))
#define GTK_PROGRESS_DIALOG_CLASS(klass)				\
  (GTK_CHECK_CLASS_CAST((klass), GTK_TYPE_PROGRESS_DIALOG,		\
			GtkProgressDialogClass))

#define GTK_IS_PROGRESS_DIALOG(obj)					\
  (GTK_CHECK_TYPE((obj), GTK_TYPE_PROGRESS_DIALOG))
#define GTK_IS_PROGRESS_DIALOG_CLASS(klass)				\
  (GTK_CHECK_CLASS_TYPE((klass), GTK_TYPE_PROGRESS_DIALOG))

#define GTK_PROGRESS_DIALOG_GET_CLASS(obj)				\
  (GTK_CHECK_GET_CLASS((obj), GTK_TYPE_PROGRESS_DIALOG,			\
		       GtkProgressDialogClass))


typedef struct _GtkProgressDialog	GtkProgressDialog;
typedef struct _GtkProgressDialogClass	GtkProgressDialogClass;

typedef gboolean (* GtkProgressDialogCallback)
		      (GtkProgressDialog *progress_dialog, gpointer user_data);

struct _GtkProgressDialog {
  GtkDialog		      dialog;

  GtkWidget		     *parent;

  GtkLabel		     *label;
  GtkProgressBar	     *progress_bar;

  GtkProgressDialogCallback   update_callback;
  GtkProgressDialogCallback   cancel_callback;
  gpointer		      user_data;

  gint			      last_displayed_percentage;

  guint			      timeout_handler_id;
};

struct _GtkProgressDialogClass {
  GtkDialogClass	      parent_class;
};


GtkType		gtk_progress_dialog_get_type(void);

GtkWidget *	gtk_progress_dialog_new
		  (GtkWindow *parent,
		   const gchar *title, const gchar *label_text,
		   GtkProgressDialogCallback update_callback,
		   GtkProgressDialogCallback cancel_callback,
		   gpointer user_data);

void		gtk_progress_dialog_set_fraction
		  (GtkProgressDialog *progress_dialog,
		   gdouble fraction, const gchar *title_part);

void		gtk_progress_dialog_recover_parent
		  (GtkProgressDialog *progress_dialog);


#endif /* THREADS_SUPPORTED */


#endif /* QUARRY_GTK_PROGRESS_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
