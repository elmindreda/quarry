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


#ifndef QUARRY_GTK_SGF_TREE_VIEW_H
#define QUARRY_GTK_SGF_TREE_VIEW_H


#include "gtk-goban-base.h"
#include "gtk-tile-set.h"
#include "sgf.h"
#include "board.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_SGF_TREE_VIEW	(gtk_sgf_tree_view_get_type ())

#define GTK_SGF_TREE_VIEW(object)					\
  GTK_CHECK_CAST ((object), GTK_TYPE_SGF_TREE_VIEW, GtkSgfTreeView)

#define GTK_SGF_TREE_VIEW_CLASS(class)					\
  GTK_CHECK_CLASS_CAST ((class), GTK_TYPE_SGF_TREE_VIEW,		\
			GtkSgfTreeViewClass)

#define GTK_IS_SGF_TREE_VIEW(object)					\
  GTK_CHECK_TYPE ((object), GTK_TYPE_SGF_TREE_VIEW)

#define GTK_IS_SGF_TREE_VIEW_CLASS(class)				\
  GTK_CHECK_CLASS_TYPE ((class), GTK_TYPE_SGF_TREE_VIEW)

#define GTK_SGF_TREE_VIEW_GET_CLASS(object)				\
  GTK_CHECK_GET_CLASS ((object), GTK_TYPE_SGF_TREE_VIEW,		\
		       GtkSgfTreeViewClass)


typedef struct _GtkSgfTreeView		GtkSgfTreeView;
typedef struct _GtkSgfTreeViewClass	GtkSgfTreeViewClass;

struct _GtkSgfTreeView {
  GtkGobanBase		base;

  GdkWindow	       *output_window;

  GtkAdjustment	       *hadjustment;
  GtkAdjustment	       *vadjustment;
  gboolean		ignore_adjustment_changes;

  SgfGameTree	       *current_tree;
  gboolean		do_track_current_node;
  gboolean		expect_map_modification;

  gint			map_width;
  gint			map_height;

  gint			view_port_x0;
  gint			view_port_y0;
  gint			view_port_x1;
  gint			view_port_y1;

  SgfNode	      **view_port_nodes;
  SgfGameTreeMapLine   *view_port_lines;
  gint			num_view_port_lines;

  char		       *tile_map;
  char		       *sgf_markup_tile_map;

  unsigned int		button_pressed;
  gint			press_x;
  gint			press_y;
};

struct _GtkSgfTreeViewClass {
  GtkGobanBaseClass   parent_class;

  void (* set_scroll_adjustments) (GtkSgfTreeView *view,
				   GtkAdjustment *hadjustment,
				   GtkAdjustment *vadjustment);

  void (* sgf_tree_view_clicked) (GtkSgfTreeView *view, SgfNode *sgf_node,
				  gint button_index);
};


GType		gtk_sgf_tree_view_get_type (void);

GtkWidget *	gtk_sgf_tree_view_new (void);

void		gtk_sgf_tree_view_set_sgf_tree (GtkSgfTreeView *view,
						SgfGameTree *sgf_tree);

void		gtk_sgf_tree_view_update_view_port (GtkSgfTreeView *view);

void		gtk_sgf_tree_view_center_on_current_node
		  (GtkSgfTreeView *view);


#endif /* QUARRY_GTK_SGF_TREE_VIEW_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
