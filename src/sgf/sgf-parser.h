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


#ifndef QUARRY_SGF_PARSER_H
#define QUARRY_SGF_PARSER_H


#include "sgf.h"
#include "sgf-errors.h"
#include "board.h"
#include "utils.h"
#include "quarry.h"

#include <iconv.h>
#include <stdio.h>


#define MAX_ERROR_LENGTH		0x200
#define MAX_TIMES_TO_REPORT_ERROR	10


typedef struct _SgfErrorPosition	SgfErrorPosition;
typedef struct _SgfParsingData		SgfParsingData;

struct _SgfErrorPosition {
  int		     line;
  int		     column;
  SgfErrorListItem  *notch;
};

struct _SgfParsingData {
  char		      *buffer;
  int		       buffer_size;
  int		       buffer_size_increment;
  const char	      *buffer_pointer;
  const char	      *buffer_end;
  char		      *temp_buffer;
  int		       buffer_refresh_margin;
  const char	      *buffer_refresh_point;

  const int	      *cancellation_flag;
  int		       cancelled;

  const char	      *stored_buffer_pointers[3];

  char		       token;

  iconv_t	       latin1_to_utf8;
  iconv_t	       tree_char_set_to_utf8;

  FILE		      *file;
  int		       file_bytes_remaining;
  int		       buffer_offset_in_file;
  int		      *bytes_parsed;

  int		       line;
  int		       column;
  int		       pending_column;
  int		       first_column;

  int		       in_parse_root;
  SgfErrorList	      *error_list;
  char		       times_error_reported[SGF_NUM_ERRORS];

  int		       game;
  int		       game_type_expected;

  SgfError (* do_parse_move) (SgfParsingData *data);

  int		       board_width;
  int		       board_height;
  int		       use_board;
  Board		      *board;

  SgfNode	      *game_info_node;

  SgfGameTree	      *tree;
  SgfNode	      *node;
  SgfType	       property_type;

  SgfErrorPosition     property_name_error_position;
  SgfErrorPosition     move_error_position;

  SgfErrorPosition     node_error_position;

  SgfErrorPosition     non_sgf_point_error_position;
  char		       non_sgf_point_x;
  int		       non_sgf_point_y;

  int		       whitespace_passed;
  SgfErrorPosition     whitespace_error_position;

  SgfErrorPosition     zero_byte_error_position;

  unsigned int	       board_common_mark;
  unsigned int	       common_marked_positions[BOARD_GRID_SIZE];

  int		       has_any_setup_property;
  char		       has_setup_add_properties[NUM_ON_GRID_VALUES];
  int		       first_setup_add_property;
  unsigned int	       board_change_mark;
  unsigned int	       changed_positions[BOARD_GRID_SIZE];

  int		       has_any_markup_property;
  char		       has_markup_properties[NUM_SGF_MARKUPS];
  int		       first_markup_property;
  unsigned int	       board_markup_mark;
  unsigned int	       marked_positions[BOARD_GRID_SIZE];

  int		       has_any_territory_property;
  char		       has_territory_properties[NUM_COLORS];
  int		       first_territory_property;
  unsigned int	       board_territory_mark;
  unsigned int	       territory_positions[BOARD_GRID_SIZE];
};


#define DECLARE_VALUE_PARSER(name)			  \
  SgfError	name(SgfParsingData *data)


DECLARE_VALUE_PARSER(sgf_parse_none);
DECLARE_VALUE_PARSER(sgf_parse_real);
DECLARE_VALUE_PARSER(sgf_parse_double);
DECLARE_VALUE_PARSER(sgf_parse_color);
DECLARE_VALUE_PARSER(sgf_parse_simple_text);
DECLARE_VALUE_PARSER(sgf_parse_text);
DECLARE_VALUE_PARSER(sgf_parse_move);

DECLARE_VALUE_PARSER(sgf_parse_list_of_point);
DECLARE_VALUE_PARSER(sgf_parse_list_of_vector);
DECLARE_VALUE_PARSER(sgf_parse_list_of_label);

DECLARE_VALUE_PARSER(sgf_parse_application);
DECLARE_VALUE_PARSER(sgf_parse_board_size);
DECLARE_VALUE_PARSER(sgf_parse_date);
DECLARE_VALUE_PARSER(sgf_parse_figure);
DECLARE_VALUE_PARSER(sgf_parse_file_format);
DECLARE_VALUE_PARSER(sgf_parse_game_type);
DECLARE_VALUE_PARSER(sgf_parse_handicap);
DECLARE_VALUE_PARSER(sgf_parse_komi);
DECLARE_VALUE_PARSER(sgf_parse_markup_property);
DECLARE_VALUE_PARSER(sgf_parse_move_number);
DECLARE_VALUE_PARSER(sgf_parse_moves_left);
DECLARE_VALUE_PARSER(sgf_parse_print_mode);
DECLARE_VALUE_PARSER(sgf_parse_result);
DECLARE_VALUE_PARSER(sgf_parse_setup_property);
DECLARE_VALUE_PARSER(sgf_parse_style);
DECLARE_VALUE_PARSER(sgf_parse_territory);
DECLARE_VALUE_PARSER(sgf_parse_time_limit);

DECLARE_VALUE_PARSER(sgf_parse_letters);
DECLARE_VALUE_PARSER(sgf_parse_simple_markup);


/* Tree structure used by parse_properties() for very quick property
 * name lookup.
 */
extern const SgfType	property_tree[][1 + ('Z' - 'A' + 1)];

/* List of errors the parser generates on incorrect input. */
extern const char      *sgf_errors[];


#endif /* QUARRY_SGF_PARSER_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
