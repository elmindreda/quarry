/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004 Paul Pogonyshev.                             *
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


#include "gtk-configuration.h"
#include "gtk-preferences.h"
#include "markup-theme-configuration.h"
#include "tile-renderer.h"
#include "sgf.h"
#include "board.h"
#include "game-info.h"
#include "utils.h"
#include "gtk-tile-set.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <librsvg/rsvg.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#if HAVE_MEMORY_H
#include <memory.h>
#endif


typedef struct _GtkMainTileSetKey	GtkMainTileSetKey;
typedef struct _GtkSgfMarkupTileSetKey	GtkSgfMarkupTileSetKey;

struct _GtkMainTileSetKey {
  gint		 tile_size;
  Game		 game;
};

struct _GtkSgfMarkupTileSetKey {
  gint		 tile_size;

  const gchar	*theme_directory;
  gdouble	 scale;
  QuarryColor	 colors[NUM_SGF_MARKUP_BACKGROUNDS];
  gdouble	 opacity;
};


static int	    gtk_main_tile_set_compare_keys
		      (const GtkMainTileSetKey *first_key,
		       const GtkMainTileSetKey *second_key);
static void *	    gtk_main_tile_set_duplicate_key
		      (const GtkMainTileSetKey *key);
static void *	    gtk_main_tile_set_create(const GtkMainTileSetKey *key);
static void	    gtk_main_tile_set_delete(GtkMainTileSet *tile_set);

static int	    gtk_sgf_markup_tile_set_compare_keys
		      (const GtkSgfMarkupTileSetKey *first_key,
		       const GtkSgfMarkupTileSetKey *second_key);
static void *	    gtk_sgf_markup_tile_set_duplicate_key
		      (const GtkSgfMarkupTileSetKey *key);
static void *	    gtk_sgf_markup_tile_set_create
		      (const GtkSgfMarkupTileSetKey *key);
static void	    gtk_sgf_markup_tile_set_delete
		      (GtkSgfMarkupTileSet *tile_set);

static GdkPixbuf *  scale_and_paint_svg_image(char *buffer,
					      const char *buffer_end,
					      gint tile_size, gdouble scale,
					      QuarryColor color,
					      gdouble opacity);
static void	    set_pixbuf_size(gint *width, gint *height,
				    gpointer tile_size);


ObjectCache	gtk_main_tile_set_cache =
  { NULL, NULL, 0, 20,
    (ObjectCacheCompareKeys) gtk_main_tile_set_compare_keys,
    (ObjectCacheCreate) gtk_main_tile_set_duplicate_key,
    (ObjectCacheCreate) gtk_main_tile_set_create,
    (ObjectCacheDelete) g_free,
    (ObjectCacheDelete) gtk_main_tile_set_delete };

ObjectCache	gtk_sgf_markup_tile_set_cache =
  { NULL, NULL, 0, 10,
    (ObjectCacheCompareKeys) gtk_sgf_markup_tile_set_compare_keys,
    (ObjectCacheCreate) gtk_sgf_markup_tile_set_duplicate_key,
    (ObjectCacheCreate) gtk_sgf_markup_tile_set_create,
    (ObjectCacheDelete) g_free,
    (ObjectCacheDelete) gtk_sgf_markup_tile_set_delete };



GtkMainTileSet *
gtk_main_tile_set_create_or_reuse(gint tile_size, Game game)
{
  const GtkMainTileSetKey key
    = { (game == GAME_GO ? (tile_size - 1) | 1 : tile_size), game };

  assert(tile_size > 0);
  assert(GAME_IS_SUPPORTED(game));

  return ((GtkMainTileSet *)
	  object_cache_create_or_reuse_object(&gtk_main_tile_set_cache, &key));
}


static int
gtk_main_tile_set_compare_keys(const GtkMainTileSetKey *first_key,
			       const GtkMainTileSetKey *second_key)
{
  return (first_key->tile_size == second_key->tile_size
	  && first_key->game   == second_key->game);
}


static void *
gtk_main_tile_set_duplicate_key(const GtkMainTileSetKey *key)
{
  GtkMainTileSetKey *key_copy = g_malloc(sizeof(GtkMainTileSetKey));

  key_copy->tile_size = key->tile_size;
  key_copy->game = key->game;

  return key_copy;
}


static void *
gtk_main_tile_set_create(const GtkMainTileSetKey *key)
{
  GtkMainTileSet *tile_set = g_malloc(sizeof(GtkMainTileSet));
  gint tile_size = key->tile_size;
  unsigned char *black_pixel_data;
  unsigned char *white_pixel_data;
  int row_stride;
  unsigned char *pixel_data;
  unsigned char *black_50_transparent_pixel_data;
  unsigned char *white_50_transparent_pixel_data;

  tile_set->tile_size = tile_size;

  tile_set->tiles[BLACK_OPAQUE] = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
						 tile_size, tile_size);
  tile_set->tiles[WHITE_OPAQUE] = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
						 tile_size, tile_size);

  black_pixel_data = gdk_pixbuf_get_pixels(tile_set->tiles[BLACK_OPAQUE]);
  white_pixel_data = gdk_pixbuf_get_pixels(tile_set->tiles[WHITE_OPAQUE]);

  row_stride = gdk_pixbuf_get_rowstride(tile_set->tiles[BLACK_OPAQUE]);

  assert(gdk_pixbuf_get_rowstride(tile_set->tiles[WHITE_OPAQUE])
	 == row_stride);

  if (key->game != GAME_OTHELLO) {
    render_go_stones(tile_size, &go_stones_defaults,
		     black_pixel_data, row_stride,
		     white_pixel_data, row_stride,
		     &tile_set->stones_x_offset, &tile_set->stones_y_offset);
  }
  else {
    render_othello_disks(tile_size, &othello_disks_defaults,
			 black_pixel_data, row_stride,
			 white_pixel_data, row_stride,
			 &tile_set->stones_x_offset,
			 &tile_set->stones_y_offset);
  }

  pixel_data = duplicate_and_adjust_alpha(3, 4, tile_size,
					  black_pixel_data, row_stride);
  tile_set->tiles[BLACK_25_TRANSPARENT]
    = gdk_pixbuf_new_from_data(pixel_data, GDK_COLORSPACE_RGB, TRUE, 8,
			       tile_size, tile_size, row_stride,
			       (GdkPixbufDestroyNotify) utils_free, NULL);

  pixel_data = duplicate_and_adjust_alpha(3, 4, tile_size,
					  white_pixel_data, row_stride);
  tile_set->tiles[WHITE_25_TRANSPARENT]
    = gdk_pixbuf_new_from_data(pixel_data, GDK_COLORSPACE_RGB, TRUE, 8,
			       tile_size, tile_size, row_stride,
			       (GdkPixbufDestroyNotify) utils_free, NULL);

  black_50_transparent_pixel_data
    = duplicate_and_adjust_alpha(1, 2, tile_size,
				 black_pixel_data, row_stride);
  tile_set->tiles[BLACK_50_TRANSPARENT]
    = gdk_pixbuf_new_from_data(black_50_transparent_pixel_data,
			       GDK_COLORSPACE_RGB, TRUE, 8,
			       tile_size, tile_size, row_stride,
			       (GdkPixbufDestroyNotify) utils_free, NULL);

  white_50_transparent_pixel_data
    = duplicate_and_adjust_alpha(1, 2, tile_size,
				 white_pixel_data, row_stride);
  tile_set->tiles[WHITE_50_TRANSPARENT]
    = gdk_pixbuf_new_from_data(white_50_transparent_pixel_data,
			       GDK_COLORSPACE_RGB, TRUE, 8,
			       tile_size, tile_size, row_stride,
			       (GdkPixbufDestroyNotify) utils_free, NULL);

  pixel_data = combine_pixels_diagonally(tile_size,
					 black_50_transparent_pixel_data,
					 white_50_transparent_pixel_data,
					 row_stride);
  tile_set->tiles[MIXED_50_TRANSPARENT]
    = gdk_pixbuf_new_from_data(pixel_data, GDK_COLORSPACE_RGB, TRUE, 8,
			       tile_size, tile_size, row_stride,
			       (GdkPixbufDestroyNotify) utils_free, NULL);

  return tile_set;
}


static void
gtk_main_tile_set_delete(GtkMainTileSet *tile_set)
{
  int k;

  for (k = 0; k < NUM_TILES; k++) {
    if (k != TILE_NONE && k != TILE_SPECIAL)
      g_object_unref(tile_set->tiles[k]);
  }

  g_free(tile_set);
}



GtkSgfMarkupTileSet *
gtk_sgf_markup_tile_set_create_or_reuse(gint tile_size, Game game)
{
  BoardAppearance *board_appearance = game_to_board_appearance_structure(game);
  GtkSgfMarkupTileSetKey key;
  MarkupThemeListItem *theme_item = NULL;
  int k;

  if (board_appearance->markup_theme) {
    theme_item = markup_theme_list_find(&markup_themes,
					board_appearance->markup_theme);
  }

  if (!theme_item)
    theme_item = markup_theme_list_find(&markup_themes, "Default");

  assert(theme_item);

  key.tile_size	      = tile_size;
  key.theme_directory = theme_item->directory;
  key.scale	      = CLAMP(board_appearance->markup_size, 0.2, 1.0);
  key.opacity	      = CLAMP(board_appearance->markup_opacity, 0.2, 1.0);

  /* FIXME */
  if (board_appearance->markup_size_is_relative)
    key.scale *= 0.91;

  for (k = 0; k < NUM_SGF_MARKUP_BACKGROUNDS; k++)
    key.colors[k] = board_appearance->markup_colors[k];

  /* For Go, markup can only be properly centered if it is
   * odd-sized.
   */
  if (game == GAME_GO && tile_size % 2 == 0) {
    key.tile_size--;
    key.scale = MIN(1.0, (key.scale * tile_size) / (tile_size - 1));
  }

  return ((GtkSgfMarkupTileSet *)
	  object_cache_create_or_reuse_object(&gtk_sgf_markup_tile_set_cache,
					      &key));
}


static int
gtk_sgf_markup_tile_set_compare_keys(const GtkSgfMarkupTileSetKey *first_key,
				     const GtkSgfMarkupTileSetKey *second_key)
{
  int k;

  if (first_key->tile_size  != second_key->tile_size
      || strcmp(first_key->theme_directory, second_key->theme_directory) != 0
      || first_key->scale   != second_key->scale
      || first_key->opacity != second_key->opacity)
    return 0;

  for (k = 0; k < NUM_SGF_MARKUP_BACKGROUNDS; k++) {
    if (!QUARRY_COLORS_ARE_EQUAL(first_key->colors[k], second_key->colors[k]))
      return 0;
  }

  return 1;
}


static void *
gtk_sgf_markup_tile_set_duplicate_key(const GtkSgfMarkupTileSetKey *key)
{
  GtkSgfMarkupTileSetKey *key_copy = g_malloc(sizeof(GtkSgfMarkupTileSetKey));
  int k;

  key_copy->tile_size	    = key->tile_size;
  key_copy->theme_directory = key->theme_directory;
  key_copy->scale	    = key->scale;

  for (k = 0; k < NUM_SGF_MARKUP_BACKGROUNDS; k++)
    key_copy->colors[k] = key->colors[k];

  key_copy->opacity = key->opacity;

  return key_copy;
}


static void *
gtk_sgf_markup_tile_set_create(const GtkSgfMarkupTileSetKey *key)
{
  static const gchar *svg_file_base_names[NUM_SGF_MARKUPS]
    = { "cross", "circle", "square", "triangle", "selected" };

  GtkSgfMarkupTileSet *tile_set = g_malloc(sizeof(GtkSgfMarkupTileSet));
  int k;

  tile_set->tile_size = key->tile_size;

  for (k = 0; k < NUM_SGF_MARKUPS; k++) {
    gchar *filename = g_strdup_printf((PACKAGE_DATA_DIR
				       "/markup-themes/%s/%s.svg"),
				      key->theme_directory,
				      svg_file_base_names[k]);
    FILE *file = fopen(filename, "rb");
    int i;

    if (file && fseek(file, 0, SEEK_END) != -1) {
      int file_size = ftell(file);
      char *buffer = g_malloc(file_size);

      rewind(file);
      assert(fread(buffer, file_size, 1, file) == 1);
      fclose(file);

      for (i = 0; i < NUM_SGF_MARKUP_BACKGROUNDS; i++) {
	int j;

	for (j = 0; j < i; j++) {
	  if (QUARRY_COLORS_ARE_EQUAL(key->colors[j], key->colors[i]))
	    break;
	}

	if (j == i) {
	  tile_set->tiles[k][i] = scale_and_paint_svg_image(buffer,
							    buffer + file_size,
							    key->tile_size,
							    key->scale,
							    key->colors[i],
							    key->opacity);
	}
	else {
	  /* Reuse tile and add reference. */
	  tile_set->tiles[k][i] = tile_set->tiles[k][j];
	  g_object_ref(tile_set->tiles[k][i]);
	}
      }

      g_free(buffer);
    }
    else {
      /* GDK will print some warnings to stderr. */
      for (i = 0; i < NUM_SGF_MARKUP_BACKGROUNDS; i++)
	tile_set->tiles[k][i] = NULL;
    }

    g_free(filename);
  }

  return tile_set;
}


static void
gtk_sgf_markup_tile_set_delete(GtkSgfMarkupTileSet *tile_set)
{
  int k;
  int i;

  for (k = 0; k < NUM_SGF_MARKUPS; k++) {
    for (i = 0; i < NUM_SGF_MARKUP_BACKGROUNDS; i++)
      g_object_unref(tile_set->tiles[k][i]);
  }

  g_free(tile_set);
}


static GdkPixbuf *
scale_and_paint_svg_image(char *buffer, const char *buffer_end,
			  gint tile_size,
			  gdouble scale, QuarryColor color, gdouble opacity)
{
  GdkPixbuf *pixbuf;
  const char *written_up_to;
  char *scan;
  RsvgHandle *rsvg_handle = rsvg_handle_new();
  char color_string[8];
  char *scale_string = utils_cprintf(" scale(%.f)", scale);
  char *opacity_string = utils_cprintf("%.f", opacity);

  sprintf(color_string, "#%02x%02x%02x", color.red, color.green, color.blue);

  rsvg_handle_set_size_callback(rsvg_handle, set_pixbuf_size,
				GINT_TO_POINTER(tile_size), NULL);

  for (scan = buffer, written_up_to = buffer;
       (scan < buffer_end
	  && (scan = memchr(scan, '<', buffer_end - scan)) != NULL); ) {
    scan++;
    if (scan < buffer_end - 12 && memcmp(scan, "!-- [Quarry]", 12) == 0) {
      int scale_this_tag = 0;
      int blend_this_tag = 0;
      StringList color_properties = STATIC_STRING_LIST;

      scan += 12;
      while (1) {
	const char *keyword;

	while (scan < buffer_end
	       && (*scan == ' ' || *scan == '\t'
		   || *scan == '\n' || *scan == '\r'))
	  scan++;

	keyword = scan;
	while (scan < buffer_end
	       && *scan != ' ' && *scan != '\t'
	       && *scan != '\n' && *scan != '\r' && *scan != '>')
	  scan++;

	if (scan < buffer_end && *scan != '>') {
	  if (scan - keyword == 5 && memcmp(keyword, "scale", 5) == 0)
	    scale_this_tag = 1;
	  else if (scan - keyword == 5 && memcmp(keyword, "blend", 5) == 0)
	    blend_this_tag = 1;
	  else {
	    string_list_add_from_buffer(&color_properties,
					keyword, scan - keyword);
	  }
	}
	else
	  break;
      }

      scan = memchr(scan, '<', buffer_end - scan);
      while (scan < buffer_end
	     && *scan != ' ' && *scan != '\t'
	     && *scan != '\n' && *scan != '\r'
	     && *scan != '>' && *scan != '/')
	scan++;

      while (1) {
	const char *property_name;

	while (scan < buffer_end
	       && (*scan == ' ' || *scan == '\t'
		   || *scan == '\n' || *scan == '\r'))
	  scan++;

	property_name = scan;
	while (scan < buffer_end
	       && *scan != ' ' && *scan != '\t'
	       && *scan != '\n' && *scan != '\r'
	       && *scan != '=' && *scan != '/' && *scan != '>')
	  scan++;

	if (scan < buffer_end - 1 && *scan == '=' && *(scan + 1) == '"') {
	  if (scale_this_tag && scan - property_name == 9
	      && memcmp(property_name, "transform", 9) == 0) {
	    scan += 2;
	    while (scan < buffer_end && *scan != '"')
	      scan++;

	    assert(rsvg_handle_write(rsvg_handle,
				     written_up_to, scan - written_up_to,
				     NULL));
	    written_up_to = scan;

	    assert(rsvg_handle_write(rsvg_handle, scale_string,
				     strlen(scale_string), NULL));

	    scale_this_tag = 0;
	  }
	  else if (blend_this_tag && scan - property_name == 7
		   && memcmp(property_name, "opacity", 7) == 0) {
	    scan += 2;
	    assert(rsvg_handle_write(rsvg_handle,
				     written_up_to, scan - written_up_to,
				     NULL));

	    while (scan < buffer_end && *scan != '"')
	      scan++;
	    written_up_to = scan;

	    assert(rsvg_handle_write(rsvg_handle, opacity_string,
				     strlen(opacity_string), NULL));

	    blend_this_tag = 0;
	  }
	  else {
	    StringListItem *color_property;

	    *scan = 0;
	    color_property = string_list_find(&color_properties,
					      property_name);
	    *scan = '=';
	    scan += 2;

	    if (color_property) {
	      string_list_delete_item(&color_properties, color_property);

	      assert(rsvg_handle_write(rsvg_handle,
				       written_up_to, scan - written_up_to,
				       NULL));

	      assert(rsvg_handle_write(rsvg_handle, color_string, 7, NULL));

	      while (scan < buffer_end && *scan != '"')
		scan++;
	      written_up_to = scan;
	    }
	  }

	  while (scan < buffer_end && *scan != '"')
	    scan++;
	  if (scan < buffer_end)
	    scan++;
	}
	else {
	  if (scan < buffer_end
	      && (*scan == '>' || *scan == '/')
	      && (scale_this_tag || blend_this_tag)) {
	    assert(rsvg_handle_write(rsvg_handle,
				     written_up_to, scan - written_up_to,
				     NULL));
	    written_up_to = scan;

	    if (scale_this_tag) {
	      char *scale_full_string = g_strdup_printf(" transform=\"%s\"",
							scale_string + 1);

	      assert(rsvg_handle_write(rsvg_handle,
				       scale_full_string,
				       strlen(scale_full_string),
				       NULL));

	      g_free(scale_full_string);
	    }

	    if (blend_this_tag) {
	      char *opacity_full_string = g_strdup_printf(" opacity=\"%s\"",
							  opacity_string);

	      assert(rsvg_handle_write(rsvg_handle,
				       opacity_full_string,
				       strlen(opacity_full_string),
				       NULL));

	      g_free(opacity_full_string);
	    }
	  }

	  break;
	}
      }

      string_list_empty(&color_properties);
    }
  }

  assert(rsvg_handle_write(rsvg_handle,
			   written_up_to, buffer_end - written_up_to, NULL));

  assert(rsvg_handle_close(rsvg_handle, NULL));

  pixbuf = rsvg_handle_get_pixbuf(rsvg_handle);

  rsvg_handle_free(rsvg_handle);

  utils_free(scale_string);
  utils_free(opacity_string);

  return pixbuf;
}


static void
set_pixbuf_size(gint *width, gint *height, gpointer tile_size)
{
  *width = GPOINTER_TO_INT(tile_size);
  *height = GPOINTER_TO_INT(tile_size);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
