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
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,     *
 * Boston, MA 02110-1301, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* This file includes modified code from gtkhbox.c which is
 * distributed with GTK+.  GTK+ can be found at
 * ftp://ftp.gtk.org/pub/gtk/.
 *
 * This file contains implementation of QVBox GTK+ class.  It is
 * basically the same as GtkVBox, but allows some additional size
 * negotiation via callbacks.  It also cannot be homogeneous (using
 * gtk_box_set_homogeneous() won't have any effect).
 *
 * This is kept reasonably separated from the rest of Quarry to be
 * useful in other programs.  However, some things (e.g. properties)
 * are missing from the implementation, so this won't count as a
 * completed GTK+ component.
 */


#include "gtk-qvbox.h"


static void	gtk_qvbox_class_init (GtkQVBoxClass *class);

static void	gtk_qvbox_size_request (GtkWidget *widget,
					GtkRequisition *requisition);
static void	gtk_qvbox_size_allocate (GtkWidget *widget,
					 GtkAllocation *allocation);


GType
gtk_qvbox_get_type (void)
{
  static GType qvbox_type = 0;

  if (!qvbox_type) {
    static GTypeInfo qvbox_info = {
      sizeof (GtkQVBoxClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_qvbox_class_init,
      NULL,
      NULL,
      sizeof (GtkQVBox),
      0,
      NULL,
      NULL
    };

    qvbox_type = g_type_register_static (GTK_TYPE_QBOX, "GtkQVBox",
					 &qvbox_info, 0);
  }

  return qvbox_type;
}


static void
gtk_qvbox_class_init (GtkQVBoxClass *class)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *) class;

  widget_class->size_request = gtk_qvbox_size_request;
  widget_class->size_allocate = gtk_qvbox_size_allocate;
}


GtkWidget *
gtk_qvbox_new (gint spacing)
{
  GtkWidget *widget = GTK_WIDGET (g_object_new (GTK_TYPE_QVBOX, NULL));
  GtkBox *box = GTK_BOX (widget);

  box->homogeneous = FALSE;
  box->spacing = spacing;

  return widget;
}


static void
gtk_qvbox_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  GtkBox *box = GTK_BOX (widget);
  GList *children;
  gint visible_children = 0;
  gint double_border_width;

  double_border_width = GTK_CONTAINER (widget)->border_width * 2;
  requisition->width  = double_border_width;
  requisition->height = double_border_width;

  for (children = box->children; children; children = children->next) {
    GtkBoxChild *child = children->data;

    if (GTK_WIDGET_VISIBLE (child->widget)) {
      GtkRequisition child_requisition;

      visible_children++;

      gtk_widget_size_request (child->widget, &child_requisition);
      requisition->width = MAX (requisition->width, child_requisition.width);
      requisition->height += child_requisition.height + child->padding * 2;
    }
  }

  if (visible_children > 0)
    requisition->height += (visible_children - 1) * box->spacing;
}


static void
gtk_qvbox_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GtkBox *box = GTK_BOX (widget);
  GList *children;
  gint visible_children = 0;
  gint expandable_children = 0;

  widget->allocation = *allocation;

  for (children = box->children; children; children = children->next) {
    GtkBoxChild *child = children->data;

    if (GTK_WIDGET_VISIBLE (child->widget)) {
      visible_children++;
      if (child->expand)
	expandable_children++;
    }
  }

  if (visible_children > 0) {
    GtkQBox *qbox = GTK_QBOX (widget);
    GtkRequisition child_requisition;
    GtkAllocation child_allocation;
    gint border_width = GTK_CONTAINER (widget)->border_width;
    gint extra_height = allocation->height - widget->requisition.height;
    gint extra_height_share = 0;
    gint ruling_widget_height = 0;
    GtkTextDirection direction = gtk_widget_get_direction (widget);
    gint pack_start_y;
    gint pack_end_y;

    child_allocation.x = allocation->x + border_width;
    child_allocation.width = MAX (1, allocation->width - border_width * 2);

    if (qbox->ruling_widget && GTK_WIDGET_VISIBLE (qbox->ruling_widget)) {
      gtk_widget_get_child_requisition (qbox->ruling_widget,
					&child_requisition);
      ruling_widget_height = child_requisition.height;

      if (extra_height > 0) {
	gint requested_height = qbox->widget_callback (qbox->ruling_widget,
						       child_allocation.width);

	if (requested_height >= child_requisition.height + extra_height) {
	  ruling_widget_height = child_requisition.height + extra_height;
	  extra_height = 0;
	}
	else if (requested_height > child_requisition.height) {
	  ruling_widget_height = requested_height;
	  extra_height -= requested_height - child_requisition.height;
	}
      }
    }

    if (expandable_children > 0)
      extra_height_share = extra_height / expandable_children;
    else
      extra_height = 0;

    pack_start_y = allocation->y + border_width;
    pack_end_y = ((allocation->y + allocation->height - border_width)
		  + box->spacing);

    for (children = box->children; children; children = children->next) {
      GtkBoxChild *child = children->data;

      if (GTK_WIDGET_VISIBLE (child->widget)) {
	gint child_extra_height = 0;
	gint full_child_height;

	gtk_widget_get_child_requisition (child->widget, &child_requisition);

	if (child->widget == qbox->ruling_widget)
	  child_allocation.height = ruling_widget_height;
	else
	  child_allocation.height = child_requisition.height;

	if (child->expand) {
	  if (expandable_children == 1)
	    child_extra_height = extra_height;
	  else {
	    child_extra_height = extra_height_share;
	    extra_height -= extra_height_share;

	    expandable_children--;
	  }
	}

	full_child_height = (child_allocation.height + child_extra_height
			     + child->padding * 2 + box->spacing);

	if (child->pack == GTK_PACK_START) {
	  child_allocation.y = pack_start_y + child->padding;
	  pack_start_y += full_child_height;
	}
	else {
	  pack_end_y -= full_child_height;
	  child_allocation.y = pack_end_y + child->padding;
	}

	if (child->fill)
	  child_allocation.height += child_extra_height;
	else
	  child_allocation.y += child_extra_height / 2;

	if (direction == GTK_TEXT_DIR_RTL) {
	  child_allocation.y = (allocation->y + allocation->height
				- (child_allocation.y - allocation->y)
				- child_allocation.height);
	}

	gtk_widget_size_allocate (child->widget, &child_allocation);
      }
    }
  }
}


gint
gtk_qvbox_negotiate_width (GtkWidget *widget, gint height)
{
  GtkQBox *qbox = GTK_QBOX (widget);
  GtkRequisition child_requisition;

  if (qbox->ruling_widget == NULL || !GTK_WIDGET_VISIBLE (qbox->ruling_widget))
    return 0;

  gtk_widget_get_child_requisition (qbox->ruling_widget, &child_requisition);
  return qbox->widget_callback (qbox->ruling_widget,
				(child_requisition.height
				 + height - widget->requisition.height));
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
