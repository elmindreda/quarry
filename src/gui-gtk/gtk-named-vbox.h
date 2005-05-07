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


#ifndef QUARRY_GTK_NAMED_VBOX_H
#define QUARRY_GTK_NAMED_VBOX_H


#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_NAMED_VBOX	(gtk_named_vbox_get_type ())
#define GTK_NAMED_VBOX(obj)						\
  GTK_CHECK_CAST ((obj), GTK_TYPE_NAMED_VBOX, GtkNamedVBox)
#define GTK_NAMED_VBOX_CLASS(klass)					\
  GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_NAMED_VBOX,			\
			GtkNamedVBoxClass)

#define GTK_IS_NAMED_VBOX(obj)						\
  GTK_CHECK_TYPE ((obj), GTK_TYPE_NAMED_VBOX)
#define GTK_IS_NAMED_VBOX_CLASS(klass)					\
  GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_NAMED_VBOX)

#define GTK_NAMED_VBOX_GET_CLASS(obj)					\
  GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_NAMED_VBOX, GtkNamedVBoxClass)


typedef struct _GtkNamedVBox		GtkNamedVBox;
typedef struct _GtkNamedVBoxClass	GtkNamedVBoxClass;

struct _GtkNamedVBox {
  GtkVBox	   vbox;

  GtkWidget	  *label;
  guint		   left_padding;

  GtkRequisition   vbox_requisition;
};

struct _GtkNamedVBoxClass {
  GtkVBoxClass	   parent_class;
};


GType		gtk_named_vbox_get_type (void);

GtkWidget *	gtk_named_vbox_new (const gchar *label_text,
				    gboolean homogeneous, gint spacing);

void		gtk_named_vbox_set_label_text (GtkNamedVBox *named_vbox,
					       const gchar *label_text);


#endif /* QUARRY_GTK_NAMED_VBOX_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
