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


#include "gui-back-end.h"
#include "gui-utils.h"
#include "utils.h"
#include "quarry-main.h"

#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>


enum {
  /* Options with no short equivalent. */
  OPTION_GTP_LOG = UCHAR_MAX + 1,
  OPTION_GTP_SHOW_STDERR,

  OPTION_HELP,
  OPTION_USAGE,
  OPTION_VERSION
};


static const struct option quarry_options[] = {
  { "gtp-log",		required_argument, NULL, OPTION_GTP_LOG		},
  { "gtp-show-stderr",	no_argument,	   NULL, OPTION_GTP_SHOW_STDERR },

  { "help",		no_argument,	   NULL, OPTION_HELP		},
  { "usage",		no_argument,	   NULL, OPTION_USAGE		},
  { "version",		no_argument,	   NULL, OPTION_VERSION		},

  {  NULL,		no_argument,	   NULL, 0			}
};

static const char *usage_string =
  "Usage: %s [OPTION...] [GAME-RECORD-FILE...]\n";

static const char *help_string =
  "\n"
  "GTP debugging options:\n"
  "  --gtp-log=BASE-NAME     log GTP streams to files named `BASE-NAME.<PID>';\n"
  "                          single hyphen stands for stdout\n"
  "  --gtp-show-stderr       pipe GTP servers' stderr to Quarry's stderr;\n"
  "                          if `--gtp-log' is specified, pipe stderr there\n"
  "\n"
  "Help options:\n"
  "  --help                  display this help and exit\n"
  "  --usage                 display brief usage message and exit\n"
  "  --version               output version information and exit\n"
  "\n"
  "Report bugs at https://gna.org/bugs/?group=quarry "
  "or to <quarry-dev@gna.org>.\n";

static const char *version_string =
  PACKAGE_STRING "\n"
  "\n"
  "Copyright (C) 2003, 2004, 2005 Paul Pogonyshev and others.\n"
  "This is free software; see the source for copying conditions.  There is NO\n"
  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";


/* Global option variables. */

/* Whether to log GTP conversations and where.  Either NULL (don't log
 * anything), points to '-' (log to stdout), or points to some other
 * string, in which case log to file named `<BASE-NAME>.<PID>', where
 * BASE-NAME is the value of this variable and PID is the PID of the
 * child.
 */
const char     *option_gtp_log_base_name = NULL;

/* Whether to show children's stderr.  If yes, pipe them to Quarry's
 * stderr, or to whatever `option_gtp_log_base_name' specifies if that
 * is non-NULL.
 */
int		option_gtp_show_stderr = 0;


int
main (int argc, char *argv[])
{
  int return_code = 0;
  int option;

  utils_remember_program_name (argv[0]);

  /* As a side effect, gui_utils_enumerate_themes() determines if
   * Quarry is (supposedly properly) installed: it returns zero if no
   * themes are found.
   */
  if (gui_utils_enumerate_themes ()) {
    bindtextdomain (PACKAGE, LOCALE_DIR);
    bind_textdomain_codeset (PACKAGE, "UTF-8");
    textdomain (PACKAGE);

    if (gui_back_end_init (&argc, &argv)) {
      while ((option = getopt_long (argc, argv, "", quarry_options, NULL))
	     != -1) {
	switch (option) {
	case OPTION_GTP_LOG:
	  option_gtp_log_base_name = optarg;
	  break;

	case OPTION_GTP_SHOW_STDERR:
	  option_gtp_show_stderr = 1;
	  break;

	case OPTION_USAGE:
	case OPTION_HELP:
	  printf (usage_string, full_program_name);

	  if (option == OPTION_HELP)
	    fputs (help_string, stdout);

	  goto exit_quarry;

	case OPTION_VERSION:
	  fputs (version_string, stdout);
	  goto exit_quarry;

	default:
	  fprintf (stderr, "Try `%s --help' for more information.\n",
		   full_program_name);

	  return_code = 255;
	  goto exit_quarry;
	}
      }

      /* When a GTP engine crashes (or it is not a GTP engine to begin
       * with), we receive this signal and better not abort on it.
       */
      signal (SIGPIPE, SIG_IGN);

      if (optind == argc)
	return_code = gui_back_end_main_default ();
      else {
	return_code = gui_back_end_main_open_files (argc - optind,
						    argv + optind);
      }
    }
    else
      return_code = 253;
  }
  else
    return_code = 254;

 exit_quarry:

  gui_utils_discard_theme_lists ();

  utils_free_program_name_strings ();

#if ENABLE_MEMORY_PROFILING
  utils_print_memory_profiling_info ();
#endif

  return return_code;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
