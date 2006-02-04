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
#include "sgf-privates.h"
#include "sgf-undo.h"
#include "board.h"
#include "utils.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


inline static void  free_property_value (SgfProperty *property);

static int	    compare_sgf_labels (const void *first_label,
					const void *second_label);



/* Dynamically allocate and initialize an SgfCollection structure with
 * no game trees.
 */
SgfCollection *
sgf_collection_new (void)
{
  SgfCollection *collection = utils_malloc (sizeof (SgfCollection));

  collection->num_trees			  = 0;
  collection->first_tree		  = NULL;
  collection->last_tree			  = NULL;

  collection->num_modified_undo_histories = 0;
  collection->is_irreversibly_modified	  = 0;

  collection->notification_callback	  = NULL;
  collection->user_data			  = NULL;

  return collection;
}


/* Free a previously allocated SgfCollection structure and all its
 * game trees.
 */
void
sgf_collection_delete (SgfCollection *collection)
{
  SgfGameTree *this_tree;

  assert (collection);

  for (this_tree = collection->first_tree; this_tree;) {
    SgfGameTree *next_tree = this_tree->next;

    sgf_game_tree_delete (this_tree);
    this_tree = next_tree;
  }

  utils_free (collection);
}


/* Add a game tree at the end of a collection.  Fields of both
 * structures are updated in order to keep game trees properly linked.
 */
void
sgf_collection_add_game_tree (SgfCollection *collection, SgfGameTree *tree)
{
  assert (collection);
  assert (tree);

  tree->collection = collection;
  tree->previous   = collection->last_tree;
  tree->next	   = NULL;

  if (collection->last_tree)
    collection->last_tree->next = tree;
  else
    collection->first_tree = tree;

  collection->num_trees++;
  collection->last_tree = tree;
}


int
sgf_collection_is_modified (const SgfCollection *collection)
{
  assert (collection);

  return (collection->num_modified_undo_histories > 0
	  || collection->is_irreversibly_modified);
}


/* Note: caller is responsible for resetting any undo histories that
 * are used with collection's trees, but are not attached to them
 * right now.
 */
void
sgf_collection_set_unmodified (SgfCollection *collection)
{
  SgfGameTree *tree;

  if (!sgf_collection_is_modified (collection))
    return;

  collection->num_modified_undo_histories = 0;
  collection->is_irreversibly_modified	  = 0;

  for (tree = collection->first_tree; tree; tree = tree->next) {
    if (tree->undo_history) {
      tree->undo_history->unmodified_state_entry
	= tree->undo_history->last_applied_entry;
    }
  }

  COLLECTION_DO_NOTIFY (collection);
}


void
sgf_collection_set_notification_callback
  (SgfCollection *collection,
   SgfCollectionNotificationCallback callback, void *user_data)
{
  assert (collection);

  collection->notification_callback = callback;
  collection->user_data		    = user_data;
}



/* Dynamically allocate an SgfGameTree structure. */
SgfGameTree *
sgf_game_tree_new (void)
{
  SgfGameTree *tree = utils_malloc (sizeof (SgfGameTree));

  tree->collection	      = NULL;
  tree->previous	      = NULL;
  tree->next		      = NULL;

  tree->root		      = NULL;
  tree->current_node	      = NULL;
  tree->current_node_depth    = 0;

  tree->board		      = NULL;
  tree->board_state	      = NULL;

  tree->undo_history	      = NULL;
  tree->undo_history_list     = NULL;

  tree->undo_operation_level  = 0;

  tree->char_set	      = NULL;

  tree->application_name      = NULL;
  tree->application_version   = NULL;
  tree->style_is_set	      = 0;

  tree->node_pool.item_size   = 0;

  memory_pool_init (&tree->property_pool, sizeof (SgfProperty));

  tree->notification_callback = NULL;
  tree->user_data	      = NULL;

  tree->map_data_list	      = NULL;

  tree->view_port_nodes	      = NULL;
  tree->view_port_lines	      = NULL;

  return tree;
}


SgfGameTree *
sgf_game_tree_new_with_root (Game game, int board_width, int board_height,
			     int provide_default_setup)
{
  SgfGameTree *tree = sgf_game_tree_new ();
  BoardPositionList *black_stones;
  BoardPositionList *white_stones;

  assert (SGF_MIN_BOARD_SIZE <= board_width
	  && board_width <= SGF_MAX_BOARD_SIZE);
  assert (SGF_MIN_BOARD_SIZE <= board_height
	  && board_height <= SGF_MAX_BOARD_SIZE);

  sgf_game_tree_set_game (tree, game);
  tree->board_width = board_width;
  tree->board_height = board_height;

  tree->root	     = sgf_node_new (tree, NULL);
  tree->current_node = tree->root;

  if (provide_default_setup
      && game_get_default_setup (game, board_width, board_height,
				 &black_stones, &white_stones)) {
    tree->root->move_color = SETUP_NODE;

    if (black_stones) {
      sgf_node_add_list_of_point_property (tree->root, tree,
					   SGF_ADD_BLACK, black_stones, 0);
    }

    if (white_stones) {
      sgf_node_add_list_of_point_property (tree->root, tree,
					   SGF_ADD_WHITE, white_stones, 0);
    }
  }

  return tree;
}


/* Free a previously allocated game tree and all its nodes. */
void
sgf_game_tree_delete (SgfGameTree *tree)
{
  SgfUndoHistory *undo_history;

  assert (tree);

  if (tree->notification_callback)
    tree->notification_callback (tree, SGF_GAME_TREE_DELETED, tree->user_data);

  sgf_game_tree_invalidate_map (tree, NULL);

  undo_history = tree->undo_history_list;
  while (undo_history) {
    SgfUndoHistory *next_undo_history = undo_history->next;

    sgf_undo_history_delete (undo_history, tree);
    undo_history = next_undo_history;
  }

#if ENABLE_MEMORY_POOLS

  memory_pool_traverse (&tree->property_pool,
			(MemoryPoolCallback) free_property_value);

  memory_pool_flush (&tree->property_pool);
  if (tree->node_pool.item_size > 0)
    memory_pool_flush (&tree->node_pool);

#else

  if (tree->root)
    sgf_node_delete (tree->root, tree);

#endif

  utils_free (tree->char_set);
  utils_free (tree->application_name);
  utils_free (tree->application_version);

  utils_free (tree);
}


void
sgf_game_tree_set_game (SgfGameTree *tree, Game game)
{
  int node_size;

  assert (tree);
  assert (FIRST_GAME <= game && game <= LAST_GAME);

  tree->game = game;

  if (game == GAME_GO)
    node_size = sizeof (SgfNodeGo);
  else if (game == GAME_REVERSI)
    node_size = sizeof (SgfNodeReversi);
  else if (game == GAME_AMAZONS)
    node_size = sizeof (SgfNodeAmazons);
  else
    node_size = sizeof (SgfNodeGeneric);

  memory_pool_init (&tree->node_pool, node_size);
}


/* Set given tree's associated board, board state and current node.
 * All fields but `board_state' must be non-NULL.
 */
void
sgf_game_tree_set_state (SgfGameTree *tree, const SgfGameTreeState *state)
{
  assert (tree);
  assert (state);
  assert (state->board);
  assert (state->board->game == tree->game);
  assert (state->board->width == tree->board_width);
  assert (state->board->height == tree->board_height);
  assert (state->current_node);

  tree->board		   = state->board;
  tree->board_state	   = state->board_state;
  tree->current_node	   = state->current_node;
  tree->current_node_depth = state->current_node_depth;
}


void
sgf_game_tree_get_state (SgfGameTree *tree, SgfGameTreeState *state)
{
  assert (tree);
  assert (state);

  state->board		    = tree->board;
  state->board_state	    = tree->board_state;
  state->current_node	    = tree->current_node;
  state->current_node_depth = tree->current_node_depth;
}


/* Create a copy of given game tree structure.  Nodes are not
 * duplicated (not even root.)  User data is not copied either.  Use
 * sgf_game_tree_duplicate_with_nodes() if you need a copy with all
 * nodes.
 */
SgfGameTree *
sgf_game_tree_duplicate (const SgfGameTree *tree)
{
  SgfGameTree *tree_copy = sgf_game_tree_new ();

  assert (tree);

  sgf_game_tree_set_game (tree_copy, tree->game);
  tree_copy->board_width  = tree->board_width;
  tree_copy->board_height = tree->board_height;

  tree_copy->char_set = utils_duplicate_string (tree->char_set);

  tree_copy->style_is_set = tree->style_is_set;
  if (tree->style_is_set)
    tree_copy->style = tree->style;

  return tree_copy;
}


SgfGameTree *
sgf_game_tree_duplicate_with_nodes (const SgfGameTree *tree)
{
  SgfGameTree *tree_copy;

  assert (tree);
  assert (tree->root);

  tree_copy = sgf_game_tree_duplicate (tree);
  tree_copy->root = sgf_node_duplicate_recursively (tree->root, tree_copy,
						    NULL);

  return tree_copy;
}


/* Get the ``first'' node in the `tree' in traversing sense.  This is
 * always the root of the tree, but the caller should not know this.
 *
 * Trees are traversed forward in this order:
 *
 * - The first node is the root.
 *
 * - The next node is...
 *     the `child', if it exists;
 *     the `next' (in normal sense) node of the `parent', if exists;
 *     the `next' node of second order parent (`->parent->parent') if
 *     exists;
 *     ...
 *
 * See also sgf_node_traverse_forward().
 */
SgfNode *
sgf_game_tree_traverse_forward (const SgfGameTree *tree)
{
  assert (tree);

  return tree->root;
}


/* Get the ``last'' node in the `tree' traversing order.  The backward
 * traversing order is defined by reversing the forward traversing
 * order.  Traversing backward is generally slower.
 *
 * See also sgf_node_traverse_backward().
 */
SgfNode *
sgf_game_tree_traverse_backward (const SgfGameTree *tree)
{
  SgfNode *result;

  assert (tree);

  result = tree->root;
  if (!result)
    return NULL;

  while (1) {
    if (!result->child)
      return result;

    result = result->child;
    while (result->next)
      result = result->next;
  }
}


int
sgf_game_tree_count_nodes (const SgfGameTree *tree)
{
  assert (tree);

#if ENABLE_MEMORY_POOLS
  return memory_pool_count_items (&tree->node_pool);
#else
  return tree->root ? sgf_node_count_subtree_nodes (tree->root) : 0;
#endif
}


void
sgf_game_tree_set_notification_callback
  (SgfGameTree *tree,
   SgfGameTreeNotificationCallback callback, void *user_data)
{
  assert (tree);

  tree->notification_callback = callback;
  tree->user_data	      = user_data;
}



/* Dynamically allocate an SgfNode structure and links it to the
 * specified parent node.
 */
SgfNode *
sgf_node_new (SgfGameTree *tree, SgfNode *parent)
{
  SgfNode *node;

  node = memory_pool_alloc (&tree->node_pool);

  node->parent			  = parent;
  node->child			  = NULL;
  node->next			  = NULL;
  node->current_variation	  = NULL;

  node->is_collapsed		  = 0;
  node->has_intermediate_map_data = 0;

  node->to_play_color		  = EMPTY;
  node->move_color		  = EMPTY;

  node->properties		  = NULL;

  return node;
}


/* Free a previously allocated SgfNode structure and all its
 * properties.  All children nodes are deleted as well.  To reduce
 * stack usage, non-branching sequences of nodes are deleted in a
 * loop, not with recursion.
 */
void
sgf_node_delete (SgfNode *node, SgfGameTree *tree)
{
  assert (node);

  /* Delete a sequence of nodes starting at the given `node' until it
   * ends or we find a branching point.
   */
  do {
    SgfProperty *this_property;
    SgfNode *next_node = node->child;

    for (this_property = node->properties; this_property;) {
      SgfProperty *next_property = this_property->next;

      sgf_property_delete (this_property, tree);
      this_property = next_property;
    }

    memory_pool_free (&tree->node_pool, node);
    node = next_node;
  } while (node && !node->next);

  /* Recurse for each branch. */
  while (node) {
    SgfNode *next_node = node->next;

    sgf_node_delete (node, tree);
    node = next_node;
  }
}


/* Get the previous node of given node.  Return NULL if the node is
 * the first child or has no parent at all (is tree root.)
 */
SgfNode *
sgf_node_get_previous_node (const SgfNode *node)
{
  assert (node);

  if (node->parent && node->parent->child != node) {
    SgfNode *child;

    for (child = node->parent->child; child->next != node; child = child->next)
      assert (child);

    return child;
  }

  return NULL;
}


/* Get the last child of given node (the first is `node->child'.)  Can
 * return NULL if the node has no children at all.
 */
SgfNode *
sgf_node_get_last_child (const SgfNode *node)
{
  SgfNode *last_child;

  assert (node);

  last_child = node->child;
  if (last_child) {
    while (last_child->next)
      last_child = last_child->next;
  }

  return last_child;
}


/* Create a copy of an SGF node (with properties) for a given tree.
 * This function copies only the specified node, not its children.
 * Use sgf_node_duplicate_recursively() if you need a copy of full
 * subtree.
 */
SgfNode *
sgf_node_duplicate (const SgfNode *node, SgfGameTree *tree, SgfNode *parent)
{
  SgfNode *node_copy = sgf_node_new (tree, parent);
  const SgfProperty *property;
  SgfProperty **link;

  assert (node);
  assert (tree);

  node_copy->to_play_color = node->to_play_color;
  node_copy->move_color	   = node->move_color;

  if (IS_STONE (node->move_color)) {
    node_copy->move_point = node->move_point;

    if (tree->game == GAME_AMAZONS)
      node_copy->data.amazons = node->data.amazons;
  }

  for (property = node->properties, link = &node_copy->properties; property;
       property = property->next, link = & (*link)->next)
    *link = sgf_property_duplicate (property, tree, NULL);

  return node_copy;
}


/* Make a copy of given node and all its children.
 *
 * As in other functions, recursion only happens for sibling subnodes,
 * not the subnodes in main variation for performance reasons.
 */
SgfNode *
sgf_node_duplicate_recursively (const SgfNode *node, SgfGameTree *tree,
				SgfNode *parent)
{
  /* Duplicate just the given node. */
  SgfNode *node_copy = sgf_node_duplicate (node, tree, parent);

  /* Duplicate all nodes in sequence until we find a branching
   * point or the sequence ends.
   */
  parent = node_copy;
  while (1) {
    node = node->child;

    if (!node || node->next)
      break;

    parent->child = sgf_node_duplicate (node, tree, parent);
    parent = parent->child;
  }

  if (node) {
    SgfNode **link;

    /* Recurse for each branch. */
    for (link = &parent->child; node;
	 node = node->next, link = & (*link)->next)
      *link = sgf_node_duplicate (node, tree, parent);
  }

  return node_copy;
}


/* Similar to sgf_node_duplicate_recursively(), but duplicates only
 * some levels of nodes, not the whole node subtree.  If `depth'
 * parameter is 1, duplicate only the node itself.  If it is 2 then
 * direct children are included and so on.
 */
SgfNode *
sgf_node_duplicate_to_given_depth (const SgfNode *node, SgfGameTree *tree,
				   SgfNode *parent, int depth)
{
  SgfNode *node_copy = sgf_node_duplicate (node, tree, parent);

  assert (depth > 0);

  if (depth > 1) {
    SgfNode **link;

    for (node = node->child, link = &node_copy->child; node;
	 node = node->next, link = & (*link)->next) {
      *link = sgf_node_duplicate_to_given_depth (node, tree, node_copy,
						 depth - 1);
    }
  }

  return node_copy;
}


/* Find a property specified by type in a given node.  If a property
 * of this type is found, (*link) is set to point to the link to the
 * found property and nonzero is returned.  Otherwise, return value is
 * zero (*link) is set to point to the link on which to insert a
 * property of given type.
 *
 * Maybe `SgfProperty ***link' is not the easiest thing to explain,
 * but it is very easy to work with (see `sgf-parser.c'.)
 */
int
sgf_node_find_property (SgfNode *node, SgfType type, SgfProperty ***link)
{
  SgfProperty **internal_link;

  assert (node);

  for (internal_link = &node->properties; *internal_link;
       internal_link = & (*internal_link)->next) {
    if ((*internal_link)->type >= type) {
      if (link)
	*link = internal_link;

      return (*internal_link)->type == type;
    }
  }

  if (link)
    *link = internal_link;

  return 0;
}


/* Find an unknown property by its identifier (name).  Return value
 * and the meaning of (*link) variable are the same as for
 * sgf_node_find_property() function above.
 */
int
sgf_node_find_unknown_property (SgfNode *node, char *id, int length,
				SgfProperty ***link)
{
  assert (node);
  assert (id);
  assert (link);

  if (sgf_node_find_property (node, SGF_UNKNOWN, link)) {
    for (; **link; *link = & (**link)->next) {
      char *stored_id = (**link)->value.unknown_value_list->first->text;
      int relation = strncmp (stored_id, id, length);

      if (relation > 0)
	return 0;
      if (relation == 0 && !stored_id[length])
	return 1;
    }
  }

  return 0;
}


/* Return nonzero if a given node contains at least one game-info
 * property or, in other words, is a game-info node.
 */
int
sgf_node_is_game_info_node (const SgfNode *node)
{
  const SgfProperty *property;

  assert (node);

  for (property = node->properties; property; property = property->next) {
    if (property->type >= SGF_FIRST_GAME_INFO_PROPERTY)
      return property->type <= SGF_LAST_GAME_INFO_PROPERTY;
  }

  return 0;
}



/* Functions below are used for retrieving specific properties'
 * values.  They are of higher level than sgf_node_find_property() and
 * should always be used instead of the latter outside SGF code.
 *
 * Note that their code overlaps with sgf_node_find_property()
 * somewhat, but since it is simple anyway, it seems that the extra
 * couple of lines is a good exchange for the `const' modifier.
 */

/* This macro is used in many of the below function. */
#define GET_PROPERTY_VALUE(type_assertion, return_field, fail_value)	\
  do {									\
    const SgfProperty *property;					\
    assert (node);							\
    assert (type_assertion);						\
    for (property = node->properties;					\
	 property && property->type <= type;				\
	 property = property->next) {					\
      if (property->type == type)					\
	return property->value.return_field;				\
    }									\
    return fail_value;							\
  } while (0)


/* Get the value of property of specified type with `number' value
 * type.  If such a property has not been found, return value is zero
 * and (*number) remains untouched.
 */
int
sgf_node_get_number_property_value (const SgfNode *node, SgfType type,
				    int *number)
{
  const SgfProperty *property;

  assert (node);
  assert (property_info[type].value_type == SGF_NUMBER);

  for (property = node->properties; property && property->type <= type;
       property = property->next) {
    if (property->type == type) {
      *number = property->value.number;
      return 1;
    }
  }

  return 0;
}


/* Get the value of `double' property of specified type.  Value of 1
 * means "normal", 2--"emphasized.  Zero means there's no such
 * property at all.
 */
int
sgf_node_get_double_property_value (const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE (property_info[type].value_type == SGF_DOUBLE,
		      emphasized, 0);
}


/* Get the value of specified `color' property or EMPTY if it is
 * not present.
 */
int
sgf_node_get_color_property_value (const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE (property_info[type].value_type == SGF_COLOR,
		      color, EMPTY);
}


/* Return handicap (`HA') property value if it is stored in the given
 * node, or -1 otherwise.  Handicaps are stored as text internally,
 * because SGF specification requires to preserve illegally formated
 * game-info properties.  Therefore, some validation tricks are
 * necessary before returning anything.
 */
int
sgf_node_get_handicap (const SgfNode *node)
{
  const char *text = sgf_node_get_text_property_value (node, SGF_HANDICAP);

  if (text) {
    int handicap = atoi (text);

    if (0 <= handicap && handicap <= BOARD_MAX_POSITIONS) {
      char buffer[32];

      utils_ncprintf (buffer, sizeof buffer, "%d", handicap);
      if (strcmp (text, buffer) == 0)
	return handicap;
    }
  }

  return -1;
}


int
sgf_node_get_komi (const SgfNode *node, double *komi)
{
  const char *text = sgf_node_get_text_property_value (node, SGF_KOMI);

  if (text)
    return utils_parse_double (text, komi);

  return 0;
}


SgfResult
sgf_node_get_result (const SgfNode *node, double *score)
{
  const char *result = sgf_node_get_text_property_value (node, SGF_RESULT);

  if (result) {
    if (strcmp (result, "?") == 0)
      return SGF_RESULT_UNKNOWN;
    if (strcmp (result, "Draw") == 0 || strcmp (result, "0") == 0)
      return SGF_RESULT_DRAW;
    if (strcmp (result, "Void") == 0)
      return SGF_RESULT_VOID;

    if ((*result == 'B' || *result == 'W') && *(result + 1) == '+') {
      int color_index = (*result == 'B' ? BLACK_INDEX : WHITE_INDEX);

      result += 2;
      if (! *result)
	return SGF_RESULT_WIN | color_index;

      if (strcmp (result, "F") == 0 || strcmp (result, "Forfeit") == 0)
	return SGF_RESULT_WIN_BY_FORFEIT | color_index;
      if (strcmp (result, "R") == 0 || strcmp (result, "Resign") == 0)
	return SGF_RESULT_WIN_BY_RESIGNATION | color_index;
      if (strcmp (result, "T") == 0 || strcmp (result, "Time") == 0)
	return SGF_RESULT_WIN_BY_TIME | color_index;

      if ('0' <= *result && *result <= '9'
	  && utils_parse_double (result, score))
	return SGF_RESULT_WIN_BY_SCORE | color_index;
    }

    return SGF_RESULT_INVALID;
  }

  return SGF_RESULT_NOT_SET;
}


int
sgf_node_get_time_limit (const SgfNode *node, double *time_limit)
{
  const char *text = sgf_node_get_text_property_value (node, SGF_TIME_LIMIT);

  if (text)
    return utils_parse_double (text, time_limit);

  return 0;
}


/* Get the value of `real' property of specified type.  Return value
 * is the same as for sgf_node_get_number_property_value().
 */
int
sgf_node_get_real_property_value (const SgfNode *node, SgfType type,
				  double *value)
{
  const SgfProperty *property;

  assert (node);
  assert (property_info[type].value_type == SGF_REAL);

  for (property = node->properties; property && property->type <= type;
       property = property->next) {
    if (property->type == type) {
#if SGF_REAL_VALUES_ALLOCATED_SEPARATELY
      *value = * property->value.real;
#else
      *value = property->value.real;
#endif

      return 1;
    }
  }

  return 0;
}


const char *
sgf_node_get_text_property_value (const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE
    ((property_info[type].value_type == SGF_SIMPLE_TEXT
      || property_info[type].value_type == SGF_FAKE_SIMPLE_TEXT
      || property_info[type].value_type == SGF_TEXT),
     text, NULL);
}


const BoardPositionList *
sgf_node_get_list_of_point_property_value (const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE
    ((property_info[type].value_type == SGF_LIST_OF_POINT
      || property_info[type].value_type == SGF_ELIST_OF_POINT),
     position_list, NULL);
}


const SgfVectorList *
sgf_node_get_list_of_vector_property_value (const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE (property_info[type].value_type == SGF_LIST_OF_VECTOR,
		      vector_list, NULL);
}


const SgfLabelList *
sgf_node_get_list_of_label_property_value (const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE (property_info[type].value_type == SGF_LIST_OF_LABEL,
		      label_list, NULL);
}



/* Functions for adding/overwriting property values. */

int
sgf_node_add_none_property (SgfNode *node, SgfGameTree *tree, SgfType type)
{
  SgfProperty **link;

  assert (node);
  assert (property_info[type].value_type == SGF_NONE);

  if (!sgf_node_find_property (node, type, &link)) {
    *link = sgf_property_new (tree, type, *link);

    return 1;
  }

  return 0;
}


int
sgf_node_add_number_property (SgfNode *node, SgfGameTree *tree,
			      SgfType type, int number, int overwrite)
{
  SgfProperty **link;

  assert (node);
  assert (property_info[type].value_type == SGF_NUMBER
	  || property_info[type].value_type == SGF_DOUBLE
	  || property_info[type].value_type == SGF_COLOR);

  if (!sgf_node_find_property (node, type, &link)) {
    *link = sgf_property_new (tree, type, *link);
    (*link)->value.number = number;

    return 1;
  }

  if (overwrite) {
    (*link)->value.number = number;
    return 1;
  }

  return 0;
}


int
sgf_node_add_real_property (SgfNode *node, SgfGameTree *tree,
			    SgfType type, double value, int overwrite)
{
  SgfProperty **link;

  assert (node);
  assert (property_info[type].value_type == SGF_REAL);

  if (!sgf_node_find_property (node, type, &link)) {
    *link = sgf_property_new (tree, type, *link);

#if SGF_REAL_VALUES_ALLOCATED_SEPARATELY
    (*link)->value.real = utils_duplicate_buffer (&value, sizeof (double));
#else
    (*link)->value.real = value;
#endif

    return 1;
  }

  if (overwrite) {
#if SGF_REAL_VALUES_ALLOCATED_SEPARATELY
    * (*link)->value.real = value;
#else
    (*link)->value.real = value;
#endif

    return 1;
  }

  return 0;
}


int
sgf_node_add_pointer_property (SgfNode *node, SgfGameTree *tree,
			       SgfType type, void *pointer, int overwrite)
{
  SgfProperty **link;
  void *pointer_to_free;

  assert (node);
  assert (SGF_FIRST_MALLOC_TYPE <= property_info[type].value_type
	  && property_info[type].value_type <= SGF_LAST_MALLOC_TYPE
	  && property_info[type].value_type != SGF_REAL
	  && type != SGF_UNKNOWN);

  if (!sgf_node_find_property (node, type, &link)) {
    *link = sgf_property_new (tree, type, *link);
    (*link)->value.memory_block = pointer;

    return 1;
  }

  pointer_to_free = (overwrite ? (*link)->value.memory_block : pointer);

  switch (property_info[type].value_type) {
  default:
    utils_free (pointer_to_free);
    break;

  case  SGF_LIST_OF_LABEL:
    sgf_label_list_delete (pointer_to_free);
    break;

  case SGF_FIGURE_DESCRIPTION:
    if (pointer_to_free)
      sgf_figure_description_delete (pointer_to_free);

    break;
  }

  if (overwrite) {
    (*link)->value.memory_block = pointer;
    return 1;
  }

  return 0;
}


int
sgf_node_add_score_result (SgfNode *node, SgfGameTree *tree,
			   double score, int overwrite)
{
  char *result;

  if (score < -0.000005 || 0.000005 < score) {
    char color_who_won = (score > 0.0 ? 'B' : 'W');

    if (tree->game == GAME_GO
	&& fabs (score - floor (score + 0.000005)) >= 0.000005)
      result = utils_cprintf ("%c+%.f", color_who_won, fabs (score));
    else
      result = utils_cprintf ("%c+%d", color_who_won, (int) fabs (score));
  }
  else
    result = utils_duplicate_string ("Draw");

  return sgf_node_add_text_property (node, tree, SGF_RESULT, result,
				     overwrite);
}


/* Add a text property to a given node.  If such a property already
 * exists, don't overwrite its value, but instead append new value to
 * the old one, putting `separator' in between.  If this property
 * doesn't yet exist, the `separator' is not used.  If `separator' is
 * NULL, then "\n\n" is used.
 *
 * Return non-zero if the value is appended (zero if added).
 */
int
sgf_node_append_text_property (SgfNode *node, SgfGameTree *tree,
			       SgfType type, char *text, const char *separator)
{
  SgfProperty **link;

  assert (node);
  assert (property_info[type].value_type == SGF_SIMPLE_TEXT
	  || property_info[type].value_type == SGF_FAKE_SIMPLE_TEXT
	  || property_info[type].value_type == SGF_TEXT);

  if (!sgf_node_find_property (node, type, &link)) {
    *link = sgf_property_new (tree, type, *link);
    (*link)->value.memory_block = text;

    return 0;
  }

  (*link)->value.memory_block = utils_cat_strings ((*link)->value.memory_block,
						   (separator
						    ? separator : "\n\n"),
						   text, NULL);
  utils_free (text);

  return 1;
}



/* Find and delete property of given type in the node.  Return nonzero
 * if succeded, or zero if there is no such property.
 */
int
sgf_node_delete_property (SgfNode *node, SgfGameTree *tree, SgfType type)
{
  SgfProperty **link;

  assert (node);

  if (sgf_node_find_property (node, type, &link)) {
    SgfProperty *next_property = (*link)->next;

    sgf_property_delete (*link, tree);
    *link = next_property;

    return 1;
  }

  return 0;
}


/* Split given node into two, leaving all root, game info, setup and
 * node name properties in the node and moving the rest into its child
 * (which is created).
 *
 * This is a low level function, which should only be used by SGF
 * parser.  It assumes that the node doesn't have any children yet.
 */
void
sgf_node_split (SgfNode *node, SgfGameTree *tree)
{
  SgfProperty *property;
  SgfProperty **node_link;
  SgfProperty **child_link;

  assert (tree);
  assert (node);

  node->child = sgf_node_new (tree, node);
  node->child->move_color = node->move_color;
  node->child->move_point = node->move_point;

  node->move_color = EMPTY;

  node_link = &node->properties;
  child_link = &node->child->properties;

  for (property = node->properties; property; property = property->next) {
    /* This is mainly to avoid GCC warning. */
    SgfType type = property->type;

    if ((SGF_FIRST_ROOT_PROPERTY <= type && type <= SGF_LAST_ROOT_PROPERTY)
	|| (SGF_FIRST_GAME_INFO_PROPERTY <= type
	    && type <= SGF_LAST_GAME_INFO_PROPERTY)
	|| (SGF_FIRST_SETUP_PROPERTY <= type
	    && type <= SGF_LAST_SETUP_PROPERTY)
	|| type == SGF_NODE_NAME) {
      *node_link = property;
      node_link = &property->next;
    }
    else {
      *child_link = property;
      child_link = &property->next;
    }
  }

  *node_link = NULL;
  *child_link = NULL;
}


/* Get ``next'' node in tree-traversing sense.  See
 * sgf_game_tree_traverse_forward() for details.
 */
SgfNode *
sgf_node_traverse_forward (const SgfNode *node)
{
  assert (node);

  if (node->child)
    return node->child;

  while (!node->next) {
    node = node->parent;
    if (!node)
      return NULL;
  }

  return node->next;
}


/* Get ``previous'' node in tree-traversing sense.  See
 * sgf_game_tree_traverse_backward() for details.
 */
SgfNode *
sgf_node_traverse_backward (const SgfNode *node)
{
  SgfNode *parent;
  SgfNode *result;

  assert (node);

  parent = node->parent;
  if (!parent)
    return NULL;

  result = parent->child;
  if (result == node)
    return parent;

  while (result->next != node)
    result = result->next;

  while (1) {
    if (!result->child)
      return result;

    result = result->child;
    while (result->next)
      result = result->next;
  }
}


int
sgf_node_count_subtree_nodes (const SgfNode *node)
{
  int num_nodes = 0;

  assert (node);

  /* Count number of nodes in non-branching node sequence. */
  do {
    num_nodes++;
    node = node->child;
  } while (node && !node->next);

  /* Recursively count nodes in all branches, if any. */
  while (node) {
    num_nodes += sgf_node_count_subtree_nodes (node);
    node = node->next;
  }

  return num_nodes;
}



/* Dynamically allocate an SgfProperty structure and initialize its
 * type and pointer to the next property with given values.
 */
inline SgfProperty *
sgf_property_new (SgfGameTree *tree, SgfType type, SgfProperty *next)
{
  SgfProperty *property = memory_pool_alloc (&tree->property_pool);

  property->type = type;
  property->next = next;

  return property;
}


/* Free a previously allocated SgfProperty structure and, if needed,
 * its value.  Only values of `simpletext', `text' or any of list
 * types need to be freed.  Values of other types are stored in the
 * property structure itself.
 */
inline void
sgf_property_delete (SgfProperty *property, SgfGameTree *tree)
{
  assert (property);
  assert (tree);

  free_property_value (property);
  memory_pool_free (&tree->property_pool, property);
}


/* Delete property attached to a given `link'. */
void
sgf_property_delete_at_link (SgfProperty **link, SgfGameTree *tree)
{
  SgfProperty *next_property = (*link)->next;

  sgf_property_delete (*link, tree);
  *link = next_property;
}


/* Copy given `property' and its value for use in the given game
 * `tree'.
 */
SgfProperty *
sgf_property_duplicate (const SgfProperty *property, SgfGameTree *tree,
			SgfProperty *next)
{
  SgfProperty *property_copy = sgf_property_new (tree, property->type, next);

  switch (property_info[property->type].value_type) {
  case SGF_NUMBER:
  case SGF_DOUBLE:
  case SGF_COLOR:
    property_copy->value.number = property->value.number;
    break;

  case SGF_REAL:

#if SGF_REAL_VALUES_ALLOCATED_SEPARATELY
    property_copy->value.real = utils_duplicate_buffer (property->value.real,
							sizeof (double));
#else
    property_copy->value.real = property->value.real;
#endif

    break;

  case SGF_SIMPLE_TEXT:
  case SGF_FAKE_SIMPLE_TEXT:
  case SGF_TEXT:
    property_copy->value.text = utils_duplicate_string (property->value.text);
    break;

  case SGF_TYPE_UNKNOWN:
    property_copy->value.unknown_value_list = string_list_new ();
    string_list_duplicate_items (property_copy->value.unknown_value_list,
				 property->value.unknown_value_list);
    break;

  case SGF_LIST_OF_POINT:
  case SGF_ELIST_OF_POINT:
    property_copy->value.position_list
      = board_position_list_duplicate (property->value.position_list);
    break;

  case SGF_LIST_OF_VECTOR:
    property_copy->value.vector_list
      = sgf_vector_list_duplicate (property->value.vector_list);
    break;

  case SGF_LIST_OF_LABEL:
    property_copy->value.label_list
      = sgf_label_list_duplicate (property->value.label_list);
    break;

  case SGF_FIGURE_DESCRIPTION:
    property_copy->value.figure
      = sgf_figure_description_duplicate (property->value.figure);
    break;

  default:
    /* Make sure all property types are handled. */
    assert (property_info[property->type].value_type == SGF_NONE);
  };

  return property_copy;
}


void
sgf_property_free_value (SgfValueType value_type, SgfValue *value)
{
  /* `SGF_REAL' type may or may not belong to this range. */
  if (SGF_FIRST_MALLOC_TYPE <= value_type
      && value_type <= SGF_LAST_MALLOC_TYPE) {
    switch (value_type) {
    default:
      utils_free (value->memory_block);
      break;

    case SGF_LIST_OF_LABEL:
      sgf_label_list_delete (value->label_list);
      break;

    case SGF_FIGURE_DESCRIPTION:
      if (value->figure)
	sgf_figure_description_delete (value->figure);

      break;

    case SGF_TYPE_UNKNOWN:
      string_list_delete (value->unknown_value_list);
      break;
    }
  }
}


inline static void
free_property_value (SgfProperty *property)
{
  sgf_property_free_value (property_info[property->type].value_type,
			   &property->value);
}




#define SGF_VECTOR_LIST_DEFAULT_INITIAL_SIZE	0x20
#define SGF_VECTOR_LIST_SIZE_INCREMENT		0x80


SgfVectorList *
sgf_vector_list_new (int num_vectors)
{
  SgfVectorList *list;

  if (num_vectors < 0)
    num_vectors = SGF_VECTOR_LIST_DEFAULT_INITIAL_SIZE;

  list = utils_malloc (sizeof (SgfVectorList)
		       + (num_vectors - 1) * sizeof (SgfVector));
  list->allocated_num_vectors = num_vectors;
  list->num_vectors	      = 0;

  return list;
}


SgfVectorList *
sgf_vector_list_shrink (SgfVectorList *list)
{
  assert (list);

  if (list->allocated_num_vectors > list->num_vectors) {
    list = utils_realloc (list,
			  (sizeof (SgfVectorList)
			   + (list->num_vectors - 1) * sizeof (SgfVector)));
    list->allocated_num_vectors = list->num_vectors;
  }

  return list;
}


SgfVectorList *
sgf_vector_list_add_vector (SgfVectorList *list,
			    BoardPoint from_point, BoardPoint to_point)
{
  assert (list);

  if (list->num_vectors == list->allocated_num_vectors) {
    list->allocated_num_vectors += SGF_VECTOR_LIST_SIZE_INCREMENT;
    list = utils_realloc (list,
			  (sizeof (SgfVectorList)
			   + ((list->allocated_num_vectors - 1)
			      * sizeof (SgfVector))));
  }

  list->vectors[list->num_vectors].from_point = from_point;
  list->vectors[list->num_vectors++].to_point = to_point;

  return list;
}


int
sgf_vector_list_has_vector (const SgfVectorList *list,
			    BoardPoint from_point, BoardPoint to_point)
{
  int k;

  assert (list);

  for (k = 0; k < list->num_vectors; k++) {
    if (list->vectors[k].from_point.x	 == from_point.x
	&& list->vectors[k].from_point.y == from_point.y
	&& list->vectors[k].to_point.x	 == to_point.x
	&& list->vectors[k].to_point.y	 == to_point.y)
      return 1;
  }

  return 0;
}


SgfVectorList *
sgf_vector_list_duplicate (const SgfVectorList *list)
{
  SgfVectorList *list_copy;

  assert (list);

  list_copy = utils_malloc (sizeof (SgfVectorList)
			    + (list->num_vectors - 1) * sizeof (SgfVector));
  list_copy->allocated_num_vectors = list->num_vectors;
  list_copy->num_vectors	   = list->num_vectors;

  memcpy (list_copy->vectors, list->vectors,
	  list->num_vectors * sizeof (SgfVector));

  return list_copy;
}



SgfLabelList *
sgf_label_list_new (int num_labels, BoardPoint *points, char **labels)
{
  SgfLabelList *list;
  int k;

  assert (0 <= num_labels && num_labels < BOARD_MAX_POSITIONS);

  list = utils_malloc (sizeof (SgfLabelList)
		       + (num_labels - 1) * sizeof (SgfLabel));
  list->num_labels = num_labels;

  for (k = 0; k < num_labels; k++) {
    list->labels[k].point = points[k];
    list->labels[k].text  = labels[k];
  }

  return list;
}


SgfLabelList *
sgf_label_list_new_empty (int num_labels)
{
  SgfLabelList *list;

  assert (0 <= num_labels && num_labels < BOARD_MAX_POSITIONS);

  list = utils_malloc (sizeof (SgfLabelList)
		       + (num_labels - 1) * sizeof (SgfLabel));
  list->num_labels = num_labels;

  return list;
}


void
sgf_label_list_delete (SgfLabelList *list)
{
  int k;

  assert (list);

  for (k = 0; k < list->num_labels; k++)
    utils_free (list->labels[k].text);

  utils_free (list);
}


const char *
sgf_label_list_get_label (const SgfLabelList *list, BoardPoint point)
{
  assert (list);

  if (list->num_labels > 0) {
    if (list->num_labels > 1) {
      SgfLabel  key   = { { point.x, point.y }, NULL };
      SgfLabel *label = bsearch (&key, list->labels, list->num_labels,
				 sizeof (SgfLabel), compare_sgf_labels);

      if (label)
	return label->text;
    }
    else {
      if (POINTS_ARE_EQUAL (list->labels[0].point, point))
	return list->labels[0].text;
    }
  }

  return NULL;
}


/* FIXME: It is better to hack at existing lists rather than creating
 *	  new ones all the times.  Tweak all
 *	  sgf_node_get_list_of_*_property_value() to return non-const
 *	  values and expand undo module to handle incremental list
 *	  changes.
 */
SgfLabelList *
sgf_label_list_set_label (const SgfLabelList *old_list, BoardPoint point,
			  char *label_text)
{
  SgfLabelList *new_list;
  int num_labels;
  int i;
  int j;

  if (!old_list) {
    if (label_text)
      return sgf_label_list_new (1, &point, &label_text);
    else
      return NULL;
  }

  num_labels = old_list->num_labels;

  if (label_text)
    num_labels++;

  if (sgf_label_list_get_label (old_list, point))
    num_labels--;

  new_list = sgf_label_list_new_empty (num_labels);

  for (i = 0, j = 0;
       (i < old_list->num_labels
	&& POINTS_LESS_THAN (old_list->labels[i].point, point));
       i++, j++) {
    new_list->labels[j].point = old_list->labels[i].point;
    new_list->labels[j].text
      = utils_duplicate_string (old_list->labels[i].text);
  }

  if (i < old_list->num_labels
      && POINTS_ARE_EQUAL (old_list->labels[i].point, point))
    i++;

  if (label_text) {
    new_list->labels[j].point = point;
    new_list->labels[j].text  = label_text;
    j++;
  }

  while (j < num_labels) {
    new_list->labels[j].point = old_list->labels[i].point;
    new_list->labels[j].text
      = utils_duplicate_string (old_list->labels[i].text);

    i++;
    j++;
  }

  return new_list;
}


SgfLabelList *
sgf_label_list_duplicate (const SgfLabelList *list)
{
  SgfLabelList *list_copy = sgf_label_list_new_empty (list->num_labels);
  int k;

  assert (list);

  for (k = 0; k < list->num_labels; k++) {
    list_copy->labels[k].point = list->labels[k].point;
    list_copy->labels[k].text  = utils_duplicate_string (list->labels[k].text);
  }

  return list_copy;
}


int
sgf_label_lists_are_equal (const SgfLabelList *first_list,
			   const SgfLabelList *second_list)
{
  int k;

  assert (first_list);
  assert (second_list);

  if (first_list->num_labels != second_list->num_labels)
    return 0;

  for (k = 0; k < first_list->num_labels; k++) {
    if (!POINTS_ARE_EQUAL (first_list->labels[k].point,
			   second_list->labels[k].point)
	|| (strcmp (first_list->labels[k].text, second_list->labels[k].text)
	    != 0))
      return 0;
  }

  return 1;
}


/* Determine if the `list' contains given `label' at some position. */
int
sgf_label_list_contains_label (const SgfLabelList *list, const char *label)
{
  int k;

  assert (list);
  assert (label);

  for (k = 0; k < list->num_labels; k++) {
    if (strcmp (list->labels[k].text, label) == 0)
      return 1;
  }

  return 0;
}


static int
compare_sgf_labels (const void *first_label, const void *second_label)
{
  const BoardPoint first_point	= ((const SgfLabel *) first_label)->point;
  const BoardPoint second_point = ((const SgfLabel *) second_label)->point;

  if (first_point.y < second_point.y)
    return -1;
  if (first_point.y > second_point.y)
    return 1;

  return (first_point.x < second_point.x
	  ? -1 : (first_point.x > second_point.x ? 1 : 0));
}



SgfFigureDescription *
sgf_figure_description_new (int flags, char *diagram_name)
{
  SgfFigureDescription *figure = utils_malloc (sizeof (SgfFigureDescription));

  figure->flags	       = flags;
  figure->diagram_name = diagram_name;

  return figure;
}


SgfFigureDescription *
sgf_figure_description_duplicate (const SgfFigureDescription *figure)
{
  SgfFigureDescription *figure_copy
    = utils_malloc (sizeof (SgfFigureDescription));

  assert (figure);

  figure_copy->flags	    = figure->flags;
  figure_copy->diagram_name = utils_duplicate_string (figure->diagram_name);

  return figure_copy;
}


void
sgf_figure_description_delete (SgfFigureDescription *figure)
{
  assert (figure);

  utils_free (figure->diagram_name);
  utils_free (figure);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
