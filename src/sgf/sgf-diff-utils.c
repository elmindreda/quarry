/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004, 2005, 2006 Paul Pogonyshev.                 *
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
#include "board.h"
#include "utils.h"

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>


#define OUT_OF_GRAPH		INT_MAX


#define NUM_CONTEXT_TREES		1
#define CONTEXT_TREE_ROOT_DEPTH		3

#define NUM_CONTEXT_NODES		1
#define CONTEXT_NODE_DEPTH		2

#define NUM_TEXT_CONTEXT_LINES		3


typedef struct _TextLine		TextLine;

struct _TextLine {
  const char	     *string;
  int		      length;
};

typedef struct _TextDiffData		TextDiffData;

struct _TextDiffData {
  int		      hunk_first_line_in_original;
  int		      hunk_first_line_in_modified;
  char		     *current_hunk;
};

typedef struct _EditGraphDiagonal	EditGraphDiagonal;
typedef struct _EditGraphLayer		EditGraphLayer;
typedef struct _EditGraph		EditGraph;

struct _EditGraphDiagonal {
  const void	     *from_trace_element;
  const void	     *to_trace_element;
  int		      trace_x;
};

struct _EditGraphLayer {
  EditGraphLayer     *next;
  EditGraphLayer     *previous;

  int		      path_diagonal;
  EditGraphDiagonal   diagonals[1];
};

struct _EditGraph {
  int		      full_distance;
  EditGraphLayer     *last_layer;
};


typedef int (* ComparisonFunction) (const void *first_element,
				    const void *second_element);
typedef const void * (* GetNextElementFunction) (const void *element);


static void	   build_abstract_edit_graph (EditGraph *edit_graph,
					      const void *from_sequence,
					      const void *to_sequence,
					      int num_skipped_elements,
					      ComparisonFunction are_equal,
					      GetNextElementFunction get_next);
static const EditGraphLayer *
		   reconstruct_minimal_path (const EditGraph *edit_graph);
static void	   edit_graph_dispose (EditGraph *edit_graph);

static int	   game_trees_are_equal (const SgfGameTree *first_tree,
					 const SgfGameTree *second_tree);
static const SgfGameTree *
		   get_next_game_tree (const SgfGameTree *tree);

static void	   generate_sgf_collection_diff
		     (const EditGraph *edit_graph,
		      const SgfCollection *from_collection,
		      const SgfCollection *to_collection,
		      SgfCollection *difference);
static void	   add_context_game_trees
		     (const SgfGameTree *context_subsequence, int num_trees,
		      SgfCollection *difference);

static void	   build_node_layer_diff (const SgfNode *from_node_layer,
					  const SgfNode *to_node_layer,
					  SgfGameTree *tree, SgfNode *parent);

static int	   generic_nodes_are_equal (const SgfNode *first_node,
					    const SgfNode *second_node);
static int	   amazons_nodes_are_equal (const SgfNode *first_node,
					    const SgfNode *second_node);
static int	   node_position_lists_are_equal (const SgfNode *first_node,
						  const SgfNode *second_node,
						  SgfType type);
static const SgfNode *
		   get_next_node (const SgfNode *node);

static void	   generate_sgf_node_layer_diff
		     (const EditGraph *edit_graph,
		      const SgfNode *from_node_layer,
		      const SgfNode *to_node_layer,
		      SgfGameTree *tree, SgfNode *parent);

#if 0

static TextLine *  chop_text_into_lines (const char *text);
static int	   text_lines_are_equal (const TextLine *first_text_line,
					 const TextLine *second_text_line);
static const TextLine *
		   get_next_text_line (const TextLine *text_line);

static char *	   do_generate_text_diff (const EditGraphLayer *layer,
					  int layer_distance,
					  int layer_diagonal,
					  int is_last_layer,
					  const TextLine *from_lines_sequence,
					  const TextLine *to_lines_sequence,
					  TextDiffData *data);
static char *	   generate_text_diff (const EditGraph *edit_graph,
				       const TextLine *from_lines_sequence,
				       const TextLine *to_lines_sequence);

#endif


SgfCollection *
sgf_diff (const SgfCollection *from_collection,
	  const SgfCollection *to_collection)
{
  EditGraph edit_graph;
  SgfCollection *difference = sgf_collection_new ();
  const SgfGameTree *from_tree_subsequence;
  const SgfGameTree *to_tree_subsequence;
  int num_skipped_trees;

  assert (from_collection);
  assert (to_collection);

  from_tree_subsequence = from_collection->first_tree;
  to_tree_subsequence = to_collection->first_tree;

  for (num_skipped_trees = 0;
       (from_tree_subsequence && to_tree_subsequence
	&& game_trees_are_equal (from_tree_subsequence, to_tree_subsequence));
       num_skipped_trees++) {
    from_tree_subsequence = from_tree_subsequence->next;
    to_tree_subsequence = to_tree_subsequence->next;
  }

  build_abstract_edit_graph (&edit_graph,
			     from_tree_subsequence, to_tree_subsequence,
			     num_skipped_trees,
			     (ComparisonFunction) game_trees_are_equal,
			     (GetNextElementFunction) get_next_game_tree);

  generate_sgf_collection_diff (&edit_graph, from_collection, to_collection,
				difference);

  edit_graph_dispose (&edit_graph);

  if (difference->num_trees > 0)
    return difference;

  sgf_collection_delete (difference);
  return NULL;
}



static void
build_abstract_edit_graph (EditGraph *edit_graph,
			   const void *from_sequence, const void *to_sequence,
			   int num_skipped_elements,
			   ComparisonFunction are_equal,
			   GetNextElementFunction get_next)
{
  EditGraphLayer *current_layer;
  int current_distance = 0;

  current_layer = utils_malloc (sizeof (EditGraphLayer));
  current_layer->previous			 = NULL;
  current_layer->diagonals[0].from_trace_element = from_sequence;
  current_layer->diagonals[0].to_trace_element	 = to_sequence;
  current_layer->diagonals[0].trace_x		 = num_skipped_elements;

  if (!from_sequence && !to_sequence) {
    current_layer->next = NULL;
    current_layer->path_diagonal = 0;

    edit_graph->full_distance = 0;
    edit_graph->last_layer = current_layer;

    return;
  }

  while (1) {
    int k;
    EditGraphLayer *previous_layer = current_layer;

    current_distance++;
    current_layer = utils_malloc (sizeof (EditGraphLayer)
				  + (current_distance
				     * sizeof (EditGraphDiagonal)));
    current_layer->previous = previous_layer;

    previous_layer->next = current_layer;

    for (k = 0; k <= current_distance; k++) {
      const void *from_element;
      const void *to_element;
      int x;

      if (k == 0
	  || (k != current_distance
	      && (previous_layer->diagonals[k].trace_x
		  > previous_layer->diagonals[k - 1].trace_x))) {
	if (!previous_layer->diagonals[k].to_trace_element) {
	  current_layer->diagonals[k].from_trace_element = NULL;
	  current_layer->diagonals[k].to_trace_element	 = NULL;
	  current_layer->diagonals[k].trace_x		 = OUT_OF_GRAPH;

	  continue;
	}

	from_element = previous_layer->diagonals[k].from_trace_element;
	to_element = get_next (previous_layer->diagonals[k].to_trace_element);

	x = previous_layer->diagonals[k].trace_x;
      }
      else {
	if (!previous_layer->diagonals[k - 1].from_trace_element) {
	  current_layer->diagonals[k].from_trace_element = NULL;
	  current_layer->diagonals[k].to_trace_element	 = NULL;
	  current_layer->diagonals[k].trace_x		 = OUT_OF_GRAPH;

	  continue;
	}

	from_element
	  = get_next (previous_layer->diagonals[k - 1].from_trace_element);
	to_element = previous_layer->diagonals[k - 1].to_trace_element;

	x = previous_layer->diagonals[k - 1].trace_x + 1;
      }

      while (from_element && to_element) {
	if (are_equal (from_element, to_element)) {
	  from_element = get_next (from_element);
	  to_element = get_next (to_element);
	  x++;
	}
	else
	  break;
      }

      current_layer->diagonals[k].from_trace_element = from_element;
      current_layer->diagonals[k].to_trace_element   = to_element;
      current_layer->diagonals[k].trace_x	     = x;

      if (!from_element && !to_element) {
	current_layer->next = NULL;
	current_layer->path_diagonal = k;

	edit_graph->full_distance = current_distance;
	edit_graph->last_layer = current_layer;

	return;
      }
    }
  }
}


static const EditGraphLayer *
reconstruct_minimal_path (const EditGraph *edit_graph)
{
  const EditGraphLayer *layer = edit_graph->last_layer;
  int layer_distance = edit_graph->full_distance;

  while (layer_distance > 0) {
    EditGraphLayer *previous_layer = layer->previous;
    int path_diagonal = layer->path_diagonal;

    if (path_diagonal == 0
	|| (path_diagonal != layer_distance
	    && (previous_layer->diagonals[path_diagonal].trace_x
		> previous_layer->diagonals[path_diagonal - 1].trace_x)))
      previous_layer->path_diagonal = path_diagonal;
    else
      previous_layer->path_diagonal = path_diagonal - 1;

    layer = previous_layer;
    layer_distance--;
  }

  return layer;
}


static void
edit_graph_dispose (EditGraph *edit_graph)
{
  EditGraphLayer *this_layer;
  EditGraphLayer *previous_layer;

  for (this_layer = edit_graph->last_layer; this_layer;
       this_layer = previous_layer) {
    previous_layer = this_layer->previous;

    utils_free (this_layer);
  }
}



/* FIXME: This function is way too forgiving.  Make it stricter. */
static int
game_trees_are_equal (const SgfGameTree *first_tree,
		      const SgfGameTree *second_tree)
{
  if (first_tree->game != second_tree->game
      || first_tree->board_width != second_tree->board_width
      || first_tree->board_height != second_tree->board_height)
    return 0;

  return (first_tree->game != GAME_AMAZONS
	  ? generic_nodes_are_equal (first_tree->root, second_tree->root)
	  : amazons_nodes_are_equal (first_tree->root, second_tree->root));
}


static const SgfGameTree *
get_next_game_tree (const SgfGameTree *tree)
{
  return tree->next;
}


/* FIXME: Add hunk "headers" generation (needs standardization?). */
static void
generate_sgf_collection_diff (const EditGraph *edit_graph,
			      const SgfCollection *from_collection,
			      const SgfCollection *to_collection,
			      SgfCollection *difference)
{
  const SgfGameTree *from_tree_subsequence = from_collection->first_tree;
  const SgfGameTree *to_tree_subsequence = to_collection->first_tree;
  const SgfGameTree *leading_context_subsequence = NULL;
  const EditGraphLayer *zero_layer = reconstruct_minimal_path (edit_graph);
  const EditGraphLayer *layer;
  int layer_distance;
  int x = 0;
  int num_leading_context_trees = 0;
  int num_traling_context_trees;

  for (layer = zero_layer, layer_distance = 0; layer;
       layer = layer->next, layer_distance++) {
    int trace_x = layer->diagonals[layer->path_diagonal].trace_x;

    if (layer_distance > 0) {
      add_context_game_trees (leading_context_subsequence,
			      num_leading_context_trees,
			      difference);

      if (layer->path_diagonal == layer->previous->path_diagonal) {
	/* A game tree got added. */
	SgfGameTree *added_tree
	  = sgf_game_tree_duplicate_with_nodes (to_tree_subsequence);

	/* FIXME: Add SGF property that shows that the game tree got
	 *	  added (needs standardization?).
	 */
	sgf_collection_add_game_tree (difference, added_tree);

	to_tree_subsequence = to_tree_subsequence->next;
      }
      else {
	/* A game tree got deleted. */
	SgfGameTree *deleted_tree
	  = sgf_game_tree_duplicate_with_nodes (from_tree_subsequence);

	/* FIXME: Add SGF property that shows that the game tree got
	 *	  deleted (needs standardization?).
	 */
	sgf_collection_add_game_tree (difference, deleted_tree);

	from_tree_subsequence = from_tree_subsequence->next;
	x++;
      }

      num_traling_context_trees = 0;
    }
    else
      num_traling_context_trees = NUM_CONTEXT_TREES;

    num_leading_context_trees = 0;

    while (x < trace_x) {
      SgfGameTree *tree_difference
	= sgf_game_tree_duplicate (from_tree_subsequence);

      build_node_layer_diff (from_tree_subsequence->root,
			     to_tree_subsequence->root,
			     tree_difference, NULL);

      if (tree_difference->root) {
	add_context_game_trees (leading_context_subsequence,
				num_leading_context_trees,
				difference);
	sgf_collection_add_game_tree (difference, tree_difference);

	num_leading_context_trees = 0;
	num_traling_context_trees = 0;
      }
      else {
	sgf_game_tree_delete (tree_difference);

	if (num_traling_context_trees < NUM_CONTEXT_TREES) {
	  add_context_game_trees (from_tree_subsequence, 1, difference);
	  num_traling_context_trees++;
	}
	else {
	  if (num_leading_context_trees < NUM_CONTEXT_TREES) {
	    if (num_leading_context_trees++ == 0)
	      leading_context_subsequence = from_tree_subsequence;
	  }
	  else
	    leading_context_subsequence = leading_context_subsequence->next;
	}
      }

      from_tree_subsequence = from_tree_subsequence->next;
      to_tree_subsequence = to_tree_subsequence->next;
      x++;
    }
  }
}


static void
add_context_game_trees (const SgfGameTree *context_subsequence, int num_trees,
			SgfCollection *difference)
{
  int k;

  for (k = 0; k < num_trees;
       k++, context_subsequence = context_subsequence->next) {
    SgfGameTree *context_tree = sgf_game_tree_duplicate (context_subsequence);

    context_tree->root
      = sgf_node_duplicate_to_given_depth (context_subsequence->root,
					   context_tree, NULL,
					   CONTEXT_TREE_ROOT_DEPTH);
    sgf_collection_add_game_tree (difference, context_tree);
  }
}



static void
build_node_layer_diff (const SgfNode *from_node_layer,
		       const SgfNode *to_node_layer,
		       SgfGameTree *tree, SgfNode *parent)
{
  EditGraph edit_graph;
  const ComparisonFunction nodes_are_equal
    = (ComparisonFunction) (tree->game != GAME_AMAZONS
			    ? generic_nodes_are_equal
			    : amazons_nodes_are_equal);
  const SgfNode *from_node_subsequence = from_node_layer;
  const SgfNode *to_node_subsequence = to_node_layer;
  int num_skipped_nodes;

  for (num_skipped_nodes = 0;
       (from_node_subsequence && to_node_subsequence
	&& nodes_are_equal (from_node_subsequence, to_node_subsequence));
       num_skipped_nodes++) {
    from_node_subsequence = from_node_subsequence->next;
    to_node_subsequence = to_node_subsequence->next;
  }

  build_abstract_edit_graph (&edit_graph,
			     from_node_subsequence, to_node_subsequence,
			     num_skipped_nodes,
			     nodes_are_equal,
			     (GetNextElementFunction) get_next_node);

  generate_sgf_node_layer_diff (&edit_graph, from_node_layer, to_node_layer,
				tree, parent);

  edit_graph_dispose (&edit_graph);
}


static int
generic_nodes_are_equal (const SgfNode *first_node, const SgfNode *second_node)
{
  if (first_node->move_color != second_node->move_color)
    return 0;

  if (IS_STONE (first_node->move_color))
    return POINTS_ARE_EQUAL (first_node->move_point, second_node->move_point);
  else if (first_node->move_color == SETUP_NODE) {
    return (node_position_lists_are_equal (first_node, second_node,
					   SGF_ADD_BLACK)
	    && node_position_lists_are_equal (first_node, second_node,
					      SGF_ADD_WHITE)
	    && node_position_lists_are_equal (first_node, second_node,
					      SGF_ADD_EMPTY));
  }
  else {
    /* FIXME: What to do in this case? */
    return 1;
  }
}


static int
amazons_nodes_are_equal (const SgfNode *first_node, const SgfNode *second_node)
{
  if (first_node->move_color != second_node->move_color)
    return 0;

  if (IS_STONE (first_node->move_color)) {
    return (POINTS_ARE_EQUAL (first_node->data.amazons.from,
			      second_node->data.amazons.from)
	    && POINTS_ARE_EQUAL (first_node->move_point,
				 second_node->move_point)
	    && POINTS_ARE_EQUAL (first_node->data.amazons.shoot_arrow_to,
				 second_node->data.amazons.shoot_arrow_to));
  }
  else if (first_node->move_color == SETUP_NODE) {
    return (node_position_lists_are_equal (first_node, second_node,
					   SGF_ADD_BLACK)
	    && node_position_lists_are_equal (first_node, second_node,
					      SGF_ADD_WHITE)
	    && node_position_lists_are_equal (first_node, second_node,
					      SGF_ADD_EMPTY)
	    && node_position_lists_are_equal (first_node, second_node,
					      SGF_ADD_ARROWS));
  }
  else {
    /* FIXME: What to do in this case? */
    return 1;
  }
}


static int
node_position_lists_are_equal (const SgfNode *first_node,
			       const SgfNode *second_node,
			       SgfType type)
{
  const BoardPositionList *first_position_list
    = sgf_node_get_list_of_point_property_value (first_node, type);
  const BoardPositionList *second_position_list
    = sgf_node_get_list_of_point_property_value (second_node, type);

  if (first_position_list && second_position_list) {
    return board_position_lists_are_equal (first_position_list,
					   second_position_list);
  }

  return first_position_list == NULL && second_position_list == NULL;
}


static const SgfNode *
get_next_node (const SgfNode *node)
{
  return node->next;
}


static void
generate_sgf_node_layer_diff (const EditGraph *edit_graph,
			      const SgfNode *from_node_layer,
			      const SgfNode *to_node_layer,
			      SgfGameTree *tree, SgfNode *parent)
{
  SgfNode **link = (parent ? &parent->child : &tree->root);
  const SgfNode *leading_context_subsequence = NULL;
  const EditGraphLayer *zero_layer = reconstruct_minimal_path (edit_graph);
  const EditGraphLayer *layer;
  int layer_distance;
  int x = 0;
  int num_leading_context_nodes = 0;
  int num_traling_context_nodes;

  for (layer = zero_layer, layer_distance = 0; layer;
       layer = layer->next, layer_distance++) {
    int k;
    int trace_x = layer->diagonals[layer->path_diagonal].trace_x;

    if (layer_distance > 0) {
      for (k = 0; k < num_leading_context_nodes; k++) {
	*link = sgf_node_duplicate_to_given_depth (leading_context_subsequence,
						   tree, parent,
						   CONTEXT_NODE_DEPTH);
	link = & (*link)->next;

	leading_context_subsequence = leading_context_subsequence->next;
      }

      if (layer->path_diagonal == layer->previous->path_diagonal) {
	/* A node got added.
	 *
	 * FIXME: Add SGF property that shows that the node got added
	 *	  (needs standardization?).
	 */
	*link = sgf_node_duplicate_recursively (to_node_layer, tree, parent);

	to_node_layer = to_node_layer->next;
      }
      else {
	/* A node got deleted.
	 *
	 * FIXME: Add SGF property that shows that the node got deleted
	 *	  (needs standardization?).
	 */
	*link = sgf_node_duplicate_recursively (from_node_layer, tree, parent);

	from_node_layer = from_node_layer->next;
	x++;
      }

      num_traling_context_nodes = 0;
    }
    else
      num_traling_context_nodes = NUM_CONTEXT_NODES;

    num_leading_context_nodes = 0;

    while (x < trace_x) {
      int is_context_node = 1;

      if (from_node_layer->child || to_node_layer->child) {
	SgfNode *node_difference = sgf_node_duplicate (from_node_layer,
						       tree, parent);

	build_node_layer_diff (from_node_layer->child, to_node_layer->child,
			       tree, node_difference);

	if (node_difference->child) {
	  for (k = 0; k < num_leading_context_nodes; k++) {
	    *link
	      = sgf_node_duplicate_to_given_depth (leading_context_subsequence,
						   tree, parent,
						   CONTEXT_NODE_DEPTH);
	    link = & (*link)->next;

	    leading_context_subsequence = leading_context_subsequence->next;
	  }

	  *link = node_difference;
	  link = &node_difference->next;

	  num_leading_context_nodes = 0;
	  num_traling_context_nodes = 0;

	  is_context_node = 0;
	}
	else
	  sgf_node_delete (node_difference, tree);
      }

      if (is_context_node) {
	if (num_traling_context_nodes < NUM_CONTEXT_NODES) {
	  *link = sgf_node_duplicate_to_given_depth (from_node_layer,
						     tree, parent,
						     CONTEXT_NODE_DEPTH);
	  link = & (*link)->next;

	  num_traling_context_nodes++;
	}
	else {
	  if (num_leading_context_nodes < NUM_CONTEXT_TREES) {
	    if (num_leading_context_nodes++ == 0)
	      leading_context_subsequence = from_node_layer;
	  }
	  else
	    leading_context_subsequence = leading_context_subsequence->next;
	}
      }

      from_node_layer = from_node_layer->next;
      to_node_layer = to_node_layer->next;
      x++;
    }
  }
}



#if 0


static TextLine *
chop_text_into_lines (const char *text)
{
  TextLine *lines = NULL;
  int num_allocated_lines = 0;
  int num_lines = 0;

  while (1) {
    const char *line_end = strchr (text, '\n');

    if (num_lines + 2 > num_allocated_lines) {
      num_allocated_lines += 20;
      lines = utils_realloc (lines, num_allocated_lines * sizeof (TextLine));
    }

    lines[num_lines].string = text;

    if (line_end) {
      lines[num_lines++].length = line_end - text;
      text = line_end + 1;
    }
    else {
      lines[num_lines++].length = strlen (text);
      break;
    }
  }

  lines[num_lines].string = NULL;
  return lines;
}


static int
text_lines_are_equal (const TextLine *first_text_line,
		      const TextLine *second_text_line)
{
  return (first_text_line->length == second_text_line->length
	  && (strncmp (first_text_line->string, second_text_line->string,
		       first_text_line->length)
	      == 0));
}


static const TextLine *
get_next_text_line (const TextLine *text_line)
{
  if ((text_line + 1)->string != NULL)
    return text_line + 1;

  return NULL;
}


static char *
generate_text_diff (const EditGraph *edit_graph,
		    const TextLine *from_lines_sequence,
		    const TextLine *to_lines_sequence)
{
  TextDiffData data;

  return do_generate_text_diff (edit_graph->last_layer,
				edit_graph->full_distance,
				/* edit_graph->last_layer_diagonal */ 0, 1,
				from_lines_sequence, to_lines_sequence,
				&data);
}


/* Recursively generate unified text diff based on given edit graph
 * layer.
 *
 * FIXME: It doesn't work at all after the change of the EditGraph
 *	  structure (or rather generate_text_diff() does not).
 *
 * FIXME: Speed this function up by first calculating the diff size
 *	  and only then copying strings.
 */
static char *
do_generate_text_diff (const EditGraphLayer *layer,
		       int layer_distance, int layer_diagonal, int last_layer,
		       const TextLine *from_lines_sequence,
		       const TextLine *to_lines_sequence,
		       TextDiffData *data)
{
  int k;
  int first_line_index;
  int last_snake_line_index = layer->diagonals[layer_diagonal].trace_x;
  char *difference;

  if (layer_distance) {
    const EditGraphLayer *previous_layer = layer->previous;
    int first_snake_line_index;

    if (layer_diagonal == 0
	|| (layer_diagonal != layer_distance
	    && (previous_layer->diagonals[layer_diagonal].trace_x
		> previous_layer->diagonals[layer_diagonal - 1].trace_x))) {
      int added_line_index = (previous_layer->diagonals[layer_diagonal].trace_x
			      - (2 * layer_diagonal - layer_distance) - 1);

      difference
	= do_generate_text_diff (previous_layer,
				 layer_distance - 1, layer_diagonal, 0,
				 from_lines_sequence, to_lines_sequence, data);

      data->current_hunk
	= utils_cat_as_strings (data->current_hunk,
				"+", 1,
				to_lines_sequence[added_line_index].string,
				to_lines_sequence[added_line_index].length,
				"\n", 1, NULL);
      first_snake_line_index
	= previous_layer->diagonals[layer_diagonal].trace_x;
    }
    else {
      int deleted_line_index
	= previous_layer->diagonals[layer_diagonal - 1].trace_x;

      difference
	= do_generate_text_diff (previous_layer,
				 layer_distance - 1, layer_diagonal - 1, 0,
				 from_lines_sequence, to_lines_sequence, data);

      data->current_hunk
	= utils_cat_as_strings (data->current_hunk,
				"-", 1,
				from_lines_sequence[deleted_line_index].string,
				from_lines_sequence[deleted_line_index].length,
				"\n", 1, NULL);
      first_snake_line_index = deleted_line_index + 1;
    }

    if ((last_snake_line_index - first_snake_line_index
	 > 2 * NUM_TEXT_CONTEXT_LINES)
	|| last_layer) {
      char buffer[64];
      int length;
      int last_hunk_line_index = (first_snake_line_index
				  + NUM_TEXT_CONTEXT_LINES);
      int trace_y_adjustment = 2 * layer_diagonal - layer_distance;

      if (last_hunk_line_index > last_snake_line_index)
	last_hunk_line_index = last_snake_line_index;

      length = utils_ncprintf (buffer, sizeof buffer, "@@ -%d,%d +%d,%d @@\n",
			       data->hunk_first_line_in_original + 1,
			       (last_hunk_line_index
				- data->hunk_first_line_in_original),
			       data->hunk_first_line_in_modified + 1,
			       ((last_hunk_line_index - trace_y_adjustment)
				- data->hunk_first_line_in_modified));

      if (difference)
	difference = utils_cat_as_string (difference, buffer, length);
      else
	difference = utils_duplicate_as_string (buffer, length);

      difference = utils_cat_string (difference, data->current_hunk);
      utils_free (data->current_hunk);

      for (k = first_snake_line_index; k < last_hunk_line_index; k++) {
	difference
	  = utils_cat_as_strings (difference,
				  " ", 1,
				  from_lines_sequence[k].string,
				  from_lines_sequence[k].length,
				  "\n", 1, NULL);
      }

      if (last_layer)
	return difference;

      first_line_index = last_snake_line_index - NUM_TEXT_CONTEXT_LINES;

      data->hunk_first_line_in_original = first_line_index;
      data->hunk_first_line_in_modified = (first_line_index
					   - trace_y_adjustment);
      data->current_hunk = NULL;
    }
    else
      first_line_index = first_snake_line_index;
  }
  else {
    difference = NULL;

    first_line_index = last_snake_line_index - NUM_TEXT_CONTEXT_LINES;

    if (first_line_index < 0)
      first_line_index = 0;

    data->hunk_first_line_in_original = first_line_index;
    data->hunk_first_line_in_modified = first_line_index;
    data->current_hunk = NULL;
  }

  for (k = first_line_index; k < last_snake_line_index; k++) {
    data->current_hunk
      = utils_cat_as_strings (data->current_hunk,
			      " ", 1,
			      from_lines_sequence[k].string,
			      from_lines_sequence[k].length,
			      "\n", 1, NULL);
  }

  return difference;
}


#endif


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
