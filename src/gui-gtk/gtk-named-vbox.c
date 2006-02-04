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


#include "gtk-named-vbox.h"
#include "gtk-utils.h"

#include <gtk/gtk.h>


static void	gtk_named_vbox_class_init (GtkNamedVBoxClass *class);
static void	gtk_named_vbox_init (GtkNamedVBox *named_vbox);

static void	gtk_named_vbox_forall (GtkContainer *container,
				       gboolean include_internals,
				       GtkCallback callback,
				       gpointer callback_data);

static void	gtk_named_vbox_style_set (GtkWidget *widget,
					  GtkStyle *previous_style);

static void	gtk_named_vbox_size_request (GtkWidget *widget,
					     GtkRequisition *requisition);
static void	gtk_named_vbox_size_allocate (GtkWidget *widget,
					      GtkAllocation *allocation);


static GtkVBoxClass  *parent_class;


GType
gtk_named_vbox_get_type (void)
{
  static GType named_vbox_type = 0;

  if (!named_vbox_type) {
    static GTypeInfo named_vbox_info = {
      sizeof (GtkNamedVBoxClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_named_vbox_class_init,
      NULL,
      NULL,
      sizeof (GtkNamedVBox),
      4,
      (GInstanceInitFunc) gtk_named_vbox_init,
      NULL
    };

    named_vbox_type = g_type_register_static (GTK_TYPE_VBOX, "GtkNamedVBox",
					      &named_vbox_info, 0);
  }

  return named_vbox_type;
}


static void
gtk_named_vbox_class_init (GtkNamedVBoxClass *class)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *) class;

  parent_class = g_type_class_peek_parent (class);

  widget_class->style_set     = gtk_named_vbox_style_set;
  widget_class->size_request  = gtk_named_vbox_size_request;
  widget_class->size_allocate = gtk_named_vbox_size_allocate;

  GTK_CONTAINER_CLASS (class)->forall = gtk_named_vbox_forall;
}


static void
gtk_named_vbox_init (GtkNamedVBox *named_vbox)
{
  GtkWidget *label = gtk_utils_create_left_aligned_label (NULL);

  gtk_widget_set_name (label, "quarry-sub-header");
  gtk_widget_set_parent (label, GTK_WIDGET (named_vbox));
  gtk_widget_show (label);

  named_vbox->label = label;
  named_vbox->left_padding = 0;
}


GtkWidget *
gtk_named_vbox_new (const gchar *label_text,
		    gboolean homogeneous, gint spacing)
{
  GtkWidget *widget = GTK_WIDGET (g_object_new (GTK_TYPE_NAMED_VBOX, NULL));
  GtkBox *box = GTK_BOX (widget);

  box->homogeneous = homogeneous;
  box->spacing = spacing;

  gtk_label_set_text (GTK_LABEL (GTK_NAMED_VBOX (widget)->label), label_text);

  return widget;
}


static void
gtk_named_vbox_forall (GtkContainer *container, gboolean include_internals,
		       GtkCallback callback, gpointer callback_data)
{
  if (include_internals)
    callback (GTK_NAMED_VBOX (container)->label, callback_data);

  GTK_CONTAINER_CLASS (parent_class)->forall (container, include_internals,
					      callback, callback_data);
}


static void
gtk_named_vbox_style_set (GtkWidget *widget, GtkStyle *previous_style)
{
  PangoLayout *layout;

  UNUSED (previous_style);

  /* Calculate padding.  Padding is equal to width of four spaces
   * (recommended by GNOME style guide).
   */
  layout = gtk_widget_create_pango_layout (widget, "    ");
  pango_layout_get_pixel_size (layout,
			       & GTK_NAMED_VBOX (widget)->left_padding, NULL);
  g_object_unref (layout);
}


static void
gtk_named_vbox_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  GtkNamedVBox *named_vbox = GTK_NAMED_VBOX (widget);
  GtkRequisition label_requisition;

  GTK_WIDGET_CLASS (parent_class)->size_request (widget,
						 (&named_vbox
						  ->vbox_requisition));
  gtk_widget_size_request (named_vbox->label, &label_requisition);

  requisition->width = MAX ((named_vbox->vbox_requisition.width
			     + named_vbox->left_padding),
			    label_requisition.width);
  requisition->height = (label_requisition.height
			 + QUARRY_SPACING_SMALL
			 + named_vbox->vbox_requisition.height);
}


static void
gtk_named_vbox_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GtkNamedVBox *named_vbox = GTK_NAMED_VBOX (widget);
  GtkRequisition widget_requisition;
  GtkRequisition label_requisition;
  GtkAllocation child_allocation;

  gtk_widget_get_child_requisition (named_vbox->label, &label_requisition);

  child_allocation.x	  = allocation->x;
  child_allocation.y	  = allocation->y;
  child_allocation.width  = allocation->width;
  child_allocation.height = label_requisition.height;

  gtk_widget_size_allocate (named_vbox->label, &child_allocation);

  child_allocation.x	  += named_vbox->left_padding;
  child_allocation.y	  += label_requisition.height + QUARRY_SPACING_SMALL;
  child_allocation.width  -= named_vbox->left_padding;
  child_allocation.height  = (allocation->height
			      - (label_requisition.height
				 + QUARRY_SPACING_SMALL));

  widget_requisition = widget->requisition;
  widget->requisition = named_vbox->vbox_requisition;
  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, &child_allocation);

  widget->requisition = widget_requisition;
  widget->allocation = *allocation;
}


void
gtk_named_vbox_set_label_text (GtkNamedVBox *named_vbox,
			       const gchar *label_text)
{
  g_return_if_fail (GTK_IS_NAMED_VBOX (named_vbox));

  gtk_label_set_text (GTK_LABEL (named_vbox->label), label_text);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
