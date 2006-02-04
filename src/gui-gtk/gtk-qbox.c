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

/* This file contains implementation of QBox GTK+ class. It is the base for
 * QHBox and QVBox classes, which provide all necessary functionality for
 * smart goban placing inside Quarry windows. Practically, they allow
 * additional size negotiation after receiving `size_allocate' signal.
 *
 * This is kept reasonably separated from the rest of Quarry to be useful in
 * other programs. However, some things (e.g. properties) are missing from
 * the implementation, so this won't count as a completed GTK+ component.
 */


#include "gtk-qbox.h"


static void	gtk_qbox_class_init (GtkQBoxClass *klass);
static void	gtk_qbox_init (GtkQBox *qbox);

static void	gtk_qbox_remove (GtkContainer *container, GtkWidget *widget);


static GtkContainerClass *parent_class;


GType
gtk_qbox_get_type (void)
{
  static GtkType qbox_type = 0;

  if (!qbox_type) {
    static GTypeInfo qbox_info = {
      sizeof (GtkQBoxClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_qbox_class_init,
      NULL,
      NULL,
      sizeof (GtkQBox),
      0,
      (GInstanceInitFunc) gtk_qbox_init,
      NULL
    };

    qbox_type = g_type_register_static (GTK_TYPE_BOX, "GtkQBox",
					&qbox_info, 0);
  }

  return qbox_type;
}


static void
gtk_qbox_class_init (GtkQBoxClass *class)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  parent_class = g_type_class_peek_parent (class);
  container_class->remove = gtk_qbox_remove;
}


static void
gtk_qbox_init (GtkQBox *qbox)
{
  qbox->ruling_widget = NULL;
  qbox->widget_callback = NULL;
}


/* We need to forget about the ruling widget if it is it being
 * removed from the box.
 */
static void
gtk_qbox_remove (GtkContainer *container, GtkWidget *widget)
{
  GtkQBox *qbox = GTK_QBOX (container);

  if (widget == qbox->ruling_widget) {
    qbox->ruling_widget = NULL;
    qbox->widget_callback = NULL;
  }

  parent_class->remove (container, widget);
}


void
gtk_qbox_set_ruling_widget (GtkQBox *qbox,
			    GtkWidget *widget, GtkQBoxCallback widget_callback)
{
  g_return_if_fail (GTK_IS_QBOX (qbox));

  if (widget != NULL) {
    g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (qbox));
    g_return_if_fail (widget_callback);
  }
  else
    widget_callback = NULL;

  qbox->ruling_widget = widget;
  qbox->widget_callback = widget_callback;
}


GtkWidget *
gtk_qbox_get_ruling_widget (GtkQBox *qbox)
{
  g_return_val_if_fail (GTK_IS_QBOX (qbox), NULL);

  return qbox->ruling_widget;
}


GtkQBoxCallback
gtk_qbox_get_ruling_widget_callback (GtkQBox *qbox)
{
  g_return_val_if_fail (GTK_IS_QBOX (qbox), NULL);

  return qbox->widget_callback;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
