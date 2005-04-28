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


#ifndef QUARRY_GTK_PARSER_INTERFACE_H
#define QUARRY_GTK_PARSER_INTERFACE_H


#include "sgf.h"
#include "quarry.h"

#include <gtk/gtk.h>


typedef void (* GtkHandleParsedData) (SgfCollection *sgf_collection,
				      SgfErrorList *error_list,
				      const gchar *filename);


void		gtk_parser_interface_present_default (void);
void		gtk_parser_interface_present (GtkWindow **dialog_window,
					      const gchar *title,
					      GtkHandleParsedData callback);

void		gtk_parse_sgf_file (const char *filename, GtkWindow *parent,
				    GtkHandleParsedData callback);


#endif /* QUARRY_GTK_PARSER_INTERFACE_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
