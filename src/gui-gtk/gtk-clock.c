/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004 Paul Pogonyshev.                             *
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

/* A widget that shows time for a player.  Its LED-like look is
 * inspired by KDE system clock (its "digital" variant), but it shares
 * no code with KDE's widget, if only because that one is based on Qt.
 */


#include "gtk-clock.h"
#include "gui-back-end.h"
#include "time-control.h"

#include <assert.h>
#include <math.h>
#include <gtk/gtk.h>


#define CLOCK_DIGIT_WIDTH(symbol_parameters)			\
  ((symbol_parameters).segment_length + 2			\
   + (symbol_parameters).segment_thickness)

#define CLOCK_SEMICOLON_WIDTH(symbol_parameters)		\
  ((symbol_parameters).segment_thickness * 3 - 2		\
   + CLOCK_SHADOW_OFFSET_X(symbol_parameters))

#define CLOCK_SYMBOL_HEIGHT(symbol_parameters)			\
  (2 * (symbol_parameters).segment_length			\
   + CLOCK_SHADOW_OFFSET_Y(symbol_parameters))

#define CLOCK_SHADOW_OFFSET_X(symbol_parameters)		\
  ((3 * ((symbol_parameters).segment_thickness + 1)) / 4)

#define CLOCK_SHADOW_OFFSET_Y(symbol_parameters)		\
  (((symbol_parameters).segment_thickness + 1) / 2)


static void	 gtk_clock_class_init(GtkClockClass *class);
static void	 gtk_clock_init(GtkClock *clock);
static void	 gtk_clock_realize(GtkWidget *widget);

static void	 gtk_clock_size_request(GtkWidget *widget,
					GtkRequisition *requisition);

static gboolean	 gtk_clock_expose(GtkWidget *widget,
				  GdkEventExpose *event);

static void	 draw_led_symbol(GtkClockSymbolParameters *symbol_parameters,
				 GdkWindow *window, GdkGC *gc,
				 gint symbol, gint x0, gint y0);
static void	 draw_semicolon(GtkClockSymbolParameters *symbol_parameters,
				GdkWindow *window, GdkGC *gc,
				gint x0, gint y0);

static void	 gtk_clock_finalize(GObject *object);


static gchar *background_xpm[] = {
  "8 8 2 1",
  "x c #c6c7b9",
  "X c #9fa092",
  "xxxxxxxx",
  "XXXXXXXX",
  "xxxxxxxx",
  "XXXXXXXX",
  "xxxxxxxx",
  "XXXXXXXX",
  "xxxxxxxx",
  "XXXXXXXX",
};


static GdkPixmap  *background_pixmap = NULL;


GtkType
gtk_clock_get_type(void)
{
  static GtkType clock_type = 0;

  if (!clock_type) {
    static GTypeInfo clock_info = {
      sizeof(GtkClockClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_clock_class_init,
      NULL,
      NULL,
      sizeof(GtkClock),
      1,
      (GInstanceInitFunc) gtk_clock_init,
      NULL
    };

    clock_type = g_type_register_static(GTK_TYPE_WIDGET, "GtkClock",
					&clock_info, 0);
  }

  return clock_type;
}


static void
gtk_clock_class_init(GtkClockClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

  G_OBJECT_CLASS(class)->finalize = gtk_clock_finalize;

  widget_class->realize	     = gtk_clock_realize;
  widget_class->expose_event = gtk_clock_expose;
  widget_class->size_request = gtk_clock_size_request;
}


static void
gtk_clock_init(GtkClock *clock)
{
  clock->normal_symbol_parameters.segment_length    = 9;
  clock->normal_symbol_parameters.segment_thickness = 2;
  clock->small_symbol_parameters.segment_length	    = 7;
  clock->small_symbol_parameters.segment_thickness  = 2;

  clock->max_hours_positions = 0;
  clock->max_moves_positions = 0;

  clock->time_control = NULL;
}


GtkWidget *
gtk_clock_new(void)
{
  return GTK_WIDGET(g_object_new(GTK_TYPE_CLOCK, NULL));
}


static void
gtk_clock_realize(GtkWidget *widget)
{
  GdkWindowAttr attributes;
  static const gint attributes_mask = (GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL
				       | GDK_WA_COLORMAP);
  static const gint event_mask = GDK_EXPOSURE_MASK;

  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

  attributes.event_mask  = gtk_widget_get_events(widget) | event_mask;
  attributes.x		 = widget->allocation.x;
  attributes.y		 = widget->allocation.y;
  attributes.width	 = widget->allocation.width;
  attributes.height	 = widget->allocation.height;
  attributes.wclass	 = GDK_INPUT_OUTPUT;
  attributes.visual	 = gtk_widget_get_visual(widget);
  attributes.colormap	 = gtk_widget_get_colormap(widget);
  attributes.window_type = GDK_WINDOW_CHILD;

  widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
				  &attributes, attributes_mask);
  gdk_window_set_user_data(widget->window, widget);

  widget->style = gtk_style_attach(widget->style, widget->window);

  if (!background_pixmap) {
    background_pixmap = gdk_pixmap_create_from_xpm_d(widget->window, NULL, NULL,
						     ((gchar **)
						      background_xpm));
    gui_back_end_register_object_to_finalize(background_pixmap);
  }

  gdk_window_set_back_pixmap(widget->window, background_pixmap, FALSE);
}


static void
gtk_clock_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
  GtkClock *clock = GTK_CLOCK(widget);
  const gint normal_digit_width
    = CLOCK_DIGIT_WIDTH(clock->normal_symbol_parameters);

  requisition->width
    = (((clock->max_hours_positions + 4) * normal_digit_width)
       + ((clock->max_hours_positions > 0 ? 2 : 1)
	  * CLOCK_SEMICOLON_WIDTH(clock->normal_symbol_parameters))
       + 4 * clock->normal_symbol_parameters.segment_thickness);

  if (clock->max_moves_positions > 0) {
    requisition->width
      += (normal_digit_width
	  + (clock->max_moves_positions
	     * CLOCK_DIGIT_WIDTH(clock->small_symbol_parameters)));
  }

  requisition->height
    = (CLOCK_SYMBOL_HEIGHT(clock->normal_symbol_parameters)
       + 2 * (clock->normal_symbol_parameters.segment_thickness + 1)
       + CLOCK_SHADOW_OFFSET_Y(clock->normal_symbol_parameters));
}


static gboolean
gtk_clock_expose(GtkWidget *widget, GdkEventExpose *event)
{
  GtkClock *clock = GTK_CLOCK(widget);
  GdkGC *gc = gdk_gc_new(widget->window);

  const gint normal_digit_width
    = CLOCK_DIGIT_WIDTH(clock->normal_symbol_parameters);
  const gint normal_semicolon_width
    = CLOCK_SEMICOLON_WIDTH(clock->normal_symbol_parameters);
  const gint small_digit_width
    = CLOCK_DIGIT_WIDTH(clock->small_symbol_parameters);
  int pass;

  UNUSED(event);

  if (!GTK_WIDGET_DRAWABLE(widget))
    return FALSE;

  gdk_gc_set_line_attributes(gc, 0, GDK_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_ROUND);

  for (pass = 0; pass < 2; pass++) {
    static const GdkColor black_color = { 0, 0, 0, 0 };
    static const GdkColor gray_color = { 0, 0x8000, 0x8000, 0x8000 };
    int x = (widget->allocation.width + 1
	     - clock->normal_symbol_parameters.segment_thickness);
    int y = clock->normal_symbol_parameters.segment_thickness + 1;
    int k;

    gdk_gc_set_rgb_fg_color(gc, pass == 0 ? &gray_color : &black_color);

    if (clock->max_moves_positions > 0) {
      int x_moves = x;
      int y_moves = (y + CLOCK_SYMBOL_HEIGHT(clock->normal_symbol_parameters)
		     - CLOCK_SYMBOL_HEIGHT(clock->small_symbol_parameters));

      if (pass == 0)
	y_moves += CLOCK_SHADOW_OFFSET_Y(clock->small_symbol_parameters);
      else
	x_moves -= CLOCK_SHADOW_OFFSET_X(clock->small_symbol_parameters);

      if (clock->moves > 0) {
	k = clock->moves;

	do {
	  draw_led_symbol(&clock->small_symbol_parameters, widget->window, gc,
			  k % 10, x_moves -= small_digit_width, y_moves);
	} while ((k /= 10) > 0);
      }
      else {
	for (k = 0; k < clock->max_moves_positions; k++) {
	  draw_led_symbol(&clock->small_symbol_parameters, widget->window, gc,
			  -1, x_moves -= small_digit_width, y_moves);
	}
      }

      x -= clock->max_moves_positions * small_digit_width + normal_digit_width;
    }

    if (pass == 0)
      y += CLOCK_SHADOW_OFFSET_Y(clock->normal_symbol_parameters);
    else
      x -= CLOCK_SHADOW_OFFSET_X(clock->normal_symbol_parameters);

    k = (clock->seconds >= 0 ? clock->seconds : -60 * 60 + 1);

    /* Seconds. */
    draw_led_symbol(&clock->normal_symbol_parameters, widget->window, gc,
		    k % 10, x -= normal_digit_width, y);
    k /= 10;

    draw_led_symbol(&clock->normal_symbol_parameters, widget->window, gc,
		    k % 6, x -= normal_digit_width, y);
    k /= 6;

    /* A semicolon separating minutes and seconds. */
    draw_semicolon(&clock->normal_symbol_parameters, widget->window, gc,
		   x -= normal_semicolon_width, y);

    /* Minutes. */
    draw_led_symbol(&clock->normal_symbol_parameters, widget->window, gc,
		    k % 10, x -= normal_digit_width, y);
    k /= 10;

    draw_led_symbol(&clock->normal_symbol_parameters, widget->window, gc,
		    k % 6, x -= normal_digit_width, y);
    k /= 6;

    if (k > 0) {
      /* A semicolon separating hours and minutes. */
      draw_semicolon(&clock->normal_symbol_parameters, widget->window, gc,
		     x -= normal_semicolon_width, y);

      /* Hours. */
      do {
	draw_led_symbol(&clock->normal_symbol_parameters, widget->window, gc,
			k % 10, x -= normal_digit_width, y);
      } while ((k /= 10) > 0);
    }
  }

  g_object_unref(gc);

  return FALSE;
}


#define SET_POINT(index, _x, _y)			\
  (points[index].x = (_x), points[index].y = (_y))


static void
draw_led_symbol(GtkClockSymbolParameters *symbol_parameters,
		GdkWindow *window, GdkGC *gc, gint symbol, gint x0, gint y0)
{
  static const guchar segment_masks[11] = {
    0x77, 0x24, 0x5d, 0x6d, 0x2e, 0x6b, 0x7b, 0x25, 0x7f, 0x6f, 0x08
  };

  GdkPoint points[6];
  gint num_points;

  const gint segment_length = symbol_parameters->segment_length;
  const gint segment_thickness = symbol_parameters->segment_thickness - 1;
  const gint x1 = x0 + segment_length;
  const gint y1 = y0 + segment_length;
  const gint y2 = y0 + 2 * segment_length;
  const gint y1_up = y1 - (segment_thickness + 1) / 2;
  const gint y1_down = y1 + (segment_thickness) / 2;

  guchar segment_mask;
  gint segment_index;

  if (0 <= symbol && symbol <= 9)
    segment_mask = segment_masks[symbol];
  else
    segment_mask = segment_masks[10];

  for (segment_index = 0; segment_index < 7; segment_index++) {
    if (!(segment_mask & (1 << segment_index)))
      continue;

    if (segment_index != 3) {
      switch (segment_index) {
      case 0:
	SET_POINT(0, x0 + 1, y0);
	SET_POINT(1, x1 - 1, y0);
	SET_POINT(2, x1 - 1 - segment_thickness, y0 + segment_thickness);
	SET_POINT(3, x0 + 1 + segment_thickness, y0 + segment_thickness);
	break;

      case 1:
	SET_POINT(0, x0, y0 + 1);
	SET_POINT(1, x0, y1 - 1);
	SET_POINT(2, x0 + segment_thickness, y1_up - 1);
	SET_POINT(3, x0 + segment_thickness, y0 + 1 + segment_thickness);
	break;

      case 2:
	SET_POINT(0, x1, y0 + 1);
	SET_POINT(1, x1, y1 - 1);
	SET_POINT(2, x1 - segment_thickness, y1_up - 1);
	SET_POINT(3, x1 - segment_thickness, y0 + 1 + segment_thickness);
	break;

      case 4:
	SET_POINT(0, x0, y1 + 1);
	SET_POINT(1, x0, y2 - 1);
	SET_POINT(2, x0 + segment_thickness, y2 - 1 - segment_thickness);
	SET_POINT(3, x0 + segment_thickness, y1_down + 1);
	break;

      case 5:
	SET_POINT(0, x1, y1 + 1);
	SET_POINT(1, x1, y2 - 1);
	SET_POINT(2, x1 - segment_thickness, y2 - 1 - segment_thickness);
	SET_POINT(3, x1 - segment_thickness, y1_down + 1);
	break;

      case 6:
	SET_POINT(0, x0 + 1, y2);
	SET_POINT(1, x1 - 1, y2);
	SET_POINT(2, x1 - 1 - segment_thickness, y2 - segment_thickness);
	SET_POINT(3, x0 + 1 + segment_thickness, y2 - segment_thickness);
	break;
      }

      num_points = 4;
    }
    else {
      SET_POINT(0, x0 + 2, y1);
      SET_POINT(1, x0 + 1 + segment_thickness, y1_up);
      SET_POINT(2, x1 - 1 - segment_thickness, y1_up);
      SET_POINT(3, x1 - 2, y1);
      SET_POINT(4, x1 - 1 - segment_thickness, y1_down);
      SET_POINT(5, x0 + 1 + segment_thickness, y1_down);

      num_points = 6;
    }

    /* This is an ugly piece of code, but filled polygons don't look
     * good.  Polygons that are both filled and outlined are much
     * nicer.
     */
    gdk_draw_polygon(window, gc, FALSE, points, num_points);
    gdk_draw_polygon(window, gc, TRUE, points, num_points);
  }
}


static void
draw_semicolon(GtkClockSymbolParameters *symbol_parameters,
	       GdkWindow *window, GdkGC *gc, gint x0, gint y0)
{
  const gint segment_length = symbol_parameters->segment_length;
  const gint segment_thickness = symbol_parameters->segment_thickness;
  int k;

  for (k = 0, x0 += segment_thickness - 1, y0 += segment_length / 2;
       k < 2; k++, y0 += segment_length) {
    GdkPoint points[4];

    SET_POINT(0, x0, y0);
    SET_POINT(1, x0 + segment_thickness, y0);
    SET_POINT(2, x0 + segment_thickness, y0 + segment_thickness);
    SET_POINT(3, x0, y0 + segment_thickness);

    gdk_draw_polygon(window, gc, TRUE, points, 4);
  }
}


static gboolean
clock_set_time(gpointer user_data)
{
  GtkClock *clock = GTK_CLOCK(user_data);
  double seconds = time_control_get_clock_seconds(clock->time_control,
						  &clock->moves);

  clock->seconds = (seconds >= 0.0 ? ceil(seconds) : -1);
  gtk_widget_queue_draw(GTK_WIDGET(clock));

  return TRUE;
}


double
gtk_clock_start(GtkClock *clock, TimeControl *time_control)
{
  double full_time_remaining;

  assert(GTK_IS_CLOCK(clock));
  assert(time_control);
  assert(!clock->time_control);

  clock->time_control = time_control;
  full_time_remaining = time_control_start(time_control);

  /* FIXME: improve. */
  clock->source_id = g_timeout_add(500, clock_set_time, clock);

  return full_time_remaining;
}


int
gtk_clock_stop(GtkClock *clock)
{
  assert(GTK_IS_CLOCK(clock));
  assert(clock->time_control);

  time_control_stop(clock->time_control, NULL);
  clock_set_time(clock);
  clock->time_control = NULL;

  g_source_remove(clock->source_id);

  return 0;
}


void
gtk_clock_initialize_for_time_control(GtkClock *clock,
				      const TimeControl *time_control)
{
  int value;

  assert(GTK_IS_CLOCK(clock));
  assert(time_control);

  for (clock->max_hours_positions = 0,
	 value = (MAX(time_control->main_time, time_control->overtime_length)
		  / (60 * 60));
       value > 0; value /= 10)
    clock->max_hours_positions++;

  for (clock->max_moves_positions = 0,
	 value = time_control->moves_per_overtime;
       value > 0; value /= 10)
    clock->max_moves_positions++;

  clock->seconds = floor(time_control_get_clock_seconds(time_control,
							&clock->moves));

  gtk_widget_queue_resize(GTK_WIDGET(clock));
}


static void
gtk_clock_finalize(GObject *object)
{
  GtkClock *clock = GTK_CLOCK(object);

  if (clock->time_control)
    g_source_remove(clock->source_id);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
