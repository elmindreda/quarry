/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and       *
 * Josh MacDonald.                                                 *
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

/* This file includes modified code from gtkhbox.c which is
 * distributed with GTK+.  GTK+ can be found at
 * ftp://ftp.gtk.org/pub/gtk/.
 *
 * This file contains implementation of QHBox GTK+ class.  It is
 * basically the same as GtkHBox, but allows some additional size
 * negotiation via callbacks.  It also cannot be homogeneous (using
 * gtk_box_set_homogeneous() won't have any effect).
 *
 * This is kept reasonably separated from the rest of Quarry to be
 * useful in other programs.  However, some things (e.g. properties)
 * are missing from the implementation, so this won't count as a
 * completed GTK+ component.
 */


#include "gtk-qhbox.h"


static void	gtk_qhbox_class_init(GtkQHBoxClass *class);

static void	gtk_qhbox_size_request(GtkWidget *widget,
				       GtkRequisition *requisition);
static void	gtk_qhbox_size_allocate(GtkWidget *widget,
					GtkAllocation *allocation);


GtkType
gtk_qhbox_get_type(void)
{
  static GtkType qhbox_type = 0;

  if (!qhbox_type) {
    static GTypeInfo qhbox_info = {
      sizeof(GtkQHBoxClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_qhbox_class_init,
      NULL,
      NULL,
      sizeof(GtkQHBox),
      0,
      NULL,
      NULL
    };

    qhbox_type = g_type_register_static(GTK_TYPE_QBOX, "GtkQHBox",
					&qhbox_info, 0);
  }

  return qhbox_type;
}


static void
gtk_qhbox_class_init(GtkQHBoxClass *class)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *) class;

  widget_class->size_request = gtk_qhbox_size_request;
  widget_class->size_allocate = gtk_qhbox_size_allocate;
}


GtkWidget *
gtk_qhbox_new(gint spacing)
{
  GtkWidget *widget = GTK_WIDGET(g_object_new(GTK_TYPE_QHBOX, NULL));
  GtkBox *box = GTK_BOX(widget);

  box->homogeneous = FALSE;
  box->spacing = spacing;

  return widget;
}


static void
gtk_qhbox_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
  GtkBox *box = GTK_BOX(widget);
  GList *children;
  gint visible_children = 0;
  gint double_border_width;

  double_border_width = GTK_CONTAINER(widget)->border_width * 2;
  requisition->width  = double_border_width;
  requisition->height = double_border_width;

  for (children = box->children; children; children = children->next) {
    GtkBoxChild *child = children->data;

    if (GTK_WIDGET_VISIBLE(child->widget)) {
      GtkRequisition child_requisition;

      visible_children++;

      gtk_widget_size_request(child->widget, &child_requisition);
      requisition->width += child_requisition.width + child->padding * 2;
      requisition->height = MAX(requisition->height, child_requisition.height);
    }
  }

  if (visible_children > 0)
    requisition->width += (visible_children - 1) * box->spacing;
}


static void
gtk_qhbox_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
  GtkBox *box = GTK_BOX(widget);
  GList *children;
  gint visible_children = 0;
  gint expandable_children = 0;

  widget->allocation = *allocation;

  for (children = box->children; children; children = children->next) {
    GtkBoxChild *child = children->data;

    if (GTK_WIDGET_VISIBLE(child->widget)) {
      visible_children++;
      if (child->expand)
	expandable_children++;
    }
  }

  if (visible_children > 0) {
    GtkQBox *qbox = GTK_QBOX(widget);
    GtkRequisition child_requisition;
    GtkAllocation child_allocation;
    gint border_width = GTK_CONTAINER(widget)->border_width;
    gint extra_width = allocation->width - widget->requisition.width;
    gint extra_width_share = 0;
    gint ruling_widget_width = 0;
    GtkTextDirection direction = gtk_widget_get_direction(widget);
    gint pack_start_x;
    gint pack_end_x;

    child_allocation.y = allocation->y + border_width;
    child_allocation.height = MAX(1, allocation->height - border_width * 2);

    if (qbox->ruling_widget && GTK_WIDGET_VISIBLE(qbox->ruling_widget)) {
      gtk_widget_get_child_requisition(qbox->ruling_widget,
				       &child_requisition);
      ruling_widget_width = child_requisition.width;

      if (extra_width > 0) {
	gint requested_width = qbox->widget_callback(qbox->ruling_widget,
						     child_allocation.height);

	if (requested_width >= child_requisition.width + extra_width) {
	  ruling_widget_width = child_requisition.width + extra_width;
	  extra_width = 0;
	}
	else if (requested_width > child_requisition.width) {
	  ruling_widget_width = requested_width;
	  extra_width -= requested_width - child_requisition.width;
	}
      }
    }

    if (expandable_children > 0)
      extra_width_share = extra_width / expandable_children;
    else
      extra_width = 0;

    pack_start_x = allocation->x + border_width;
    pack_end_x = ((allocation->x + allocation->width - border_width)
		  + box->spacing);

    for (children = box->children; children; children = children->next) {
      GtkBoxChild *child = children->data;

      if (GTK_WIDGET_VISIBLE(child->widget)) {
	gint child_extra_width = 0;
	gint full_child_width;

	gtk_widget_get_child_requisition(child->widget, &child_requisition);

	if (child->widget == qbox->ruling_widget)
	  child_allocation.width = ruling_widget_width;
	else
	  child_allocation.width = child_requisition.width;

	if (child->expand) {
	  if (expandable_children == 1)
	    child_extra_width = extra_width;
	  else {
	    child_extra_width = extra_width_share;
	    extra_width -= extra_width_share;

	    expandable_children--;
	  }
	}

	full_child_width = (child_allocation.width + child_extra_width
			    + child->padding * 2 + box->spacing);

	if (child->pack == GTK_PACK_START) {
	  child_allocation.x = pack_start_x + child->padding;
	  pack_start_x += full_child_width;
	}
	else {
	  pack_end_x -= full_child_width;
	  child_allocation.x = pack_end_x + child->padding;
	}

	if (child->fill)
	  child_allocation.width += child_extra_width;
	else
	  child_allocation.x += child_extra_width / 2;

	if (direction == GTK_TEXT_DIR_RTL) {
	  child_allocation.x = (allocation->x + allocation->width
				- (child_allocation.x - allocation->x)
				- child_allocation.width);
	}

	gtk_widget_size_allocate(child->widget, &child_allocation);
      }
    }
  }
}


gint
gtk_qhbox_negotiate_height(GtkWidget *widget, gint width)
{
  GtkQBox *qbox = GTK_QBOX(widget);
  GtkRequisition child_requisition;

  if (qbox->ruling_widget == NULL || !GTK_WIDGET_VISIBLE(qbox->ruling_widget))
    return 0;

  gtk_widget_get_child_requisition(qbox->ruling_widget, &child_requisition);
  return qbox->widget_callback(qbox->ruling_widget,
			       (child_requisition.width
				+ width - widget->requisition.width));
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
