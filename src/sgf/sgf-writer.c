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


#include "sgf.h"
#include "sgf-writer.h"
#include "sgf-privates.h"
#include "board.h"
#include "utils.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>


static void	    write_game_tree(SgfWritingData *data, SgfGameTree *tree);
static void	    write_node_sequence(SgfWritingData *data, SgfNode *node);


static inline void  do_write_point(SgfWritingData *data, BoardPoint point);
static void	    do_write_point_or_rectangle(SgfWritingData *data,
						BoardPoint left_top,
						BoardPoint right_bottom);

static void	    do_write_go_move(SgfWritingData *data, SgfNode *node);
static void	    do_write_othello_move(SgfWritingData *data, SgfNode *node);
static void	    do_write_amazons_move(SgfWritingData *data, SgfNode *node);


static int	    do_write_text(SgfWritingData *data, const char *text,
				  int simple);


/* NOTE: if `filename' is NULL, write to stdout.
 * FIXME: add proper comment and actually write this function.
 */
int
sgf_write_file(const char *filename, SgfCollection *collection)
{
  SgfWritingData data;

  if (buffered_writer_init(&data.writer, filename, 0x4000)) {
    for (data.tree = collection->first_tree; data.tree;
	 data.tree = data.tree->next) {
      write_game_tree(&data, data.tree);
      if (data.tree->next)
	buffered_writer_add_newline(&data.writer);

      if (!data.writer.successful)
	break;
    }

    return buffered_writer_dispose(&data.writer);
  }

  return 0;
}



static void
write_game_tree(SgfWritingData *data, SgfGameTree *tree)
{
  SgfNode *root = tree->root;
  const BoardPositionList *root_black_stones;
  const BoardPositionList *root_white_stones;
  BoardPositionList *black_stones;
  BoardPositionList *white_stones;
  int default_setup_hidden = 0;

  buffered_writer_printf(&data->writer,
			 ("(;GM[%d]FF[4]\nAP[" PACKAGE_NAME
			  ":" PACKAGE_VERSION "]\n"),
			 tree->game);

  if (tree->board_width == tree->board_height)
    buffered_writer_printf(&data->writer, "SZ[%d]\n", tree->board_width);
  else {
    buffered_writer_printf(&data->writer, "SZ[%d:%d]\n",
			   tree->board_width, tree->board_height);
  }

  root_black_stones = sgf_node_get_list_of_point_property_value(root,
								SGF_ADD_BLACK);
  root_white_stones = sgf_node_get_list_of_point_property_value(root,
								SGF_ADD_WHITE);
  if (root_black_stones && root_white_stones
      && (tree->game != GAME_AMAZONS
	  || !sgf_node_get_list_of_point_property_value(root,
							SGF_ADD_ARROWS))) {
    if (game_get_default_setup(tree->game,
			       tree->board_width, tree->board_height,
			       &black_stones, &white_stones)) {
      if (board_position_lists_are_equal(root_black_stones, black_stones)
	  && board_position_lists_are_equal(root_white_stones, white_stones)) {
	sgf_node_delete_property(root, tree, SGF_ADD_BLACK);
	sgf_node_delete_property(root, tree, SGF_ADD_WHITE);

	default_setup_hidden = 1;
      }
      else {
	board_position_list_delete(black_stones);
	board_position_list_delete(white_stones);
      }
    }
  }

  if (data->tree->game == GAME_GO)
    data->do_write_move = do_write_go_move;
  else if (data->tree->game == GAME_OTHELLO)
    data->do_write_move = do_write_othello_move;
  else if (data->tree->game == GAME_AMAZONS)
    data->do_write_move = do_write_amazons_move;
  else
    assert(0);

  write_node_sequence(data, root);

  if (data->writer.column > 0)
    buffered_writer_add_newline(&data->writer);
  buffered_writer_add_character(&data->writer, ')');
  buffered_writer_add_newline(&data->writer);

  if (default_setup_hidden) {
    sgf_node_add_list_of_point_property(root, tree,
					SGF_ADD_BLACK, black_stones);
    sgf_node_add_list_of_point_property(root, tree,
					SGF_ADD_WHITE, white_stones);
  }
}


static void
write_node_sequence(SgfWritingData *data, SgfNode *node)
{
  while (1) {
    SgfProperty *property;

    if (IS_STONE(node->move_color)) {
      buffered_writer_add_character(&data->writer,
				    node->move_color == BLACK ? 'B' : 'W');
      buffered_writer_add_character(&data->writer, '[');

      data->do_write_move(data, node);

      buffered_writer_add_character(&data->writer, ']');
    }

    for (property = node->properties; property; property = property->next) {
      if (property_info[property->type].value_writer) {
	if (data->writer.column >= FILL_BREAK_POINT)
	  buffered_writer_add_newline(&data->writer);

	buffered_writer_cat_string(&data->writer,
				   property_info[property->type].name);

	property_info[property->type].value_writer(data, property->value);

	if (SGF_FIRST_GAME_INFO_PROPERTY <= property->type
	    && property->type <= SGF_LAST_GAME_INFO_PROPERTY
	    && data->writer.column > 0)
	  buffered_writer_add_newline(&data->writer);
      }
      else
	assert(0);
    }

    if (!node->child)
      break;

    node = node->child;
    if (!node->next) {
      if (data->writer.column >= FILL_BREAK_POINT - 1)
	buffered_writer_add_newline(&data->writer);
      buffered_writer_add_character(&data->writer, ';');

      continue;
    }

    do {
      if (data->writer.column > 0)
	buffered_writer_add_newline(&data->writer);
      buffered_writer_add_character(&data->writer, '(');
      buffered_writer_add_character(&data->writer, ';');

      write_node_sequence(data, node);

      if (data->writer.column >= FILL_COLUMN - 1)
	buffered_writer_add_newline(&data->writer);
      buffered_writer_add_character(&data->writer, ')');

      node = node->next;
    } while (node);

    break;
  }
}



static inline void
do_write_point(SgfWritingData *data, BoardPoint point)
{
  buffered_writer_add_character(&data->writer,
				(point.x < 'z' - 'a' + 1
				 ? 'a' + point.x
				 : 'A' + (point.x - ('z' - 'a' + 1))));
  buffered_writer_add_character(&data->writer,
				(point.y < 'z' - 'a' + 1
				 ? 'a' + point.y
				 : 'A' + (point.y - ('z' - 'a' + 1))));
}


static void
do_write_point_or_rectangle(SgfWritingData *data,
			    BoardPoint left_top, BoardPoint right_bottom)
{
  buffered_writer_add_character(&data->writer, '[');

  do_write_point(data, left_top);
  if (left_top.x != right_bottom.x || left_top.y != right_bottom.y) {
    buffered_writer_add_character(&data->writer, ':');
    do_write_point(data, right_bottom);
  }

  buffered_writer_add_character(&data->writer, ']');
  if (data->writer.column >= FILL_BREAK_POINT)
    buffered_writer_add_newline(&data->writer);
}


static void
do_write_go_move(SgfWritingData *data, SgfNode *node)
{
  if (node->move_point.x != PASS_X)
    do_write_point(data, node->move_point);
}


static void
do_write_othello_move(SgfWritingData *data, SgfNode *node)
{
  do_write_point(data, node->move_point);
}


static void
do_write_amazons_move(SgfWritingData *data, SgfNode *node)
{
  /* Move-amazon-from point. */
  do_write_point(data, node->data.amazons.from);

  /* Move-amazon-to point. */
  do_write_point(data, node->move_point);

  /* Shoot-arrow-to point. */
  do_write_point(data, node->data.amazons.shoot_arrow_to);
}



void
sgf_write_none(SgfWritingData *data, SgfValue value)
{
  UNUSED(value);

  buffered_writer_add_character(&data->writer, '[');
  buffered_writer_add_character(&data->writer, ']');
}


void
sgf_write_number(SgfWritingData *data, SgfValue value)
{
  buffered_writer_add_character(&data->writer, '[');
  buffered_writer_printf(&data->writer, "%d", value.number);
  buffered_writer_add_character(&data->writer, ']');
}


void
sgf_write_real(SgfWritingData *data, SgfValue value)
{
  buffered_writer_add_character(&data->writer, '[');
  buffered_writer_cat_string(&data->writer, utils_format_double(*value.real));
  buffered_writer_add_character(&data->writer, ']');
}


void
sgf_write_double(SgfWritingData *data, SgfValue value)
{
  buffered_writer_add_character(&data->writer, '[');
  buffered_writer_add_character(&data->writer, value.emphasized ? '2' : '1');
  buffered_writer_add_character(&data->writer, ']');
}


void
sgf_write_color(SgfWritingData *data, SgfValue value)
{
  buffered_writer_add_character(&data->writer, '[');

  buffered_writer_add_character(&data->writer,
				value.color == BLACK ? 'B' : 'W');

  buffered_writer_add_character(&data->writer, ']');
}


static int
do_write_text(SgfWritingData *data, const char *text, int simple)
{
  const char *lookahead;
  int columns_left = FILL_COLUMN - data->writer.column;
  int multi_line_value = 0;

  for (lookahead = text; ; lookahead++) {
    columns_left--;
    if (*lookahead == ']' || *lookahead == '\\')
      columns_left--;

    if ((*lookahead == ' ' && (columns_left >= 0 || simple))
	|| *lookahead == '\n' || ! *lookahead) {
      while (text < lookahead) {
	if (*text == ']' || *text == '\\')
	  buffered_writer_add_character(&data->writer, '\\');
	buffered_writer_add_character(&data->writer, *text++);
      }

      if (*lookahead == '\n') {
	buffered_writer_add_newline(&data->writer);
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
	    buffered_writer_add_character(&data->writer, ' ');
	    text++;
	  }

	  buffered_writer_add_character(&data->writer, '\\');
	  columns_left--;
	}
      }
      else {
	const char *line_limit = lookahead - 1;

	if (!ispunct(*line_limit)) {
	  while (line_limit > text + 1 && !ispunct(*(line_limit - 1)))
	    line_limit--;

	  if (line_limit <= text + 1)
	    line_limit = lookahead - 1;
	}

	while (text < line_limit) {
	  if (*text == ']' || *text == '\\')
	    buffered_writer_add_character(&data->writer, '\\');
	  buffered_writer_add_character(&data->writer, *text++);
	}

	buffered_writer_add_character(&data->writer, '\\');
	columns_left--;
      }

      columns_left += data->writer.column;
      buffered_writer_add_newline(&data->writer);
      multi_line_value = 1;
    }
  }

  return multi_line_value;
}


void
sgf_write_simple_text(SgfWritingData *data, SgfValue value)
{
  int multi_line_value;

  buffered_writer_add_character(&data->writer, '[');
  multi_line_value = do_write_text(data, value.text, 1);
  buffered_writer_add_character(&data->writer, ']');

  if (multi_line_value)
    buffered_writer_add_newline(&data->writer);
}


void
sgf_write_fake_simple_text(SgfWritingData *data, SgfValue value)
{
  const char *text;

  buffered_writer_add_character(&data->writer, '[');

  for (text = value.text; *text; text++) {
    if (*text == '\\' || *text == ']')
      buffered_writer_add_character(&data->writer, '\\');
    buffered_writer_add_character(&data->writer, *text);
  }

  buffered_writer_add_character(&data->writer, ']');

  buffered_writer_add_newline(&data->writer);
}


void
sgf_write_text(SgfWritingData *data, SgfValue value)
{
  int multi_line_value;

  buffered_writer_add_character(&data->writer, '[');
  multi_line_value = do_write_text(data, value.text, 0);
  buffered_writer_add_character(&data->writer, ']');

  if (multi_line_value)
    buffered_writer_add_newline(&data->writer);
}


/* Write a compressed list of points.  See SGF 4 specification for
 * description of compressed list.  The algorithm used here should be
 * moderately efficient, extremely fast and quite simple.
 *
 * NOTE: It relies on points being sorted in ascending order.
 */
void
sgf_write_list_of_point(SgfWritingData *data, SgfValue value)
{
  int num_positions = value.position_list->num_positions;
  int *positions = value.position_list->positions;

  if (num_positions > 1) {
    int k;
    int x;
    int width = data->tree->board_width;
    BoardPoint left_top[BOARD_MAX_WIDTH];
    BoardPoint right_bottom;

    for (x = 0; x < width; x++)
      left_top[x].x = NULL_X;

    for (k = 1; k <= num_positions ; k++) {
      int left_x = POSITION_X(positions[k - 1]);
      int y	 = POSITION_Y(positions[k - 1]);
      int limit_x;

      right_bottom.x = left_x;
      right_bottom.y = y - 1;

      while (left_top[(int) right_bottom.x].x != left_x) {
	if (left_top[(int) right_bottom.x].x != NULL_X) {
	  do_write_point_or_rectangle(data, left_top[(int) right_bottom.x],
				      right_bottom);

	  left_top[(int) right_bottom.x].x = NULL_X;
	}

	if (k == num_positions || positions[k] != EAST(positions[k - 1])) {
	  left_top[(int) right_bottom.x].x = left_x;
	  left_top[(int) right_bottom.x].y = y;

	  break;
	}

	right_bottom.x++;
	k++;
      }

      if (k < num_positions && positions[k] < SOUTH(positions[k - 1]))
	limit_x = POSITION_X(positions[k]);
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
	  do_write_point_or_rectangle(data, left_top[(int) right_bottom.x],
				      right_bottom);

	  left_top[(int) right_bottom.x].x = NULL_X;
	}
      }

      if (k == num_positions || positions[k] > SOUTH(positions[k - 1])) {
	if (left_top[(int) right_bottom.x].x != NULL_X) {
	  do_write_point_or_rectangle(data, left_top[(int) right_bottom.x],
				      right_bottom);

	  left_top[(int) right_bottom.x].x = NULL_X;
	}
      }
    }

    if (data->writer.column > 0)
      buffered_writer_add_newline(&data->writer);
  }
  else {
    buffered_writer_add_character(&data->writer, '[');

    if (num_positions == 1) {
      BoardPoint point;

      point.x = POSITION_X(positions[0]);
      point.y = POSITION_Y(positions[0]);
      do_write_point(data, point);
    }

    buffered_writer_add_character(&data->writer, ']');
  }
}


/* FIXME */
void
sgf_write_list_of_vector(SgfWritingData *data, SgfValue value)
{
  UNUSED(data);
  UNUSED(value);
}


void
sgf_write_list_of_label(SgfWritingData *data, SgfValue value)
{
  SgfLabelList *label_list = value.label_list;
  int multi_line_value = 0;
  int k;

  for (k = 0; k < label_list->num_labels; k++) {
    buffered_writer_add_character(&data->writer, '[');
    do_write_point(data, label_list->labels[k].point);
    buffered_writer_add_character(&data->writer, ':');

    multi_line_value += do_write_text(data, label_list->labels[k].text, 1);

    buffered_writer_add_character(&data->writer, ']');
    if (data->writer.column >= FILL_BREAK_POINT)
      buffered_writer_add_newline(&data->writer);
  }

  if ((label_list->num_labels > 1 || multi_line_value)
      && data->writer.column > 0)
    buffered_writer_add_newline(&data->writer);
}


void
sgf_write_unknown(SgfWritingData *data, SgfValue value)
{
  const char *text = value.text;
  int need_newline = 0;

  do
    buffered_writer_add_character(&data->writer, *text++);
  while (*text != '[');

  do {
    buffered_writer_add_character(&data->writer, '[');

    while (*++text != ']') {
      if (*text == '\\')
	buffered_writer_add_character(&data->writer, *text++);

      if (*text != '\n')
	buffered_writer_add_character(&data->writer, *text);
      else {
	buffered_writer_add_newline(&data->writer);
	need_newline = 1;
      }
    }

    buffered_writer_add_character(&data->writer, ']');
    if (*++text) {
      const char *lookahead;
      int columns_left = FILL_COLUMN - data->writer.column - 1;

      for (lookahead = text + 1; *lookahead != ']' && *lookahead != '\n';
	   lookahead++) {
	if (*lookahead == '\\') {
	  if (--columns_left >= 0 && *++lookahead == '\n')
	    break;
	}

	if (--columns_left < 0) {
	  buffered_writer_add_newline(&data->writer);
	  break;
	}
      }

      need_newline = 1;
    }
  } while (*text);

  if (need_newline && data->writer.column > 0)
    buffered_writer_add_newline(&data->writer);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
