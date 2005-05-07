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


#include "sgf.h"
#include "utils.h"

#include <stdio.h>


int
main (int argc, char *argv[])
{
  int k;
  int result = 0;
  SgfCollection *collection;
  SgfErrorList *error_list;

  utils_remember_program_name (argv[0]);

  if (argc > 1) {
    for (k = 1; k < argc; k++) {
      switch (sgf_parse_file (argv[k], &collection, &error_list,
			      &sgf_parser_defaults, NULL, NULL, NULL)) {
      case SGF_PARSED:
	if (error_list) {
	  SgfErrorListItem *item;

	  printf ("%s:\n", argv[k]);
	  for (item = error_list->first; item; item = item->next)
	    printf ("%d(%d): %s\n", item->line, item->column, item->text);

	  string_list_delete (error_list);

	  putchar ('\n');

	  result = 1;
	}
#if 0
	else
	  printf ("File `%s' parsed without errors\n\n", argv[k]);
#endif

#if 0
	sgf_write_file (NULL, collection);
#endif

	sgf_collection_delete (collection);

	break;

      case SGF_ERROR_READING_FILE:
	printf ("Error reading file `%s'\n\n", argv[k]);
	result = 1;
	break;

      case SGF_INVALID_FILE:
	printf ("File `%s' doesn't seem to be an SGF file\n\n", argv[k]);
	result = 1;
	break;
      }
    }
  }
  else {
    fprintf (stderr, "Usage: %s INFILE ...\n", argv[0]);
    result = 255;
  }

  utils_free_program_name_strings ();

#if ENABLE_MEMORY_PROFILING
  utils_print_memory_profiling_info ();
#endif

  return result;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
