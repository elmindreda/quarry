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


#include "gtk-freezable-spin-button.h"

#include <assert.h>
#include <gtk/gtk.h>
#include <string.h>


static void	 gtk_freezable_spin_button_class_init
		   (GtkFreezableSpinButtonClass *class);
static void	 gtk_freezable_spin_button_editable_interface_init
		   (GtkEditableClass *interface);
static void	 gtk_freezable_spin_button_init
		   (GtkFreezableSpinButton *spin_button);

static gboolean	 gtk_freezable_spin_button_output
		   (GtkFreezableSpinButton *spin_button);

static void	 gtk_freezable_spin_button_change_value
		   (GtkSpinButton *spin_button, GtkScrollType scroll_type);
static void	 gtk_freezable_spin_button_changed (GtkEditable *editable);


static GtkSpinButtonClass  *parent_class;
static GtkEditableClass    *parent_editable_interface;


GtkType
gtk_freezable_spin_button_get_type (void)
{
  static GtkType freezable_spin_button_type = 0;

  if (!freezable_spin_button_type) {
    static const GTypeInfo freezable_spin_button_info = {
      sizeof (GtkFreezableSpinButtonClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_freezable_spin_button_class_init,
      NULL,
      NULL,
      sizeof (GtkFreezableSpinButton),
      1,
      (GInstanceInitFunc) gtk_freezable_spin_button_init,
      NULL
    };

    static const GInterfaceInfo editable_interface_info = {
      (GInterfaceInitFunc) gtk_freezable_spin_button_editable_interface_init,
      NULL,
      NULL
    };

    freezable_spin_button_type
      = g_type_register_static (GTK_TYPE_SPIN_BUTTON, "GtkFreezableSpinButton",
				&freezable_spin_button_info, 0);
    g_type_add_interface_static (freezable_spin_button_type,
				 GTK_TYPE_EDITABLE, &editable_interface_info);
  }

  return freezable_spin_button_type;
}


static void
gtk_freezable_spin_button_class_init (GtkFreezableSpinButtonClass *class)
{
  GTK_SPIN_BUTTON_CLASS (class)->change_value
    = gtk_freezable_spin_button_change_value;

  parent_class = g_type_class_peek_parent (class);
  parent_editable_interface = g_type_interface_peek (parent_class,
						     GTK_TYPE_EDITABLE);
}


static void
gtk_freezable_spin_button_editable_interface_init (GtkEditableClass *interface)
{
  interface->changed = gtk_freezable_spin_button_changed;
}


static void
gtk_freezable_spin_button_init (GtkFreezableSpinButton *spin_button)
{
  /* Unfrozen by default. */
  spin_button->freezing_string = NULL;
}


GtkWidget *
gtk_freezable_spin_button_new (GtkAdjustment *adjustment,
			       gdouble climb_rate, guint digits)
{
  GtkWidget *spin_button = g_object_new (GTK_TYPE_FREEZABLE_SPIN_BUTTON, NULL);

  assert (!adjustment || GTK_IS_ADJUSTMENT (adjustment));

  gtk_spin_button_configure (GTK_SPIN_BUTTON (spin_button),
			     adjustment, climb_rate, digits);

  /* The reason to connect it on each object instead of redefining in
   * class virtual table is that this object is created as
   * `G_RUN_LAST', but we need our handler to run first.
   */
  g_signal_connect (spin_button, "output",
		    G_CALLBACK (gtk_freezable_spin_button_output), NULL);

  return spin_button;
}


static gboolean
gtk_freezable_spin_button_output (GtkFreezableSpinButton *spin_button)
{
  if (!spin_button->freezing_string) {
    /* Fall through to other handlers. */
    return FALSE;
  }

  gtk_entry_set_text (GTK_ENTRY (spin_button), spin_button->freezing_string);
  return TRUE;
}


static void
gtk_freezable_spin_button_change_value (GtkSpinButton *spin_button,
					GtkScrollType scroll_type)
{
  gtk_freezable_spin_button_unfreeze (GTK_FREEZABLE_SPIN_BUTTON (spin_button));

  parent_class->change_value (spin_button, scroll_type);
}


static void
gtk_freezable_spin_button_changed (GtkEditable *editable)

{
  GtkFreezableSpinButton *spin_button = GTK_FREEZABLE_SPIN_BUTTON (editable);

  if (spin_button->freezing_string) {
    const gchar *entry_text = gtk_entry_get_text (GTK_ENTRY (editable));

    if (strcmp (entry_text, spin_button->freezing_string) != 0)
      gtk_freezable_spin_button_unfreeze (spin_button);
  }

  if (parent_editable_interface->changed)
    parent_editable_interface->changed (editable);
}



const gchar *
gtk_freezable_spin_button_get_freezing_string
  (GtkFreezableSpinButton *spin_button)
{
  assert (GTK_IS_FREEZABLE_SPIN_BUTTON (spin_button));

  return spin_button->freezing_string;
}


void
gtk_freezable_spin_button_freeze (GtkFreezableSpinButton *spin_button,
				  const gchar *freezing_string)
{
  assert (GTK_IS_FREEZABLE_SPIN_BUTTON (spin_button));

  spin_button->freezing_string = freezing_string;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
