/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005 Paul Pogonyshev                              *
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


#ifndef QUARRY_GTK_SGF_TREE_SIGNAL_PROXY_H
#define QUARRY_GTK_SGF_TREE_SIGNAL_PROXY_H


#include "sgf.h"

#include <gtk/gtk.h>


#define GTK_TYPE_SGF_TREE_SIGNAL_PROXY					\
  gtk_sgf_tree_signal_proxy_get_type ()

#define GTK_SGF_TREE_SIGNAL_PROXY(object)				\
  GTK_CHECK_CAST ((object), GTK_TYPE_SGF_TREE_SIGNAL_PROXY,		\
		  GtkSgfTreeSignalProxy)

#define GTK_SGF_TREE_SIGNAL_PROXY_CLASS(class)				\
  GTK_CHECK_CLASS_CAST ((class), GTK_TYPE_SGF_TREE_SIGNAL_PROXY,	\
			GtkSgfTreeSignalProxyClass)

#define GTK_IS_SGF_TREE_SIGNAL_PROXY(object)				\
  GTK_CHECK_TYPE ((object), GTK_TYPE_SGF_TREE_SIGNAL_PROXY)

#define GTK_IS_SGF_TREE_SIGNAL_PROXY_CLASS(class)			\
  GTK_CHECK_CLASS_TYPE ((class), GTK_TYPE_SGF_TREE_SIGNAL_PROXY)

#define GTK_SGF_TREE_SIGNAL_PROXY_GET_CLASS(object)			\
  (GTK_CHECK_GET_CLASS ((object), GTK_TYPE_SGF_TREE_SIGNAL_PROXY,	\
			GtkSgfTreeSignalProxyClass)


typedef struct _GtkSgfTreeSignalProxy		GtkSgfTreeSignalProxy;
typedef struct _GtkSgfTreeSignalProxyClass	GtkSgfTreeSignalProxyClass;

struct _GtkSgfTreeSignalProxy {
  GObject	 object;

  SgfGameTree	*sgf_tree;
  SgfNode	*old_current_node;

  GSList	*state_stack;
};

struct _GtkSgfTreeSignalProxyClass {
  GObjectClass	 parent_class;

  void (* about_to_modify_map)		(GtkSgfTreeSignalProxy *proxy,
					 SgfGameTree *sgf_tree);
  void (* about_to_modify_tree)		(GtkSgfTreeSignalProxy *proxy,
					 SgfGameTree *sgf_tree);
  void (* about_to_change_current_node) (GtkSgfTreeSignalProxy *proxy,
					 SgfGameTree *sgf_tree);

  void (* current_node_changed)		(GtkSgfTreeSignalProxy *proxy,
					 SgfGameTree *sgf_tree,
					 SgfNode *old_current_node);
  void (* tree_modified)		(GtkSgfTreeSignalProxy *proxy,
					 SgfGameTree *sgf_tree);
  void (* map_modified)			(GtkSgfTreeSignalProxy *proxy,
					 SgfGameTree *sgf_tree);
};


GType	 	gtk_sgf_tree_signal_proxy_get_type (void);

GObject *	gtk_sgf_tree_signal_proxy_attach (SgfGameTree *sgf_tree);

void		gtk_sgf_tree_signal_proxy_push_tree_state
		  (SgfGameTree *sgf_tree, const SgfGameTreeState *new_state);
void		gtk_sgf_tree_signal_proxy_pop_tree_state
		  (SgfGameTree *sgf_tree, SgfGameTreeState *old_state);


#define GET_SIGNAL_PROXY(sgf_tree)					\
  ((GObject *) GTK_SGF_TREE_SIGNAL_PROXY ((sgf_tree)->user_data))


#endif /* QUARRY_GTK_SGF_TREE_SIGNAL_PROXY_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
