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


#include "gtk-configuration.h"
#include "gtk-goban.h"
#include "gtk-goban-base.h"
#include "gtk-tile-set.h"
#include "gtk-utils.h"
#include "quarry-marshal.h"
#include "sgf.h"
#include "board.h"
#include "game-info.h"
#include "utils.h"

#include <assert.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>
#include <stdio.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


#define MODIFIER_MASK		(GDK_SHIFT_MASK		\
				 | GDK_CONTROL_MASK	\
				 | GDK_MOD1_MASK	\
				 | GDK_BUTTON1_MASK	\
				 | GDK_BUTTON3_MASK)


enum {
  NO_CHECKERBOARD_PATTERN,
  CHECKERBOARD_PATTERN_WITH_PIXBUF,
  CHECKERBOARD_PATTERN_WITH_PIXMAP
};


typedef struct _GtkGobanMargins		GtkGobanMargins;

struct _GtkGobanMargins {
  gint		stones_left_margin;
  gint		stones_top_margin;
  gint		small_stones_left_margin;
  gint		small_stones_top_margin;
  gint		sgf_markup_left_margin;
  gint		sgf_markup_top_margin;
};


static void	 gtk_goban_class_init (GtkGobanClass *class);
static void	 gtk_goban_init (GtkGoban *goban);
static void	 gtk_goban_realize (GtkWidget *widget);

static void	 gtk_goban_size_request (GtkWidget *widget,
					 GtkRequisition *requisition);
static void	 gtk_goban_size_allocate (GtkWidget *widget,
					  GtkAllocation *allocation);

static gboolean	 gtk_goban_expose (GtkWidget *widget,
				   GdkEventExpose *event);
inline static void
		 draw_vertical_line_with_gaps (GtkGoban *goban,
					       GdkWindow *window, GdkGC *gc,
					       int x, gint window_x,
					       gint extra_length);
inline static void
		 draw_horizontal_line_with_gaps (GtkGoban *goban,
						 GdkWindow *window, GdkGC *gc,
						 int y, gint window_y,
						 gint extra_length);

static gboolean	 gtk_goban_button_press_event (GtkWidget *widget,
					       GdkEventButton *event);
static gboolean	 gtk_goban_button_release_event (GtkWidget *widget,
						 GdkEventButton *event);
static gboolean	 gtk_goban_motion_notify_event (GtkWidget *widget,
						GdkEventMotion *event);
static gboolean	 gtk_goban_enter_notify_event (GtkWidget *widget,
					       GdkEventCrossing *event);
static gboolean	 gtk_goban_leave_notify_event (GtkWidget *widget,
					       GdkEventCrossing *event);
static gboolean	 gtk_goban_scroll_event (GtkWidget *widget,
					 GdkEventScroll *event);

static gboolean	 gtk_goban_key_press_event (GtkWidget *widget,
					    GdkEventKey *event);
static gboolean	 gtk_goban_key_release_event (GtkWidget *widget,
					      GdkEventKey *event);

static gboolean	 gtk_goban_focus_in_or_out_event (GtkWidget *widget,
						  GdkEventFocus *event);

static void	 gtk_goban_allocate_screen_resources
		   (GtkGobanBase *goban_base);
static void	 gtk_goban_free_screen_resources (GtkGobanBase *goban_base);

static void	 gtk_goban_finalize (GObject *object);

static void	 emit_pointer_moved (GtkGoban *goban, int x, int y,
				     GdkModifierType modifiers);
static void	 set_feedback_data (GtkGoban *goban, int x, int y,
				    BoardPositionList *position_list,
				    GtkGobanPointerFeedback feedback);

static void	 set_overlay_data (GtkGoban *goban, int overlay_index,
				   BoardPositionList *position_list,
				   int tile, int goban_markup_tile);

static void	 compute_goban_margins (const GtkGoban *goban,
					GtkGobanMargins *margins);
static void	 widget_coordinates_to_board (const GtkGoban *goban,
					      int window_x, int window_y,
					      int *board_x, int *board_y);


static GtkGobanBaseClass  *parent_class;


enum {
  POINTER_MOVED,
  GOBAN_CLICKED,
  NAVIGATE,
  NUM_SIGNALS
};

static guint		   goban_signals[NUM_SIGNALS];


GType
gtk_goban_get_type (void)
{
  static GType goban_type = 0;

  if (!goban_type) {
    static GTypeInfo goban_info = {
      sizeof (GtkGobanClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_goban_class_init,
      NULL,
      NULL,
      sizeof (GtkGoban),
      2,
      (GInstanceInitFunc) gtk_goban_init,
      NULL
    };

    goban_type = g_type_register_static (GTK_TYPE_GOBAN_BASE, "GtkGoban",
					 &goban_info, 0);
  }

  return goban_type;
}


static void
gtk_goban_class_init (GtkGobanClass *class)
{
  static GtkUtilsBindingInfo navigation_bindings[] = {
    {GDK_Left,		0,	GOBAN_NAVIGATE_BACK},
    {GDK_Page_Up,	0,	GOBAN_NAVIGATE_BACK_FAST},
    {GDK_Right,		0,	GOBAN_NAVIGATE_FORWARD},
    {GDK_Page_Down,	0,	GOBAN_NAVIGATE_FORWARD_FAST},
    {GDK_Up,		0,	GOBAN_NAVIGATE_PREVIOUS_VARIATION},
    {GDK_Down,		0,	GOBAN_NAVIGATE_NEXT_VARIATION},
    {GDK_Home,		0,	GOBAN_NAVIGATE_ROOT},
    {GDK_End,		0,	GOBAN_NAVIGATE_VARIATION_END}
  };

  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkGobanBaseClass *base_class = GTK_GOBAN_BASE_CLASS (class);
  GtkBindingSet *binding_set;

  parent_class = g_type_class_peek_parent (class);

  G_OBJECT_CLASS (class)->finalize = gtk_goban_finalize;

  widget_class->realize		     = gtk_goban_realize;
  widget_class->size_request	     = gtk_goban_size_request;
  widget_class->size_allocate	     = gtk_goban_size_allocate;
  widget_class->expose_event	     = gtk_goban_expose;
  widget_class->button_press_event   = gtk_goban_button_press_event;
  widget_class->button_release_event = gtk_goban_button_release_event;
  widget_class->motion_notify_event  = gtk_goban_motion_notify_event;
  widget_class->enter_notify_event   = gtk_goban_enter_notify_event;
  widget_class->leave_notify_event   = gtk_goban_leave_notify_event;
  widget_class->scroll_event	     = gtk_goban_scroll_event;
  widget_class->key_press_event	     = gtk_goban_key_press_event;
  widget_class->key_release_event    = gtk_goban_key_release_event;
  widget_class->focus_in_event	     = gtk_goban_focus_in_or_out_event;
  widget_class->focus_out_event	     = gtk_goban_focus_in_or_out_event;

  base_class->allocate_screen_resources = gtk_goban_allocate_screen_resources;
  base_class->free_screen_resources	= gtk_goban_free_screen_resources;

  class->pointer_moved = NULL;
  class->goban_clicked = NULL;
  class->navigate      = NULL;

  goban_signals[POINTER_MOVED]
    = g_signal_new ("pointer-moved",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkGobanClass, pointer_moved),
		    NULL, NULL,
		    quarry_marshal_INT__POINTER,
		    G_TYPE_INT, 1, G_TYPE_POINTER);

  goban_signals[GOBAN_CLICKED]
    = g_signal_new ("goban-clicked",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkGobanClass, pointer_moved),
		    NULL, NULL,
		    quarry_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1, G_TYPE_POINTER);

  goban_signals[NAVIGATE]
    = g_signal_new ("navigate",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		    G_STRUCT_OFFSET (GtkGobanClass, navigate),
		    NULL, NULL,
		    quarry_marshal_VOID__INT,
		    G_TYPE_NONE, 1, G_TYPE_INT);

  binding_set = gtk_binding_set_by_class (class);
  gtk_utils_add_similar_bindings (binding_set, "navigate", navigation_bindings,
				  (sizeof navigation_bindings
				   / sizeof (GtkUtilsBindingInfo)));
}


static void
gtk_goban_init (GtkGoban *goban)
{
  int k;

  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (goban), GTK_CAN_FOCUS);

  goban->width	= 0;
  goban->height	= 0;

  goban->font_size = 0;

  goban->checkerboard_pattern_object = NULL;

  goban->last_move_pos = NULL_POSITION;

  for (k = 0; k < NUM_OVERLAYS; k++) {
    goban->overlay_positon_lists[k] = NULL;
    goban->overlay_contents[k] = NULL;
  }

  set_feedback_data (goban, NULL_X, NULL_Y, NULL, GOBAN_FEEDBACK_NONE);
}


GtkWidget *
gtk_goban_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_GOBAN, NULL));
}


void
gtk_goban_set_parameters (GtkGoban *goban, Game game, int width, int height)
{
  assert (GTK_IS_GOBAN (goban));
  assert (GAME_IS_SUPPORTED (game));
  assert (width > 0 && height > 0);

  if (goban->width && goban->height) {
    int x;
    int y;

    for (y = 0; y < goban->height; y++) {
      for (x = 0; x < goban->width; x++)
	g_free (goban->sgf_labels[POSITION (x, y)]);
    }
  }

  goban->width	= width;
  goban->height = height;

  grid_fill (goban->grid, goban->width, goban->height, TILE_NONE);
  grid_fill (goban->goban_markup, goban->width, goban->height,
	     SGF_MARKUP_NONE);
  grid_fill (goban->sgf_markup, goban->width, goban->height, TILE_NONE);
  pointer_grid_fill ((void **) goban->sgf_labels, goban->width, goban->height,
		     NULL);

  if (game == GAME_GO) {
    goban->num_hoshi_points = go_get_hoshi_points (width, height,
						   goban->hoshi_points);
  }
  else
    goban->num_hoshi_points = 0;

  gtk_goban_base_set_game (GTK_GOBAN_BASE (goban), game);
}


static void
gtk_goban_realize (GtkWidget *widget)
{
  GtkGoban *goban = GTK_GOBAN (widget);
  GdkWindowAttr attributes;
  const gint attributes_mask = (GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL
				| GDK_WA_COLORMAP);
  const gint event_mask = (GDK_EXPOSURE_MASK
			   | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			   | GDK_POINTER_MOTION_MASK
			   | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK
			   | GDK_SCROLL_MASK
			   | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.event_mask	 = gtk_widget_get_events (widget) | event_mask;
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
  gdk_window_set_user_data (widget->window, goban);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  GTK_WIDGET_CLASS (parent_class)->realize (widget);
}


/* FIXME: write this function. */
static void
gtk_goban_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  UNUSED (widget);

  requisition->width = 500;
  requisition->height = 500;
}


static void
gtk_goban_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GtkGoban *goban = GTK_GOBAN (widget);
  gint cell_size;
  Game game = goban->base.game;
  int grid_width = goban->width - (game == GAME_GO ? 1 : 0);
  int grid_height = goban->height - (game == GAME_GO ? 1 : 0);
  int font_size;
  float vertical_gap = (game == GAME_GO ? 0.65 : 0.8);
  float horizontal_gap = (goban->height < 10 && game == GAME_GO ? 0.65 : 0.8);
  int horizontal_deficit;
  int horizontal_gap_pixels;
  int vertical_padding;

  assert (goban->width != 0 && goban->height != 0);

  cell_size = floor (MIN (allocation->width / (goban->width
					       + 2 * horizontal_gap),
			  allocation->height / (goban->height
						+ 2 * vertical_gap)));

  font_size = (3 * cell_size) / 7;
  if (font_size != goban->font_size) {
    PangoLayout *layout = gtk_widget_create_pango_layout (widget, "8");

    goban->font_size = font_size;

    pango_font_description_set_size (goban->base.font_description,
				     font_size * PANGO_SCALE);
    pango_layout_set_font_description (layout, goban->base.font_description);
    pango_layout_get_pixel_size (layout,
				 &goban->digit_width,
				 &goban->character_height);
    goban->digit_width += 2;

    g_object_unref (layout);
  }

  horizontal_deficit = (allocation->width - goban->width * cell_size
			- (goban->height < 10 ? 2 : 4) * goban->digit_width);
  if (horizontal_deficit < 0)
    cell_size -= (-horizontal_deficit + goban->width - 1) / goban->width;

  gtk_goban_base_set_cell_size (&goban->base, cell_size);

  goban->left_margin = (allocation->width - grid_width * cell_size) / 2;
  goban->right_margin = goban->left_margin + grid_width * cell_size;

  goban->top_margin = (allocation->height - grid_height * cell_size) / 2;
  goban->bottom_margin = goban->top_margin + grid_height * cell_size;

  goban->first_cell_center_x = (allocation->width
				- (goban->width - 1) * cell_size) / 2;
  goban->first_cell_center_y = (allocation->height
				- (goban->height - 1) * cell_size) / 2;

  horizontal_gap_pixels = MAX (horizontal_gap * cell_size,
			       ((goban->height < 10 ? 1 : 2)
				* goban->digit_width));
  vertical_padding  = (vertical_gap * cell_size - goban->character_height) / 2;

  goban->coordinates_x_left = (goban->first_cell_center_x - cell_size / 2
			       - horizontal_gap_pixels / 2 - 1);
  goban->coordinates_x_right = (goban->coordinates_x_left
				+ goban->width * cell_size
				+ horizontal_gap_pixels - 1);
  goban->coordinates_y_side = (goban->first_cell_center_y
			       - goban->character_height / 2);

  goban->coordinates_y_top = (goban->first_cell_center_y
			      - (0.5 + vertical_gap) * cell_size
			      + vertical_padding);
  goban->coordinates_y_bottom = (goban->first_cell_center_y
				 + (-0.5 + goban->height) * cell_size
				 + vertical_padding);

  goban->button_pressed = 0;

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  if (GTK_WIDGET_REALIZED (widget)) {
    int x;
    int y;
    GdkModifierType modifiers;

    gdk_window_get_pointer (widget->window, &x, &y, &modifiers);
    widget_coordinates_to_board (goban, x, y, &x, &y);

    if (x != goban->pointer_x || y != goban->pointer_y
	|| (modifiers & MODIFIER_MASK) != goban->modifiers)
      emit_pointer_moved (goban, x, y, modifiers & MODIFIER_MASK);
  }
}


static gboolean
gtk_goban_expose (GtkWidget *widget, GdkEventExpose *event)
{
  int k;
  int x;
  int y;
  gint window_x;
  gint window_y;
  gint clip_left_margin	  = event->area.x;
  gint clip_right_margin  = event->area.x + event->area.width;
  gint clip_top_margin	  = event->area.y;
  gint clip_bottom_margin = event->area.y + event->area.height;
  GtkGobanMargins margins;
  gint lower_limit;
  gint upper_limit;
  GdkRectangle row_rectangle;
  GdkRectangle cell_rectangle;
  PangoLayout *layout = NULL;

  GtkGoban *goban = GTK_GOBAN (widget);
  int cell_size = goban->base.cell_size;
  gint checkerboard_pattern_mode;

  GdkWindow *window = widget->window;
  GdkGC *gc = widget->style->fg_gc[GTK_STATE_NORMAL];

  if (!GTK_WIDGET_DRAWABLE (widget))
    return FALSE;

  if (GTK_WIDGET_HAS_FOCUS (widget)) {
    /* Draw a small rectangle to indicate focus. */
    gdk_draw_rectangle (window, gc, FALSE,
			goban->coordinates_x_left - 1,
			(goban->coordinates_y_top + goban->character_height / 2
			 - 4),
			7, 7);
  }

  if (goban->base.game == GAME_GO) {
    /* Draw grid lines. */

    for (x = MAX (0, clip_left_margin - goban->left_margin) / cell_size,
	   window_x = goban->left_margin + x * cell_size;
	 window_x <= goban->right_margin && window_x < clip_right_margin;
	 x++, window_x += cell_size)
      draw_vertical_line_with_gaps (goban, window, gc, x, window_x, 0);

    for (y = MAX (0, clip_top_margin - goban->top_margin) / cell_size,
	   window_y = goban->top_margin + y * cell_size;
	 window_y <= goban->bottom_margin && window_y < clip_bottom_margin;
	 y++, window_y += cell_size)
      draw_horizontal_line_with_gaps (goban, window, gc, y, window_y, 0);

    /* Make the outmost grid lines thicker. */

    if (clip_left_margin <= goban->left_margin - 1) {
      draw_vertical_line_with_gaps (goban, window, gc,
				    0, goban->left_margin - 1, 1);
    }

    if (clip_right_margin > goban->right_margin + 1) {
      draw_vertical_line_with_gaps (goban, window, gc,
				    goban->width - 1,
				    goban->right_margin + 1, 1);
    }

    if (clip_top_margin <= goban->top_margin - 1) {
      draw_horizontal_line_with_gaps (goban, window, gc,
				      0, goban->top_margin - 1, 1);
    }

    if (clip_bottom_margin > goban->bottom_margin + 1) {
      draw_horizontal_line_with_gaps (goban, window, gc,
				      goban->height - 1,
				      goban->bottom_margin + 1, 1);
    }
  }
  else {
    /* Draw grid lines. */

    for (window_x = (goban->left_margin
		     + ((MAX (0, clip_left_margin - goban->left_margin)
			 / cell_size)
			* cell_size));
	 window_x <= goban->right_margin && window_x < clip_right_margin;
	 window_x += cell_size) {
      gdk_draw_line (window, gc,
		     window_x, goban->top_margin,
		     window_x, goban->bottom_margin);
    }

    for (window_y = (goban->top_margin
		     + ((MAX (0, clip_top_margin - goban->top_margin)
			 / cell_size)
			* cell_size));
	 window_y <= goban->bottom_margin && window_y < clip_bottom_margin;
	 window_y += cell_size) {
      gdk_draw_line (window, gc,
		     goban->left_margin, window_y,
		     goban->right_margin, window_y);
    }

    /* Make the outmost grid lines thicker. */

    if (clip_left_margin <= goban->left_margin - 1) {
      gdk_draw_line (window, gc,
		     goban->left_margin - 1, goban->top_margin,
		     goban->left_margin - 1, goban->bottom_margin);
    }

    if (clip_right_margin > goban->right_margin + 1) {
      gdk_draw_line (window, gc,
		     goban->right_margin + 1, goban->top_margin,
		     goban->right_margin + 1, goban->bottom_margin);
    }

    if (clip_top_margin <= goban->top_margin - 1) {
      gdk_draw_line (window, gc,
		     goban->left_margin - 1, goban->top_margin - 1,
		     goban->right_margin + 1, goban->top_margin - 1);
    }

    if (clip_bottom_margin > goban->bottom_margin + 1) {
      gdk_draw_line (window, gc,
		     goban->left_margin - 1, goban->bottom_margin + 1,
		     goban->right_margin + 1, goban->bottom_margin + 1);
    }
  }

  /* Draw coordinate labels. */
  if (clip_left_margin < goban->left_margin
      || clip_right_margin > goban->right_margin
      || clip_top_margin < goban->top_margin
      || clip_bottom_margin > goban->bottom_margin) {
    const char *horizontal_coordinates
      = game_info[goban->base.game].horizontal_coordinates;

    layout = gtk_widget_create_pango_layout (widget, NULL);
    pango_layout_set_font_description (layout, goban->base.font_description);
    pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
    pango_layout_set_width (layout, 2 * cell_size);

    /* Left and right label columns. */
    if (clip_left_margin < goban->left_margin
	|| clip_right_margin > goban->right_margin) {
      lower_limit = MAX (0,
			 ((clip_top_margin - goban->coordinates_y_side)
			  / cell_size));
      upper_limit = MIN (goban->height,
			 (1 + ((clip_bottom_margin - goban->coordinates_y_side)
			       / cell_size)));

      for (k = lower_limit; k < upper_limit; k++) {
	char buffer[4];

	sprintf (buffer, "%d",
		 (game_info[goban->base.game].reversed_vertical_coordinates
		  ? goban->height - k : k + 1));
	pango_layout_set_text (layout, buffer, -1);

	if (clip_left_margin < goban->left_margin) {
	  gdk_draw_layout (widget->window, gc,
			   goban->coordinates_x_left,
			   goban->coordinates_y_side + k * cell_size,
			   layout);
	}

	if (clip_right_margin > goban->right_margin) {
	  gdk_draw_layout (widget->window, gc,
			   goban->coordinates_x_right,
			   goban->coordinates_y_side + k * cell_size,
			   layout);
	}
      }
    }

    /* Top and bottom label rows. */
    if (clip_top_margin < goban->top_margin
	|| clip_bottom_margin > goban->bottom_margin) {
      lower_limit = MAX (0,
			 ((clip_left_margin
			   - (goban->first_cell_center_x - cell_size / 2))
			  / cell_size));
      upper_limit = MIN (goban->width,
			 (1 + ((clip_right_margin
				- (goban->first_cell_center_x - cell_size / 2))
			       / cell_size)));

      for (k = lower_limit; k < upper_limit; k++) {
	pango_layout_set_text (layout, horizontal_coordinates + k, 1);

	if (clip_top_margin < goban->top_margin) {
	  gdk_draw_layout (widget->window, gc,
			   goban->first_cell_center_x + k * cell_size,
			   goban->coordinates_y_top,
			   layout);
	}

	if (clip_bottom_margin > goban->bottom_margin) {
	  gdk_draw_layout (widget->window, gc,
			   goban->first_cell_center_x + k * cell_size,
			   goban->coordinates_y_bottom,
			   layout);
	}
      }
    }
  }

  /* FIXME: Not clipped now.  Needs a rewrite anyway. */
  for (k = 0; k < goban->num_hoshi_points; k++) {
    if (!goban->sgf_labels[POINT_TO_POSITION (goban->hoshi_points[k])]) {
      window_x = goban->left_margin + goban->hoshi_points[k].x * cell_size;
      window_y = goban->top_margin + goban->hoshi_points[k].y * cell_size;

      gdk_draw_rectangle (window, gc, FALSE, window_x - 2, window_y - 1, 4, 2);
      gdk_draw_rectangle (window, gc, FALSE, window_x - 1, window_y - 2, 2, 4);
    }
  }

  if (goban->checkerboard_pattern_object) {
    if (GDK_IS_PIXBUF (goban->checkerboard_pattern_object))
      checkerboard_pattern_mode = CHECKERBOARD_PATTERN_WITH_PIXBUF;
    else {
      assert (GDK_IS_PIXMAP (goban->checkerboard_pattern_object));
      checkerboard_pattern_mode = CHECKERBOARD_PATTERN_WITH_PIXMAP;
    }
  }
  else
    checkerboard_pattern_mode = NO_CHECKERBOARD_PATTERN;

  compute_goban_margins (goban, &margins);

  row_rectangle.x = event->area.x;
  row_rectangle.width = event->area.width;

  row_rectangle.y = (checkerboard_pattern_mode == NO_CHECKERBOARD_PATTERN
		     ? margins.stones_top_margin
		     : MIN (margins.stones_top_margin, goban->top_margin + 1));

  if (checkerboard_pattern_mode == NO_CHECKERBOARD_PATTERN)
    row_rectangle.height = cell_size;
  else {
    row_rectangle.height = (MAX ((margins.stones_top_margin
				  + goban->base.main_tile_set->tile_size),
				 goban->top_margin + 1 + cell_size)
			    - row_rectangle.y);
  }

  cell_rectangle.width = row_rectangle.height;
  cell_rectangle.height = row_rectangle.height;

  lower_limit = MAX (0, (clip_top_margin - row_rectangle.y) / cell_size);
  upper_limit = MIN (goban->height,
		     (1 + ((clip_bottom_margin
			    - (row_rectangle.y
			       + (row_rectangle.height - cell_size)))
			   / cell_size)));

  for (y = lower_limit, row_rectangle.y += lower_limit * cell_size;
       y < upper_limit; y++, row_rectangle.y += cell_size) {
    GdkRegion *row_region = gdk_region_rectangle (&row_rectangle);

    gdk_region_intersect (row_region, event->region);
    if (!gdk_region_empty (row_region)) {
      cell_rectangle.x = (checkerboard_pattern_mode == NO_CHECKERBOARD_PATTERN
			  ? margins.stones_left_margin
			  : MIN (margins.stones_left_margin,
				 goban->left_margin + 1));
      cell_rectangle.y = row_rectangle.y;

      for (x = 0; x < goban->width; x++, cell_rectangle.x += cell_size) {
	if (gdk_region_rect_in (row_region, &cell_rectangle)
	    != GDK_OVERLAP_RECTANGLE_OUT) {
	  int pos	      = POSITION (x, y);
	  int tile	      = goban->grid[pos];
	  int markup_tile     = (goban->goban_markup[pos]
				 & GOBAN_MARKUP_TILE_MASK);
	  int sgf_markup_tile = (pos != goban->last_move_pos
				 ? goban->sgf_markup[pos]
				 : SGF_PSEUDO_MARKUP_LAST_MOVE);

	  if (checkerboard_pattern_mode != NO_CHECKERBOARD_PATTERN
	      && (x + y) % 2 == 1) {
	    if (checkerboard_pattern_mode
		== CHECKERBOARD_PATTERN_WITH_PIXBUF) {
	      gdk_draw_pixbuf (window, gc,
			       ((GdkPixbuf *)
				goban->checkerboard_pattern_object),
			       0, 0,
			       goban->left_margin + 1 + x * cell_size,
			       goban->top_margin + 1 + y * cell_size,
			       -1, -1,
			       GDK_RGB_DITHER_NORMAL, 0, 0);
	    }
	    else {
	      gdk_draw_drawable (window, gc,
				 ((GdkDrawable *)
				  goban->checkerboard_pattern_object),
				 0, 0,
				 goban->left_margin + 1 + x * cell_size,
				 goban->top_margin + 1 + y * cell_size,
				 -1, -1);
	    }
	  }

	  if (tile != TILE_NONE && tile != TILE_SPECIAL) {
	    if (IS_STONE (tile)) {
	      if (goban->goban_markup[pos] & GOBAN_MARKUP_GHOSTIFY)
		tile = STONE_50_TRANSPARENT + COLOR_INDEX (tile);
	      else if (goban->goban_markup[pos]
		       & GOBAN_MARKUP_GHOSTIFY_SLIGHTLY)
		tile = STONE_25_TRANSPARENT + COLOR_INDEX (tile);
	    }

	    gdk_draw_pixbuf (window, gc,
			     goban->base.main_tile_set->tiles[tile],
			     0, 0,
			     margins.stones_left_margin + x * cell_size,
			     margins.stones_top_margin + y * cell_size,
			     -1, -1,
			     GDK_RGB_DITHER_NORMAL, 0, 0);
	  }
	  else if (tile == TILE_SPECIAL) {
	    int base_x = goban->left_margin + x * cell_size;
	    int base_y = goban->top_margin + y * cell_size;

	    gdk_draw_line (window, gc,
			   base_x, base_y,
			   base_x + cell_size, base_y + cell_size);
	    gdk_draw_line (window, gc,
			   base_x, base_y + cell_size,
			   base_x + cell_size, base_y);
	  }

	  if (markup_tile != TILE_NONE) {
	    assert (markup_tile != TILE_SPECIAL);

	    gdk_draw_pixbuf (window, gc,
			     goban->small_tile_set->tiles[markup_tile],
			     0, 0,
			     margins.small_stones_left_margin + x * cell_size,
			     margins.small_stones_top_margin + y * cell_size,
			     -1, -1,
			     GDK_RGB_DITHER_NORMAL, 0, 0);
	  }

	  if (sgf_markup_tile != SGF_MARKUP_NONE) {
	    gint background = (IS_STONE (goban->grid[pos])
			       ? goban->grid[pos] : EMPTY);
	    GdkPixbuf *pixbuf = (goban->base.sgf_markup_tile_set
				 ->tiles[sgf_markup_tile][background]);

	    gdk_draw_pixbuf (window, gc, pixbuf, 0, 0,
			     margins.sgf_markup_left_margin + x * cell_size,
			     margins.sgf_markup_top_margin + y * cell_size,
			     -1, -1,
			     GDK_RGB_DITHER_NORMAL, 0, 0);
	  }

	  if (goban->sgf_labels[pos]) {
	    if (!layout) {
	      layout = gtk_widget_create_pango_layout (widget, NULL);
	      pango_layout_set_font_description (layout,
						 goban->base.font_description);
	      pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	      pango_layout_set_width (layout, cell_size - 2);
	    }

	    pango_layout_set_text (layout, goban->sgf_labels[pos], -1);
	    gdk_draw_layout (widget->window,
			     (goban->grid[pos] != BLACK
			      ? widget->style->black_gc
			      : widget->style->white_gc),
			     goban->first_cell_center_x + x * cell_size,
			     (goban->first_cell_center_y
			      - goban->character_height / 2
			      + y * cell_size),
			     layout);
	  }
	}
      }
    }

    gdk_region_destroy (row_region);
  }

  if (layout)
    g_object_unref (layout);

  return FALSE;
}


/* Draw a vertical line, leaving gaps in places where there are labels
 * (to avoid cluttering them.)
 */
inline static void
draw_vertical_line_with_gaps (GtkGoban *goban, GdkWindow *window, GdkGC *gc,
			      int x, gint window_x, gint extra_length)
{
  gint gap_half = 2 + (goban->character_height * 5) / 9;
  gint top = goban->top_margin - extra_length;
  int y;
  gint window_y;

  for (y = 0, window_y = goban->first_cell_center_y; y < goban->height;
       y++, window_y += goban->base.cell_size) {
    if (goban->sgf_labels[POSITION (x, y)]) {
      if (window_y - gap_half > top) {
	gdk_draw_line (window, gc,
		       window_x, top, window_x, window_y - gap_half);
      }

      top = window_y + gap_half;
    }
  }

  if (goban->bottom_margin > top) {
    gdk_draw_line (window, gc,
		   window_x, top,
		   window_x, goban->bottom_margin + extra_length);
  }
}


/* Just as draw_vertical_line_with_gaps(), but draws horizontally. */
inline static void
draw_horizontal_line_with_gaps (GtkGoban *goban, GdkWindow *window, GdkGC *gc,
				int y, gint window_y, gint extra_length)
{
  gint gap_half = (goban->base.cell_size - 1) / 2;
  gint left = goban->left_margin - extra_length;
  int x;
  gint window_x;

  for (x = 0, window_x = goban->first_cell_center_x; x < goban->width;
       x++, window_x += goban->base.cell_size) {
    if (goban->sgf_labels[POSITION (x, y)]) {
      if (window_x - gap_half > left) {
	gdk_draw_line (window, gc,
		       left, window_y, window_x - gap_half, window_y);
      }

      left = window_x + gap_half;
    }
  }

  if (goban->right_margin > left) {
    gdk_draw_line (window, gc,
		   left, window_y,
		   goban->right_margin + extra_length, window_y);
  }
}


static gboolean
gtk_goban_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  if (event->button == 1 || event->button == 3) {
    GtkGoban *goban = GTK_GOBAN (widget);
    int x;
    int y;
    GdkModifierType modifiers;

    gdk_window_get_pointer (widget->window, &x, &y, &modifiers);
    widget_coordinates_to_board (goban, x, y, &x, &y);

    if (!IS_NULL_POINT (x, y)) {
      if (goban->button_pressed == 0) {
	goban->button_pressed	      = event->button;
	goban->press_x		      = x;
	goban->press_y		      = y;
	goban->press_modifiers	      = modifiers & MODIFIER_MASK;
	goban->feedback_tile_at_press = goban->feedback_tile;
      }
      else
	goban->button_pressed = -1;

      emit_pointer_moved (goban, x, y, modifiers & MODIFIER_MASK);
    }
    else if (goban->button_pressed == 0 && !GTK_WIDGET_HAS_FOCUS (widget))
      gtk_widget_grab_focus (widget);
  }

  return FALSE;
}


static gboolean
gtk_goban_button_release_event (GtkWidget *widget, GdkEventButton *event)
{
  if (event->button == 1 || event->button == 3) {
    GtkGoban *goban = GTK_GOBAN (widget);
    GdkModifierType modifiers;
    GdkModifierType button_mask = (event->button == 1
				   ? GDK_BUTTON1_MASK : GDK_BUTTON3_MASK);
    GtkGobanClickData data;

    gdk_window_get_pointer (widget->window, &data.x, &data.y, &modifiers);
    widget_coordinates_to_board (goban, data.x, data.y, &data.x, &data.y);

    if (goban->button_pressed == event->button) {
      goban->button_pressed = 0;

      if (data.x == goban->press_x && data.y == goban->press_y
	  && (((modifiers & MODIFIER_MASK) | button_mask)
	      == goban->press_modifiers)) {
	data.feedback_tile = goban->feedback_tile;
	data.button	   = event->button;
	data.modifiers	   = modifiers & ~button_mask;
	g_signal_emit (G_OBJECT (goban), goban_signals[GOBAN_CLICKED], 0,
		       &data);
      }
    }
    else {
      if (!(modifiers & (GDK_BUTTON1_MASK | GDK_BUTTON3_MASK)))
	goban->button_pressed = 0;
    }

    emit_pointer_moved (goban, data.x, data.y, modifiers & MODIFIER_MASK);
  }

  return FALSE;
}


static gboolean
gtk_goban_motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
  GtkGoban *goban = GTK_GOBAN (widget);
  int x;
  int y;

  widget_coordinates_to_board (goban, event->x, event->y, &x, &y);
  if (x != goban->pointer_x || y != goban->pointer_y
      || (event->state & MODIFIER_MASK) != goban->modifiers)
    emit_pointer_moved (goban, x, y, event->state & MODIFIER_MASK);

  return FALSE;
}


static gboolean
gtk_goban_enter_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
  GtkGoban *goban = GTK_GOBAN (widget);
  int x;
  int y;

  widget_coordinates_to_board (goban, event->x, event->y, &x, &y);
  emit_pointer_moved (goban, x, y, event->state & MODIFIER_MASK);

  return FALSE;
}


static gboolean
gtk_goban_leave_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
  GtkGoban *goban = GTK_GOBAN (widget);

  UNUSED (event);

  if (goban->button_pressed)
    goban->button_pressed = 0;

  set_feedback_data (goban, NULL_X, NULL_Y, NULL, GOBAN_FEEDBACK_NONE);

  return FALSE;
}


/* Navigate goban in respond to mouse wheel movement.  Default is
 * navigating back/forth by moves.  Shift causes switching between
 * variations, Ctrl makes navigation fast.
 */
static gboolean
gtk_goban_scroll_event (GtkWidget *widget, GdkEventScroll *event)
{
  gint direction;

  switch (event->direction) {
  case GDK_SCROLL_UP:
    if (!(event->state & GDK_SHIFT_MASK)) {
      direction = (event->state & GDK_CONTROL_MASK
		   ? GOBAN_NAVIGATE_BACK_FAST : GOBAN_NAVIGATE_BACK);
      break;
    }

  case GDK_SCROLL_LEFT:
    direction = GOBAN_NAVIGATE_PREVIOUS_VARIATION;
    break;

  case GDK_SCROLL_DOWN:
    if (!(event->state & GDK_SHIFT_MASK)) {
      direction = (event->state & GDK_CONTROL_MASK
		   ? GOBAN_NAVIGATE_FORWARD_FAST : GOBAN_NAVIGATE_FORWARD);
      break;
    }

  case GDK_SCROLL_RIGHT:
    direction = GOBAN_NAVIGATE_NEXT_VARIATION;
    break;

  default:
    return FALSE;
  }

  g_signal_emit (G_OBJECT (widget), goban_signals[NAVIGATE], 0, direction);

  return FALSE;
}


static gboolean
gtk_goban_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  GtkGoban *goban = GTK_GOBAN (widget);
  GdkModifierType modifiers;

  gdk_window_get_pointer (widget->window, NULL, NULL, &modifiers);
  if ((modifiers & MODIFIER_MASK) != goban->modifiers) {
    emit_pointer_moved (goban, goban->pointer_x, goban->pointer_y,
			modifiers & MODIFIER_MASK);
  }

  return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
}


static gboolean
gtk_goban_key_release_event (GtkWidget *widget, GdkEventKey *event)
{
  GtkGoban *goban = GTK_GOBAN (widget);
  GdkModifierType modifiers;

  gdk_window_get_pointer (widget->window, NULL, NULL, &modifiers);
  if ((modifiers & MODIFIER_MASK) != goban->modifiers) {
    emit_pointer_moved (goban, goban->pointer_x, goban->pointer_y,
			modifiers & MODIFIER_MASK);
  }

  return GTK_WIDGET_CLASS (parent_class)->key_release_event (widget, event);
}


static gboolean
gtk_goban_focus_in_or_out_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkGoban *goban = GTK_GOBAN (widget);
  GdkRectangle rectangle = { goban->coordinates_x_left - 1,
			     (goban->coordinates_y_top
			      + goban->character_height / 2
			      - 4),
			     8, 8 };

  UNUSED (event);

  gdk_window_invalidate_rect (widget->window, &rectangle, FALSE);

  /* Don't fallback to default handler, since we don't need full
   * widget redraw.
   */
  return FALSE;
}


static void
gtk_goban_allocate_screen_resources (GtkGobanBase *goban_base)
{
  GtkGoban *goban = GTK_GOBAN (goban_base);
  gint cell_size = goban->base.cell_size;

  parent_class->allocate_screen_resources (goban_base);

  goban->small_tile_set
    = gtk_main_tile_set_create_or_reuse ((2 * cell_size - 1) / 3,
					 goban->base.game);

  if (goban->base.game == GAME_AMAZONS) {
    QuarryColor color = amazons_board_appearance.checkerboard_pattern_color;
    double opacity = amazons_board_appearance.checkerboard_pattern_opacity;
    guint8 actual_opacity = G_MAXUINT8 * CLAMP (opacity, 0.0, 1.0);

    if (actual_opacity > 0) {
      if (!amazons_board_appearance.board_appearance.use_background_texture) {
	QuarryColor *background_color
	  = &amazons_board_appearance.board_appearance.background_color;

	/* No point in using semi-transparent pixbuf if there is no
	 * texture.  Just mix the colors.
	 */
	color.red   = (opacity * color.red
		       + (1 - opacity) * background_color->red);
	color.green = (opacity * color.green
		       + (1 - opacity) * background_color->green);
	color.blue  = (opacity * color.blue
		       + (1 - opacity) * background_color->blue);

	actual_opacity = G_MAXUINT8;
      }

      if (actual_opacity < G_MAXUINT8) {
	goban->checkerboard_pattern_object
	  = (GObject *) gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
					cell_size - 1, cell_size - 1);
	gdk_pixbuf_fill (GDK_PIXBUF (goban->checkerboard_pattern_object),
			 (actual_opacity
			  | (color.red << 24)
			  | (color.green << 16)
			  | (color.blue << 8)));
      }
      else {
	GdkGC *gc = gdk_gc_new (GTK_WIDGET (goban)->window);
	GdkColor gdk_color;

	gtk_utils_set_gdk_color (&gdk_color, color);
	gdk_gc_set_rgb_fg_color (gc, &gdk_color);

	goban->checkerboard_pattern_object
	  = (GObject *) gdk_pixmap_new (GTK_WIDGET (goban)->window,
					cell_size - 1, cell_size - 1, -1);
	gdk_draw_rectangle (GDK_DRAWABLE (goban->checkerboard_pattern_object),
			    gc, TRUE, 0, 0, cell_size - 1, cell_size - 1);

	g_object_unref (gc);
      }
    }
  }
}


static void
gtk_goban_free_screen_resources (GtkGobanBase *goban_base)
{
  GtkGoban *goban = GTK_GOBAN (goban_base);

  object_cache_unreference_object (&gtk_main_tile_set_cache,
				   goban->small_tile_set);

  if (goban->checkerboard_pattern_object) {
    g_object_unref (goban->checkerboard_pattern_object);
    goban->checkerboard_pattern_object = NULL;
  }

  parent_class->free_screen_resources (goban_base);
}


static void
gtk_goban_finalize (GObject *object)
{
  GtkGoban *goban = GTK_GOBAN (object);
  int k;

  if (goban->width && goban->height) {
    int x;
    int y;

    for (y = 0; y < goban->height; y++) {
      for (x = 0; x < goban->width; x++)
	g_free (goban->sgf_labels[POSITION (x, y)]);
    }
  }

  for (k = 0; k < NUM_OVERLAYS; k++) {
    if (goban->overlay_positon_lists[k]) {
      board_position_list_delete (goban->overlay_positon_lists[k]);
      g_free (goban->overlay_contents[k]);
    }
  }

  if (goban->checkerboard_pattern_object)
    g_object_unref (goban->checkerboard_pattern_object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}



gint
gtk_goban_negotiate_width (GtkWidget *widget, gint height)
{
  UNUSED (widget);
  UNUSED (height);

  return height;
}


gint
gtk_goban_negotiate_height (GtkWidget *widget, gint width)
{
  UNUSED (widget);
  UNUSED (width);

  return width;
}


void
gtk_goban_update (GtkGoban *goban,
		  const char grid[BOARD_GRID_SIZE],
		  const char goban_markup[BOARD_GRID_SIZE],
		  const char sgf_markup[BOARD_GRID_SIZE],
		  const SgfLabelList *sgf_label_list,
		  int last_move_x, int last_move_y)
{
  GtkWidget *widget;
  gint width;
  gint height;
  gint cell_size;
  int k;
  int x;
  int y;
  int last_move_pos;

  assert (GTK_IS_GOBAN (goban));

  for (k = NUM_OVERLAYS; --k >= 0;)
    set_overlay_data (goban, k, NULL, TILE_NONE, TILE_NONE);

  if (!grid)
    grid = goban->grid;
  if (!goban_markup)
    goban_markup = goban->goban_markup;
  if (!sgf_markup)
    sgf_markup = goban->sgf_markup;

  /* Never let last move markup to override real SGF markup. */
  last_move_pos = POSITION (last_move_x, last_move_y);
  if (sgf_markup[last_move_pos] != SGF_MARKUP_NONE)
    last_move_pos = NULL_POSITION;

  widget = GTK_WIDGET (goban);

  width	    = goban->width;
  height    = goban->height;
  cell_size = goban->base.cell_size;

  if (GTK_WIDGET_REALIZED (widget)) {
    GtkGobanMargins margins;
    GdkRectangle rectangle_stone;
    GdkRectangle rectangle_markup;
    GdkRectangle rectangle_sgf_markup;
    GdkRectangle rectangle_sgf_label;
    const SgfLabel *sgf_label = NULL;
    const SgfLabel *sgf_labels_limit = NULL;

    if (sgf_label_list && sgf_label_list != KEEP_SGF_LABELS) {
      sgf_label	       = sgf_label_list->labels;
      sgf_labels_limit = sgf_label_list->labels + sgf_label_list->num_labels;
    }

    rectangle_stone.width	= goban->base.main_tile_set->tile_size;
    rectangle_stone.height	= goban->base.main_tile_set->tile_size;
    rectangle_markup.width	= goban->small_tile_set->tile_size;
    rectangle_markup.height	= goban->small_tile_set->tile_size;
    rectangle_sgf_markup.width	= goban->base.sgf_markup_tile_set->tile_size;
    rectangle_sgf_markup.height = goban->base.sgf_markup_tile_set->tile_size;
    rectangle_sgf_label.width	= cell_size - 2;

    /* NOTE: Keep in sync with draw_vertical_line_with_gaps().  Or
     *	     maybe define a macro.
     */
    rectangle_sgf_label.height	= 6 + (10 * goban->character_height) / 9;

    compute_goban_margins (goban, &margins);

    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
	int pos = POSITION (x, y);

	if (goban->grid[pos] != grid[pos]
	    || ((goban->goban_markup[pos] & GOBAN_MARKUP_FLAGS_MASK)
		!= (goban_markup[pos] & GOBAN_MARKUP_FLAGS_MASK))) {
	  rectangle_stone.x = margins.stones_left_margin + x * cell_size;
	  rectangle_stone.y = margins.stones_top_margin + y * cell_size;
	  gdk_window_invalidate_rect (widget->window, &rectangle_stone, FALSE);

	  goban->grid[pos]	   = grid[pos];
	  goban->goban_markup[pos] = goban_markup[pos];
	  goban->sgf_markup[pos]   = sgf_markup[pos];
	}
	else if (goban->sgf_markup[pos] != sgf_markup[pos]
		 || ((pos == last_move_pos || pos == goban->last_move_pos)
		     && last_move_pos != goban->last_move_pos)) {
	  rectangle_sgf_markup.x = (margins.sgf_markup_left_margin
				    + x * cell_size);
	  rectangle_sgf_markup.y = (margins.sgf_markup_top_margin
				    + y * cell_size);
	  gdk_window_invalidate_rect (widget->window, &rectangle_sgf_markup,
				      FALSE);

	  goban->goban_markup[pos] = goban_markup[pos];
	  goban->sgf_markup[pos]   = sgf_markup[pos];
	}
	else if (goban->goban_markup[pos] != goban_markup[pos]) {
	  rectangle_markup.x = (margins.small_stones_left_margin
				+ x * cell_size);
	  rectangle_markup.y = (margins.small_stones_top_margin
				+ y * cell_size);
	  gdk_window_invalidate_rect (widget->window, &rectangle_markup,
				      FALSE);

	  goban->goban_markup[pos] = goban_markup[pos];
	}

	if (sgf_label_list != KEEP_SGF_LABELS) {
	  char *label_text = ((sgf_label != sgf_labels_limit
			       && sgf_label->point.x == x
			       && sgf_label->point.y == y)
			      ? sgf_label->text : NULL);

	  if (goban->sgf_labels[pos]
	      ? (!label_text
		 || strcmp (goban->sgf_labels[pos], label_text) != 0)
	      : label_text != NULL) {
	    rectangle_sgf_label.x = (goban->first_cell_center_x
				     - cell_size / 2
				     + x * cell_size);
	    rectangle_sgf_label.y = (goban->first_cell_center_y
				     - rectangle_sgf_label.height / 2
				     + y * cell_size);
	    gdk_window_invalidate_rect (widget->window, &rectangle_sgf_label,
					FALSE);

	    g_free (goban->sgf_labels[pos]);
	    goban->sgf_labels[pos] = (label_text
				      ? g_strdup (label_text) : NULL);
	  }

	  if (label_text)
	    sgf_label++;
	}
      }
    }
  }
  else {
    grid_copy (goban->grid, grid, width, height);
    grid_copy (goban->goban_markup, goban_markup, width, height);
    grid_copy (goban->sgf_markup, sgf_markup, width, height);

    if (sgf_label_list != KEEP_SGF_LABELS) {
      for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	  int pos = POSITION (x, y);

	  g_free (goban->sgf_labels[pos]);
	  goban->sgf_labels[pos] = NULL;
	}
      }

      if (sgf_label_list) {
	for (k = 0; k < sgf_label_list->num_labels; k++) {
	  int pos = POINT_TO_POSITION (sgf_label_list->labels[k].point);

	  goban->sgf_labels[pos] = g_strdup (sgf_label_list->labels[k].text);
	}
      }
    }
  }

  goban->last_move_pos = last_move_pos;
}


void
gtk_goban_force_feedback_poll (GtkGoban *goban)
{
  emit_pointer_moved (goban, goban->pointer_x, goban->pointer_y,
		      goban->modifiers);
}


void
gtk_goban_set_overlay_data (GtkGoban *goban, int overlay_index,
			    BoardPositionList *position_list,
			    int tile, int goban_markup_tile)
{
  const BoardPositionList *feedback_position_list
    = goban->overlay_positon_lists[FEEDBACK_OVERLAY];
  int need_feedback_poll
    = (position_list && feedback_position_list
       && board_position_lists_overlap (feedback_position_list,
					position_list));

  assert (goban);
  assert (0 <= overlay_index && overlay_index < NUM_OVERLAYS
	  && overlay_index != FEEDBACK_OVERLAY);
  assert ((0 <= tile && tile < NUM_TILES) || tile == GOBAN_TILE_DONT_CHANGE);
  assert ((0 <= goban_markup_tile && goban_markup_tile < NUM_TILES)
	  || goban_markup_tile == GOBAN_TILE_DONT_CHANGE);
  assert (tile != GOBAN_TILE_DONT_CHANGE
	  || goban_markup_tile != GOBAN_TILE_DONT_CHANGE);

  if (need_feedback_poll)
    set_overlay_data (goban, FEEDBACK_OVERLAY, NULL, TILE_NONE, TILE_NONE);

  set_overlay_data (goban, overlay_index, position_list,
		    tile, goban_markup_tile);

  if (need_feedback_poll) {
    emit_pointer_moved (goban, goban->pointer_x, goban->pointer_y,
			goban->modifiers);
  }
}



void
gtk_goban_set_contents (GtkGoban *goban, BoardPositionList *position_list,
			int grid_contents, int goban_markup_contents)
{
  int k;

  assert (GTK_IS_GOBAN (goban));
  assert (goban->base.game != GAME_DUMMY);

  for (k = NUM_OVERLAYS; --k >= 0;)
    set_overlay_data (goban, k, NULL, TILE_NONE, TILE_NONE);

  set_overlay_data (goban, 0, position_list,
		    grid_contents, goban_markup_contents);
  goban->overlay_positon_lists[0] = NULL;
}


int
gtk_goban_get_grid_contents (GtkGoban *goban, int x, int y)
{
  int k;
  int pos = POSITION (x, y);

  assert (GTK_IS_GOBAN (goban));
  assert (goban->base.game != GAME_DUMMY);
  assert (ON_SIZED_GRID (goban->width, goban->height, x, y));

  for (k = NUM_OVERLAYS; --k >= 0;) {
    if (goban->overlay_positon_lists[k]) {
      int position_index
	= board_position_list_find_position (goban->overlay_positon_lists[k],
					     pos);

      if (position_index != -1)
	return goban->overlay_contents[k][position_index];
    }
  }

  return goban->grid[pos];
}


void
gtk_goban_diff_against_grid
  (GtkGoban *goban, const char *grid,
   BoardPositionList *position_lists[NUM_ON_GRID_VALUES])
{
  int k;
  int x;
  int y;
  int num_positions[NUM_ON_GRID_VALUES];
  int positions[NUM_ON_GRID_VALUES][BOARD_MAX_POSITIONS];

  assert (GTK_IS_GOBAN (goban));
  assert (grid);

  for (k = 0; k < NUM_ON_GRID_VALUES; k++)
    num_positions[k] = 0;

  for (y = 0; y < goban->height; y++) {
    for (x = 0; x < goban->width; x++) {
      int pos = POSITION (x, y);
      int contents = goban->grid[pos];

      if (contents != grid[pos]) {
	assert (0 <= contents && contents < NUM_ON_GRID_VALUES);

	positions[contents][num_positions[contents]++] = pos;
      }
    }
  }

  for (k = 0; k < NUM_ON_GRID_VALUES; k++) {
    if (num_positions[k] > 0) {
      position_lists[k] = board_position_list_new (positions[k],
						   num_positions[k]);
    }
    else
      position_lists[k] = NULL;
  }
}


static void
emit_pointer_moved (GtkGoban *goban, int x, int y, GdkModifierType modifiers)
{
  if (ON_SIZED_GRID (goban->width, goban->height, x, y)
      && (goban->button_pressed == 0 || goban->press_modifiers == modifiers)) {
    GtkGobanPointerData data;
    GtkGobanPointerFeedback feedback;

    data.x			= x;
    data.y			= y;
    data.feedback_position_list = NULL;

    data.modifiers		= modifiers;
    data.button			= goban->button_pressed;
    data.press_x		= goban->press_x;
    data.press_y		= goban->press_y;

    g_signal_emit (G_OBJECT (goban), goban_signals[POINTER_MOVED], 0,
		   &data, &feedback);

    assert (data.button
	    || ((feedback & GOBAN_FEEDBACK_GRID_MASK)
		!= GOBAN_FEEDBACK_PRESS_DEFAULT));

    if (data.feedback_position_list)
      set_feedback_data (goban, x, y, data.feedback_position_list, feedback);
    else {
      int pos = POSITION (x, y);

      set_feedback_data (goban, x, y, board_position_list_new (&pos, 1),
			 feedback);
    }
  }
  else
    set_feedback_data (goban, NULL_X, NULL_Y, NULL, GOBAN_FEEDBACK_NONE);

  goban->modifiers = modifiers;
}


static void
set_feedback_data (GtkGoban *goban,
		   int x, int y, BoardPositionList *position_list,
		   GtkGobanPointerFeedback feedback)
{
  int feedback_grid = feedback & GOBAN_FEEDBACK_GRID_MASK;
  int feedback_goban_markup = feedback / GOBAN_FEEDBACK_MARKUP_FACTOR;
  int feedback_tile;
  int goban_markup_feedback_tile;

  switch (feedback_grid) {
  case GOBAN_FEEDBACK_FORCE_TILE_NONE:
    feedback_tile = TILE_NONE;
    break;

  case GOBAN_FEEDBACK_BLACK_OPAQUE:
  case GOBAN_FEEDBACK_WHITE_OPAQUE:
    feedback_tile = (STONE_OPAQUE + (feedback_grid - GOBAN_FEEDBACK_OPAQUE));
    break;

  case GOBAN_FEEDBACK_BLACK_GHOST:
  case GOBAN_FEEDBACK_WHITE_GHOST:
    feedback_tile = (STONE_50_TRANSPARENT
		     + (feedback_grid - GOBAN_FEEDBACK_GHOST));
    break;

  case GOBAN_FEEDBACK_THICK_BLACK_GHOST:
  case GOBAN_FEEDBACK_THICK_WHITE_GHOST:
    feedback_tile = (STONE_25_TRANSPARENT
		     + (feedback_grid - GOBAN_FEEDBACK_THICK_GHOST));
    break;

  case GOBAN_FEEDBACK_PRESS_DEFAULT:
    feedback_tile = GOBAN_TILE_DONT_CHANGE;

    if (x == goban->press_x && y == goban->press_y) {
      switch (goban->feedback_tile_at_press) {
      case BLACK_25_TRANSPARENT:
      case WHITE_25_TRANSPARENT:
	feedback_tile = (goban->feedback_tile_at_press
			 + STONE_50_TRANSPARENT - STONE_25_TRANSPARENT);
	break;

      case BLACK_50_TRANSPARENT:
      case WHITE_50_TRANSPARENT:
	feedback_tile = (goban->feedback_tile_at_press
			 + STONE_25_TRANSPARENT - STONE_50_TRANSPARENT);
	break;

      case TILE_SPECIAL:
	feedback_tile = TILE_SPECIAL;
	break;
      }
    }

    break;

  case GOBAN_FEEDBACK_BLACK_MOVE:
  case GOBAN_FEEDBACK_WHITE_MOVE:
    {
      int color_index = feedback_grid - GOBAN_FEEDBACK_MOVE;

      assert (position_list->num_positions == 1);

      if (COLOR_INDEX (goban->grid[POSITION (x, y)]) != color_index)
	feedback_tile = STONE_50_TRANSPARENT + color_index;
      else
	feedback_tile = STONE_25_TRANSPARENT + color_index;
    }

    break;

  case GOBAN_FEEDBACK_ADD_BLACK_OR_REMOVE:
  case GOBAN_FEEDBACK_ADD_WHITE_OR_REMOVE:
    {
      int contents = gtk_goban_get_grid_contents (goban, x, y);

      if (contents == EMPTY) {
	feedback_tile = (STONE_50_TRANSPARENT
			 + (feedback_grid - GOBAN_FEEDBACK_ADD_OR_REMOVE));
      }
      else if (IS_STONE (contents))
	feedback_tile = STONE_25_TRANSPARENT + COLOR_INDEX (contents);
      else
	feedback_tile = GOBAN_TILE_DONT_CHANGE;
    }

    break;

  case GOBAN_FEEDBACK_SPECIAL:
    feedback_tile = TILE_SPECIAL;
    break;

  default:
    feedback_tile = GOBAN_TILE_DONT_CHANGE;
  }

  switch (feedback_goban_markup) {
  case GOBAN_FEEDBACK_FORCE_TILE_NONE:
    goban_markup_feedback_tile = TILE_NONE;
    break;

  case GOBAN_FEEDBACK_BLACK_OPAQUE:
  case GOBAN_FEEDBACK_WHITE_OPAQUE:
    goban_markup_feedback_tile = (STONE_OPAQUE
				  + (feedback_goban_markup
				     - GOBAN_FEEDBACK_OPAQUE));
    break;

  case GOBAN_FEEDBACK_BLACK_GHOST:
  case GOBAN_FEEDBACK_WHITE_GHOST:
    goban_markup_feedback_tile = (STONE_50_TRANSPARENT
				  + (feedback_goban_markup
				     - GOBAN_FEEDBACK_GHOST));
    break;

  case GOBAN_FEEDBACK_THICK_BLACK_GHOST:
  case GOBAN_FEEDBACK_THICK_WHITE_GHOST:
    goban_markup_feedback_tile = (STONE_25_TRANSPARENT
				  + (feedback_goban_markup
				     - GOBAN_FEEDBACK_THICK_GHOST));
    break;

  default:
    goban_markup_feedback_tile = GOBAN_TILE_DONT_CHANGE;
  }

  if (feedback_tile != GOBAN_TILE_DONT_CHANGE
      || goban_markup_feedback_tile != GOBAN_TILE_DONT_CHANGE) {
    set_overlay_data (goban, FEEDBACK_OVERLAY, position_list,
		      feedback_tile, goban_markup_feedback_tile);
  }
  else {
    if (position_list)
      board_position_list_delete (position_list);

    set_overlay_data (goban, FEEDBACK_OVERLAY, NULL, TILE_NONE, TILE_NONE);
  }

  goban->pointer_x     = x;
  goban->pointer_y     = y;
  goban->feedback_tile = feedback_tile;
}


static void
set_overlay_data (GtkGoban *goban, int overlay_index,
		  BoardPositionList *position_list,
		  int tile, int goban_markup_tile)
{
  GtkWidget *widget = GTK_WIDGET (goban);
  BoardPositionList *old_position_list
    = goban->overlay_positon_lists[overlay_index];
  int num_new_positions = (position_list ? position_list->num_positions : -1);
  int num_old_positions = (old_position_list
			   ? old_position_list->num_positions : -1);
  char *old_overlay_contents = goban->overlay_contents[overlay_index];
  char *grid = goban->grid;
  char *goban_markup = goban->goban_markup;
  int i;
  int j;

  if (position_list == NULL && old_position_list == NULL)
    return;

  if (GTK_WIDGET_REALIZED (widget)) {
    int cell_size = goban->base.cell_size;
    GtkGobanMargins margins;
    GdkRectangle rectangle_stone;
    GdkRectangle rectangle_markup;

    compute_goban_margins (goban, &margins);

    rectangle_stone.width   = goban->base.main_tile_set->tile_size;
    rectangle_stone.height  = goban->base.main_tile_set->tile_size;
    rectangle_markup.width  = goban->small_tile_set->tile_size;
    rectangle_markup.height = goban->small_tile_set->tile_size;

    for (i = 0, j = 0; i < num_new_positions || j < num_old_positions;) {
      int pos;
      char new_tile;
      char new_goban_markup_tile;

      if (i < num_new_positions
	  && (j >= num_old_positions
	      || (position_list->positions[i]
		  <= old_position_list->positions[j]))) {
	int same_positions;

	pos = position_list->positions[i++];
	same_positions = (j < num_old_positions
			  && pos == old_position_list->positions[j]);

	if (tile != GOBAN_TILE_DONT_CHANGE)
	  new_tile = tile;
	else {
	  if (same_positions)
	    new_tile = old_overlay_contents[j];
	  else
	    new_tile = grid[pos];
	}

	if (goban_markup_tile != GOBAN_TILE_DONT_CHANGE)
	  new_goban_markup_tile = goban_markup_tile;
	else {
	  if (same_positions) {
	    new_goban_markup_tile = old_overlay_contents[num_old_positions
							 + j];
	  }
	  else
	    new_goban_markup_tile = goban_markup[pos];
	}

	if (same_positions)
	  j++;
      }
      else {
	pos = old_position_list->positions[j];

	new_tile = old_overlay_contents[j];
	new_goban_markup_tile = old_overlay_contents[num_old_positions + j];
	j++;
      }

      if (new_tile != grid[pos]
	  || new_goban_markup_tile != goban_markup[pos]) {
	int x = POSITION_X (pos);
	int y = POSITION_Y (pos);

	if (new_tile != grid[pos]
	    || (new_goban_markup_tile & GOBAN_MARKUP_FLAGS_MASK)
	    || (goban_markup[pos] & GOBAN_MARKUP_FLAGS_MASK)) {
	  rectangle_stone.x = margins.stones_left_margin + x * cell_size;
	  rectangle_stone.y = margins.stones_top_margin + y * cell_size;
	  gdk_window_invalidate_rect (widget->window, &rectangle_stone, FALSE);
	}
	else {
	  rectangle_markup.x = (margins.small_stones_left_margin
				+ x * cell_size);
	  rectangle_markup.y = (margins.small_stones_top_margin
				+ y * cell_size);
	  gdk_window_invalidate_rect (widget->window, &rectangle_markup,
				      FALSE);
	}
      }
    }
  }

  if (old_position_list) {
    for (j = 0; j < num_old_positions; j++) {
      grid[old_position_list->positions[j]] = old_overlay_contents[j];
      goban_markup[old_position_list->positions[j]]
	= old_overlay_contents[num_old_positions + j];
    }

    board_position_list_delete (old_position_list);
    g_free (old_overlay_contents);
  }

  goban->overlay_positon_lists[overlay_index] = position_list;
  if (position_list) {
    goban->overlay_contents[overlay_index] = g_malloc ((2 * num_new_positions)
						       * sizeof (char));

    for (i = 0; i < num_new_positions; i++) {
      int pos = position_list->positions[i];

      goban->overlay_contents[overlay_index][i] = grid[pos];
      if (tile != GOBAN_TILE_DONT_CHANGE)
	grid[pos] = tile;

      goban->overlay_contents[overlay_index][num_new_positions + i]
	= goban_markup[pos];
      if (goban_markup_tile != GOBAN_TILE_DONT_CHANGE)
	goban_markup[pos] = goban_markup_tile;
    }
  }
}


static void
compute_goban_margins (const GtkGoban *goban, GtkGobanMargins *margins)
{
  margins->stones_left_margin = goban->left_margin;
  margins->stones_top_margin  = goban->top_margin;

  if (goban->base.game == GAME_GO) {
    margins->sgf_markup_left_margin
      = goban->left_margin - goban->base.sgf_markup_tile_set->tile_size / 2;
    margins->sgf_markup_top_margin
      = goban->top_margin - goban->base.sgf_markup_tile_set->tile_size / 2;
  }
  else {
    margins->stones_left_margin += goban->base.cell_size / 2;
    margins->stones_top_margin	+= goban->base.cell_size / 2;

    margins->sgf_markup_left_margin = goban->left_margin + 1;
    margins->sgf_markup_top_margin  = goban->top_margin + 1;
  }

  margins->small_stones_left_margin
    = margins->stones_left_margin + goban->small_tile_set->stones_x_offset;
  margins->small_stones_top_margin
    = margins->stones_top_margin + goban->small_tile_set->stones_y_offset;

  margins->stones_left_margin += goban->base.main_tile_set->stones_x_offset;
  margins->stones_top_margin  += goban->base.main_tile_set->stones_y_offset;
}


static void
widget_coordinates_to_board (const GtkGoban *goban, int window_x, int window_y,
			     int *board_x, int *board_y)
{
  gint cell_size = goban->base.cell_size;

  window_x -= goban->first_cell_center_x - cell_size / 2;
  window_y -= goban->first_cell_center_y - cell_size / 2;

  if (window_x >= 0 && window_y >= 0) {
    *board_x = window_x / cell_size;
    *board_y = window_y / cell_size;

    if (*board_x < goban->width && *board_y < goban->height)
      return;
  }

  *board_x = NULL_X;
  *board_y = NULL_Y;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
