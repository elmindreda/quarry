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


#include "tile-set.h"
#include "board.h"
#include "utils.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


/* The limit on number of unused tile sets to keep cached.  They can
 * be evenually deleted by tile_set_dump_recycle() (should be called
 * from time to time.
 */
#define DUMP_SIZE_LIMIT		20


typedef struct _TileSetListItem	TileSetListItem;

struct _TileSetListItem {
  TileSetListItem   *next;

  int		     reference_count;

  int		     cell_size;
  TileSetParameters  parameters;

  int		     is_complete;
  TileSet	     tile_set;
};


#define SQR(x)		((x) * (x))


typedef struct _Pixel	Pixel;

struct _Pixel {
  unsigned char  red;
  unsigned char  green;
  unsigned char  blue;
  unsigned char  opacity;
};


#define PIXEL(center, rowstride, x, y)					\
  ((Pixel *) (((unsigned char *) (center))				\
	      + (y) * (rowstride) + (x) * sizeof(Pixel)))


static void	create_go_stone_images(int cell_size,
				       unsigned char *pixel_data[NUM_TILES],
				       int *stones_x_offset,
				       int *stones_y_offset,
				       const TileSetParameters *parameters);


const TileSetParameters tile_set_defaults = {
  0.91,
  4.0, 4.0,
  -0.3, -0.4, 80,
  { { 0.05, 0.55, 0.4, 8.0,
      120, 120, 120 },
    { 0.2, 0.6, 0.2, 10.0,
      255, 240, 220 } }
};


static TileSetListItem *first_tile_set_stock_item = NULL;
static TileSetListItem *first_tile_set_dump_item = NULL;
static int dump_size = 0;


TileSet *
tile_set_create_or_reuse(int cell_size, const TileSetParameters *parameters)
{
  TileSetListItem *stock_item;
  TileSetListItem **link;
  int k;
  unsigned char *pixel_data[NUM_TILES];

  assert(parameters);

  /* Check if we have required tile set in stock. */
  for (stock_item = first_tile_set_stock_item;
       stock_item; stock_item = stock_item->next) {
    if (stock_item->cell_size == cell_size
	&& !memcmp(&stock_item->parameters, parameters,
		   sizeof(TileSetParameters))) {
      stock_item->reference_count++;
      return &stock_item->tile_set;
    }
  }

  /* Maybe such tile set is present in the dump, pending for deletion?
   * Then we move it to stock and avoid recreating it.
   */
  link = &first_tile_set_dump_item;
  while (*link) {
    TileSetListItem *dump_item = *link;

    if (dump_item->cell_size == cell_size
	&& !memcmp(&dump_item->parameters, parameters,
		   sizeof(TileSetParameters))) {
      *link = dump_item->next;

      dump_item->next = first_tile_set_stock_item;
      first_tile_set_stock_item = dump_item;

      dump_item->reference_count = 1;
      dump_size--;

      return &dump_item->tile_set;
    }

    link = &dump_item->next;
  }

  stock_item = utils_malloc(sizeof(TileSetListItem));

  stock_item->next = first_tile_set_stock_item;
  stock_item->cell_size = cell_size;
  memcpy(&stock_item->parameters, parameters, sizeof(TileSetParameters));
  stock_item->reference_count = 1;

  first_tile_set_stock_item = stock_item;

  for (k = 0; k < NUM_TILES; k++) {
    if (k == TILE_NONE || k == TILE_SPECIAL)
      continue;

    pixel_data[k] = utils_malloc(cell_size * cell_size * 4
				 * sizeof(unsigned char));
    stock_item->tile_set.tiles[k]
      = gui_back_end_image_new(pixel_data[k], cell_size, cell_size);
  }

  stock_item->tile_set.cell_size = cell_size;
  create_go_stone_images(cell_size, pixel_data,
			 &stock_item->tile_set.stones_x_offset,
			 &stock_item->tile_set.stones_y_offset,
			 parameters);

  return &stock_item->tile_set;
}


void
tile_set_unreference(TileSet *tile_set)
{
  TileSetListItem **link = &first_tile_set_stock_item;

  assert(tile_set);

  while (*link) {
    TileSetListItem *stock_item = *link;

    if (&stock_item->tile_set == tile_set) {
      stock_item->reference_count--;
      if (stock_item->reference_count == 0) {
	*link = stock_item->next;

	stock_item->next = first_tile_set_dump_item;
	first_tile_set_dump_item = stock_item;

	dump_size++;
	if (dump_size > DUMP_SIZE_LIMIT)
	  tile_set_dump_recycle(1);
      }

      return;
    }

    link = &stock_item->next;
  }

  assert(0);
}


/* Recycle tile set dump.  On first recycle, tile set's reference
 * count is set to -1.  On second recycle it is deleted.  If
 * `lazy_recycling' is set, this function recycles only one tile set.
 * It should be set if the dump overflows.
 */
void
tile_set_dump_recycle(int lazy_recycling)
{
  TileSetListItem **link = &first_tile_set_dump_item;

  while (*link) {
    TileSetListItem *dump_item = *link;

    if (dump_item->reference_count == 0 && !lazy_recycling)
      dump_item->reference_count = -1;
    else if (dump_item->reference_count == -1
	     || (lazy_recycling && !dump_item->next)) {
      int k;

      for (k = 0; k < NUM_TILES; k++) {
	if (k == TILE_NONE || k == TILE_SPECIAL)
	  continue;

	gui_back_end_image_delete(dump_item->tile_set.tiles[k]);
      }

      *link = dump_item->next;

      utils_free(dump_item);
      dump_size--;

      if (lazy_recycling)
	return;

      continue;
    }

    link = &dump_item->next;
  }
}


/* FIXME: replace with better antialising. */

#define SUPERSAMPLING_FACTOR	4

static void
create_go_stone_images(int cell_size, unsigned char *pixel_data[NUM_TILES],
		       int *stones_x_offset, int *stones_y_offset,
		       const TileSetParameters *parameters)
{
  double stone_radius = 0.5 * parameters->relative_stone_size * cell_size;
  int stone_radius_in_pixels = ceil(stone_radius);
  int center_x = (parameters->light_x < 0.0 ? stone_radius_in_pixels
		  : cell_size - stone_radius_in_pixels);
  int center_y = (parameters->light_y < 0.0 ? stone_radius_in_pixels
		  : cell_size - stone_radius_in_pixels);

  double squared_lense_radius = SQR(parameters->lense_radius);
  double stone_half_height = (parameters->lense_radius
			      - sqrt(squared_lense_radius - 1.0)
			      + 1.0 / parameters->ellipsoid_radius);

  double light_vector_length = sqrt(SQR(parameters->light_x)
				    + SQR(parameters->light_y) + 1.0);
  double light_x = parameters->light_x / light_vector_length;
  double light_y = parameters->light_y / light_vector_length;
  double light_z = 1.0 / light_vector_length;

  double shadow_center_dx = stone_half_height * parameters->light_x;
  double shadow_center_dy = stone_half_height * parameters->light_y;

  double black_ambiance_level
    = (255 * parameters->stone[BLACK_INDEX].ambiance_level);
  double black_diffusion_level
    = (255 * parameters->stone[BLACK_INDEX].diffusion_level);
  double black_highlight_level
    = (255 * parameters->stone[BLACK_INDEX].highlight_level);

  double white_ambiance_level
    = (255 * parameters->stone[WHITE_INDEX].ambiance_level);
  double white_diffusion_level
    = (255 * parameters->stone[WHITE_INDEX].diffusion_level);
  double white_highlight_level
    = (255 * parameters->stone[WHITE_INDEX].highlight_level);

  int x;
  int y;

  for (y = 0; y < cell_size; y++) {
    for (x = 0; x < cell_size; x++) {
      int k;
      int x_subpixel;
      int y_subpixel;
      double black_intensity = 0.0;
      double white_intensity = 0.0;
      int opacity = 0;

      for (y_subpixel = 1;
	   y_subpixel < 2 * SUPERSAMPLING_FACTOR; y_subpixel += 2) {
	double dy = (((y - center_y)
		      + y_subpixel / (double) (2 * SUPERSAMPLING_FACTOR))
		     / stone_radius);

	for (x_subpixel = 1;
	     x_subpixel < 2 * SUPERSAMPLING_FACTOR; x_subpixel += 2) {
	  double dx = (((x - center_x)
			+ x_subpixel / (double) (2 * SUPERSAMPLING_FACTOR))
		       / stone_radius);
	  double squared_r = SQR(dx) + SQR(dy);

	  if (squared_r >= 1.0) {
	    if (SQR(dx + shadow_center_dx) + SQR(dy + shadow_center_dy) <= 1.0)
	      opacity += parameters->shadow_level;

	    continue;
	  }
	  else {
	    double A = sqrt(squared_lense_radius - squared_r);
	    double B = parameters->ellipsoid_radius * sqrt(1.0 - squared_r);
	    double dz = (A * B) / (A + B);
	    double normal_length = sqrt(squared_r + SQR(dz));
	    double cos_alpha = ((dx * light_x + dy * light_y + dz * light_z)
				/ normal_length);

	    if (cos_alpha > 0.0) {
	      double cos_beta = (2.0 * ((dz / normal_length) * cos_alpha)
				 - light_z);

	      if (cos_beta > 0.0) {
		black_intensity
		  += (black_highlight_level
		      * pow(cos_beta,
			    parameters->stone[0].highlight_sharpness));
		white_intensity
		  += (white_highlight_level
		      * pow(cos_beta,
			    parameters->stone[1].highlight_sharpness));
	      }

	      black_intensity += (black_ambiance_level
				  + black_diffusion_level * cos_alpha);
	      white_intensity += (white_ambiance_level
				  + white_diffusion_level * cos_alpha);
	    }
	  }

	  opacity += 255;
	}
      }

      if (opacity) {
	black_intensity /= opacity;
	white_intensity /= opacity;
	opacity /= SUPERSAMPLING_FACTOR * SUPERSAMPLING_FACTOR;

	for (k = 0; k < NUM_COLORS; k++) {
	  double intensity = (k == BLACK_INDEX
			      ? black_intensity : white_intensity);
	  int red = parameters->stone[k].red * intensity;
	  int green = parameters->stone[k].green * intensity;
	  int blue = parameters->stone[k].blue * intensity;

	  if (red > 255)
	    red = 255;
	  if (green > 255)
	    green = 255;
	  if (blue > 255)
	    blue = 255;

	  *pixel_data[k + STONE_OPAQUE]++ = red;
	  *pixel_data[k + STONE_OPAQUE]++ = green;
	  *pixel_data[k + STONE_OPAQUE]++ = blue;
	  *pixel_data[k + STONE_OPAQUE]++ = opacity;

	  *pixel_data[k + STONE_25_TRANSPARENT]++ = red;
	  *pixel_data[k + STONE_25_TRANSPARENT]++ = green;
	  *pixel_data[k + STONE_25_TRANSPARENT]++ = blue;
	  *pixel_data[k + STONE_25_TRANSPARENT]++ = (3 * opacity) / 4;

	  *pixel_data[k + STONE_50_TRANSPARENT]++ = red;
	  *pixel_data[k + STONE_50_TRANSPARENT]++ = green;
	  *pixel_data[k + STONE_50_TRANSPARENT]++ = blue;
	  *pixel_data[k + STONE_50_TRANSPARENT]++ = opacity / 2;

	  if ((k == WHITE_INDEX && x + y < cell_size)
	      || (k == BLACK_INDEX && x + y >= cell_size)) {
	    *pixel_data[MIXED_50_TRANSPARENT]++ = red;
	    *pixel_data[MIXED_50_TRANSPARENT]++ = green;
	    *pixel_data[MIXED_50_TRANSPARENT]++ = blue;
	    *pixel_data[MIXED_50_TRANSPARENT]++ = opacity / 2;
	  }
	}
      }
      else {
	for (k = 0; k < NUM_TILES; k++) {
	  if (k == TILE_NONE || k == TILE_SPECIAL)
	    continue;

	  *pixel_data[k]++ = 0;
	  *pixel_data[k]++ = 0;
	  *pixel_data[k]++ = 0;
	  *pixel_data[k]++ = 0;
	}
      }
    }
  }

  *stones_x_offset = -center_x;
  *stones_y_offset = -center_y;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
