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


#include "configuration.h"
#include "gui-utils.h"
#include "markup-theme-configuration.h"
#include "board.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* These will probably cause serious portability problems.  Will need
 * to make dependent on OS.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>


int
gui_utils_enumerate_themes (void)
{
  /* We don't need to initialize themes' configuration sections as
   * they are all repeatable.
   */

  DIR *markup_themes_directory = opendir (PACKAGE_DATA_DIR "/markup-themes");
  struct dirent *directory_entry;

  if (!markup_themes_directory) {
    /* FIXME: Add proper i18n at some point (and same below). */
    fprintf (stderr, "%s: fatal error: directory `%s' doesn't exist\n",
	     short_program_name, PACKAGE_DATA_DIR);
    fprintf (stderr, "%s: seems like you need to reinstall me\n",
	     short_program_name);

    return 0;
  }

  while ((directory_entry = readdir (markup_themes_directory)) != NULL) {
    if (strcmp (directory_entry->d_name, ".") != 0
	&& strcmp (directory_entry->d_name, "..") != 0) {
      char *theme_directory
	= utils_cat_strings (NULL,
			     PACKAGE_DATA_DIR "/markup-themes/",
			     directory_entry->d_name, NULL);
      struct stat file_statistics;

      if (stat (theme_directory, &file_statistics) == 0
	  && S_ISDIR (file_statistics.st_mode)) {
	char *theme_configuration_file = utils_cat_strings (NULL,
							    theme_directory,
							    "/theme.cfg",
							    NULL);

	configuration_read_from_file (markup_theme_configuration_sections,
				      NUM_MARKUP_THEME_CONFIGURATION_SECTIONS,
				      theme_configuration_file);

	if (markup_themes.last && markup_themes.last->directory == NULL) {
	  /* Configuration file has been read and contained a theme
	   * description.  We assume that SVG files are in place.
	   */
	  markup_themes.last->directory
	    = utils_duplicate_string (directory_entry->d_name);
	}

	utils_free (theme_configuration_file);
      }

      utils_free (theme_directory);
    }
  }

  closedir (markup_themes_directory);

  if (!string_list_find (&markup_themes, "Default")) {
    fprintf (stderr, "%s: fatal error: `Default' markup theme not found\n",
	     short_program_name);
    fprintf (stderr, "%s: seems like you need to reinstall me\n",
	     short_program_name);

    return 0;
  }

  return 1;
}


void
gui_utils_discard_theme_lists (void)
{
  configuration_dispose (markup_theme_configuration_sections,
			 NUM_MARKUP_THEME_CONFIGURATION_SECTIONS);
}



void
gui_utils_mark_variations_on_grid (char grid[BOARD_GRID_SIZE],
				   const Board *board,
				   int black_variations[BOARD_GRID_SIZE],
				   int white_variations[BOARD_GRID_SIZE],
				   char black_variations_mark,
				   char white_variations_mark,
				   char mixed_variations_mark)
{
  int x;
  int y;

  assert (grid);
  assert (board);
  assert (black_variations);
  assert (white_variations);

  for (y = 0; y < board->height; y++) {
    for (x = 0; x < board->width; x++) {
      int pos = POSITION (x, y);

      if (board->grid[pos] == EMPTY) {
	if (black_variations[pos] > 0) {
	  if (white_variations[pos] > 0)
	    grid[pos] = mixed_variations_mark;
	  else
	    grid[pos] = black_variations_mark;
	}
	else {
	  if (white_variations[pos] > 0)
	    grid[pos] = white_variations_mark;
	}
      }
    }
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
