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


#ifndef QUARRY_GTK_GTP_CLIENT_INTERFACE_H
#define QUARRY_GTK_GTP_CLIENT_INTERFACE_H


#include "gtp-client.h"
#include "quarry.h"

#include <gtk/gtk.h>


typedef void (* GtkGtpClientDeletedCallback) (GtpClient *client,
					      GError *shutdown_reason,
					      void *user_data);


GtpClient *	gtk_create_gtp_client
		  (const gchar *command_line,
		   GtpClientInitializedCallback initialized_callback,
		   GtkGtpClientDeletedCallback deleted_callback,
		   void *user_data,
		   GError **error);
gboolean	gtk_schedule_gtp_client_deletion (const GtpClient *client);


#endif /* QUARRY_GTK_GTP_CLIENT_INTERFACE_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
