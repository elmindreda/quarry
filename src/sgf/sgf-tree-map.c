/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004, 2005 Paul Pogonyshev.                       *
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

/* Generate flat ``maps'' of SGF trees.  The used layout is the same
 * as in cGoban 1, but the speed is much higher :-)
 *
 * Work is distributed like this:
 *
 * - update_internal_map_data() calculates map width and height and,
 *   for very large trees, stores some internal data at some nodes;
 *   this data is later used to remap only necessary tree chunks
 *   instead of the whole tree.
 *
 * - update_internal_view_port() uses do_update_internal_view_port()
 *   to actually generate a rectangular piece of the map (view port),
 *   using the data previously stored by update_internal_map_data().
 */


#include "sgf.h"
#include "utils.h"

#include <assert.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


/* Define to non-zero to make various functions report run times and
 * other performance information.
 */
#define REPORT_PERFORMANCE_STATISTICS	0


#if REPORT_PERFORMANCE_STATISTICS


#include <stdio.h>
#include <sys/time.h>


#define DECLARE_TIME_VARIABLES						\
  struct timeval start_time;						\
  struct timeval finish_time

#define STORE_STARTING_TIME	gettimeofday (&start_time, NULL)

#define PRINT_RUN_TIME(function_name)					\
  do {									\
    gettimeofday (&finish_time, NULL);					\
    fprintf (stderr, "Time spent in " #function_name "(): %f\n",	\
	     ((finish_time.tv_usec - start_time.tv_usec) * 0.000001	\
	      + (finish_time.tv_sec - start_time.tv_sec)));		\
  } while (0)


#else /* not REPORT_PERFORMANCE_STATISTICS */

#define DECLARE_TIME_VARIABLES
#define STORE_STARTING_TIME
#define PRINT_RUN_TIME(function_name)

#endif /* not REPORT_PERFORMANCE_STATISTICS */


#define MIN_NODES_PER_DATA_CHUNK	0x4000


#define Y_LEVEL_ARRAY_INITIAL_SIZE	0x400

/* Must be a power of two. */
#define Y_LEVEL_ARRAY_SIZE_GRAIN	0x400


#define VIEW_PORT_NODE(tree, x, y)				\
  ((tree)->view_port_nodes					\
   + (((y) - (tree)->view_port_y0)				\
      * ((tree)->view_port_x1 - (tree)->view_port_x0))		\
   + ((x) - (tree)->view_port_x0))

#define VIEW_PORT_LINE_ADJUSTMENT(tree, width)			\
  (((tree)->view_port_x1 - (tree)->view_port_x0) - (width))


static SgfGameTreeMapData *  find_intermediate_data_for_node
			       (const SgfGameTree *tree, const SgfNode *node,
				int strictly_before_node);

static int		     view_port_is_after_data_point
			       (const SgfGameTree *tree,
				const SgfGameTreeMapData *intermediate_data);
static int		     view_port_is_before_data_point
			       (const SgfGameTree *tree,
				const SgfGameTreeMapData *intermediate_data);

static void		     free_map_data_point
			       (SgfGameTreeMapData *data_point);

static void		     update_internal_map_data (SgfGameTree *tree);

static void		     update_internal_view_port (SgfGameTree *tree,
							int view_port_x0,
							int view_port_y0,
							int view_port_x1,
							int view_port_y1);
static SgfGameTreeMapLine *  do_update_internal_view_port
			       (const SgfGameTree *tree,
				const SgfGameTreeMapData *intermediate_data,
				SgfGameTreeMapLine *line_pointer);
static int		     get_node_coordinates (SgfGameTree *tree,
						   const SgfNode *node_to_find,
						   int *node_x, int *node_y);


/* Invalidate `tree's map or a portion of it.  If `node' is non-NULL,
 * assume that the map remains valid everywhere _before_ the node (in
 * sgf_node_traverse_backward() sense) and invalidate the rest.  If
 * `node' is NULL (or equals to `tree->root'), invalidate the entire
 * map.
 *
 * Invalidating entire map always frees all allocated memory.
 */
void
sgf_game_tree_invalidate_map (SgfGameTree *tree, SgfNode *node)
{
  assert (tree);

  if (tree->map_data_list) {
    SgfGameTreeMapData *last_valid_data_point
      = (node ? find_intermediate_data_for_node (tree, node, 1) :NULL);

    if (last_valid_data_point) {
      SgfGameTreeMapData *data_point;
      int invalidate_view_port = 0;

      if (last_valid_data_point != tree->last_valid_data_point) {
	for (data_point = last_valid_data_point->next;
	     data_point != tree->last_valid_data_point;
	     data_point = data_point->next) {
	  utils_free (data_point->y_level);
	  data_point->y_level = NULL;
	}

	tree->last_valid_data_point = last_valid_data_point;

	if (tree->view_port_nodes) {
	  invalidate_view_port
	    = !view_port_is_before_data_point (tree, last_valid_data_point);
	}
      }

      if (!invalidate_view_port)
	return;
    }
    else {
      free_map_data_point (tree->map_data_list);
      tree->map_data_list = NULL;
    }
  }

  /* Invalidate the view port. */
  utils_free (tree->view_port_nodes);
  utils_free (tree->view_port_lines);

  tree->view_port_nodes = NULL;
  tree->view_port_lines = NULL;
}


void
sgf_game_tree_get_map_dimensions (SgfGameTree *tree,
				  int *map_width, int *map_height)
{
  assert (tree);

  if (map_width || map_height) {
    update_internal_map_data (tree);

    if (map_width)
      *map_width = tree->map_width;

    if (map_height)
      *map_height = tree->map_height;
  }
}


void
sgf_game_tree_fill_map_view_port (SgfGameTree *tree,
				  int view_port_x0, int view_port_y0,
				  int view_port_x1, int view_port_y1,
				  SgfNode ***view_port_nodes,
				  SgfGameTreeMapLine **view_port_lines,
				  int *num_view_port_lines)
{
  int view_port_width  = view_port_x1 - view_port_x0;
  int view_port_height = view_port_y1 - view_port_y0;

  assert (tree);
  assert (0 <= view_port_x0 && view_port_x0 < view_port_x1);
  assert (0 <= view_port_y0 && view_port_y0 < view_port_y1);
  assert (view_port_nodes);
  assert (view_port_lines);
  assert (num_view_port_lines);

  update_internal_view_port (tree,
			     view_port_x0, view_port_y0,
			     view_port_x1, view_port_y1);

  if (tree->view_port_x0 == view_port_x0
      && tree->view_port_x1 == view_port_x1) {
    /* Just duplicate the required portion of view port. */
    *view_port_nodes
      = utils_duplicate_buffer ((tree->view_port_nodes
				 + ((view_port_y0 - tree->view_port_y0)
				    * view_port_width)),
				(view_port_width * view_port_height
				 * sizeof (SgfNode *)));
  }
  else {
    SgfNode **copy_pointer;
    SgfNode **original_pointer;
    int y;

    /* A little more complication. */
    *view_port_nodes = utils_malloc (view_port_width * view_port_height
				     * sizeof (SgfNode *));

    copy_pointer     = *view_port_nodes;
    original_pointer = VIEW_PORT_NODE (tree, view_port_x0, view_port_y0);

    for (y = 0; y < view_port_height; y++) {
      memcpy (copy_pointer, original_pointer,
	      view_port_width * sizeof (SgfNode *));

      copy_pointer     += view_port_width;
      original_pointer += tree->view_port_x1 - tree->view_port_x0;
    }
  }

  *view_port_lines = utils_duplicate_buffer (tree->view_port_lines,
					     (tree->num_view_port_lines
					      * sizeof (SgfGameTreeMapLine)));
  *num_view_port_lines = tree->num_view_port_lines;
}


/* FIXME: Urgently releasing 0.1.12, this function is not yet used and
 *	  is completely untested.
 */
char *
sgf_game_tree_get_current_branch_marks (SgfGameTree *tree, 
					int view_port_x0, int view_port_y0,
					int view_port_x1, int view_port_y1)
{
  int view_port_width  = view_port_x1 - view_port_x0;
  int view_port_height = view_port_y1 - view_port_y0;
  char *current_branch_marks = utils_malloc (view_port_width * view_port_height
					     * sizeof (char));
  const SgfNode *const *view_port_scan;
  char *current_branch_marks_scan;
  int x;
  int y;
  SgfNode *node;

  assert (tree);
  assert (tree->current_node);
  assert (0 <= view_port_x0 && view_port_x0 < view_port_x1);
  assert (0 <= view_port_y0 && view_port_y0 < view_port_y1);

  update_internal_view_port (tree,
			     view_port_x0, view_port_y0,
			     view_port_x1, view_port_y1);

  for (view_port_scan = ((const SgfNode *const *)
			 VIEW_PORT_NODE (tree, view_port_x0, view_port_y0)),
	 current_branch_marks_scan = current_branch_marks,
	 y = view_port_y0;
       y < view_port_y1;
       view_port_scan += VIEW_PORT_LINE_ADJUSTMENT (tree, view_port_width),
	 y++) {
    for (x = view_port_x0; x < view_port_x1; x++) {
      if (*view_port_scan++)
	*current_branch_marks_scan++ = SGF_NON_CURRENT_BRANCH_NODE;
      else
	*current_branch_marks_scan++ = SGF_NO_NODE;
    }
  }

  node = tree->current_node;
  for (x = tree->current_node_depth; x > view_port_x0; x--)
    node = node->parent;

  do {
    if (x >= view_port_x0) {
      for (view_port_scan = ((const SgfNode *const *)
			     VIEW_PORT_NODE (tree, x, view_port_y0)),
	     y = view_port_y0;
	   y < view_port_y1;
	   view_port_scan += tree->view_port_x1 - tree->view_port_x0, y++) {
	if (*view_port_scan == node) {
	  * (current_branch_marks
	     + (y - view_port_y0) * view_port_width
	     + (x - view_port_x0))
	    = (x < tree->current_node_depth
	       ? SGF_CURRENT_BRANCH_HEAD_NODE
	       : (x > tree->current_node_depth
		  ? SGF_CURRENT_BRANCH_TAIL_NODE : SGF_CURRENT_NODE));

	  break;
	}
      }
    }

    if (!node->current_variation) {
      if (!node->child)
	return current_branch_marks;

      node->current_variation = node->child;
    }

    node = node->current_variation;
    x++;
  } while (x < view_port_x1);

  return current_branch_marks;
}


/* Store given `node' coordinates in the tree map in `node_x' and
 * `node_y' and return non-zero.  However, this function can fail with
 * zero return value if the node is in a collapsed subtree (and thus
 * is not included in the map.)  In this case `node_x' and `node_y'
 * are not modified.
 */
int
sgf_game_tree_get_node_coordinates (SgfGameTree *tree, const SgfNode *node,
				    int *node_x, int *node_y)
{
  assert (tree);
  assert (node);
  assert (node_x);
  assert (node_y);

  /* First try simple solution. */
  if (tree->view_port_nodes
      && sgf_game_tree_node_is_within_view_port (tree, node,
						 tree->view_port_x0,
						 tree->view_port_y0,
						 tree->view_port_x1,
						 tree->view_port_y1,
						 node_x, node_y))
    return 1;

  return get_node_coordinates (tree, node, node_x, node_y);
}


/* Check if the given `node' is within specified map view port and
 * return non-zero if it is.  If `node_x' and `node_y' are non-NULL,
 * `node's coordinates are stored in them.
 *
 * It is legal to call this function with a different from current
 * view port, but this is not very efficient, as it will force view
 * port update.  You should probably use
 * sgf_game_tree_get_node_coordinates() instead in such cases.
 */
int
sgf_game_tree_node_is_within_view_port (SgfGameTree *tree, const SgfNode *node,
					int view_port_x0, int view_port_y0,
					int view_port_x1, int view_port_y1,
					int *node_x, int *node_y)
{
  const SgfNode *const *view_port_scan;
  int view_port_width = view_port_x1 - view_port_x0;
  int x;
  int y;

  assert (tree);
  assert (node);
  assert (0 <= view_port_x0 && view_port_x0 < view_port_x1);
  assert (0 <= view_port_y0 && view_port_y0 < view_port_y1);

  update_internal_view_port (tree,
			     view_port_x0, view_port_y0,
			     view_port_x1, view_port_y1);

  for (view_port_scan = ((const SgfNode *const *)
			 VIEW_PORT_NODE (tree, view_port_x0, view_port_y0)),
	 y = view_port_y0;
       y < view_port_y1;
       view_port_scan += VIEW_PORT_LINE_ADJUSTMENT (tree, view_port_width),
	 y++) {
    for (x = view_port_x0; x < view_port_x1; x++) {
      if (*view_port_scan++ == node) {
	if (node_x)
	  *node_x = x;

	if (node_y)
	  *node_y = y;

	return 1;
      }
    }
  }

  return 0;
}




static SgfGameTreeMapData *
find_intermediate_data_for_node (const SgfGameTree *tree, const SgfNode *node,
				 int strictly_before_node)
{
  SgfGameTreeMapData *intermediate_data;

  if (strictly_before_node || !node->has_intermediate_map_data) {
    do {
      /* This is essentially a loop of inlined
       * sgf_node_traverse_backward().  However, we only stop at
       * leaves, since intermediate map data is stored only in leaves.
       */

      SgfNode *previous_child;

      while (node->parent && node->parent->child == node)
	node = node->parent;

      if (!node->parent)
	return NULL;

      previous_child = node->parent->child;
      while (previous_child->next != node)
	previous_child = previous_child->next;

      for (node = previous_child; node->child; ) {
	node = node->child;
	while (node->next)
	  node = node->next;
      }
    } while (!node->has_intermediate_map_data);
  }

  for (intermediate_data = tree->map_data_list;
       (intermediate_data != tree->last_valid_data_point
	&& intermediate_data->node != node);)
    intermediate_data = intermediate_data->next;

  return intermediate_data;
}


static int
view_port_is_after_data_point (const SgfGameTree *tree,
			       const SgfGameTreeMapData *intermediate_data)
{
  int last_valid_y_level = intermediate_data->last_valid_y_level;
  int x;

  if (tree->view_port_x0 > last_valid_y_level) {
    return (tree->view_port_y0
	    > intermediate_data->y_level[last_valid_y_level]);
  }

  for (x = tree->view_port_x0;
       x < tree->view_port_x1 && x <= last_valid_y_level; x++) {
    if (tree->view_port_y0 <= intermediate_data->y_level[x])
      return 0;
  }

  return 1;
}


static int
view_port_is_before_data_point (const SgfGameTree *tree,
				const SgfGameTreeMapData *intermediate_data)
{
  int last_valid_y_level = intermediate_data->last_valid_y_level;
  int x;

  if (tree->view_port_x0 > last_valid_y_level) {
    return (tree->view_port_y1
	    <= intermediate_data->y_level[last_valid_y_level] + 1);
  }

  for (x = tree->view_port_x0;
       x < tree->view_port_x1 && x <= last_valid_y_level; x++) {
    if (tree->view_port_y1 > intermediate_data->y_level[x] + 1)
      return 0;
  }

  return 1;
}


static void
free_map_data_point (SgfGameTreeMapData *data_point)
{
  while (data_point) {
    SgfGameTreeMapData *next_data_point = data_point->next;

    data_point->node->has_intermediate_map_data = 0;

    utils_free (data_point->y_level);
    utils_free (data_point);

    data_point = next_data_point;
  }
}


/* Recompute map width and height and, maybe, internal data, if the
 * tree is large enough.  The algorithm is explained in the comments
 * in the function.
 */
static void
update_internal_map_data (SgfGameTree *tree)
{
  SgfNode *node;
  int node_count;
  int x;
  int largest_x_so_far;
  int *y_level;
  int y_level_array_size;
  int last_valid_y_level;
  SgfGameTreeMapData **intermediate_data_link;
  DECLARE_TIME_VARIABLES;

  STORE_STARTING_TIME;

  if (tree->map_data_list && !tree->last_valid_data_point) {
    /* Nothing to update, everything is valid. */
    return;
  }

  if (tree->map_data_list) {
    /* We have at least some chunks of intermediate data left from a
     * previous run.  Skip corresponding part of the tree.
     */
    last_valid_y_level = tree->last_valid_data_point->last_valid_y_level;
    y_level_array_size = ROUND_UP (last_valid_y_level + 1,
				   Y_LEVEL_ARRAY_SIZE_GRAIN);
    y_level	       = utils_malloc (y_level_array_size * sizeof (int));
    memcpy (y_level, tree->last_valid_data_point->y_level,
	    (last_valid_y_level + 1) * sizeof (int));

    node	       = tree->last_valid_data_point->node;
    x		       = tree->last_valid_data_point->x;
    largest_x_so_far   = tree->last_valid_data_point->largest_x_so_far;

    node_count	       = 0;

    intermediate_data_link = &tree->last_valid_data_point->next;

    /* Maybe there are leftovers of invalidated data. */
    free_map_data_point (*intermediate_data_link);
  }
  else {
    /* We have no data left from the previous run or there was no
     * previous run at all.  Start from scratch.
     */
    y_level_array_size = Y_LEVEL_ARRAY_INITIAL_SIZE;
    y_level	       = utils_malloc (y_level_array_size * sizeof (int));
    y_level[0]	       = 0;
    last_valid_y_level = 0;

    /* First scan the tree ``trunk'', i.e. the main variation.  It is
     * a little different from all else variations in that starting
     * `x' is zero.  So it is broken out of the main loop below.
     */
    node = tree->root;
    x    = 0;

    while (node->child && !node->is_collapsed) {
      node = node->child;
      x++;
    }

    largest_x_so_far = x;
    node_count	     = x + 1;

    intermediate_data_link = &tree->map_data_list;
  }

  while (1) {
    /* Find the next branch fork by ascending the last layed out
     * branch in the root direction.
     */
    while (!node->next) {
      if (!node->parent) {
	/* No more branch forks as we are at the root. */
	goto finished;
      }

      x--;
      node = node->parent;
    }

    node = node->next;
    node_count++;

    if (x < last_valid_y_level) {
      /* This case is the most complicated.  The branch may bend with
       * its first part going diagonally, like that marked with '@'
       * below:
       *
       *	...-O-O-O-O-O-O
       *	     \ \
       *	      @ O-O
       *	       \
       *		@-@-@
       */

      if (node->child && !node->is_collapsed) {
	SgfNode *branch_scan_node;
	int branch_scan_x;
	int branch_highest_y;
	int branch_scan_delta;
	int branch_y;

	/* First scan the branch to find its main part's `y'
	 * coordinate.  Since `y_level' doesn't change past
	 * `last_valid_y_level', we stop if we reach it.
	 */

	branch_scan_node = node->child;
	branch_scan_x	 = x + 1;

	while (branch_scan_x < last_valid_y_level
	       && branch_scan_node->child
	       && !branch_scan_node->is_collapsed) {
	  branch_scan_node = branch_scan_node->child;
	  branch_scan_x++;
	}

	branch_highest_y = y_level[branch_scan_x] + 1;

	/* Now find where the branch bends.  More precisely, we find
	 * such `y' coordinate of the first branch node that bended
	 * branch doesn't collide with any already layed out branches.
	 */
	for (branch_y = y_level[x] + 1, branch_scan_delta = 1;
	     branch_scan_delta < branch_highest_y - branch_y;
	     branch_scan_delta++) {
	  branch_y = MAX (branch_y,
			  (y_level[x + branch_scan_delta] + 1
			   - branch_scan_delta));
	}

	/* Update `y_level' for (x - 1) (because of the branch
	 * attachment line) and the diagonal part of the branch.
	 */

	y_level[x - 1] = branch_y - 1;
	y_level[x]     = branch_y;

	while (branch_y < branch_highest_y) {
	  y_level[++x] = ++branch_y;
	  node	       = node->child;
	  node_count++;
	}

	if (branch_y >= y_level[last_valid_y_level]) {
	  /* We reach a new highest `y' for the map.  Truncate
	   * `y_level'.
	   */
	  last_valid_y_level = x;

	  /* Skip the rest of the nodes. */
	  while (node->child && !node->is_collapsed) {
	    x++;
	    node = node->child;
	    node_count++;
	  }
	}
	else {
	  int x_scan;

	  /* Skip the rest of the nodes, but also update `y_level'.
	   * We are guaranteed to remain in the allocated part of it,
	   * since `branch_y' is small enough.
	   */
	  while (node->child && !node->is_collapsed) {
	    y_level[++x] = branch_y;
	    node	 = node->child;
	    node_count++;
	  }

	  /* We need to update `y_level'. */
	  for (x_scan = x + 1; y_level[x_scan] < branch_y; x_scan++)
	    y_level[x_scan] = branch_y;
	}
      }
      else {
	/* In case the branch has only one (non-collapsed) node, we
	 * skip all the above complications at once.
	 */
	y_level[x - 1] = y_level[x]++;

	if (y_level[x] >= y_level[last_valid_y_level]) {
	  /* New highest `y' coordinate. */
	  last_valid_y_level = x;
	}
	else {
	  int y = y_level[x];
	  int x_scan;

	  /* We need to update `y_level'. */
	  for (x_scan = x + 1; y_level[x_scan] < y; x_scan++)
	    y_level[x_scan] = y;
	}
      }
    }
    else {
      /* The branch starts at or beyond `last_valid_y_level'.  It can
       * only go horizontally, so this case is simpler.
       */

      if (x > last_valid_y_level) {
	int previous_branch_y;

	/* We are past `last_valid_y_level'.  Here is an example:
	 *
	 *	...-O-O
	 *	     \
	 *	      O-O-O-O-O-O-O-O
	 *	      ^		 \
	 *	      |		  @-@-@-@
	 *   `last_valid_y_level'
	 */

	if (x > y_level_array_size) {
	  /* The only case when we need to increase the `y_level'
	   * array size.
	   */
	  y_level_array_size = ROUND_UP (x, Y_LEVEL_ARRAY_SIZE_GRAIN);
	  y_level	     = utils_realloc (y_level,
					      (y_level_array_size
					       * sizeof (int)));
	}

	/* Fill `y_level' between `last_valid_y_level' and `x', at the
	 * same time setting the former to be equal to the latter.
	 */
	previous_branch_y = y_level[last_valid_y_level];
	while (++last_valid_y_level < x)
	  y_level[last_valid_y_level] = previous_branch_y;

	y_level[x] = previous_branch_y + 1;
      }
      else {
	/* `x' is the same as `last_valid_y_level'.  Basically the
	 * same as the above case except that we don't need to
	 * increase `last_valid_y_level', so this all reduces to a
	 * single statement.
	 */
	y_level[x - 1] = y_level[x]++;
      }

      /* Skip the nodes.  They are all beyond `last_valid_y_level', so
       * we don't need to update `y_level'.
       */
      while (node->child && !node->is_collapsed) {
	x++;
	node = node->child;
	node_count++;
      }
    }

    /* Maybe this branch'es leaf is beyond the known tree width? */
    if (x > largest_x_so_far)
      largest_x_so_far = x;

    if (node_count >= MIN_NODES_PER_DATA_CHUNK) {
      SgfGameTreeMapData *intermediate_data
	= utils_malloc (sizeof (SgfGameTreeMapData));

      intermediate_data->node		    = node;
      intermediate_data->x		    = x;
      intermediate_data->y_level
	= utils_duplicate_buffer (y_level,
				  (last_valid_y_level + 1) * sizeof (int));
      intermediate_data->last_valid_y_level = last_valid_y_level;

      intermediate_data->largest_x_so_far   = largest_x_so_far;

      *intermediate_data_link = intermediate_data;
      intermediate_data_link  = &intermediate_data->next;

      node->has_intermediate_map_data = 1;

      node_count = 0;
    }
  }

 finished:
  /* Fix map width by adding missing 1.  Write map height. */
  tree->map_width  = largest_x_so_far + 1;
  tree->map_height = y_level[last_valid_y_level] + 1;

  /* All intermediate data is valid. */
  tree->last_valid_data_point = NULL;

  /* Terminate the list. */
  *intermediate_data_link = NULL;

  utils_free (y_level);

  PRINT_RUN_TIME (update_internal_map_data);
}


static void
update_internal_view_port (SgfGameTree *tree,
			   int view_port_x0, int view_port_y0,
			   int view_port_x1, int view_port_y1)
{
  int view_port_width  = view_port_x1 - view_port_x0;
  int view_port_height = view_port_y1 - view_port_y0;
  SgfGameTreeMapLine *line_pointer;
  DECLARE_TIME_VARIABLES;

#if REPORT_PERFORMANCE_STATISTICS

  int num_total_tree_chunks;
  int num_skipped_tree_chunks = 0;
  const SgfGameTreeMapData *intermediate_data_scan;

  for (intermediate_data_scan = tree->map_data_list, num_total_tree_chunks = 1;
       intermediate_data_scan;
       intermediate_data_scan = intermediate_data_scan->next)
    num_total_tree_chunks++;

#endif /* not REPORT_PERFORMANCE_STATISTICS */

  if (tree->view_port_nodes
      && tree->view_port_x0 <= view_port_x0
      && tree->view_port_y0 <= view_port_y0
      && tree->view_port_x1 >= view_port_x1
      && tree->view_port_y1 >= view_port_y1) {
    /* Nothing to update. */
    return;
  }

  tree->view_port_x0 = view_port_x0;
  tree->view_port_y0 = view_port_y0;
  tree->view_port_x1 = view_port_x1;
  tree->view_port_y1 = view_port_y1;

  update_internal_map_data (tree);

  STORE_STARTING_TIME;

  utils_free (tree->view_port_nodes);
  utils_free (tree->view_port_lines);

  tree->view_port_nodes = utils_malloc0 (view_port_width * view_port_height
					 * sizeof (SgfNode *));

  /* FIXME: Too large (but not too important.) */
  tree->view_port_lines = utils_malloc (3 * view_port_width * view_port_height
					* sizeof (SgfGameTreeMapLine));
  line_pointer		= tree->view_port_lines;

  if (tree->map_data_list) {
    const SgfGameTreeMapData *intermediate_data	     = NULL;
    const SgfGameTreeMapData *next_intermediate_data = tree->map_data_list;

    /* FIXME: There is still a lot of space for improvement here.
     *	      This code performs well in near-leaves parts of the
     *	      tree, but doesn't skip much around tree root.
     */
    while (view_port_is_after_data_point (tree, next_intermediate_data)) {
      intermediate_data	     = next_intermediate_data;
      next_intermediate_data = intermediate_data->next;

#if REPORT_PERFORMANCE_STATISTICS
      num_skipped_tree_chunks++;
#endif

      if (!next_intermediate_data)
	break;
    }

    while (1) {
      line_pointer = do_update_internal_view_port (tree, intermediate_data,
						   line_pointer);

      intermediate_data = next_intermediate_data;
      if (!intermediate_data
	  || view_port_is_before_data_point (tree, intermediate_data))
	break;

      next_intermediate_data = intermediate_data->next;
    }

#if REPORT_PERFORMANCE_STATISTICS

    while (intermediate_data) {
      num_skipped_tree_chunks++;
      intermediate_data = intermediate_data->next;
    }

#endif
  }
  else
    line_pointer = do_update_internal_view_port (tree, NULL, line_pointer);

  tree->num_view_port_lines = (line_pointer - tree->view_port_lines);
  tree->view_port_lines
    = utils_realloc (tree->view_port_lines,
		     (char *) line_pointer - (char *) tree->view_port_lines);

  PRINT_RUN_TIME (update_internal_view_port);

#if REPORT_PERFORMANCE_STATISTICS

  fprintf (stderr,
	   "update_internal_view_port(): Skipped %d of %d tree chunks\n",
	   num_skipped_tree_chunks, num_total_tree_chunks);

#endif
}


#define IS_WITHIN_VIEW_PORT(x, y)					\
  (view_port_x0 <= (x) && (x) < view_port_x1				\
   && view_port_y0 <= (y) && (y) < view_port_y1)

#define WRITE_VIEW_PORT_NODE(x, y, node)				\
  do {									\
    * (tree->view_port_nodes +						\
       ((y) - view_port_y0) * view_port_width + ((x) - view_port_x0))	\
      = (node);								\
  } while (0)


/* Lay out a part of the `tree' specified by `intermediate_data' (NULL
 * means the part starting at the tree root.)  Algorithm is
 * essentially the same as used in update_internal_map_data(), but
 * this function has to watch if layed out nodes are within the
 * requested view ports and track connection lines.
 */
static SgfGameTreeMapLine *
do_update_internal_view_port (const SgfGameTree *tree,
			      const SgfGameTreeMapData *intermediate_data,
			      SgfGameTreeMapLine *line_pointer)
{
  int view_port_x0    = tree->view_port_x0;
  int view_port_y0    = tree->view_port_y0;
  int view_port_x1    = tree->view_port_x1;
  int view_port_y1    = tree->view_port_y1;
  int view_port_width = view_port_x1 - view_port_x0;
  SgfNode *node;
  int x;
  int y;
  int *y_level;
  int y_level_array_size;
  int last_valid_y_level;
  DECLARE_TIME_VARIABLES;

  STORE_STARTING_TIME;

  if (intermediate_data) {
    last_valid_y_level = intermediate_data->last_valid_y_level;
    y_level_array_size = ROUND_UP (last_valid_y_level + 1,
				   Y_LEVEL_ARRAY_SIZE_GRAIN);
    y_level	       = utils_malloc (y_level_array_size * sizeof (int));
    memcpy (y_level, intermediate_data->y_level,
	    (last_valid_y_level + 1) * sizeof (int));

    node	       = intermediate_data->node;
    x		       = intermediate_data->x;
  }
  else {
    y_level_array_size = Y_LEVEL_ARRAY_INITIAL_SIZE;
    y_level	       = utils_malloc (y_level_array_size * sizeof (int));
    y_level[0]	       = 0;
    last_valid_y_level = 0;

    for (node = tree->root, x = 0; ; ) {
      if (IS_WITHIN_VIEW_PORT (x, 0))
	WRITE_VIEW_PORT_NODE (x, 0, node);

      if (!node->child || node->is_collapsed)
	break;

      x++;
      node = node->child;
    }

    if (view_port_y0 == 0 && view_port_x0 <= x) {
      line_pointer->x0 = 0;
      line_pointer->y0 = 0;
      line_pointer->y1 = 0;
      line_pointer->x2 = 0;
      line_pointer->x3 = x;

      line_pointer++;
    }
  }

  do {
    while (!node->next) {
      if (!node->parent)
	goto finished;

      x--;
      node = node->parent;
    }

    node = node->next;

    if (x < last_valid_y_level) {
      if (node->child && !node->is_collapsed) {
	SgfNode *branch_scan_node = node->child;
	int branch_scan_delta;
	int branch_leaf_x = x + 1;
	int branch_leaf_y;
	int have_node_within_view_port = 0;

	while (branch_scan_node->child && !branch_scan_node->is_collapsed) {
	  branch_scan_node = branch_scan_node->child;
	  branch_leaf_x++;
	}

	branch_leaf_y = y_level[MIN (branch_leaf_x, last_valid_y_level)] + 1;

	for (y = y_level[x] + 1, branch_scan_delta = 1;
	     branch_scan_delta < branch_leaf_y - y; branch_scan_delta++)
	  y = MAX (y, y_level[x + branch_scan_delta] + 1 - branch_scan_delta);

	line_pointer->x0 = x - 1;
	line_pointer->y0 = y_level[x - 1];

	y_level[x - 1] = y - 1;

	for (; y < branch_leaf_y; x++, y++, node = node->child) {
	  y_level[x] = y;

	  if (IS_WITHIN_VIEW_PORT (x, y)) {
	    WRITE_VIEW_PORT_NODE (x, y, node);
	    have_node_within_view_port = 1;
	  }
	}

	if (IS_WITHIN_VIEW_PORT (x, y)) {
	  WRITE_VIEW_PORT_NODE (x, y, node);
	  have_node_within_view_port = 1;
	}

	line_pointer->x2 = x;

	if (y >= y_level[last_valid_y_level]) {
	  y_level[x] = y;
	  last_valid_y_level = x;
	}
	else {
	  do
	    y_level[x++] = y;
	  while (y_level[x] < y);

	  x = line_pointer->x2;
	}

	while (x < branch_leaf_x) {
	  x++;
	  node = node->child;

	  if (IS_WITHIN_VIEW_PORT (x, y)) {
	    WRITE_VIEW_PORT_NODE (x, y, node);
	    have_node_within_view_port = 1;
	  }
	}

	if (have_node_within_view_port
	    || (view_port_x0 <= line_pointer->x0
		&& line_pointer->x0 < view_port_x1
		&& line_pointer->y0 < view_port_y1
		&& y_level[line_pointer->x0] >= view_port_y0)) {
	  line_pointer->y1 = y_level[line_pointer->x0];
	  line_pointer->x3 = branch_leaf_x;

	  line_pointer++;
	}
      }
      else {
	y = ++y_level[x];

	if (y >= y_level[last_valid_y_level])
	  last_valid_y_level = x;
	else {
	  int x_scan;

	  for (x_scan = x + 1; y_level[x_scan] < y; x_scan++)
	    y_level[x_scan] = y;
	}

	if (IS_WITHIN_VIEW_PORT (x, y))
	  WRITE_VIEW_PORT_NODE (x, y, node);

	if (IS_WITHIN_VIEW_PORT (x, y)
	    || (view_port_x0 <= x - 1 && x - 1 < view_port_x1
		&& y_level[x - 1] < view_port_y1 && y - 1 >= view_port_y0)) {
	  line_pointer->x0 = x - 1;
	  line_pointer->y0 = y_level[x - 1];
	  line_pointer->y1 = y - 1;
	  line_pointer->x2 = x;
	  line_pointer->x3 = x;

	  line_pointer++;
	}

	y_level[x - 1] = y - 1;
      }
    }
    else {
      int have_node_within_view_port = 0;

      line_pointer->x0 = x - 1;

      if (x > last_valid_y_level) {
	if (x > y_level_array_size) {
	  y_level_array_size += ROUND_UP (x, Y_LEVEL_ARRAY_SIZE_GRAIN);
	  y_level = utils_realloc (y_level, y_level_array_size * sizeof (int));
	}

	y = y_level[last_valid_y_level];
	line_pointer->y0 = y;

	while (++last_valid_y_level < x)
	  y_level[last_valid_y_level] = y;

	y_level[x] = ++y;
      }
      else {
	line_pointer->y0 = y_level[x - 1];

	y_level[x - 1] = y_level[x];
	y = ++y_level[x];
      }

      while (1) {
	if (IS_WITHIN_VIEW_PORT (x, y)) {
	  WRITE_VIEW_PORT_NODE (x, y, node);
	  have_node_within_view_port = 1;
	}

	if (!node->child || node->is_collapsed)
	  break;

	x++;
	node = node->child;
      }

      if (have_node_within_view_port
	  || (view_port_x0 <= line_pointer->x0
	      && line_pointer->x0 < view_port_x1
	      && view_port_y0 <= y - 1 && line_pointer->y0 < view_port_y1)) {
	line_pointer->y1 = y - 1;
	line_pointer->x2 = line_pointer->x0 + 1;
	line_pointer->x3 = x;

	line_pointer++;
      }
    }
  } while (!node->has_intermediate_map_data);

 finished:

  utils_free (y_level);

  PRINT_RUN_TIME (do_update_internal_view_port);

  return line_pointer;
}


/* Get `node_to_find's coordinates in the `tree's map.  Uses the same
 * algorithm as update_internal_map_data(), but never lays out more
 * than one chunk of the tree and stops if bumps into
 * `node_to_find'.
 */
static int
get_node_coordinates (SgfGameTree *tree, const SgfNode *node_to_find,
		      int *node_x, int *node_y)
{
  const SgfGameTreeMapData *intermediate_data;
  const SgfNode *node;
  int x;
  int *y_level;
  int y_level_array_size;
  int last_valid_y_level;
  DECLARE_TIME_VARIABLES;

  update_internal_map_data (tree);

  STORE_STARTING_TIME;

  intermediate_data = find_intermediate_data_for_node (tree, node_to_find, 0);

  if (intermediate_data) {
    last_valid_y_level = intermediate_data->last_valid_y_level;
    y_level_array_size = ROUND_UP (last_valid_y_level + 1,
				   Y_LEVEL_ARRAY_SIZE_GRAIN);
    y_level	       = utils_malloc (y_level_array_size * sizeof (int));
    memcpy (y_level, intermediate_data->y_level,
	    (last_valid_y_level + 1) * sizeof (int));

    node	       = intermediate_data->node;
    x		       = intermediate_data->x;
  }
  else {
    y_level_array_size = Y_LEVEL_ARRAY_INITIAL_SIZE;
    y_level	       = utils_malloc (y_level_array_size * sizeof (int));
    y_level[0]	       = 0;
    last_valid_y_level = 0;

    node = tree->root;
    x    = 0;

    while (node->child && !node->is_collapsed) {
      if (node == node_to_find)
	goto found_the_node;

      node = node->child;
      x++;
    }
  }

  do {
    while (!node->next) {
      if (node == node_to_find)
	goto found_the_node;

      if (!node->parent)
	goto failed_to_find_node;

      x--;
      node = node->parent;
    }

    if (node == node_to_find)
      goto found_the_node;

    node = node->next;

    if (x < last_valid_y_level) {
      if (node->child && !node->is_collapsed) {
	SgfNode *branch_scan_node;
	int branch_scan_x;
	int branch_highest_y;
	int branch_scan_delta;
	int branch_y;

	branch_scan_node = node->child;
	branch_scan_x	 = x + 1;

	while (branch_scan_x < last_valid_y_level
	       && branch_scan_node->child
	       && !branch_scan_node->is_collapsed) {
	  branch_scan_node = branch_scan_node->child;
	  branch_scan_x++;
	}

	branch_highest_y = y_level[branch_scan_x] + 1;
	for (branch_y = y_level[x] + 1, branch_scan_delta = 1;
	     branch_scan_delta < branch_highest_y - branch_y;
	     branch_scan_delta++) {
	  branch_y = MAX (branch_y,
			  (y_level[x + branch_scan_delta] + 1
			   - branch_scan_delta));
	}

	y_level[x - 1] = branch_y - 1;
	y_level[x]     = branch_y;

	while (branch_y < branch_highest_y) {
	  if (node == node_to_find)
	    goto found_the_node;

	  y_level[++x] = ++branch_y;
	  node	       = node->child;
	}

	if (branch_y >= y_level[last_valid_y_level]) {
	  last_valid_y_level = x;

	  while (node->child && !node->is_collapsed) {
	    if (node == node_to_find)
	      goto found_the_node;

	    x++;
	    node = node->child;
	  }
	}
	else {
	  int x_scan;

	  while (node->child && !node->is_collapsed) {
	    if (node == node_to_find)
	      goto found_the_node;

	    y_level[++x] = branch_y;
	    node	 = node->child;
	  }

	  for (x_scan = x + 1; y_level[x_scan] < branch_y; x_scan++)
	    y_level[x_scan] = branch_y;
	}
      }
      else {
	y_level[x - 1] = y_level[x]++;

	if (node == node_to_find)
	  goto found_the_node;

	if (y_level[x] >= y_level[last_valid_y_level])
	  last_valid_y_level = x;
	else {
	  int y = y_level[x];
	  int x_scan;

	  for (x_scan = x + 1; y_level[x_scan] < y; x_scan++)
	    y_level[x_scan] = y;
	}
      }
    }
    else {
      if (x > last_valid_y_level) {
	int previous_branch_y;

	if (x > y_level_array_size) {
	  y_level_array_size = ROUND_UP (x, Y_LEVEL_ARRAY_SIZE_GRAIN);
	  y_level	     = utils_realloc (y_level,
					      (y_level_array_size
					       * sizeof (int)));
	}

	previous_branch_y = y_level[last_valid_y_level];
	while (++last_valid_y_level < x)
	  y_level[last_valid_y_level] = previous_branch_y;

	y_level[x] = previous_branch_y + 1;
      }
      else
	y_level[x - 1] = y_level[x]++;

      while (node->child && !node->is_collapsed) {
	if (node == node_to_find)
	  goto found_the_node;

	x++;
	node = node->child;
      }
    }
  } while (!node->has_intermediate_map_data);

 failed_to_find_node:
  /* Can happen if the requested node is within a collapsed subtree or
   * if the call is seriously f*cked up with a non-existant node, for
   * instance.
   */

  utils_free (y_level);

  PRINT_RUN_TIME (get_node_coordinates);

  return 0;

 found_the_node:

  *node_x = x;
  *node_y = y_level[MIN (x, last_valid_y_level)];

  utils_free (y_level);

  PRINT_RUN_TIME (get_node_coordinates);

  return 1;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
