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


#include "gtk-goban.h"
#include "gtk-utils.h"
#include "gtk-tile-set-interface.h"
#include "quarry-marshal.h"
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

static gboolean	 gtk_goban_key_press_event(GtkWidget *widget,
					   GdkEventKey *event);
static gboolean	 gtk_goban_key_release_event(GtkWidget *widget,
					     GdkEventKey *event);

static void	 gtk_goban_finalize(GObject *object);

/* FIXME: Doesn't belong here.  And name is bad too. */
static void	 find_hoshi_points(GtkGoban *goban);

static void	 emit_pointer_moved(GtkGoban *goban, int x, int y,
				    GdkModifierType modifiers);
static void	 set_feedback_data(GtkGoban *goban, int x, int y,
				   GtkGobanPointerFeedback feedback);

static void	 set_overlay_data(GtkGoban *goban, int overlay_index,
				  int x, int y, int tile);

static void	 widget_coordinates_to_board(GtkGoban *goban,
					     int window_x, int window_y,
					     int *board_x, int *board_y);


static GtkWidgetClass  *parent_class;
static GtkStyle	       *goban_style;

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
  GdkPixbuf *pixbuf;
  GdkPixmap *pixmap;
  GtkBindingSet *binding_set;

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
  widget_class->key_press_event	     = gtk_goban_key_press_event;
  widget_class->key_release_event    = gtk_goban_key_release_event;

  class->pointer_moved = NULL;
  class->goban_clicked = NULL;
  class->navigate      = NULL;

  goban_style = gtk_style_new();

  assert(pixbuf = gdk_pixbuf_new_from_file(PACKAGE_TEXTURES_DIR "/wood1.jpg",
					   NULL));
  gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, NULL, 0);
  g_object_unref(pixbuf);

  goban_style->bg_pixmap[GTK_STATE_NORMAL] = pixmap;

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

  goban->font_description = pango_font_description_copy(goban_style->font_desc);
  goban->font_size = 0;

  goban->main_tile_set = NULL;
  goban->small_tile_set = NULL;

  goban->last_move_x = NULL_X;
  goban->last_move_y = NULL_Y;

  for (k = 0; k < NUM_OVERLAYS; k++)
    goban->overlay_pos[k] = NULL_POSITION;

  set_feedback_data(goban, NULL_X, NULL_Y, GOBAN_FEEDBACK_NONE);
}


GtkWidget *
gtk_goban_new(void)
{
  return GTK_WIDGET(g_object_new(GTK_TYPE_GOBAN, NULL));
}


void
gtk_goban_set_parameters(GtkGoban *goban, Game game, int width, int height)
{
  assert(GTK_IS_GOBAN(goban));
  assert(GAME_IS_SUPPORTED(game));
  assert(width > 0 && height > 0);

  goban->game	= game;
  goban->width	= width;
  goban->height = height;
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

  g_object_ref(goban_style);
  g_object_ref(goban_style->bg_pixmap[GTK_STATE_NORMAL]);
  widget->style = gtk_style_attach(goban_style, widget->window);
  gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);
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
    goban->cell_size = cell_size;
    goban->small_cell_size = (2 * cell_size - 1) / 3;

    if (goban->main_tile_set)
      tile_set_unreference(goban->main_tile_set);
    if (goban->small_tile_set)
      tile_set_unreference(goban->small_tile_set);

    goban->main_tile_set = tile_set_create_or_reuse(cell_size,
						    &tile_set_defaults);
    goban->small_tile_set = tile_set_create_or_reuse(goban->small_cell_size,
						     &tile_set_defaults);
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

  goban->stones_left_margin = (goban->left_margin
			       + goban->main_tile_set->stones_x_offset);
  goban->stones_top_margin = (goban->top_margin
			      + goban->main_tile_set->stones_y_offset);
  if (goban->game != GAME_GO) {
    goban->stones_left_margin += cell_size / 2;
    goban->stones_top_margin += cell_size / 2;
  }

  goban->small_stones_left_margin = (goban->stones_left_margin
				     - goban->main_tile_set->stones_x_offset
				     + goban->small_tile_set->stones_x_offset);
  goban->small_stones_top_margin = (goban->stones_top_margin
				    - goban->main_tile_set->stones_y_offset
				    + goban->small_tile_set->stones_y_offset);

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

  GtkGoban *goban = GTK_GOBAN(widget);
  int cell_size = goban->cell_size;

  GdkWindow *window = widget->window;
  GdkGC *gc = widget->style->black_gc;

  UNUSED(event);

  if (!GTK_WIDGET_DRAWABLE(widget))
    return FALSE;

  for (x = goban->left_margin; x <= goban->right_margin; x += cell_size) {
    gdk_draw_line(window, gc,
		  x, goban->top_margin, x, goban->bottom_margin);
  }

  for (y = goban->top_margin; y <= goban->bottom_margin; y += cell_size) {
    gdk_draw_line(window, gc,
		  goban->left_margin, y, goban->right_margin, y);
  }

  if (1) {
    gdk_draw_line(window, gc,
		  goban->left_margin - 1, goban->top_margin - 1,
		  goban->right_margin, goban->top_margin - 1);
    gdk_draw_line(window, gc,
		  goban->left_margin - 1, goban->top_margin,
		  goban->left_margin - 1, goban->bottom_margin + 1);
    gdk_draw_line(window, gc,
		  goban->left_margin, goban->bottom_margin + 1,
		  goban->right_margin + 1, goban->bottom_margin + 1);
    gdk_draw_line(window, gc,
		  goban->right_margin + 1, goban->top_margin - 1,
		  goban->right_margin + 1, goban->bottom_margin);
  }

  if (1) {
    PangoLayout *layout = gtk_widget_create_pango_layout(widget, NULL);
    const char *horizontal_coordinates
      = game_info[goban->game].horizontal_coordinates;

    pango_layout_set_font_description(layout, goban->font_description);
    pango_layout_set_width(layout, 2 * cell_size);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

    for (k = 0; k < goban->width; k++) {
      pango_layout_set_text(layout, horizontal_coordinates + k, 1);

      gdk_draw_layout(widget->window, gc,
		      goban->first_cell_center_x + k * cell_size,
		      goban->coordinates_y_top,
		      layout);
      gdk_draw_layout(widget->window, gc,
		      goban->first_cell_center_x + k * cell_size,
		      goban->coordinates_y_bottom,
		      layout);
    }

    for (k = 0; k < goban->height; k++) {
      char buffer[4];

      sprintf(buffer, "%d",
	      (game_info[goban->game].reversed_vertical_coordinates
	       ? goban->height - k : k + 1));
      pango_layout_set_text(layout, buffer, -1);

      gdk_draw_layout(widget->window, gc,
		      goban->coordinates_x_left,
		      goban->coordinates_y_side + k * cell_size,
		      layout);
      gdk_draw_layout(widget->window, gc,
		      goban->coordinates_x_right,
		      goban->coordinates_y_side + k * cell_size,
		      layout);
    }

    g_object_unref(layout);
  }

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

  for (y = 0; y < goban->height; y++) {
    for (x = 0; x < goban->width; x++) {
      int pos = POSITION(x, y);
      int tile = goban->grid[pos];
      int markup_tile = goban->goban_markup[pos] & GOBAN_MARKUP_TILE_MASK;

      if (tile != TILE_NONE && tile != TILE_SPECIAL) {
	if (IS_STONE(tile)) {
	  if (goban->goban_markup[pos] & GOBAN_MARKUP_GHOSTIFY)
	    tile = STONE_50_TRANSPARENT + COLOR_INDEX(tile);
	  else if (goban->goban_markup[pos] & GOBAN_MARKUP_GHOSTIFY_SLIGHTLY)
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
		      base_x, base_y, base_x + cell_size, base_y + cell_size);
	gdk_draw_line(window, gc,
		      base_x, base_y + cell_size, base_x + cell_size, base_y);
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
    }
  }

  if (!IS_NULL_POINT(goban->last_move_x, goban->last_move_y)) {
    if (goban->grid[POSITION(goban->last_move_x, goban->last_move_y)] == BLACK)
      gc = widget->style->white_gc;

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

  set_feedback_data(goban, NULL_X, NULL_Y, GOBAN_FEEDBACK_NONE);

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


static void
gtk_goban_finalize(GObject *object)
{
  GtkGoban *goban = GTK_GOBAN(object);

  if (goban->font_description)
    pango_font_description_free(goban->font_description);

  if (goban->main_tile_set)
    tile_set_unreference(goban->main_tile_set);
  if (goban->small_tile_set)
    tile_set_unreference(goban->small_tile_set);

  G_OBJECT_CLASS(parent_class)->finalize(object);
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
		 int last_move_x, int last_move_y)
{
  GtkWidget *widget;
  int cell_size;

  assert(GTK_IS_GOBAN(goban));

  if (grid) {
    int k;

    for (k = 0; k < NUM_OVERLAYS; k++)
      goban->overlay_pos[k] = NULL_POSITION;
  }
  else
    grid = goban->grid;

  if (!goban_markup)
    goban_markup = goban->goban_markup;
  if (!sgf_markup)
    sgf_markup = goban->sgf_markup;

  widget = GTK_WIDGET(goban);
  cell_size = goban->cell_size;

  if (GTK_WIDGET_REALIZED(widget)) {
    GdkRectangle rectangle_stone;
    GdkRectangle rectangle_markup;
    int x;
    int y;

    rectangle_stone.width   = cell_size;
    rectangle_stone.height  = cell_size;
    rectangle_markup.width  = goban->small_cell_size;
    rectangle_markup.height = goban->small_cell_size;

    for (y = 0; y < goban->height; y++) {
      for (x = 0; x < goban->width; x++) {
	int pos = POSITION(x, y);

	if (goban->grid[pos] != grid[pos]
	    || ((goban->goban_markup[pos] & GOBAN_MARKUP_FLAGS_MASK)
		!= (goban_markup[pos] & GOBAN_MARKUP_FLAGS_MASK))) {
	  rectangle_stone.x = goban->stones_left_margin + x * cell_size;
	  rectangle_stone.y = goban->stones_top_margin + y * cell_size;
	  gdk_window_invalidate_rect(widget->window, &rectangle_stone, FALSE);

	  goban->grid[pos] = grid[pos];
	  goban->goban_markup[pos] = goban_markup[pos];
	}
	else if (goban->goban_markup[pos] != goban_markup[pos]) {
	  rectangle_markup.x = goban->small_stones_left_margin + x * cell_size;
	  rectangle_markup.y = goban->small_stones_top_margin + y * cell_size;
	  gdk_window_invalidate_rect(widget->window, &rectangle_markup, FALSE);

	  goban->goban_markup[pos] = goban_markup[pos];
	}
      }
    }
  }
  else {
    memcpy(goban->grid, grid, sizeof(goban->grid));
    memcpy(goban->goban_markup, goban_markup, sizeof(goban->goban_markup));
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

      if (ON_SIZED_GRID(goban->width, goban->height, last_move_x, last_move_y)
	  && goban->grid[pos] == grid[pos]) {
	rectangle.x = goban->first_cell_center_x + last_move_x * cell_size - 3;
	rectangle.y = goban->first_cell_center_y + last_move_y * cell_size - 3;
	gdk_window_invalidate_rect(widget->window, &rectangle, FALSE);
      }
    }

    if (ON_SIZED_GRID(goban->width, goban->height, last_move_x, last_move_y)) {
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
			   int x, int y, int tile)
{
  int need_feedback_poll = (goban->overlay_pos[FEEDBACK_OVERLAY]
			    == POSITION(x, y));

  assert(goban);
  assert(0 <= overlay_index && overlay_index <= NUM_OVERLAYS
	 && overlay_index != FEEDBACK_OVERLAY);
  assert(0 <= tile && tile <= NUM_TILES);

  if (need_feedback_poll)
    set_overlay_data(goban, FEEDBACK_OVERLAY, NULL_X, NULL_Y, TILE_NONE);

  set_overlay_data(goban, overlay_index, x, y, tile);

  if (need_feedback_poll) {
    emit_pointer_moved(goban, goban->pointer_x, goban->pointer_y,
		       goban->modifiers);
  }
}


static void
emit_pointer_moved(GtkGoban *goban, int x, int y, GdkModifierType modifiers)
{
  if (ON_SIZED_GRID(goban->width, goban->height, x, y)
      && (goban->button_pressed == 0 || goban->press_modifiers == modifiers)) {
    GtkGobanPointerData data;
    GtkGobanPointerFeedback feedback;

    data.x	   = x;
    data.y	   = y;
    data.modifiers = modifiers;
    data.button    = goban->button_pressed;
    data.press_x   = goban->press_x;
    data.press_y   = goban->press_y;

    g_signal_emit(G_OBJECT(goban), goban_signals[POINTER_MOVED], 0,
		  &data, &feedback);

    assert(GOBAN_FEEDBACK_NONE <= feedback && feedback < NUM_GOBAN_FEEDBACKS);
    assert(feedback != GOBAN_FEEDBACK_PRESS_DEFAULT || data.button);

    set_feedback_data(goban, x, y, feedback);
  }
  else
    set_feedback_data(goban, NULL_X, NULL_Y, GOBAN_FEEDBACK_NONE);

  goban->modifiers = modifiers;
}


static void
set_feedback_data(GtkGoban *goban, int x, int y,
		  GtkGobanPointerFeedback feedback)
{
  int feedback_tile;

  switch (feedback) {
  case GOBAN_FEEDBACK_BLACK_GHOST:
  case GOBAN_FEEDBACK_WHITE_GHOST:
    feedback_tile = (STONE_50_TRANSPARENT + (feedback - GOBAN_FEEDBACK_GHOST));
    break;

  case GOBAN_FEEDBACK_THICK_BLACK_GHOST:
  case GOBAN_FEEDBACK_THICK_WHITE_GHOST:
    feedback_tile = (STONE_25_TRANSPARENT
		     + (feedback - GOBAN_FEEDBACK_THICK_GHOST));
    break;

  case GOBAN_FEEDBACK_PRESS_DEFAULT:
    feedback_tile = TILE_NONE;

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

  case GOBAN_FEEDBACK_BLACK_MOVE_DEFAULT:
  case GOBAN_FEEDBACK_WHITE_MOVE_DEFAULT:
    {
      int color_index = feedback - GOBAN_FEEDBACK_MOVE_DEFAULT;

      if (COLOR_INDEX(goban->grid[POSITION(x, y)]) != color_index)
	feedback_tile = STONE_50_TRANSPARENT + color_index;
      else
	feedback_tile = STONE_25_TRANSPARENT + color_index;
    }

    break;

  case GOBAN_FEEDBACK_SPECIAL:
    feedback_tile = TILE_SPECIAL;
    break;

  default:
    feedback_tile = TILE_NONE;
  }

  set_overlay_data(goban, FEEDBACK_OVERLAY, x, y, feedback_tile);

  goban->pointer_x     = x;
  goban->pointer_y     = y;
  goban->feedback_tile = feedback_tile;
}


static void
set_overlay_data(GtkGoban *goban, int overlay_index, int x, int y, int tile)
{
  GtkWidget *widget = GTK_WIDGET(goban);
  int new_pos = (tile != TILE_NONE ? POSITION(x, y) : NULL_POSITION);
  int old_pos = goban->overlay_pos[overlay_index];

  if (old_pos == new_pos
      && (new_pos == NULL_POSITION || goban->grid[new_pos] == tile))
    return;

  if (GTK_WIDGET_REALIZED(widget)) {
    int cell_size = goban->cell_size;
    GdkRectangle rectangle;

    rectangle.width = cell_size;
    rectangle.height = cell_size;

    if (old_pos != NULL_POSITION) {
      rectangle.x = goban->stones_left_margin + POSITION_X(old_pos) * cell_size;
      rectangle.y = goban->stones_top_margin + POSITION_Y(old_pos) * cell_size;
      gdk_window_invalidate_rect(widget->window, &rectangle, FALSE);
    }

    if (new_pos != NULL_POSITION && new_pos != old_pos) {
      rectangle.x = goban->stones_left_margin + x * cell_size;
      rectangle.y = goban->stones_top_margin + y * cell_size;
      gdk_window_invalidate_rect(widget->window, &rectangle, FALSE);
    }
  }

  if (old_pos != NULL_POSITION) {
    goban->grid[old_pos] = goban->overlay_contents[overlay_index];
  }

  goban->overlay_pos[overlay_index]	 = new_pos;
  goban->overlay_contents[overlay_index] = goban->grid[new_pos];
  goban->grid[new_pos]			 = tile;
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
