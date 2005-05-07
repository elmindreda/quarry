/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004 Paul Pogonyshev and Martin Holters.          *
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

/* GTP client is an interface to a GTP engine (an external program).
 * It issues specific commands and parses engine's responses.
 *
 * GTP client is a very generic module that doesn't know what a GTP
 * engine is, how to start it or communicate with it.  Instead, it
 * uses a callback function to send information to the engine and any
 * response from the engine can be passed to it by calling function
 * gtp_client_grab_response().
 */

/* Fuck me if I remember what are the internal command/response
 * counters for...  (consider this a FIXME.)
 */


#include "gtp-client.h"
#include "board.h"
#include "game-info.h"
#include "utils.h"

#include <assert.h>
#include <string.h>


#define COLOR_STRING(color)	(color == BLACK ? "black" : "white")


typedef struct _GtpClientUserCallbackData	GtpClientUserCallbackData;

struct _GtpClientUserCallbackData {
  GtpClientResponseCallback   response_callback;
  void			     *user_data;

  int			      integer_data;
  void			     *pointer_data;
};


static void	do_delete_client (GtpClient *client);

static void	send_command (GtpClient *client,
			      GtpClientResponseCallback response_callback,
			      void *user_data, const char *format_string, ...);
static inline GtpClientUserCallbackData *
		store_user_callback_data
		  (GtpClientResponseCallback response_callback,
		   void *user_data, int integer_data, void *pointer_data);

static void	dispatch_response (GtpClient *client);


static int	store_protocol_version (GtpClient *client,
					int successful);
static int	store_single_line_response (GtpClient *client,
					    int successful, char **copy);
static int	store_known_commands_list (GtpClient *client, int successful);
static int	store_supported_games_list (GtpClient *client, int successful);

static int	change_client_integer_parameter
		  (GtpClient *client, int successful,
		   GtpClientUserCallbackData *callback_data);

static int	parse_free_handicap_placement
		  (GtpClient *client, int successful,
		   GtpClientUserCallbackData *callback_data);
static int	parse_generated_move
		  (GtpClient *client, int successful,
		   GtpClientUserCallbackData *callback_data);
static int	parse_final_status_list
		  (GtpClient *client, int successful,
		   GtpClientUserCallbackData *callback_data);


/* Create a new client for an engine that can be sent commands to by
 * calling `send_to_engine' callback.  Function specified in
 * `line_callback'--if not NULL--will be called each time the client
 * receives a full line of response from the engine.  Parameter
 * `deleted_callback' is optional and should be used for freeing any
 * resources (normally, closing pipes) used to communicate with the
 * engine upon client deletion.
 *
 * Parameter `user_data' will be passed to all three callbacks.
 */
GtpClient *
gtp_client_new (GtpClientSendToEngine send_to_engine,
		GtpClientLineCallback line_callback,
		GtpClientErrorCallback error_callback,
		GtpClientInitializedCallback initialized_callback,
		GtpClientDeletedCallback deleted_callback,
		void *user_data)
{
  GtpClient *client = utils_malloc (sizeof (GtpClient));

  assert (send_to_engine);

  client->send_to_engine       = send_to_engine;
  client->line_callback	       = line_callback;
  client->error_callback       = error_callback;
  client->deleted_callback     = deleted_callback;
  client->initialized_callback = initialized_callback;
  client->user_data	       = user_data;

  client->engine_name	   = NULL;
  client->engine_version   = NULL;

  string_list_init (&client->known_commands);
  string_list_init (&client->supported_games);

  client->game = GAME_DUMMY;
  client->board_size = 0;

  gtp_command_list_init (&client->pending_commands);

  string_list_init (&client->response);
  client->incomplete_line = 0;

  client->echo_mode = 0;

  client->operation_stage	  = GTP_CLIENT_UNINITIALIZED;
  client->internal_command_index  = 0;
  client->internal_response_index = 0;

  return client;
}


void
gtp_client_setup_connection (GtpClient *client)
{
  assert (client);
  assert (client->operation_stage == GTP_CLIENT_UNINITIALIZED);

  send_command (client, (GtpClientResponseCallback) store_protocol_version,
		NULL, "protocol_version");

  send_command (client, (GtpClientResponseCallback) store_single_line_response,
		&client->engine_name, "name");

  send_command (client, (GtpClientResponseCallback) store_single_line_response,
		&client->engine_version, "version");

  send_command (client, (GtpClientResponseCallback) store_known_commands_list,
		NULL, "list_commands");
}


/* Free a previously allocated GtpClient structure and all internal
 * information.  If non-NULL `deleted_callback' is specified at client
 * creation, it is called from this function.
 *
 * You should use gtp_client_quit() function most of the time (which
 * schedules a call to gtp_client_delete()).  Use this function
 * directly only when an engine is unreachable (it crashed or the
 * connection is broken).
 */
void
gtp_client_delete (GtpClient *client)
{
  GtpCommandListItem *item;

  assert (client);

  if (client->deleted_callback)
    client->deleted_callback (client, client->user_data);

  /* If there are any pending commands then their response callbacks
   * are called with `client' set to NULL and `successful' to zero,
   * thus indicating that commands were not actually responded to.
   * Callbacks should just perform cleanups (like freeing any
   * allocated resources) if needed.
   */
  for (item = client->pending_commands.first; item; item = item->next) {
    if (item->response_callback
	&& (item->response_callback
	    != (GtpClientResponseCallback) do_delete_client))
      item->response_callback (NULL, 0, item->user_data);
  }

  string_list_empty (&client->pending_commands);

  do_delete_client (client);
}


static void
do_delete_client (GtpClient *client)
{
  string_list_empty (&client->response);

  utils_free (client->engine_name);
  utils_free (client->engine_version);

  string_list_empty (&client->known_commands);
  string_list_empty (&client->supported_games);

  utils_free (client);
}


/* Send client's engine `quit' command and delete the client upon
 * receiving response.
 */
void
gtp_client_quit (GtpClient *client)
{
  assert (client);
  assert (client->operation_stage != GTP_CLIENT_UNINITIALIZED);

  if (client->operation_stage != GTP_CLIENT_QUIT_SCHEDULED) {
    send_command (client, (GtpClientResponseCallback) do_delete_client,
		  NULL, "quit");
    client->operation_stage = GTP_CLIENT_QUIT_SCHEDULED;
  }
}


void
gtp_client_grab_response (GtpClient *client, char *response, int length)
{
  char *const response_end = response + length;
  char *buffer_beginning;

  assert (client);
  assert (response);
  assert (length >= 0);

  for (buffer_beginning = response; buffer_beginning < response_end;) {
    char *buffer_pointer = buffer_beginning;
    char *line_pointer = buffer_beginning;

    while (1) {
      unsigned char character = *buffer_pointer++;

      if (character != '\n') {
	/* Skip all control characters. */
	if ((character > 31 && character != 127) || character == '\t')
	  *line_pointer++ = character;

	if (buffer_pointer < response_end)
	  continue;
      }

      if (character != '\n'
	  || buffer_pointer != buffer_beginning + 1
	  || client->incomplete_line
	  || string_list_is_empty (&client->response)) {
	char *end_of_line = line_pointer;

	if (character == '\n') {
	  /* Ignore trailing whitespace. */
	  while (end_of_line > buffer_beginning
		 && (*(end_of_line - 1) == ' ' || *(end_of_line - 1) == '\t'))
	    end_of_line--;

	  *end_of_line++ = '\n';

	  if (end_of_line == buffer_beginning) {
	    /* Delete trailing whitespace in the stored incomplete
	     * line beginnning (actually, full line).
	     */
	    char *stored_line = client->response.last->text;
	    char *end_of_stored_line = stored_line + strlen (stored_line);

	    while (end_of_stored_line > stored_line
		   && (*(end_of_stored_line - 1) == ' '
		       || *(end_of_stored_line - 1) == '\t'))
	      end_of_stored_line--;

	    *end_of_stored_line = 0;
	  }
	}

	if (!client->incomplete_line) {
	  string_list_add_from_buffer (&client->response, buffer_beginning,
				       end_of_line - buffer_beginning);
	}
	else {
	  if (end_of_line > buffer_beginning) {
	    client->response.last->text
	      = utils_cat_as_string (client->response.last->text,
				     buffer_beginning,
				     end_of_line - buffer_beginning);
	  }
	}

	if (character == '\n') {
	  char *this_line = client->response.last->text;

	  if (string_list_is_empty (&client->pending_commands)) {
	    if (*this_line != '\n' && client->error_callback) {
	      client->error_callback (GTP_WARNING_UNEXPECTED_OUTPUT, -1,
				      client->user_data);
	    }

	    if (client->line_callback)
	      client->line_callback (this_line, 0, -1, client->user_data);

	    string_list_empty (&client->response);
	  }
	  else if (string_list_is_single_string (&client->response)
		   && *this_line != '\n') {
	    /* First line of response contains success flag and
	     * command/response id.  We need to check and store
	     * these for further processing.
	     */
	    GtpError error = GTP_SUCCESS;
	    char *line_beginning = this_line + 1;
	    int command_id = client->pending_commands.first->command_id;

	    if (this_line[0] == '=' || this_line[0] == '?') {
	      client->successful = (this_line[0] == '=');

	      if (command_id >= 0) {
		int response_id = -1;

		if ('0' <= *line_beginning && *line_beginning <= '9') {
		  int num_characters_eaten;

		  sscanf (line_beginning, "%d%n", &response_id,
			  &num_characters_eaten);
		  line_beginning += num_characters_eaten;
		}

		if (response_id != command_id)
		  error = GTP_ERROR_MISMATCHED_ID;
	      }
	      else if ('0' <= *line_beginning && *line_beginning <= '9')
		error = GTP_ERROR_UNEXPECTED_ID;

	      if (error == GTP_SUCCESS && (*line_beginning != ' '
					   && *line_beginning != '\t'
					   && *line_beginning != '\n'))
		error = GTP_ERROR_UNRECOGNIZED_RESPONSE;
	    }
	    else
	      error = GTP_ERROR_UNRECOGNIZED_RESPONSE;

	    if (error == GTP_SUCCESS) {
	      if (client->line_callback
		  && client->pending_commands.first->command) {
		client->line_callback (client->pending_commands.first->command,
				       1, client->internal_response_index,
				       client->user_data);
		client->line_callback (this_line, 0,
				       client->internal_response_index,
				       client->user_data);
	      }

	      /* Throw '=' / '?' flag and command id out of stored
	       * response line.
	       */
	      client->response.first->text
		= utils_duplicate_string (line_beginning);
	      utils_free (this_line);
	    }
	    else {
	      if (client->error_callback)
		client->error_callback (error, command_id, client->user_data);

	      if (client->line_callback)
		client->line_callback (this_line, 0, -1, client->user_data);

	      string_list_empty (&client->response);
	    }
	  }
	  else {
	    if (client->pending_commands.first->command) {
	      client->line_callback (this_line, 0,
				     client->internal_response_index,
				     client->user_data);
	    }

	    if (string_list_is_single_string (&client->response))
	      string_list_empty (&client->response);
	  }

	  client->incomplete_line = 0;
	}
	else
	  client->incomplete_line = 1;
      }
      else {
	/* Two consecutive newlines--we've got a full response! */
	dispatch_response (client);
      }

      break;
    }

    buffer_beginning = buffer_pointer;
  }
}


int
gtp_client_set_echo_mode (GtpClient *client, int echo_mode)
{
  int current_echo_mode;

  assert (client);

  current_echo_mode = client->echo_mode;
  client->echo_mode = echo_mode;

  return current_echo_mode;
}


int
gtp_client_is_known_command (const GtpClient *client, const char *command)
{
  assert (client);
  assert (command);

  return (string_list_is_empty (&client->known_commands)
	  || string_list_find (&client->known_commands, command) != NULL);
}



void
gtp_client_set_game (GtpClient *client,
		     GtpClientResponseCallback response_callback,
		     void *user_data, Game game)
{
  assert (client);
  assert (game >= FIRST_GAME && GAME_IS_SUPPORTED (game));

  send_command (client,
		(GtpClientResponseCallback) change_client_integer_parameter,
		store_user_callback_data (response_callback, user_data,
					  game, &client->game),
		"set_game %s", game_info[game].name);
}


void
gtp_client_set_board_size (GtpClient *client,
			   GtpClientResponseCallback response_callback,
			   void *user_data, int board_size)
{
  assert (client);
  assert (GTP_MIN_BOARD_SIZE <= board_size && board_size <= GTP_MAX_BOARD_SIZE
	  && BOARD_MIN_WIDTH <= board_size && board_size <= BOARD_MAX_WIDTH
	  && BOARD_MIN_HEIGHT <= board_size && board_size <= BOARD_MAX_HEIGHT);

  send_command (client,
		(GtpClientResponseCallback) change_client_integer_parameter,
		store_user_callback_data (response_callback, user_data,
					  board_size, &client->board_size),
		"boardsize %d", board_size);
}


void
gtp_client_set_fixed_handicap (GtpClient *client,
			       GtpClientResponseCallback response_callback,
			       void *user_data, int handicap)
{
  assert (client);

  send_command (client, response_callback, user_data,
		"fixed_handicap %d", handicap);
}


void
gtp_client_place_free_handicap
  (GtpClient *client, GtpClientFreeHandicapCallback response_callback,
   void *user_data, int handicap)
{
  assert (client);

  send_command (client,
		(GtpClientResponseCallback) parse_free_handicap_placement,
		store_user_callback_data (((GtpClientResponseCallback)
					   response_callback),
					  user_data, handicap, NULL),
		"place_free_handicap %d", handicap);
}


void
gtp_client_set_free_handicap (GtpClient *client,
			      GtpClientResponseCallback response_callback,
			      void *user_data,
			      const BoardPositionList *handicap_stones)
{
  char buffer[SUGGESTED_POSITION_LIST_BUFFER_SIZE];

  assert (client);
  assert (handicap_stones);

  game_format_position_list (GAME_GO, client->board_size, client->board_size,
			     buffer, handicap_stones);
  send_command (client, response_callback, user_data,
		"set_free_handicap %s", buffer);
}


void
gtp_client_set_komi (GtpClient *client,
		     GtpClientResponseCallback response_callback,
		     void *user_data, double komi)
{
  assert (client);

  send_command (client, response_callback, user_data, "komi %.f", komi);
}


void
gtp_client_send_time_settings (GtpClient *client,
			       GtpClientResponseCallback response_callback,
			       void *user_data,
			       int main_time, int byo_yomi_time,
			       int moves_per_byo_yomi_period)
{
  assert (client);
  assert (main_time >= 0 && byo_yomi_time >= 0
	  && moves_per_byo_yomi_period >= 0
	  && (byo_yomi_time == 0 || moves_per_byo_yomi_period > 0));

  /* GTP uses slightly different values for "no limit". */
  if (main_time == 0 && byo_yomi_time == 0)
    byo_yomi_time = 1;

  send_command (client, response_callback, user_data,
		"time_settings %d %d %d",
		main_time, byo_yomi_time, moves_per_byo_yomi_period);
}


void
gtp_client_send_time_left (GtpClient *client,
			   GtpClientResponseCallback response_callback,
			   void *user_data,
			   int color, int seconds_left, int moves_left)
{
  assert (client);
  assert (IS_STONE (color));
  assert (seconds_left >= 0 && moves_left >= 0);

  send_command (client, response_callback, user_data,
		"time_left %s %d %d",
		COLOR_STRING (color), seconds_left, moves_left);
}


void
gtp_client_play_move (GtpClient *client,
		      GtpClientResponseCallback response_callback,
		      void *user_data, int color, ...)
{
  va_list move;
  char move_buffer[32];

  assert (client);
  assert (IS_STONE (color));

  va_start (move, color);
  game_format_move_valist (client->game,
			   client->board_size, client->board_size,
			   move_buffer, move);
  va_end (move);

  send_command (client, response_callback, user_data,
		client->protocol_version != 1 ? "play %s %s" : "%s %s",
		COLOR_STRING (color), move_buffer);
}


void
gtp_client_play_move_from_sgf_node
  (GtpClient *client, GtpClientResponseCallback response_callback,
   void *user_data, const SgfGameTree *sgf_game_tree, const SgfNode *sgf_node)
{
  assert (client);
  assert (sgf_game_tree);
  assert (sgf_game_tree->game == client->game);
  assert (sgf_game_tree->board_width == client->board_size
	  && sgf_game_tree->board_height == client->board_size);
  assert (sgf_node);

  if (client->game != GAME_AMAZONS) {
    gtp_client_play_move (client, response_callback, user_data,
			  sgf_node->move_color,
			  sgf_node->move_point.x, sgf_node->move_point.y);
  }
  else {
    gtp_client_play_move (client, response_callback, user_data,
			  sgf_node->move_color,
			  sgf_node->move_point.x, sgf_node->move_point.y,
			  sgf_node->data.amazons);
  }
}


void
gtp_client_generate_move (GtpClient *client,
			  GtpClientMoveCallback response_callback,
			  void *user_data, int color)
{
  assert (client);
  assert (IS_STONE (color));

  send_command (client, (GtpClientResponseCallback) parse_generated_move,
		store_user_callback_data (((GtpClientResponseCallback)
					   response_callback),
					  user_data, color, NULL),
		client->protocol_version != 1 ? "genmove %s" : "genmove_%s",
		COLOR_STRING (color));
}


void
gtp_client_final_status_list
  (GtpClient *client, GtpClientFinalStatusListCallback response_callback,
   void *user_data, GtpStoneStatus status)
{
  const char *status_string = NULL;

  assert (client);

  switch (status) {
  case GTP_ALIVE:
    status_string = "alive";
    break;

  case GTP_DEAD:
    status_string = "dead";
    break;

  case GTP_SEKI:
    status_string = "seki";
    break;
  }

  assert (status_string);

  send_command (client, (GtpClientResponseCallback) parse_final_status_list,
		store_user_callback_data (((GtpClientResponseCallback)
					   response_callback),
					  user_data, status, NULL),
		"final_status_list %s", status_string);
}


static void
send_command (GtpClient *client,
	      GtpClientResponseCallback response_callback, void *user_data,
	      const char *format_string, ...)
{
  char *command;
  va_list arguments;

  va_start (arguments, format_string);
  command = utils_vcprintf (format_string, arguments);
  va_end (arguments);

  command = utils_cat_string (command, "\n");
  string_list_add_ready (&client->pending_commands,
			 client->echo_mode ? command : NULL);

  client->pending_commands.last->command_id	   = -1;
  client->pending_commands.last->response_callback = response_callback;
  client->pending_commands.last->user_data	   = user_data;

  client->send_to_engine (command, client->user_data);

  if (!client->echo_mode)
    utils_free (command);

  client->internal_command_index++;
}


static inline GtpClientUserCallbackData *
store_user_callback_data
  (GtpClientResponseCallback response_callback,
   void *user_data, int integer_data, void *pointer_data)
{
  GtpClientUserCallbackData *callback_data
    = utils_malloc (sizeof (GtpClientUserCallbackData));

  callback_data->response_callback = response_callback;
  callback_data->user_data	   = user_data;
  callback_data->integer_data	   = integer_data;
  callback_data->pointer_data	   = pointer_data;

  return callback_data;
}


static void
dispatch_response (GtpClient *client)
{
  StringListItem *list_item;
  GtpClientResponseCallback response_callback
    = client->pending_commands.first->response_callback;
  void *user_data = client->pending_commands.first->user_data;

  /* Normalize all response lines by collapsing all whitespace (spaces
   * and tabs) to a single space to ease further parsing.  Leading
   * whitespace is removed completely and newline is dropped as well.
   */
  for (list_item = client->response.first; list_item;
       list_item = list_item->next) {
    char *line_pointer = list_item->text;
    char *new_line_pointer = list_item->text;

    while (*line_pointer == ' ' || *line_pointer == '\t')
      line_pointer++;

    while (*line_pointer != '\n') {
      if (*line_pointer != ' ' && *line_pointer != '\t')
	*new_line_pointer++ = *line_pointer++;
      else {
	do
	  line_pointer++;
	while (*line_pointer == ' ' || *line_pointer == '\t');

	*new_line_pointer++ = ' ';
      }
    }

    *new_line_pointer = 0;
  }

  if (response_callback
      && response_callback != (GtpClientResponseCallback) do_delete_client
      && !response_callback (client, client->successful, user_data)
      && client->error_callback) {
    client->error_callback (GTP_ERROR_WRONG_RESPONSE_FORMAT,
			    client->internal_response_index,
			    client->user_data);
  }

  if (client->line_callback && client->pending_commands.first->command) {
    client->line_callback ("\n", 0, client->internal_response_index,
			   client->user_data);
  }

  string_list_delete_first_item (&client->pending_commands);

  if (response_callback != (GtpClientResponseCallback) do_delete_client) {
    string_list_empty (&client->response);
    client->internal_response_index++;
  }
  else {
    /* This special case is needed, because do_delete_client() frees
     * the `client' structure, so it avoids segmentation faults.
     */
    if (client->deleted_callback) {
      client->operation_stage = GTP_CLIENT_QUIT;
      client->deleted_callback (client, client->user_data);
    }

    assert (string_list_is_empty (&client->pending_commands));
    do_delete_client (client);
  }
}


static int
store_protocol_version (GtpClient *client, int successful)
{
  int result;

  if (client)
    client->protocol_version = 1;

  result = (!successful
	    || sscanf (client->response.first->text, "%d",
		       &client->protocol_version));

  if (successful) {
    if (client->protocol_version > 2) {
      if (client->error_callback) {
	client->error_callback (GTP_WARNING_FUTURE_GTP_VERSION,
				client->internal_response_index,
				client->user_data);
      }
    }
    else if (client->protocol_version < 1) {
      result = 0;
      client->protocol_version = 1;
    }
  }

  return result;
}


static int
store_single_line_response (GtpClient *client, int successful, char **copy)
{
  if (successful && string_list_is_single_string (&client->response))
    *copy = utils_duplicate_string (client->response.first->text);

  return !successful || string_list_is_single_string (&client->response);
}


static int
store_known_commands_list (GtpClient *client, int successful)
{
  int valid_response = 1;

  if (successful) {
    StringListItem *list_item;

    for (list_item = client->response.first; list_item;
	 list_item = list_item->next) {
      if (!*list_item->text || strchr (list_item->text, ' ')) {
	valid_response = 0;
	break;
      }
    }

    if (valid_response)
      string_list_steal_items (&client->known_commands, &client->response);
  }

  if (client) {
    if (gtp_client_is_known_command (client, "list_games")) {
      send_command (client,
		    (GtpClientResponseCallback) store_supported_games_list,
		    NULL, "list_games");
    }
    else
      store_supported_games_list (client, 0);
  }

  return valid_response;
}


static int
store_supported_games_list (GtpClient *client, int successful)
{
  if (successful) {
    string_list_steal_items (&client->supported_games, &client->response);

    if (string_list_is_single_string (&client->supported_games)) {
      Game game;

      for (game = FIRST_GAME; game <= LAST_GAME; game++) {
	if (GAME_IS_SUPPORTED (game)
	    && strcmp (client->supported_games.first->text,
		       game_info[game].name) == 0) {
	  client->game = game;
	  break;
	}
      }
    }
  }
  else if (client) {
    string_list_add (&client->supported_games, "Go");
    client->game = GAME_GO;
  }

  if (client) {
    client->operation_stage = GTP_CLIENT_WORKING;
    if (client->initialized_callback)
      client->initialized_callback (client, client->user_data);
  }

  return 1;
}


static int
change_client_integer_parameter (GtpClient *client, int successful,
				 GtpClientUserCallbackData *callback_data)
{
  if (successful)
    * (int *) callback_data->pointer_data = callback_data->integer_data;

  if (callback_data->response_callback) {
    callback_data->response_callback (client, successful,
				      callback_data->user_data);
  }

  utils_free (callback_data);

  return (!successful
	  || (string_list_is_single_string (&client->response)
	      && ! *client->response.first->text));
}


static int
parse_free_handicap_placement (GtpClient *client, int successful,
			       GtpClientUserCallbackData *callback_data)
{
  GtpClientFreeHandicapCallback free_handicap_callback
    = (GtpClientFreeHandicapCallback) callback_data->response_callback;
  BoardPositionList *handicap_stones = NULL;

  if (successful && string_list_is_single_string (&client->response)) {
    handicap_stones = game_parse_position_list (GAME_GO,
						client->board_size,
						client->board_size,
						client->response.first->text);
  }

  free_handicap_callback (client, handicap_stones != NULL,
			  callback_data->user_data, handicap_stones);
  utils_free (callback_data);

  return !successful || handicap_stones != NULL;
}


static int
parse_generated_move (GtpClient *client, int successful,
		      GtpClientUserCallbackData *callback_data)
{
  GtpClientMoveCallback move_callback = ((GtpClientMoveCallback)
					 callback_data->response_callback);
  int move_parsed = 0;

  if (successful && string_list_is_single_string (&client->response)) {
    int x;
    int y;
    BoardAbstractMoveData move_data;
    int num_characters_eaten;

    num_characters_eaten = game_parse_move (client->game,
					    client->board_size,
					    client->board_size,
					    client->response.first->text,
					    &x, &y, &move_data);

    if (num_characters_eaten == 0
	&& strcasecmp (client->response.first->text, "resign") == 0) {
      x = RESIGNATION_X;
      y = RESIGNATION_Y;

      num_characters_eaten = 6;
    }

    if (num_characters_eaten > 0
	&& !client->response.first->text[num_characters_eaten]) {
      if (move_callback) {
	move_callback (client, 1, callback_data->user_data,
		       callback_data->integer_data, x, y, &move_data);
      }

      move_parsed = 1;
    }
  }

  if (!move_parsed && move_callback) {
    move_callback (client, 0, callback_data->user_data,
		   EMPTY, PASS_X, PASS_Y, NULL);
  }

  utils_free (callback_data);

  return !successful || move_parsed;
}


static int
parse_final_status_list (GtpClient *client, int successful,
			 GtpClientUserCallbackData *callback_data)
{
  GtpClientFinalStatusListCallback final_status_list_callback
    = (GtpClientFinalStatusListCallback) callback_data->response_callback;

  if (successful) {
    StringListItem *this_item;
    BoardPositionList *stones = board_position_list_new_empty (0);

    if (!string_list_is_single_string (&client->response)
	|| *client->response.first->text) {
      for (this_item = client->response.first; this_item;
	   this_item = this_item->next) {
	BoardPositionList *string
	  = game_parse_position_list (GAME_GO,
				      client->board_size, client->board_size,
				      this_item->text);

	if (string) {
	  BoardPositionList *stones_new = board_position_list_union (stones,
								     string);

	  board_position_list_delete (stones);
	  board_position_list_delete (string);
	  stones = stones_new;
	}
	else {
	  board_position_list_delete (stones);
	  break;
	}
      }
    }
    else {
      /* Empty response is valid as well. */
      this_item = NULL;
    }

    if (!this_item) {
      /* The list is parsed just fine. */
      final_status_list_callback (client, successful,
				  callback_data->user_data,
				  callback_data->integer_data, stones);
      utils_free (callback_data);

      board_position_list_delete (stones);

      return 1;
    }
  }

  final_status_list_callback (client, 0,
			      callback_data->user_data,
			      callback_data->integer_data, NULL);
  utils_free (callback_data);

  return !successful;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
