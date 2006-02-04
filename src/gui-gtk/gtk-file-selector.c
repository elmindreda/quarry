/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005, 2006 Paul Pogonyshev.                       *
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


#include "gtk-utils.h"


#if GTK_2_4_OR_LATER


#include "gtk-file-selector.h"

#include "quarry-marshal.h"

#include <gtk/gtk.h>
#include <string.h>


static void	 gtk_file_selector_class_init (GtkFileSelectorClass *class);
static void	 gtk_file_selector_init (GtkFileSelector *selector);

static void	 gtk_file_selector_finalize (GObject *object);

static gboolean	 entry_focus_out_event (GtkFileSelector *selector);

static void	 free_glob_patterns (GtkFileSelector *selector);


static GtkComboBoxEntryClass  *parent_class;


GType
gtk_file_selector_get_type (void)
{
  static GType file_selector_type = 0;

  if (!file_selector_type) {
    static const GTypeInfo file_selector_info = {
      sizeof (GtkFileSelectorClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_file_selector_class_init,
      NULL,
      NULL,
      sizeof (GtkFileSelector),
      0,
      (GInstanceInitFunc) gtk_file_selector_init,
      NULL
    };

    file_selector_type
      = g_type_register_static (GTK_TYPE_COMBO_BOX_ENTRY, "GtkFileSelector",
				&file_selector_info, 0);
  }

  return file_selector_type;
}


static void
gtk_file_selector_class_init (GtkFileSelectorClass *class)
{
  parent_class = g_type_class_peek_parent (class);

  G_OBJECT_CLASS (class)->finalize = gtk_file_selector_finalize;
}


static void
gtk_file_selector_init (GtkFileSelector *selector)
{
  GtkListStore *files_list = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_combo_box_set_model (GTK_COMBO_BOX (selector),
			   GTK_TREE_MODEL (files_list));
  gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (selector), 0);
  g_object_unref (files_list);

  g_signal_connect_swapped (GTK_BIN (selector)->child, "focus-out-event",
			    G_CALLBACK (entry_focus_out_event), selector);

  selector->glob_patterns  = NULL;
  selector->last_directory = NULL;
}


GtkWidget *
gtk_file_selector_new (void)
{
  return g_object_new (GTK_TYPE_FILE_SELECTOR, NULL);
}


static gboolean
entry_focus_out_event (GtkFileSelector *selector)
{
  gtk_file_selector_repopulate (selector);
  return FALSE;
}


static void
gtk_file_selector_finalize (GObject *object)
{
  GtkFileSelector *selector = GTK_FILE_SELECTOR (object);

  g_free (selector->last_directory);
  free_glob_patterns (selector);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


void
gtk_file_selector_set_glob_patterns (GtkFileSelector *selector,
				     const gchar *patterns)
{
  g_return_if_fail (GTK_IS_FILE_SELECTOR (selector));

  free_glob_patterns (selector);

  if (patterns) {
    const gchar *pattern;

    for (pattern = patterns; *pattern; pattern += strlen (pattern) + 1) {
      selector->glob_patterns = g_slist_prepend (selector->glob_patterns,
						 g_pattern_spec_new (pattern));
    }
  }
}


void
gtk_file_selector_set_text (GtkFileSelector *selector, const gchar *text)
{
  g_return_if_fail (GTK_IS_FILE_SELECTOR (selector));
  g_return_if_fail (text);

  gtk_entry_set_text (GTK_ENTRY (GTK_BIN (selector)->child), text);

  gtk_file_selector_repopulate (selector);
}


void
gtk_file_selector_repopulate (GtkFileSelector *selector)
{
  GtkEntry *entry;
  gchar *current_directory;

  g_return_if_fail (GTK_IS_FILE_SELECTOR (selector));

  entry = GTK_ENTRY (GTK_BIN (selector)->child);
  current_directory = g_path_get_dirname (gtk_entry_get_text (entry));

  if (!selector->last_directory
      || strcmp (current_directory, selector->last_directory) != 0) {
    GtkListStore *list_store
      = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (selector)));
    GDir *directory;
    GError *error = NULL;

    gtk_list_store_clear (list_store);

    g_free (selector->last_directory);
    selector->last_directory = current_directory;

    directory = g_dir_open (current_directory, 0, &error);
    if (!error) {
      const gchar *filename;

      while ((filename = g_dir_read_name (directory)) != NULL) {
	if (!g_file_test (filename, G_FILE_TEST_IS_DIR)) {
	  gchar *filename_with_path;
	  GtkTreeIter iterator;

	  if (selector->glob_patterns) {
	    GSList *item;
	    guint filename_length = strlen (filename);
	    gchar *filename_reversed = g_utf8_strreverse (filename,
							  filename_length);

	    for (item = selector->glob_patterns; item; item = item->next) {
	      if (g_pattern_match ((GPatternSpec *) (item->data),
				   filename_length,
				   filename, filename_reversed))
		break;
	    }

	    g_free (filename_reversed);

	    if (!item)
	      continue;
	  }

	  filename_with_path = g_strconcat (current_directory,
					    G_DIR_SEPARATOR_S,
					    filename, NULL);

	  gtk_list_store_append (list_store, &iterator);
	  gtk_list_store_set (list_store, &iterator,
			      0, filename_with_path, -1);
	  g_free (filename_with_path);
	}
      }

      g_dir_close (directory);
    }
    else
      g_error_free (error);
  }
  else
    g_free (current_directory);
}


static void
free_glob_patterns (GtkFileSelector *selector)
{
  if (selector->glob_patterns) {
    g_slist_foreach (selector->glob_patterns, (GFunc) g_pattern_spec_free,
		     NULL);

    g_slist_free (selector->glob_patterns);
    selector->glob_patterns = NULL;
  }
}


#endif /* GTK_2_4_OR_LATER */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
