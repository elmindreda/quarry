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


#include "gtp-client.h"
#include "utils.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <string.h>
#include <assert.h>


static int	client_is_alive = 1;


static void
send_to_engine (const char *command, void *user_data)
{
  write (* (int *) user_data, command, strlen (command));
  fdatasync (* (int *) user_data);
}


static void
line_callback (const char *line, int is_command, int internal_index,
	       void *user_data)
{
  UNUSED (user_data);

  printf ("%c [%d] %s", is_command ? '>' : ' ', internal_index, line);
}


static void
error_callback (GtpError error, int command_id, void *user_data)
{
  UNUSED (command_id);
  UNUSED (user_data);

  fprintf (stderr, "*** GTP error %d ***\n", error);
}


static void
initialized_callback (GtpClient *client, void *user_data)
{
  UNUSED (user_data);

  printf ("Connected to %s version %s using GTP %d\n",
	  client->engine_name, client->engine_version,
	  client->protocol_version);

  gtp_client_quit (client);
}


static void
deleted_callback (GtpClient *client, void *user_data)
{
  UNUSED (client);
  UNUSED (user_data);

  client_is_alive = 0;
}


int
main (int argc, char *argv[], char *envp[])
{
  int result = 0;

  utils_remember_program_name (argv[0]);

  if (argc > 1) {
    int engine_in_pipe[2];
    int engine_out_pipe[2];
    pid_t child_pid;

    if (pipe (engine_in_pipe) != 0)
      assert (0);

    if (pipe (engine_out_pipe) != 0)
      assert (0);

    child_pid = fork ();

    if (child_pid == 0) {
      int descriptor;
      int open_max = sysconf (_SC_OPEN_MAX);

      for (descriptor = 0; descriptor < open_max; descriptor++) {
	if (descriptor != STDERR_FILENO)
	  fcntl (descriptor, F_SETFD, FD_CLOEXEC);
      }

      if (dup2 (engine_in_pipe[0], STDIN_FILENO) != STDIN_FILENO)
	assert (0);

      if (dup2 (engine_out_pipe[1], STDOUT_FILENO) != STDOUT_FILENO)
	assert (0);

      execve (argv[1], argv + 1, envp);

      fprintf (stderr, "%s: error: unable to execute %s\n", argv[0], argv[1]);
      exit (255);
    }
    else {
      close (engine_in_pipe[0]);
      close (engine_out_pipe[1]);

      if (child_pid > 0) {
	int engine_in  = engine_in_pipe[1];
	int engine_out = engine_out_pipe[0];
	GtpClient *client = gtp_client_new (send_to_engine,
					    line_callback, error_callback,
					    initialized_callback,
					    deleted_callback,
					    &engine_in);

	gtp_client_echo_on (client);
	gtp_client_setup_connection (client);

	while (client_is_alive) {
	  fd_set in_set;

	  FD_ZERO (&in_set);
	  FD_SET (STDIN_FILENO, &in_set);
	  FD_SET (engine_out, &in_set);

	  if (select (engine_out + 1, &in_set, NULL, NULL, NULL) <= 0)
	    break;

	  if (FD_ISSET (engine_out, &in_set)) {
	    /* The intention is to stress gtp_client_grab_response()
	     * here.  We first sleep for a little bit hoping that more
	     * input will arive.  Then we throw the response at the
	     * function in random chunks, completely disregarding line
	     * boundaries.
	     */
	    usleep (1);

	    while (client_is_alive) {
	      char buffer[0x1000];
	      int bytes_read = read (engine_out, buffer, sizeof buffer);
	      int processed_length = 0;

	      while (processed_length < bytes_read) {
		int random_chunk_length
		  = 1 + random () % (((bytes_read - processed_length) * 3)
				     / 2);

		if (random_chunk_length > bytes_read - processed_length)
		  random_chunk_length = bytes_read - processed_length;

		gtp_client_grab_response (client, buffer + processed_length,
					  random_chunk_length);

		processed_length += random_chunk_length;
	      }

	      if (bytes_read < sizeof buffer)
		break;
	    }
	  }
	}

	close (engine_in);
	close (engine_out);
      }
      else {
	fprintf (stderr, "%s: error: unable to fork\n", argv[0]);

	close (engine_in_pipe[1]);
	close (engine_out_pipe[0]);

	result = 254;
      }
    }
  }
  else {
    fprintf (stderr, "Usage: %s ENGINE [PARAMETER ...]\n", full_program_name);
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
