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
 * Software Foundation, Inc., 59 Temple Place - Suite 330,         *
 * Boston, MA 02111-1307, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include "gtk-configuration.h"
#include "gtk-goban.h"
#include "gtk-preferences.h"
#include "gtk-utils.h"
#include "gtk-tile-set.h"
#include "quarry-marshal.h"
#include "sgf.h"
#include "board.h"
#include "game-info.h"
#include "utils.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

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


static void	 gtk_goban_class_init(GtkGobanClass *class);
static void	 gtk_goban_init(GtkGoban *goban);
static void	 gtk_goban_realize(GtkWidget *widget);

static void	 gtk_goban_size_request(GtkWidget *widget,
					GtkRequisition *requisition);
static void	 gtk_goban_size_allocate(GtkWidget *widget,
					 GtkAllocation *allocation);

static gboolean	 gtk_goban_expose(GtkWidget *widget,
				  GdkEventExpose *event);

static gboolean	 gtk_goban_button_press_event(GtkWidget *widget,
					      GdkEventButton *event);
static gboolean	 gtk_goban_button_release_event(GtkWidget *widget,
						GdkEventButton *event);
static gboolean	 gtk_goban_motion_notify_event(GtkWidget *widget,
					       GdkEventMotion *event);
static gboolean	 gtk_goban_enter_notify_event(GtkWidget *widget,
					      GdkEventCrossing *event);
static gboolean	 gtk_goban_leave_notify_event(GtkWidget *widget,
					      GdkEventCrossing *event);
static gboolean	 gtk_goban_scroll_event(GtkWidget *widget,
					GdkEventScroll *event);

static gboolean	 gtk_goban_key_press_event(GtkWidget *widget,
					   GdkEventKey *event);
static gboolean	 gtk_goban_key_release_event(GtkWidget *widget,
					     GdkEventKey *event);

static gboolean	 gtk_goban_focus_in_or_out_event(GtkWidget *widget,
						 GdkEventFocus *event);

static void	 gtk_goban_finalize(GObject *object);

static void	 unreference_cached_objects(GtkGoban *goban);

static void	 set_goban_style_appearance(GtkGoban *goban);
static void	 set_goban_non_style_appearance(GtkGoban *goban);

/* FIXME: Doesn't belong here.  And name is bad too. */
static void	 find_hoshi_points(GtkGoban *goban);

static void	 emit_pointer_moved(GtkGoban *goban, int x, int y,
				    GdkModifierType modifiers);
static void	 set_feedback_data(GtkGoban *goban, int x, int y,
				   BoardPositionList *position_list,
				   GtkGobanPointerFeedback feedback);

static void	 set_overlay_data(GtkGoban *goban, int overlay_index,
				  BoardPositionList *position_list,
				  int tile, int goban_markup_tile);

static void	 widget_coordinates_to_board(GtkGoban *goban,
					     int window_x, int window_y,
					     int *board_x, int *board_y);


static GtkWidgetClass  *parent_class;

static GSList	       *all_gobans = NULL;


enum {
  POINTER_MOVED,
  GOBAN_CLICKED,
  NAVIGATE,
  NUM_SIGNALS
};

static guint		goban_signals[NUM_SIGNALS];


GtkType
gtk_goban_get_type(void)
{
  static GtkType goban_type = 0;

  if (!goban_type) {
    static GTypeInfo goban_info = {
      sizeof(GtkGobanClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_goban_class_init,
      NULL,
      NULL,
      sizeof(GtkGoban),
      2,
      (GInstanceInitFunc) gtk_goban_init,
      NULL
    };

    goban_type = g_type_register_static(GTK_TYPE_WIDGET, "GtkGoban",
					&goban_info, 0);
  }

  return goban_type;
}


static void
gtk_goban_class_init(GtkGobanClass *class)
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

  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
  GtkBindingSet *binding_set;

  parent_class = g_type_class_peek_parent(class);

  G_OBJECT_CLASS(class)->finalize = gtk_goban_finalize;

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

  class->pointer_moved = NULL;
  class->goban_clicked = NULL;
  class->navigate      = NULL;

  goban_signals[POINTER_MOVED]
    = g_signal_new("pointer-moved",
		   G_TYPE_FROM_CLASS(class),
		   G_SIGNAL_RUN_LAST,
		   G_STRUCT_OFFSET(GtkGobanClass, pointer_moved),
		   NULL, NULL,
		   quarry_marshal_INT__POINTER,
		   G_TYPE_INT, 1, G_TYPE_POINTER);

  goban_signals[GOBAN_CLICKED]
    = g_signal_new("goban-clicked",
		   G_TYPE_FROM_CLASS(class),
		   G_SIGNAL_RUN_LAST,
		   G_STRUCT_OFFSET(GtkGobanClass, pointer_moved),
		   NULL, NULL,
		   quarry_marshal_VOID__POINTER,
		   G_TYPE_NONE, 1, G_TYPE_POINTER);

  goban_signals[NAVIGATE]
    = g_signal_new("navigate",
		   G_TYPE_FROM_CLASS(class),
		   G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		   G_STRUCT_OFFSET(GtkGobanClass, navigate),
		   NULL, NULL,
		   quarry_marshal_VOID__INT,
		   G_TYPE_NONE, 1, G_TYPE_INT);

  binding_set = gtk_binding_set_by_class(class);
  gtk_utils_add_similar_bindings(binding_set, "navigate", navigation_bindings,
				 (sizeof(navigation_bindings)
				  / sizeof(GtkUtilsBindingInfo)));
}


static void
gtk_goban_init(GtkGoban *goban)
{
  int k;

  GTK_WIDGET_SET_FLAGS(GTK_WIDGET(goban), GTK_CAN_FOCUS);

  goban->game	= GAME_DUMMY;
  goban->width	= 0;
  goban->height	= 0;

  goban->cell_size = 0;

  goban->font_description
    = pango_font_description_copy(gtk_widget_get_default_style()->font_desc);
  goban->font_size = 0;

  goban->main_tile_set		     = NULL;
  goban->small_tile_set		     = NULL;
  goban->sgf_markup_tile_set	     = NULL;
  goban->checkerboard_pattern_object = NULL;

  goban->last_move_x = NULL_X;
  goban->last_move_y = NULL_Y;

  for (k = 0; k < NUM_OVERLAYS; k++) {
    goban->overlay_positon_lists[k] = NULL;
    goban->overlay_contents[k] = NULL;
  }

  set_feedback_data(goban, NULL_X, NULL_Y, NULL, GOBAN_FEEDBACK_NONE);

  all_gobans = g_slist_prepend(all_gobans, goban);
}


GtkWidget *
gtk_goban_new(void)
{
  return GTK_WIDGET(g_object_new(GTK_TYPE_GOBAN, NULL));
}


void
gtk_goban_set_parameters(GtkGoban *goban, Game game, int width, int height)
{
  int is_new_game;

  assert(GTK_IS_GOBAN(goban));
  assert(GAME_IS_SUPPORTED(game));
  assert(width > 0 && height > 0);

  if (goban->width && goban->height) {
    int x;
    int y;

    for (y = 0; y < goban->height; y++) {
      for (x = 0; x < goban->width; x++)
	g_free(goban->sgf_labels[POSITION(x, y)]);
    }
  }

  is_new_game	= (goban->game != game);
  goban->game	= game;
  goban->width	= width;
  goban->height = height;

  grid_fill(goban->grid, goban->width, goban->height, TILE_NONE);
  grid_fill(goban->goban_markup, goban->width, goban->height, TILE_NONE);
  grid_fill(goban->sgf_markup, goban->width, goban->height, TILE_NONE);
  pointer_grid_fill((void **) goban->sgf_labels, goban->width, goban->height,
		    NULL);

  if (is_new_game)
    set_goban_style_appearance(goban);

  set_goban_non_style_appearance(goban);
}


static void
gtk_goban_realize(GtkWidget *widget)
{
  GtkGoban *goban = GTK_GOBAN(widget);
  GdkWindowAttr attributes;
  const gint attributes_mask = (GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL
				| GDK_WA_COLORMAP);
  const gint event_mask = (GDK_EXPOSURE_MASK | (GDK_ALL_EVENTS_MASK&0)
			   | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			   | GDK_POINTER_MOTION_MASK
			   | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK
			   | GDK_SCROLL_MASK
			   | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

  attributes.event_mask = gtk_widget_get_events(widget) | event_mask;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual(widget);
  attributes.colormap = gtk_widget_get_colormap(widget);
  attributes.window_type = GDK_WINDOW_CHILD;

  widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
				  &attributes, attributes_mask);
  gdk_window_set_user_data(widget->window, goban);

  widget->style = gtk_style_attach(widget->style, widget->window);
  gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

  set_goban_non_style_appearance(goban);
}


/* FIXME: write this function. */
static void
gtk_goban_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
  UNUSED(widget);

  requisition->width = 500;
  requisition->height = 500;
}


static void
gtk_goban_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
  GtkGoban *goban = GTK_GOBAN(widget);
  int cell_size;
  int grid_width = goban->width - (goban->game == GAME_GO ? 1 : 0);
  int grid_height = goban->height - (goban->game == GAME_GO ? 1 : 0);
  int font_size;
  float vertical_gap = (goban->game == GAME_GO ? 0.65 : 0.8);
  float horizontal_gap = (goban->height < 10 && goban->game == GAME_GO
			  ? 0.65 : 0.8);
  int horizontal_deficit;
  int horizontal_gap_pixels;
  int vertical_padding;

  assert(goban->width != 0 && goban->height != 0);

  widget->allocation = *allocation;

  cell_size = floor(MIN(allocation->width / (goban->width
					     + 2 * horizontal_gap),
			allocation->height / (goban->height
					      + 2 * vertical_gap)));
  font_size = (3 * cell_size) / 7;

  if (font_size != goban->font_size) {
    PangoLayout *layout = gtk_widget_create_pango_layout(widget, "8");

    goban->font_size = font_size;

    pango_font_description_set_size(goban->font_description,
				    font_size * PANGO_SCALE);
    pango_layout_set_font_description(layout, goban->font_description);
    pango_layout_get_pixel_size(layout,
				&goban->digit_width, &goban->character_height);
    goban->digit_width += 2;

    g_object_unref(layout);
  }

  horizontal_deficit = (allocation->width - goban->width * cell_size
			- (goban->height < 10 ? 2 : 4) * goban->digit_width);
  if (horizontal_deficit < 0)
    cell_size -= (-horizontal_deficit + goban->width - 1) / goban->width;

  if (goban->cell_size != cell_size) {
    int main_tile_size = cell_size - (goban->game == GAME_GO ? 0 : 1);

    goban->cell_size = cell_size;

    unreference_cached_objects(goban);

    goban->main_tile_set
      = gtk_main_tile_set_create_or_reuse(main_tile_size, goban->game);
    goban->small_tile_set
      = gtk_main_tile_set_create_or_reuse((2 * cell_size - 1) / 3, goban->game);
    goban->sgf_markup_tile_set
      = gtk_sgf_markup_tile_set_create_or_reuse(main_tile_size, goban->game);

    set_goban_non_style_appearance(goban);
  }

  goban->left_margin = (allocation->width - grid_width * cell_size) / 2;
  goban->right_margin = goban->left_margin + grid_width * cell_size;

  goban->top_margin = (allocation->height - grid_height * cell_size) / 2;
  goban->bottom_margin = goban->top_margin + grid_height * cell_size;

  goban->first_cell_center_x = (allocation->width
				- (goban->width - 1) * cell_size) / 2;
  goban->first_cell_center_y = (allocation->height
				- (goban->height - 1) * cell_size) / 2;

  horizontal_gap_pixels = MAX(horizontal_gap * cell_size,
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

  goban->stones_left_margin = goban->left_margin;
  goban->stones_top_margin = goban->top_margin;

  if (goban->game == GAME_GO) {
    goban->sgf_markup_left_margin
      = goban->left_margin - goban->sgf_markup_tile_set->tile_size / 2;
    goban->sgf_markup_top_margin
      = goban->top_margin - goban->sgf_markup_tile_set->tile_size / 2;
  }
  else {
    goban->stones_left_margin += cell_size / 2;
    goban->stones_top_margin += cell_size / 2;

    goban->sgf_markup_left_margin = goban->left_margin + 1;
    goban->sgf_markup_top_margin = goban->top_margin + 1;
  }

  goban->small_stones_left_margin = (goban->stones_left_margin
				     + goban->small_tile_set->stones_x_offset);
  goban->small_stones_top_margin = (goban->stones_top_margin
				    + goban->small_tile_set->stones_y_offset);

  goban->stones_left_margin += goban->main_tile_set->stones_x_offset;
  goban->stones_top_margin += goban->main_tile_set->stones_y_offset;

  find_hoshi_points(goban);

  goban->button_pressed = 0;

  if (GTK_WIDGET_REALIZED(widget)) {
    int x;
    int y;
    GdkModifierType modifiers;

    gdk_window_move_resize(widget->window, allocation->x, allocation->y,
			   allocation->width, allocation->height);

    gdk_window_get_pointer(widget->window, &x, &y, &modifiers);
    widget_coordinates_to_board(goban, x, y, &x, &y);

    if (x != goban->pointer_x || y != goban->pointer_y
	|| (modifiers & MODIFIER_MASK) != goban->modifiers)
      emit_pointer_moved(goban, x, y, modifiers & MODIFIER_MASK);
  }
}


static gboolean
gtk_goban_expose(GtkWidget *widget, GdkEventExpose *event)
{
  int k;
  int x;
  int y;
  gint clip_left_margin	  = event->area.x;
  gint clip_right_margin  = event->area.x + event->area.width;
  gint clip_top_margin	  = event->area.y;
  gint clip_bottom_margin = event->area.y + event->area.height;
  gint lower_limit;
  gint upper_limit;
  GdkRectangle row_rectangle;
  GdkRectangle cell_rectangle;
  PangoLayout *layout = NULL;

  GtkGoban *goban = GTK_GOBAN(widget);
  int cell_size = goban->cell_size;
  gint checkerboard_pattern_mode;

  GdkWindow *window = widget->window;
  GdkGC *gc = widget->style->fg_gc[GTK_STATE_NORMAL];

  if (!GTK_WIDGET_DRAWABLE(widget))
    return FALSE;

  if (GTK_WIDGET_HAS_FOCUS(widget)) {
    /* Draw a small rectangle to indicate focus. */
    gdk_draw_rectangle(window, gc, FALSE,
		       goban->coordinates_x_left - 1,
		       (goban->coordinates_y_top + goban->character_height / 2
			- 4),
		       7, 7);
  }

  /* Draw grid lines. */

  for (x = (goban->left_margin
	    + ((MAX(0, clip_left_margin - goban->left_margin) / cell_size)
	       * cell_size));
       x <= goban->right_margin && x < clip_right_margin; x += cell_size) {
    gdk_draw_line(window, gc,
		  x, goban->top_margin, x, goban->bottom_margin);
  }

  for (y = (goban->top_margin
	    + ((MAX(0, clip_top_margin - goban->top_margin) / cell_size)
	       * cell_size));
       y <= goban->bottom_margin && y < clip_bottom_margin; y += cell_size) {
    gdk_draw_line(window, gc,
		  goban->left_margin, y, goban->right_margin, y);
  }

  /* Make the outmost grid lines thicker. */

  if (clip_left_margin <= goban->left_margin - 1) {
    gdk_draw_line(window, gc,
		  goban->left_margin - 1, goban->top_margin,
		  goban->left_margin - 1, goban->bottom_margin + 1);
  }

  if (clip_right_margin > goban->right_margin + 1) {
    gdk_draw_line(window, gc,
		  goban->right_margin + 1, goban->top_margin - 1,
		  goban->right_margin + 1, goban->bottom_margin);
  }

  if (clip_top_margin <= goban->top_margin - 1) {
    gdk_draw_line(window, gc,
		  goban->left_margin - 1, goban->top_margin - 1,
		  goban->right_margin, goban->top_margin - 1);
  }

  if (clip_bottom_margin > goban->bottom_margin + 1) {
    gdk_draw_line(window, gc,
		  goban->left_margin, goban->bottom_margin + 1,
		  goban->right_margin + 1, goban->bottom_margin + 1);

  }

  /* Draw coordinate labels. */
  if (clip_left_margin < goban->left_margin
      || clip_right_margin > goban->right_margin
      || clip_top_margin < goban->top_margin
      || clip_bottom_margin > goban->bottom_margin) {
    const char *horizontal_coordinates
      = game_info[goban->game].horizontal_coordinates;

    layout = gtk_widget_create_pango_layout(widget, NULL);
    pango_layout_set_font_description(layout, goban->font_description);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_set_width(layout, 2 * cell_size);

    /* Left and right label columns. */
    if (clip_left_margin < goban->left_margin
	|| clip_right_margin > goban->right_margin) {
      lower_limit = MAX(0,
			((clip_top_margin - goban->coordinates_y_side)
			 / cell_size));
      upper_limit = MIN(goban->height,
			(1 + ((clip_bottom_margin - goban->coordinates_y_side)
			      / cell_size)));

      for (k = lower_limit; k < upper_limit; k++) {
	char buffer[4];

	sprintf(buffer, "%d",
		(game_info[goban->game].reversed_vertical_coordinates
		 ? goban->height - k : k + 1));
	pango_layout_set_text(layout, buffer, -1);

	if (clip_left_margin < goban->left_margin) {
	  gdk_draw_layout(widget->window, gc,
			  goban->coordinates_x_left,
			  goban->coordinates_y_side + k * cell_size,
			  layout);
	}

	if (clip_right_margin > goban->right_margin) {
	  gdk_draw_layout(widget->window, gc,
			  goban->coordinates_x_right,
			  goban->coordinates_y_side + k * cell_size,
			  layout);
	}
      }
    }

    /* Top and bottom label rows. */
    if (clip_top_margin < goban->top_margin
	|| clip_bottom_margin > goban->bottom_margin) {
      lower_limit = MAX(0,
			((clip_left_margin
			  - (goban->first_cell_center_x - cell_size / 2))
			 / cell_size));
      upper_limit = MIN(goban->width,
			(1 + ((clip_right_margin
			       - (goban->first_cell_center_x - cell_size / 2))
			      / cell_size)));

      for (k = lower_limit; k < upper_limit; k++) {
	pango_layout_set_text(layout, horizontal_coordinates + k, 1);

	if (clip_top_margin < goban->top_margin) {
	  gdk_draw_layout(widget->window, gc,
			  goban->first_cell_center_x + k * cell_size,
			  goban->coordinates_y_top,
			  layout);
	}

	if (clip_bottom_margin > goban->bottom_margin) {
	  gdk_draw_layout(widget->window, gc,
			  goban->first_cell_center_x + k * cell_size,
			  goban->coordinates_y_bottom,
			  layout);
	}
      }
    }
  }

  /* FIXME: Not clipped now.  Needs a rewrite anyway. */
  for (k = 0; k < goban->num_hoshi_points; k++) {
    gdk_draw_rectangle(window, gc, FALSE,
		       goban->left_margin + goban->hoshi_point_x[k] - 2,
		       goban->top_margin + goban->hoshi_point_y[k] - 1,
		       4, 2);
    gdk_draw_rectangle(window, gc, FALSE,
		       goban->left_margin + goban->hoshi_point_x[k] - 1,
		       goban->top_margin + goban->hoshi_point_y[k] - 2,
		       2, 4);
  }

  if (goban->checkerboard_pattern_object) {
    if (GDK_IS_PIXBUF(goban->checkerboard_pattern_object))
      checkerboard_pattern_mode = CHECKERBOARD_PATTERN_WITH_PIXBUF;
    else {
      assert(GDK_IS_PIXMAP(goban->checkerboard_pattern_object));
      checkerboard_pattern_mode = CHECKERBOARD_PATTERN_WITH_PIXMAP;
    }
  }
  else
    checkerboard_pattern_mode = NO_CHECKERBOARD_PATTERN;

  row_rectangle.x = event->area.x;
  row_rectangle.width = event->area.width;

  row_rectangle.y = (checkerboard_pattern_mode == NO_CHECKERBOARD_PATTERN
		     ? goban->stones_top_margin
		     : MIN(goban->stones_top_margin, goban->top_margin + 1));

  if (checkerboard_pattern_mode == NO_CHECKERBOARD_PATTERN)
    row_rectangle.height = goban->cell_size;
  else {
    row_rectangle.height = (MAX((goban->stones_top_margin
				 + goban->main_tile_set->tile_size),
				goban->top_margin + 1 + cell_size)
			    - row_rectangle.y);
  }

  cell_rectangle.width = row_rectangle.height;
  cell_rectangle.height = row_rectangle.height;

  lower_limit = MAX(0, (clip_top_margin - row_rectangle.y) / cell_size);
  upper_limit = MIN(goban->height,
		    (1 + ((clip_bottom_margin
			   - (row_rectangle.y
			      + (row_rectangle.height - cell_size)))
			  / cell_size)));

  for (y = lower_limit, row_rectangle.y += lower_limit * cell_size;
       y < upper_limit; y++, row_rectangle.y += cell_size) {
    GdkRegion *row_region = gdk_region_rectangle(&row_rectangle);

    gdk_region_intersect(row_region, event->region);
    if (!gdk_region_empty(row_region)) {
      cell_rectangle.x = (checkerboard_pattern_mode == NO_CHECKERBOARD_PATTERN
			  ? goban->stones_left_margin
			  : MIN(goban->stones_left_margin,
				goban->left_margin + 1));
      cell_rectangle.y = row_rectangle.y;

      for (x = 0; x < goban->width; x++, cell_rectangle.x += cell_size) {
	if (gdk_region_rect_in(row_region, &cell_rectangle)
	    != GDK_OVERLAP_RECTANGLE_OUT) {
	  int pos = POSITION(x, y);
	  int tile = goban->grid[pos];
	  int markup_tile = goban->goban_markup[pos] & GOBAN_MARKUP_TILE_MASK;
	  int sgf_markup_tile = goban->sgf_markup[pos];

	  if (checkerboard_pattern_mode != NO_CHECKERBOARD_PATTERN
	      && (x + y) % 2 == 1) {
	    if (checkerboard_pattern_mode
		== CHECKERBOARD_PATTERN_WITH_PIXBUF) {
	      gdk_draw_pixbuf(window, gc,
			      (GdkPixbuf *) goban->checkerboard_pattern_object,
			      0, 0,
			      goban->left_margin + 1 + x * cell_size,
			      goban->top_margin + 1 + y * cell_size,
			      -1, -1,
			      GDK_RGB_DITHER_NORMAL, 0, 0);
	    }
	    else {
	      gdk_draw_drawable(window, gc,
				((GdkDrawable *)
				 goban->checkerboard_pattern_object),
				0, 0,
				goban->left_margin + 1 + x * cell_size,
				goban->top_margin + 1 + y * cell_size,
				-1, -1);
	    }
	  }

	  if (tile != TILE_NONE && tile != TILE_SPECIAL) {
	    if (IS_STONE(tile)) {
	      if (goban->goban_markup[pos] & GOBAN_MARKUP_GHOSTIFY)
		tile = STONE_50_TRANSPARENT + COLOR_INDEX(tile);
	      else if (goban->goban_markup[pos]
		       & GOBAN_MARKUP_GHOSTIFY_SLIGHTLY)
		tile = STONE_25_TRANSPARENT + COLOR_INDEX(tile);
	    }

	    gdk_draw_pixbuf(window, gc,
			    goban->main_tile_set->tiles[tile],
			    0, 0,
			    goban->stones_left_margin + x * cell_size,
			    goban->stones_top_margin + y * cell_size,
			    -1, -1,
			    GDK_RGB_DITHER_NORMAL, 0, 0);
	  }
	  else if (tile == TILE_SPECIAL) {
	    int base_x = goban->left_margin + x * cell_size;
	    int base_y = goban->top_margin + y * cell_size;

	    gdk_draw_line(window, gc,
			  base_x, base_y,
			  base_x + cell_size, base_y + cell_size);
	    gdk_draw_line(window, gc,
			  base_x, base_y + cell_size,
			  base_x + cell_size, base_y);
	  }

	  if (markup_tile != TILE_NONE) {
	    assert(markup_tile != TILE_SPECIAL);

	    gdk_draw_pixbuf(window, gc,
			    goban->small_tile_set->tiles[markup_tile],
			    0, 0,
			    goban->small_stones_left_margin + x * cell_size,
			    goban->small_stones_top_margin + y * cell_size,
			    -1, -1,
			    GDK_RGB_DITHER_NORMAL, 0, 0);
	  }

	  if (sgf_markup_tile != SGF_MARKUP_NONE) {
	    gint background = (IS_STONE(goban->grid[pos])
			       ? goban->grid[pos] : EMPTY);
	    GdkPixbuf *pixbuf
	      = goban->sgf_markup_tile_set->tiles[sgf_markup_tile][background];

	    gdk_draw_pixbuf(window, gc, pixbuf, 0, 0,
			    goban->sgf_markup_left_margin + x * cell_size,
			    goban->sgf_markup_top_margin + y * cell_size,
			    -1, -1,
			    GDK_RGB_DITHER_NORMAL, 0, 0);
	  }

	  if (goban->sgf_labels[pos]) {
	    if (!layout) {
	      layout = gtk_widget_create_pango_layout(widget, NULL);
	      pango_layout_set_font_description(layout,
						goban->font_description);
	      pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	      pango_layout_set_width(layout, cell_size - 2);
	    }

	    pango_layout_set_text(layout, goban->sgf_labels[pos], -1);
	    gdk_draw_layout(widget->window, gc,
			    goban->first_cell_center_x + x * cell_size,
			    (goban->first_cell_center_y
			     - goban->character_height / 2
			     + y * cell_size),
			    layout);
	  }
	}
      }
    }

    gdk_region_destroy(row_region);
  }

  if (layout)
    g_object_unref(layout);

  if (!IS_NULL_POINT(goban->last_move_x, goban->last_move_y)) {
    if (goban->grid[POSITION(goban->last_move_x, goban->last_move_y)] == BLACK)
      gc = widget->style->white_gc;
    else
      gc = widget->style->black_gc;

    x = goban->first_cell_center_x + goban->last_move_x * cell_size;
    y = goban->first_cell_center_y + goban->last_move_y * cell_size;

    gdk_draw_line(window, gc, x - 3, y, x + 3, y);
    gdk_draw_line(window, gc, x, y - 3, x, y + 3);
  }

  return FALSE;
}


static gboolean
gtk_goban_button_press_event(GtkWidget *widget, GdkEventButton *event)
{
  if (event->button == 1 || event->button == 3) {
    GtkGoban *goban = GTK_GOBAN(widget);
    int x;
    int y;
    GdkModifierType modifiers;

    gdk_window_get_pointer(widget->window, &x, &y, &modifiers);
    widget_coordinates_to_board(goban, x, y, &x, &y);

    if (!IS_NULL_POINT(x, y)) {
      if (goban->button_pressed == 0) {
	goban->button_pressed	      = event->button;
	goban->press_x		      = x;
	goban->press_y		      = y;
	goban->press_modifiers	      = modifiers & MODIFIER_MASK;
	goban->feedback_tile_at_press = goban->feedback_tile;
      }
      else
	goban->button_pressed = -1;

      emit_pointer_moved(goban, x, y, modifiers & MODIFIER_MASK);
    }
    else if (goban->button_pressed == 0 && !GTK_WIDGET_HAS_FOCUS(widget))
      gtk_widget_grab_focus(widget);
  }

  return FALSE;
}


static gboolean
gtk_goban_button_release_event(GtkWidget *widget, GdkEventButton *event)
{
  if (event->button == 1 || event->button == 3) {
    GtkGoban *goban = GTK_GOBAN(widget);
    GdkModifierType modifiers;
    GdkModifierType button_mask = (event->button == 1
				   ? GDK_BUTTON1_MASK : GDK_BUTTON3_MASK);
    GtkGobanClickData data;

    gdk_window_get_pointer(widget->window, &data.x, &data.y, &modifiers);
    widget_coordinates_to_board(goban, data.x, data.y, &data.x, &data.y);

    if (goban->button_pressed == event->button) {
      goban->button_pressed = 0;

      if (data.x == goban->press_x && data.y == goban->press_y
	  && (((modifiers & MODIFIER_MASK) | button_mask)
	      == goban->press_modifiers)) {
	data.feedback_tile = goban->feedback_tile;
	data.button	   = event->button;
	data.modifiers	   = modifiers & ~button_mask;
	g_signal_emit(G_OBJECT(goban), goban_signals[GOBAN_CLICKED], 0, &data);
      }
    }
    else {
      if (!(modifiers & (GDK_BUTTON1_MASK | GDK_BUTTON3_MASK)))
	goban->button_pressed = 0;
    }

    emit_pointer_moved(goban, data.x, data.y, modifiers & MODIFIER_MASK);
  }

  return FALSE;
}


static gboolean
gtk_goban_motion_notify_event(GtkWidget *widget, GdkEventMotion *event)
{
  GtkGoban *goban = GTK_GOBAN(widget);
  int x;
  int y;

  widget_coordinates_to_board(goban, event->x, event->y, &x, &y);
  if (x != goban->pointer_x || y != goban->pointer_y
      || (event->state & MODIFIER_MASK) != goban->modifiers)
    emit_pointer_moved(goban, x, y, event->state & MODIFIER_MASK);

  return FALSE;
}


static gboolean
gtk_goban_enter_notify_event(GtkWidget *widget, GdkEventCrossing *event)
{
  GtkGoban *goban = GTK_GOBAN(widget);
  int x;
  int y;

  widget_coordinates_to_board(goban, event->x, event->y, &x, &y);
  emit_pointer_moved(goban, x, y, event->state & MODIFIER_MASK);

  return FALSE;
}


static gboolean
gtk_goban_leave_notify_event(GtkWidget *widget, GdkEventCrossing *event)
{
  GtkGoban *goban = GTK_GOBAN(widget);

  UNUSED(event);

  if (goban->button_pressed)
    goban->button_pressed = 0;

  set_feedback_data(goban, NULL_X, NULL_Y, NULL, GOBAN_FEEDBACK_NONE);

  return FALSE;
}


/* Navigate goban in respond to mouse wheel movement.  Default is
 * navigating back/forth by moves.  Shift causes switching between
 * variations, Ctrl makes navigation fast.
 */
static gboolean
gtk_goban_scroll_event(GtkWidget *widget, GdkEventScroll *event)
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

  g_signal_emit(G_OBJECT(widget), goban_signals[NAVIGATE], 0, direction);

  return FALSE;
}


static gboolean
gtk_goban_key_press_event(GtkWidget *widget, GdkEventKey *event)
{
  GtkGoban *goban = GTK_GOBAN(widget);
  GdkModifierType modifiers;

  gdk_window_get_pointer(widget->window, NULL, NULL, &modifiers);
  if ((modifiers & MODIFIER_MASK) != goban->modifiers) {
    emit_pointer_moved(goban, goban->pointer_x, goban->pointer_y,
		       modifiers & MODIFIER_MASK);
  }

  return parent_class->key_press_event(widget, event);
}


static gboolean
gtk_goban_key_release_event(GtkWidget *widget, GdkEventKey *event)
{
  GtkGoban *goban = GTK_GOBAN(widget);
  GdkModifierType modifiers;

  gdk_window_get_pointer(widget->window, NULL, NULL, &modifiers);
  if ((modifiers & MODIFIER_MASK) != goban->modifiers) {
    emit_pointer_moved(goban, goban->pointer_x, goban->pointer_y,
		       modifiers & MODIFIER_MASK);
  }

  return parent_class->key_release_event(widget, event);
}


static gboolean
gtk_goban_focus_in_or_out_event(GtkWidget *widget, GdkEventFocus *event)
{
  GtkGoban *goban = GTK_GOBAN(widget);
  GdkRectangle rectangle = { goban->coordinates_x_left - 1,
			     (goban->coordinates_y_top
			      + goban->character_height / 2
			      - 4),
			     8, 8 };

  UNUSED(event);

  gdk_window_invalidate_rect(widget->window, &rectangle, FALSE);

  /* Don't fallback to default handler, since we don't need full
   * widget redraw.
   */
  return FALSE;
}


static void
gtk_goban_finalize(GObject *object)
{
  GtkGoban *goban = GTK_GOBAN(object);

  if (goban->width && goban->height) {
    int x;
    int y;

    for (y = 0; y < goban->height; y++) {
      for (x = 0; x < goban->width; x++)
	g_free(goban->sgf_labels[POSITION(x, y)]);
    }
  }

  if (goban->font_description)
    pango_font_description_free(goban->font_description);

  unreference_cached_objects(goban);

  if (goban->checkerboard_pattern_object)
    g_object_unref(goban->checkerboard_pattern_object);

  all_gobans = g_slist_remove(all_gobans, goban);

  G_OBJECT_CLASS(parent_class)->finalize(object);
}


static void
unreference_cached_objects(GtkGoban *goban)
{
  if (goban->main_tile_set) {
    object_cache_unreference_object(&gtk_main_tile_set_cache,
				    goban->main_tile_set);
  }

  if (goban->small_tile_set) {
    object_cache_unreference_object(&gtk_main_tile_set_cache,
				    goban->small_tile_set);
  }

  if (goban->sgf_markup_tile_set) {
    object_cache_unreference_object(&gtk_sgf_markup_tile_set_cache,
				    goban->sgf_markup_tile_set);
  }
}



static void
set_goban_style_appearance(GtkGoban *goban)
{
  const BoardAppearance *board_appearance
    = game_to_board_appearance_structure(goban->game);
  GtkRcStyle *rc_style = gtk_rc_style_new();

  rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_FG | GTK_RC_BG;
  gtk_utils_set_gdk_color(&rc_style->fg[GTK_STATE_NORMAL],
			  board_appearance->grid_and_labels_color);
  gtk_utils_set_gdk_color(&rc_style->bg[GTK_STATE_NORMAL],
			  board_appearance->background_color);

  if (board_appearance->use_background_texture) {
    rc_style->bg_pixmap_name[GTK_STATE_NORMAL]
      = g_strdup(board_appearance->background_texture);
  }

  gtk_widget_modify_style(GTK_WIDGET(goban), rc_style);
  g_object_unref(rc_style);
}


static void
set_goban_non_style_appearance(GtkGoban *goban)
{
  Game game = goban->game;

  if (goban->cell_size == 0)
    return;

  if (goban->checkerboard_pattern_object) {
    g_object_unref(goban->checkerboard_pattern_object);
    goban->checkerboard_pattern_object = NULL;
  }

  if (game == GAME_AMAZONS && GTK_WIDGET_REALIZED(goban)
      && goban->cell_size > 0) {
    QuarryColor color = amazons_board_appearance.checkerboard_pattern_color;
    double opacity = amazons_board_appearance.checkerboard_pattern_opacity;
    guint8 actual_opacity = G_MAXUINT8 * CLAMP(opacity, 0.0, 1.0);

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
	  = (GObject *) gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
				       goban->cell_size - 1,
				       goban->cell_size - 1);
	gdk_pixbuf_fill(GDK_PIXBUF(goban->checkerboard_pattern_object),
			(actual_opacity
			 | (color.red << 24)
			 | (color.green << 16)
			 | (color.blue << 8)));
      }
      else {
	GdkGC *gc = gdk_gc_new(goban->widget.window);
	GdkColor gdk_color;

	gtk_utils_set_gdk_color(&gdk_color, color);
	gdk_gc_set_rgb_fg_color(gc, &gdk_color);

	goban->checkerboard_pattern_object
	  = (GObject *) gdk_pixmap_new(goban->widget.window,
				       goban->cell_size - 1,
				       goban->cell_size - 1,
				       -1);
	gdk_draw_rectangle(GDK_DRAWABLE(goban->checkerboard_pattern_object),
			   gc, TRUE,
			   0, 0, goban->cell_size - 1, goban->cell_size - 1);

	g_object_unref(gc);
      }
    }
  }

  object_cache_unreference_object(&gtk_sgf_markup_tile_set_cache,
				  goban->sgf_markup_tile_set);
  goban->sgf_markup_tile_set
    = gtk_sgf_markup_tile_set_create_or_reuse((goban->cell_size
					       - (game == GAME_GO ? 0 : 1)),
					      game);
}


gint
gtk_goban_negotiate_width(GtkWidget *widget, gint height)
{
  UNUSED(widget);
  UNUSED(height);

  return height;
}


gint
gtk_goban_negotiate_height(GtkWidget *widget, gint width)
{
  UNUSED(widget);
  UNUSED(width);

  return width;
}


void
gtk_goban_update(GtkGoban *goban,
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

  assert(GTK_IS_GOBAN(goban));

  for (k = NUM_OVERLAYS; --k >= 0;)
    set_overlay_data(goban, k, NULL, TILE_NONE, TILE_NONE);

  if (!grid)
    grid = goban->grid;
  if (!goban_markup)
    goban_markup = goban->goban_markup;
  if (!sgf_markup)
    sgf_markup = goban->sgf_markup;

  widget = GTK_WIDGET(goban);

  width	    = goban->width;
  height    = goban->height;
  cell_size = goban->cell_size;

  if (GTK_WIDGET_REALIZED(widget)) {
    GdkRectangle rectangle_stone;
    GdkRectangle rectangle_markup;
    GdkRectangle rectangle_sgf_markup;
    GdkRectangle rectangle_sgf_label;
    const SgfLabel *sgf_label = NULL;
    const SgfLabel *sgf_labels_limit = NULL;

    if (sgf_label_list && sgf_label_list != KEEP_SGF_LABELS) {
      sgf_label = sgf_label_list->labels;
      sgf_labels_limit = sgf_label_list->labels + sgf_label_list->num_labels;
    }

    rectangle_stone.width	= goban->main_tile_set->tile_size;
    rectangle_stone.height	= goban->main_tile_set->tile_size;
    rectangle_markup.width	= goban->small_tile_set->tile_size;
    rectangle_markup.height	= goban->small_tile_set->tile_size;
    rectangle_sgf_markup.width	= goban->sgf_markup_tile_set->tile_size;
    rectangle_sgf_markup.height = goban->sgf_markup_tile_set->tile_size;
    rectangle_sgf_label.width	= cell_size - 2;
    rectangle_sgf_label.height	= goban->character_height;

    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
	int pos = POSITION(x, y);

	if (goban->grid[pos] != grid[pos]
	    || ((goban->goban_markup[pos] & GOBAN_MARKUP_FLAGS_MASK)
		!= (goban_markup[pos] & GOBAN_MARKUP_FLAGS_MASK))) {
	  rectangle_stone.x = goban->stones_left_margin + x * cell_size;
	  rectangle_stone.y = goban->stones_top_margin + y * cell_size;
	  gdk_window_invalidate_rect(widget->window, &rectangle_stone, FALSE);

	  goban->grid[pos]	   = grid[pos];
	  goban->goban_markup[pos] = goban_markup[pos];
	  goban->sgf_markup[pos]   = sgf_markup[pos];
	}
	else if (goban->sgf_markup[pos] != sgf_markup[pos]) {
	  rectangle_sgf_markup.x = (goban->sgf_markup_left_margin
				    + x * cell_size);
	  rectangle_sgf_markup.y = (goban->sgf_markup_top_margin
				    + y * cell_size);
	  gdk_window_invalidate_rect(widget->window, &rectangle_sgf_markup,
				     FALSE);

	  goban->goban_markup[pos] = goban_markup[pos];
	  goban->sgf_markup[pos] = sgf_markup[pos];
	}
	else if (goban->goban_markup[pos] != goban_markup[pos]) {
	  rectangle_markup.x = goban->small_stones_left_margin + x * cell_size;
	  rectangle_markup.y = goban->small_stones_top_margin + y * cell_size;
	  gdk_window_invalidate_rect(widget->window, &rectangle_markup, FALSE);

	  goban->goban_markup[pos] = goban_markup[pos];
	}

	if (sgf_label_list != KEEP_SGF_LABELS) {
	  char *label_text = ((sgf_label != sgf_labels_limit
			       && sgf_label->point.x == x
			       && sgf_label->point.y == y)
			      ? sgf_label->text : NULL);

	  if (goban->sgf_labels[pos]
	      ? (!label_text
		 || strcmp(goban->sgf_labels[pos], label_text) != 0)
	      : label_text != NULL) {
	    rectangle_sgf_label.x = (goban->first_cell_center_x
				     - cell_size / 2
				     + x * cell_size);
	    rectangle_sgf_label.y = (goban->first_cell_center_y
				     - goban->character_height / 2
				     + y * cell_size);
	    gdk_window_invalidate_rect(widget->window, &rectangle_sgf_label,
				       FALSE);

	    g_free(goban->sgf_labels[pos]);
	    goban->sgf_labels[pos] = (label_text
				      ? g_strdup(label_text) : NULL);
	  }

	  if (label_text)
	    sgf_label++;
	}
      }
    }
  }
  else {
    grid_copy(goban->grid, grid, width, height);
    grid_copy(goban->goban_markup, goban_markup, width, height);
    grid_copy(goban->sgf_markup, sgf_markup, width, height);

    if (sgf_label_list != KEEP_SGF_LABELS) {
      for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	  int pos = POSITION(x, y);

	  g_free(goban->sgf_labels[pos]);
	  goban->sgf_labels[pos] = NULL;
	}
      }

      if (sgf_label_list) {
	for (k = 0; k < sgf_label_list->num_labels; k++) {
	  int pos = POINT_TO_POSITION(sgf_label_list->labels[k].point);

	  goban->sgf_labels[pos] = g_strdup(sgf_label_list->labels[k].text);
	}
      }
    }
  }

  if (goban->last_move_x != last_move_x || goban->last_move_y != last_move_y) {
    if (GTK_WIDGET_REALIZED(widget)) {
      GdkRectangle rectangle;
      int pos = POSITION(last_move_x, last_move_y);

      rectangle.width = 7;
      rectangle.height = 7;

      if (!IS_NULL_POINT(goban->last_move_x, goban->last_move_y)) {
	rectangle.x = (goban->first_cell_center_x
		       + goban->last_move_x * cell_size - 3);
	rectangle.y = (goban->first_cell_center_y
		       + goban->last_move_y * cell_size - 3);
	gdk_window_invalidate_rect(widget->window, &rectangle, FALSE);
      }

      if (ON_SIZED_GRID(width, height, last_move_x, last_move_y)
	  && goban->grid[pos] == grid[pos]) {
	rectangle.x = goban->first_cell_center_x + last_move_x * cell_size - 3;
	rectangle.y = goban->first_cell_center_y + last_move_y * cell_size - 3;
	gdk_window_invalidate_rect(widget->window, &rectangle, FALSE);
      }
    }

    if (ON_SIZED_GRID(width, height, last_move_x, last_move_y)) {
      goban->last_move_x = last_move_x;
      goban->last_move_y = last_move_y;
    }
    else {
      goban->last_move_x = NULL_X;
      goban->last_move_y = NULL_Y;
    }
  }
}


void
gtk_goban_force_feedback_poll(GtkGoban *goban)
{
  emit_pointer_moved(goban, goban->pointer_x, goban->pointer_y,
		     goban->modifiers);
}


void
gtk_goban_set_overlay_data(GtkGoban *goban, int overlay_index,
			   BoardPositionList *position_list,
			   int tile, int goban_markup_tile)
{
  const BoardPositionList *feedback_position_list
    = goban->overlay_positon_lists[FEEDBACK_OVERLAY];
  int need_feedback_poll
    = (position_list && feedback_position_list
       && board_position_lists_overlap(feedback_position_list, position_list));

  assert(goban);
  assert(0 <= overlay_index && overlay_index < NUM_OVERLAYS
	 && overlay_index != FEEDBACK_OVERLAY);
  assert((0 <= tile && tile < NUM_TILES) || tile == GOBAN_TILE_DONT_CHANGE);
  assert((0 <= goban_markup_tile && goban_markup_tile < NUM_TILES)
	 || goban_markup_tile == GOBAN_TILE_DONT_CHANGE);
  assert(tile != GOBAN_TILE_DONT_CHANGE
	 || goban_markup_tile != GOBAN_TILE_DONT_CHANGE);

  if (need_feedback_poll)
    set_overlay_data(goban, FEEDBACK_OVERLAY, NULL, TILE_NONE, TILE_NONE);

  set_overlay_data(goban, overlay_index, position_list,
		   tile, goban_markup_tile);

  if (need_feedback_poll) {
    emit_pointer_moved(goban, goban->pointer_x, goban->pointer_y,
		       goban->modifiers);
  }
}



void
gtk_goban_set_contents(GtkGoban *goban, BoardPositionList *position_list,
		       int grid_contents, int goban_markup_contents)
{
  int k;

  assert(GTK_IS_GOBAN(goban));
  assert(goban->game != GAME_DUMMY);

  for (k = NUM_OVERLAYS; --k >= 0;)
    set_overlay_data(goban, k, NULL, TILE_NONE, TILE_NONE);

  set_overlay_data(goban, 0, position_list,
		   grid_contents, goban_markup_contents);
  goban->overlay_positon_lists[0] = NULL;
}


int
gtk_goban_get_grid_contents(GtkGoban *goban, int x, int y)
{
  int k;
  int pos = POSITION(x, y);

  assert(GTK_IS_GOBAN(goban));
  assert(goban->game != GAME_DUMMY);
  assert(ON_SIZED_GRID(goban->width, goban->height, x, y));

  for (k = NUM_OVERLAYS; --k >= 0;) {
    if (goban->overlay_positon_lists[k]) {
      int position_index
	= board_position_list_find_position(goban->overlay_positon_lists[k],
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

  assert(GTK_IS_GOBAN(goban));
  assert(grid);

  for (k = 0; k < NUM_ON_GRID_VALUES; k++)
    num_positions[k] = 0;

  for (y = 0; y < goban->height; y++) {
    for (x = 0; x < goban->width; x++) {
      int pos = POSITION(x, y);
      int contents = goban->grid[pos];

      if (contents != grid[pos]) {
	assert(0 <= contents && contents < NUM_ON_GRID_VALUES);

	positions[contents][num_positions[contents]++] = pos;
      }
    }
  }

  for (k = 0; k < NUM_ON_GRID_VALUES; k++) {
    if (num_positions[k] > 0) {
      position_lists[k] = board_position_list_new(positions[k],
						  num_positions[k]);
    }
    else
      position_lists[k] = NULL;
  }
}


void
gtk_goban_update_appearance(Game game)
{
  GSList *item;

  assert(game >= FIRST_GAME && GAME_IS_SUPPORTED(game));

  for (item = all_gobans; item; item = item->next) {
    GtkGoban *goban = (GtkGoban *) (item->data);

    if (goban->game == game) {
      set_goban_style_appearance(goban);
      set_goban_non_style_appearance(goban);
    }
  }
}


static void
emit_pointer_moved(GtkGoban *goban, int x, int y, GdkModifierType modifiers)
{
  if (ON_SIZED_GRID(goban->width, goban->height, x, y)
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

    g_signal_emit(G_OBJECT(goban), goban_signals[POINTER_MOVED], 0,
		  &data, &feedback);

    assert(data.button
	   || ((feedback & GOBAN_FEEDBACK_GRID_MASK)
	       != GOBAN_FEEDBACK_PRESS_DEFAULT));

    if (data.feedback_position_list)
      set_feedback_data(goban, x, y, data.feedback_position_list, feedback);
    else {
      int pos = POSITION(x, y);

      set_feedback_data(goban, x, y, board_position_list_new(&pos, 1),
			feedback);
    }
  }
  else
    set_feedback_data(goban, NULL_X, NULL_Y, NULL, GOBAN_FEEDBACK_NONE);

  goban->modifiers = modifiers;
}


static void
set_feedback_data(GtkGoban *goban,
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

      assert(position_list->num_positions == 1);

      if (COLOR_INDEX(goban->grid[POSITION(x, y)]) != color_index)
	feedback_tile = STONE_50_TRANSPARENT + color_index;
      else
	feedback_tile = STONE_25_TRANSPARENT + color_index;
    }

    break;

  case GOBAN_FEEDBACK_ADD_BLACK_OR_REMOVE:
  case GOBAN_FEEDBACK_ADD_WHITE_OR_REMOVE:
    {
      int contents = gtk_goban_get_grid_contents(goban, x, y);

      if (contents == EMPTY) {
	feedback_tile = (STONE_50_TRANSPARENT
			 + (feedback_grid - GOBAN_FEEDBACK_ADD_OR_REMOVE));
      }
      else if (IS_STONE(contents))
	feedback_tile = STONE_25_TRANSPARENT + COLOR_INDEX(contents);
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
    set_overlay_data(goban, FEEDBACK_OVERLAY,
		     position_list, feedback_tile, goban_markup_feedback_tile);
  }
  else {
    if (position_list)
      board_position_list_delete(position_list);

    set_overlay_data(goban, FEEDBACK_OVERLAY, NULL, TILE_NONE, TILE_NONE);
  }

  goban->pointer_x     = x;
  goban->pointer_y     = y;
  goban->feedback_tile = feedback_tile;
}


static void
set_overlay_data(GtkGoban *goban, int overlay_index,
		 BoardPositionList *position_list,
		 int tile, int goban_markup_tile)
{
  GtkWidget *widget = GTK_WIDGET(goban);
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

  if (GTK_WIDGET_REALIZED(widget)) {
    int cell_size = goban->cell_size;
    GdkRectangle rectangle_stone;
    GdkRectangle rectangle_markup;

    rectangle_stone.width   = goban->main_tile_set->tile_size;
    rectangle_stone.height  = goban->main_tile_set->tile_size;
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

      if (new_tile != grid[pos] || new_goban_markup_tile != goban_markup[pos]) {
	int x = POSITION_X(pos);
	int y = POSITION_Y(pos);

	if (new_tile != grid[pos]
	    || (new_goban_markup_tile & GOBAN_MARKUP_FLAGS_MASK)
	    || (goban_markup[pos] & GOBAN_MARKUP_FLAGS_MASK)) {
	  rectangle_stone.x = goban->stones_left_margin + x * cell_size;
	  rectangle_stone.y = goban->stones_top_margin + y * cell_size;
	  gdk_window_invalidate_rect(widget->window, &rectangle_stone, FALSE);
	}
	else {
	  rectangle_markup.x = goban->small_stones_left_margin + x * cell_size;
	  rectangle_markup.y = goban->small_stones_top_margin + y * cell_size;
	  gdk_window_invalidate_rect(widget->window, &rectangle_markup, FALSE);
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

    board_position_list_delete(old_position_list);
    g_free(old_overlay_contents);
  }

  goban->overlay_positon_lists[overlay_index] = position_list;
  if (position_list) {
    goban->overlay_contents[overlay_index] = g_malloc((2 * num_new_positions)
						      * sizeof(char));

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
find_hoshi_points(GtkGoban *goban)
{
  int edge_distance_x = (goban->width > 11 ? 3 : 2);
  int edge_distance_y = (goban->height > 11 ? 3 : 2);

  if (goban->game != GAME_GO
      || goban->width < 5 || goban->width == 6
      || goban->height < 5 || goban->height == 6)
    goban->num_hoshi_points = 0;
  else {
    int k;
    int num_points;
    int point_x[9];
    int point_y[9];

    point_x[0] = edge_distance_x;
    point_y[0] = edge_distance_y;

    point_x[1] = goban->width - 1 - edge_distance_x;
    point_y[1] = edge_distance_y;

    point_x[2] = edge_distance_x;
    point_y[2] = goban->height - 1 - edge_distance_y;

    point_x[3] = goban->width - 1 - edge_distance_x;
    point_y[3] = goban->height - 1 - edge_distance_y;

    if (goban->width % 2 == 1 && goban->height % 2 == 1) {
      point_x[4] = goban->width / 2;
      point_y[4] = goban->height / 2;
      num_points = 5;
    }
    else
      num_points = 4;

    if (goban->width % 2 == 1 && goban->width >= 13) {
      point_x[num_points] = goban->width / 2;
      point_y[num_points++] = edge_distance_y;

      point_x[num_points] = goban->width / 2;
      point_y[num_points++] = goban->height - 1 - edge_distance_y;
    }

    if (goban->height % 2 == 1 && goban->height >= 13) {
      point_x[num_points] = edge_distance_x;
      point_y[num_points++] = goban->height / 2;

      point_x[num_points] = goban->width - 1 - edge_distance_x;
      point_y[num_points++] = goban->height / 2;
    }

    goban->num_hoshi_points = num_points;
    for (k = 0; k < num_points; k++) {
      goban->hoshi_point_x[k] = point_x[k] * goban->cell_size;
      goban->hoshi_point_y[k] = point_y[k] * goban->cell_size;
    }
  }
}


static void
widget_coordinates_to_board(GtkGoban *goban, int window_x, int window_y,
			    int *board_x, int *board_y)
{
  window_x -= goban->first_cell_center_x - goban->cell_size / 2;
  window_y -= goban->first_cell_center_y - goban->cell_size / 2;

  if (window_x >= 0 && window_y >= 0) {
    *board_x = window_x / goban->cell_size;
    *board_y = window_y / goban->cell_size;

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
