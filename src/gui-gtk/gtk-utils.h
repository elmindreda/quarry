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


#ifndef QUARRY_GTK_UTILS_H
#define QUARRY_GTK_UTILS_H


#include "quarry.h"

#include <gtk/gtk.h>


#define QUARRY_SPACING_VERY_SMALL	 2
#define QUARRY_SPACING_GOBAN_WINDOW	 4
#define QUARRY_SPACING_SMALL		 6
#define QUARRY_SPACING			12
#define QUARRY_SPACING_BIG		18
#define QUARRY_SPACING_VERY_BIG		24


#define GTK_2_2_OR_LATER				\
  (GTK_MAJOR_VERSION >= 2 && GTK_MINOR_VERSION >= 2)

#define GTK_2_4_OR_LATER				\
  (GTK_MAJOR_VERSION >= 2 && GTK_MINOR_VERSION >= 4)


#if !GTK_2_2_OR_LATER


/* Workaround a function name change between GTK+ 2.0 and GTK+ 2.2. */
#define gdk_draw_pixbuf(drawable, gc, pixbuf,				\
			src_x, src_y, dest_x, dest_y, width, height,	\
			dither, x_dither, y_dither)			\
  gdk_pixbuf_render_to_drawable((pixbuf), (drawable), (gc),		\
				(src_x), (src_y), (dest_x), (dest_y),	\
				(width), (height),			\
				(dither), (x_dither), (y_dither))

/* Certain functions are not present in GTK+ 2.0 */
#define gtk_window_set_skip_taskbar_hint(window, setting)
#define gtk_window_set_skip_pager_hint(window, setting)


#endif


typedef struct _GtkUtilsBindingInfo	GtkUtilsBindingInfo;

struct _GtkUtilsBindingInfo {
  guint		   keyval;
  GdkModifierType  modifiers;
  gint		   signal_parameter;
};


typedef enum {
  GTK_UTILS_NO_BUTTONS = 0,
  GTK_UTILS_BUTTONS_OK,
  GTK_UTILS_BUTTONS_OK_CANCEL,
  GTK_UTILS_BUTTONS_CLOSE,
  GTK_UTILS_BUTTONS_MASK = 0xFF,

  GTK_UTILS_DONT_SHOW		= 1 << 8,
  GTK_UTILS_NON_MODAL_WINDOW	= 1 << 9,
  GTK_UTILS_DESTROY_ON_RESPONSE = 1 << 10
} GtkUtilsMessageDialogFlags;


#define GTK_UTILS_EXPAND		(1 << 16)
#define GTK_UTILS_FILL			(1 << 17)
#define GTK_UTILS_PACK_DEFAULT		(GTK_UTILS_EXPAND | GTK_UTILS_FILL)

#define GTK_UTILS_PACK_PADDING_MASK	0xFFFF


void		gtk_utils_add_similar_bindings(GtkBindingSet *binding_set,
					       const gchar *signal_name,
					       GtkUtilsBindingInfo *bindings,
					       gint num_bindings);


void		gtk_utils_make_window_only_horizontally_resizable
		  (GtkWindow *window);

void		gtk_utils_standardize_dialog(GtkDialog *dialog,
					      GtkWidget *contents);
GtkWidget *	gtk_utils_create_message_dialog
		  (GtkWindow *parent, const gchar *icon_stock_id,
		   GtkUtilsMessageDialogFlags flags,
		   const gchar *hint, const gchar *message_format_string, ...);

void		gtk_utils_add_file_selection_response_handlers
		  (GtkWidget *file_selection, gboolean saving_file,
		   GCallback response_callback, gpointer user_data);

GtkWidget *	gtk_utils_create_titled_page(GtkWidget *contents,
					     const gchar *icon_stock_id,
					     const gchar *title);

GtkWidget *	gtk_utils_pack_in_box(GType box_type, gint spacing, ...);
GtkWidget *	gtk_utils_align_widget(GtkWidget *widget,
				       gfloat x_alignment, gfloat y_alignment);
GtkWidget *	gtk_utils_sink_widget(GtkWidget *widget);
GtkWidget *	gtk_utils_make_widget_scrollable
		  (GtkWidget *widget,
		   GtkPolicyType hscrollbar_policy,
		   GtkPolicyType vscrollbar_policy);

GtkWidget *	gtk_utils_create_entry(const gchar *text);
GtkWidget *	gtk_utils_create_spin_button(GtkAdjustment *adjustment,
					     gdouble climb_rate,
					     guint num_digits,
					     gboolean snap_to_ticks);

void		gtk_utils_create_radio_chain(GtkWidget **radio_buttons,
					     const gchar **label_texts,
					     gint num_radio_buttons);
GtkSizeGroup *	gtk_utils_create_size_group(GtkSizeGroupMode mode, ...);


void		gtk_utils_set_sensitive_on_toggle
		  (GtkToggleButton *toggle_button, GtkWidget *widget);
void		gtk_utils_set_sensitive_on_input(GtkEntry *entry,
						 GtkWidget *widget);

void		gtk_utils_set_menu_items_sensitive(GtkItemFactory *item_factory,
						   gboolean are_sensitive, ...);


#endif /* QUARRY_GTK_UTILS_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
