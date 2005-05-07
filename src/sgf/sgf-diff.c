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
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,     *
 * Boston, MA 02110-1301, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include "sgf.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>


int
main (int argc, char *argv[])
{
  int result = 0;
  SgfErrorList *error_list;

  utils_remember_program_name (argv[0]);

  if (argc == 3) {
    SgfCollection *first_collection;
    SgfCollection *second_collection;
    SgfCollection *difference;

    if (sgf_parse_file (argv[1], &first_collection, &error_list,
			&sgf_parser_defaults, NULL, NULL, NULL)
	!= SGF_PARSED)
      assert (0);

    if (error_list)
      string_list_delete (error_list);

    if (sgf_parse_file (argv[2], &second_collection, &error_list,
			&sgf_parser_defaults, NULL, NULL, NULL)
	!= SGF_PARSED)
      assert (0);

    if (error_list)
      string_list_delete (error_list);


    difference = sgf_diff (first_collection, second_collection);

    sgf_collection_delete (first_collection);
    sgf_collection_delete (second_collection);

    if (difference) {
      sgf_write_file (NULL, difference, 1);
      sgf_collection_delete (difference);
    }
  }
  else {
    fprintf (stderr, "Usage: %s FROM-FILE TO-FILE\n", argv[0]);
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
