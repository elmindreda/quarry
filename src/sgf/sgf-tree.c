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
#include "sgf-privates.h"
#include "board.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#if ENABLE_MEMORY_POOLS

static void	free_property_value(SgfProperty *property);

#endif



/* Dynamically allocate and initialize an SgfCollection structure with
 * no game trees.
 */
SgfCollection *
sgf_collection_new(void)
{
  SgfCollection *collection = utils_malloc(sizeof(SgfCollection));

  collection->num_trees	 = 0;
  collection->first_tree = NULL;
  collection->last_tree	 = NULL;

  return collection;
}


/* Free a previously allocated SgfCollection structure and all its
 * game trees.
 */
void
sgf_collection_delete(SgfCollection *collection)
{
  SgfGameTree *this_tree;

  assert(collection);

  for (this_tree = collection->first_tree; this_tree;) {
    SgfGameTree *next_tree = this_tree->next;

    sgf_game_tree_delete(this_tree);
    this_tree = next_tree;
  }

  utils_free(collection);
}


/* Add a game tree at the end of a collection.  Fields of both
 * structures are updated in order to keep game trees properly linked.
 */
void
sgf_collection_add_game_tree(SgfCollection *collection, SgfGameTree *tree)
{
  assert(collection);
  assert(tree);

  tree->previous = collection->last_tree;
  tree->next = NULL;

  if (collection->last_tree)
    collection->last_tree->next = tree;
  else
    collection->first_tree = tree;

  collection->num_trees++;
  collection->last_tree = tree;
}



/* Dynamically allocate an SgfGametree structure. */
SgfGameTree *
sgf_game_tree_new(void)
{
  SgfGameTree *tree = utils_malloc(sizeof(SgfGameTree));

  tree->previous = NULL;
  tree->next = NULL;

  tree->root = NULL;
  tree->current_node = NULL;

  tree->board = NULL;

  tree->application_name = NULL;
  tree->application_version = NULL;

  tree->node_pool.item_size = 0;

  memory_pool_init(&tree->property_pool, sizeof(SgfProperty));

  return tree;
}


SgfGameTree *
sgf_game_tree_new_with_root(Game game, int board_width, int board_height,
			    int provide_default_setup)
{
  SgfGameTree *tree = sgf_game_tree_new();
  SgfPositionList *black_stones;
  SgfPositionList *white_stones;

  assert(SGF_MIN_BOARD_SIZE <= board_width
	 && board_width <= SGF_MAX_BOARD_SIZE);
  assert(SGF_MIN_BOARD_SIZE <= board_height
	 && board_height <= SGF_MAX_BOARD_SIZE);

  sgf_game_tree_set_game(tree, game);
  tree->board_width = board_width;
  tree->board_height = board_height;

  tree->root = sgf_node_new(tree, NULL);
  tree->current_node = tree->root;

  if (provide_default_setup
      && game_get_default_setup(game, board_width, board_height,
				&black_stones, &white_stones)) {
    tree->root->move_color = SETUP_NODE;

    if (black_stones) {
      sgf_node_add_list_of_point_property(tree->root, tree,
					  SGF_ADD_BLACK, black_stones);
    }

    if (white_stones) {
      sgf_node_add_list_of_point_property(tree->root, tree,
					  SGF_ADD_WHITE, white_stones);
    }
  }

  return tree;
}


/* Free a previously allocated game tree and all its nodes. */
void
sgf_game_tree_delete(SgfGameTree *tree)
{
  assert(tree);

#if ENABLE_MEMORY_POOLS

  memory_pool_traverse(&tree->property_pool,
		       (MemoryPoolCallback) free_property_value);

  memory_pool_flush(&tree->property_pool);
  if (tree->node_pool.item_size > 0)
    memory_pool_flush(&tree->node_pool);

#else

  if (tree->root)
    sgf_node_delete(tree->root, tree);

#endif

  utils_free(tree->application_name);
  utils_free(tree->application_version);

  utils_free(tree);
}


void
sgf_game_tree_set_game(SgfGameTree *tree, Game game)
{
  int node_size;

  assert(tree);
  assert(FIRST_GAME <= game && game <= LAST_GAME);

  tree->game = game;

  if (game == GAME_GO)
    node_size = sizeof(SgfNodeGo);
  else if (game == GAME_OTHELLO)
    node_size = sizeof(SgfNodeOthello);
  else if (game == GAME_AMAZONS)
    node_size = sizeof(SgfNodeAmazons);
  else
    node_size = sizeof(SgfNodeGeneric);

  memory_pool_init(&tree->node_pool, node_size);
}


int
sgf_game_tree_count_nodes(const SgfGameTree *tree)
{
  assert(tree);

#if ENABLE_MEMORY_POOLS
  return memory_pool_count_items(&tree->node_pool);
#else
  return tree->root ? sgf_node_count_sub_tree_nodes(tree->root) : 0;
#endif
}


/* Dynamically allocate an SgfNode structure and links it to the
 * specified parent node.
 */
SgfNode *
sgf_node_new(SgfGameTree *tree, SgfNode *parent)
{
  SgfNode *node;

  node = memory_pool_alloc(&tree->node_pool);
  node->parent		  = parent;
  node->child		  = NULL;
  node->next		  = NULL;
  node->current_variation = NULL;

  node->move_color = EMPTY;
  node->properties = NULL;

  return node;
}


/* Free a previously allocated SgfNode structure and all its
 * properties.  To reduce stack usage, we free nodes in main variation
 * (the one, determined by `node->child') in a loop, rather than with
 * recursion.
 */
void
sgf_node_delete(SgfNode *node, SgfGameTree *tree)
{
  assert(node);

  do {
    SgfProperty *this_property;
    SgfNode *next_node = node->child;

    for (this_property = node->properties; this_property;) {
      SgfProperty *next_property = this_property->next;

      sgf_property_delete(this_property, tree);
      this_property = next_property;
    }

    if (node->next)
      sgf_node_delete(node->next, tree);

    memory_pool_free(&tree->node_pool, node);
    node = next_node;
  } while (node);
}


/* Create a new SGF node and append it as the last child of the given
 * node.
 */
SgfNode *
sgf_node_append_child(SgfNode *node, SgfGameTree *tree)
{
  SgfNode *child;
  SgfNode **link;

  assert(tree);
  assert(node);

  child = sgf_node_new(tree, node);

  link = &node->child;
  while (*link)
    link = & (*link)->next;

  *link = child;

  return child;
}


/* Find a property specified by type in a given node.  If a property
 * of this type is found, (*link) is set to point to the link to the
 * found property and nonzero is returned.  Otherwise, return value is
 * zero (*link) is set to point to the link on which to insert a
 * property of given type.
 *
 * Maybe `SgfProperty ***link' is not the easiest thing to explain,
 * but it is very easy to work with (see `sgf-parser.c').
 */
int
sgf_node_find_property(SgfNode *node, SgfType type, SgfProperty ***link)
{
  assert(node);
  assert(link);

  for (*link = &node->properties; **link; *link = & (**link)->next) {
    if ((**link)->type >= type)
      return (**link)->type == type;
  }

  return 0;
}


/* Find an unknown property by its identifier (name).  Return value
 * and the meaning of (*link) variable are the same as for
 * sgf_node_find_property() function above.
 */
int
sgf_node_find_unknown_property(SgfNode *node, char *id, int length,
			       SgfProperty ***link)
{
  assert(node);
  assert(id);
  assert(link);

  if (sgf_node_find_property(node, SGF_UNKNOWN, link)) {
    for (; **link; *link = & (**link)->next) {
      char *stored_id = (**link)->value.text;
      int relation = strncmp(stored_id, id, length);

      if (relation > 0)
	return 0;
      if (relation == 0 && stored_id[length] == '[')
	return 1;
    }
  }

  return 0;
}


/* Return nonzero if a given node contains at least one game-info
 * property or, in other words, is a game-info node.
 */
int
sgf_node_is_game_info_node(const SgfNode *node)
{
  SgfProperty *const *link;

  assert(node);

  for (link = &node->properties; *link; link = & (*link)->next) {
    if ((*link)->type >= SGF_FIRST_GAME_INFO_PROPERTY)
      return (*link)->type <= SGF_LAST_GAME_INFO_PROPERTY;
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
    SgfProperty *const *link;						\
    assert(node);							\
    assert(type_assertion);						\
    for (link = &node->properties; *link && (*link)->type <= type;	\
	 link = & (*link)->next) {					\
      if ((*link)->type == type)					\
	return (*link)->value.return_field;				\
    }									\
    return fail_value;							\
  } while (0)
    

/* Get the value of property of specified type with `number' value
 * type.  If such a property has not been found, return value is zero
 * and (*number) remains untouched.
 */
int
sgf_node_get_number_property_value(const SgfNode *node, SgfType type,
				   int *number)
{
  SgfProperty *const *link;

  assert(node);
  assert(property_info[type].value_type == SGF_NUMBER);

  for (link = &node->properties; *link && (*link)->type <= type;
       link = & (*link)->next) {
    if ((*link)->type == type) {
      *number = (*link)->value.number;
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
sgf_node_get_double_property_value(const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE(property_info[type].value_type == SGF_DOUBLE,
		     emphasized, 0);
}


/* Get the value of specified `color' property or EMPTY if it is
 * not present.
 */
int
sgf_node_get_color_property_value(const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE(property_info[type].value_type == SGF_COLOR,
		     color, EMPTY);
}


/* Return handicap (`HA') property value if it is stored in the given
 * node, or -1 otherwise.  Handicaps are stored as text internally,
 * because SGF specification requires to preserve illegally formated
 * game-info properties.  Therefore, some validation tricks are
 * necessary before returning anything.
 */
int
sgf_node_get_handicap(const SgfNode *node)
{
  const char *text = sgf_node_get_text_property_value(node, SGF_HANDICAP);

  if (text) {
    int handicap = atoi(text);

    if (0 <= handicap && handicap <= BOARD_MAX_POSITIONS) {
      char buffer[32];

      sprintf(buffer, "%d", handicap);
      if (strcmp(text, buffer) == 0)
	return handicap;
    }
  }

  return -1;
}


/* Get the value of `real' property of specified type.  Return value
 * is the same as for sgf_node_get_number_property_value().
 */
int
sgf_node_get_real_property_value(const SgfNode *node, SgfType type,
				 double *value)
{
  SgfProperty *const *link;

  assert(node);
  assert(property_info[type].value_type == SGF_REAL);

  for (link = &node->properties; *link && (*link)->type <= type;
       link = & (*link)->next) {
    if ((*link)->type == type) {
      *value = * (*link)->value.real;
      return 1;
    }
  }

  return 0;
}


const char *
sgf_node_get_text_property_value(const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE((property_info[type].value_type == SGF_SIMPLE_TEXT
		      || property_info[type].value_type == SGF_FAKE_SIMPLE_TEXT
		      || property_info[type].value_type == SGF_TEXT),
		     text, NULL);
}


const SgfPositionList *
sgf_node_get_list_of_point_property_value(const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE((property_info[type].value_type == SGF_LIST_OF_POINT
		      || property_info[type].value_type == SGF_ELIST_OF_POINT),
		     position_list, NULL);
}


const SgfLabelList *
sgf_node_get_list_of_label_property_value(const SgfNode *node, SgfType type)
{
  GET_PROPERTY_VALUE(property_info[type].value_type == SGF_LIST_OF_LABEL,
		     label_list, NULL);
}


int
sgf_node_add_none_property(SgfNode *node, SgfGameTree *tree, SgfType type)
{
  SgfProperty **link;

  assert(node);
  assert(property_info[type].value_type == SGF_NONE);

  if (!sgf_node_find_property(node, type, &link)) {
    *link = sgf_property_new(tree, type, *link);

    return 1;
  }

  return 0;
}


int
sgf_node_add_number_property(SgfNode *node, SgfGameTree *tree,
			     SgfType type, int number)
{
  SgfProperty **link;

  assert(node);
  assert(property_info[type].value_type == SGF_NUMBER
	 || property_info[type].value_type == SGF_DOUBLE
	 || property_info[type].value_type == SGF_COLOR);

  if (!sgf_node_find_property(node, type, &link)) {
    *link = sgf_property_new(tree, type, *link);
    (*link)->value.number = number;

    return 1;
  }

  return 0;
}


int
sgf_node_add_real_property(SgfNode *node, SgfGameTree *tree,
			   SgfType type, double value)
{
  SgfProperty **link;

  assert(node); 
  assert(property_info[type].value_type == SGF_REAL);

  if (!sgf_node_find_property(node, type, &link)) {
    *link = sgf_property_new(tree, type, *link);
    (*link)->value.real = utils_malloc(sizeof(double));
    * (*link)->value.real = value;

    return 1;
  }

  return 0;
}


int
sgf_node_add_pointer_property(SgfNode *node, SgfGameTree *tree,
			      SgfType type, void *pointer)
{
  SgfProperty **link;

  assert(node);
  assert(SGF_FIRST_MALLOC_TYPE <= property_info[type].value_type
	 && property_info[type].value_type <= SGF_LAST_MALLOC_TYPE
	 && property_info[type].value_type != SGF_REAL
	 && type != SGF_UNKNOWN);

  if (!sgf_node_find_property(node, type, &link)) {
    *link = sgf_property_new(tree, type, *link);
    (*link)->value.memory_block = pointer;

    return 1;
  }

  if (property_info[type].value_type != SGF_LIST_OF_LABEL)
    utils_free(pointer);
  else
    sgf_label_list_delete(pointer);

  return 0;
}


/* Find and delete property of given type in the node.  Return nonzero
 * if succeded, or zero if there is no such property.
 */
int
sgf_node_delete_property(SgfNode *node, SgfGameTree *tree, SgfType type)
{
  SgfProperty **link;

  assert(node);

  if (sgf_node_find_property(node, type, &link)) {
    SgfProperty *next_property = (*link)->next;

    sgf_property_delete(*link, tree);
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
sgf_node_split(SgfNode *node, SgfGameTree *tree)
{
  SgfProperty *property;
  SgfProperty **node_link;
  SgfProperty **child_link;

  assert(tree);
  assert(node);

  node->child = sgf_node_new(tree, node);
  node->child->move_color = node->move_color;
  node->child->move_point = node->move_point;

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


int
sgf_node_count_sub_tree_nodes(const SgfNode *node)
{
  const SgfNode *child;
  int num_nodes = 1;

  assert(node);

  for (child = node->child; child; child = child->next)
    num_nodes += sgf_node_count_sub_tree_nodes(child);

  return num_nodes;
}



/* Dynamically allocate an SgfProperty structure and initialize its
 * type and pointer to the next property with given values.
 */
inline SgfProperty *
sgf_property_new(SgfGameTree *tree, SgfType type, SgfProperty *next)
{
  SgfProperty *property = memory_pool_alloc(&tree->property_pool);

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
sgf_property_delete(SgfProperty *property, SgfGameTree *tree)
{
  assert(property);

  if (SGF_FIRST_MALLOC_TYPE <= property_info[property->type].value_type
      && property_info[property->type].value_type <= SGF_LAST_MALLOC_TYPE
      && property_info[property->type].value_type != SGF_LIST_OF_LABEL)
    utils_free(property->value.memory_block);
  else if (property_info[property->type].value_type == SGF_LIST_OF_LABEL)
    sgf_label_list_delete(property->value.label_list);

  memory_pool_free(&tree->property_pool, property);
}


/* Delete property attached to a given `link'. */
void
sgf_property_delete_at_link(SgfProperty **link, SgfGameTree *tree)
{
  SgfProperty *next_property = (*link)->next;

  sgf_property_delete(*link, tree);
  *link = next_property;
}


#if ENABLE_MEMORY_POOLS


static void
free_property_value(SgfProperty *property)
{
  if (SGF_FIRST_MALLOC_TYPE <= property_info[property->type].value_type
      && property_info[property->type].value_type <= SGF_LAST_MALLOC_TYPE
      && property_info[property->type].value_type != SGF_LIST_OF_LABEL)
    utils_free(property->value.memory_block);
  else if (property_info[property->type].value_type == SGF_LIST_OF_LABEL)
    sgf_label_list_delete(property->value.label_list);
}


#endif /* ENABLE_MEMORY_POOLS */



SgfLabelList *
sgf_label_list_new(int num_labels, SgfPoint *points, char **labels)
{
  SgfLabelList *list;
  int k;

  assert(0 <= num_labels && num_labels < BOARD_MAX_POSITIONS);

  list = utils_malloc(sizeof(SgfLabelList)
		      + (num_labels - 1) * sizeof(SgfLabel));
  list->num_labels = num_labels;

  for (k = 0; k < num_labels; k++) {
    list->labels[k].point = points[k];
    list->labels[k].text = labels[k];
  }

  return list;
}


SgfLabelList *
sgf_label_list_new_empty(int num_labels)
{
  SgfLabelList *list;

  assert(0 <= num_labels && num_labels < BOARD_MAX_POSITIONS);

  list = utils_malloc(sizeof(SgfLabelList)
		      + (num_labels - 1) * sizeof(SgfLabel));
  list->num_labels = num_labels;

  return list;
}


void
sgf_label_list_delete(SgfLabelList *list)
{
  int k;

  assert(list);

  for (k = 0; k < list->num_labels; k++)
    utils_free(list->labels[k].text);

  utils_free(list);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
