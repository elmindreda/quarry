/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005, 2006 Paul Pogonyshev.           *
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


#include "gtk-control-center.h"

#include "gtk-goban-window.h"
#include "gtk-help.h"
#include "gtk-new-game-dialog.h"
#include "gtk-new-game-record-dialog.h"
#include "gtk-parser-interface.h"
#include "gtk-preferences.h"
#include "gtk-resume-game-dialog.h"
#include "gtk-utils.h"
#include "quarry-stock.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>


static GSList	  *windows = NULL;
static int	   num_other_reasons_to_live = 0;

static GtkWindow  *control_center = NULL;


void
gtk_control_center_present (void)
{
  if (!control_center) {
    GtkWidget *vbox;
    GtkWidget *new_game_button;
    GtkWidget *new_game_record_button;
    GtkWidget *open_game_record_button;
    GtkWidget *resume_game_button;
    GtkWidget *preferences_button;
    GtkWidget *quit_button;
    GtkWidget *help_button;
    GtkWidget *close_button;
    GtkAccelGroup *accel_group;

    control_center = (GtkWindow *) gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_control_center_window_created (control_center);
    gtk_utils_null_pointer_on_destroy (&control_center, TRUE);

    gtk_window_set_title (control_center, _("Quarry Control Center"));
    gtk_window_set_resizable (control_center, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (control_center),
				    QUARRY_SPACING);

    accel_group = gtk_accel_group_new ();
    gtk_window_add_accel_group (control_center, accel_group);

    new_game_button = gtk_button_new_from_stock (QUARRY_STOCK_NEW_GAME);
    g_signal_connect (new_game_button, "clicked",
		      G_CALLBACK (gtk_new_game_dialog_present), NULL);

    gtk_widget_add_accelerator (new_game_button, "clicked",
				accel_group, GDK_N, GDK_CONTROL_MASK, 0);

    new_game_record_button
      = gtk_button_new_with_mnemonic (_("Ne_w Game Record"));
    g_signal_connect (new_game_record_button, "clicked",
		      G_CALLBACK (gtk_new_game_record_dialog_present), NULL);

    gtk_widget_add_accelerator (new_game_record_button, "clicked",
				accel_group,
				GDK_N, GDK_SHIFT_MASK | GDK_CONTROL_MASK, 0);

    open_game_record_button
      = gtk_button_new_from_stock (QUARRY_STOCK_OPEN_GAME_RECORD);
    g_signal_connect (open_game_record_button, "clicked",
		      G_CALLBACK (gtk_parser_interface_present_default), NULL);

    gtk_widget_add_accelerator (open_game_record_button, "clicked",
				accel_group, GDK_O, GDK_CONTROL_MASK, 0);

    resume_game_button = gtk_button_new_with_mnemonic (_("_Resume Game"));
    g_signal_connect (resume_game_button, "clicked",
		      G_CALLBACK (gtk_resume_game), NULL);

    preferences_button = gtk_button_new_from_stock (GTK_STOCK_PREFERENCES);
    g_signal_connect_swapped (preferences_button, "clicked",
			      G_CALLBACK (gtk_preferences_dialog_present),
			      GINT_TO_POINTER (-1));

    quit_button = gtk_button_new_from_stock (GTK_STOCK_QUIT);
    g_signal_connect_swapped (quit_button, "clicked",
			      G_CALLBACK (gtk_control_center_quit), NULL);

    gtk_widget_add_accelerator (quit_button, "clicked",
				accel_group, GDK_Q, GDK_CONTROL_MASK, 0);

    vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				  new_game_button, GTK_UTILS_FILL,
				  new_game_record_button, GTK_UTILS_FILL,
				  gtk_hseparator_new (),
				  GTK_UTILS_FILL | QUARRY_SPACING_VERY_SMALL,
				  open_game_record_button, GTK_UTILS_FILL,
				  resume_game_button, GTK_UTILS_FILL,
				  gtk_hseparator_new (),
				  GTK_UTILS_FILL | QUARRY_SPACING_VERY_SMALL,
				  preferences_button, GTK_UTILS_FILL,
				  gtk_hseparator_new (),
				  GTK_UTILS_FILL | QUARRY_SPACING_VERY_SMALL,
				  quit_button, GTK_UTILS_FILL, NULL);
    gtk_container_add (GTK_CONTAINER (control_center), vbox);
    gtk_widget_show_all (vbox);

    /* Add invisible buttons, used only for adding accelerator. */

    help_button = gtk_button_new ();
    g_signal_connect_swapped (help_button, "clicked",
			      G_CALLBACK (gtk_help_display), NULL);

    gtk_widget_add_accelerator (help_button, "clicked",
				accel_group, GDK_F1, 0, 0);

    close_button = gtk_button_new ();
    g_signal_connect_swapped (close_button, "clicked",
			      G_CALLBACK (gtk_widget_destroy), control_center);

    gtk_widget_add_accelerator (close_button, "clicked",
				accel_group, GDK_W, GDK_CONTROL_MASK, 0);

#if GTK_2_4_OR_LATER

    g_signal_connect (help_button, "can-activate-accel",
		      G_CALLBACK (gtk_true), NULL);
    g_signal_connect (close_button, "can-activate-accel",
		      G_CALLBACK (gtk_true), NULL);

#endif

    gtk_container_add (GTK_CONTAINER (vbox), help_button);
    gtk_container_add (GTK_CONTAINER (vbox), close_button);
  }

  gtk_window_present (control_center);
}



inline void
gtk_control_center_window_created (GtkWindow *window)
{
  windows = g_slist_prepend (windows, window);
}


gint
gtk_control_center_window_destroyed (const GtkWindow *window)
{
  GSList *element = g_slist_find (windows, window);

  if (element) {
    windows = g_slist_delete_link (windows, element);

    if (windows != NULL || num_other_reasons_to_live > 0) {
      if (control_center && windows->next == NULL)
	gtk_control_center_present ();
    }
    else
      gtk_main_quit ();

    return TRUE;
  }

  return FALSE;
}


inline void
gtk_control_center_new_reason_to_live (void)
{
  num_other_reasons_to_live++;
}


inline void
gtk_control_center_lost_reason_to_live (void)
{
  if (--num_other_reasons_to_live == 0 && windows == NULL)
    gtk_main_quit ();
}


void
gtk_control_center_quit (void)
{
  while (1) {
    GSList *element;

    for (element = windows; element; element = element->next) {
      if (GTK_IS_GOBAN_WINDOW (element->data)) {
	if (gtk_goban_window_stops_closing (GTK_GOBAN_WINDOW (element->data)))
	  return;
	else {
	  gtk_widget_destroy (GTK_WIDGET (element->data));
	  break;
	}
      }
    }

    if (!element)
      break;
  }

  if (control_center)
    gtk_widget_destroy (GTK_WIDGET (control_center));

  while (windows)
    gtk_widget_destroy (GTK_WIDGET (windows->data));
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
