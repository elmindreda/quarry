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


#include "utils.h"
#include "gui-back-end.h"

#include <signal.h>


/* FIXME: This is currently just a stub. */
int
main(int argc, char *argv[])
{
  int return_code;

  utils_remember_program_name(argv[0]);

  if (!gui_back_end_init(&argc, &argv))
    return_code = 255;

  /* When a GTP engine crashes (or it is not a GTP engine in the first
   * place), we receive this signal and better not abort on it.
   */
  signal(SIGPIPE, SIG_IGN);

  if (argc == 1)
    return_code = gui_back_end_main_default();
  else
    return_code = gui_back_end_main_open_files(argc - 1, argv + 1);

  utils_free_program_name_strings();

#if ENABLE_MEMORY_PROFILING
  utils_print_memory_profiling_info();
#endif

  return return_code;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
