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


#ifndef QUARRY_GTK_CLOCK_H
#define QUARRY_GTK_CLOCK_H


#include "time-control.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_CLOCK		(gtk_clock_get_type())
#define GTK_CLOCK(obj)		(GTK_CHECK_CAST((obj), GTK_TYPE_CLOCK,	\
						GtkClock))
#define GTK_CLOCK_CLASS(klass)						\
  (GTK_CHECK_CLASS_CAST((klass), GTK_TYPE_CLOCK, GtkClockClass))

#define GTK_IS_CLOCK(obj)	(GTK_CHECK_TYPE((obj), GTK_TYPE_CLOCK))
#define GTK_IS_CLOCK_CLASS(klass)					\
  (GTK_CHECK_CLASS_TYPE((klass), GTK_TYPE_CLOCK))

#define GTK_CLOCK_GET_CLASS(obj)					\
  (GTK_CHECK_GET_CLASS((obj), GTK_TYPE_CLOCK, GtkClockClass))


typedef struct _GtkClockSymbolParameters	GtkClockSymbolParameters;

typedef struct _GtkClock			GtkClock;
typedef struct _GtkClockClass			GtkClockClass;

struct _GtkClockSymbolParameters {
  gint			     segment_length;
  gint			     segment_thickness;
};

struct _GtkClock {
  GtkWidget		     widget;

  GtkClockSymbolParameters   normal_symbol_parameters;
  GtkClockSymbolParameters   small_symbol_parameters;

  gint			     max_hours_positions;
  gint			     max_moves_positions;

  gint			     seconds;
  gint			     moves;

  TimeControl		    *time_control;
  gint			     source_id;
};

struct _GtkClockClass {
  GtkWidgetClass   parent_class;
};


GtkType		gtk_clock_get_type(void);

GtkWidget *	gtk_clock_new(void);


double		gtk_clock_start(GtkClock *clock, TimeControl *time_control);
int		gtk_clock_stop(GtkClock *clock);

void		gtk_clock_initialize_for_time_control
		  (GtkClock *clock, const TimeControl *time_control);


#endif /* QUARRY_GTK_CLOCK_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
