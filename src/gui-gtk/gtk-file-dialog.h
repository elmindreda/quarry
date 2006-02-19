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


#ifndef QUARRY_GTK_FILE_DIALOG_H
#define QUARRY_GTK_FILE_DIALOG_H


#include "quarry.h"

#include <gtk/gtk.h>


GtkWidget *	gtk_file_dialog_new (const gchar *title, GtkWindow *parent,
				     gboolean for_opening,
				     const gchar *affirmative_button_text);

gchar *		gtk_file_dialog_get_filename (GtkWidget *dialog);
void		gtk_file_dialog_set_filename (GtkWidget *dialog,
					      const gchar *filename);
void		gtk_file_dialog_set_current_name (GtkWidget *dialog,
						  const gchar *filename);


#endif /* QUARRY_GTK_FILE_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
