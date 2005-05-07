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


#ifndef QUARRY_GTK_QHBOX_H
#define QUARRY_GTK_QHBOX_H


#include "quarry.h"

#include "gtk-qbox.h"


#define GTK_TYPE_QHBOX		(gtk_qhbox_get_type ())
#define GTK_QHBOX(obj)		GTK_CHECK_CAST ((obj), GTK_TYPE_QHBOX,	\
						GtkQHBox)
#define GTK_QHBOX_CLASS(klass)						\
  GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_QHBOX, GtkQHBoxClass)

#define GTK_IS_QHBOX(obj)	GTK_CHECK_TYPE ((obj), GTK_TYPE_QHBOX)
#define GTK_IS_QHBOX_CLASS(klass)					\
  GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_QHBOX)

#define GTK_QHBOX_GET_CLASS(obj)					\
  GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_QHBOX, GtkQHBoxClass)


typedef struct _GtkQHBox	GtkQHBox;
typedef struct _GtkQHBoxClass	GtkQHBoxClass;

struct _GtkQHBox {
  GtkQBox	qbox;
};

struct _GtkQHBoxClass {
  GtkQBoxClass	parent_class;
};


GType		gtk_qhbox_get_type (void);
GtkWidget *	gtk_qhbox_new (gint spacing);

gint		gtk_qhbox_negotiate_height (GtkWidget *widget, gint width);


#endif /* QUARRY_GTK_QHBOX_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
