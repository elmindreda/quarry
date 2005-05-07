/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005 Paul Pogonyshev.                             *
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


#ifndef QUARRY_GTK_FILE_SELECTOR_H
#define QUARRY_GTK_FILE_SELECTOR_H


#include "gtk-utils.h"
#include "quarry.h"


/* FIXME: Try to do the same with GtkCombo for pre-2.4 GTK+? */


#if GTK_2_4_OR_LATER


#include <gtk/gtk.h>


#define GTK_TYPE_FILE_SELECTOR	(gtk_file_selector_get_type ())

#define GTK_FILE_SELECTOR(object)					\
  GTK_CHECK_CAST ((object), GTK_TYPE_FILE_SELECTOR, GtkFileSelector)

#define GTK_FILE_SELECTOR_CLASS(class)					\
  GTK_CHECK_CLASS_CAST ((class), GTK_TYPE_FILE_SELECTOR,		\
			GtkFileSelectorClass)

#define GTK_IS_FILE_SELECTOR(object)					\
  GTK_CHECK_TYPE ((object), GTK_TYPE_FILE_SELECTOR)

#define GTK_IS_FILE_SELECTOR_CLASS(class)				\
  GTK_CHECK_CLASS_TYPE ((class), GTK_TYPE_FILE_SELECTOR)

#define GTK_FILE_SELECTOR_GET_CLASS(object)				\
  GTK_CHECK_GET_CLASS ((object), GTK_TYPE_FILE_SELECTOR,		\
		       GtkFileSelectorClass)


typedef struct _GtkFileSelector		GtkFileSelector;
typedef struct _GtkFileSelectorClass	GtkFileSelectorClass;

struct _GtkFileSelector {
  GtkComboBoxEntry	  combo_box_entry;

  GSList		 *glob_patterns;

  gchar			 *last_directory;
};

struct _GtkFileSelectorClass {
  GtkComboBoxEntryClass	  parent_class;
};


GType		gtk_file_selector_get_type (void);

GtkWidget *	gtk_file_selector_new (void);

void		gtk_file_selector_set_glob_patterns (GtkFileSelector *selector,
						     const gchar *patterns);

void		gtk_file_selector_set_text (GtkFileSelector *selector,
					    const gchar *text);
void		gtk_file_selector_repopulate (GtkFileSelector *selector);


#endif /* GTK_2_4_OR_LATER */


#endif /* QUARRY_GTK_FILE_SELECTOR_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
