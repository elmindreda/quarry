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

/* This file is just a wrapper around `gui-utils/tile-set.h' that
 * forces it to define `TileSet' structure with `GdkPixbuf *' type of
 * tiles, so that it can be used without type casting from GTK+
 * interface.
 */


#ifndef QUARRY_GTK_TILE_SET_INTERFACE_H
#define QUARRY_GTK_TILE_SET_INTERFACE_H


#include "quarry.h"

/* Note the unusual header including sequence.  This is needed for
 * `typedef' in `gui-utils/tile-set.h' to work properly.
 */
#include <gdk-pixbuf/gdk-pixbuf.h>

#define TILE_IMAGE_TYPE		GdkPixbuf
#include "tile-set.h"


#endif /* QUARRY_GTK_TILE_SET_INTERFACE_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
