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


#include "gtk-utils.h"
#include "quarry-stock.h"
#include "utils.h"

#include <assert.h>
#include <string.h>


typedef void (* GtkUtilsFileSelectionCallback)
  (GtkFileSelection *file_selection, gint response_id, gpointer user_data);

typedef struct _GtkUtilsFileSelectionData	GtkUtilsFileSelectionData;

struct _GtkUtilsFileSelectionData {
  GtkFileSelection		*file_selection;

  gboolean			 saving_file;

  GtkUtilsFileSelectionCallback	 response_callback;
  gpointer			 user_data;
};


static void	file_selection_response(GtkFileSelection *file_selection,
					gint response_id,
					GtkUtilsFileSelectionData *data);
static void	overwrite_confirmation(GtkWidget *message_dialog,
				       gint response_id,
				       GtkUtilsFileSelectionData *data);


static void	set_widget_sensitivity_on_toggle
		  (GtkToggleButton *toggle_button, GtkWidget *widget);
static void	set_widget_sensitivity_on_input(GtkEntry *entry,
						GtkWidget *widget);


void
gtk_utils_add_similar_bindings(GtkBindingSet *binding_set,
			       const gchar *signal_name,
			       GtkUtilsBindingInfo *bindings,
			       int num_bindings)
{
  int k;

  assert(binding_set);
  assert(signal_name);
  assert(bindings);
  assert(num_bindings > 0);

  for (k = 0; k < num_bindings; k++) {
    gtk_binding_entry_add_signal(binding_set,
				 bindings[k].keyval, bindings[k].modifiers,
				 signal_name,
				 1, G_TYPE_INT, bindings[k].signal_parameter);
  }
}


void
gtk_utils_make_window_only_horizontally_resizable(GtkWindow *window)
{
  GdkGeometry geometry;

  assert(GTK_IS_WINDOW(window));

  geometry.max_width  = G_MAXINT;
  geometry.max_height = -1;
  gtk_window_set_geometry_hints(window, NULL, &geometry, GDK_HINT_MAX_SIZE);
}


void
gtk_utils_standardize_dialog(GtkDialog *dialog, GtkWidget *contents)
{
  assert(GTK_IS_DIALOG(dialog));
  assert(GTK_IS_CONTAINER(contents));

  gtk_widget_set_name(GTK_WIDGET(dialog), "quarry-dialog");

  gtk_box_set_spacing(GTK_BOX(dialog->vbox), QUARRY_SPACING);
  gtk_box_pack_start_defaults(GTK_BOX(dialog->vbox), contents);
}


GtkWidget *
gtk_utils_create_message_dialog(GtkWindow *parent, const gchar *icon_stock_id,
				GtkUtilsMessageDialogFlags flags,
				const gchar *hint,
				const gchar *message_format_string, ...)
{
  GtkWidget *widget = gtk_dialog_new();
  GtkDialog *dialog = GTK_DIALOG(widget);
  GtkWindow *window = GTK_WINDOW(dialog);
  GtkUtilsMessageDialogFlags buttons = flags & GTK_UTILS_BUTTONS_MASK;
  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *hbox;
  gchar *message_text;
  gchar *label_text;
  va_list arguments;

  assert(icon_stock_id);

  gtk_window_set_title(window, "");
  gtk_window_set_resizable(window, FALSE);
  gtk_window_set_skip_pager_hint(window, TRUE);

  /* Note: disabled because KDE window manager doesn't give focus to
   * message dialog after this call (is it a bug or what?).
   */
#if 0
  gtk_window_set_skip_taskbar_hint(window, TRUE);
#endif

  if (parent)
    gtk_window_set_transient_for(window, GTK_WINDOW(parent));

  if (!(flags & GTK_UTILS_NON_MODAL_WINDOW))
    gtk_window_set_modal(window, TRUE);

  gtk_dialog_set_has_separator(dialog, FALSE);

  if (buttons == GTK_UTILS_BUTTONS_OK_CANCEL)
    gtk_dialog_add_button(dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  if (buttons == GTK_UTILS_BUTTONS_OK
      || buttons == GTK_UTILS_BUTTONS_OK_CANCEL) {
    gtk_dialog_add_button(dialog, GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
  }

  if (buttons == GTK_UTILS_BUTTONS_CLOSE) {
    gtk_dialog_add_button(dialog, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_CLOSE);
  }

  icon = gtk_image_new_from_stock(icon_stock_id, GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment(GTK_MISC(icon), 0.5, 0.0);

  va_start(arguments, message_format_string);
  message_text = g_strdup_vprintf(message_format_string, arguments);
  va_end(arguments);

  if (hint) {
    label_text = g_strdup_printf(("<span weight=\"bold\" size=\"larger\">%s"
				  "</span>\n\n%s"),
				 message_text, hint);
  }
  else {
    label_text = g_strdup_printf(("<span weight=\"bold\" size=\"larger\">%s"
				  "</span>"),
				 message_text);
  }

  label = gtk_label_new(NULL);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_selectable(GTK_LABEL(label), TRUE);
  gtk_label_set_markup(GTK_LABEL(label), label_text);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);

  g_free(message_text);
  g_free(label_text);

  hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
			       icon, GTK_UTILS_FILL,
			       label, GTK_UTILS_PACK_DEFAULT, NULL);
  gtk_widget_show_all(hbox);

  gtk_utils_standardize_dialog(dialog, hbox);
  gtk_box_set_spacing(GTK_BOX(dialog->vbox), QUARRY_SPACING_VERY_BIG);

  if (!(flags & GTK_UTILS_DONT_SHOW))
    gtk_window_present(window);

  if (flags & GTK_UTILS_DESTROY_ON_RESPONSE)
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

  return widget;
}


void
gtk_utils_add_file_selection_response_handlers(GtkWidget *file_selection,
					       gboolean saving_file,
					       GCallback response_callback,
					       gpointer user_data)
{
  GtkUtilsFileSelectionData *data
    = g_malloc(sizeof(GtkUtilsFileSelectionData));

  assert(response_callback);

  data->file_selection = GTK_FILE_SELECTION(file_selection);

  data->saving_file	  = saving_file;
  data->response_callback = (GtkUtilsFileSelectionCallback) response_callback;
  data->user_data	  = user_data;

  g_signal_connect(file_selection, "response",
		   G_CALLBACK(file_selection_response), data);
  g_signal_connect_swapped(file_selection, "destroy",
			   G_CALLBACK(g_free), data);
}


static void
file_selection_response(GtkFileSelection *file_selection, gint response_id,
			GtkUtilsFileSelectionData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *filename = gtk_file_selection_get_filename(file_selection);

    if (*filename) {
      if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
	/* Don't try to read directory, just browse into it. */
	if (filename[strlen(filename) - 1] != G_DIR_SEPARATOR) {
	  gchar *directory_name = g_strconcat(filename, G_DIR_SEPARATOR_S, NULL);

	  gtk_file_selection_set_filename(file_selection, directory_name);
	  g_free(directory_name);
	}
	else
	  gtk_file_selection_set_filename(file_selection, filename);

	/* We are done, no need to try to open/save. */
	return;
      }
      else if (data->saving_file
	       && g_file_test(filename, G_FILE_TEST_EXISTS)) {
	static const gchar *hint
	  = ("Note that all information in the existing file will be lost "
	     "permanently if you choose to overwrite it.");
	static const gchar *message_format_string
	  = ("File named `%s' already exists. "
	     "Do you want to overwrite it with the one you are saving?");

	GtkWidget *confirmation_dialog
	  = gtk_utils_create_message_dialog(GTK_WINDOW(file_selection),
					    GTK_STOCK_DIALOG_WARNING,
					    (GTK_UTILS_NO_BUTTONS
					     | GTK_UTILS_DONT_SHOW),
					    hint,
					    message_format_string, filename);

	gtk_dialog_add_buttons(GTK_DIALOG(confirmation_dialog),
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			       QUARRY_STOCK_OVERWRITE, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(confirmation_dialog),
					GTK_RESPONSE_OK);

	g_signal_connect(confirmation_dialog, "response",
			 G_CALLBACK(overwrite_confirmation), data);

	gtk_window_present(GTK_WINDOW(confirmation_dialog));

	/* Don't try to overwrite the file before we get user
	 * confirmation.
	 */
	return;
      }
    }
  }

  data->response_callback(file_selection, response_id, data->user_data);
}


static void
overwrite_confirmation(GtkWidget *message_dialog, gint response_id,
		       GtkUtilsFileSelectionData *data)
{
  gtk_widget_destroy(message_dialog);

  if (response_id == GTK_RESPONSE_OK) {
    data->response_callback(data->file_selection, GTK_RESPONSE_OK,
			    data->user_data);
  }
}


GtkWidget *
gtk_utils_create_titled_page(GtkWidget *contents,
			     const gchar *icon_stock_id, const gchar *title)
{
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *title_widget;
  GtkWidget *hseparator;

  assert(GTK_IS_WIDGET(contents));
  assert(icon_stock_id || title);

  if (icon_stock_id) {
    image = gtk_image_new_from_stock(icon_stock_id,
				     GTK_ICON_SIZE_LARGE_TOOLBAR);
  }

  if (title) {
    char *marked_up_title
      = utils_cat_strings(NULL,
			  "<span weight=\"bold\" size=\"x-large\">",
			  title, "</span>", NULL);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), marked_up_title);

    utils_free(marked_up_title);
  }

  if (icon_stock_id && title) {
    title_widget = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING_SMALL,
					 image, GTK_UTILS_FILL, label, 0,
					 NULL);
  }
  else if (title) {
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    title_widget = label;
  }
  else
    title_widget = image;

  gtk_widget_show_all(title_widget);

  hseparator = gtk_hseparator_new();
  gtk_widget_show(hseparator);

  return gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
			       title_widget, GTK_UTILS_FILL,
			       hseparator, GTK_UTILS_FILL,
			       contents, GTK_UTILS_PACK_DEFAULT, NULL);
}


GtkWidget *
gtk_utils_pack_in_box(GType box_type, gint spacing, ...)
{
  GtkBox *box = GTK_BOX(g_object_new(box_type, NULL));
  GtkWidget *widget;
  va_list arguments;

  gtk_box_set_spacing(box, spacing);

  va_start(arguments, spacing);
  while ((widget = va_arg(arguments, GtkWidget *)) != NULL) {
    guint packing_parameters = va_arg(arguments, guint);

    assert(GTK_IS_WIDGET(widget));

    gtk_box_pack_start(box, widget,
		       packing_parameters & GTK_UTILS_EXPAND ? TRUE : FALSE,
		       packing_parameters & GTK_UTILS_FILL ? TRUE : FALSE,
		       packing_parameters & GTK_UTILS_PACK_PADDING_MASK);
  }

  return GTK_WIDGET(box);
}


GtkWidget *
gtk_utils_align_widget(GtkWidget *widget,
		       gfloat x_alignment, gfloat y_alignment)
{
  GtkWidget *alignment = gtk_alignment_new(x_alignment, y_alignment,
					   0.0, 0.0);

  assert(GTK_IS_WIDGET(widget));

  gtk_container_add(GTK_CONTAINER(alignment), widget);

  return alignment;
}


GtkWidget *
gtk_utils_sink_widget(GtkWidget *widget)
{
  GtkWidget *frame = gtk_frame_new(NULL);

  assert(GTK_IS_WIDGET(widget));

  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_container_add(GTK_CONTAINER(frame), widget);

  return frame;
}


GtkWidget *
gtk_utils_make_widget_scrollable(GtkWidget *widget,
				 GtkPolicyType hscrollbar_policy,
				 GtkPolicyType vscrollbar_policy)
{
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);

  assert(GTK_IS_WIDGET(widget));

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				 hscrollbar_policy, vscrollbar_policy);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
				      GTK_SHADOW_IN);
  gtk_container_add(GTK_CONTAINER(scrolled_window), widget);

  gtk_widget_show_all(scrolled_window);
  return scrolled_window;
}


GtkWidget *
gtk_utils_create_entry(const gchar *text)
{
  GtkWidget *entry = gtk_entry_new();

  if (text)
    gtk_entry_set_text(GTK_ENTRY(entry), text);

  gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

  return entry;
}


GtkWidget *
gtk_utils_create_spin_button(GtkAdjustment *adjustment, gdouble climb_rate,
			     guint num_digits, gboolean snap_to_ticks)
{
  GtkWidget *spin_button = gtk_spin_button_new(adjustment, climb_rate,
					       num_digits);

  gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin_button),
				    snap_to_ticks);
  gtk_entry_set_alignment(GTK_ENTRY(spin_button), 1.0);
  gtk_entry_set_activates_default(GTK_ENTRY(spin_button), TRUE);

  return spin_button;
}


void
gtk_utils_create_radio_chain(GtkWidget **radio_buttons,
			     const gchar **label_texts,
			     gint num_radio_buttons)
{
  GtkRadioButton *last_button = NULL;
  int k;

  assert(radio_buttons);
  assert(label_texts);

  for (k = 0; k < num_radio_buttons; k++) {
    radio_buttons[k]
      = gtk_radio_button_new_with_mnemonic_from_widget(last_button,
						       label_texts[k]);
    last_button = GTK_RADIO_BUTTON(radio_buttons[k]);
  }
}


GtkSizeGroup *
gtk_utils_create_size_group(GtkSizeGroupMode mode, ...)
{
  GtkSizeGroup *size_group = gtk_size_group_new(mode);
  GtkWidget *widget;
  va_list arguments;

  va_start(arguments, mode);
  while ((widget = va_arg(arguments, GtkWidget *)) != NULL) {
    assert(GTK_IS_WIDGET(widget));
    gtk_size_group_add_widget(size_group, widget);
  }

  va_end(arguments);

  g_object_unref(size_group);
  return size_group;
}


void
gtk_utils_set_sensitive_on_toggle(GtkToggleButton *toggle_button,
				  GtkWidget *widget)
{
  assert(GTK_IS_TOGGLE_BUTTON(toggle_button));
  assert(GTK_IS_WIDGET(widget));

  set_widget_sensitivity_on_toggle(toggle_button, widget);
  g_signal_connect(toggle_button, "toggled",
		   G_CALLBACK(set_widget_sensitivity_on_toggle), widget);
}


static void
set_widget_sensitivity_on_toggle(GtkToggleButton *toggle_button,
				 GtkWidget *widget)
{
  gtk_widget_set_sensitive(widget,
			   gtk_toggle_button_get_active(toggle_button));
}


void
gtk_utils_set_sensitive_on_input(GtkEntry *entry, GtkWidget *widget)
{
  assert(GTK_IS_ENTRY(entry));
  assert(GTK_IS_WIDGET(widget));

  set_widget_sensitivity_on_input(entry, widget);
  g_signal_connect(entry, "changed",
		   G_CALLBACK(set_widget_sensitivity_on_input), widget);
}


static void
set_widget_sensitivity_on_input(GtkEntry *entry, GtkWidget *widget)
{
  const gchar *text = gtk_entry_get_text(entry);

  gtk_widget_set_sensitive(widget, *text ? TRUE : FALSE);
}


void
gtk_utils_set_menu_items_sensitive(GtkItemFactory *item_factory,
				   gboolean are_sensitive, ...)
{
  va_list arguments;
  const gchar *path;

  assert(GTK_IS_ITEM_FACTORY(item_factory));

  va_start(arguments, are_sensitive);
  while ((path = va_arg(arguments, const gchar *)) != NULL) {
    gtk_widget_set_sensitive(gtk_item_factory_get_widget(item_factory, path),
			     are_sensitive);
  }

  va_end(arguments);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
