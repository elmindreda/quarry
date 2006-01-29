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


#ifndef QUARRY_GTK_FREEZABLE_SPIN_BUTTON_H
#define QUARRY_GTK_FREEZABLE_SPIN_BUTTON_H


#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_FREEZABLE_SPIN_BUTTON					\
  (gtk_freezable_spin_button_get_type ())

#define GTK_FREEZABLE_SPIN_BUTTON(object)				\
  GTK_CHECK_CAST ((object), GTK_TYPE_FREEZABLE_SPIN_BUTTON,		\
		  GtkFreezableSpinButton)

#define GTK_TYPE_FREEZABLE_SPIN_BUTTON_CLASS(class)			\
  GTK_CHECK_CLASS_CAST ((class), GTK_TYPE_FREEZABLE_SPIN_BUTTON,	\
			GtkFreezableSpinButtonClass)

#define GTK_IS_FREEZABLE_SPIN_BUTTON(object)				\
  GTK_CHECK_TYPE ((object), GTK_TYPE_FREEZABLE_SPIN_BUTTON)

#define GTK_IS_FREEZABLE_SPIN_BUTTON_CLASS(class)			\
  GTK_CHECK_CLASS_TYPE ((class), GTK_TYPE_FREEZABLE_SPIN_BUTTON)

#define GTK_FREEZABLE_SPIN_BUTTON_GET_CLASS(object)			\
  GTK_CHECK_GET_CLASS ((object), GTK_TYPE_FREEZABLE_SPIN_BUTTON,	\
		       GtkFreezableSpinButtonClass)


typedef struct _GtkFreezableSpinButton		GtkFreezableSpinButton;
typedef struct _GtkFreezableSpinButtonClass	GtkFreezableSpinButtonClass;

struct _GtkFreezableSpinButton {
  GtkSpinButton	       spin_button;

  const gchar	      *freezing_string;
  gboolean	       is_in_output;
};

struct _GtkFreezableSpinButtonClass {
  GtkSpinButtonClass   parent_class;
};


GtkType		gtk_freezable_spin_button_get_type (void);

GtkWidget *	gtk_freezable_spin_button_new (GtkAdjustment *adjustment,
					       gdouble climb_rate,
					       guint digits);


const gchar *	gtk_freezable_spin_button_get_freezing_string
		  (GtkFreezableSpinButton *spin_button);

void		gtk_freezable_spin_button_freeze
		  (GtkFreezableSpinButton *spin_button,
		   const gchar *freezing_string);
void		gtk_freezable_spin_button_freeze_and_stop_input
		  (GtkFreezableSpinButton *spin_button,
		   const gchar *freezing_string);

#define gtk_freezable_spin_button_unfreeze(spin_button)		\
  gtk_freezable_spin_button_freeze ((spin_button), NULL)


#endif /* QUARRY_GTK_FREEZABLE_SPIN_BUTTON_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
