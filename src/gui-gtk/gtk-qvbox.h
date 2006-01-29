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


#ifndef QUARRY_GTK_QVBOX_H
#define QUARRY_GTK_QVBOX_H


#include "quarry.h"

#include "gtk-qbox.h"


#define GTK_TYPE_QVBOX		(gtk_qvbox_get_type ())
#define GTK_QVBOX(obj)		GTK_CHECK_CAST ((obj), GTK_TYPE_QVBOX,	\
						GtkQVBox)
#define GTK_QVBOX_CLASS(klass)						\
  GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_QVBOX, GtkQVBoxClass)

#define GTK_IS_QVBOX(obj)	GTK_CHECK_TYPE ((obj), GTK_TYPE_QVBOX)
#define GTK_IS_QVBOX_CLASS(klass)					\
  GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_QVBOX)

#define GTK_QVBOX_GET_CLASS(obj)					\
  GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_QVBOX, GtkQVBoxClass)


typedef struct _GtkQVBox	GtkQVBox;
typedef struct _GtkQVBoxClass	GtkQVBoxClass;

struct _GtkQVBox {
  GtkQBox	qbox;
};

struct _GtkQVBoxClass {
  GtkQBoxClass	parent_class;
};


GtkType		gtk_qvbox_get_type (void);
GtkWidget *	gtk_qvbox_new (gint spacing);

gint		gtk_qvbox_negotiate_width (GtkWidget *widget, gint height);


#endif /* QUARRY_GTK_QVBOX_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
