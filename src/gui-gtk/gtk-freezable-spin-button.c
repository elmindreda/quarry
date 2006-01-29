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
static gint	 gtk_freezable_spin_button_input
		   (GtkFreezableSpinButton *spin_button, gdouble *new_value);

static void	 gtk_freezable_spin_button_change_value
		   (GtkSpinButton *spin_button, GtkScrollType scroll_type);
static void	 gtk_freezable_spin_button_changed (GtkEditable *editable);


static GtkSpinButtonClass  *parent_class;
static GtkEditableClass    *parent_editable_interface;

static guint		    input_signal_id;


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

  input_signal_id = g_signal_lookup ("input", GTK_TYPE_SPIN_BUTTON);
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
  spin_button->is_in_output = FALSE;
}


GtkWidget *
gtk_freezable_spin_button_new (GtkAdjustment *adjustment,
			       gdouble climb_rate, guint digits)
{
  GtkWidget *spin_button = g_object_new (GTK_TYPE_FREEZABLE_SPIN_BUTTON, NULL);

  assert (!adjustment || GTK_IS_ADJUSTMENT (adjustment));

  gtk_spin_button_configure (GTK_SPIN_BUTTON (spin_button),
			     adjustment, climb_rate, digits);

  /* The reason to connect on each object instead of redefining in
   * class virtual table is that these signals are created as
   * `G_RUN_LAST', but we need our handlers to run first.
   */
  g_signal_connect (spin_button, "output",
		    G_CALLBACK (gtk_freezable_spin_button_output), NULL);
  g_signal_connect (spin_button, "input",
		    G_CALLBACK (gtk_freezable_spin_button_input), NULL);

  return spin_button;
}


static gboolean
gtk_freezable_spin_button_output (GtkFreezableSpinButton *spin_button)
{
  if (!spin_button->freezing_string) {
    /* Fall through to other handlers. */
    return FALSE;
  }

  spin_button->is_in_output = TRUE;
  gtk_entry_set_text (GTK_ENTRY (spin_button), spin_button->freezing_string);
  spin_button->is_in_output = FALSE;

  return TRUE;
}


static gint
gtk_freezable_spin_button_input (GtkFreezableSpinButton *spin_button,
				 gdouble *new_value)
{
  if (spin_button->freezing_string) {
    /* No input, this is not a real spin button right now. */
    g_signal_stop_emission (spin_button, input_signal_id, 0);
    *new_value = 0.0;

    return TRUE;
  }

  return FALSE;
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

  if (spin_button->freezing_string && !spin_button->is_in_output) {
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


void
gtk_freezable_spin_button_freeze_and_stop_input
  (GtkFreezableSpinButton *spin_button, const gchar *freezing_string)
{
  assert (freezing_string);

  gtk_freezable_spin_button_freeze (spin_button, freezing_string);
  g_signal_stop_emission (spin_button, input_signal_id, 0);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
