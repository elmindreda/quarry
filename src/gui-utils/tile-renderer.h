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


#ifndef QUARRY_TILE_RENDERER_H
#define QUARRY_TILE_RENDERER_H


#include "board.h"
#include "utils.h"
#include "quarry.h"


typedef struct _PieceParameters		PieceParameters;
typedef struct _LightParameters		LightParameters;

typedef struct _GoStonesParameters	GoStonesParameters;

struct _PieceParameters {
  double	    ambiance_level;
  double	    diffusion_level;
  double	    highlight_level;
  double	    highlight_sharpness;

  QuarryColor	    color;
};

struct _LightParameters {
  double	    dx;
  double	    dy;
  int		    shadow_level;
};

struct _GoStonesParameters {
  /* Go-specific fields, describing shape of stones. */
  double	    relative_stone_size;
  double	    lense_radius;
  double	    ellipsoid_radius;

  LightParameters   light;
  PieceParameters   stones[NUM_COLORS];
};


/* FIXME: will need to be removed or replaced with game-specific
 *	  alternatives.
 */
extern const GoStonesParameters  go_stones_defaults;


void		 render_go_stones(int cell_size,
				  const GoStonesParameters *parameters,
				  unsigned char *black_pixel_data,
				  int black_row_stride,
				  unsigned char *white_pixel_data,
				  int white_row_stride,
				  int *stones_x_offset, int *stones_y_offset);


unsigned char *	 duplicate_and_adjust_alpha(int alpha_up, int alpha_down,
					    int image_size,
					    unsigned char *pixel_data,
					    int row_stride);

unsigned char *	 combine_pixels_diagonally(int image_size,
					   unsigned char *first_pixel_data,
					   unsigned char *second_pixel_data,
					   int row_stride);


#endif /* QUARRY_TILE_RENDERER_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
