/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005 Paul Pogonyshev                  *
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

#include "gtk-control-center.h"
#include "gtk-freezable-spin-button.h"
#include "quarry-stock.h"
#include "utils.h"

#include <assert.h>
#include <stdarg.h>
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


typedef struct _GtkUtilsBrowseButtonData	GtkUtilsBrowseButtonData;

struct _GtkUtilsBrowseButtonData {
  GtkWindow			*browsing_dialog;
  const gchar			*browsing_dialog_caption;

  GtkEntry			*associated_entry;
  gboolean			 is_command_line_entry;

  GtkUtilsBrowsingDoneCallback	 callback;
  gpointer			 user_data;
};


typedef struct _GtkUtilsToolbarCallbackArguments
		GtkUtilsToolbarCallbackArguments;

struct _GtkUtilsToolbarCallbackArguments {
  gboolean	are_sensitive;
  va_list	entries;
};


static void	 null_pointer_on_destroy (GtkWindow *window,
					  GtkWindow **window_pointer);
static void	 null_pointer_on_destroy_ask_control_center
		   (GtkWindow *window, GtkWindow **window_pointer);

static void	 file_selection_response (GtkFileSelection *file_selection,
					  gint response_id,
					  GtkUtilsFileSelectionData *data);
static void	 overwrite_confirmation (GtkWidget *message_dialog,
					 gint response_id,
					 GtkUtilsFileSelectionData *data);

static void	 browse_button_clicked (GtkWidget *button,
					GtkUtilsBrowseButtonData *data);
static void	 browsing_dialog_response (GtkFileSelection *file_selection,
					   gint response_id,
					   GtkUtilsBrowseButtonData *data);

static void	 advance_focus (GtkWidget *widget);

static gboolean	 time_spin_button_output (GtkSpinButton *spin_button);
static gint	 time_spin_button_input (GtkSpinButton *spin_button,
					 gdouble *new_value);

static void	 do_align_left_widgets (GtkWidget *first_level_child,
					GtkSizeGroup *size_group);
static void	 do_align_left_widget (GtkWidget *widget,
				       GtkSizeGroup **size_group);

static void	 invoke_toolbar_button_callback (GObject *button,
						 gpointer user_data);
static void	 set_toolbar_item_sensitive
		   (GtkWidget *button,
		    const GtkUtilsToolbarCallbackArguments *arguments);

static void	 set_widget_sensitivity_on_toggle
		   (GtkToggleButton *toggle_button, GtkWidget *widget);
static void	 set_widget_sensitivity_on_toggle_reversed
		   (GtkToggleButton *toggle_button, GtkWidget *widget);
static void	 set_widget_sensitivity_on_input (GtkEntry *entry,
						  GtkWidget *widget);

static gint	 freeze_on_empty_input
		   (GtkFreezableSpinButton *freezable_spin_button,
		    gdouble *new_value);


static GQuark	toolbar_button_entry_quark = 0;


void
gtk_utils_add_similar_bindings (GtkBindingSet *binding_set,
				const gchar *signal_name,
				GtkUtilsBindingInfo *bindings,
				int num_bindings)
{
  int k;

  assert (binding_set);
  assert (signal_name);
  assert (bindings);
  assert (num_bindings > 0);

  for (k = 0; k < num_bindings; k++) {
    gtk_binding_entry_add_signal (binding_set,
				  bindings[k].keyval, bindings[k].modifiers,
				  signal_name,
				  1, G_TYPE_INT, bindings[k].signal_parameter);
  }
}


#if GTK_2_2_OR_LATER

void
gtk_utils_make_window_only_horizontally_resizable (GtkWindow *window)
{
  GdkGeometry geometry;

  assert (GTK_IS_WINDOW (window));

  geometry.max_width  = G_MAXINT;
  geometry.max_height = -1;
  gtk_window_set_geometry_hints (window, NULL, &geometry, GDK_HINT_MAX_SIZE);
}

#endif


void
gtk_utils_standardize_dialog (GtkDialog *dialog, GtkWidget *contents)
{
  assert (GTK_IS_DIALOG (dialog));
  assert (GTK_IS_CONTAINER (contents));

  gtk_widget_set_name (GTK_WIDGET (dialog), "quarry-dialog");

  gtk_box_set_spacing (GTK_BOX (dialog->vbox), QUARRY_SPACING);
  gtk_box_pack_start_defaults (GTK_BOX (dialog->vbox), contents);
}


GtkWidget *
gtk_utils_create_message_dialog (GtkWindow *parent, const gchar *icon_stock_id,
				 GtkUtilsMessageDialogFlags flags,
				 const gchar *hint,
				 const gchar *message_format_string, ...)
{
  GtkWidget *widget = gtk_dialog_new ();
  GtkDialog *dialog = GTK_DIALOG (widget);
  GtkWindow *window = GTK_WINDOW (dialog);
  GtkUtilsMessageDialogFlags buttons = flags & GTK_UTILS_BUTTONS_MASK;
  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *hbox;
  gchar *message_text;
  gchar *label_text;
  va_list arguments;

  assert (icon_stock_id);

  gtk_window_set_title (window, "");
  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_skip_pager_hint (window, TRUE);

  /* Note: disabled because KDE window manager doesn't give focus to
   * message dialog after this call (is it a bug or what?).
   */
#if 0
  gtk_window_set_skip_taskbar_hint (window, TRUE);
#endif

  if (parent)
    gtk_window_set_transient_for (window, GTK_WINDOW (parent));

  if (!(flags & GTK_UTILS_NON_MODAL_WINDOW))
    gtk_window_set_modal (window, TRUE);

  gtk_dialog_set_has_separator (dialog, FALSE);

  if (buttons == GTK_UTILS_BUTTONS_OK_CANCEL)
    gtk_dialog_add_button (dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  if (buttons == GTK_UTILS_BUTTONS_OK
      || buttons == GTK_UTILS_BUTTONS_OK_CANCEL) {
    gtk_dialog_add_button (dialog, GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);
  }

  if (buttons == GTK_UTILS_BUTTONS_CLOSE) {
    gtk_dialog_add_button (dialog, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
    gtk_dialog_set_default_response (dialog, GTK_RESPONSE_CLOSE);
  }

  icon = gtk_image_new_from_stock (icon_stock_id, GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (icon), 0.5, 0.0);

  va_start (arguments, message_format_string);
  message_text = g_strdup_vprintf (message_format_string, arguments);
  va_end (arguments);

  if (hint) {
    label_text = g_strdup_printf (("<span weight=\"bold\" size=\"larger\">%s"
				   "</span>\n\n%s"),
				  message_text, hint);
  }
  else {
    label_text = g_strdup_printf (("<span weight=\"bold\" size=\"larger\">%s"
				   "</span>"),
				  message_text);
  }

  label = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_label_set_markup (GTK_LABEL (label), label_text);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);

  g_free (message_text);
  g_free (label_text);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				icon, GTK_UTILS_FILL,
				label, GTK_UTILS_PACK_DEFAULT, NULL);
  gtk_widget_show_all (hbox);

  gtk_utils_standardize_dialog (dialog, hbox);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), QUARRY_SPACING_VERY_BIG);

  if (!(flags & GTK_UTILS_DONT_SHOW))
    gtk_window_present (window);

  if (flags & GTK_UTILS_DESTROY_ON_RESPONSE) {
    g_signal_connect (dialog, "response",
		      G_CALLBACK (gtk_widget_destroy), NULL);
  }

  return widget;
}


void
gtk_utils_null_pointer_on_destroy (GtkWindow **window_pointer,
				   gboolean ask_control_center)
{
  assert (window_pointer && GTK_IS_WINDOW (*window_pointer));

  g_signal_connect (*window_pointer, "destroy",
		    G_CALLBACK (ask_control_center
				? null_pointer_on_destroy_ask_control_center
				: null_pointer_on_destroy),
		    window_pointer);
}


static void
null_pointer_on_destroy (GtkWindow *window, GtkWindow **window_pointer)
{
  assert (window == *window_pointer);

  *window_pointer = NULL;
}


static void
null_pointer_on_destroy_ask_control_center (GtkWindow *window,
					    GtkWindow **window_pointer)
{
  assert (window == *window_pointer);

  if (gtk_control_center_window_destroyed (window))
    *window_pointer = NULL;
}


/* GTK+ has an annoying "feature" (I tend to consider it a bug): if
 * you activate a button from keyboard (i.e. Alt+O for "OK" button),
 * then keyboard changes in the currently focused spin button are not
 * saved.  This might be true for some other widgets too.
 *
 * Every dialog with spin buttons should call this function from all
 * button/response handlers.
 */
void
gtk_utils_workaround_focus_bug (GtkWindow *window)
{
  GtkWidget *focused_widget = gtk_window_get_focus (window);

  if (focused_widget) {
    if (GTK_IS_SPIN_BUTTON (focused_widget))
      gtk_spin_button_update (GTK_SPIN_BUTTON (focused_widget));
  }
}


void
gtk_utils_add_file_selection_response_handlers (GtkWidget *file_selection,
						gboolean saving_file,
						GCallback response_callback,
						gpointer user_data)
{
  GtkUtilsFileSelectionData *data
    = g_malloc (sizeof (GtkUtilsFileSelectionData));

  assert (response_callback);

  data->file_selection = GTK_FILE_SELECTION (file_selection);

  data->saving_file	  = saving_file;
  data->response_callback = (GtkUtilsFileSelectionCallback) response_callback;
  data->user_data	  = user_data;

  g_signal_connect (file_selection, "response",
		    G_CALLBACK (file_selection_response), data);
  g_signal_connect_swapped (file_selection, "destroy",
			    G_CALLBACK (g_free), data);
}


static void
file_selection_response (GtkFileSelection *file_selection, gint response_id,
			 GtkUtilsFileSelectionData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *filename = gtk_file_selection_get_filename (file_selection);

    if (*filename) {
      if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
	/* Don't try to read directory, just browse into it. */
	if (filename[strlen (filename) - 1] != G_DIR_SEPARATOR) {
	  gchar *directory_name = g_strconcat (filename, G_DIR_SEPARATOR_S,
					       NULL);

	  gtk_file_selection_set_filename (file_selection, directory_name);
	  g_free (directory_name);
	}
	else
	  gtk_file_selection_set_filename (file_selection, filename);

	/* We are done, no need to try to open/save. */
	return;
      }
      else if (data->saving_file
	       && g_file_test (filename, G_FILE_TEST_EXISTS)) {
	static const gchar *hint
	  = N_("Note that all information in the existing file will be lost "
	       "permanently if you choose to overwrite it.");
	static const gchar *message_format_string
	  = N_("File named `%s' already exists. "
	       "Do you want to overwrite it with the one you are saving?");

	gchar *filename_in_utf8 = g_filename_to_utf8 (filename, -1,
						      NULL, NULL, NULL);
	GtkWidget *confirmation_dialog
	  = gtk_utils_create_message_dialog (GTK_WINDOW (file_selection),
					     GTK_STOCK_DIALOG_WARNING,
					     (GTK_UTILS_NO_BUTTONS
					      | GTK_UTILS_DONT_SHOW),
					     _(hint),
					     _(message_format_string),
					     filename_in_utf8);

	g_free (filename_in_utf8);

	gtk_dialog_add_buttons (GTK_DIALOG (confirmation_dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				QUARRY_STOCK_OVERWRITE, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (confirmation_dialog),
					 GTK_RESPONSE_OK);

	g_signal_connect (confirmation_dialog, "response",
			  G_CALLBACK (overwrite_confirmation), data);

	gtk_window_present (GTK_WINDOW (confirmation_dialog));

	/* Don't try to overwrite the file before we get user
	 * confirmation.
	 */
	return;
      }
    }
  }

  data->response_callback (file_selection, response_id, data->user_data);
}


static void
overwrite_confirmation (GtkWidget *message_dialog, gint response_id,
			GtkUtilsFileSelectionData *data)
{
  gtk_widget_destroy (message_dialog);

  if (response_id == GTK_RESPONSE_OK) {
    data->response_callback (data->file_selection, GTK_RESPONSE_OK,
			     data->user_data);
  }
}


GtkWidget *
gtk_utils_create_titled_page (GtkWidget *contents,
			      const gchar *icon_stock_id, const gchar *title)
{
  GtkWidget *image = NULL;
  GtkWidget *label = NULL;
  GtkWidget *title_widget;
  GtkWidget *hseparator;

  assert (GTK_IS_WIDGET (contents));
  assert (icon_stock_id || title);

  if (icon_stock_id) {
    image = gtk_image_new_from_stock (icon_stock_id,
				      GTK_ICON_SIZE_LARGE_TOOLBAR);
  }

  if (title) {
    char *marked_up_title
      = utils_cat_strings (NULL,
			   "<span weight=\"bold\" size=\"x-large\">",
			   title, "</span>", NULL);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), marked_up_title);

    utils_free (marked_up_title);
  }

  if (image && label) {
    title_widget = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING_SMALL,
					  image, GTK_UTILS_FILL, label, 0,
					  NULL);
  }
  else if (label) {
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    title_widget = label;
  }
  else
    title_widget = image;

  gtk_widget_show_all (title_widget);

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);

  return gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				title_widget, GTK_UTILS_FILL,
				hseparator, GTK_UTILS_FILL,
				contents, GTK_UTILS_PACK_DEFAULT, NULL);
}


GtkWidget *
gtk_utils_pack_in_box (GType box_type, gint spacing, ...)
{
  void (* packing_function) (GtkBox *box, GtkWidget *widget,
			     gboolean expand, gboolean fill, guint padding)
    = gtk_box_pack_start;

  GtkBox *box = GTK_BOX (g_object_new (box_type, NULL));
  GtkWidget *widget;
  va_list arguments;

  gtk_box_set_spacing (box, spacing);

  va_start (arguments, spacing);
  while ((widget = va_arg (arguments, GtkWidget *)) != NULL) {
    guint packing_parameters = va_arg (arguments, guint);

    assert (GTK_IS_WIDGET (widget));

    if (packing_parameters & GTK_UTILS_PACK_END)
      packing_function = gtk_box_pack_end;

    packing_function (box, widget,
		      packing_parameters & GTK_UTILS_EXPAND ? TRUE : FALSE,
		      packing_parameters & GTK_UTILS_FILL ? TRUE : FALSE,
		      packing_parameters & GTK_UTILS_PACK_PADDING_MASK);
  }

  return GTK_WIDGET (box);
}


GtkWidget *
gtk_utils_align_widget (GtkWidget *widget,
			gfloat x_alignment, gfloat y_alignment)
{
  GtkWidget *alignment = gtk_alignment_new (x_alignment, y_alignment,
					    0.0, 0.0);

  assert (GTK_IS_WIDGET (widget));

  gtk_container_add (GTK_CONTAINER (alignment), widget);

  return alignment;
}


GtkWidget *
gtk_utils_sink_widget (GtkWidget *widget)
{
  GtkWidget *frame = gtk_frame_new (NULL);

  assert (GTK_IS_WIDGET (widget));

  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (frame), widget);

  return frame;
}


GtkWidget *
gtk_utils_make_widget_scrollable (GtkWidget *widget,
				  GtkPolicyType hscrollbar_policy,
				  GtkPolicyType vscrollbar_policy)
{
  GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);

  assert (GTK_IS_WIDGET (widget));

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  hscrollbar_policy, vscrollbar_policy);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
				       GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

  gtk_widget_show_all (scrolled_window);
  return scrolled_window;
}



GtkWidget *
gtk_utils_create_left_aligned_label (const gchar *label_text)
{
  GtkWidget *label = gtk_label_new (label_text);

  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  return label;
}


GtkWidget *
gtk_utils_create_mnemonic_label (const gchar *label_text,
				 GtkWidget *mnemonic_widget)
{
  GtkWidget *label = gtk_label_new_with_mnemonic (label_text);

  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), mnemonic_widget);

  return label;
}


GtkWidget *
gtk_utils_create_entry (const gchar *text, GtkUtilsEntryActivationMode mode)
{
  GtkWidget *entry = gtk_entry_new ();

  if (text)
    gtk_entry_set_text (GTK_ENTRY (entry), text);

  switch (mode) {
  case RETURN_ACTIVATES_DEFAULT:
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    break;

  case RETURN_ADVANCES_FOCUS:
    g_signal_connect (entry, "activate", G_CALLBACK (advance_focus), NULL);
    break;

  case RETURN_DEFAULT_MODE:
    /* It's the default mode, do nothing. */
    break;
  }

  return entry;
}


static void
advance_focus (GtkWidget *widget)
{
  do
    widget = gtk_widget_get_parent (widget);
  while (widget && !GTK_IS_WINDOW (widget));

  if (widget)
    g_signal_emit_by_name (widget, "move-focus", GTK_DIR_TAB_FORWARD);
}


GtkWidget *
gtk_utils_create_browse_button (gboolean with_text,
				GtkWidget *associated_entry,
				gboolean is_command_line_entry,
				const gchar *browsing_dialog_caption,
				GtkUtilsBrowsingDoneCallback callback,
				gpointer user_data)
{
  GtkWidget *button;
  GtkUtilsBrowseButtonData *data
    = g_malloc (sizeof (GtkUtilsBrowseButtonData));

  assert (browsing_dialog_caption);

  if (with_text)
    button = gtk_button_new_from_stock (QUARRY_STOCK_BROWSE);
  else {
    button = gtk_button_new ();
    gtk_container_add (GTK_CONTAINER (button),
		       gtk_image_new_from_stock (QUARRY_STOCK_BROWSE,
						 GTK_ICON_SIZE_BUTTON));
  }

  data->browsing_dialog = NULL;
  data->browsing_dialog_caption = browsing_dialog_caption;

  data->associated_entry = GTK_ENTRY (associated_entry);
  data->is_command_line_entry = is_command_line_entry;

  data->callback = callback;
  data->user_data = user_data;

  g_signal_connect (button, "clicked",
		    G_CALLBACK (browse_button_clicked), data);
  g_signal_connect_swapped (button, "destroy", G_CALLBACK (g_free), data);

  return button;
}


static void
browse_button_clicked (GtkWidget *button, GtkUtilsBrowseButtonData *data)
{
  if (!data->browsing_dialog) {
    GtkWidget *file_selection_widget
      = gtk_file_selection_new (data->browsing_dialog_caption);
    GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_toplevel (button));
    const gchar *current_entry_text;

    data->browsing_dialog = GTK_WINDOW (file_selection_widget);
    gtk_window_set_transient_for (data->browsing_dialog, parent_window);
    gtk_window_set_destroy_with_parent (data->browsing_dialog, TRUE);

    current_entry_text = gtk_entry_get_text (data->associated_entry);
    if (*current_entry_text) {
      GtkFileSelection *file_selection
	= GTK_FILE_SELECTION (data->browsing_dialog);
      gchar *disk_encoded_filename = NULL;

      if (data->is_command_line_entry) {
	gchar **argv;

	if (g_shell_parse_argv (current_entry_text, NULL, &argv, NULL)) {
	  disk_encoded_filename = g_filename_from_utf8 (argv[0], -1,
							NULL, NULL, NULL);
	  g_strfreev (argv);
	}
      }
      else {
	disk_encoded_filename = g_filename_from_utf8 (current_entry_text, -1,
						      NULL, NULL, NULL);
      }

      gtk_file_selection_set_filename (file_selection, disk_encoded_filename);
      g_free (disk_encoded_filename);
    }

    gtk_utils_add_file_selection_response_handlers
      (file_selection_widget, FALSE,
       G_CALLBACK (browsing_dialog_response), data);
  }

  gtk_window_present (data->browsing_dialog);
}


static void
browsing_dialog_response (GtkFileSelection *file_selection,
			  gint response_id, GtkUtilsBrowseButtonData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *filename   = gtk_file_selection_get_filename (file_selection);
    gchar *filename_in_utf8 = g_filename_to_utf8 (filename, -1,
						  NULL, NULL, NULL);

    gtk_entry_set_text (data->associated_entry, filename_in_utf8);
    gtk_widget_grab_focus (GTK_WIDGET (data->associated_entry));

    g_free (filename_in_utf8);

    if (data->callback)
      data->callback (data->associated_entry, NULL, data->user_data);
  }

  gtk_widget_destroy (GTK_WIDGET (file_selection));
  data->browsing_dialog = NULL;
}


GtkWidget *
gtk_utils_create_spin_button (GtkAdjustment *adjustment, gdouble climb_rate,
			      guint num_digits, gboolean snap_to_ticks)
{
  GtkWidget *spin_button = gtk_spin_button_new (adjustment, climb_rate,
						num_digits);

  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (spin_button),
				     snap_to_ticks);
  gtk_entry_set_alignment (GTK_ENTRY (spin_button), 1.0);
  gtk_entry_set_activates_default (GTK_ENTRY (spin_button), TRUE);

  return spin_button;
}


GtkWidget *
gtk_utils_create_freezable_spin_button (GtkAdjustment *adjustment,
					gdouble climb_rate, guint num_digits,
					gboolean snap_to_ticks)
{
  GtkWidget *spin_button = gtk_freezable_spin_button_new (adjustment,
							  climb_rate,
							  num_digits);

  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (spin_button),
				     snap_to_ticks);
  gtk_entry_set_alignment (GTK_ENTRY (spin_button), 1.0);
  gtk_entry_set_activates_default (GTK_ENTRY (spin_button), TRUE);

  return spin_button;
}


void
gtk_utils_convert_to_time_spin_button (GtkSpinButton *spin_button)
{
  GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (spin_button);
  gdouble upper_limit;
  gint max_width;

  assert (adjustment);

  g_signal_connect (spin_button, "output",
		    G_CALLBACK (time_spin_button_output), NULL);
  g_signal_connect (spin_button, "input",
		    G_CALLBACK (time_spin_button_input), NULL);

  if (adjustment->upper > 60.0 * 60.0) {
    /* One less, since two colons would normally take space of only
     * one "common" char.  Anyway Quarry time upper bounds are not
     * something one would use.
     */
    for (max_width = 6, upper_limit = adjustment->upper / (60.0 * 60.0);
	 upper_limit >= 10.0; upper_limit /= 10.0)
      max_width++;
  }
  else
    max_width = 5;

  gtk_entry_set_width_chars (GTK_ENTRY (spin_button), max_width);
}


GtkWidget *
gtk_utils_create_time_spin_button (GtkAdjustment *adjustment,
				   gdouble climb_rate)
{
  GtkWidget *spin_button = gtk_utils_create_spin_button (adjustment,
							 climb_rate,
							 0, FALSE);

  gtk_utils_convert_to_time_spin_button (GTK_SPIN_BUTTON (spin_button));
  return spin_button;
}


static gboolean
time_spin_button_output (GtkSpinButton *spin_button)
{
  gint value = gtk_spin_button_get_adjustment (spin_button)->value;
  char buffer[32];

  /* Don't print zero hours.  In Quarry context hours are probably
   * used only occasionally.
   */
  if (value >= 60 * 60) {
    utils_ncprintf (buffer, sizeof buffer, "%d:%02d:%02d",
		    value / (60 * 60), (value / 60) % 60, value % 60);
  }
  else {
    utils_ncprintf (buffer, sizeof buffer, "%02d:%02d",
		    value / 60, value % 60);
  }

  gtk_entry_set_text (GTK_ENTRY (spin_button), buffer);

  return TRUE;
}


static gint
time_spin_button_input (GtkSpinButton *spin_button, gdouble *new_value)
{
  int seconds
    = utils_parse_time (gtk_entry_get_text (GTK_ENTRY (spin_button)));

  if (seconds >= 0) {
    *new_value = (gdouble) seconds;
    return TRUE;
  }

  return GTK_INPUT_ERROR;
}


GtkWidget *
gtk_utils_create_selector (const gchar **items, gint num_items,
			   gint selected_item)
{
  GtkWidget *widget;
  int k;

  assert (items);
  assert (num_items > 0);

#if GTK_2_4_OR_LATER

  widget = gtk_combo_box_new_text ();

  {
    GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

    for (k = 0; k < num_items; k++)
      gtk_combo_box_append_text (combo_box, _(items[k]));

    gtk_combo_box_set_active (combo_box,
			      (0 <= selected_item && selected_item < num_items
			       ? selected_item : 0));
  }

#else /* not GTK_2_4_OR_LATER */

  widget = gtk_option_menu_new ();

  {
    GtkWidget *menu = gtk_menu_new ();
    GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);

    for (k = 0; k < num_items; k++) {
      gtk_menu_shell_append (menu_shell,
			     gtk_menu_item_new_with_label (_(items[k])));
    }

    gtk_widget_show_all (menu);
    gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
  }

  if (0 <= selected_item && selected_item < num_items)
    gtk_option_menu_set_history (GTK_OPTION_MENU (widget), selected_item);

#endif /* not GTK_2_4_OR_LATER */

  return widget;
}


GtkWidget *
gtk_utils_create_selector_from_string_list (void *abstract_list,
					    const gchar *selected_item)
{
  GtkWidget *widget;
  StringList *string_list = (StringList *) abstract_list;
  StringListItem *list_item;
  int k;

  assert (string_list);
  assert (string_list->first);

#if GTK_2_4_OR_LATER

  widget = gtk_combo_box_new_text ();

  {
    GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

    for (list_item = string_list->first, k = 0; list_item;
	 list_item = list_item->next, k++) {
      gtk_combo_box_append_text (combo_box, list_item->text);

      if (selected_item && strcmp (list_item->text, selected_item) == 0) {
	gtk_combo_box_set_active (combo_box, k);
	selected_item = NULL;
      }
    }
  }

#else /* not GTK_2_4_OR_LATER */

  widget = gtk_option_menu_new ();

  {
    GtkWidget *menu = gtk_menu_new ();
    GtkMenuShell *menu_shell = GTK_MENU_SHELL (menu);
    int selected_item_index = -1;

    for (list_item = string_list->first, k = 0; list_item;
	 list_item = list_item->next, k++) {
      gtk_menu_shell_append (menu_shell,
			     gtk_menu_item_new_with_label (list_item->text));

      if (selected_item && strcmp (list_item->text, selected_item) == 0) {
	selected_item_index = k;
	selected_item = NULL;
      }
    }

    gtk_widget_show_all (menu);
    gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);

    if (selected_item_index != -1) {
      gtk_option_menu_set_history (GTK_OPTION_MENU (widget),
				   selected_item_index);
    }
  }

#endif /* not GTK_2_4_OR_LATER */

  return widget;
}


GtkWidget *
gtk_utils_create_invisible_notebook (void)
{
  GtkWidget *notebook = gtk_notebook_new ();

  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);

  return notebook;
}


void
gtk_utils_create_radio_chain (GtkWidget **radio_buttons,
			      const gchar **label_texts,
			      gint num_radio_buttons)
{
  GtkRadioButton *last_button = NULL;
  int k;

  assert (radio_buttons);
  assert (label_texts);

  for (k = 0; k < num_radio_buttons; k++) {
    radio_buttons[k]
      = gtk_radio_button_new_with_mnemonic_from_widget (last_button,
							_(label_texts[k]));
    last_button = GTK_RADIO_BUTTON (radio_buttons[k]);
  }
}


GtkSizeGroup *
gtk_utils_create_size_group (GtkSizeGroupMode mode, ...)
{
  GtkSizeGroup *size_group = gtk_size_group_new (mode);
  GtkWidget *widget;
  va_list arguments;

  va_start (arguments, mode);
  while ((widget = va_arg (arguments, GtkWidget *)) != NULL) {
    assert (GTK_IS_WIDGET (widget));
    gtk_size_group_add_widget (size_group, widget);
  }

  va_end (arguments);

  g_object_unref (size_group);
  return size_group;
}


GtkSizeGroup *
gtk_utils_align_left_widgets (GtkContainer *container,
			      GtkSizeGroup *size_group)
{
  assert (GTK_IS_CONTAINER (container));
  assert (!size_group || GTK_IS_SIZE_GROUP (size_group));

  if (!size_group)
    size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_container_foreach (container,
			 (GtkCallback) do_align_left_widgets, size_group);

  return size_group;
}


static void
do_align_left_widgets (GtkWidget *first_level_child, GtkSizeGroup *size_group)
{
  if (GTK_IS_HBOX (first_level_child)
      || GTK_IS_HBUTTON_BOX (first_level_child)) {
    GtkSizeGroup *size_group_copy = size_group;

    gtk_container_foreach (GTK_CONTAINER (first_level_child),
			   (GtkCallback) do_align_left_widget,
			   &size_group_copy);
  }
  else if (GTK_IS_CONTAINER (first_level_child)) {
    gtk_container_foreach (GTK_CONTAINER (first_level_child),
			   (GtkCallback) do_align_left_widgets, size_group);
  }
}


static void
do_align_left_widget (GtkWidget *widget, GtkSizeGroup **size_group)
{
  if (*size_group) {
    if (!GTK_IS_BOX (widget)) {
      gtk_size_group_add_widget (*size_group, widget);
      *size_group = NULL;
    }
    else
      do_align_left_widgets (widget, *size_group);
  }
}



GtkWidget *
gtk_utils_append_toolbar_button (GtkToolbar *toolbar,
				 GtkUtilsToolbarEntry *entry,
				 gpointer user_data)
{
  GtkWidget *button;

  assert (entry);

  button = (gtk_toolbar_append_item
	    (toolbar, (entry->label_text ? _(entry->label_text) : NULL),
	     _(entry->tooltip_text), NULL,
	     gtk_image_new_from_stock (entry->icon_stock_id,
				       gtk_toolbar_get_icon_size (toolbar)),
	     (GtkSignalFunc) invoke_toolbar_button_callback, user_data));

  if (!toolbar_button_entry_quark) {
    toolbar_button_entry_quark
      = g_quark_from_static_string ("quarry-toolbar-button-entry");
  }

  g_object_set_qdata (G_OBJECT (button), toolbar_button_entry_quark, entry);

  return button;
}


static void
invoke_toolbar_button_callback (GObject *button, gpointer user_data)
{
  const GtkUtilsToolbarEntry *entry
    = g_object_get_qdata (button, toolbar_button_entry_quark);

  entry->callback (user_data, entry->callback_action);
}


void
gtk_utils_set_toolbar_buttons_sensitive (GtkToolbar *toolbar,
					 gboolean are_sensitive, ...)
{
  GtkUtilsToolbarCallbackArguments callback_arguments;

  assert (GTK_IS_TOOLBAR (toolbar));
  assert (toolbar_button_entry_quark);

  callback_arguments.are_sensitive = are_sensitive;
  va_start (callback_arguments.entries, are_sensitive);

  gtk_container_foreach (GTK_CONTAINER (toolbar),
			 (GtkCallback) set_toolbar_item_sensitive,
			 &callback_arguments);

  va_end (callback_arguments.entries);
}


static void
set_toolbar_item_sensitive (GtkWidget *button,
			    const GtkUtilsToolbarCallbackArguments *arguments)
{
  const GtkUtilsToolbarEntry *button_entry
    = g_object_get_qdata (G_OBJECT (button), toolbar_button_entry_quark);

  if (button_entry) {
    va_list entries_copy;
    const GtkUtilsToolbarEntry *entry;

    QUARRY_VA_COPY (entries_copy, arguments->entries);
    while ((entry = va_arg (entries_copy, const GtkUtilsToolbarEntry *))
	   != NULL) {
      if (entry == button_entry) {
	gtk_widget_set_sensitive (button, arguments->are_sensitive);
	break;
      }
    }

    va_end (entries_copy);
  }
}



/* Set `text_buffer's text.  This is a convenient function.  If `text'
 * doesn't end in a newline, it silently appends one, to ease editing.
 * If `text' is NULL, then it sets text buffer's text to be empty
 * string.  Finally, it resets text buffer's `modified' flag.
 */
void
gtk_utils_set_text_buffer_text (GtkTextBuffer *text_buffer, const gchar *text)
{
  assert (GTK_IS_TEXT_BUFFER (text_buffer));

  if (text) {
    gint length = strlen (text);
    GtkTextIter start_iterator;

    gtk_text_buffer_set_text (text_buffer, text, length);

    gtk_text_buffer_get_start_iter (text_buffer, &start_iterator);
    gtk_text_buffer_place_cursor (text_buffer, &start_iterator);

    if (text[length - 1] != '\n') {
      GtkTextIter end_iterator;

      gtk_text_buffer_get_end_iter (text_buffer, &end_iterator);
      gtk_text_buffer_insert (text_buffer, &end_iterator, "\n", 1);
    }
  }
  else
    gtk_text_buffer_set_text (text_buffer, "", 0);
}



void
gtk_utils_set_sensitive_on_toggle (GtkToggleButton *toggle_button,
				   GtkWidget *widget, gboolean reverse_meaning)
{
  void (* set_widget_sensitivity_function) (GtkToggleButton *toggle_button,
					    GtkWidget *widget)
    = (reverse_meaning
       ? set_widget_sensitivity_on_toggle_reversed
       : set_widget_sensitivity_on_toggle);

  assert (GTK_IS_TOGGLE_BUTTON (toggle_button));
  assert (GTK_IS_WIDGET (widget));

  set_widget_sensitivity_function (toggle_button, widget);
  g_signal_connect (toggle_button, "toggled",
		    G_CALLBACK (set_widget_sensitivity_function),
		    widget);
}


static void
set_widget_sensitivity_on_toggle (GtkToggleButton *toggle_button,
				  GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget,
			    gtk_toggle_button_get_active (toggle_button));
}


static void
set_widget_sensitivity_on_toggle_reversed (GtkToggleButton *toggle_button,
					   GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget,
			    !gtk_toggle_button_get_active (toggle_button));
}


void
gtk_utils_set_sensitive_on_input (GtkEntry *entry, GtkWidget *widget)
{
  assert (GTK_IS_ENTRY (entry));
  assert (GTK_IS_WIDGET (widget));

  set_widget_sensitivity_on_input (entry, widget);
  g_signal_connect (entry, "changed",
		    G_CALLBACK (set_widget_sensitivity_on_input), widget);
}


static void
set_widget_sensitivity_on_input (GtkEntry *entry, GtkWidget *widget)
{
  const gchar *text = gtk_entry_get_text (entry);

  gtk_widget_set_sensitive (widget, *text ? TRUE : FALSE);
}


void
gtk_utils_freeze_on_empty_input (GtkFreezableSpinButton *freezable_spin_button)
{
  assert (GTK_IS_FREEZABLE_SPIN_BUTTON (freezable_spin_button));

  g_signal_connect (freezable_spin_button, "input",
		    G_CALLBACK (freeze_on_empty_input), NULL);
}


static gint
freeze_on_empty_input (GtkFreezableSpinButton *freezable_spin_button,
		       gdouble *new_value)
{
  const gchar *entry_text
    = gtk_entry_get_text (GTK_ENTRY (freezable_spin_button));

  while (*entry_text == ' ' || *entry_text == '\t'
	 || *entry_text == '\n' || *entry_text == '\r'
	 || *entry_text == '\v' || *entry_text == '\f')
    entry_text++;

  if (*entry_text)
    return FALSE;

  *new_value = 0.0;
  gtk_freezable_spin_button_freeze_and_stop_input (freezable_spin_button, "");

  return TRUE;
}



void
gtk_utils_set_widgets_visible (gboolean visible, ...)
{
  GtkWidget *widget;
  va_list arguments;

  va_start (arguments, visible);
  while ((widget = va_arg (arguments, GtkWidget *)) != NULL) {
    if (visible && !GTK_WIDGET_VISIBLE (widget))
      gtk_widget_show (widget);
    else if (!visible && GTK_WIDGET_VISIBLE (widget))
      gtk_widget_hide (widget);
  }

  va_end (arguments);
}


void
gtk_utils_set_menu_items_sensitive (GtkItemFactory *item_factory,
				    gboolean are_sensitive, ...)
{
  va_list arguments;
  const gchar *path;

  assert (GTK_IS_ITEM_FACTORY (item_factory));

  va_start (arguments, are_sensitive);
  while ((path = va_arg (arguments, const gchar *)) != NULL) {
    gtk_widget_set_sensitive (gtk_item_factory_get_widget (item_factory, path),
			      are_sensitive);
  }

  va_end (arguments);
}


void
gtk_utils_set_gdk_color (GdkColor *gdk_color, QuarryColor quarry_color)
{
  assert (gdk_color);

  gdk_color->red   = (quarry_color.red * G_MAXUINT16) / G_MAXUINT8;
  gdk_color->green = (quarry_color.green * G_MAXUINT16) / G_MAXUINT8;
  gdk_color->blue  = (quarry_color.blue * G_MAXUINT16) / G_MAXUINT8;
}


void
gtk_utils_set_quarry_color (QuarryColor *quarry_color,
			    const GdkColor *gdk_color)
{
  assert (quarry_color);
  assert (gdk_color);

  quarry_color->red   = (gdk_color->red * G_MAXUINT8) / G_MAXUINT16;
  quarry_color->green = (gdk_color->green * G_MAXUINT8) / G_MAXUINT16;
  quarry_color->blue  = (gdk_color->blue * G_MAXUINT8) / G_MAXUINT16;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
