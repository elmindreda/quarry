/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004 Paul Pogonyshev.                             *
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
 * as in Cgoban, but the speed is much higher :-)
 *
 * Currently all the work is done by update_internal_view_port().
 * Each time sgf_game_tree_update_view_port() is asked for a new view
 * port it cannot fetch from the internal one, it recalculates the
 * complete layout.  This can still be optimized further (for speed),
 * but since it is very fast anyway, optimization is left out for now.
 *
 *
 * Really Fast Tree Map algorithm outline (not yet implemented.)
 *
 * - Use the very same algorithm as in update_internal_view_port().
 *
 * - Create another function, say sgf_game_tree_update_map().  It
 *   should be called whenever the tree structure is changed (as
 *   opposed to both structure and view port change.)  It should not
 *   bother with view ports and lines and hence be even faster.
 *
 * - In this function store y_level[] and a few other variables say
 *   each 20000 nodes.  Make a list of this ``check points'' and mark
 *   the nodes they are attached to.  Also compute map's width and
 *   height.
 *
 * - Now update_internal_view_port() should traverse only those parts
 *   of the tree that intersect with the view port.  The parts are
 *   found by examining stored ``check points.''  Speed of view port
 *   updates will no longer depend on tree size (maybe excluding
 *   pathological cases.)
 */


#include "sgf.h"
#include "utils.h"

#include <assert.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


/* Define to non-zero to make update_internal_view_port() print
 * consumed wall time to stderr.
 */
#define PRINT_RUN_TIMES			0


#if PRINT_RUN_TIMES

#include <stdio.h>
#include <sys/time.h>

#endif


static void	update_internal_view_port (SgfGameTree *tree,
					   int view_port_x0, int view_port_y0,
					   int view_port_x1, int view_port_y1);


void
sgf_game_tree_activate_map (SgfGameTree *tree)
{
  assert (tree);

  if (tree->map_width == 0) {
    /* Generate a reasonably-sized internal view port.  Actually this
     * is needed only to compute map width and height.
     */
    update_internal_view_port (tree, 0, 0, 32, 32);
  }
}


void
sgf_game_tree_deactivate_map (SgfGameTree *tree)
{
  assert (tree);

  sgf_game_tree_invalidate_map (tree);
  tree->map_width = 0;
}


void
sgf_game_tree_invalidate_map (SgfGameTree *tree)
{
  assert (tree);

  if (tree->map_width > 0) {
    utils_free (tree->view_port_nodes);
    utils_free (tree->view_port_lines);

    tree->view_port_nodes = NULL;
    tree->view_port_lines = NULL;
  }
}


void
sgf_game_tree_update_view_port (SgfGameTree *tree,
				int view_port_x0, int view_port_y0,
				int view_port_x1, int view_port_y1,
				SgfNode ***view_port_nodes,
				SgfGameTreeMapLine **view_port_lines,
				int *num_view_port_lines)
{
  int view_port_width  = view_port_x1 - view_port_x0;
  int view_port_height = view_port_y1 - view_port_y0;

  assert (tree);
  assert (tree->map_width > 0);
  assert (0 <= view_port_x0 && view_port_x0 < view_port_x1);
  assert (0 <= view_port_y0 && view_port_y0 < view_port_y1);
  assert (view_port_nodes);
  assert (view_port_lines);
  assert (num_view_port_lines);

  if (!tree->view_port_nodes
      || tree->view_port_x0 >= view_port_x0
      || tree->view_port_y0 >= view_port_y0
      || tree->view_port_x1 <= view_port_x1
      || tree->view_port_y1 <= view_port_y1) {
    update_internal_view_port (tree,
			       view_port_x0, view_port_y0,
			       view_port_x1, view_port_y1);
  }

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
    original_pointer = (tree->view_port_nodes
			+ ((view_port_y0 - tree->view_port_y0)
			   * (tree->view_port_x1 - tree->view_port_x0))
			+ (view_port_x0 - tree->view_port_x0));

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




#define Y_LEVEL_ARRAY_INITIAL_SIZE	0x400

/* Must be a power of two. */
#define Y_LEVEL_ARRAY_SIZE_GRAIN	0x400


#define IS_WITHIN_VIEW_PORT(x, y)					\
  (view_port_x0 <= (x) && (x) < view_port_x1				\
   && view_port_y0 <= (y) && (y) < view_port_y1)

#define WRITE_VIEW_PORT_NODE(x, y, node)				\
  do {									\
    * (tree->view_port_nodes +						\
       ((y) - view_port_y0) * view_port_width + ((x) - view_port_x0))	\
      = (node);								\
  } while (0)

/* FIXME: Explain the laying out algorithm. */
static void
update_internal_view_port (SgfGameTree *tree,
			   int view_port_x0, int view_port_y0,
			   int view_port_x1, int view_port_y1)
{
  int view_port_width  = view_port_x1 - view_port_x0;
  int view_port_height = view_port_y1 - view_port_y0;
  SgfGameTreeMapLine *line_pointer;
  SgfNode *node;
  int x;
  int y;
  int *y_level;
  int y_level_array_size;
  int last_valid_y_level;

#if PRINT_RUN_TIMES

  struct timeval start_time;
  struct timeval finish_time;

  gettimeofday (&start_time, NULL);

#endif

  utils_free (tree->view_port_nodes);
  utils_free (tree->view_port_lines);

  tree->view_port_x0 = view_port_x0;
  tree->view_port_y0 = view_port_y0;
  tree->view_port_x1 = view_port_x1;
  tree->view_port_y1 = view_port_y1;

  tree->view_port_nodes = utils_malloc0 (view_port_width * view_port_height
					 * sizeof (SgfNode *));

  /* FIXME: Too large (but not too important.) */
  tree->view_port_lines = utils_malloc (3 * view_port_width * view_port_height
					* sizeof (SgfGameTreeMapLine));
  line_pointer		= tree->view_port_lines;

  y_level	     = utils_malloc (Y_LEVEL_ARRAY_INITIAL_SIZE
				     * sizeof (int));
  y_level[0]	     = 0;
  y_level_array_size = Y_LEVEL_ARRAY_INITIAL_SIZE;
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

  tree->map_width  = x + 1;
  tree->map_height = 1;

  while (1) {
    do {
      if (node->next) {
	node = node->next;
	break;
      }

      x--;
      node = node->parent;
    } while (node);

    if (!node)
      break;

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
	  while (x <= branch_leaf_x);

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

    if (x >= tree->map_width)
      tree->map_width = x + 1;

    if (y >= tree->map_height)
      tree->map_height = y + 1;
  }

  utils_free (y_level);

  tree->num_view_port_lines = (line_pointer - tree->view_port_lines);
  tree->view_port_lines
    = utils_realloc (tree->view_port_lines,
		     (char *) line_pointer - (char *) tree->view_port_lines);

#if PRINT_RUN_TIMES

  gettimeofday (&finish_time, NULL);

  fprintf (stderr, "Time spent in sgf_game_tree_update_view_port(): %f\n",
	   ((finish_time.tv_usec - start_time.tv_usec) * 0.000001
	    + (finish_time.tv_sec - start_time.tv_sec)));

#endif
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
