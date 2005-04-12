/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005 Paul Pogonyshev.                 *
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


#include "gtk-freezable-spin-button.h"
#include "utils.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define QUARRY_SPACING_VERY_SMALL	 2
#define QUARRY_SPACING_GOBAN_WINDOW	 4
#define QUARRY_SPACING_SMALL		 6
#define QUARRY_SPACING			12
#define QUARRY_SPACING_BIG		18
#define QUARRY_SPACING_VERY_BIG		24


/* These limits appeared only in GLib 2.4. */
#ifndef G_MAXUINT8
#define G_MAXUINT8	((guint8) 0xff)
#endif

#ifndef G_MAXUINT16
#define G_MAXUINT16	((guint16) 0xffff)
#endif


#define GTK_2_2_OR_LATER				\
  (GTK_MAJOR_VERSION >= 2 && GTK_MINOR_VERSION >= 2)

#define GTK_2_4_OR_LATER				\
  (GTK_MAJOR_VERSION >= 2 && GTK_MINOR_VERSION >= 4)

#define GTK_2_6_OR_LATER				\
  (GTK_MAJOR_VERSION >= 2 && GTK_MINOR_VERSION >= 6)


#if !GTK_2_2_OR_LATER


/* Workaround a function name change between GTK+ 2.0 and GTK+ 2.2. */
#define gdk_draw_pixbuf(drawable, gc, pixbuf,				\
			src_x, src_y, dest_x, dest_y, width, height,	\
			dither, x_dither, y_dither)			\
  gdk_pixbuf_render_to_drawable ((pixbuf), (drawable), (gc),		\
				 (src_x), (src_y), (dest_x), (dest_y),	\
				 (width), (height),			\
				 (dither), (x_dither), (y_dither))

/* Certain functions are not present in GTK+ 2.0 */
#define gtk_window_set_skip_taskbar_hint(window, setting)
#define gtk_window_set_skip_pager_hint(window, setting)


/* Workaround GTK+ 2.0 focus slipping problem. */
#define gtk_dialog_set_default_response(dialog, response_id)		\
  gtk_utils_workaround_set_default_response (dialog, response_id)


#endif


#if !GTK_2_4_OR_LATER
/* The manual doesn't say this, but apparently the function is only
 * introduced in GTK+ 2.4.
 */
#define gtk_entry_set_alignment(entry, alignment)
#endif


/* Much like g_signal_handlers_block_by_func() and
 * g_signal_handlers_unblock_by_func(), but don't require to match
 * data.
 */
#define gtk_utils_block_signal_handlers(instance, function)		\
  g_signal_handlers_block_matched ((instance), G_SIGNAL_MATCH_FUNC,	\
				   0, 0, NULL, (function), NULL)

#define gtk_utils_unblock_signal_handlers(instance, function)		\
  g_signal_handlers_unblock_matched ((instance), G_SIGNAL_MATCH_FUNC,	\
				     0, 0, NULL, (function), NULL)



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


typedef enum {
  RETURN_ACTIVATES_DEFAULT,
  RETURN_ADVANCES_FOCUS,
  RETURN_DEFAULT_MODE
} GtkUtilsEntryActivationMode;


typedef enum {
  GTK_UTILS_HOMOGENEOUS	 = 1 << 0,
  GTK_UTILS_IS_IMPORTANT = 1 << 1,
} GtkUtilsToolbarButtonFlags;


typedef void (* GtkUtilsToolbarEntryCallback) (gpointer user_data,
					       guint callback_action);

typedef struct _GtkUtilsToolbarEntry	GtkUtilsToolbarEntry;

struct _GtkUtilsToolbarEntry {
  const gchar			*label_text;
  const gchar			*tooltip_text;
  const gchar			*icon_stock_id;

  GtkUtilsToolbarEntryCallback	 callback;
  guint				 callback_action;
};


/* This is the same as the prototype of the `focus-out-event' handler
 * since this callback should basically do the same job.  Note that
 * `dummy_event' will be `NULL'.
 */
typedef gboolean (* GtkUtilsBrowsingDoneCallback) (GtkEntry *entry,
						   GdkEventFocus *dummy_event,
						   gpointer user_data);


#define GTK_UTILS_EXPAND		(1 << 16)
#define GTK_UTILS_FILL			(1 << 17)
#define GTK_UTILS_PACK_DEFAULT		(GTK_UTILS_EXPAND | GTK_UTILS_FILL)

#define GTK_UTILS_PACK_END		(1 << 18)

#define GTK_UTILS_PACK_PADDING_MASK	0xFFFF


void		gtk_utils_add_similar_bindings (GtkBindingSet *binding_set,
						const gchar *signal_name,
						GtkUtilsBindingInfo *bindings,
						gint num_bindings);


#if GTK_2_2_OR_LATER

void		gtk_utils_make_window_only_horizontally_resizable
		  (GtkWindow *window);

#else

#define gtk_utils_make_window_only_horizontally_resizable(window)

void		gtk_utils_workaround_set_default_response (GtkDialog *dialog,
							   gint response_id);

#endif


void		gtk_utils_standardize_dialog (GtkDialog *dialog,
					      GtkWidget *contents);
GtkWidget *	gtk_utils_create_message_dialog
		  (GtkWindow *parent, const gchar *icon_stock_id,
		   GtkUtilsMessageDialogFlags flags,
		   const gchar *hint, const gchar *message_format_string, ...);
void		gtk_utils_null_pointer_on_destroy
		  (GtkWindow **window, gboolean ask_control_center);
void		gtk_utils_workaround_focus_bug (GtkWindow *window);

GtkWidget *	gtk_utils_create_titled_page (GtkWidget *contents,
					      const gchar *icon_stock_id,
					      const gchar *title);

GtkWidget *	gtk_utils_pack_in_box (GType box_type, gint spacing, ...);
GtkWidget *	gtk_utils_align_widget (GtkWidget *widget,
					gfloat x_alignment,
					gfloat y_alignment);
GtkWidget *	gtk_utils_sink_widget (GtkWidget *widget);
GtkWidget *	gtk_utils_make_widget_scrollable
		  (GtkWidget *widget,
		   GtkPolicyType hscrollbar_policy,
		   GtkPolicyType vscrollbar_policy);

GtkWidget *	gtk_utils_create_left_aligned_label (const gchar *label_text);
GtkWidget *	gtk_utils_create_mnemonic_label (const gchar *label_text,
						 GtkWidget *mnemonic_widget);
GtkWidget *	gtk_utils_create_entry (const gchar *text,
					GtkUtilsEntryActivationMode mode);
GtkWidget *	gtk_utils_create_browse_button
		  (gboolean with_text,
		   GtkWidget *associated_entry, gboolean is_command_line_entry,
		   const gchar *browsing_dialog_caption,
		   GtkUtilsBrowsingDoneCallback callback, gpointer user_data);
GtkWidget *	gtk_utils_create_spin_button (GtkAdjustment *adjustment,
					      gdouble climb_rate,
					      guint num_digits,
					      gboolean snap_to_ticks);
GtkWidget *	gtk_utils_create_freezable_spin_button
		  (GtkAdjustment *adjustment, gdouble climb_rate,
		   guint num_digits, gboolean snap_to_ticks);
void		gtk_utils_convert_to_time_spin_button
		  (GtkSpinButton *spin_button);
GtkWidget *	gtk_utils_create_time_spin_button (GtkAdjustment *adjustment,
						   gdouble climb_rate);
GtkWidget *	gtk_utils_create_selector (const gchar **items, gint num_items,
					   gint selected_item);
GtkWidget *	gtk_utils_create_selector_from_string_list
		  (void *abstract_list, const gchar *selected_item);
GtkWidget *	gtk_utils_create_invisible_notebook (void);

void		gtk_utils_create_radio_chain (GtkWidget **radio_buttons,
					      const gchar **label_texts,
					      gint num_radio_buttons);
GtkSizeGroup *	gtk_utils_create_size_group (GtkSizeGroupMode mode, ...);
GtkSizeGroup *  gtk_utils_align_left_widgets (GtkContainer *container,
					      GtkSizeGroup *size_group);

GtkWidget *	gtk_utils_append_toolbar_button
		  (GtkToolbar *toolbar, GtkUtilsToolbarEntry *entry,
		   GtkUtilsToolbarButtonFlags flags, gpointer user_data);
void		gtk_utils_set_toolbar_buttons_sensitive
		  (GtkToolbar *toolbar, gboolean are_sensitive, ...);


#if GTK_2_4_OR_LATER


#define gtk_utils_append_toolbar_space(toolbar)				\
  gtk_toolbar_insert ((toolbar), gtk_separator_tool_item_new (), -1)


#else /* not GTK_2_4_OR_LATER */


#define gtk_utils_append_toolbar_space(toolbar)				\
  gtk_toolbar_append_space (toolbar)

/* Not defined in older GTK+ versions. */
#define gtk_toolbar_set_show_arrow(toolbar, show_arrow)


#endif /* not GTK_2_4_OR_LATER */


void		gtk_utils_set_text_buffer_text (GtkTextBuffer *text_buffer,
						const gchar *text);


void		gtk_utils_set_sensitive_on_toggle
		  (GtkToggleButton *toggle_button, GtkWidget *widget,
		   gboolean reverse_meaning);
void		gtk_utils_set_sensitive_on_input (GtkEntry *entry,
						  GtkWidget *widget);

void		gtk_utils_freeze_on_empty_input
		  (GtkFreezableSpinButton *freezable_spin_button);


void		gtk_utils_set_widgets_visible (gboolean visible, ...);
void		gtk_utils_set_menu_items_sensitive
		  (GtkItemFactory *item_factory, gboolean are_sensitive, ...);


void		gtk_utils_set_gdk_color (GdkColor *gdk_color,
					 QuarryColor quarry_color);
void		gtk_utils_set_quarry_color (QuarryColor *quarry_color,
					    const GdkColor *gdk_color);


#if GTK_2_4_OR_LATER

#define gtk_utils_get_selector_active_item_index(selector)		\
  gtk_combo_box_get_active (GTK_COMBO_BOX (selector))

#define gtk_utils_set_selector_active_item_index(selector, index)	\
  gtk_combo_box_set_active (GTK_COMBO_BOX (selector), (index))

#else

#define gtk_utils_get_selector_active_item_index(selector)		\
  gtk_option_menu_get_history (GTK_OPTION_MENU (selector))

#define gtk_utils_set_selector_active_item_index(selector, index)	\
  gtk_option_menu_set_history (GTK_OPTION_MENU (selector), (index))

#endif


#endif /* QUARRY_GTK_UTILS_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
