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


#include "gtk-help.h"

#include <locale.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


void
gtk_help_display(const gchar *link_id)
{
  static gboolean yelp_works = TRUE;
  static gchar locale_directory[16] = "";
  gchar *child_argv[3];

  if (! *locale_directory) {
    /* Get the current locale and find help directory for it. */
    const char *current_locale = setlocale(LC_MESSAGES, NULL);
    const char *current_locale_end = current_locale + strlen(current_locale);
    char *pointer;

    /* First remove anything after a dot or at-sign. */
    pointer = strchr(current_locale, '@');
    if (pointer)
      current_locale_end = pointer;

    pointer = strchr(current_locale, '.');
    if (pointer && pointer < current_locale_end)
      current_locale_end = pointer;

    if (current_locale_end - current_locale < sizeof locale_directory) {
      gchar *full_path;
      struct stat dummy_info;

      memcpy(locale_directory, current_locale,
	     current_locale_end - current_locale);
      locale_directory[current_locale_end - current_locale] = 0;

      full_path = g_strconcat(PACKAGE_DATA_DIR "/help/", locale_directory,
			      "/quarry.xml", NULL);
      if (stat(full_path, &dummy_info)) {
	pointer = strchr(locale_directory, '_');
	if (pointer) {
	  /* Try removing part of locale name after '_'. */
	  *pointer = 0;

	  full_path = g_strconcat(PACKAGE_DATA_DIR "/help/", locale_directory,
				  "/quarry.xml", NULL);
	  if (stat(full_path, &dummy_info))
	    strcpy(locale_directory, "C");
	}
	else
	  strcpy(locale_directory, "C");
      }

      g_free(full_path);
    }
    else {
      /* A very weird long locale, if this can happen at all. */
      strcpy(locale_directory, "C");
    }
  }

  if (yelp_works) {
    child_argv[0] = "yelp";

    if (link_id) {
      child_argv[1] = g_strconcat(("ghelp://" PACKAGE_DATA_DIR
				   "/help/C/quarry.xml?"),
				  link_id, NULL);
    }
    else
      child_argv[1] = "ghelp://" PACKAGE_DATA_DIR "/help/C/quarry.xml";

    child_argv[2] = NULL;
    if (!g_spawn_async(NULL, child_argv, NULL, G_SPAWN_SEARCH_PATH,
		       NULL, NULL, NULL, NULL))
      yelp_works = FALSE;

    if (link_id)
      g_free(child_argv[1]);
  }

  if (!yelp_works) {
    /* FIXME: Make fallback browser configurable. */
    child_argv[0] = "mozilla";

    if (link_id) {
      child_argv[1] = g_strconcat(PACKAGE_DATA_DIR "/help/C/quarry.html#",
				  link_id, NULL);
    }
    else
      child_argv[1] = PACKAGE_DATA_DIR "/help/C/quarry.html";

    child_argv[2] = NULL;

    /* FIXME: If not works, pop up configuration dialog. */
    g_spawn_async(NULL, child_argv, NULL, G_SPAWN_SEARCH_PATH,
		  NULL, NULL, NULL, NULL);

    if (link_id)
      g_free(child_argv[1]);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
