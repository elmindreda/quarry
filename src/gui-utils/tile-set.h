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


#ifndef QUARRY_TILE_SET_H
#define QUARRY_TILE_SET_H


#include "board.h"
#include "quarry.h"


/* When this header is included from GUI directory, this macro should
 * be defined to whatever image type is used for tiles.  This saves
 * constant casting from `void *' at GUI level.
 */
#ifndef TILE_IMAGE_TYPE
#define TILE_IMAGE_TYPE		void
#endif


/* Internally, everything is called "stone", but other game's pieces
 * don't look like Go stones.
 */
typedef enum {
  GO_STONE,

  /* FIXME */
  OTHELLO_DISK = GO_STONE,
  AMAZONS_QUEEN = GO_STONE,

  NUM_STONE_TYPES
} StoneType;

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


typedef TILE_IMAGE_TYPE	TileImage;

typedef struct _TileSet		 TileSet;

struct _TileSet {
  int		    cell_size;

  int		    stones_x_offset;
  int		    stones_y_offset;

  TileImage	   *tiles[NUM_TILES];
};


typedef struct _StoneParameters	   StoneParameters;
typedef struct _TileSetParameters  TileSetParameters;

struct _StoneParameters {
  /* Fields describing stone's material qualities. */
  double	    ambiance_level;
  double	    diffusion_level;
  double	    highlight_level;
  double	    highlight_sharpness;

  /* Stone's color. */
  unsigned char	    red;
  unsigned char	    green;
  unsigned char	    blue;
};

struct _TileSetParameters {
  double	    relative_stone_size;
  double	    lense_radius;
  double	    ellipsoid_radius;

  double	    light_x;
  double	    light_y;
  int		    shadow_level;

  StoneParameters   stone[NUM_COLORS];
};


TileSet *	tile_set_create_or_reuse(int cell_size,
					 const TileSetParameters *parameters);
void		tile_set_unreference(TileSet *tile_set);

void		tile_set_dump_recycle(int lazy_recycling);


/* GUI back-end specific functions (don't belong to GUI utils). */
void *		gui_back_end_image_new(const unsigned char *pixel_data,
				       int width, int height);
void		gui_back_end_image_delete(void *image);


/* FIXME: will need to be removed or replaced with game-specific
 *	  alternatives.
 */
extern const TileSetParameters	tile_set_defaults;


#endif /* QUARRY_TILE_SET_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
