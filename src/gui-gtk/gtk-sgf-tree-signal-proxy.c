/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005, 2006 Paul Pogonyshev.                       *
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


#include "gtk-sgf-tree-signal-proxy.h"

#include "quarry-marshal.h"
#include "sgf.h"

#include <assert.h>
#include <gtk/gtk.h>


static void	gtk_sgf_tree_signal_proxy_class_init
		  (GtkSgfTreeSignalProxyClass *class);
static void	gtk_sgf_tree_signal_proxy_finalize (GObject *object);

static void	receive_notification
		  (SgfGameTree *sgf_tree,
		   SgfGameTreeNotificationCode notification_code,
		   GtkSgfTreeSignalProxy *proxy);


enum {
  ABOUT_TO_MODIFY_MAP,
  ABOUT_TO_MODIFY_TREE,
  ABOUT_TO_CHANGE_CURRENT_NODE,

  CURRENT_NODE_CHANGED,
  TREE_MODIFIED,
  MAP_MODIFIED,

  NUM_SIGNALS
};

static guint	sgf_tree_signal_proxy_signals[NUM_SIGNALS];


GType
gtk_sgf_tree_signal_proxy_get_type (void)
{
  static GType sgf_tree_signal_proxy_type = 0;

  if (!sgf_tree_signal_proxy_type) {
    static GTypeInfo sgf_tree_signal_proxy_info = {
      sizeof (GtkSgfTreeSignalProxyClass),
      NULL,
      NULL,
      (GClassInitFunc) gtk_sgf_tree_signal_proxy_class_init,
      NULL,
      NULL,
      sizeof (GtkSgfTreeSignalProxy),
      1,
      NULL,
      NULL
    };

    sgf_tree_signal_proxy_type
      = g_type_register_static (G_TYPE_OBJECT, "GtkSgfTreeSignalProxy",
				&sgf_tree_signal_proxy_info, 0);
  }

  return sgf_tree_signal_proxy_type;
}


static void
gtk_sgf_tree_signal_proxy_class_init (GtkSgfTreeSignalProxyClass *class)
{
  G_OBJECT_CLASS (class)->finalize    = gtk_sgf_tree_signal_proxy_finalize;

  class->about_to_modify_map	      = NULL;
  class->about_to_modify_tree	      = NULL;
  class->about_to_change_current_node = NULL;

  class->current_node_changed	      = NULL;
  class->tree_modified		      = NULL;
  class->map_modified		      = NULL;

  sgf_tree_signal_proxy_signals[ABOUT_TO_MODIFY_MAP]
    = g_signal_new ("about-to-modify-map",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkSgfTreeSignalProxyClass,
				     about_to_modify_map),
		    NULL, NULL,
		    quarry_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1, G_TYPE_POINTER);

  sgf_tree_signal_proxy_signals[ABOUT_TO_MODIFY_TREE]
    = g_signal_new ("about-to-modify-tree",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkSgfTreeSignalProxyClass,
				     about_to_modify_tree),
		    NULL, NULL,
		    quarry_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1, G_TYPE_POINTER);

  sgf_tree_signal_proxy_signals[ABOUT_TO_CHANGE_CURRENT_NODE]
    = g_signal_new ("about-to-change-current-node",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkSgfTreeSignalProxyClass,
				     about_to_change_current_node),
		    NULL, NULL,
		    quarry_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1, G_TYPE_POINTER);

  sgf_tree_signal_proxy_signals[CURRENT_NODE_CHANGED]
    = g_signal_new ("current-node-changed",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkSgfTreeSignalProxyClass,
				     current_node_changed),
		    NULL, NULL,
		    quarry_marshal_VOID__POINTER_POINTER,
		    G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

  sgf_tree_signal_proxy_signals[TREE_MODIFIED]
    = g_signal_new ("tree-modified",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkSgfTreeSignalProxyClass,
				     tree_modified),
		    NULL, NULL,
		    quarry_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1, G_TYPE_POINTER);

  sgf_tree_signal_proxy_signals[MAP_MODIFIED]
    = g_signal_new ("map-modified",
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkSgfTreeSignalProxyClass,
				     map_modified),
		    NULL, NULL,
		    quarry_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1, G_TYPE_POINTER);
}


static void
gtk_sgf_tree_signal_proxy_finalize (GObject *object)
{
  GtkSgfTreeSignalProxy *proxy = GTK_SGF_TREE_SIGNAL_PROXY (object);

  if (proxy->state_stack) {
    g_warning ("warning: finalizing SGF tree signal proxy"
	       " with non-empty state stack");

    g_slist_foreach (proxy->state_stack, (GFunc) g_free, NULL);
    g_slist_free (proxy->state_stack);
  }
}


GObject *
gtk_sgf_tree_signal_proxy_attach (SgfGameTree *sgf_tree)
{
  assert (sgf_tree);

  if (sgf_tree->user_data) {
    assert (GTK_IS_SGF_TREE_SIGNAL_PROXY (sgf_tree->user_data));

    return G_OBJECT (sgf_tree->user_data);
  }
  else {
    GObject *proxy = g_object_new (GTK_TYPE_SGF_TREE_SIGNAL_PROXY, NULL);

    GTK_SGF_TREE_SIGNAL_PROXY (proxy)->sgf_tree    = sgf_tree;
    GTK_SGF_TREE_SIGNAL_PROXY (proxy)->state_stack = NULL;

    sgf_game_tree_set_notification_callback (sgf_tree,
					     ((SgfGameTreeNotificationCallback)
					      receive_notification),
					     proxy);

    return proxy;
  }
}


void
gtk_sgf_tree_signal_proxy_push_tree_state (SgfGameTree *sgf_tree,
					   const SgfGameTreeState *new_state)
{
  GtkSgfTreeSignalProxy *proxy;
  SgfGameTreeState *old_state;

  assert (sgf_tree);
  assert (GTK_IS_SGF_TREE_SIGNAL_PROXY (sgf_tree->user_data));

  proxy = GTK_SGF_TREE_SIGNAL_PROXY (sgf_tree->user_data);

  old_state = g_malloc (sizeof (SgfGameTreeState));
  sgf_game_tree_get_state (sgf_tree, old_state);
  sgf_game_tree_set_state (sgf_tree, new_state);

  proxy->state_stack = g_slist_prepend (proxy->state_stack, old_state);
}


void
gtk_sgf_tree_signal_proxy_pop_tree_state (SgfGameTree *sgf_tree,
					  SgfGameTreeState *old_state)
{
  GtkSgfTreeSignalProxy *proxy;
  GSList *popped_link;

  assert (sgf_tree);
  assert (GTK_IS_SGF_TREE_SIGNAL_PROXY (sgf_tree->user_data));

  proxy = GTK_SGF_TREE_SIGNAL_PROXY (sgf_tree->user_data);

  assert (proxy->state_stack);

  if (old_state)
    sgf_game_tree_get_state (sgf_tree, old_state);

  popped_link	     = proxy->state_stack;
  proxy->state_stack = popped_link->next;

  sgf_game_tree_set_state (sgf_tree, (SgfGameTreeState *) popped_link->data);

  g_free (popped_link->data);
  g_slist_free_1 (popped_link);
}


static void
receive_notification (SgfGameTree *sgf_tree,
		      SgfGameTreeNotificationCode notification_code,
		      GtkSgfTreeSignalProxy *proxy)
{
  SgfGameTreeState current_tree_state;

  assert (proxy->sgf_tree == sgf_tree);

  if (proxy->state_stack) {
    if (notification_code == SGF_ABOUT_TO_CHANGE_CURRENT_NODE
	|| notification_code == SGF_CURRENT_NODE_CHANGED) {
      /* These are suppressed when working with ``secondary'' game
       * tree states.
       */
      return;
    }

    /* Signal handlers are kept unaware of board state changes. */
    sgf_game_tree_get_state (sgf_tree, &current_tree_state);
    sgf_game_tree_set_state (sgf_tree,
			     ((SgfGameTreeState *)
			      g_slist_last (proxy->state_stack)->data));
  }

  switch (notification_code) {
  case SGF_ABOUT_TO_MODIFY_MAP:
    g_signal_emit (proxy,
		   sgf_tree_signal_proxy_signals[ABOUT_TO_MODIFY_MAP], 0,
		   sgf_tree);
    break;

  case SGF_ABOUT_TO_MODIFY_TREE:
    g_signal_emit (proxy,
		   sgf_tree_signal_proxy_signals[ABOUT_TO_MODIFY_TREE], 0,
		   sgf_tree);
    break;

  case SGF_ABOUT_TO_CHANGE_CURRENT_NODE:
    g_signal_emit (proxy,
		   sgf_tree_signal_proxy_signals[ABOUT_TO_CHANGE_CURRENT_NODE],
		   0,
		   sgf_tree);
    return;

  case SGF_CURRENT_NODE_CHANGED:
    g_signal_emit (proxy,
		   sgf_tree_signal_proxy_signals[CURRENT_NODE_CHANGED], 0,
		   sgf_tree, proxy->old_current_node);
    return;

  case SGF_TREE_MODIFIED:
    g_signal_emit (proxy, sgf_tree_signal_proxy_signals[TREE_MODIFIED], 0,
		   sgf_tree);
    break;

  case SGF_MAP_MODIFIED:
    g_signal_emit (proxy, sgf_tree_signal_proxy_signals[MAP_MODIFIED], 0,
		   sgf_tree);
    break;

  case SGF_GAME_TREE_DELETED:
    g_object_unref (proxy);
    break;

  default:
    assert (0);
  }

  if (proxy->state_stack) {
    /* Restore current game tree state. */
    sgf_game_tree_set_state (sgf_tree, &current_tree_state);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
