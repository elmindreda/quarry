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


#ifndef QUARRY_GTK_TILE_SET_H
#define QUARRY_GTK_TILE_SET_H


#include "sgf.h"
#include "board.h"
#include "utils.h"
#include "quarry.h"

#include <gdk-pixbuf/gdk-pixbuf.h>


#define NUM_SGF_MARKUP_BACKGROUNDS	(MAX (BLACK, WHITE) + 1)

/* We consider ``last move markup'' a pseudo-SGF markup in the GTK+
 * GUI.  So, we need to tweak things a little here.
 */
#define SGF_PSEUDO_MARKUP_LAST_MOVE	NUM_SGF_MARKUPS
#define NUM_ALL_SGF_MARKUPS		(SGF_PSEUDO_MARKUP_LAST_MOVE + 1)

#define SGF_MARKUP_OPAQUE		0
#define SGF_MARKUP_25_TRANSPARENT	NUM_ALL_SGF_MARKUPS
#define SGF_MARKUP_50_TRANSPARENT	(SGF_MARKUP_25_TRANSPARENT	\
					 + NUM_ALL_SGF_MARKUPS)

#define NUM_ALL_SGF_MARKUP_SHADES	(SGF_MARKUP_50_TRANSPARENT	\
					 + NUM_ALL_SGF_MARKUPS)


enum {
  TILE_NONE    = EMPTY,
  STONE_OPAQUE = FIRST_COLOR,
  BLACK_OPAQUE = STONE_OPAQUE + BLACK_INDEX,
  WHITE_OPAQUE = STONE_OPAQUE + WHITE_INDEX,
  TILE_SPECIAL = SPECIAL_ON_GRID_VALUE,
  STONE_25_TRANSPARENT,
  BLACK_25_TRANSPARENT = STONE_25_TRANSPARENT + BLACK_INDEX,
  WHITE_25_TRANSPARENT = STONE_25_TRANSPARENT + WHITE_INDEX,
  STONE_50_TRANSPARENT,
  BLACK_50_TRANSPARENT = STONE_50_TRANSPARENT + BLACK_INDEX,
  WHITE_50_TRANSPARENT = STONE_50_TRANSPARENT + WHITE_INDEX,
  MIXED_50_TRANSPARENT,

  NUM_TILES
};


typedef struct _GtkMainTileSet		GtkMainTileSet;
typedef struct _GtkSgfMarkupTileSet	GtkSgfMarkupTileSet;

struct _GtkMainTileSet {
  gint		tile_size;

  gint		stones_x_offset;
  gint		stones_y_offset;

  GdkPixbuf    *tiles[NUM_TILES];
};

struct _GtkSgfMarkupTileSet {
  gint		tile_size;

  GdkPixbuf    *tiles[NUM_ALL_SGF_MARKUP_SHADES][NUM_SGF_MARKUP_BACKGROUNDS];
};


GtkMainTileSet *       gtk_main_tile_set_create_or_reuse (gint tile_size,
							  Game game);
GtkSgfMarkupTileSet *  gtk_sgf_markup_tile_set_create_or_reuse (gint tile_size,
								Game game);


extern ObjectCache	gtk_main_tile_set_cache;
extern ObjectCache	gtk_sgf_markup_tile_set_cache;


#endif /* QUARRY_GTK_TILE_SET_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
