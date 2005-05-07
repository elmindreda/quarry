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
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,     *
 * Boston, MA 02110-1301, USA.                                     *
 \* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef QUARRY_GTK_CONTROL_CENTER_H
#define QUARRY_GTK_CONTROL_CENTER_H


#include "quarry.h"

#include <gtk/gtk.h>


void		gtk_control_center_present (void);

inline void	gtk_control_center_window_created (GtkWindow *window);
gint		gtk_control_center_window_destroyed (const GtkWindow *window);

inline void	gtk_control_center_new_reason_to_live (void);
inline void	gtk_control_center_lost_reason_to_live (void);

void		gtk_control_center_quit (void);


#endif /* QUARRY_GTK_CONTROL_CENTER_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
