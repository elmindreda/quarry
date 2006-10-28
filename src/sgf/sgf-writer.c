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


#include "sgf.h"
#include "sgf-writer.h"
#include "sgf-privates.h"
#include "board.h"
#include "utils.h"

#include <assert.h>
#include <ctype.h>
#include <iconv.h>
#include <stdio.h>
#include <string.h>


#define SGF_WRITER_BUFFER_SIZE	0x4000


static void	    write_collection (SgfWritingData *data,
				      SgfCollection *collection,
				      int force_utf8);
static void	    write_game_tree (SgfWritingData *data, SgfGameTree *tree,
				     int force_utf8);
static void	    write_node_sequence (SgfWritingData *data,
					 const SgfNode *node);


inline static void  do_write_point (SgfWritingData *data, BoardPoint point);
static void	    do_write_point_or_rectangle (SgfWritingData *data,
						 BoardPoint left_top,
						 BoardPoint right_bottom);

static void	    do_write_go_move (SgfWritingData *data,
				      const SgfNode *node);
static void	    do_write_reversi_move (SgfWritingData *data,
					   const SgfNode *node);
static void	    do_write_amazons_move (SgfWritingData *data,
					   const SgfNode *node);


static int	    do_write_text (SgfWritingData *data, const char *text,
				   char terminating_character, int simple);


/* NOTE: if `filename' is NULL, write to stdout.
 * FIXME: add proper comment and actually write this function.
 */
char *
sgf_write_file (const char *filename, SgfCollection *collection,
		int force_utf8)
{
  SgfWritingData data;
  const char *initialization_error;

  assert (collection);

  initialization_error = buffered_writer_init (&data.writer, filename,
					       SGF_WRITER_BUFFER_SIZE);
  if (initialization_error)
    return utils_duplicate_string (initialization_error);

  write_collection (&data, collection, force_utf8);

  if (data.writer.successful) {
    buffered_writer_dispose (&data.writer);
    return NULL;
  }
  else {
    char* error = utils_duplicate_string (data.writer.error_string);

    buffered_writer_dispose (&data.writer);
    return error;
  }
}


char *
sgf_write_in_memory (SgfCollection *collection, int force_utf8,
		     int *sgf_length)
{
  SgfWritingData data;

  assert (collection);
  assert (sgf_length);

  buffered_writer_init_memory (&data.writer, SGF_WRITER_BUFFER_SIZE);
  write_collection (&data, collection, force_utf8);

  return buffered_writer_dispose_memory (&data.writer, sgf_length);
}



static void
write_collection (SgfWritingData *data, SgfCollection *collection,
		  int force_utf8)
{
  for (data->tree = collection->first_tree; data->tree;
       data->tree = data->tree->next) {
    write_game_tree (data, data->tree, force_utf8);
    if (data->tree->next)
      buffered_writer_add_newline (&data->writer);

    if (!data->writer.successful)
      break;
  }
}


static void
write_game_tree (SgfWritingData *data, SgfGameTree *tree, int force_utf8)
{
  SgfNode *root = tree->root;
  const BoardPositionList *root_black_stones;
  const BoardPositionList *root_white_stones;
  BoardPositionList *black_stones;
  BoardPositionList *white_stones;
  int default_setup_hidden = 0;

  buffered_writer_cprintf (&data->writer, "(;GM[%d]FF[4]\n", tree->game);

  if (force_utf8 || tree->char_set) {
    buffered_writer_cprintf (&data->writer, "CA[%s]\n",
			     force_utf8 ? "UTF-8" : tree->char_set);
  }

  buffered_writer_cat_string (&data->writer,
			      "AP[" PACKAGE_NAME ":" PACKAGE_VERSION "]\n");

  if (tree->board_width == tree->board_height)
    buffered_writer_cprintf (&data->writer, "SZ[%d]\n", tree->board_width);
  else {
    buffered_writer_cprintf (&data->writer, "SZ[%d:%d]\n",
			     tree->board_width, tree->board_height);
  }

  if (tree->style_is_set)
    buffered_writer_cprintf (&data->writer, "ST[%d]\n", tree->style);

  root_black_stones
    = sgf_node_get_list_of_point_property_value (root, SGF_ADD_BLACK);
  root_white_stones
    = sgf_node_get_list_of_point_property_value (root, SGF_ADD_WHITE);

  if (root_black_stones && root_white_stones
      && (tree->game != GAME_AMAZONS
	  || !sgf_node_get_list_of_point_property_value (root,
							 SGF_ADD_ARROWS))) {
    if (game_get_default_setup (tree->game,
				tree->board_width, tree->board_height,
				&black_stones, &white_stones)) {
      if (board_position_lists_are_equal (root_black_stones, black_stones)
	  && board_position_lists_are_equal (root_white_stones,
					     white_stones)) {
	sgf_node_delete_property (root, tree, SGF_ADD_BLACK);
	sgf_node_delete_property (root, tree, SGF_ADD_WHITE);

	default_setup_hidden = 1;
      }
      else {
	board_position_list_delete (black_stones);
	board_position_list_delete (white_stones);
      }
    }
  }

  if (data->tree->game == GAME_GO)
    data->do_write_move = do_write_go_move;
  else if (data->tree->game == GAME_REVERSI)
    data->do_write_move = do_write_reversi_move;
  else if (data->tree->game == GAME_AMAZONS)
    data->do_write_move = do_write_amazons_move;
  else
    assert (0);

  if (force_utf8 || (tree->char_set && strcmp (tree->char_set, "UTF-8") == 0))
    data->utf8_to_tree_encoding = NULL;
  else {
    data->utf8_to_tree_encoding = iconv_open ((tree->char_set
					       ? tree->char_set
					       : "ISO-8859-1"),
					      "UTF-8");
    assert (data->utf8_to_tree_encoding != (iconv_t) (-1));
  }

  write_node_sequence (data, root);

  if (data->utf8_to_tree_encoding)
    iconv_close (data->utf8_to_tree_encoding);

  if (data->writer.column > 0)
    buffered_writer_add_newline (&data->writer);
  buffered_writer_add_character (&data->writer, ')');
  buffered_writer_add_newline (&data->writer);

  if (default_setup_hidden) {
    sgf_node_add_list_of_point_property (root, tree,
					 SGF_ADD_BLACK, black_stones, 0);
    sgf_node_add_list_of_point_property (root, tree,
					 SGF_ADD_WHITE, white_stones, 0);
  }
}


static void
write_node_sequence (SgfWritingData *data, const SgfNode *node)
{
  while (1) {
    SgfValue to_play;
    SgfProperty *property;

    if (IS_STONE (node->move_color)) {
      buffered_writer_add_character (&data->writer,
				     node->move_color == BLACK ? 'B' : 'W');
      buffered_writer_add_character (&data->writer, '[');

      data->do_write_move (data, node);

      buffered_writer_add_character (&data->writer, ']');
    }

    to_play.color = node->to_play_color;

    for (property = node->properties; property; property = property->next) {
      if (property_info[property->type].value_writer) {
	if (data->writer.column >= FILL_BREAK_POINT)
	  buffered_writer_add_newline (&data->writer);

	if (to_play.color != EMPTY
	    && property->type > SGF_LAST_SETUP_PROPERTY) {
	  buffered_writer_cat_string (&data->writer,
				      property_info[SGF_TO_PLAY].name);
	  sgf_write_color (data, &to_play);

	  to_play.color = EMPTY;
	}

	buffered_writer_cat_string (&data->writer,
				    property_info[property->type].name);

	property_info[property->type].value_writer (data, &property->value);

	if (SGF_FIRST_GAME_INFO_PROPERTY <= property->type
	    && property->type <= SGF_LAST_GAME_INFO_PROPERTY
	    && data->writer.column > 0)
	  buffered_writer_add_newline (&data->writer);
      }
      else
	assert (0);
    }

    /* This can happen if there are no properties after `PL'. */
    if (to_play.color != EMPTY) {
      buffered_writer_cat_string (&data->writer,
				  property_info[SGF_TO_PLAY].name);
      sgf_write_color (data, &to_play);
    }

    if (!node->child)
      break;

    node = node->child;
    if (!node->next) {
      if (data->writer.column >= FILL_BREAK_POINT - 1)
	buffered_writer_add_newline (&data->writer);
      buffered_writer_add_character (&data->writer, ';');

      continue;
    }

    do {
      if (data->writer.column > 0)
	buffered_writer_add_newline (&data->writer);
      buffered_writer_add_character (&data->writer, '(');
      buffered_writer_add_character (&data->writer, ';');

      write_node_sequence (data, node);

      if (data->writer.column >= FILL_COLUMN - 1)
	buffered_writer_add_newline (&data->writer);
      buffered_writer_add_character (&data->writer, ')');

      node = node->next;
    } while (node);

    break;
  }
}



inline static void
do_write_point (SgfWritingData *data, BoardPoint point)
{
  buffered_writer_add_character (&data->writer,
				 (point.x < 'z' - 'a' + 1
				  ? 'a' + point.x
				  : 'A' + (point.x - ('z' - 'a' + 1))));
  buffered_writer_add_character (&data->writer,
				 (point.y < 'z' - 'a' + 1
				  ? 'a' + point.y
				  : 'A' + (point.y - ('z' - 'a' + 1))));
}


static void
do_write_point_or_rectangle (SgfWritingData *data,
			     BoardPoint left_top, BoardPoint right_bottom)
{
  buffered_writer_add_character (&data->writer, '[');

  do_write_point (data, left_top);
  if (left_top.x != right_bottom.x || left_top.y != right_bottom.y) {
    buffered_writer_add_character (&data->writer, ':');
    do_write_point (data, right_bottom);
  }

  buffered_writer_add_character (&data->writer, ']');
  if (data->writer.column >= FILL_BREAK_POINT)
    buffered_writer_add_newline (&data->writer);
}


static void
do_write_go_move (SgfWritingData *data, const SgfNode *node)
{
  if (node->move_point.x != PASS_X)
    do_write_point (data, node->move_point);
}


static void
do_write_reversi_move (SgfWritingData *data, const SgfNode *node)
{
  do_write_point (data, node->move_point);
}


static void
do_write_amazons_move (SgfWritingData *data, const SgfNode *node)
{
  /* Move-amazon-from point. */
  do_write_point (data, node->data.amazons.from);

  /* Move-amazon-to point. */
  do_write_point (data, node->move_point);

  /* Shoot-arrow-to point. */
  do_write_point (data, node->data.amazons.shoot_arrow_to);
}



void
sgf_write_none (SgfWritingData *data, const SgfValue *value)
{
  UNUSED (value);

  buffered_writer_add_character (&data->writer, '[');
  buffered_writer_add_character (&data->writer, ']');
}


void
sgf_write_number (SgfWritingData *data, const SgfValue *value)
{
  buffered_writer_add_character (&data->writer, '[');
  buffered_writer_cprintf (&data->writer, "%d", value->number);
  buffered_writer_add_character (&data->writer, ']');
}


void
sgf_write_real (SgfWritingData *data, const SgfValue *value)
{
  buffered_writer_add_character (&data->writer, '[');

#if SGF_REAL_VALUES_ALLOCATED_SEPARATELY
  buffered_writer_cprintf (&data->writer, "%.f", *value->real);
#else
  buffered_writer_cprintf (&data->writer, "%.f", value->real);
#endif

  buffered_writer_add_character (&data->writer, ']');
}


void
sgf_write_double (SgfWritingData *data, const SgfValue *value)
{
  buffered_writer_add_character (&data->writer, '[');
  buffered_writer_add_character (&data->writer, value->emphasized ? '2' : '1');
  buffered_writer_add_character (&data->writer, ']');
}


void
sgf_write_color (SgfWritingData *data, const SgfValue *value)
{
  buffered_writer_add_character (&data->writer, '[');

  buffered_writer_add_character (&data->writer,
				 value->color == BLACK ? 'B' : 'W');

  buffered_writer_add_character (&data->writer, ']');
}


/* FIXME: Comment.
 *
 * Argument `terminating_character' should be ither ']' or ':'.  Other
 * values are not prohibited, but they don't make sense in SGF.
 */
static int
do_write_text (SgfWritingData *data, const char *text,
	       char terminating_character, int simple)
{
  const char *lookahead;
  const char *written_up_to;
  int columns_left = FILL_COLUMN - data->writer.column;
  int multi_line_value = 0;

  buffered_writer_set_iconv_handle (&data->writer,
				    data->utf8_to_tree_encoding);

  for (lookahead = text; ; lookahead++) {
    if (!IS_UTF8_STARTER (*lookahead))
      continue;

    columns_left--;
    if (*lookahead == ']' || *lookahead == '\\')
      columns_left--;

    if ((*lookahead == ' ' && (columns_left >= 0 || simple))
	|| *lookahead == '\n' || ! *lookahead) {
      for (written_up_to = text; text < lookahead; text++) {
	if (*text == ']' || *text == '\\') {
	  buffered_writer_cat_as_string (&data->writer,
					 written_up_to, text - written_up_to);
	  written_up_to = text;
	  buffered_writer_add_character (&data->writer, '\\');
	}
      }

      buffered_writer_cat_as_string (&data->writer,
				     written_up_to, text - written_up_to);

      if (*lookahead == '\n') {
	buffered_writer_add_newline (&data->writer);
	columns_left = FILL_COLUMN;
	text++;
      }
      else if (! *lookahead)
	break;
    }

    if (columns_left < 0) {
      if (*text == ' ') {
	if (simple) {
	  text++;
	  columns_left++;
	}
	else {
	  if (text < lookahead - 1) {
	    buffered_writer_add_character (&data->writer, ' ');
	    text++;
	  }

	  buffered_writer_add_character (&data->writer, '\\');
	  columns_left--;
	}
      }
      else {
	const char *line_limit = lookahead - 1;

	if (!ispunct (*line_limit)) {
	  while (line_limit > text + 1 && !ispunct (*(line_limit - 1)))
	    line_limit--;

	  if (line_limit <= text + 1)
	    line_limit = lookahead - 1;
	}

	for (written_up_to = text; text < line_limit; text++) {
	  if (*text == ']' || *text == '\\') {
	    buffered_writer_cat_as_string (&data->writer,
					   written_up_to,
					   text - written_up_to);
	    written_up_to = text;
	    buffered_writer_add_character (&data->writer, '\\');
	  }
	}

	buffered_writer_cat_as_string (&data->writer,
				       written_up_to, text - written_up_to);

	buffered_writer_add_character (&data->writer, '\\');
	columns_left--;
      }

      columns_left += data->writer.column;
      buffered_writer_add_newline (&data->writer);
      multi_line_value = 1;
    }
  }

  /* Note that terminating character is written in encoding specified
   * by `CA' property.  SGF specification remains silent about this,
   * but otherwise files can become unparseable in certain encodings.
   */
  buffered_writer_add_character (&data->writer, terminating_character);
  buffered_writer_set_iconv_handle (&data->writer, NULL);

  return multi_line_value;
}


void
sgf_write_simple_text (SgfWritingData *data, const SgfValue *value)
{
  int multi_line_value;

  buffered_writer_add_character (&data->writer, '[');
  multi_line_value = do_write_text (data, value->text, ']', 1);

  if (multi_line_value)
    buffered_writer_add_newline (&data->writer);
}


void
sgf_write_fake_simple_text (SgfWritingData *data, const SgfValue *value)
{
  const char *text;
  const char *written_up_to;

  buffered_writer_add_character (&data->writer, '[');

  buffered_writer_set_iconv_handle (&data->writer,
				    data->utf8_to_tree_encoding);

  for (text = value->text, written_up_to = text; *text; text++) {
    if (*text == '\\' || *text == ']') {
      buffered_writer_cat_as_string (&data->writer,
				     written_up_to, text - written_up_to);
      written_up_to = text;

      buffered_writer_add_character (&data->writer, '\\');
    }
  }

  buffered_writer_cat_as_string (&data->writer, written_up_to,
				 text - written_up_to);

  buffered_writer_set_iconv_handle (&data->writer, NULL);

  buffered_writer_add_character (&data->writer, ']');
  buffered_writer_add_newline (&data->writer);
}


void
sgf_write_text (SgfWritingData *data, const SgfValue *value)
{
  int multi_line_value;

  buffered_writer_add_character (&data->writer, '[');
  multi_line_value = do_write_text (data, value->text, ']', 0);

  if (multi_line_value)
    buffered_writer_add_newline (&data->writer);
}


/* Write a compressed list of points.  See SGF 4 specification for
 * description of compressed list.  The algorithm used here should be
 * moderately efficient, extremely fast and quite simple.
 *
 * NOTE: It relies on points being sorted in ascending order.
 */
void
sgf_write_list_of_point (SgfWritingData *data, const SgfValue *value)
{
  int num_positions = value->position_list->num_positions;
  int *positions    = value->position_list->positions;

  if (num_positions > 1) {
    int k;
    int x;
    int width = data->tree->board_width;
    BoardPoint left_top[BOARD_MAX_WIDTH];
    BoardPoint right_bottom;

    for (x = 0; x < width; x++)
      left_top[x].x = NULL_X;

    for (k = 1; k <= num_positions ; k++) {
      int left_x = POSITION_X (positions[k - 1]);
      int y	 = POSITION_Y (positions[k - 1]);
      int limit_x;

      right_bottom.x = left_x;
      right_bottom.y = y - 1;

      while (left_top[(int) right_bottom.x].x != left_x) {
	if (left_top[(int) right_bottom.x].x != NULL_X) {
	  do_write_point_or_rectangle (data, left_top[(int) right_bottom.x],
				       right_bottom);

	  left_top[(int) right_bottom.x].x = NULL_X;
	}

	if (k == num_positions || positions[k] != EAST (positions[k - 1])) {
	  left_top[(int) right_bottom.x].x = left_x;
	  left_top[(int) right_bottom.x].y = y;

	  break;
	}

	right_bottom.x++;
	k++;
      }

      if (k < num_positions && positions[k] < SOUTH (positions[k - 1]))
	limit_x = POSITION_X (positions[k]);
      else
	limit_x = right_bottom.x;

      while (1) {
	if (++right_bottom.x == width) {
	  right_bottom.x = 0;
	  right_bottom.y++;
	}

	if (right_bottom.x == limit_x)
	  break;

	if (left_top[(int) right_bottom.x].x != NULL_X) {
	  do_write_point_or_rectangle (data, left_top[(int) right_bottom.x],
				       right_bottom);

	  left_top[(int) right_bottom.x].x = NULL_X;
	}
      }

      if (k == num_positions || positions[k] > SOUTH (positions[k - 1])) {
	if (left_top[(int) right_bottom.x].x != NULL_X) {
	  do_write_point_or_rectangle (data, left_top[(int) right_bottom.x],
				       right_bottom);

	  left_top[(int) right_bottom.x].x = NULL_X;
	}
      }
    }

    if (data->writer.column > 0)
      buffered_writer_add_newline (&data->writer);
  }
  else {
    buffered_writer_add_character (&data->writer, '[');

    if (num_positions == 1) {
      BoardPoint point;

      point.x = POSITION_X (positions[0]);
      point.y = POSITION_Y (positions[0]);
      do_write_point (data, point);
    }

    buffered_writer_add_character (&data->writer, ']');
  }
}


void
sgf_write_list_of_vector (SgfWritingData *data, const SgfValue *value)
{
  int k;
  int num_vectors = value->vector_list->num_vectors;
  const SgfVector *vectors = value->vector_list->vectors;

  for (k = 0; k < num_vectors; k++) {
    if (data->writer.column >= FILL_BREAK_POINT)
      buffered_writer_add_newline (&data->writer);

    buffered_writer_add_character (&data->writer, '[');
    do_write_point (data, vectors[k].from_point);
    buffered_writer_add_character (&data->writer, ':');
    do_write_point (data, vectors[k].to_point);
    buffered_writer_add_character (&data->writer, ']');
  }

  if (data->writer.column > 0)
    buffered_writer_add_newline (&data->writer);
}


void
sgf_write_list_of_label (SgfWritingData *data, const SgfValue *value)
{
  SgfLabelList *label_list = value->label_list;
  int multi_line_value = 0;
  int k;

  for (k = 0; k < label_list->num_labels; k++) {
    buffered_writer_add_character (&data->writer, '[');
    do_write_point (data, label_list->labels[k].point);
    buffered_writer_add_character (&data->writer, ':');

    multi_line_value += do_write_text (data,
				       label_list->labels[k].text, ']', 1);

    if (data->writer.column >= FILL_BREAK_POINT)
      buffered_writer_add_newline (&data->writer);
  }

  if ((label_list->num_labels > 1 || multi_line_value)
      && data->writer.column > 0)
    buffered_writer_add_newline (&data->writer);
}


void
sgf_write_figure_description (SgfWritingData *data, const SgfValue *value)
{
  buffered_writer_add_character (&data->writer, '[');

  if (value->figure)
    buffered_writer_cprintf (&data->writer, "%d:", value->figure->flags);

  if (value->figure && value->figure->diagram_name)
    do_write_text (data, value->figure->diagram_name, ']', 1);
  else
    buffered_writer_add_character (&data->writer, ']');
}


void
sgf_write_unknown (SgfWritingData *data, const SgfValue *value)
{
  const StringListItem *list_item = value->unknown_value_list->first;
  int need_newline;

  buffered_writer_cat_string (&data->writer, list_item->text);

  list_item = list_item->next;

  if (!list_item->next) {
    const char *lookahead;

    need_newline = 0;
    for (lookahead = list_item->text; *lookahead; lookahead++) {
      if (*lookahead == '\\')
	lookahead++;

      if (*lookahead == '\n') {
	need_newline = 1;
	break;
      }
    }
  }
  else
    need_newline = 1;

  do {
    buffered_writer_add_character (&data->writer, '[');

    buffered_writer_set_iconv_handle (&data->writer,
				      data->utf8_to_tree_encoding);
    buffered_writer_cat_string (&data->writer, list_item->text);
    buffered_writer_add_character (&data->writer, ']');
    buffered_writer_set_iconv_handle (&data->writer, NULL);

    list_item = list_item->next;
    if (list_item) {
      const char *lookahead;
      int columns_left = FILL_COLUMN - data->writer.column - 1;

      for (lookahead = list_item->text; *lookahead && *lookahead != '\n';
	   lookahead++) {
	if (IS_UTF8_STARTER (*lookahead)) {
	  if (*lookahead == '\\') {
	    if (--columns_left >= 0 && *++lookahead == '\n')
	      break;
	  }

	  if (--columns_left < 0) {
	    buffered_writer_add_newline (&data->writer);
	    break;
	  }
	}
      }
    }
  } while (list_item);

  if (need_newline && data->writer.column > 0)
    buffered_writer_add_newline (&data->writer);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
