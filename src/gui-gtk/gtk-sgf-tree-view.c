/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004, 2005, 2006 Paul Pogonyshev.                 *
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


/* FIXME: Implement smarter invalidation of view: only invalidate the
 *	  changed part, not the whole widget.
 */


#include "gtk-sgf-tree-view.h"

#include "gtk-configuration.h"
#include "gtk-goban-base.h"
#include "gtk-sgf-tree-signal-proxy.h"
#include "gtk-tile-set.h"
#include "gtk-utils.h"
#include "gui-back-end.h"
#include "quarry-marshal.h"
#include "sgf.h"
#include "board.h"
#include "utils.h"

#include <gtk/gtk.h>

#if HAVE_MEMORY_H
#include <memory.h>
#endif


/* FIXME: Make it zoomable and delete this (or, rather, throw them to
 * `gtk-configuration.list'.
 */
#define DEFAULT_CELL_SIZE	24


#define PADDING_WIDTH(cell_size)					\
  ((gint) (((cell_size) + 6) / 12))

#define FULL_CELL_SIZE(view)						\
  ((view)->base.cell_size + 2 * PADDING_WIDTH ((view)->base.cell_size))

#define VIEW_PORT_NODE(view, x, y)					\
  * ((view)->view_port_nodes						\
     + (((y) - (view)->view_port_y0)					\
	* ((view)->view_port_x1 - (view)->view_port_x0))		\
     + ((x) - (view)->view_port_x0))



#define SHOULD_TRACK_CURRENT_NODE(view)					\
  ((view)->current_tree							\
   && (game_tree_view.track_current_node == TRACK_CURRENT_NODE_ALWAYS	\
       || ((game_tree_view.track_current_node				\
	    == TRACK_CURRENT_NODE_AUTOMATICALLY)			\
	   && (sgf_game_tree_node_is_within_view_port			\
	       ((view)->current_tree,					\
		(view)->current_tree->current_node,			\
		(view)->view_port_x0, (view)->view_port_y0,		\
		(view)->view_port_x1, (view)->view_port_y1,		\
		NULL, NULL)))))


static void	 gtk_sgf_tree_view_class_init (GtkSgfTreeViewClass *class);
static void	 gtk_sgf_tree_view_init (GtkSgfTreeView *view);
static void	 gtk_sgf_tree_view_realize (GtkWidget *widget);

static void	 gtk_sgf_tree_view_size_request (GtkWidget *widget,
						 GtkRequisition *requisition);
static void	 gtk_sgf_tree_view_size_allocate (GtkWidget *widget,
						  GtkAllocation  *allocation);

static void	 gtk_sgf_tree_view_set_scroll_adjustments
		   (GtkSgfTreeView *view,
		    GtkAdjustment *hadjustment, GtkAdjustment *vadjustment);

static void	 gtk_sgf_tree_view_style_set (GtkWidget *widget,
					      GtkStyle *previous_style);

static gboolean	 gtk_sgf_tree_view_expose (GtkWidget *widget,
					   GdkEventExpose *event);

static gboolean	 gtk_sgf_tree_view_button_press_event (GtkWidget *widget,
						       GdkEventButton *event);
static gboolean	 gtk_sgf_tree_view_button_release_event
		   (GtkWidget *widget, GdkEventButton *event);

static gboolean	 gtk_sgf_tree_view_motion_notify_event (GtkWidget *widget,
							GdkEventMotion *event);

static void	 gtk_sgf_tree_view_unrealize (GtkWidget *widget);

static void	 gtk_sgf_tree_view_finalize (GObject *object);
static void	 gtk_sgf_tree_view_destroy (GtkObject *object);


static void	 configure_adjustment (GtkSgfTreeView *view,
				       gboolean horizontal,
				       gint original_value);
static void	 disconnect_adjustment (GtkSgfTreeView *view,
					GtkAdjustment *adjustment);
static void	 scroll_adjustment_value_changed (GtkSgfTreeView *view);

static void	 center_on_current_node (GtkSgfTreeView *view);
static void	 track_current_node (GtkSgfTreeView *view);

static void	 update_view_port (GtkSgfTreeView *view);
static gboolean	 update_view_port_and_maybe_move_or_resize_window
		   (GtkSgfTreeView *view,
		    gint original_hadjustment_value,
		    gint original_vadjustment_value);

static void	 about_to_modify_map (GtkSgfTreeView *view);
static void	 about_to_change_current_node (GtkSgfTreeView *view);
static void	 current_node_changed (GtkSgfTreeView *view);
static void	 map_modified (GtkSgfTreeView *view);

static GtkTooltips *
		 get_shared_tooltips (void);

static void	 append_limited_text (StringBuffer *buffer, const char *text,
				      gint num_characters_limit);

static void	 synthesize_enter_notify_event (GdkEvent *event);


static GtkGobanBaseClass  *parent_class;


enum {
  SGF_TREE_VIEW_CLICKED,
  NUM_SIGNALS
};

static guint		   sgf_tree_view_signals[NUM_SIGNALS];


GType
gtk_sgf_tree_view_get_type (void)
{
  static GType sgf_tree_view_type = 0;

  if (!sgf_tree_view_type) {
    static GTypeInfo sgf_tree_view_info = {
      sizeof (GtkSgfTreeViewClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_sgf_tree_view_class_init,
      NULL,
      NULL,
      sizeof (GtkSgfTreeView),
      1,
      (GInstanceInitFunc) gtk_sgf_tree_view_init,
      NULL
    };

    sgf_tree_view_type = g_type_register_static (GTK_TYPE_GOBAN_BASE,
						 "GtkSgfTreeView",
						 &sgf_tree_view_info, 0);
  }

  return sgf_tree_view_type;
}


static void
gtk_sgf_tree_view_class_init (GtkSgfTreeViewClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  G_OBJECT_CLASS (class)->finalize = gtk_sgf_tree_view_finalize;

  GTK_OBJECT_CLASS (class)->destroy = gtk_sgf_tree_view_destroy;

  widget_class->realize		     = gtk_sgf_tree_view_realize;
  widget_class->unrealize	     = gtk_sgf_tree_view_unrealize;
  widget_class->size_request	     = gtk_sgf_tree_view_size_request;
  widget_class->size_allocate	     = gtk_sgf_tree_view_size_allocate;
  widget_class->style_set	     = gtk_sgf_tree_view_style_set;
  widget_class->expose_event	     = gtk_sgf_tree_view_expose;
  widget_class->button_press_event   = gtk_sgf_tree_view_button_press_event;
  widget_class->button_release_event = gtk_sgf_tree_view_button_release_event;
  widget_class->motion_notify_event  = gtk_sgf_tree_view_motion_notify_event;

  class->set_scroll_adjustments	= gtk_sgf_tree_view_set_scroll_adjustments;
  class->sgf_tree_view_clicked	= NULL;

  widget_class->set_scroll_adjustments_signal
    = g_signal_new ("set-scroll-adjustments",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		    G_STRUCT_OFFSET (GtkSgfTreeViewClass,
				     set_scroll_adjustments),
		    NULL, NULL,
		    quarry_marshal_VOID__OBJECT_OBJECT,
		    G_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

  sgf_tree_view_signals[SGF_TREE_VIEW_CLICKED]
    = g_signal_new ("sgf-tree-view-clicked",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkSgfTreeViewClass,
				     sgf_tree_view_clicked),
		    NULL, NULL,
		    quarry_marshal_VOID__POINTER_INT,
		    G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT);
}


static void
gtk_sgf_tree_view_init (GtkSgfTreeView *view)
{
  /* FIXME */
  gtk_goban_base_set_cell_size (GTK_GOBAN_BASE (view), DEFAULT_CELL_SIZE);

  view->hadjustment		  = NULL;
  view->vadjustment		  = NULL;
  view->ignore_adjustment_changes = FALSE;

  view->current_tree		  = NULL;

  view->map_width		  = 1;
  view->map_height		  = 1;

  view->last_tooltips_node	  = NULL;
}


GtkWidget *
gtk_sgf_tree_view_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_SGF_TREE_VIEW, NULL));
}


static void
gtk_sgf_tree_view_realize (GtkWidget *widget)
{
  GtkSgfTreeView *view = GTK_SGF_TREE_VIEW (widget);
  GdkWindowAttr attributes;
  const gint attributes_mask = (GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL
				| GDK_WA_COLORMAP);
  const gint event_mask = (GDK_EXPOSURE_MASK
			   | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			   | GDK_POINTER_MOTION_MASK
			   | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

  if (!view->hadjustment || !view->vadjustment)
    gtk_sgf_tree_view_set_scroll_adjustments (view, NULL, NULL);

  if (view->current_tree) {
    sgf_game_tree_get_map_dimensions (view->current_tree,
				      &view->map_width, &view->map_height);

    update_view_port (view);
  }

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.event_mask	 = 0;
  attributes.x		 = widget->allocation.x;
  attributes.y		 = widget->allocation.y;
  attributes.width	 = widget->allocation.width;
  attributes.height	 = widget->allocation.height;
  attributes.wclass	 = GDK_INPUT_OUTPUT;
  attributes.visual	 = gtk_widget_get_visual (widget);
  attributes.colormap	 = gtk_widget_get_colormap (widget);
  attributes.window_type = GDK_WINDOW_CHILD;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, view);

  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);

  attributes.event_mask	 = gtk_widget_get_events (widget) | event_mask;
  attributes.x		 = - view->hadjustment->value;
  attributes.y		 = - view->vadjustment->value;
  attributes.width	 = view->hadjustment->upper;
  attributes.height	 = view->vadjustment->upper;

  view->output_window = gdk_window_new (widget->window,
					&attributes, attributes_mask);
  gdk_window_set_user_data (view->output_window, view);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, view->output_window,
			    GTK_STATE_NORMAL);

  gdk_window_show (view->output_window);

  GTK_WIDGET_CLASS (parent_class)->realize (widget);
}


static void
gtk_sgf_tree_view_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  GtkSgfTreeView *view = GTK_SGF_TREE_VIEW (widget);
  gint full_cell_size = FULL_CELL_SIZE (view);

  requisition->width  = view->map_width  * full_cell_size;
  requisition->height = view->map_height * full_cell_size;
}


static void
gtk_sgf_tree_view_size_allocate (GtkWidget *widget, GtkAllocation  *allocation)
{
  GtkSgfTreeView *view = GTK_SGF_TREE_VIEW (widget);

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  update_view_port_and_maybe_move_or_resize_window
    (view, (gint) view->hadjustment->value, (gint) view->vadjustment->value);
}


static void
gtk_sgf_tree_view_set_scroll_adjustments (GtkSgfTreeView *view,
					  GtkAdjustment *hadjustment,
					  GtkAdjustment *vadjustment)
{
  if (hadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadjustment));
  else {
    hadjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0,
						      0.0, 0.0, 0.0));
  }

  if (view->hadjustment != hadjustment) {
    disconnect_adjustment (view, view->hadjustment);

    view->hadjustment = hadjustment;
    g_object_ref (hadjustment);
    gtk_object_sink (GTK_OBJECT (hadjustment));

    configure_adjustment (view, TRUE, 0);

    g_signal_connect_swapped (hadjustment, "value-changed",
			      G_CALLBACK (scroll_adjustment_value_changed),
			      view);
  }

  if (vadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadjustment));
  else {
    vadjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0,
						      0.0, 0.0, 0.0));
  }

  if (view->vadjustment != vadjustment) {
    disconnect_adjustment (view, view->vadjustment);

    view->vadjustment = vadjustment;
    g_object_ref (vadjustment);
    gtk_object_sink (GTK_OBJECT (vadjustment));

    configure_adjustment (view, FALSE, 0);

    g_signal_connect_swapped (vadjustment, "value-changed",
			      G_CALLBACK (scroll_adjustment_value_changed),
			      view);
  }
}


/* We need to set style background not on `widget->window', but on the
 * `output_window'.  Therefore the override.
 */
static void
gtk_sgf_tree_view_style_set (GtkWidget *widget, GtkStyle *previous_style)
{
  UNUSED (previous_style);

  if (GTK_WIDGET_REALIZED (widget)) {
    gtk_style_set_background (widget->style,
			      GTK_SGF_TREE_VIEW (widget)->output_window,
			      GTK_STATE_NORMAL);
  }
}


static gboolean
gtk_sgf_tree_view_expose (GtkWidget *widget, GdkEventExpose *event)
{
  UNUSED (event);

  if (GTK_WIDGET_DRAWABLE (widget)) {
    GtkSgfTreeView *view = GTK_SGF_TREE_VIEW (widget);
    int x;
    int y;
    int map_offset;
    int k;
    SgfGameTreeMapLine *lines_scan;
    gint full_cell_size = FULL_CELL_SIZE (view);
    gint full_cell_size_half = full_cell_size / 2;
    gint stones_left_margin = (full_cell_size / 2
			       + view->base.main_tile_set->stones_x_offset);
    gint stones_top_margin = (full_cell_size / 2
			      + view->base.main_tile_set->stones_y_offset);

    /* FIXME: Something is not aligned well... */
    gint sgf_markup_margin = ((full_cell_size
			       - view->base.sgf_markup_tile_set->tile_size)
			      / 2) + 1;

    GdkWindow *window = view->output_window;
    GdkGC *gc = widget->style->fg_gc[GTK_STATE_NORMAL];

    g_return_val_if_fail (view->current_tree, FALSE);
    g_return_val_if_fail (view->view_port_nodes, FALSE);
    g_return_val_if_fail (view->view_port_lines, FALSE);
    g_return_val_if_fail (view->tile_map, FALSE);
    g_return_val_if_fail (view->sgf_markup_tile_map, FALSE);

    gdk_gc_set_line_attributes (gc, 2, GDK_LINE_SOLID,
				GDK_CAP_ROUND, GDK_JOIN_ROUND);

    for (lines_scan = view->view_port_lines, k = 0;
	 k < view->num_view_port_lines; lines_scan++, k++) {
      GdkPoint points[4];
      GdkPoint *next_point = points;
      GdkPoint *point_scan;
      gint y2 = lines_scan->y1 + (lines_scan->x2 - lines_scan->x0);

      /* The clipping below is a workaround for X drawing functions'
       * bug: they cannot handle line lengths over 32K properly.
       */

      if (lines_scan->x2 >= view->view_port_x0) {
	if (lines_scan->x0 >= view->view_port_x0) {
	  if (lines_scan->y1 >= view->view_port_y0) {
	    if (lines_scan->y0 < lines_scan->y1) {
	      next_point->x = lines_scan->x0;
	      next_point->y = MAX (lines_scan->y0, view->view_port_y0 - 1);
	      next_point++;
	    }

	    if (lines_scan->y1 < view->view_port_y1) {
	      next_point->x = lines_scan->x0;
	      next_point->y = lines_scan->y1;
	      next_point++;
	    }
	    else {
	      next_point->x = lines_scan->x0;
	      next_point->y = view->view_port_y1;
	      next_point++;

	      goto line_clipped;
	    }
	  }
	  else {
	    next_point->y = view->view_port_y0 - 1;
	    next_point->x = lines_scan->x0 + (next_point->y - lines_scan->y1);
	    next_point++;
	  }
	}
	else {
	  next_point->x = view->view_port_x0 - 1;
	  next_point->y = lines_scan->y1 + (next_point->x - lines_scan->x0);
	  next_point++;
	}

	if (y2 > lines_scan->y1) {
	  if ((view->view_port_x1 - (next_point - 1)->x)
	      > (view->view_port_y1 - (next_point - 1)->y)) {
	    next_point->y = MIN (y2, view->view_port_y1);
	    next_point->x = lines_scan->x0 + (next_point->y - lines_scan->y1);
	    next_point++;
	  }
	  else {
	    next_point->x = MIN (lines_scan->x2, view->view_port_x1);
	    next_point->y = lines_scan->y1 + (next_point->x - lines_scan->x0);
	    next_point++;
	  }
	}
      }
      else {
	next_point->x = view->view_port_x0 - 1;
	next_point->y = y2;
	next_point++;
      }

      if (y2 < view->view_port_y1) {
	next_point->x = MIN (lines_scan->x3, view->view_port_x1);
	next_point->y = y2;
	next_point++;
      }

    line_clipped:

      for (point_scan = points; point_scan < next_point; point_scan++) {
	point_scan->x = point_scan->x * full_cell_size + full_cell_size_half;
	point_scan->y = point_scan->y * full_cell_size + full_cell_size_half;
      }

      gdk_draw_lines (window, gc, points, next_point - points);
    }

    for (map_offset = 0, y = view->view_port_y0; y < view->view_port_y1; y++) {
      for (x = view->view_port_x0; x < view->view_port_x1; map_offset++, x++) {
	const SgfNode *sgf_node = view->view_port_nodes[map_offset];
	gint main_tile		= view->tile_map[map_offset];
	gint sgf_markup_tile	= view->sgf_markup_tile_map[map_offset];

	if (sgf_node == view->current_tree->current_node) {
	  /* FIXME: Improve. */
	  gdk_draw_rectangle (window, gc, FALSE,
			      x * full_cell_size + 1, y * full_cell_size + 1,
			      full_cell_size - 2, full_cell_size - 2);
	}

	if (main_tile != TILE_NONE) {
	  gdk_draw_pixbuf (window, gc,
			   view->base.main_tile_set->tiles[main_tile],
			   0, 0,
			   stones_left_margin + x * full_cell_size,
			   stones_top_margin + y * full_cell_size,
			   -1, -1, GDK_RGB_DITHER_NORMAL, 0, 0);
	}

	if (sgf_markup_tile != SGF_MARKUP_NONE) {
	  gint background = (sgf_node->move_color != SETUP_NODE
			     ? sgf_node->move_color : EMPTY);

	  gdk_draw_pixbuf (window, gc,
			   (view->base.sgf_markup_tile_set
			    ->tiles[sgf_markup_tile][background]),
			   0, 0,
			   sgf_markup_margin + x * full_cell_size,
			   sgf_markup_margin + y * full_cell_size,
			   -1, -1, GDK_RGB_DITHER_NORMAL, 0, 0);
	}
      }
    }

    gdk_gc_set_line_attributes (gc, 0, GDK_LINE_SOLID,
				GDK_CAP_BUTT, GDK_JOIN_MITER);
  }

  return FALSE;
}


static gboolean
gtk_sgf_tree_view_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  if (event->button == 1 || event->button == 3) {
    GtkSgfTreeView *view = GTK_SGF_TREE_VIEW (widget);
    gint full_cell_size = FULL_CELL_SIZE (view);
    gint x;
    gint y;

    gdk_window_get_pointer (view->output_window, &x, &y, NULL);
    x /= full_cell_size;
    y /= full_cell_size;

    if (view->button_pressed == 0) {
      view->button_pressed = event->button;
      view->press_x	   = x;
      view->press_y	   = y;
    }
    else
      view->button_pressed = -1;
  }

  return FALSE;
}


static gboolean
gtk_sgf_tree_view_button_release_event (GtkWidget *widget,
					GdkEventButton *event)
{
  if (event->button == 1 || event->button == 3) {
    GtkSgfTreeView *view = GTK_SGF_TREE_VIEW (widget);

    if (view->button_pressed == event->button) {
      gint full_cell_size = FULL_CELL_SIZE (view);
      gint x;
      gint y;

      gdk_window_get_pointer (view->output_window, &x, &y, NULL);
      x /= full_cell_size;
      y /= full_cell_size;

      if (x == view->press_x && y == view->press_y) {
	SgfNode *clicked_node = VIEW_PORT_NODE (view, x, y);

	if (clicked_node) {
	  g_signal_emit (G_OBJECT (view),
			 sgf_tree_view_signals[SGF_TREE_VIEW_CLICKED], 0,
			 clicked_node, event->button);
	}
      }
    }

    view->button_pressed = 0;
  }

  /* Make sure tooltips reappear now. */
  synthesize_enter_notify_event ((GdkEvent *) event);

  return FALSE;
}


static gboolean
gtk_sgf_tree_view_motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
  GtkSgfTreeView *view = GTK_SGF_TREE_VIEW (widget);
  gint full_cell_size = FULL_CELL_SIZE (view);
  const SgfNode *node = VIEW_PORT_NODE (view,
					(int) event->x / full_cell_size,
					(int) event->y / full_cell_size);

  if (node != view->last_tooltips_node) {
    /* FIXME: I would like some markup in the tooltip, e.g. boldened
     *	      node name.  However, this is not very important, let's
     *	      see if GTK+ provides advanced tooltips itself before
     *	      inventing our own.
     */

    GtkTooltips *tooltips = get_shared_tooltips ();
    StringBuffer tooltip_text;
    gboolean had_tooltip_set = (gtk_tooltips_data_get (widget) != NULL);

    string_buffer_init (&tooltip_text, 0x400, 0x200);

    if (node) {
      const char *node_name = sgf_node_get_text_property_value (node,
								SGF_NODE_NAME);
      const char *comment   = sgf_node_get_text_property_value (node,
								SGF_COMMENT);

      if (IS_STONE (node->move_color)) {
	int move_number = sgf_utils_get_node_move_number (node,
							  view->current_tree);

	string_buffer_printf (&tooltip_text, _("Move %d: "), move_number);
	sgf_utils_format_node_move (view->current_tree, node,
				    &tooltip_text, _("B "), _("W "), _("pass"));
      }

      if (node_name) {
	if (tooltip_text.length)
	  string_buffer_add_characters (&tooltip_text, '\n', 2);

	string_buffer_cat_string (&tooltip_text, _("Node name: "));
	append_limited_text (&tooltip_text, node_name, 80);
      }

      if (comment) {
	if (tooltip_text.length)
	  string_buffer_add_characters (&tooltip_text, '\n', 2);

	append_limited_text (&tooltip_text, comment, 400);
      }
    }

    gtk_tooltips_set_tip (tooltips, GTK_WIDGET (view),
			  (tooltip_text.length ? tooltip_text.string : NULL),
			  NULL);

    if (tooltip_text.length && !had_tooltip_set)
      synthesize_enter_notify_event ((GdkEvent *) event);

    string_buffer_dispose (&tooltip_text);
    view->last_tooltips_node = node;
  }

  return FALSE;
}


static void
gtk_sgf_tree_view_unrealize (GtkWidget *widget)
{
  GtkSgfTreeView *view = GTK_SGF_TREE_VIEW (widget);

  gdk_window_set_user_data (view->output_window, NULL);
  gdk_window_destroy (view->output_window);

  utils_free (view->view_port_nodes);
  utils_free (view->view_port_lines);
  utils_free (view->tile_map);
  utils_free (view->sgf_markup_tile_map);

  view->view_port_nodes	    = NULL;
  view->view_port_lines	    = NULL;
  view->tile_map	    = NULL;
  view->sgf_markup_tile_map = NULL;

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}


static void
gtk_sgf_tree_view_destroy (GtkObject *object)
{
  GtkSgfTreeView *view = GTK_SGF_TREE_VIEW (object);

  disconnect_adjustment (view, view->hadjustment);
  disconnect_adjustment (view, view->vadjustment);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


static void
gtk_sgf_tree_view_finalize (GObject *object)
{
  GtkSgfTreeView *view = GTK_SGF_TREE_VIEW (object);

  disconnect_adjustment (view, view->hadjustment);
  disconnect_adjustment (view, view->vadjustment);

  if (view->last_tooltips_node) {
    gtk_tooltips_set_tip (get_shared_tooltips (), GTK_WIDGET (view),
			  NULL, NULL);
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}




void
gtk_sgf_tree_view_set_sgf_tree (GtkSgfTreeView *view, SgfGameTree *sgf_tree)
{
  GObject *proxy;

  g_return_if_fail (GTK_IS_SGF_TREE_VIEW (view));
  g_return_if_fail (sgf_tree);

  if (view->current_tree) {
    GObject *old_proxy = GET_SIGNAL_PROXY (view->current_tree);

    g_signal_handlers_disconnect_by_func (old_proxy,
					  about_to_modify_map, view);
    g_signal_handlers_disconnect_by_func (old_proxy,
					  about_to_change_current_node, view);
    g_signal_handlers_disconnect_by_func (old_proxy,
					  current_node_changed, view);
    g_signal_handlers_disconnect_by_func (old_proxy, map_modified, view);
  }

  view->current_tree = sgf_tree;

  proxy = GET_SIGNAL_PROXY (sgf_tree);

  g_signal_connect_swapped (proxy, "about-to-modify-map",
			    G_CALLBACK (about_to_modify_map), view);
  g_signal_connect_swapped (proxy, "about-to-change-current-node",
			    G_CALLBACK (about_to_change_current_node), view);
  g_signal_connect_swapped (proxy, "current-node-changed",
			    G_CALLBACK (current_node_changed), view);
  g_signal_connect_swapped (proxy, "map-modified",
			    G_CALLBACK (map_modified), view);

  gtk_goban_base_set_game (GTK_GOBAN_BASE (view), sgf_tree->game);

  if (!view->hadjustment || !view->vadjustment)
    gtk_sgf_tree_view_set_scroll_adjustments (view, NULL, NULL);
}


void
gtk_sgf_tree_view_update_view_port (GtkSgfTreeView *view)
{
  g_return_if_fail (GTK_IS_SGF_TREE_VIEW (view));

  update_view_port (view);

  /* FIXME: Suboptimal. */
  gtk_widget_queue_draw (GTK_WIDGET (view));
}


void
gtk_sgf_tree_view_center_on_current_node (GtkSgfTreeView *view)
{
  gint view_x;
  gint view_y;

  g_return_if_fail (GTK_IS_SGF_TREE_VIEW (view));

  view_x = (gint) view->hadjustment->value;
  view_y = (gint) view->vadjustment->value;

  center_on_current_node (view);
  update_view_port_and_maybe_move_or_resize_window (view, view_x, view_y);
}


gboolean
gtk_sgf_tree_view_get_tooltips_enabled (void)
{
  return game_tree_view.show_tooltips;
}


void
gtk_sgf_tree_view_set_tooltips_enabled (gboolean enabled)
{
  game_tree_view.show_tooltips = enabled;

  if (enabled)
    gtk_tooltips_enable (get_shared_tooltips ());
  else
    gtk_tooltips_disable (get_shared_tooltips ());
}



static void
configure_adjustment (GtkSgfTreeView *view, gboolean horizontal,
		      gint original_value)
{
  GtkAdjustment *adjustment = (horizontal
			       ? view->hadjustment : view->vadjustment);
  GtkWidget *widget = GTK_WIDGET (view);
  gboolean adjustment_has_changed = FALSE;
  gboolean value_has_changed = (original_value != (gint) adjustment->value);
  gint page_size = (horizontal
		    ? widget->allocation.width : widget->allocation.height);
  gint full_cell_size = FULL_CELL_SIZE (view);
  gint upper = MAX (((horizontal ? view->map_width : view->map_height)
		     * full_cell_size),
		    page_size);

  if (adjustment->lower != 0.0) {
    adjustment->lower = 0.0;
    adjustment_has_changed = TRUE;
  }

  if ((gint) adjustment->upper != upper) {
    adjustment->upper = upper;
    adjustment_has_changed = TRUE;
  }

  if ((gint) adjustment->page_size != page_size) {
    adjustment->page_size = page_size;
    adjustment_has_changed = TRUE;
  }

  if ((gint) adjustment->page_increment != (gint) (page_size * 0.9)) {
    adjustment->page_increment = (gint) (page_size * 0.9);
    adjustment_has_changed = TRUE;
  }

  if ((gint) adjustment->step_increment != full_cell_size) {
    adjustment->step_increment = full_cell_size;
    adjustment_has_changed = TRUE;
  }

  if (adjustment->value < 0.0 || (upper - page_size) < adjustment->value) {
    adjustment->value = (adjustment->value < 0.0 ? 0.0 : upper - page_size);
    value_has_changed = TRUE;
  }

  if (adjustment_has_changed)
    gtk_adjustment_changed (adjustment);

  if (value_has_changed)
    gtk_adjustment_value_changed (adjustment);
}


static void
disconnect_adjustment (GtkSgfTreeView *view, GtkAdjustment *adjustment)
{
  if (adjustment) {
    g_signal_handlers_disconnect_by_func (adjustment,
					  scroll_adjustment_value_changed,
					  view);

    g_object_unref (adjustment);

    if (view->hadjustment == adjustment)
      view->hadjustment = NULL;
    else
      view->vadjustment = NULL;
  }
}


static void
scroll_adjustment_value_changed (GtkSgfTreeView *view)
{
  if (GTK_WIDGET_REALIZED (view) && !view->ignore_adjustment_changes) {
    gint old_x_value;
    gint old_y_value;
    gint new_x_value = - view->hadjustment->value;
    gint new_y_value = - view->vadjustment->value;

    gdk_window_get_position (view->output_window, &old_x_value, &old_y_value);

    if (old_x_value != new_x_value || old_y_value != new_y_value) {
      update_view_port (view);

      gdk_window_move (view->output_window, new_x_value, new_y_value);
      gdk_window_process_updates (view->output_window, FALSE);
    }
  }
}


/* See if we need to move the view port to keep the current node
 * within it, and if so, set adjustment values.
 */
static void
track_current_node (GtkSgfTreeView *view)
{
  gint view_x	      = (gint) view->hadjustment->value;
  gint view_y	      = (gint) view->vadjustment->value;
  gint view_width     = GTK_WIDGET (view)->allocation.width;
  gint view_height    = GTK_WIDGET (view)->allocation.height;
  gint full_cell_size = FULL_CELL_SIZE (view);
  gint current_node_x;
  gint current_node_y;

  if (!view->current_tree) {
    /* No tree to begin with, so nothing to track. */
    return;
  }

  if (!sgf_game_tree_get_node_coordinates (view->current_tree,
					   view->current_tree->current_node,
					   &current_node_x, &current_node_y)) {
    /* This means the current node is in a collapsed subtree.  Nothing
     * to track, return now.
     */
    return;
  }

  current_node_x *= full_cell_size;
  current_node_y *= full_cell_size;

  if (current_node_x < view_x
      || view_x + view_width - full_cell_size < current_node_x
      || (view_width > 4 * full_cell_size
	  && (current_node_x < view_x + full_cell_size
	      || (view_x + view_width - 2 * full_cell_size
		  < current_node_x)))
      || current_node_y < view_y
      || view_y + view_height - full_cell_size < current_node_y
      || (view_height > 4 * full_cell_size
	  && (current_node_y < view_y + full_cell_size
	      || (view_y + view_height - 2 * full_cell_size
		  < current_node_y)))) {
    if (game_tree_view.center_on_current_node)
      center_on_current_node (view);
    else {
      if (view_width > 4 * full_cell_size) {
	if (view_x > current_node_x - full_cell_size)
	  view->hadjustment->value = current_node_x - full_cell_size;
	else {
	  gint minimal_value = (((current_node_x - view_width
				  + (8 * full_cell_size) / 3)
				 / full_cell_size)
				* full_cell_size);

	  if (view_x < minimal_value)
	    view->hadjustment->value = minimal_value;
	}
      }
      else {
	if (view_x > current_node_x)
	  view->hadjustment->value = current_node_x;
	else {
	  gint minimal_value = (((current_node_x - view_width
				  + (5 * full_cell_size) / 3)
				 / full_cell_size)
				* full_cell_size);

	  if (view_x < minimal_value)
	    view->hadjustment->value = minimal_value;
	}
      }

      if (view_height > 4 * full_cell_size) {
	if (view_y > current_node_y - full_cell_size)
	  view->vadjustment->value = current_node_y - full_cell_size;
	else {
	  gint minimal_value = (((current_node_y - view_height
				  + (8 * full_cell_size) / 3)
				 / full_cell_size)
				* full_cell_size);

	  if (view_y < minimal_value)
	    view->vadjustment->value = minimal_value;
	}
      }
      else {
	if (view_y > current_node_y)
	  view->vadjustment->value = current_node_y;
	else {
	  gint minimal_value = (((current_node_y - view_height
				  + 2 * full_cell_size)
				 / full_cell_size)
				* full_cell_size);

	  if (view_y < minimal_value)
	    view->vadjustment->value = minimal_value;
	}
      }
    }

    update_view_port_and_maybe_move_or_resize_window (view, view_x, view_y);
  }
}


/* Center the view on current node.  This function only sets
 * adjustment values, but doesn't update view port---the caller is
 * supposed to do that.
 */
static void
center_on_current_node (GtkSgfTreeView *view)
{
  gint current_node_x;
  gint current_node_y;

  if (view->current_tree
      && sgf_game_tree_get_node_coordinates (view->current_tree,
					     view->current_tree->current_node,
					     &current_node_x,
					     &current_node_y)) {
    gint view_width     = GTK_WIDGET (view)->allocation.width;
    gint view_height    = GTK_WIDGET (view)->allocation.height;
    gint full_cell_size = FULL_CELL_SIZE (view);

    view->hadjustment->value
      = (((current_node_x * full_cell_size
	   - (2 * view_width - 3 * full_cell_size) / 4)
	  / full_cell_size)
	 * full_cell_size);
    view->vadjustment->value
      = (((current_node_y * full_cell_size
	   - (2 * view_height - 3 * full_cell_size) / 4)
	  / full_cell_size)
	 * full_cell_size);
  }
}


static void
update_view_port (GtkSgfTreeView *view)
{
  GtkAllocation *allocation = & GTK_WIDGET (view)->allocation;
  gint full_cell_size = FULL_CELL_SIZE (view);
  const SgfNode **view_port_scan;
  char *tile_map_scan;
  char *sgf_markup_tile_map_scan;
  int x;
  int y;

  view->view_port_x0 = (gint) view->hadjustment->value / full_cell_size;
  view->view_port_y0 = (gint) view->vadjustment->value / full_cell_size;
  view->view_port_x1 = (((gint) view->hadjustment->value + allocation->width
			 + full_cell_size - 1)
			/ full_cell_size);
  view->view_port_y1 = (((gint) view->vadjustment->value + allocation->height
			 + full_cell_size - 1)
			/ full_cell_size);

  utils_free (view->view_port_nodes);
  utils_free (view->view_port_lines);
  utils_free (view->tile_map);
  utils_free (view->sgf_markup_tile_map);

  if (view->current_tree) {
    sgf_game_tree_fill_map_view_port (view->current_tree,
				      view->view_port_x0, view->view_port_y0,
				      view->view_port_x1, view->view_port_y1,
				      &view->view_port_nodes,
				      &view->view_port_lines,
				      &view->num_view_port_lines);
    view->tile_map
      = sgf_game_tree_get_current_branch_marks (view->current_tree,
						view->view_port_x0,
						view->view_port_y0,
						view->view_port_x1,
						view->view_port_y1);
  }
  else {
    view->view_port_nodes = utils_malloc0 (((view->view_port_x1
					     - view->view_port_x0)
					    * (view->view_port_y1
					       - view->view_port_y0))
					   * sizeof (SgfNode *));
    view->view_port_lines     = NULL;
    view->num_view_port_lines = 0;
  }

  view->sgf_markup_tile_map = utils_malloc (((view->view_port_x1
					      - view->view_port_x0)
					     * (view->view_port_y1
						- view->view_port_y0))
					    * sizeof (char));

  for (view_port_scan = (const SgfNode **) view->view_port_nodes,
	 tile_map_scan = view->tile_map,
	 sgf_markup_tile_map_scan = view->sgf_markup_tile_map,
	 y = view->view_port_y0;
       y < view->view_port_y1; y++) {
    for (x = view->view_port_x0; x < view->view_port_x1;
	 view_port_scan++, tile_map_scan++, sgf_markup_tile_map_scan++, x++) {
      const SgfNode *sgf_node = *view_port_scan;

      if (sgf_node && IS_STONE (sgf_node->move_color)) {
	switch (*tile_map_scan) {
	case SGF_NON_CURRENT_BRANCH_NODE:
	  *tile_map_scan = (sgf_node->move_color == BLACK
			    ? BLACK_50_TRANSPARENT : WHITE_50_TRANSPARENT);
	  break;

	case SGF_CURRENT_BRANCH_HEAD_NODE:
	case SGF_CURRENT_NODE:
	case SGF_CURRENT_BRANCH_TAIL_NODE:
	  *tile_map_scan = (sgf_node->move_color == BLACK
			    ? BLACK_OPAQUE : WHITE_OPAQUE);
	  break;

	default:
	  g_critical ("unknown SGF map code %d", *tile_map_scan);
	  *tile_map_scan = TILE_NONE;
	}

	*sgf_markup_tile_map_scan = (sgf_node->is_collapsed
				     ? SGF_MARKUP_CROSS : SGF_MARKUP_NONE);
      }
      else {
	*tile_map_scan		  = TILE_NONE;
	*sgf_markup_tile_map_scan = SGF_MARKUP_NONE;
      }
    }
  }
}


static gboolean
update_view_port_and_maybe_move_or_resize_window
  (GtkSgfTreeView *view,
   gint original_hadjustment_value, gint original_vadjustment_value)
{
  gint original_hadjustment_upper = view->hadjustment->upper;
  gint original_vadjustment_upper = view->vadjustment->upper;
  gboolean need_to_resize_window;
  gboolean need_to_move_window;

  if (view->current_tree) {
    sgf_game_tree_get_map_dimensions (view->current_tree,
				      &view->map_width, &view->map_height);
  }
  else {
    view->map_width  = 1;
    view->map_height = 1;
  }

  view->ignore_adjustment_changes = TRUE;

  configure_adjustment (view, TRUE, original_hadjustment_value);
  configure_adjustment (view, FALSE, original_vadjustment_value);

  view->ignore_adjustment_changes = FALSE;

  need_to_resize_window = ((original_hadjustment_upper
			    != (gint) view->hadjustment->upper)
			   || (original_vadjustment_upper
			       != (gint) view->vadjustment->upper));
  need_to_move_window	= ((original_hadjustment_value
			    != (gint) view->hadjustment->value)
			   || (original_vadjustment_value
			       != (gint) view->vadjustment->value));

  if (GTK_WIDGET_REALIZED (view)) {
    update_view_port (view);

    if (need_to_resize_window || need_to_move_window) {
      if (need_to_resize_window) {
	if (need_to_move_window) {
	  gdk_window_move_resize (view->output_window,
				  - (gint) view->hadjustment->value,
				  - (gint) view->vadjustment->value,
				  (gint) view->hadjustment->upper,
				  (gint) view->vadjustment->upper);
	}
	else {
	  gdk_window_resize (view->output_window,
			     (gint) view->hadjustment->upper,
			     (gint) view->vadjustment->upper);
	}
      }
      else {
	gdk_window_move (view->output_window,
			 - (gint) view->hadjustment->value,
			 - (gint) view->vadjustment->value);
      }

      /* FIXME: Improve. */
      gtk_widget_queue_draw (GTK_WIDGET (view));

      gdk_window_process_updates (view->output_window, FALSE);

      return TRUE;
    }
  }

  return FALSE;
}


static void
about_to_modify_map (GtkSgfTreeView *view)
{
  if (GTK_WIDGET_REALIZED (view)) {
    view->expect_map_modification = TRUE;
    view->do_track_current_node	  = SHOULD_TRACK_CURRENT_NODE (view);
  }
}


static void
about_to_change_current_node (GtkSgfTreeView *view)
{
  if (GTK_WIDGET_REALIZED (view) && !view->expect_map_modification)
    view->do_track_current_node = SHOULD_TRACK_CURRENT_NODE (view);
}


static void
current_node_changed (GtkSgfTreeView *view)
{
  if (GTK_WIDGET_REALIZED (view)) {
    if (view->expect_map_modification)
      return;

    if (view->do_track_current_node) {
      gint view_x = (gint) view->hadjustment->value;
      gint view_y = (gint) view->vadjustment->value;

      track_current_node (view);

      if (!update_view_port_and_maybe_move_or_resize_window (view,
							     view_x, view_y)) {
	/* FIXME: Suboptimal. */
	gtk_widget_queue_draw (GTK_WIDGET (view));
      }
    }
    else {
      update_view_port (view);

      /* FIXME: Suboptimal. */
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }
  }
}


static void
map_modified (GtkSgfTreeView *view)
{
  if (GTK_WIDGET_REALIZED (view)) {
    gint view_x = (gint) view->hadjustment->value;
    gint view_y = (gint) view->vadjustment->value;

    if (view->do_track_current_node)
      track_current_node (view);

    if (!update_view_port_and_maybe_move_or_resize_window (view,
							   view_x, view_y)) {
      /* FIXME: Suboptimal. */
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }
  }

  view->expect_map_modification = FALSE;
}


/* Append not more than about `num_characters_limit' of `text' to the
 * given string `buffer'.  Note that `num_characters_limit' is
 * considered to be advisory only and more text can be appended still.
 */
static void
append_limited_text (StringBuffer *buffer, const char *text,
		     gint num_characters_limit)
{
  int k;
  const char *limit;
  const char *scan;

  for (k = 0, limit = text; k < num_characters_limit; k++) {
    if (!*limit) {
      /* All the text doesn't exceed the limit.  Append everything. */
      string_buffer_cat_as_string (buffer, text, limit - text);
      return;
    }

    limit = g_utf8_next_char (limit);
  }

  /* Let's not cut off 10% of text. */
  for (k = 0, scan = limit; k < num_characters_limit / 10; k++) {
    if (!*scan) {
      string_buffer_cat_as_string (buffer, text, scan - text);
      return;
    }

    scan = g_utf8_next_char (scan);
  }

  /* OK, so the text is quite long.  Now let's determine an
   * appropriate cutting point.  For now, we only uncoditionally skip
   * backward all whitespace-like and control-like characters.
   *
   * FIXME: Improve.  In particular, try to not cut words apart.
   *	    Maybe try using Pango for really good behavior...
   */
  for (scan = limit; scan > text; ) {
    GUnicodeType type;

    scan = g_utf8_prev_char (scan);
    type = g_unichar_type (g_utf8_get_char (scan));

    if (type	!= G_UNICODE_CONTROL
	&& type != G_UNICODE_CONTROL
	&& type != G_UNICODE_FORMAT
	&& type != G_UNICODE_UNASSIGNED
	&& type != G_UNICODE_PRIVATE_USE
	&& type != G_UNICODE_SURROGATE
	&& type != G_UNICODE_COMBINING_MARK
	&& type != G_UNICODE_ENCLOSING_MARK
	&& type != G_UNICODE_NON_SPACING_MARK
	&& type != G_UNICODE_LINE_SEPARATOR
	&& type != G_UNICODE_PARAGRAPH_SEPARATOR
	&& type != G_UNICODE_SPACE_SEPARATOR)
      break;
  }

  string_buffer_cat_as_string (buffer, text, g_utf8_next_char (scan) - text);

  /* Append ellipsis to indicate text truncation. */
  string_buffer_cat_string (buffer, "\342\200\246");
}


static GtkTooltips *
get_shared_tooltips (void)
{
  static GtkTooltips *shared_tooltips = NULL;

  if (!shared_tooltips) {
    shared_tooltips = gtk_tooltips_new ();
    g_object_ref (shared_tooltips);
    gtk_object_sink (GTK_OBJECT (shared_tooltips));

    gui_back_end_register_object_to_finalize (shared_tooltips);

    /* Enable or disable the tooltips, as appropriate. */
    gtk_sgf_tree_view_set_tooltips_enabled (game_tree_view.show_tooltips);
  }

  return shared_tooltips;
}


/* This is a workaround for GTK+ deficiency: if you set tooltip for
 * the widget under the pointer and it didn't have one before, the
 * tooltip won't appear.  We synthesize an `enter-notify' event so
 * that GtkTooltips will think the pointer has just entered the widget
 * window.  This function is also called after clicks on the widget.
 */
static void
synthesize_enter_notify_event (GdkEvent *event)
{
  GdkEventCrossing synthesized_event;

  synthesized_event.type       = GDK_ENTER_NOTIFY;
  synthesized_event.window     = event->any.window;
  synthesized_event.send_event = TRUE;
  synthesized_event.subwindow  = event->any.window;
  synthesized_event.time       = gdk_event_get_time (event);
  synthesized_event.mode       = GDK_CROSSING_NORMAL;
  synthesized_event.detail     = GDK_NOTIFY_UNKNOWN;
  synthesized_event.focus      = FALSE;

  gdk_event_get_coords (event, &synthesized_event.x, &synthesized_event.y);
  gdk_event_get_root_coords (event,
			     &synthesized_event.x_root,
			     &synthesized_event.y_root);
  gdk_event_get_state (event, &synthesized_event.state);

  /* Note: this works better than gtk_main_do_event().  Apparently,
   * GtkTooltips won't show tooltips when a button is still pressed
   * and we call this function from
   * gtk_sgf_tree_view_button_release_event() too.
   */
  gdk_event_put ((GdkEvent *) &synthesized_event);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
