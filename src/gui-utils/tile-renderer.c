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
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,     *
 * Boston, MA 02110-1301, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include "tile-renderer.h"
#include "board.h"
#include "utils.h"

#include <assert.h>
#include <math.h>

#if HAVE_MEMORY_H
#include <memory.h>
#endif


#define SQR(x)		((x) * (x))


const GoStonesParameters	go_stones_defaults = {
  0.91, 4.0, 4.0,
  { -0.3, -0.4, 80 },
  { { 0.05, 0.55, 0.4, 8.0,
      { 120, 120, 120 } },
    { 0.2, 0.6, 0.2, 10.0,
      { 255, 240, 220 } } }
};

const OthelloDisksParameters	othello_disks_defaults = {
  0.91, 0.3, 0.25,
  { -0.3, -0.4, 80 },
  { { 0.05, 0.55, 0.4, 8.0,
      { 120, 120, 120 } },
    { 0.2, 0.6, 0.2, 10.0,
      { 255, 250, 235 } } }
};


/* FIXME: replace with better antialising. */

#define SUPERSAMPLING_FACTOR	4


void
render_go_stones (int cell_size, const GoStonesParameters *parameters,
		  unsigned char *black_pixel_data, int black_row_stride,
		  unsigned char *white_pixel_data, int white_row_stride,
		  int *stones_x_offset, int *stones_y_offset)
{
  double stone_radius = 0.5 * parameters->relative_stone_size * cell_size;
  double stone_radius_in_pixels = (cell_size % 2 == 1
				   ? ceil (stone_radius - 0.5) + 0.5
				   : ceil (stone_radius));
  double center_x = (parameters->light.dx < 0.0 ? stone_radius_in_pixels
		     : cell_size - stone_radius_in_pixels);
  double center_y = (parameters->light.dy < 0.0 ? stone_radius_in_pixels
		     : cell_size - stone_radius_in_pixels);

  double squared_lense_radius = SQR (parameters->lense_radius);
  double stone_half_height = (parameters->lense_radius
			      - sqrt (squared_lense_radius - 1.0)
			      + 1.0 / parameters->ellipsoid_radius);

  double light_vector_length = sqrt (SQR (parameters->light.dx)
				     + SQR (parameters->light.dy) + 1.0);
  double light_dx = parameters->light.dx / light_vector_length;
  double light_dy = parameters->light.dy / light_vector_length;
  double light_dz = 1.0 / light_vector_length;

  double shadow_center_dx = stone_half_height * parameters->light.dx;
  double shadow_center_dy = stone_half_height * parameters->light.dy;

  double black_ambiance_level
    = 255 * parameters->stones[BLACK_INDEX].ambiance_level;
  double black_diffusion_level
    = 255 * parameters->stones[BLACK_INDEX].diffusion_level;
  double black_highlight_level
    = 255 * parameters->stones[BLACK_INDEX].highlight_level;

  double white_ambiance_level
    = 255 * parameters->stones[WHITE_INDEX].ambiance_level;
  double white_diffusion_level
    = 255 * parameters->stones[WHITE_INDEX].diffusion_level;
  double white_highlight_level
    = 255 * parameters->stones[WHITE_INDEX].highlight_level;

  int x;
  int y;

  for (y = 0; y < cell_size; y++) {
    for (x = 0; x < cell_size; x++) {
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
	  double squared_r = SQR (dx) + SQR (dy);

	  if (squared_r >= 1.0) {
	    if (SQR (dx + shadow_center_dx) + SQR (dy + shadow_center_dy)
		<= 1.0)
	      opacity += parameters->light.shadow_level;

	    continue;
	  }
	  else {
	    double A = sqrt (squared_lense_radius - squared_r);
	    double B = parameters->ellipsoid_radius * sqrt (1.0 - squared_r);
	    double dz = (A * B) / (A + B);
	    double normal_length = sqrt (squared_r + SQR (dz));
	    double cos_alpha = ((dx * light_dx + dy * light_dy + dz * light_dz)
				/ normal_length);

	    if (cos_alpha > 0.0) {
	      double cos_beta = (2.0 * ((dz / normal_length) * cos_alpha)
				 - light_dz);

	      if (cos_beta > 0.0) {
		black_intensity
		  += (black_highlight_level
		      * pow (cos_beta,
			     (parameters
			      ->stones[BLACK_INDEX].highlight_sharpness)));
		white_intensity
		  += (white_highlight_level
		      * pow (cos_beta,
			     (parameters
			      ->stones[WHITE_INDEX].highlight_sharpness)));
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
	opacity /= SQR (SUPERSAMPLING_FACTOR);

	*black_pixel_data++ = MIN ((parameters->stones[BLACK_INDEX].color.red
				    * black_intensity),
				   255);
	*black_pixel_data++ = MIN ((parameters->stones[BLACK_INDEX].color.green
				    * black_intensity),
				   255);
	*black_pixel_data++ = MIN ((parameters->stones[BLACK_INDEX].color.blue
				    * black_intensity),
				   255);
	*black_pixel_data++ = opacity;

	*white_pixel_data++ = MIN ((parameters->stones[WHITE_INDEX].color.red
				    * white_intensity),
				   255);
	*white_pixel_data++ = MIN ((parameters->stones[WHITE_INDEX].color.green
				    * white_intensity),
				   255);
	*white_pixel_data++ = MIN ((parameters->stones[WHITE_INDEX].color.blue
				    * white_intensity),
				   255);
	*white_pixel_data++ = opacity;
      }
      else {
	*black_pixel_data++ = 0;
	*black_pixel_data++ = 0;
	*black_pixel_data++ = 0;
	*black_pixel_data++ = 0;

	*white_pixel_data++ = 0;
	*white_pixel_data++ = 0;
	*white_pixel_data++ = 0;
	*white_pixel_data++ = 0;
      }
    }

    black_pixel_data += black_row_stride - 4 * cell_size;
    white_pixel_data += white_row_stride - 4 * cell_size;
  }

  *stones_x_offset = - (int) (center_x - 0.5);
  *stones_y_offset = - (int) (center_y - 0.5);
}


void
render_othello_disks (int cell_size, const OthelloDisksParameters *parameters,
		      unsigned char *black_pixel_data, int black_row_stride,
		      unsigned char *white_pixel_data, int white_row_stride,
		      int *disks_x_offset, int *disks_y_offset)
{
  double disk_radius = 0.5 * parameters->relative_disk_size * cell_size;
  double disk_radius_in_pixels = (cell_size % 2 == 1
				  ? ceil (disk_radius - 0.5) + 0.5
				  : ceil (disk_radius));
  double disk_flat_part_radius = 1.0 - parameters->border_curve_size;
  double center_x = (parameters->light.dx < 0.0 ? disk_radius_in_pixels
		     : cell_size - disk_radius_in_pixels);
  double center_y = (parameters->light.dy < 0.0 ? disk_radius_in_pixels
		     : cell_size - disk_radius_in_pixels);

  double light_vector_length = sqrt (SQR (parameters->light.dx)
				     + SQR (parameters->light.dy) + 1.0);
  double light_dx = parameters->light.dx / light_vector_length;
  double light_dy = parameters->light.dy / light_vector_length;
  double light_dz = 1.0 / light_vector_length;

  double shadow_center_dx = (parameters->height_to_diameter_ratio
			     * parameters->light.dx);
  double shadow_center_dy = (parameters->height_to_diameter_ratio
			     * parameters->light.dy);

  double black_ambiance_level
    = 255 * parameters->disks[BLACK_INDEX].ambiance_level;
  double black_diffusion_level
    = 255 * parameters->disks[BLACK_INDEX].diffusion_level;
  double black_highlight_level
    = 255 * parameters->disks[BLACK_INDEX].highlight_level;
  double black_flat_part_intensity
    = (black_ambiance_level + (black_diffusion_level * light_dz)
       + (black_highlight_level
	  * pow (light_dz,
		 parameters->disks[BLACK_INDEX].highlight_sharpness)));

  double white_ambiance_level
    = 255 * parameters->disks[WHITE_INDEX].ambiance_level;
  double white_diffusion_level
    = 255 * parameters->disks[WHITE_INDEX].diffusion_level;
  double white_highlight_level
    = 255 * parameters->disks[WHITE_INDEX].highlight_level;
  double white_flat_part_intensity
    = (white_ambiance_level + (white_diffusion_level * light_dz)
       + (white_highlight_level
	  * pow (light_dz,
		 parameters->disks[WHITE_INDEX].highlight_sharpness)));

  int x;
  int y;

  for (y = 0; y < cell_size; y++) {
    for (x = 0; x < cell_size; x++) {
      int x_subpixel;
      int y_subpixel;
      double black_intensity = 0.0;
      double white_intensity = 0.0;
      int opacity = 0;

      for (y_subpixel = 1;
	   y_subpixel < 2 * SUPERSAMPLING_FACTOR; y_subpixel += 2) {
	double dy = (((y - center_y)
		      + y_subpixel / (double) (2 * SUPERSAMPLING_FACTOR))
		     / disk_radius);

	for (x_subpixel = 1;
	     x_subpixel < 2 * SUPERSAMPLING_FACTOR; x_subpixel += 2) {
	  double dx = (((x - center_x)
			+ x_subpixel / (double) (2 * SUPERSAMPLING_FACTOR))
		       / disk_radius);
	  double squared_r = SQR (dx) + SQR (dy);

	  if (squared_r >= 1.0) {
	    if (SQR (dx + shadow_center_dx) + SQR (dy + shadow_center_dy)
		<= 1.0)
	      opacity += parameters->light.shadow_level;

	    continue;
	  }
	  else {
	    double r = sqrt (squared_r);

	    if (r <= disk_flat_part_radius) {
	      black_intensity += black_flat_part_intensity;
	      white_intensity += white_flat_part_intensity;
	    }
	    else {
	      double dr = r - disk_flat_part_radius;
	      double dx_scaled = dx * dr / r;
	      double dy_scaled = dy * dr / r;
	      double dz
		= (parameters->height_to_diameter_ratio
		   * sqrt (1.0 - SQR (dr / parameters->border_curve_size)));
	      double normal_length = sqrt (SQR (dr) + SQR (dz));
	      double cos_alpha = ((dx_scaled * light_dx + dy_scaled * light_dy
				   + dz * light_dz)
				  / normal_length);

	      if (cos_alpha > 0.0) {
		double cos_beta = (2.0 * ((dz / normal_length) * cos_alpha)
				   - light_dz);

		if (cos_beta > 0.0) {
		  black_intensity
		    += (black_highlight_level
			* pow (cos_beta,
			       (parameters
				->disks[BLACK_INDEX].highlight_sharpness)));
		  white_intensity
		    += (white_highlight_level
			* pow (cos_beta,
			       (parameters
				->disks[WHITE_INDEX].highlight_sharpness)));
		}

		black_intensity += (black_ambiance_level
				    + black_diffusion_level * cos_alpha);
		white_intensity += (white_ambiance_level
				    + white_diffusion_level * cos_alpha);
	      }
	    }
	  }

	  opacity += 255;
	}
      }

      if (opacity) {
	black_intensity /= opacity;
	white_intensity /= opacity;
	opacity /= SQR (SUPERSAMPLING_FACTOR);

	*black_pixel_data++ = MIN ((parameters->disks[BLACK_INDEX].color.red
				    * black_intensity),
				   255);
	*black_pixel_data++ = MIN ((parameters->disks[BLACK_INDEX].color.green
				    * black_intensity),
				   255);
	*black_pixel_data++ = MIN ((parameters->disks[BLACK_INDEX].color.blue
				    * black_intensity),
				   255);
	*black_pixel_data++ = opacity;

	*white_pixel_data++ = MIN ((parameters->disks[WHITE_INDEX].color.red
				    * white_intensity),
				   255);
	*white_pixel_data++ = MIN ((parameters->disks[WHITE_INDEX].color.green
				    * white_intensity),
				   255);
	*white_pixel_data++ = MIN ((parameters->disks[WHITE_INDEX].color.blue
				    * white_intensity),
				   255);
	*white_pixel_data++ = opacity;
      }
      else {
	*black_pixel_data++ = 0;
	*black_pixel_data++ = 0;
	*black_pixel_data++ = 0;
	*black_pixel_data++ = 0;

	*white_pixel_data++ = 0;
	*white_pixel_data++ = 0;
	*white_pixel_data++ = 0;
	*white_pixel_data++ = 0;
      }
    }

    black_pixel_data += black_row_stride - 4 * cell_size;
    white_pixel_data += white_row_stride - 4 * cell_size;
  }

  *disks_x_offset = - (int) (center_x - 0.5);
  *disks_y_offset = - (int) (center_y - 0.5);
}


/* Make a copy of given pixel data, but with changed alpha channel
 * (opacity).  Alpha value is first multiplied by `alpha_up' and then
 * divided by `alpha_down' (which must be larger).  So, this function
 * can only be used to make a more transparent copy of the pixels.
 */
unsigned char *
duplicate_and_adjust_alpha (int alpha_up, int alpha_down,
			    int width, int height,
			    unsigned char *pixel_data, int row_stride)
{
  unsigned char *new_pixel_data = utils_malloc (height * row_stride);
  unsigned char *scan;
  int x;
  int y;

  assert (0 < alpha_up && alpha_up < alpha_down);
  assert (width > 0 && height > 0);
  assert (pixel_data);
  assert (row_stride >= 4 * width);

  for (scan = new_pixel_data, y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      /* Simply copy color values. */
      *scan++ = *pixel_data++;
      *scan++ = *pixel_data++;
      *scan++ = *pixel_data++;

      /* And adjust alpha. */
      *scan++ = (alpha_up * ((int) *pixel_data++)) / alpha_down;
    }

    pixel_data += row_stride - 4 * width;
    scan       += row_stride - 4 * width;
  }

  return new_pixel_data;
}


/* Make a colored copy of given monochrome pixel data, possibly making
 * the copy more transparent in process.  The input should contain
 * only shades of gray.  All pixels in the output have the same
 * `color' and varying opacity with black in input converted to fully
 * transparent pixels and white---to fully opaque.  In addition,
 * opacity may be scaled down as in duplicate_and_adjust_alpha(), but
 * saturate_and_set_alpha() allows `alpha_up' equal to `alpha_down'.
 *
 * This function's main purpose is to replace background in
 * prerendered text with transparency, something that cannot be done
 * easily with X at present.
 */
unsigned char *
saturate_and_set_alpha (QuarryColor color, int alpha_up, int alpha_down,
			int width, int height,
			unsigned char *pixel_data, int row_stride)
{
  unsigned char *new_pixel_data = utils_malloc (height * row_stride);
  unsigned char *scan;
  int x;
  int y;

  assert (0 < alpha_up && alpha_up <= alpha_down);
  assert (width > 0 && height > 0);
  assert (pixel_data);
  assert (row_stride >= 4 * width);

  for (scan = new_pixel_data, y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      /* Fill the color bytes. */
      *scan++ = color.red;
      *scan++ = color.green;
      *scan++ = color.blue;

      /* Set and scale alpha channel.  We actually only use the Red
       * channel in input for this, but the caller shouldn't assume
       * this behavior.
       */
      *scan++	  = (alpha_up * ((int) *pixel_data)) / alpha_down;
      pixel_data += 4;
    }

    pixel_data += row_stride - 4 * width;
    scan       += row_stride - 4 * width;
  }

  return new_pixel_data;
}


/* Combine two square images into one: the first is placed in the
 * lower-right corner, the second---in the upper-left.
 */
unsigned char *
combine_pixels_diagonally (int image_size,
			   unsigned char *first_pixel_data,
			   unsigned char *second_pixel_data,
			   int row_stride)
{
  unsigned char *new_pixel_data = utils_malloc (image_size * row_stride);
  unsigned char *scan;
  int y;

  assert (image_size > 0);
  assert (first_pixel_data);
  assert (second_pixel_data);
  assert (row_stride >= 4 * image_size);

  for (scan = new_pixel_data, y = 0; y < image_size; y++) {
    int left_sub_line_size = 4 * (image_size - y - 1);

    /* FIXME: blend at the diagonal. */
    memcpy (scan, second_pixel_data, 4 * left_sub_line_size);
    memcpy (scan + left_sub_line_size, first_pixel_data + left_sub_line_size,
	    4 * (y + 1));

    first_pixel_data += row_stride;
    second_pixel_data += row_stride;
    scan += row_stride;
  }

  return new_pixel_data;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
