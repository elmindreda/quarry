/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005 Paul Pogonyshev                  *
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

/* This SGF parser is monstrously large, but since it is already
 * written and it is good, there's no point in throwing it out ;)
 *
 * The parser provides the following features:
 *
 * - Full support of SGF 1--4 (SGF 1--3 files are upgraded to SGF 4).
 *
 * - Robustness: can handle many erroneous files.
 *
 * - Quite complete error checking: use it to test your SGF writer.
 *
 * - No limits on any string lengths (be that text property value or
 *   property identifier) except those imposed by memory availability.
 *
 * - High parsing speed (if anyone out there has huge SGF files...)
 *
 * - Reading of input file in chunks: smaller memory footprint (again,
 *   noticeable on huge files only).
 *
 * - Thread safety; simple progress/cancellation interface (must be
 *   matched with support at higher levels).
 */

/* FIXME: add more comments, especially in tricky/obscure places. */


#include "sgf.h"
#include "sgf-parser.h"
#include "sgf-errors.h"
#include "sgf-privates.h"
#include "board.h"
#include "game-info.h"
#include "utils.h"

#include <assert.h>
#include <iconv.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


#define SGF_END			0

/* SGF specification remains silent about escaping in non-text
 * properties, but `SGFC' allows escaping in them.  Function
 * next_token_in_value() provides transparent escaping.  In order to
 * avoid premature end of value parsing, escaped ']' is replaced with
 * the below character, which is never returned by next_character().
 */
#define ESCAPED_BRACKET		'\r'


typedef struct _BufferPositionStorage	BufferPositionStorage;

struct _BufferPositionStorage {
  char		token;
  int		line;
  int		column;
  int		pending_column;
};


#define STORE_BUFFER_POSITION(data, index, storage)			\
  do {									\
    (data)->stored_buffer_pointers[index] = (data)->buffer_pointer;	\
    (storage).token			  = (data)->token;		\
    (storage).line			  = (data)->line;		\
    (storage).column			  = (data)->column;		\
    (storage).pending_column		  = (data)->pending_column;	\
  } while (0)

#define RESTORE_BUFFER_POSITION(data, index, storage)			\
  do {									\
    (data)->buffer_pointer = (data)->stored_buffer_pointers[index];	\
    (data)->token	   = (storage).token;				\
    (data)->line	   = (storage).line;				\
    (data)->column	   = (storage).column;				\
    (data)->pending_column = (storage).pending_column;			\
  } while (0)


#define STORE_ERROR_POSITION(data, storage)				\
  do {									\
    (storage).line   = (data)->line;					\
    (storage).column = (data)->column;					\
    (storage).notch  = (data)->error_list->last;			\
  } while (0)


static int	    parse_buffer (SgfParsingData *data,
				  SgfCollection **collection,
				  SgfErrorList **error_list,
				  const SgfParserParameters *parameters,
				  int *bytes_parsed,
				  const int *cancellation_flag);
static int	    parse_root (SgfParsingData *data);
static SgfNode *    parse_node_tree (SgfParsingData *data, SgfNode *parent);
static void	    parse_node_sequence (SgfParsingData *data, SgfNode *node);
static void	    parse_property (SgfParsingData *data);

static void	    refresh_buffer (SgfParsingData *data);
static void	    expand_buffer (SgfParsingData *data);

static int	    complete_node_and_update_board (SgfParsingData *data,
						    int is_leaf_node);
static void	    create_position_lists
		      (SgfParsingData *data, BoardPositionList **position_list,
		       const SgfType *property_types, int num_properties,
		       const unsigned int marked_positions[BOARD_GRID_SIZE],
		       unsigned int current_mark);

static void	    discard_single_value (SgfParsingData *data);
static void	    discard_values (SgfParsingData *data);
static void	    parse_unknown_property_values
		      (SgfParsingData *data, StringList *property_value_list);


static int	    do_parse_number (SgfParsingData *data, int *number);
static int	    do_parse_real (SgfParsingData *data, double *real);

static char *	    do_parse_simple_text (SgfParsingData *data,
					  char extra_stop_character);
static char *	    do_parse_text (SgfParsingData *data, char *existing_text);
static char *	    convert_text_to_utf8 (SgfParsingData *data,
					  char *existing_text);

static int	    do_parse_point (SgfParsingData *data, BoardPoint *point);

static SgfError	    do_parse_go_move (SgfParsingData *data);
static SgfError	    do_parse_othello_move (SgfParsingData *data);
static SgfError	    do_parse_amazons_move (SgfParsingData *data);

static inline int   do_parse_point_or_rectangle (SgfParsingData *data,
						 BoardPoint *left_top,
						 BoardPoint *right_bottom);
static int	    do_parse_list_of_point
		      (SgfParsingData *data,
		       unsigned int marked_positions[BOARD_GRID_SIZE],
		       unsigned int current_mark, int value,
		       SgfError duplication_error,
		       const char match_grid[BOARD_GRID_SIZE]);

static int	    do_parse_board_size (SgfParsingData *data, int *width,
					 int *height, int add_errors);

static int	    looking_at (SgfParsingData *data,
				const char **strings, int num_strings);


static void	    add_error (SgfParsingData *data, SgfError error, ...);
static void	    insert_error (SgfParsingData *data, SgfError error,
				  SgfErrorPosition *error_position, ...);
static void	    insert_error_valist (SgfParsingData *data, SgfError error,
					 SgfErrorPosition *error_position,
					 va_list arguments);

static SgfError	    invalid_game_info_property
		      (SgfParsingData *data, SgfProperty *property,
		       BufferPositionStorage *storage);

static inline void  begin_parsing_value (SgfParsingData *data);
static int	    is_composed_value (SgfParsingData *data,
				       int expecting_simple_text);
static SgfError	    end_parsing_value (SgfParsingData *data);

static int	    format_error_valist (SgfParsingData *data, char *buffer,
					 SgfError error, va_list arguments);


static inline void  next_token (SgfParsingData *data);
static void	    next_token_in_value (SgfParsingData *data);
static void	    next_character (SgfParsingData *data);


const SgfParserParameters sgf_parser_defaults = {
  4 * 1024 * 1024, 64 * 1024, 1024 * 1024,
  0
};



/* Read an SGF file and parse all game trees it contains.  If the
 * maximum buffer size specified in `parameters' is not enough to keep
 * the whole file in memory, this function sets up data required for
 * refreshing the buffer.
 */
int
sgf_parse_file (const char *filename, SgfCollection **collection,
		SgfErrorList **error_list,
		const SgfParserParameters *parameters,
		int *file_size, int *bytes_parsed,
		const int *cancellation_flag)
{
  int result;
  FILE *file;

  assert (filename);
  assert (collection);
  assert (error_list);
  assert (parameters);

  /* Buffer size parameters should be sane. */
  assert (parameters->max_buffer_size >= 512 * 1024);
  assert (parameters->buffer_size_increment >= 256 * 1024);
  assert (parameters->buffer_refresh_margin >= 1024);
  assert (8 * parameters->buffer_refresh_margin
	  <= parameters->max_buffer_size);

  *collection = NULL;
  *error_list = NULL;
  result = SGF_ERROR_READING_FILE;

  file = fopen (filename, "rb");
  if (file) {
    if (fseek (file, 0, SEEK_END) != -1) {
      SgfParsingData parsing_data;
      int max_buffer_size = ROUND_UP (parameters->max_buffer_size, 4 * 1024);
      int local_file_size;
      int buffer_size;
      char *buffer;

      local_file_size = ftell (file);
      if (local_file_size == 0) {
	fclose (file);
	return SGF_INVALID_FILE;
      }

      if (file_size)
	*file_size = local_file_size;

      rewind (file);

      if (local_file_size <= max_buffer_size)
	buffer_size = local_file_size;
      else
	buffer_size = max_buffer_size;

      buffer = utils_malloc (buffer_size);

      if (fread (buffer, buffer_size, 1, file) == 1) {
	parsing_data.buffer	 = buffer;
	parsing_data.buffer_end	 = buffer + buffer_size;
	parsing_data.buffer_size = buffer_size;

	parsing_data.file_bytes_remaining = local_file_size - buffer_size;

	if (local_file_size <= max_buffer_size)
	  parsing_data.buffer_refresh_point = parsing_data.buffer_end;
	else {
	  parsing_data.buffer_size_increment
	    = ROUND_UP (parameters->buffer_size_increment, 4 * 1024);
	  parsing_data.buffer_refresh_margin
	    = ROUND_UP (parameters->buffer_refresh_margin, 1024);
	  parsing_data.buffer_refresh_point
	    = parsing_data.buffer_end - parsing_data.buffer_refresh_margin;

	  parsing_data.file = file;
	}

	result = parse_buffer (&parsing_data, collection, error_list,
			       parameters, bytes_parsed, cancellation_flag);
      }

      utils_free (parsing_data.buffer);
    }

    fclose (file);
  }

  return result;
}


/* Parse SGF data in a buffer and creates a game collection from it.
 * In case of errors it returns NULL.
 *
 * Note that the data in buffer is overwritten in process, at least
 * partially.
 */
int
sgf_parse_buffer (char *buffer, int size,
		  SgfCollection **collection, SgfErrorList **error_list,
		  const SgfParserParameters *parameters,
		  int *bytes_parsed, const int *cancellation_flag)
{
  SgfParsingData parsing_data;

  assert (buffer);
  assert (size >= 0);
  assert (collection);
  assert (error_list);
  assert (parameters);

  parsing_data.buffer = buffer;
  parsing_data.buffer_refresh_point = buffer + size;
  parsing_data.buffer_end = buffer + size;

  parsing_data.file_bytes_remaining = 0;

  return parse_buffer (&parsing_data, collection, error_list, parameters,
		       bytes_parsed, cancellation_flag);
}



static int
parse_buffer (SgfParsingData *data,
	      SgfCollection **collection, SgfErrorList **error_list,
	      const SgfParserParameters *parameters,
	      int *bytes_parsed, const int *cancellation_flag)
{
  int dummy_bytes_parsed;
  int dummy_cancellation_flag;

  assert (parameters->first_column == 0 || parameters->first_column == 1);

  *collection = sgf_collection_new ();
  *error_list = sgf_error_list_new ();

  memset (data->times_error_reported, 0, sizeof data->times_error_reported);

  data->buffer_pointer = data->buffer;

  data->buffer_offset_in_file = 0;
  if (bytes_parsed) {
    *bytes_parsed = 0;
    data->bytes_parsed = bytes_parsed;
  }
  else
    data->bytes_parsed = &dummy_bytes_parsed;

  data->cancelled = 0;
  if (cancellation_flag)
    data->cancellation_flag = cancellation_flag;
  else {
    dummy_cancellation_flag = 0;
    data->cancellation_flag = &dummy_cancellation_flag;
  }

  data->token = 0;

  data->line				  = 0;
  data->pending_column			  = 0;
  data->first_column			  = parameters->first_column;
  data->ko_property_error_position.line	  = 0;
  data->non_sgf_point_error_position.line = 0;
  data->zero_byte_error_position.line	  = 0;

  data->board = NULL;
  data->error_list = *error_list;

  data->latin1_to_utf8 = iconv_open ("UTF-8", "ISO-8859-1");
  assert (data->latin1_to_utf8 != (iconv_t) (-1));

  next_token (data);

  do {
    /* Skip any junk that might appear before game tree. */
    if (data->token == '(') {
      next_token (data);
      if (data->token != ';')
	continue;
    }
    else {
      next_token (data);
      continue;
    }

    /* Parse the tree. */
    data->tree = sgf_game_tree_new ();
    if (parse_root (data))
      sgf_collection_add_game_tree (*collection, data->tree);
    else
      sgf_game_tree_delete (data->tree);

    if (data->tree_char_set_to_utf8 != data->latin1_to_utf8
	&& data->tree_char_set_to_utf8 != NULL)
      iconv_close (data->tree_char_set_to_utf8);
  } while (data->token != SGF_END);

  if (data->board)
    board_delete (data->board);

  iconv_close (data->latin1_to_utf8);

  if (data->cancelled || (*collection)->num_trees == 0) {
    string_list_delete (*error_list);
    *error_list = NULL;

    sgf_collection_delete (*collection);
    *collection = NULL;

    return data->cancelled ? SGF_PARSING_CANCELLED : SGF_INVALID_FILE;
  }

  if (string_list_is_empty (*error_list)) {
    string_list_delete (*error_list);
    *error_list = NULL;
  }

  return SGF_PARSED;
}


/* Parse root node.  This function is needed because values of `CA',
 * `GM' and `SZ' properties are crucial for property value validation.
 * The function does nothing but finding these properties (if they are
 * present at all).  Afterwards, root node is parsed as usually.
 */
static int
parse_root (SgfParsingData *data)
{
  SgfGameTree *tree = data->tree;
  BufferPositionStorage storage;

  data->tree_char_set_to_utf8 = data->latin1_to_utf8;

  STORE_BUFFER_POSITION (data, 1, storage);

  data->in_parse_root = 1;
  data->game	      = 0;
  data->board_width   = 0;

  next_token (data);

  while (data->token != ';' && data->token != '(' && data->token != ')'
	 && data->token != SGF_END) {
    SgfType property_type = SGF_NUM_PROPERTIES;

    while (data->token != '[' && data->token != SGF_END) {
      if ('A' <= data->token && data->token <= 'Z') {
	if (property_type >= SGF_NUM_PROPERTIES) {
	  property_type = (property_tree[property_type - SGF_NUM_PROPERTIES]
			   [1 + (data->token - 'A')]);
	}
	else
	  property_type = SGF_UNKNOWN;
      }
      else if (data->token < 'a' || 'z' < data->token) {
	property_type = SGF_NUM_PROPERTIES;
	next_token (data);
	continue;
      }

      next_character (data);
    }

    if (property_type >= SGF_NUM_PROPERTIES)
      property_type = property_tree[property_type - SGF_NUM_PROPERTIES][0];

    if (property_type == SGF_GAME_TYPE && !data->game) {
      next_token (data);
      if (do_parse_number (data, &data->game)) {
	if (1 <= data->game && data->game <= 1000) {
	  if ((data->board_width || !GAME_IS_SUPPORTED (data->game))
	      && tree->char_set)
	    break;
	}
	else
	  data->game = 0;
      }
    }
    else if (property_type == SGF_BOARD_SIZE && !data->board_width) {
      next_token (data);
      if (do_parse_board_size (data, &data->board_width, &data->board_height,
			       0)
	  && data->game && tree->char_set)
	break;
    }
    else if (property_type == SGF_CHAR_SET && !tree->char_set) {
      tree->char_set = do_parse_simple_text (data, SGF_END);

      if (tree->char_set) {
	char *char_set_uppercased = utils_duplicate_string (tree->char_set);
	char *scan;

	/* We now deal with an UTF-8 string, so uppercasing latin
	 * letters is not a problem.
	 */
	for (scan = char_set_uppercased; *scan; scan++) {
	  if ('a' <= *scan && *scan <= 'z')
	    *scan += 'A' - 'a';
	}

	data->tree_char_set_to_utf8
	  = (strcmp (char_set_uppercased, "UTF-8") == 0
	     ? NULL : iconv_open ("UTF-8", char_set_uppercased));
	utils_free (char_set_uppercased);

	if (data->tree_char_set_to_utf8 != (iconv_t) (-1)) {
	  if (data->game && data->board_width)
	    break;
	}
	else {
	  data->tree_char_set_to_utf8 = data->latin1_to_utf8;
	  utils_free (tree->char_set);
	}
      }
    }

    discard_values (data);
  }

  if (data->game)
    data->game_type_expected = 1;
  else {
    data->game = GAME_GO;
    data->game_type_expected = 0;
  }

  if (GAME_IS_SUPPORTED (data->game)) {
    if (data->board_width == 0) {
      data->board_width = game_info[data->game].default_board_size;
      data->board_height = game_info[data->game].default_board_size;
    }

    if (data->board_width > SGF_MAX_BOARD_SIZE)
      data->board_width = SGF_MAX_BOARD_SIZE;
    if (data->board_height > SGF_MAX_BOARD_SIZE)
      data->board_height = SGF_MAX_BOARD_SIZE;

    data->use_board = (BOARD_MIN_WIDTH <= data->board_width
		       && data->board_width <= BOARD_MAX_WIDTH
		       && BOARD_MIN_HEIGHT <= data->board_height
		       && data->board_height <= BOARD_MAX_HEIGHT);
    if (data->use_board) {
      if (data->game == GAME_GO)
	data->do_parse_move = do_parse_go_move;
      else if (data->game == GAME_OTHELLO)
	data->do_parse_move = do_parse_othello_move;
      else if (data->game == GAME_AMAZONS)
	data->do_parse_move = do_parse_amazons_move;
      else
	assert (0);

      if (!data->board) {
	data->board = board_new (data->game,
				 data->board_width, data->board_height);
      }
      else {
	board_set_parameters (data->board, data->game,
			      data->board_width, data->board_height);
      }

      data->board_common_mark = 0;
      data->board_change_mark = 0;
      data->board_markup_mark = 0;
      board_fill_uint_grid (data->board, data->common_marked_positions, 0);
      board_fill_uint_grid (data->board, data->changed_positions, 0);
      board_fill_uint_grid (data->board, data->marked_positions, 0);

      if (data->game == GAME_GO) {
	data->board_territory_mark = 0;
	board_fill_uint_grid (data->board, data->territory_positions, 0);
      }
    }
  }

  RESTORE_BUFFER_POSITION (data, 1, storage);

  data->in_parse_root = 0;
  data->game_info_node = NULL;

  data->has_any_setup_property	   = 0;
  data->first_setup_add_property   = 1;
  data->has_any_markup_property	   = 0;
  data->first_markup_property	   = 1;
  data->has_any_territory_property = 0;
  data->first_territory_property   = 1;

  sgf_game_tree_set_game (tree, data->game);
  tree->board_width = data->board_width;
  tree->board_height = data->board_height;

  tree->file_format = 0;
  tree->root = parse_node_tree (data, NULL);

  if (data->token == SGF_END && !data->cancelled)
    add_error (data, SGF_CRITICAL_UNEXPECTED_END_OF_FILE);
  next_token (data);

  if (tree->root) {
    tree->current_node = tree->root;
    return 1;
  }

  return 0;
}


static SgfNode *
parse_node_tree (SgfParsingData *data, SgfNode *parent)
{
  SgfNode *node;

  /* Skip any junk that might appear before the first node. */
  while (data->token != ';') {
    if (data->token == ')') {
      add_error (data, SGF_ERROR_EMPTY_VARIATION);
      next_token (data);
      return NULL;
    }

    if (data->token == SGF_END)
      return NULL;

    next_token (data);
  }

  STORE_ERROR_POSITION (data, data->node_error_position);
  next_token (data);

  node = sgf_node_new (data->tree, parent);
  parse_node_sequence (data, node);

  /* Skip any junk after the last variation. */
  while (data->token != ')' && data->token != SGF_END)
    next_token (data);

  return node;
}


/* Parse a sequence (a straight tree branch) of nodes.
 *
 * It would have been more straightforward to parse a single node and
 * then recurse, but parsing whole sequences saves huge amounts of
 * stack space.  It even prevents segment violations (due to stack
 * finiteness) in case of extremely long node sequences (a bug found
 * with `sgf-board-stress.pike').  Finally, it saves almost 10%
 * runtime in the latter case :).
 */
static void
parse_node_sequence (SgfParsingData *data, SgfNode *node)
{
  SgfNode *game_info_node = data->game_info_node;
  int num_undos = 0;

  while (1) {
    if (*data->cancellation_flag) {
      data->cancelled		 = 1;
      data->buffer_end		 = data->buffer_pointer;
      data->buffer_refresh_point = data->buffer_end;
    }

    *data->bytes_parsed = (data->buffer_offset_in_file
			   + (data->buffer_pointer - data->buffer));

    if (data->buffer_pointer > data->buffer_refresh_point)
      refresh_buffer (data);

    data->node = node;
    while (data->token != ';' && data->token != '(' && data->token != ')'
	   && data->token != SGF_END)
      parse_property (data);

    if (data->node == node) {
      num_undos += complete_node_and_update_board (data,
						   (data->token != ';'
						    && data->token != '('));
      node = data->node;

      if (data->token == ';') {
	STORE_ERROR_POSITION (data, data->node_error_position);
	next_token (data);

	node->child = sgf_node_new (data->tree, node);
	node = node->child;

	continue;
      }

      if (data->token == '(') {
	SgfNode **link = &node->child;

	do {
	  next_token (data);

	  *link = parse_node_tree (data, node);
	  if (*link)
	    link = & (*link)->next;

	  next_token (data);
	} while (data->token == '(');
      }
    }

    break;
  }

  board_undo (data->board, num_undos);

  data->game_info_node = game_info_node;
}


static void
parse_property (SgfParsingData *data)
{
  /* FIXME: this loop should add errors in certain cases. */
  while (data->token != '[') {
    data->temp_buffer = data->buffer;

    STORE_ERROR_POSITION (data, data->property_name_error_position);

    while (data->token != '[') {
      if ('A' <= data->token && data->token <= 'Z')
	*data->temp_buffer++ = data->token;
      else if (data->token == ';' || data->token == '(' || data->token == ')'
	       || data->token == SGF_END)
	return;
      else if (data->token < 'a' || 'z' < data->token) {
	if (data->token != ' ' && data->token != '\n')
	  data->temp_buffer = data->buffer;

	next_token (data);
	break;
      }

      next_character (data);
    }
  }

  if (data->temp_buffer > data->buffer) {
    char *name = data->buffer;
    char *name_end = data->temp_buffer;
    SgfType property_type = 0;

    /* We have parsed some name.  Now look it up in the name tree. */
    while (1) {
      property_type = property_tree[property_type][1 + (*name - 'A')];
      name++;

      if (property_type < SGF_NUM_PROPERTIES) {
	if (name < name_end)
	  property_type = SGF_UNKNOWN;
	break;
      }

      property_type -= SGF_NUM_PROPERTIES;
      if (name == name_end) {
	property_type = property_tree[property_type][0];
	break;
      }
    }

    if (property_type != SGF_UNKNOWN) {
      data->property_type = property_type;

      /* Root and game-info nodes cannot appear anywhere. */
      if (SGF_FIRST_GAME_INFO_PROPERTY <= property_type
	  && property_type <= SGF_LAST_GAME_INFO_PROPERTY
	  && data->game_info_node != data->node
	  && data->game_info_node != NULL)
	add_error (data, SGF_ERROR_MISPLACED_GAME_INFO_PROPERTY);
      else if (SGF_FIRST_ROOT_PROPERTY <= property_type
	       && property_type <= SGF_LAST_ROOT_PROPERTY
	       && data->node->parent)
	add_error (data, SGF_ERROR_MISPLACED_ROOT_PROPERTY);
      else {
	SgfError error;

	error = property_info[property_type].value_parser (data);
	if (error > SGF_LAST_FATAL_ERROR) {
	  if (SGF_FIRST_GAME_INFO_PROPERTY <= property_type
	      && property_type <= SGF_LAST_GAME_INFO_PROPERTY)
	    data->game_info_node = data->node;

	  if (error != SGF_SUCCESS) {
	    add_error (data, error);
	    discard_single_value (data);
	  }

	  if (data->token != '[')
	    return;

	  add_error (data, SGF_ERROR_MULTIPLE_VALUES);
	}
	else {
	  if (error != SGF_FAIL)
	    add_error (data, error);
	}
      }
    }
    else {
      /* An unknown property.  We preserve it for it might be used by
       * some other program (and SGF specification requires us to do
       * so).  The property is preserved as a string list:
       *
       *   name (identifier), value, value ...
       */
      SgfProperty **link;

      if (!sgf_node_find_unknown_property (data->node, data->buffer,
					   name_end - data->buffer, &link)) {
	*link = sgf_property_new (data->tree, SGF_UNKNOWN, *link);
	(*link)->value.unknown_value_list = string_list_new ();
	string_list_add_from_buffer ((*link)->value.unknown_value_list,
				     data->buffer, name_end - data->buffer);
      }
      else {
	/* Duplicated unknown properties.  Assume list value type. */
	add_error (data, SGF_WARNING_UNKNOWN_PROPERTIES_MERGED, data->buffer);
      }

      parse_unknown_property_values (data, (*link)->value.unknown_value_list);
      return;
    }
  }

  discard_values (data);
}



/* Refresh a buffer by reading next portion of data from file.  Data
 * left from previous reading that has not been parsed yet is copied
 * to the buffer beginning.  Note that we reserve one extra byte at
 * the very beginning of the buffer for storing current token.
 */
static void
refresh_buffer (SgfParsingData *data)
{
  int unused_bytes = data->buffer_end - data->buffer_pointer;
  int bytes_to_read = data->buffer_size - (unused_bytes + 1);

  memcpy (data->buffer + 1, data->buffer_pointer, unused_bytes);

  if (data->file_bytes_remaining <= bytes_to_read) {
    bytes_to_read = data->file_bytes_remaining;
    data->buffer_end = (data->buffer + 1) + unused_bytes + bytes_to_read;
    data->buffer_refresh_point = data->buffer_end;
  }

  /* FIXME: add less severe handling of errors. */
  if (fread ((data->buffer + 1) + unused_bytes, bytes_to_read, 1,
	     data->file)
      != 1)
    assert (0);

  data->buffer_pointer	       = data->buffer + 1;
  data->file_bytes_remaining  -= bytes_to_read;
  data->buffer_offset_in_file += data->buffer_size - (unused_bytes + 1);
  *data->bytes_parsed	       = (data->buffer_offset_in_file
				  + (data->buffer_pointer - data->buffer));
}


/* Reallocate buffer to increase its size and read more data from
 * file.  This is an emergency function: it is only called if not a
 * single node started within `data->buffer_refresh_margin', which
 * must be very unlikely for sane SGF files.
 */
static void
expand_buffer (SgfParsingData *data)
{
  const char *original_buffer = data->buffer;
  int buffer_increase = data->buffer_size_increment;

  if (data->file_bytes_remaining <= 2 * buffer_increase)
    buffer_increase = data->file_bytes_remaining;

  data->buffer = utils_realloc ((char *) data->buffer,
				data->buffer_size + buffer_increase);
  data->buffer_pointer = data->buffer + data->buffer_size;

  /* FIXME: add less severe handling of errors. */
  if (fread ((char *) data->buffer + data->buffer_size,
	     buffer_increase, 1, data->file)
      != 1)
    assert (0);

  data->buffer_size		  += buffer_increase;
  data->buffer_end		   = data->buffer + data->buffer_size;
  data->temp_buffer		  += data->buffer - original_buffer;
  data->stored_buffer_pointers[0] += data->buffer - original_buffer;
  data->stored_buffer_pointers[1] += data->buffer - original_buffer;

  data->file_bytes_remaining -= buffer_increase;
  if (data->file_bytes_remaining > 0) {
    data->buffer_refresh_point = (data->buffer_end
				  - data->buffer_refresh_margin);
  }
  else
    data->buffer_refresh_point = data->buffer_end;

  *data->bytes_parsed = (data->buffer_offset_in_file
			 + (data->buffer_pointer - data->buffer));
}



static int
complete_node_and_update_board (SgfParsingData *data, int is_leaf_node)
{
  int num_undos = 0;
  int has_setup_add_properties = 0;
  BoardPositionList *position_lists[NUM_ON_GRID_VALUES];

  if (!data->first_setup_add_property
      && (data->has_setup_add_properties[BLACK]
	  || data->has_setup_add_properties[WHITE]
	  || data->has_setup_add_properties[EMPTY]
	  || data->has_setup_add_properties[SPECIAL_ON_GRID_VALUE])) {
    /* Create setup add properties position lists. */
    has_setup_add_properties = 1;
    create_position_lists (data, position_lists, NULL, NUM_ON_GRID_VALUES,
			   data->changed_positions, data->board_change_mark);
  }

  /* For root node there might be default setup. */
  if (!data->node->parent && !has_setup_add_properties && data->use_board
      && game_get_default_setup (data->game,
				 data->board_width, data->board_height,
				 &position_lists[BLACK],
				 &position_lists[WHITE])) {
    data->has_any_setup_property = 1;
    has_setup_add_properties = 1;

    position_lists[EMPTY] = NULL;
    position_lists[SPECIAL_ON_GRID_VALUE] = NULL;
  }

  if (has_setup_add_properties && data->use_board) {
    board_apply_changes (data->board,
			 (const BoardPositionList **) position_lists);
    num_undos++;
  }

  /* Amazons move are senseless if there is no piece of appropriate
   * color at "from" point.  Such moves are deleted.
   */
  if (data->game == GAME_AMAZONS
      && data->node->move_color != EMPTY
      && (data->board->grid[POINT_TO_POSITION (data->node->data.amazons.from)]
	  != data->node->move_color)) {
    insert_error (data, SGF_ERROR_SENSELESS_MOVE,
		  &data->move_error_position,
		  data->node->data.amazons.from.x,
		  data->node->data.amazons.from.y);
    data->node->move_color = EMPTY;
  }

  if (data->ko_property_error_position.line) {
    if (data->node->move_color == EMPTY) {
      insert_error (data, SGF_ERROR_KO_PROPERTY_WITHOUT_MOVE,
		    &data->ko_property_error_position);
    }

    data->ko_property_error_position.line = 0;
  }

  if (data->has_any_setup_property) {
    data->has_any_setup_property = 0;
    data->first_setup_add_property = 1;

    /* Nodes with both setup and move properties have to be split. */
    if (data->node->move_color != EMPTY
	|| sgf_node_find_property (data->node, SGF_MOVE_NUMBER, NULL)) {
      if (has_setup_add_properties
	  || (sgf_node_get_color_property_value (data->node, SGF_TO_PLAY)
	      == data->node->move_color)) {
	sgf_node_split (data->node, data->tree);
	insert_error (data, SGF_ERROR_MIXED_SETUP_ADD_AND_MOVE_PROPERTIES,
		      &data->node_error_position);
      }
      else {
	sgf_node_delete_property (data->node, data->tree, SGF_TO_PLAY);
	insert_error (data, SGF_ERROR_MIXED_PL_AND_MOVE_PROPERTIES,
		      &data->node_error_position);
      }
    }

    if (has_setup_add_properties) {
      /* Add setup add properties to the node. */
      if (position_lists[BLACK]) {
	sgf_node_add_list_of_point_property (data->node, data->tree,
					     SGF_ADD_BLACK,
					     position_lists[BLACK], 0);
      }

      if (position_lists[WHITE]) {
	sgf_node_add_list_of_point_property (data->node, data->tree,
					     SGF_ADD_WHITE,
					     position_lists[WHITE], 0);
      }

      if (position_lists[EMPTY]) {
	sgf_node_add_list_of_point_property (data->node, data->tree,
					     SGF_ADD_EMPTY,
					     position_lists[EMPTY], 0);
      }

      if (position_lists[SPECIAL_ON_GRID_VALUE]) {
	/* NOTE: might have to "if" further if we have more games.  */
	sgf_node_add_list_of_point_property (data->node, data->tree,
					     SGF_ADD_ARROWS,
					     position_lists[ARROW], 0);
      }

      data->node->move_color = SETUP_NODE;
    }

    if (data->node->child)
      data->node = data->node->child;
  }

  if (data->has_any_markup_property) {
    static const SgfType markup_property_types[NUM_SGF_MARKUPS]
      = { SGF_MARK, SGF_CIRCLE, SGF_SQUARE, SGF_TRIANGLE, SGF_SELECTED };

    data->has_any_markup_property = 0;
    data->first_markup_property = 1;

    create_position_lists (data, NULL, markup_property_types, NUM_SGF_MARKUPS,
			   data->marked_positions, data->board_markup_mark);
  }

  if (data->has_any_territory_property) {
    static const SgfType territory_property_types[NUM_COLORS]
      = { SGF_BLACK_TERRITORY, SGF_WHITE_TERRITORY };

    data->first_territory_property = 1;
    data->has_any_territory_property = 0;

    create_position_lists (data, NULL, territory_property_types, NUM_COLORS,
			   data->territory_positions,
			   data->board_territory_mark);
  }

  if (IS_STONE (data->node->move_color) && !is_leaf_node && data->use_board) {
    sgf_utils_play_node_move (data->node, data->board);
    num_undos++;
  }

  return num_undos;
}


static void
create_position_lists (SgfParsingData *data,
		       BoardPositionList **position_lists,
		       const SgfType *property_types, int num_properties,
		       const unsigned int marked_positions[BOARD_GRID_SIZE],
		       unsigned int current_mark)
{
  int positions[NUM_SGF_MARKUPS][BOARD_MAX_POSITIONS];
  int num_positions[NUM_SGF_MARKUPS];
  int value;
  int x;
  int y;

  for (value = 0; value < num_properties; value++)
    num_positions[value] = 0;

  for (y = 0; y < data->board_height; y++) {
    for (x = 0; x < data->board_width; x++) {
      int pos = POSITION (x, y);

      if (marked_positions[pos] >= current_mark) {
	value = marked_positions[pos] - current_mark;
	positions[value][num_positions[value]++] = pos;
      }
    }
  }

  for (value = 0; value < num_properties; value++) {
    if (num_positions[value] > 0) {
      BoardPositionList *position_list
	= board_position_list_new (positions[value], num_positions[value]);

      if (position_lists)
	position_lists[value] = position_list;
      else {
	sgf_node_add_list_of_point_property (data->node, data->tree,
					     property_types[value],
					     position_list, 0);
      }
    }
    else if (position_lists)
      position_lists[value] = NULL;
  }
}



/* Skip a value of a property without storing it anywhere. */
static void
discard_single_value (SgfParsingData *data)
{
  while (data->token != ']' && data->token != SGF_END) {
    if (data->token == '\\')
      next_character (data);
    next_character (data);
  }

  next_token (data);
}


/* Skip all values of a property without storing them.  Used when
 * something is wrong.
 */
static void
discard_values (SgfParsingData *data)
{
  do
    discard_single_value (data);
  while (data->token == '[');
}


/* Parse a value of an unknown property.  No assumptions about format
 * of the property are made besides that "\]" is considered an escaped
 * bracket and doesn't terminate value string.  Unknown properties are
 * always allowed to have a list of values.
 *
 * FIXME: Add charsets support.
 */
static void
parse_unknown_property_values (SgfParsingData *data,
			       StringList *property_value_list)
{
  do {
    data->temp_buffer = data->buffer;

    next_character (data);
    while (data->token != ']' && data->token != SGF_END) {
      *data->temp_buffer++ = data->token;
      if (data->token == '\\') {
	next_character (data);
	*data->temp_buffer++ = data->token;
      }

      next_character (data);
    }

    if (data->token == SGF_END)
      return;

    string_list_add_from_buffer (property_value_list,
				 data->buffer,
				 data->temp_buffer - data->buffer);
    next_token (data);
  } while (data->token == '[');
}



/* Parse value of "none" type.  Basically value validation only. */
SgfError
sgf_parse_none (SgfParsingData *data)
{
  SgfProperty **link;

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  if (data->property_type == SGF_KO)
    data->ko_property_error_position = data->property_name_error_position;

  *link = sgf_property_new (data->tree, data->property_type, *link);

  next_character (data);
  if (data->token == ']') {
    next_token (data);
    return SGF_SUCCESS;
  }

  return SGF_ERROR_NON_EMPTY_VALUE;
}


/* Parse a number.  This function is used instead of atoi() or
 * strtol() because we prefer not to create a string for the latter.
 * This function operates directly on buffer.  In addition, with
 * library functions, it would have been difficult to check for
 * errors.
 */
static int
do_parse_number (SgfParsingData *data, int *number)
{
  int negative = 0;
  unsigned value = 0;

  if (data->token == '-' || data->token == '+') {
    if (data->token == '-')
      negative = 1;

    next_token_in_value (data);
  }

  if ('0' <= data->token && data->token <= '9') {
    do {
      if (value < (UINT_MAX / 10U))
	value = value * 10 + (data->token - '0');
      else
	value = UINT_MAX;

      next_token_in_value (data);
    } while ('0' <= data->token && data->token <= '9');

    if (!negative) {
      if (value <= INT_MAX) {
	*number = value;
	return 1;
      }

      *number = INT_MAX;
    }
    else {
      if (value <= - (unsigned) INT_MIN) {
	*number = -value;
	return 1;
      }

      *number = INT_MIN;
    }

    return 2;
  }

  return 0;
}


/* Parse a number, but discard it as illegal if negative, or
 * non-positive (for `MN' property.)  For `PM' property, give a
 * warning if the print mode specified is not described in SGF
 * specification.
 */
SgfError
sgf_parse_constrained_number (SgfParsingData *data)
{
  SgfProperty **link;
  BufferPositionStorage storage;
  int number;

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  STORE_BUFFER_POSITION (data, 0, storage);

  if (do_parse_number (data, &number)
      && number >= (data->property_type != SGF_MOVE_NUMBER ? 0 : 1)) {
    if (data->property_type == SGF_PRINT_MODE && number >= NUM_SGF_PRINT_MODES)
      add_error (data, SGF_WARNING_UNKNOWN_PRINT_MODE, number);

    *link = sgf_property_new (data->tree, data->property_type, *link);
    (*link)->value.number = number;

    return end_parsing_value (data);
  }

  RESTORE_BUFFER_POSITION (data, 0, storage);
  return SGF_FATAL_INVALID_VALUE;
}


static int
do_parse_real (SgfParsingData *data, double *real)
{
  double value = 0.0;
  int negative = 0;

  if (data->token == '-' || data->token == '+') {
    if (data->token == '-')
      negative = 1;

    next_token_in_value (data);
  }

  if (('0' <= data->token && data->token <= '9') || data->token == '.') {
    SgfErrorPosition integer_part_error_position;

    if ('0' <= data->token && data->token <= '9') {
      do {
	value = value * 10 + (double) (data->token - '0');
	next_token_in_value (data);
      } while ('0' <= data->token && data->token <= '9');

      integer_part_error_position.line = 0;
    }
    else
      STORE_ERROR_POSITION (data, integer_part_error_position);

    if (data->token == '.') {
      next_token_in_value (data);
      if ('0' <= data->token && data->token <= '9') {
	double decimal_multiplier = 0.1;

	do {
	  value += decimal_multiplier * (double) (data->token - '0');
	  decimal_multiplier /= 10;
	  next_token_in_value (data);
	} while ('0' <= data->token && data->token <= '9');
      }
      else {
	if (integer_part_error_position.line)
	  return 0;
      }
    }

    if (integer_part_error_position.line) {
      insert_error (data, SGF_WARNING_INTEGER_PART_SUPPLIED,
		    &integer_part_error_position);
    }

    if (!negative) {
      /* OMG, wrote this with floats, but with doubles, I doubt it
       * makes any sense at all ;).  No point in deleting anyway,
       * right?
       */
      if (value <= 10e100) {
	*real = value;
	return 1;
      }

      *real = 10e100;
    }
    else {
      if (value >= -10e100) {
	*real = -value;
	return 1;
      }

      *real = -10e100;
    }

    add_error (data, SGF_ERROR_TOO_LARGE_ABSOLUTE_VALUE);
    return 2;
  }

  return 0;
}


SgfError
sgf_parse_real (SgfParsingData *data)
{
  SgfProperty **link;
  BufferPositionStorage storage;
  double real;

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  STORE_BUFFER_POSITION (data, 0, storage);

  if (do_parse_real (data, &real)) {
    *link = sgf_property_new (data->tree, data->property_type, *link);
    (*link)->value.real = utils_duplicate_buffer (&real, sizeof (double));

    return end_parsing_value (data);
  }

  RESTORE_BUFFER_POSITION (data, 0, storage);
  return SGF_FATAL_INVALID_VALUE;
}


/* Parse a "double" value.  Allowed values are [1] (normal) and [2]
 * (emphasized).  Anything else is considered an error.
 */
SgfError
sgf_parse_double (SgfParsingData *data)
{
  SgfProperty **link;

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  begin_parsing_value (data);

  *link = sgf_property_new (data->tree, data->property_type, *link);
  (*link)->value.emphasized = (data->token == '2');

  if (data->token == '1' || data->token == '2') {
    next_token_in_value (data);
    return end_parsing_value (data);
  }

  return SGF_ERROR_INVALID_DOUBLE_VALUE;
}


/* Parse color value.  [W] stands for white and [B] - for black.  No
 * other values are allowed except that [w] and [b] are upcased and
 * warned about.
 */
SgfError
sgf_parse_color (SgfParsingData *data)
{
  SgfProperty **link;

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  if (data->token == 'b' || data->token == 'w') {
    char original_color = data->token;

    data->token -= ('b' - 'B');
    add_error (data, SGF_WARNING_LOWER_CASE_COLOR,
	       original_color, data->token);
  }

  if (data->token == 'B' || data->token == 'W') {
    *link = sgf_property_new (data->tree, data->property_type, *link);
    (*link)->value.color = (data->token == 'B' ? BLACK : WHITE);

    /* `PL' is a setup property. */
    if (data->property_type == SGF_TO_PLAY)
      data->has_any_setup_property = 1;

    next_token_in_value (data);
    return end_parsing_value (data);
  }

  return SGF_FATAL_INVALID_VALUE;
}


/* Parse a simple text value.  All leading and trailing whitespace
 * characters are removed.  Other newlines, if encountered, are
 * converted into spaces.  Escaped newlines are removed completely.
 * Return either a heap copy of parsed values or NULL in case of error
 * (empty value).
 *
 * The `extra_stop_character' parameter should be set to either
 * SGF_END or color (':') depending on desired value terminator in
 * addition to ']'.
 */
static char *
do_parse_simple_text (SgfParsingData *data, char extra_stop_character)
{
  data->temp_buffer = data->buffer;

  /* Skip leading whitespace. */
  do {
    next_token (data);
    if (data->token == '\\') {
      next_character (data);
      if (data->token == ' ' || data->token == '\n')
	continue;

      *data->temp_buffer++ = data->token;
      next_character (data);
    }
  } while (0);

  if (data->temp_buffer != data->buffer
      || (data->token != ']' && data->token != extra_stop_character
	  && data->token != SGF_END)) {
    do {
      if (data->token != '\\' && data->token != '\n')
	*data->temp_buffer++ = data->token;
      else if (data->token == '\\') {
	next_character (data);
	if (data->token != '\n')
	  *data->temp_buffer++ = data->token;
      }
      else
	*data->temp_buffer++ = ' ';

      next_character (data);
    } while (data->token != ']' && data->token != extra_stop_character
	     && data->token != SGF_END);

    /* Delete trailing whitespace. */
    while (*(data->temp_buffer - 1) == ' ')
      data->temp_buffer--;

    return convert_text_to_utf8 (data, NULL);
  }

  return NULL;
}


static char *
do_parse_text (SgfParsingData *data, char *existing_text)
{
  next_character (data);

  data->temp_buffer = data->buffer;
  while (data->token != ']' && data->token != SGF_END) {
    if (data->token != '\\')
      *data->temp_buffer++ = data->token;
    else {
      next_character (data);
      if (data->token != '\n')
	*data->temp_buffer++ = data->token;
    }

    next_character (data);
  }

  while (1) {
    if (data->temp_buffer == data->buffer) {
      add_error (data, SGF_WARNING_EMPTY_VALUE);
      next_token (data);
      return existing_text;
    }

    if ((*(data->temp_buffer - 1) != ' ' && *(data->temp_buffer - 1) != '\n'))
      break;

    data->temp_buffer--;
  }

  next_token (data);

  if (existing_text)
    existing_text = utils_cat_as_string (existing_text, "\n\n", 2);

  return convert_text_to_utf8 (data, existing_text);
}


/* Convert text to UTF-8 encoding.  Text to be converted is bounded by
 * `data->buffer' and `data->temp_buffer' pointers.  Memory between
 * `data->temp_buffer' and `data->buffer_pointer' can be used as
 * conversion buffer and thus overwritten.
 *
 * Converted text is concatenated to `existing_text'.  If
 * `existing_text' is NULL to begin with, then converted text is
 * allocated in a new memory region on the heap.
 */
static char *
convert_text_to_utf8 (SgfParsingData *data, char *existing_text)
{
  if (data->tree_char_set_to_utf8) {
    char local_buffer[0x1000];
    char *original_text = data->buffer;
    size_t original_bytes_left = data->temp_buffer - data->buffer;
    char *utf8_buffer;
    size_t utf8_buffer_size;

    if (data->buffer_pointer - data->temp_buffer > sizeof local_buffer) {
      utf8_buffer = data->temp_buffer;
      utf8_buffer_size = data->buffer_pointer - data->temp_buffer;
    }
    else {
      utf8_buffer = local_buffer;
      utf8_buffer_size = sizeof local_buffer;
    }

    while (original_bytes_left > 0) {
      size_t utf8_bytes_left = utf8_buffer_size;
      char *utf8_text = utf8_buffer;

      iconv (data->tree_char_set_to_utf8,
	     &original_text, &original_bytes_left,
	     &utf8_text, &utf8_bytes_left);

      existing_text = utils_cat_as_string (existing_text, utf8_buffer,
					   utf8_text - utf8_buffer);
    }

    return existing_text;
  }
  else {
    /* The text is already in UTF-8, no conversion needed. */
    return utils_cat_as_string (existing_text, data->buffer,
				data->temp_buffer - data->buffer);
  }
}


/* Parse a simple text value, that is, a line of text. */
SgfError
sgf_parse_simple_text (SgfParsingData *data)
{
  SgfProperty **link;
  char *text;

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  text = do_parse_simple_text (data, SGF_END);
  if (text) {
    next_token (data);

    *link = sgf_property_new (data->tree, data->property_type, *link);
    (*link)->value.text = text;

    return SGF_SUCCESS;
  }

  return SGF_WARNING_PROPERTY_WITH_EMPTY_VALUE;
}


SgfError
sgf_parse_text (SgfParsingData *data)
{
  int property_found;
  SgfProperty **link;
  char *text;

  property_found = sgf_node_find_property (data->node, data->property_type,
					   &link);
  if (property_found) {
    add_error (data, SGF_WARNING_PROPERTIES_MERGED);
    text = do_parse_text (data, (*link)->value.text);
  }
  else
    text = do_parse_text (data, NULL);

  if (data->token == '[') {
    add_error (data, SGF_WARNING_VALUES_MERGED);

    do
      text = do_parse_text (data, text);
    while (data->token == '[');
  }

  if (!property_found) {
    if (!text) {
      /* Not really a success, but simpler this way. */
      return SGF_SUCCESS;
    }

    *link = sgf_property_new (data->tree, data->property_type, *link);
  }

  (*link)->value.text = text;
  return SGF_SUCCESS;
}


static int
do_parse_point (SgfParsingData *data, BoardPoint *point)
{
  if (('a' <= data->token && data->token <= 'z')
      || ('A' <= data->token && data->token <= 'Z')) {
    char x = data->token;

    next_token_in_value (data);

    if (('a' <= data->token && data->token <= 'z')
	|| ('A' <= data->token && data->token <= 'Z')) {
      char y = data->token;

      next_token_in_value (data);

      point->x = (x >= 'a' ? x - 'a' : x - 'A' + ('z' - 'a' + 1));
      point->y = (y >= 'a' ? y - 'a' : y - 'A' + ('z' - 'a' + 1));

      return (point->x < data->board_width && point->y < data->board_height
	      ? 0 : 1);
    }

    if ('1' <= data->token && data->token <= '9') {
      int y;

      do_parse_number (data, &y);

      point->x = (x >= 'a' ? x - 'a' : x - 'A');
      if (data->game == GAME_GO) {
	if (point->x == 'I' - 'A')
	  return 2;

	if (point->x > 'I' - 'A')
	  point->x--;
      }

      if (point->x < data->board_width && y <= data->board->height) {
	point->y = (data->game != GAME_OTHELLO
		    ? data->board->height - y : y - 1);
	if (!data->times_error_reported[SGF_WARNING_NON_SGF_POINT_NOTATION]
	    && !data->non_sgf_point_error_position.line) {
	  STORE_ERROR_POSITION (data, data->non_sgf_point_error_position);
	  data->non_sgf_point_x = x;
	  data->non_sgf_point_y = y;
	}

	return 0;
      }

      point->x = -1;
      return 1;
    }
  }

  return 2;
}


static SgfError
do_parse_go_move (SgfParsingData *data)
{
  BoardPoint *move_point = &data->node->move_point;

  if (data->token != ']') {
    BufferPositionStorage storage;

    STORE_BUFFER_POSITION (data, 0, storage);

    switch (do_parse_point (data, move_point)) {
    case 0:
      return SGF_SUCCESS;

    case 1:
      if (move_point->x != 19 || move_point->y != 19
	  || data->board_width > 19 || data->board_height > 19)
	return SGF_FATAL_MOVE_OUT_OF_BOARD;

      break;

    default:
      RESTORE_BUFFER_POSITION (data, 0, storage);
      return SGF_FATAL_INVALID_VALUE;
    }
  }

  move_point->x = PASS_X;
  move_point->y = PASS_Y;

  return SGF_SUCCESS;
}


static SgfError
do_parse_othello_move (SgfParsingData *data)
{
  if (data->token != ']') {
    BufferPositionStorage storage;

    STORE_BUFFER_POSITION (data, 0, storage);

    switch (do_parse_point (data, &data->node->move_point)) {
    case 0:
      return SGF_SUCCESS;

    case 1:
      return SGF_FATAL_MOVE_OUT_OF_BOARD;

    default:
      RESTORE_BUFFER_POSITION (data, 0, storage);
      return SGF_FATAL_INVALID_VALUE;
    }
  }

  return SGF_FATAL_EMPTY_VALUE;
}


static SgfError
do_parse_amazons_move (SgfParsingData *data)
{
  if (data->token != ']') {
    int point_parsing_error;
    BufferPositionStorage storage;

    STORE_BUFFER_POSITION (data, 0, storage);

    point_parsing_error = do_parse_point (data,
					  &data->node->data.amazons.from);
    if (point_parsing_error == 0) {
      point_parsing_error = do_parse_point (data, &data->node->move_point);
      if (point_parsing_error == 0) {
	point_parsing_error
	  = do_parse_point (data, &data->node->data.amazons.shoot_arrow_to);

	if (point_parsing_error == 0)
	  return SGF_SUCCESS;
      }
    }

    if (point_parsing_error == 1)
      return SGF_FATAL_MOVE_OUT_OF_BOARD;

    RESTORE_BUFFER_POSITION (data, 0, storage);
    return SGF_FATAL_INVALID_VALUE;
  }

  return SGF_FATAL_EMPTY_VALUE;
}


SgfError
sgf_parse_move (SgfParsingData *data)
{
  SgfError error;

  begin_parsing_value (data);

  if (data->node->move_color == EMPTY) {
    error = data->do_parse_move (data);
    if (error == SGF_SUCCESS) {
      data->node->move_color = (data->property_type == SGF_BLACK
				? BLACK : WHITE);
      if (data->game == GAME_AMAZONS)
	data->move_error_position = data->property_name_error_position;

      return end_parsing_value (data);
    }
  }
  else {
    SgfNode *current_node = data->node;
    SgfNode *node = sgf_node_new (data->tree, NULL);

    data->node = node;
    error = data->do_parse_move (data);
    data->node = current_node;

    if (error == SGF_SUCCESS) {
      int num_undos;

      add_error (data, SGF_ERROR_ANOTHER_MOVE);

      num_undos = complete_node_and_update_board (data, 0);

      node->move_color = (data->property_type == SGF_BLACK ? BLACK : WHITE);
      node->parent = data->node;
      data->node->child = node;

      if (end_parsing_value (data) == SGF_ERROR_ILLEGAL_TRAILING_CHARACTERS)
	add_error (data, SGF_ERROR_ILLEGAL_TRAILING_CHARACTERS);

      if (data->token == '[') {
	add_error (data, SGF_ERROR_MULTIPLE_VALUES);
	discard_values (data);
      }

      STORE_ERROR_POSITION (data, data->node_error_position);
      if (data->game == GAME_AMAZONS)
	data->move_error_position = data->property_name_error_position;
      parse_node_sequence (data, node);

      board_undo (data->board, num_undos);

      return SGF_SUCCESS;
    }
    else
      sgf_node_delete (node, data->tree);
  }

  data->non_sgf_point_error_position.line = 0;
  return error;
}


static int
do_parse_point_or_rectangle (SgfParsingData *data,
			     BoardPoint *left_top, BoardPoint *right_bottom)
{
  int point_parsing_error;
  BufferPositionStorage storage;

  begin_parsing_value (data);
  if (data->token == ']') {
    add_error (data, SGF_WARNING_EMPTY_VALUE);
    next_token (data);

    return 0;
  }

  STORE_BUFFER_POSITION (data, 0, storage);

  point_parsing_error = do_parse_point (data, left_top);
  if (point_parsing_error == 0) {
    if (!is_composed_value (data, 0))
      *right_bottom = *left_top;
    else {
      point_parsing_error = do_parse_point (data, right_bottom);
      if (point_parsing_error == 0) {
	if (left_top->x > right_bottom->x || left_top->y > right_bottom->y) {
	  add_error (data, SGF_ERROR_INVALID_CORNERS);

	  if (left_top->x > right_bottom->x) {
	    int temp_x = left_top->x;
	    left_top->x = right_bottom->x;
	    right_bottom->x = temp_x;
	  }

	  if (left_top->y > right_bottom->y) {
	    int temp_y = left_top->y;
	    left_top->y = right_bottom->y;
	    right_bottom->y = temp_y;
	  }
	}
	else if (left_top->x == right_bottom->x
		 && left_top->y == right_bottom->y)
	  add_error (data, SGF_WARNING_POINT_AS_RECTANGLE);
      }
    }
  }

  switch (point_parsing_error) {
  case 0:
    if (end_parsing_value (data) == SGF_ERROR_ILLEGAL_TRAILING_CHARACTERS) {
      add_error (data, SGF_ERROR_ILLEGAL_TRAILING_CHARACTERS);
      next_token (data);
    }

    return 1;

  case 1:
    add_error (data, SGF_ERROR_POINT_OUT_OF_BOARD);
    discard_single_value (data);
    break;

  default:
    RESTORE_BUFFER_POSITION (data, 0, storage);
    add_error (data, SGF_ERROR_INVALID_VALUE);
    next_token (data);
  }

  data->non_sgf_point_error_position.line = 0;
  return 0;
}


static int
do_parse_list_of_point (SgfParsingData *data,
			unsigned int marked_positions[BOARD_GRID_SIZE],
			unsigned int current_mark, int value,
			SgfError duplication_error,
			const char match_grid[BOARD_GRID_SIZE])
{
  int num_parsed_positions = 0;

  do {
    BoardPoint left_top;
    BoardPoint right_bottom;

    if (do_parse_point_or_rectangle (data, &left_top, &right_bottom)) {
      int x;
      int y;

      for (y = left_top.y; y <= right_bottom.y; y++) {
	for (x = left_top.x; x <= right_bottom.x; x++) {
	  int pos = POSITION (x, y);

	  if (marked_positions[pos] < current_mark) {
	    if (!match_grid || match_grid[pos] != value) {
	      marked_positions[pos] = current_mark + value;
	      num_parsed_positions++;
	    }
	    else
	      add_error (data, SGF_WARNING_SETUP_HAS_NO_EFFECT, x, y);
	  }
	  else {
	    if (marked_positions[pos] == current_mark + value)
	      add_error (data, SGF_WARNING_DUPLICATE_POINT, x, y);
	    else
	      add_error (data, duplication_error, x, y);
	  }
	}
      }
    }
  } while (data->token == '[');

  return num_parsed_positions;
}


SgfError
sgf_parse_list_of_point (SgfParsingData *data)
{
  SgfProperty **link;
  int num_positions = 0;

  if (data->property_type == SGF_ILLEGAL_MOVE && data->game != GAME_GO) {
    add_error (data, SGF_ERROR_WRONG_GAME,
	       game_info[GAME_GO].name, game_info[data->game].name);
    return SGF_FAIL;
  }

  data->board_common_mark++;
  if (sgf_node_find_property (data->node, data->property_type, &link)) {
    int k;
    BoardPositionList *position_list = (*link)->value.position_list;

    num_positions = position_list->num_positions;
    for (k = 0; k < num_positions; k++) {
      data->common_marked_positions[position_list->positions[k]]
	= data->board_common_mark;
    }

    sgf_property_delete_at_link (link, data->tree);
    add_error (data, SGF_WARNING_PROPERTIES_MERGED);
  }

  if (property_info[data->property_type].value_type == SGF_ELIST_OF_POINT) {
    BufferPositionStorage storage;

    STORE_BUFFER_POSITION (data, 0, storage);

    begin_parsing_value (data);
    if (data->token == ']') {
      end_parsing_value (data);
      if (data->token == '[') {
	add_error (data, SGF_ERROR_VALUES_AFTER_EMPTY_LIST);
	discard_values (data);
      }
    }
    else
      RESTORE_BUFFER_POSITION (data, 0, storage);
  }

  if (data->token == '[') {
    num_positions += do_parse_list_of_point (data,
					     data->common_marked_positions,
					     data->board_common_mark, 0,
					     SGF_FAIL, NULL);
  }

  if (num_positions > 0 ||
      property_info[data->property_type].value_type == SGF_ELIST_OF_POINT) {
    int k;
    int x;
    int y;

    *link = sgf_property_new (data->tree, data->property_type, *link);
    (*link)->value.position_list
      = board_position_list_new_empty (num_positions);

    for (y = 0, k = 0; k < num_positions; y++) {
      for (x = 0; x < data->board_width; x++) {
	if (data->common_marked_positions[POSITION (x, y)]
	    == data->board_common_mark)
	  (*link)->value.position_list->positions[k++] = POSITION (x, y);
      }
    }
  }

  return SGF_SUCCESS;
}


SgfError
sgf_parse_list_of_vector (SgfParsingData *data)
{
  int property_found;
  SgfProperty **link;
  SgfVectorList *vector_list;

  property_found = sgf_node_find_property (data->node, data->property_type,
					   &link);
  if (!property_found)
    vector_list = sgf_vector_list_new (-1);
  else {
    add_error (data, SGF_WARNING_PROPERTIES_MERGED);
    vector_list = (*link)->value.vector_list;
  }

  do {
    int point_parsing_error;
    BufferPositionStorage storage;
    BoardPoint from_point;
    BoardPoint to_point;

    begin_parsing_value (data);
    if (data->token == ']') {
      add_error (data, SGF_WARNING_EMPTY_VALUE);
      next_token (data);

      continue;
    }

    STORE_BUFFER_POSITION (data, 0, storage);

    point_parsing_error = do_parse_point (data, &from_point);
    if (point_parsing_error == 0) {
      if (is_composed_value (data, 0))
	point_parsing_error = do_parse_point (data, &to_point);
      else
	point_parsing_error = -1;
    }

    switch (point_parsing_error) {
    case 0:
      if (from_point.x != to_point.x || from_point.y != to_point.y) {
	if (!sgf_vector_list_has_vector (vector_list, from_point, to_point)) {
	  if (end_parsing_value (data)
	      == SGF_ERROR_ILLEGAL_TRAILING_CHARACTERS) {
	    add_error (data, SGF_ERROR_ILLEGAL_TRAILING_CHARACTERS);
	    next_token (data);
	  }

	  vector_list = sgf_vector_list_add_vector (vector_list,
						    from_point, to_point);
	  continue;
	}
	else {
	  add_error (data, SGF_WARNING_DUPLICATE_VECTOR,
		     from_point.x, from_point.y, to_point.x, to_point.y);
	}
      }
      else {
	add_error (data, SGF_ERROR_ZERO_LENGTH_VECTOR,
		   from_point.x, from_point.y, to_point.x, to_point.y);
      }

      discard_single_value (data);
      break;

    case 1:
      add_error (data, SGF_ERROR_POINT_OUT_OF_BOARD);
      discard_single_value (data);
      break;

    default:
      RESTORE_BUFFER_POSITION (data, 0, storage);
      add_error (data, SGF_ERROR_INVALID_VALUE);
      next_token (data);
    }

    data->non_sgf_point_error_position.line = 0;
  } while (data->token == '[');

  if (vector_list->num_vectors > 0) {
    if (!property_found)
      *link = sgf_property_new (data->tree, data->property_type, *link);

    (*link)->value.vector_list = sgf_vector_list_shrink (vector_list);
  }

  return SGF_SUCCESS;
}


SgfError
sgf_parse_list_of_label (SgfParsingData *data)
{
  SgfProperty **link;
  char *labels[BOARD_GRID_SIZE];
  int num_labels = 0;

  data->board_common_mark++;
  if (sgf_node_find_property (data->node, data->property_type, &link)) {
    int k;
    SgfLabelList *label_list = (*link)->value.label_list;

    num_labels = label_list->num_labels;
    for (k = 0; k < num_labels; k++) {
      int pos = POINT_TO_POSITION (label_list->labels[k].point);

      labels[pos] = label_list->labels[k].text;
      label_list->labels[k].text = NULL;

      data->common_marked_positions[pos] = data->board_common_mark;
    }

    sgf_property_delete_at_link (link, data->tree);
    add_error (data, SGF_WARNING_PROPERTIES_MERGED);
  }

  do {
    begin_parsing_value (data);
    if (data->token != ']') {
      BufferPositionStorage storage;
      BoardPoint point;
      int pos;

      STORE_BUFFER_POSITION (data, 0, storage);

      switch (do_parse_point (data, &point)) {
      case 0:
	pos = POINT_TO_POSITION (point);
	if (data->common_marked_positions[pos] != data->board_common_mark) {
	  if (is_composed_value (data, 1)) {
	    labels[pos] = do_parse_simple_text (data, SGF_END);
	    if (labels[pos]) {
	      data->common_marked_positions[pos] = data->board_common_mark;
	      num_labels++;
	    }
	    else
	      add_error (data, SGF_WARNING_EMPTY_LABEL, point.x, point.y);
	  }
	  else
	    add_error (data, SGF_WARNING_EMPTY_LABEL, point.x, point.y);

	  if (end_parsing_value (data)
	      == SGF_ERROR_ILLEGAL_TRAILING_CHARACTERS) {
	    add_error (data, SGF_ERROR_ILLEGAL_TRAILING_CHARACTERS);
	    next_token (data);
	  }

	  continue;
	}

	add_error (data, SGF_ERROR_DUPLICATE_LABEL, point.x, point.y);
	discard_single_value (data);

	data->non_sgf_point_error_position.line = 0;
	continue;

      case 1:
	add_error (data, SGF_ERROR_POINT_OUT_OF_BOARD);
	discard_single_value (data);
	continue;
      }

      RESTORE_BUFFER_POSITION (data, 0, storage);
      add_error (data, SGF_ERROR_INVALID_VALUE);

      data->non_sgf_point_error_position.line = 0;
    }
    else
      add_error (data, SGF_WARNING_EMPTY_VALUE);

    next_token (data);
  } while (data->token == '[');

  if (num_labels > 0) {
    SgfLabelList *label_list = sgf_label_list_new_empty (num_labels);
    int k;
    int x;
    int y;

    *link = sgf_property_new (data->tree, data->property_type, *link);
    (*link)->value.label_list = label_list;

    for (y = 0, k = 0; k < num_labels; y++) {
      for (x = 0; x < data->board_width; x++) {
	if (data->common_marked_positions[POSITION (x, y)]
	    == data->board_common_mark) {
	  label_list->labels[k].point.x = x;
	  label_list->labels[k].point.y = y;
	  label_list->labels[k].text = labels[POSITION (x, y)];
	  k++;
	}
      }
    }
  }

  return SGF_SUCCESS;
}


/* Parse a value of `AP' property (composed simpletext ":"
 * simpletext).  The value is stored in SgfGameTree structure.
 */
SgfError
sgf_parse_application (SgfParsingData *data)
{
  char *text;

  if (data->tree->application_name)
    return SGF_FATAL_DUPLICATE_PROPERTY;

  /* Parse the first part of value. */
  text = do_parse_simple_text (data, ':');
  if (text) {
    data->tree->application_name = text;

    if (data->token == ':') {
      /* Parse the second part of value. */
      data->tree->application_version = do_parse_simple_text (data, SGF_END);
    }
    else if (data->token == ']')
      add_error (data, SGF_WARNING_COMPOSED_SIMPLE_TEXT_EXPECTED);

    next_token (data);
    return SGF_SUCCESS;
  }

  return SGF_WARNING_PROPERTY_WITH_EMPTY_VALUE;
}


static int
do_parse_board_size (SgfParsingData *data, int *width, int *height,
		     int add_errors)
{
  BufferPositionStorage storage;

  STORE_BUFFER_POSITION (data, 0, storage);

  if (!do_parse_number (data, width) || *width < SGF_MIN_BOARD_SIZE) {
    if (add_errors) {
      RESTORE_BUFFER_POSITION (data, 0, storage);
      add_error (data, SGF_FATAL_INVALID_VALUE);
    }

    return 0;
  }

  if (is_composed_value (data, 0)) {
    if (!do_parse_number (data, height) || *height < SGF_MIN_BOARD_SIZE) {
      if (add_errors) {
	RESTORE_BUFFER_POSITION (data, 0, storage);
	add_error (data, SGF_FATAL_INVALID_VALUE);
      }

      return 0;
    }

    if (*height == *width && add_errors)
      add_error (data, SGF_WARNING_SAME_WIDTH_AND_HEIGHT);
  }
  else
    *height = *width;

  if (*width > SGF_MAX_BOARD_SIZE || *height > SGF_MAX_BOARD_SIZE) {
    int original_width = *width;
    int original_height = *height;

    if (*height > SGF_MAX_BOARD_SIZE)
      *height = SGF_MAX_BOARD_SIZE;

    if (*width > SGF_MAX_BOARD_SIZE)
      *width = SGF_MAX_BOARD_SIZE;

    if (add_errors) {
      add_error (data, SGF_ERROR_TOO_LARGE_BOARD,
		 original_width, original_height, *width, *height);
    }
  }

  return 1;
}


SgfError
sgf_parse_board_size (SgfParsingData *data)
{
  SgfProperty **link;
  int width;
  int height;

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  if (do_parse_board_size (data, &width, &height, 1))
    return end_parsing_value (data);

  return SGF_FAIL;
}


/* FIXME: write this function. */
SgfError
sgf_parse_char_set (SgfParsingData *data)
{
  discard_values (data);
  return SGF_SUCCESS;
}


/* FIXME: write this function. */
SgfError
sgf_parse_date (SgfParsingData *data)
{
  return sgf_parse_simple_text (data);
}


SgfError
sgf_parse_figure (SgfParsingData *data)
{
  SgfProperty **link;
  BufferPositionStorage storage;
  int figure_flags = SGF_FIGURE_USE_DEFAULTS;
  int flags_parsed;
  char *diagram_name = NULL;

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  *link = sgf_property_new (data->tree, data->property_type, *link);

  begin_parsing_value (data);
  if (data->token == ']') {
    (*link)->value.figure = NULL;
    return SGF_SUCCESS;
  }

  STORE_BUFFER_POSITION (data, 0, storage);

  flags_parsed = do_parse_number (data, &figure_flags);
  if (flags_parsed) {
    if (figure_flags & ~SGF_FIGURE_FLAGS_MASK)
      add_error (data, SGF_WARNING_UNKNOWN_FLAGS);

    if (is_composed_value (data, 1))
      diagram_name = do_parse_simple_text (data, SGF_END);
    else
      add_error (data, SGF_WARNING_COMPOSED_SIMPLE_TEXT_EXPECTED);
  }
  else {
    add_error (data, SGF_ERROR_FIGURE_FLAGS_EXPECTED);

    RESTORE_BUFFER_POSITION (data, 0, storage);
    diagram_name = do_parse_simple_text (data, SGF_END);
  }

  (*link)->value.figure = sgf_figure_description_new (figure_flags,
						      diagram_name);

  return end_parsing_value (data);
}


SgfError
sgf_parse_file_format (SgfParsingData *data)
{
  BufferPositionStorage storage;
  int file_format;

  if (data->tree->file_format)
    return SGF_FATAL_DUPLICATE_PROPERTY;

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  STORE_BUFFER_POSITION (data, 0, storage);

  if (do_parse_number (data, &file_format)
      && 1 <= file_format && file_format <= 1000) {
    if (file_format > 4)
      add_error (data, SGF_CRITICAL_FUTURE_FILE_FORMAT, file_format);

    data->tree->file_format = file_format;
    return end_parsing_value (data);
  }

  RESTORE_BUFFER_POSITION (data, 0, storage);
  return SGF_FATAL_INVALID_VALUE;
}


/* Parse a `GM' property.  Note that this property is not stored, this
 * function only validates it.  Property value is actually parsed by
 * parse_root() and is kept in SgfGameTree structure.
 */
SgfError
sgf_parse_game_type (SgfParsingData *data)
{
  BufferPositionStorage storage;
  int game_type;

  if (!data->game_type_expected)
    return SGF_FATAL_DUPLICATE_PROPERTY;

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  STORE_BUFFER_POSITION (data, 0, storage);

  if (do_parse_number (data, &game_type)
      && 1 <= game_type && game_type <= 1000) {
    if (game_type > LAST_GAME)
      add_error (data, SGF_WARNING_UNKNOWN_GAME, game_type);
    else if (!GAME_IS_SUPPORTED (game_type)) {
      add_error (data, SGF_WARNING_UNSUPPORTED_GAME,
		 game_info[game_type].name, game_type);
    }

    data->game_type_expected = 0;
    return end_parsing_value (data);
  }

  RESTORE_BUFFER_POSITION (data, 0, storage);
  return (data->game_type_expected
	  ? SGF_FATAL_INVALID_VALUE : SGF_FATAL_INVALID_GAME_TYPE);
}


SgfError
sgf_parse_handicap (SgfParsingData *data)
{
  SgfProperty **link;
  BufferPositionStorage storage;
  int handicap;

  if (data->game != GAME_GO) {
    add_error (data, SGF_ERROR_WRONG_GAME,
	       game_info[GAME_GO].name, game_info[data->game].name);
    return SGF_FAIL;
  }

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  STORE_BUFFER_POSITION (data, 0, storage);

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  *link = sgf_property_new (data->tree, data->property_type, *link);

  if (do_parse_number (data, &handicap) && data->token == ']') {
    if (handicap > data->board_width * data->board_height) {
      handicap = data->board_width * data->board_height;
      add_error (data, SGF_WARNING_HANDICAP_REDUCED, handicap);
    }
    else if (handicap == 1 || handicap < 0) {
      handicap = 0;
      add_error (data, SGF_ERROR_INVALID_HANDICAP);
    }

    (*link)->value.text = utils_cprintf ("%d", handicap);
    return end_parsing_value (data);
  }

  return invalid_game_info_property (data, *link, &storage);
}


SgfError
sgf_parse_komi (SgfParsingData *data)
{
  SgfProperty **link;
  BufferPositionStorage storage;
  double komi;

  if (data->game != GAME_GO) {
    add_error (data, SGF_ERROR_WRONG_GAME,
	       game_info[GAME_GO].name, game_info[data->game].name);
    return SGF_FAIL;
  }

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  STORE_BUFFER_POSITION (data, 0, storage);

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  *link = sgf_property_new (data->tree, data->property_type, *link);

  if (do_parse_real (data, &komi) && data->token == ']') {
    (*link)->value.text = utils_cprintf ("%.f", komi);
    return end_parsing_value (data);
  }

  return invalid_game_info_property (data, *link, &storage);
}


SgfError
sgf_parse_markup_property (SgfParsingData *data)
{
  int markup;

  if (data->first_markup_property) {
    for (markup = 0; markup < NUM_SGF_MARKUPS; markup++)
      data->has_markup_properties[markup] = 0;

    data->first_markup_property = 0;
    data->board_markup_mark += NUM_SGF_MARKUPS;
  }

  if (data->property_type == SGF_MARK)
    markup = SGF_MARKUP_CROSS;
  else if (data->property_type == SGF_CIRCLE)
    markup = SGF_MARKUP_CIRCLE;
  else if (data->property_type == SGF_SQUARE)
    markup = SGF_MARKUP_SQUARE;
  else if (data->property_type == SGF_TRIANGLE)
    markup = SGF_MARKUP_TRIANGLE;
  else if (data->property_type == SGF_SELECTED)
    markup = SGF_MARKUP_SELECTED;
  else
    assert (0);

  if (data->has_markup_properties[markup])
    add_error (data, SGF_WARNING_PROPERTIES_MERGED);

  if (do_parse_list_of_point (data, data->marked_positions,
			      data->board_markup_mark, markup,
			      SGF_ERROR_DUPLICATE_MARKUP, NULL)) {
    data->has_any_markup_property = 1;
    data->has_markup_properties[markup] = 1;
  }

  return SGF_SUCCESS;
}


SgfError
sgf_parse_result (SgfParsingData *data)
{
  SgfProperty **link;
  BufferPositionStorage storage;

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  STORE_BUFFER_POSITION (data, 0, storage);

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  *link = sgf_property_new (data->tree, data->property_type, *link);

  if (data->token == 'B' || data->token == 'W') {
    char color = data->token;

    next_token_in_value (data);
    if (data->token == '+') {
      next_token_in_value (data);
      if (('0' <= data->token && data->token <= '9') || data->token == '.') {
	double score;

	if (do_parse_real (data, &score) && data->token == ']') {
	  (*link)->value.text = utils_cprintf ("%c+%.f", color, score);
	  return end_parsing_value (data);
	}
      }
      else {
	static const char *non_score_results[6]
	  = { "F", "Forfeit", "R", "Resign", "T", "Time" };
	int result_index = looking_at (data, non_score_results, 6);

	if (result_index != -1) {
	  /* Use full-word reasons internally. */
	  (*link)->value.text
	    = utils_cprintf ("%c+%s",
			     color, non_score_results[result_index | 1]);

	  return end_parsing_value (data);
	}
      }
    }
  }
  else {
    static const char *no_winner_results[4] = { "0", "?", "Draw", "Void" };
    int result_index = looking_at (data, no_winner_results, 4);

    if (result_index != -1) {
      /* Prefer "Draw" to "0". */
      (*link)->value.text
	= utils_duplicate_string (no_winner_results[result_index != 0
						    ? result_index : 2]);

      return end_parsing_value (data);
    }
  }

  return invalid_game_info_property (data, *link, &storage);
}


SgfError
sgf_parse_setup_property (SgfParsingData *data)
{
  int color;

  if (data->first_setup_add_property) {
    for (color = 0; color < NUM_ON_GRID_VALUES; color++)
      data->has_setup_add_properties[color] = 0;

    data->first_setup_add_property = 0;
    data->board_change_mark += NUM_ON_GRID_VALUES;
  }

  if (data->property_type == SGF_ADD_BLACK)
    color = BLACK;
  else if (data->property_type == SGF_ADD_WHITE)
    color = WHITE;
  else if (data->property_type == SGF_ADD_EMPTY)
    color = EMPTY;
  else if (data->property_type == SGF_ADD_ARROWS) {
    if (data->game != GAME_AMAZONS) {
      add_error (data, SGF_ERROR_WRONG_GAME,
		 game_info[GAME_AMAZONS].name, game_info[data->game].name);
      return SGF_FAIL;
    }

    color = ARROW;
  }
  else
    assert (0);

  if (data->has_setup_add_properties[color])
    add_error (data, SGF_WARNING_PROPERTIES_MERGED);

  if (do_parse_list_of_point (data, data->changed_positions,
			      data->board_change_mark, color,
			      SGF_ERROR_DUPLICATE_SETUP, data->board->grid)) {
    data->has_any_setup_property = 1;
    data->has_setup_add_properties[color] = 1;
  }

  return SGF_SUCCESS;
}


SgfError
sgf_parse_style (SgfParsingData *data)
{
  BufferPositionStorage storage;

  if (data->tree->style_is_set)
    return SGF_FATAL_DUPLICATE_PROPERTY;

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  STORE_BUFFER_POSITION (data, 0, storage);

  if (do_parse_number (data, &data->tree->style)) {
    data->tree->style_is_set = 1;

    if (data->tree->style & ~SGF_STYLE_FLAGS_MASK)
      add_error (data, SGF_WARNING_UNKNOWN_FLAGS);

    return end_parsing_value (data);
  }

  RESTORE_BUFFER_POSITION (data, 0, storage);
  return SGF_FATAL_INVALID_VALUE;
}


SgfError
sgf_parse_territory (SgfParsingData *data)
{
  int color_index;

  if (data->game != GAME_GO) {
    add_error (data, SGF_ERROR_WRONG_GAME,
	       game_info[GAME_GO].name, game_info[data->game].name);
    return SGF_FAIL;
  }

  if (data->first_territory_property) {
    data->has_territory_properties[BLACK_INDEX] = 0;
    data->has_territory_properties[WHITE_INDEX] = 0;

    data->first_territory_property = 0;
    data->board_territory_mark += NUM_COLORS;
  }

  if (data->property_type == SGF_BLACK_TERRITORY)
    color_index = BLACK_INDEX;
  else if (data->property_type == SGF_WHITE_TERRITORY)
    color_index = WHITE_INDEX;
  else
    assert (0);

  if (data->has_territory_properties[color_index])
    add_error (data, SGF_WARNING_PROPERTIES_MERGED);

  if (do_parse_list_of_point (data, data->territory_positions,
			      data->board_territory_mark, color_index,
			      SGF_ERROR_DUPLICATE_TERRITORY, NULL)) {
    data->has_any_territory_property = 1;
    data->has_territory_properties[color_index] = 1;
  }

  return SGF_SUCCESS;
}


SgfError
sgf_parse_time_limit (SgfParsingData *data)
{
  SgfProperty **link;
  BufferPositionStorage storage;
  double time_limit;

  if (sgf_node_find_property (data->node, data->property_type, &link))
    return SGF_FATAL_DUPLICATE_PROPERTY;

  STORE_BUFFER_POSITION (data, 0, storage);

  begin_parsing_value (data);
  if (data->token == ']')
    return SGF_FATAL_EMPTY_VALUE;

  if (do_parse_real (data, &time_limit) && data->token == ']') {
    if (time_limit < 0.0) {
      RESTORE_BUFFER_POSITION (data, 0, storage);
      next_character (data);

      return SGF_FATAL_NEGATIVE_TIME_LIMIT;
    }

    *link = sgf_property_new (data->tree, data->property_type, *link);
    (*link)->value.text = utils_cprintf ("%.f", time_limit);

    return end_parsing_value (data);
  }

  *link = sgf_property_new (data->tree, data->property_type, *link);
  return invalid_game_info_property (data, *link, &storage);
}


/* FIXME: write this function. */
SgfError
sgf_parse_letters (SgfParsingData *data)
{
  begin_parsing_value (data);
  while (data->token != ']') next_token (data);
  return end_parsing_value (data);
}


/* FIXME: write this function. */
SgfError
sgf_parse_simple_markup (SgfParsingData *data)
{
  begin_parsing_value (data);
  while (data->token != ']') next_token (data);
  return end_parsing_value (data);
}


static int
looking_at (SgfParsingData *data, const char **strings, int num_strings)
{
  int first_candidate = 0;
  int last_candidate = num_strings;
  int character_index;

  for (character_index = 0; first_candidate < last_candidate;
       character_index++) {
    int k;

    if (data->token == ']')
      return strings[first_candidate][character_index] ? -1 : first_candidate;

    for (k = first_candidate; k < last_candidate; k++) {
      if (strings[k][character_index] < data->token)
	first_candidate++;
      else if (strings[k][character_index] > data->token) {
	last_candidate = k;
	break;
      }
    }

    next_token_in_value (data);
  }

  return -1;
}



static void
add_error (SgfParsingData *data, SgfError error, ...)
{
  va_list arguments;

  if (error < SGF_FIRST_PROPERTY_NAME_ERROR
      || SGF_LAST_PROPERTY_NAME_ERROR < error) {
    char buffer[MAX_ERROR_LENGTH];
    int length;

    if (error != SGF_WARNING_ERROR_SUPPRESSED
	&& data->times_error_reported[error] == MAX_TIMES_TO_REPORT_ERROR)
      return;

    va_start (arguments, error);
    length = format_error_valist (data, buffer, error, arguments);
    va_end (arguments);

    string_list_add_from_buffer (data->error_list, buffer, length);
    data->error_list->last->line = data->line;
    data->error_list->last->column = data->column + data->first_column;

    if (error != SGF_WARNING_ERROR_SUPPRESSED
	&& ++data->times_error_reported[error] == MAX_TIMES_TO_REPORT_ERROR)
      add_error (data, SGF_WARNING_ERROR_SUPPRESSED);
  }
  else {
    va_start (arguments, error);
    insert_error_valist (data, error, &data->property_name_error_position,
			 arguments);
    va_end (arguments);
  }
}


static void
insert_error (SgfParsingData *data, SgfError error,
	      SgfErrorPosition *error_position, ...)
{
  va_list arguments;

  va_start (arguments, error_position);
  insert_error_valist (data, error, error_position, arguments);
  va_end (arguments);
}


static void
insert_error_valist (SgfParsingData *data, SgfError error,
		     SgfErrorPosition *error_position, va_list arguments)
{
  SgfErrorListItem *error_item;
  char buffer[MAX_ERROR_LENGTH];
  int length;

  if (error != SGF_WARNING_ERROR_SUPPRESSED
      && data->times_error_reported[error] == MAX_TIMES_TO_REPORT_ERROR)
    return;

  length = format_error_valist (data, buffer, error, arguments);

  error_item = string_list_insert_from_buffer (data->error_list,
					       error_position->notch,
					       buffer, length);
  error_item->line   = error_position->line;
  error_item->column = error_position->column + data->first_column;

  if (error != SGF_WARNING_ERROR_SUPPRESSED
      && ++data->times_error_reported[error] == MAX_TIMES_TO_REPORT_ERROR) {
    error_position->notch = error_position->notch->next;
    insert_error (data, SGF_WARNING_ERROR_SUPPRESSED, error_position);
  }
}


static SgfError
invalid_game_info_property (SgfParsingData *data, SgfProperty *property,
			    BufferPositionStorage *storage)
{
  RESTORE_BUFFER_POSITION (data, 0, *storage);
  property->value.text = do_parse_simple_text (data, SGF_END);

  RESTORE_BUFFER_POSITION (data, 0, *storage);
  next_character (data);

  return SGF_ERROR_INVALID_GAME_INFO_PROPERTY;
}


static inline void
begin_parsing_value (SgfParsingData *data)
{
  data->whitespace_error_position.line = 0;
  next_token_in_value (data);
}


static int
is_composed_value (SgfParsingData *data, int expecting_simple_text)
{
  BufferPositionStorage storage;
  int whitespace_line;
  int whitespace_passed;

  if (data->token == ']')
    return 0;

  STORE_BUFFER_POSITION (data, 2, storage);
  whitespace_line = data->whitespace_error_position.line;
  whitespace_passed = data->whitespace_passed;

  while (data->token != ':' && data->token != ']' && data->token != SGF_END)
    next_token_in_value (data);

  if (data->token == ':') {
    if (!expecting_simple_text) {
      next_token_in_value (data);
      if (data->token == ']') {
	RESTORE_BUFFER_POSITION (data, 2, storage);
	data->whitespace_error_position.line = whitespace_line;
	data->whitespace_passed = whitespace_passed;

	return 0;
      }
    }

    if (storage.token != ':' && !data->in_parse_root) {
      RESTORE_BUFFER_POSITION (data, 2, storage);
      add_error (data, SGF_ERROR_ILLEGAL_TRAILING_CHARACTERS_BEFORE_COLON);

      if (data->whitespace_error_position.line
	  && !(whitespace_line && whitespace_passed))
	data->whitespace_error_position.line = 0;

      if (!expecting_simple_text)
	next_token_in_value (data);
    }

    return 1;
  }

  RESTORE_BUFFER_POSITION (data, 2, storage);
  data->whitespace_error_position.line = whitespace_line;
  data->whitespace_passed = whitespace_passed;

  return 0;
}


static SgfError
end_parsing_value (SgfParsingData *data)
{
  if (data->non_sgf_point_error_position.line) {
    insert_error (data, SGF_WARNING_NON_SGF_POINT_NOTATION,
		  &data->non_sgf_point_error_position,
		  data->non_sgf_point_x, data->non_sgf_point_y);
    data->non_sgf_point_error_position.line = 0;
  }

  if (data->whitespace_error_position.line
      && (data->token == ']' || data->whitespace_passed)) {
    insert_error (data, SGF_WARNING_ILLEGAL_WHITESPACE,
		  &data->whitespace_error_position);
  }

  if (data->token == ']') {
    next_token (data);
    return SGF_SUCCESS;
  }

  return SGF_ERROR_ILLEGAL_TRAILING_CHARACTERS;
}


/* Format an error.  It recognizes the following custom conversion
 * specifiers in addition to standard `%c', `%d' and `%s':
 *
 * - `%N' stands for current property name (stack is untouched).
 *
 * - `%V' stands for current value (shortened if needed; no arguments
 *   are taken from stack).
 *
 * - `%P': two integers are taken from the stack and coordinates of
 *   the point they make are put into output buffer.
 *
 * - `%M': move, stored in `data->node' is formatted according to
 *   current game.
 */
static int
format_error_valist (SgfParsingData *data, char *buffer,
		     SgfError error, va_list arguments)
{
  char *pointer = buffer;
  const char *format_string = sgf_errors[error];

  while (*format_string) {
    if (*format_string != '%')
      *pointer++ = *format_string++;
    else {
      format_string++;
      switch (*format_string++) {
      case 'N':
	strcpy (pointer, property_info[data->property_type].name);
	pointer += strlen (property_info[data->property_type].name);

	break;

      case 'V':
	data->temp_buffer = data->buffer;

	if (data->token == ESCAPED_BRACKET) {
	  *data->temp_buffer++ = '\\';
	  *data->temp_buffer++ = ']';
	  next_character (data);
	}

	while (data->token != *format_string && data->token != SGF_END) {
	  if (data->token == '\\') {
	    *data->temp_buffer++ = data->token;
	    next_character (data);
	  }

	  *data->temp_buffer++ = (data->token != '\n' ? data->token : ' ');

	  if (data->token == ' ' || data->token == '\n')
	    next_token (data);
	  else
	    next_character (data);
	}

	if (data->temp_buffer - data->buffer <= 27) {
	  memcpy (pointer, data->buffer, data->temp_buffer - data->buffer);
	  pointer += data->temp_buffer - data->buffer;
	}
	else {
	  /* Value is long, only output the beginning and the end. */
	  memcpy (pointer, data->buffer, 12);
	  pointer += 12;

	  *pointer++ = '.';
	  *pointer++ = '.';
	  *pointer++ = '.';

	  memcpy (pointer, data->temp_buffer - 12, 12);
	  pointer += 12;
	}

	break;

      case 'P':
	{
	  int x = va_arg (arguments, int);
	  int y = va_arg (arguments, int);

	  pointer += game_format_point (data->game,
					data->board_width, data->board_height,
					pointer, x, y);
	}

	break;

      case 'M':
	pointer += sgf_utils_format_node_move (data->tree, data->node, pointer,
					       "B ", "W ", NULL);
	break;

      case 'c':
	*pointer++ = (char) va_arg (arguments, int);
	break;

      case 'd':
	pointer += sprintf (pointer, "%d", va_arg (arguments, int));
	break;

      case 's':
	{
	  const char *string = va_arg (arguments, const char *);

	  strcpy (pointer, string);
	  pointer += strlen (string);
	}

	break;

      case 0:
	break;

      default:
	*pointer++ = *(format_string - 1);
      }
    }
  }

  return pointer - buffer;
}



/* Read characters from the input buffer skipping any whitespace
 * encountered.  This is just a wrapper around next_character().
 */
static inline void
next_token (SgfParsingData *data)
{
  do
    next_character (data);
  while (data->token == ' ' || data->token == '\n');
}


/* Similar to next_token() except that keeps track of the first
 * whitespace encountered (if any).  This information is used by
 * end_parsing_value() for reporting errors.
 *
 * This function also handles the escape character ('\') transparently
 * for its callers (SGF specification remains silent about escaping in
 * non-text properties, but this is the way `SGFC' behaves).
 */
static void
next_token_in_value (SgfParsingData *data)
{
  next_character (data);
  if (data->token == '\\') {
    next_character (data);
    if (data->token == ']')
      data->token = ESCAPED_BRACKET;
  }

  data->whitespace_passed = 1;

  if (data->token == ' ' || data->token == '\n') {
    if (!data->whitespace_error_position.line) {
      STORE_ERROR_POSITION (data, data->whitespace_error_position);
      data->whitespace_passed = 0;
    }

    do {
      next_character (data);
      if (data->token == '\\') {
	next_character (data);
	if (data->token == ']')
	  data->token = ESCAPED_BRACKET;
      }
    } while (data->token == ' ' || data->token == '\n');
  }
}


/* Read a character from the input buffer.  All linebreak combinations
 * allowed by SGF (LF, CR, CR LF, LF CR) are replaced with a single
 * '\n'.  All other whitespace characters ('\t', '\v' and '\f') are
 * converted to spaces.  The function also keeps track of the current
 * line and column in the buffer.
 */
static void
next_character (SgfParsingData *data)
{
  if (data->buffer_pointer < data->buffer_end) {
    char token = *data->buffer_pointer++;

    /* Update current line and column based on the _previous_
     * character.  This way newlines appear at ends of lines.
     */
    data->column = data->pending_column;
    if (data->column == 0)
      data->line++;

    if (token != '\n' && token != '\r') {
      if (token == 0) {
	if (!data->zero_byte_error_position.line) {
	  /* To avoid spoiling add_error() calls, we add warning about
	   * zero byte after the complete buffer is parsed.
	   */
	  STORE_ERROR_POSITION (data, data->zero_byte_error_position);
	}

	while (data->buffer_pointer < data->buffer_end
	       && *data->buffer_pointer == 0) {
	  data->buffer_pointer++;
	  data->pending_column++;
	}

	next_character (data);
	return;
      }

      data->token = token;
      data->pending_column++;

      /* SGF specification tells to handle '\t', '\v' and '\f' as a
       * space.  Also, '\t' updates column in a non-standard way.
       */
      if (token == '\t') {
	data->pending_column = ROUND_UP (data->pending_column, 8);
	data->token = ' ';
      }

      if (token == '\v' || token == '\f')
	data->token = ' ';
    }
    else {
      if (data->buffer_pointer == data->buffer_end
	  && data->buffer_refresh_point < data->buffer_end)
	expand_buffer (data);

      if (data->buffer_pointer < data->buffer_end
	  && token + *data->buffer_pointer == '\n' + '\r') {
	/* Two character linebreak, e.g. for Windows systems. */
	data->buffer_pointer++;
      }

      data->token = '\n';
      data->pending_column = 0;
    }
  }
  else {
    if (data->buffer_refresh_point == data->buffer_end)
      data->token = SGF_END;
    else {
      expand_buffer (data);
      next_character (data);
    }
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
