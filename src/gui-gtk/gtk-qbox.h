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


#ifndef QUARRY_GTK_QBOX_H
#define QUARRY_GTK_QBOX_H


#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_QBOX		(gtk_qbox_get_type ())
#define GTK_QBOX(obj)		GTK_CHECK_CAST ((obj), GTK_TYPE_QBOX,	\
						GtkQBox)
#define GTK_QBOX_CLASS(klass)						\
  GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_QBOX, GtkQBoxClass)

#define GTK_IS_QBOX(obj)	GTK_CHECK_TYPE ((obj), GTK_TYPE_QBOX))
#define GTK_IS_QBOX_CLASS(klass)					\
  GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_QBOX)

#define GTK_QBOX_GET_CLASS(obj)						\
  GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_QBOX, GtkQBoxClass)


typedef gint (* GtkQBoxCallback) (GtkWidget *widget, gint dimension);

typedef struct _GtkQBox		GtkQBox;
typedef struct _GtkQBoxClass	GtkQBoxClass;

struct _GtkQBox {
  GtkBox	    box;

  GtkWidget	   *ruling_widget;
  GtkQBoxCallback   widget_callback;
};

struct _GtkQBoxClass {
  GtkBoxClass	    parent_class;
};


GtkType		 gtk_qbox_get_type (void);

void		 gtk_qbox_set_ruling_widget (GtkQBox *qbox, GtkWidget *widget,
					     GtkQBoxCallback widget_callback);

#define gtk_qbox_unset_ruling_widget(qbox)		\
  gtk_qbox_set_ruling_widget ((qbox), NULL, NULL)

GtkWidget *	 gtk_qbox_get_ruling_widget (GtkQBox *qbox);
GtkQBoxCallback  gtk_qbox_get_ruling_widget_callback (GtkQBox *qbox);


#endif /* QUARRY_GTK_QBOX_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
