/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005, 2006 Paul Pogonyshev.           *
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


#include "parse-list.h"
#include "board-topology.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>


#define CAPITALIZATION_HINT						\
  "  /* TRANSLATORS: Use your language's natural capitalization for the\n" \
  "     name of the game.  I.e. it doesn't have to start with a capital\n" \
  "     letter. */\n"


static int	game_list_parse_game1 (char **line);
static int	game_list_parse_game2 (StringBuffer *c_file_arrays,
				       char **line,
				       const char *identifier,
				       char **pending_eol_comment,
				       int *pending_linefeeds);


static const ListDescription game_lists[] = {
  { "games", 0, SORT_NORMAL, 0, "const GameInfo ",
    NULL, game_list_parse_game1, game_list_parse_game2, NULL },
  { NULL, 0, SORT_NORMAL, 0, NULL, NULL, NULL, NULL, NULL }
};

const ListDescriptionSet list_set = { "games", game_lists };


static const char  *game_full_name;


int
main (int argc, char *argv[])
{
  return parse_list_main (argc, argv, &list_set, 1, NULL);
}


static int
game_list_parse_game1 (char **line)
{
  PARSE_THING (game_full_name, STRING_OR_NULL, line, "game full name");
  return 0;
}


static int
game_list_parse_game2 (StringBuffer *c_file_arrays,
		       char **line, const char *identifier,
		       char **pending_eol_comment, int *pending_linefeeds)
{
  const char *default_board_size;
  StringBuffer standard_board_sizes;
  const char *color_to_play_first;
  const char *adjust_color_to_play_function;
  const char *is_game_over_function;
  const char *horizontal_coordinates;
  int reversed_vertical_coordinates_flag;
  const char *get_default_setup_function;
  const char *reset_game_data_function;
  const char *is_legal_move_function;
  const char *play_move_function;
  const char *undo_function;
  const char *apply_changes_function;
  const char *add_dummy_move_entry_function;
  const char *format_move_function;
  const char *parse_move_function;
  const char *validate_board_function;
  const char *dump_board_function;
  const char *move_stack_entry_structure;
  const char *relative_num_moves_per_game;

  UNUSED (identifier);
  UNUSED (pending_eol_comment);
  UNUSED (pending_linefeeds);

  if (looking_at ("UNSUPPORTED", line)) {
    /* A special, but very common case. */
    if (strcmp (game_full_name, "NULL") != 0) {
      string_buffer_cprintf (c_file_arrays,
			     ("  /* TRANSLATORS: Not important currently. */\n"
			      CAPITALIZATION_HINT
			      "  { N_(%s), "),
			     game_full_name);
    }
    else
      string_buffer_cat_string (c_file_arrays, "  { NULL, ");

    string_buffer_cat_string (c_file_arrays,
			      ("0, NULL, EMPTY, NULL,\n"
			       "    NULL,\n    NULL, 0,\n    NULL, NULL,\n"
			       "    NULL, NULL, NULL,\n    NULL, NULL,\n"
			       "    NULL, NULL,\n    NULL, NULL,\n"
			       "    0, 0.0 }"));

    return 0;
  }

  PARSE_THING (default_board_size, INTEGER_NUMBER, line, "default board size");

  string_buffer_init (&standard_board_sizes, 0x20, 0x20);

  if (looking_at ("{", line)) {
    while (!looking_at ("}", line)) {
      const char *board_size;

      PARSE_THING (board_size, INTEGER_NUMBER, line, "board size");

      if (standard_board_sizes.length == 0)
	string_buffer_add_character (&standard_board_sizes, '"');

      string_buffer_cat_string (&standard_board_sizes, board_size);
      string_buffer_cat_string (&standard_board_sizes, "\\000");
    }

    if (standard_board_sizes.length > 0)
      string_buffer_add_character (&standard_board_sizes, '"');
  }

  if (standard_board_sizes.length == 0)
    string_buffer_cat_string (&standard_board_sizes, "NULL");

  if (looking_at ("black", line))
    color_to_play_first = "BLACK";
  else if (looking_at ("white", line))
    color_to_play_first = "WHITE";
  else {
    print_error ("color to play first expected");
    return 1;
  }

  PARSE_IDENTIFIER (adjust_color_to_play_function, line,
		    "adjust_color_to_play function");
  PARSE_IDENTIFIER (is_game_over_function, line, "is_game_over function");

  PARSE_IDENTIFIER (horizontal_coordinates, line, "horizontal coordinates");
  if (strlen (horizontal_coordinates) > BOARD_MAX_WIDTH) {
    print_error ("too many horizontal coordinates (%d)",
		 strlen (horizontal_coordinates));
    return 1;
  }

  if (looking_at ("normal", line))
    reversed_vertical_coordinates_flag = 0;
  else if (looking_at ("reversed", line))
    reversed_vertical_coordinates_flag = 1;
  else {
    print_error ("reversed vertical coordinates flag expected");
    return 1;
  }

  PARSE_IDENTIFIER (get_default_setup_function, line,
		    "get_default_setup function");
  PARSE_IDENTIFIER (reset_game_data_function, line,
		    "reset_game_data function");
  PARSE_IDENTIFIER (is_legal_move_function, line, "is_legal_move function");
  PARSE_IDENTIFIER (play_move_function, line, "play_move function");
  PARSE_IDENTIFIER (undo_function, line, "undo function");
  PARSE_IDENTIFIER (apply_changes_function, line, "apply_changes function");
  PARSE_IDENTIFIER (add_dummy_move_entry_function, line,
		    "add_dummy_move_entry function");
  PARSE_IDENTIFIER (format_move_function, line, "format_move function");
  PARSE_IDENTIFIER (parse_move_function, line, "parse_move function");
  PARSE_IDENTIFIER (validate_board_function, line, "validate_board function");
  PARSE_IDENTIFIER (dump_board_function, line, "dump_board function");
  PARSE_IDENTIFIER (move_stack_entry_structure, line,
		    "MoveStackEntry structure");
  PARSE_THING (relative_num_moves_per_game, FLOATING_POINT_NUMBER, line,
	       "relative number of moves per game");

  string_buffer_cprintf (c_file_arrays,
			 (CAPITALIZATION_HINT
			  "  { N_(%s), %s, %s, %s, %s,\n    %s,\n"
			  "    \"%s\", %d,\n    %s, %s,\n    %s, %s, %s,\n"
			  "    %s, %s,\n    %s, %s,\n    %s, %s,\n"
			  "    sizeof (%s), %s }"),
			 game_full_name, default_board_size,
			 standard_board_sizes.string, color_to_play_first,
			 adjust_color_to_play_function, is_game_over_function,
			 horizontal_coordinates,
			 reversed_vertical_coordinates_flag,
			 get_default_setup_function, reset_game_data_function,
			 is_legal_move_function, play_move_function,
			 undo_function,
			 apply_changes_function, add_dummy_move_entry_function,
			 format_move_function, parse_move_function,
			 validate_board_function, dump_board_function,
			 move_stack_entry_structure,
			 relative_num_moves_per_game);

  string_buffer_dispose (&standard_board_sizes);

  return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
