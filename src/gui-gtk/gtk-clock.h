/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004, 2005 Paul Pogonyshev.                       *
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


#ifndef QUARRY_GTK_CLOCK_H
#define QUARRY_GTK_CLOCK_H


#include "time-control.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_CLOCK		(gtk_clock_get_type ())
#define GTK_CLOCK(obj)		GTK_CHECK_CAST ((obj), GTK_TYPE_CLOCK,	\
						GtkClock)
#define GTK_CLOCK_CLASS(klass)						\
  GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CLOCK, GtkClockClass)

#define GTK_IS_CLOCK(obj)	GTK_CHECK_TYPE ((obj), GTK_TYPE_CLOCK)
#define GTK_IS_CLOCK_CLASS(klass)					\
  GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CLOCK)

#define GTK_CLOCK_GET_CLASS(obj)					\
  GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_CLOCK, GtkClockClass)


typedef struct _GtkClockSymbolParameters	GtkClockSymbolParameters;

typedef struct _GtkClock			GtkClock;
typedef struct _GtkClockClass			GtkClockClass;

typedef void (* GtkClockOutOfTimeCallback) (GtkClock *clock,
					    gpointer user_data);


struct _GtkClockSymbolParameters {
  gint			      segment_length;
  gint			      segment_thickness;
};


struct _GtkClock {
  GtkWidget		      widget;

  GtkClockSymbolParameters    normal_symbol_parameters;
  GtkClockSymbolParameters    small_symbol_parameters;

  gint			      max_hours_positions;
  gint			      max_moves_positions;

  gint			      seconds;
  gint			      moves;
  gboolean		      is_highlighted;

  const TimeControl	     *time_control;
  GSource		     *time_control_watch_source;

  GtkClockOutOfTimeCallback   out_of_time_callback;
  gpointer		      user_data;
};

struct _GtkClockClass {
  GtkWidgetClass	      parent_class;
};


GType		gtk_clock_get_type (void);

GtkWidget *	gtk_clock_new (void);


void		gtk_clock_set_time (GtkClock *clock,
				    gdouble seconds, gint moves);

void		gtk_clock_use_time_control
		  (GtkClock *clock, const TimeControl *time_control,
		   GtkClockOutOfTimeCallback  out_of_time_callback,
		   gpointer user_data);

void		gtk_clock_time_control_state_changed (GtkClock *clock);


#endif /* QUARRY_GTK_CLOCK_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
