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

/* Implementation of loggable and revertable changes to SGF trees,
 * which are stored in the form of undo histories.  Many non-static
 * function here are private to the SGF module.  All external entry
 * points are contained in the first page, but the reverse is not
 * true.
 *
 * This file is very closely related to `sgf-undo.c' and was actually
 * a part of it earlier.  There are mutual interactions between the
 * two.  Also, since undo histories are optional, most functions here
 * take an `SgfGameTree' as its first argument and such functions have
 * `sgf_utils' as name prefix.
 */


#include "sgf.h"
#include "sgf-privates.h"
#include "sgf-undo.h"
#include "board.h"
#include "utils.h"

#include <assert.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


inline static void  delete_undo_history_entry (SgfUndoHistoryEntry *entry,
					       int is_applied,
					       SgfGameTree *tree);

static void	    begin_undoing_or_redoing (SgfGameTree *tree);
static void	    set_is_modifying_map (SgfGameTree *tree);
static void	    set_is_modifying_tree (SgfGameTree *tree);
static void	    end_undoing_or_redoing (SgfGameTree *tree);


inline static SgfNode **
		    find_node_link (SgfNode *parent, const SgfNode *next);
inline static SgfProperty **
		    find_property_link (SgfNode *node,
					const SgfProperty *next);


SgfUndoHistory *
sgf_undo_history_new (SgfGameTree *tree)
{
  SgfUndoHistory *history = utils_malloc (sizeof (SgfUndoHistory));

  assert (tree);

  history->first_entry		  = NULL;
  history->last_entry		  = NULL;
  history->last_applied_entry	  = NULL;
  history->unmodified_state_entry = NULL;

  history->notification_callback  = NULL;
  history->user_data		  = NULL;

  history->next		  = tree->undo_history_list;
  tree->undo_history_list = history;

  return history;
}


void
sgf_undo_history_delete (SgfUndoHistory *history, SgfGameTree *tree)
{
  SgfUndoHistory **link;
  SgfUndoHistoryEntry *this_entry;
  int is_applied;

  assert (history);
  assert (tree);

  /* Detach the `history' from the list of histories associated with
   * `tree'.
   */
  link = &tree->undo_history_list;
  while (*link != history) {
    assert (*link);
    link = & (*link)->next;
  }

  *link = history->next;

  for (this_entry = history->first_entry, is_applied = 1; this_entry;) {
    SgfUndoHistoryEntry *next_entry = this_entry->next;

    if (this_entry->previous == history->last_applied_entry)
      is_applied = 0;

    delete_undo_history_entry (this_entry, is_applied, tree);
    this_entry = next_entry;
  }

  utils_free (history);
}


int
sgf_undo_history_is_last_applied_entry_single (const SgfUndoHistory *history)
{
  assert (history);

  if (history->last_applied_entry) {
    return (history->last_applied_entry->is_last_in_action
	    && (!history->last_applied_entry->previous
		|| history->last_applied_entry->previous->is_last_in_action));
  }
  else
    return 0;
}


int
sgf_undo_history_check_last_applied_custom_entry_type
  (const SgfUndoHistory *history,
   const SgfCustomUndoHistoryEntryData *entry_data)
{
  const SgfCustomOperationEntry *custom_entry;

  assert (history);
  assert (entry_data);

  custom_entry = (const SgfCustomOperationEntry *) history->last_applied_entry;

  return (custom_entry
	  && custom_entry->entry.operation_index == SGF_OPERATION_CUSTOM
	  && custom_entry->entry_data == entry_data);
}


void *
sgf_undo_history_get_last_applied_custom_entry_data
  (const SgfUndoHistory *history)
{
  assert (history);
  assert (history->last_applied_entry);
  assert (history->last_applied_entry->operation_index
	  == SGF_OPERATION_CUSTOM);

  return (((const SgfCustomOperationEntry *) history->last_applied_entry)
	  ->user_data);
}


void
sgf_undo_history_set_notification_callback
  (SgfUndoHistory *history,
   SgfUndoHistoryNotificationCallback notification_callback, void *user_data)
{
  assert (history);

  history->notification_callback = notification_callback;
  history->user_data		 = user_data;
}


/* Designate a beginning of an undoable action.  Must always be paired
 * by a call to sgf_utils_end_action().  Such actions can and do nest
 * and only the topmost-level invocation of sgf_utils_begin_action()
 * and sgf_utils_end_action() constitute a user-visible action.
 *
 * This allows to easily construct composite actions.  By convention,
 * all functions in this file that alter undo history must set up an
 * action, so that the top-level code can avoid this if it only needs
 * a ``single-primitive action.''
 */
void
sgf_utils_begin_action (SgfGameTree *tree)
{
  assert (tree);

  if (tree->undo_operation_level++ == 0)
    begin_undoing_or_redoing (tree);
}


/* Finish an undoable action.  See sgf_utils_begin_action() for more
 * information.
 */
void
sgf_utils_end_action (SgfGameTree *tree)
{
  assert (tree);

  tree->undo_operation_level--;
  assert (tree->undo_operation_level >= 0);

  if (tree->undo_operation_level == 0) {
    if (tree->undo_history && tree->undo_history->last_entry)
      tree->undo_history->last_entry->is_last_in_action = 1;

    end_undoing_or_redoing (tree);
  }
}


void
sgf_utils_undo (SgfGameTree *tree)
{
  SgfUndoHistoryEntry *entry;

  assert (tree);
  assert (tree->board_state);
  assert (tree->undo_operation_level == 0);

  if (!sgf_utils_can_undo (tree))
    return;

  begin_undoing_or_redoing (tree);

  entry = tree->undo_history->last_applied_entry;

  do {
    sgf_undo_operations[entry->operation_index].undo (entry, tree);
    entry = entry->previous;
  } while (entry && !entry->is_last_in_action);

  tree->undo_history->last_applied_entry = entry;
  end_undoing_or_redoing (tree);
}


void
sgf_utils_redo (SgfGameTree *tree)
{
  SgfUndoHistory *history;
  SgfUndoHistoryEntry *entry;

  assert (tree);
  assert (tree->board_state);
  assert (tree->undo_operation_level == 0);

  if (!sgf_utils_can_redo (tree))
    return;

  history = tree->undo_history;
  entry	  = (history->last_applied_entry
	     ? history->last_applied_entry->next : history->first_entry);

  begin_undoing_or_redoing (tree);

  while (1) {
    sgf_undo_operations[entry->operation_index].redo (entry, tree);
    if (entry->is_last_in_action)
      break;

    entry = entry->next;
    assert (entry);
  }

  history->last_applied_entry = entry;
  end_undoing_or_redoing (tree);
}



/* All remaining functions are private to the SGF module. */

SgfUndoHistoryEntry *
sgf_new_node_undo_history_entry_new (SgfNode *new_node)
{
  SgfNodeOperationEntry *operation_data
    = utils_malloc (sizeof (SgfNodeOperationEntry));

  operation_data->entry.operation_index	   = SGF_OPERATION_NEW_NODE;
  operation_data->node			   = new_node;
  operation_data->parent_current_variation
    = new_node->parent->current_variation;

  return (SgfUndoHistoryEntry *) operation_data;
}


SgfUndoHistoryEntry *
sgf_delete_node_undo_history_entry_new (SgfNode *node)
{
  SgfNodeOperationEntry *operation_data
    = utils_malloc (sizeof (SgfNodeOperationEntry));

  operation_data->entry.operation_index = SGF_OPERATION_DELETE_NODE;
  operation_data->node			= node;

  /* This field is used after node deletion.  So, we should set it so
   * it is _not_ equal to the `node', which is being deleted.  Use a
   * neighbor node instead.
   */
  operation_data->parent_current_variation
    = (node->next ? node->next : sgf_node_get_previous_node (node));

  return (SgfUndoHistoryEntry *) operation_data;
}


SgfUndoHistoryEntry *
sgf_delete_node_children_undo_history_entry_new (SgfNode *node)
{
  SgfNodeOperationEntry *operation_data
    = utils_malloc (sizeof (SgfNodeOperationEntry));

  operation_data->entry.operation_index
    = SGF_OPERATION_DELETE_NODE_CHILDREN;
  operation_data->node			   = node->child;
  operation_data->parent_current_variation = node->current_variation;

  return (SgfUndoHistoryEntry *) operation_data;
}


SgfUndoHistoryEntry *
sgf_swap_nodes_undo_history_entry_new (SgfNode *node1, SgfNode *node2)
{
  SgfTwoNodesOperationEntry *operation_data
    = utils_malloc (sizeof (SgfTwoNodesOperationEntry));

  operation_data->entry.operation_index	   = SGF_OPERATION_SWAP_NODES;
  operation_data->node1			   = node1;
  operation_data->node2			   = node2;
  operation_data->parent_current_variation = node1->parent->current_variation;

  return (SgfUndoHistoryEntry *) operation_data;
}


SgfUndoHistoryEntry *
sgf_change_node_inlined_color_undo_history_entry_new
  (SgfNode *node, SgfUndoOperation operation, int new_color)
{
  SgfChangeNodeInlinedColorOperationEntry *operation_data
    = utils_malloc (sizeof (SgfChangeNodeInlinedColorOperationEntry));

  operation_data->entry.operation_index = operation;
  operation_data->node			= node;
  operation_data->color			= new_color;

  return (SgfUndoHistoryEntry *) operation_data;
}


SgfUndoHistoryEntry *
sgf_new_property_undo_history_entry_new (SgfGameTree *tree, SgfNode *node,
					 SgfProperty **link, SgfType type)
{
  SgfPropertyOperationEntry *operation_data
    = utils_malloc (sizeof (SgfPropertyOperationEntry));

  operation_data->entry.operation_index = SGF_OPERATION_NEW_PROPERTY;
  operation_data->node			= node;
  operation_data->property		= sgf_property_new (tree, type, *link);

  return (SgfUndoHistoryEntry *) operation_data;
}


SgfUndoHistoryEntry *
sgf_delete_property_undo_history_entry_new (SgfNode *node,
					    SgfProperty *property)
{
  SgfPropertyOperationEntry *operation_data
    = utils_malloc (sizeof (SgfPropertyOperationEntry));

  operation_data->entry.operation_index = SGF_OPERATION_DELETE_PROPERTY;
  operation_data->node			= node;
  operation_data->property		= property;

  return (SgfUndoHistoryEntry *) operation_data;
}


SgfUndoHistoryEntry *
sgf_change_property_undo_history_entry_new (SgfNode *node,
					    SgfProperty *property)
{
  SgfChangePropertyOperationEntry *operation_data
    = utils_malloc (sizeof (SgfChangePropertyOperationEntry));

  operation_data->entry.operation_index = SGF_OPERATION_CHANGE_PROPERTY;
  operation_data->node			= node;
  operation_data->property		= property;

  return (SgfUndoHistoryEntry *) operation_data;
}


#if SGF_REAL_VALUES_ALLOCATED_SEPARATELY

SgfUndoHistoryEntry *
sgf_change_real_property_undo_history_entry_new (SgfNode *node,
						 SgfProperty *property,
						 double new_value)
{
  SgfChangeRealPropertyOperationEntry *operation_data
    = utils_malloc (sizeof (SgfChangeRealPropertyOperationEntry));

  operation_data->entry.operation_index = SGF_OPERATION_CHANGE_REAL_PROPERTY;
  operation_data->node			= node;
  operation_data->property		= property;
  operation_data->value			= new_value;

  return (SgfUndoHistoryEntry *) operation_data;
}

#endif /* SGF_REAL_VALUES_ALLOCATED_SEPARATELY */


SgfUndoHistoryEntry *
sgf_custom_undo_history_entry_new
  (const SgfCustomUndoHistoryEntryData *entry_data, void *user_data,
   SgfNode *node_to_switch_to)
{
  SgfCustomOperationEntry *operation_data
    = utils_malloc (sizeof (SgfCustomOperationEntry));

  operation_data->entry.operation_index = SGF_OPERATION_CUSTOM;
  operation_data->entry_data		= entry_data;
  operation_data->user_data		= user_data;
  operation_data->node_to_switch_to	= node_to_switch_to;

  return (SgfUndoHistoryEntry *) operation_data;
}


inline static void
delete_undo_history_entry (SgfUndoHistoryEntry *entry, int is_applied,
			   SgfGameTree *tree)
{
  if (sgf_undo_operations[entry->operation_index].free_data) {
    sgf_undo_operations[entry->operation_index].free_data (entry, is_applied,
							   tree);
  }

  utils_free (entry);
}




static void
begin_undoing_or_redoing (SgfGameTree *tree)
{
  tree->node_to_switch_to = NULL;
  tree->is_modifying_map  = 0;
  tree->is_modifying_tree = 0;

  if (tree->undo_history) {
    tree->tree_was_modified = (tree->undo_history->last_applied_entry
			       != tree->undo_history->unmodified_state_entry);
    tree->could_undo	    = sgf_utils_can_undo (tree);
    tree->could_redo	    = sgf_utils_can_redo (tree);
  }

  tree->collection_was_modified
    = sgf_collection_is_modified (tree->collection);
}


static void
set_is_modifying_map (SgfGameTree *tree)
{
  if (!tree->is_modifying_map) {
    GAME_TREE_DO_NOTIFY (tree, SGF_ABOUT_TO_MODIFY_MAP);
    tree->is_modifying_map = 1;
  }
}


static void
set_is_modifying_tree (SgfGameTree *tree)
{
  if (!tree->is_modifying_tree) {
    GAME_TREE_DO_NOTIFY (tree, SGF_ABOUT_TO_MODIFY_TREE);
    tree->is_modifying_tree = 1;
  }
}


static void
end_undoing_or_redoing (SgfGameTree *tree)
{
  SgfNode *node_to_switch_to = tree->node_to_switch_to;

  if (node_to_switch_to) {
    GAME_TREE_DO_NOTIFY (tree, SGF_ABOUT_TO_CHANGE_CURRENT_NODE);

    if (tree->current_node->parent == node_to_switch_to) {
      /* A common case, optimize it. */
      sgf_utils_ascend_nodes (tree, 1);
    }
    else
      sgf_utils_do_switch_to_given_node (tree, node_to_switch_to);

    GAME_TREE_DO_NOTIFY (tree, SGF_CURRENT_NODE_CHANGED);
  }

  if (tree->is_modifying_tree)
    GAME_TREE_DO_NOTIFY (tree, SGF_TREE_MODIFIED);
  if (tree->is_modifying_map)
    GAME_TREE_DO_NOTIFY (tree, SGF_MAP_MODIFIED);

  if (tree->undo_history) {
    if (tree->tree_was_modified
	&& (tree->undo_history->last_applied_entry
	    == tree->undo_history->unmodified_state_entry))
      tree->collection->num_modified_undo_histories--;
    else if (!tree->tree_was_modified
	     && (tree->undo_history->last_applied_entry
		 != tree->undo_history->unmodified_state_entry))
      tree->collection->num_modified_undo_histories++;

    if (tree->could_undo != sgf_utils_can_undo (tree)
	|| tree->could_redo != sgf_utils_can_redo (tree))
      UNDO_HISTORY_DO_NOTIFY (tree->undo_history);
  }

  if (sgf_collection_is_modified (tree->collection)
      != tree->collection_was_modified)
    COLLECTION_DO_NOTIFY (tree->collection);
}


void
sgf_utils_apply_undo_history_entry (SgfGameTree *tree,
				    SgfUndoHistoryEntry *entry)
{
  SgfUndoHistory *history = tree->undo_history;

  if (history) {
    SgfUndoHistoryEntry *this_entry;

    for (this_entry = (history->last_applied_entry
		       ? history->last_applied_entry->next
		       : history->first_entry);
	 this_entry;) {
      SgfUndoHistoryEntry *next_entry = this_entry->next;

      delete_undo_history_entry (this_entry, 0, tree);
      this_entry = next_entry;
    }

    if (history->last_applied_entry) {
      history->last_applied_entry->next = entry;
      entry->previous			= history->last_applied_entry;
    }
    else {
      history->first_entry = entry;
      entry->previous	   = NULL;
    }

    history->last_entry		= entry;
    history->last_applied_entry = entry;
    entry->next			= NULL;
    entry->is_last_in_action	= 0;
  }

  /* Perform the operation. */
  sgf_undo_operations[entry->operation_index].redo (entry, tree);

  if (!history) {
    delete_undo_history_entry (entry, 1, tree);

    tree->collection->is_irreversibly_modified = 1;
  }
}




void
sgf_operation_add_node (SgfUndoHistoryEntry *entry, SgfGameTree *tree)
{
  SgfNode *node	  = ((SgfNodeOperationEntry *) entry)->node;

  set_is_modifying_map (tree);
  set_is_modifying_tree (tree);

  tree->node_to_switch_to = node;

  * find_node_link (node->parent, node->next) = node;
  sgf_game_tree_invalidate_map (tree, node->parent);
}


void
sgf_operation_delete_node (SgfUndoHistoryEntry *entry, SgfGameTree *tree)
{
  SgfNode *node	  = ((SgfNodeOperationEntry *) entry)->node;
  SgfNode *parent = node->parent;

  set_is_modifying_map (tree);
  set_is_modifying_tree (tree);

  tree->node_to_switch_to = parent;

  * find_node_link (parent, node) = node->next;
  parent->current_variation = (((SgfNodeOperationEntry *) entry)
			       ->parent_current_variation);

  sgf_game_tree_invalidate_map (tree, parent);
}


void
sgf_operation_new_node_free_data (SgfUndoHistoryEntry *entry, int is_applied,
				  SgfGameTree *tree)
{
  if (!is_applied)
    sgf_node_delete (((SgfNodeOperationEntry *) entry)->node, tree);
}


void
sgf_operation_delete_node_free_data (SgfUndoHistoryEntry *entry,
				     int is_applied, SgfGameTree *tree)
{
  if (is_applied)
    sgf_node_delete (((SgfNodeOperationEntry *) entry)->node, tree);
}


void
sgf_operation_delete_node_children_undo (SgfUndoHistoryEntry *entry,
					 SgfGameTree *tree)
{
  SgfNode *first_child = ((SgfNodeOperationEntry *) entry)->node;
  SgfNode *parent      = first_child->parent;

  set_is_modifying_map (tree);
  set_is_modifying_tree (tree);

  tree->node_to_switch_to = parent;

  parent->child = first_child;
  parent->current_variation
    = ((SgfNodeOperationEntry *) entry)->parent_current_variation;

  sgf_game_tree_invalidate_map (tree, parent);
}


void
sgf_operation_delete_node_children_redo (SgfUndoHistoryEntry *entry,
					 SgfGameTree *tree)
{
  SgfNode *parent = ((SgfNodeOperationEntry *) entry)->node->parent;

  set_is_modifying_map (tree);
  set_is_modifying_tree (tree);

  tree->node_to_switch_to = parent;

  parent->child		    = NULL;
  parent->current_variation = NULL;

  sgf_game_tree_invalidate_map (tree, parent);
}


void
sgf_operation_delete_node_children_free_data (SgfUndoHistoryEntry *entry,
					      int is_applied,
					      SgfGameTree *tree)
{
  if (is_applied) {
    SgfNode *this_node = ((SgfNodeOperationEntry *) entry)->node;

    do {
      SgfNode *next_node = this_node->next;

      sgf_node_delete (this_node, tree);
      this_node = next_node;
    } while (this_node);
  }
}


void
sgf_operation_swap_nodes_do_swap (SgfUndoHistoryEntry *entry, 
				  SgfGameTree *tree)
{
  SgfNode *node1  = ((SgfTwoNodesOperationEntry *) entry)->node1;
  SgfNode *node2  = ((SgfTwoNodesOperationEntry *) entry)->node2;
  SgfNode *parent = node1->parent;
  SgfNode *temp;
  SgfNode **link;

  assert (node1->parent == node2->parent);

  set_is_modifying_map (tree);
  set_is_modifying_tree (tree);

  tree->node_to_switch_to = node1;

  link = &parent->child;
  while (*link != node1 && *link != node2) {
    assert (*link);
    link = & (*link)->next;
  }

  if (*link == node2) {
    temp  = node1;
    node1 = node2;
    node2 = temp;
  }

  *link = node2;

  temp	      = node1->next;
  node1->next = node2->next;
  node2->next = temp;

  do {
    assert (*link);
    link = & (*link)->next;
  } while (*link != node2);

  *link = node1;

  sgf_game_tree_invalidate_map (tree, parent);
}


/* Works as both undo and redo handler.  Since we just swap move
 * colors between the node and the undo history entry, it will always
 * do the right thing.
 */
void
sgf_operation_change_node_move_color_do_change (SgfUndoHistoryEntry *entry,
						SgfGameTree *tree)
{
  SgfChangeNodeInlinedColorOperationEntry *color_entry
    = (SgfChangeNodeInlinedColorOperationEntry *) entry;
  int temp_color;

  tree->node_to_switch_to = color_entry->node;

  temp_color			= color_entry->node->move_color;
  color_entry->node->move_color = color_entry->color;
  color_entry->color		= temp_color;
}


/* Works as both undo and redo handler.  Since we just swap move
 * colors between the node and the undo history entry, it will always
 * do the right thing.
 */
void
sgf_operation_change_node_to_play_color_do_change (SgfUndoHistoryEntry *entry,
						   SgfGameTree *tree)
{
  SgfChangeNodeInlinedColorOperationEntry *color_entry
    = (SgfChangeNodeInlinedColorOperationEntry *) entry;
  SgfNode *node = (SgfNode *) color_entry->node;
  int temp_color;

  tree->node_to_switch_to = node;

  temp_color	      = node->to_play_color;
  node->to_play_color = color_entry->color;
  color_entry->color  = temp_color;

  if (node == tree->current_node)
    sgf_utils_find_board_state_data (tree, 1, 0);
}


void
sgf_operation_add_property (SgfUndoHistoryEntry *entry, SgfGameTree *tree)
{
  SgfNode     *node	= ((SgfPropertyOperationEntry *) entry)->node;
  SgfProperty *property = ((SgfPropertyOperationEntry *) entry)->property;

  tree->node_to_switch_to = node;

  * find_property_link (node, property->next) = property;
}


void
sgf_operation_delete_property (SgfUndoHistoryEntry *entry, SgfGameTree *tree)
{
  SgfNode     *node	= ((SgfPropertyOperationEntry *) entry)->node;
  SgfProperty *property = ((SgfPropertyOperationEntry *) entry)->property;

  tree->node_to_switch_to = node;

  * find_property_link (node, property) = property->next;
}


void
sgf_operation_new_property_free_data (SgfUndoHistoryEntry *entry,
				      int is_applied, SgfGameTree *tree)
{
  if (!is_applied) {
    sgf_property_delete (((SgfPropertyOperationEntry *) entry)->property,
			 tree);
  }
}


void
sgf_operation_delete_property_free_data (SgfUndoHistoryEntry *entry,
					 int is_applied, SgfGameTree *tree)
{
  if (is_applied) {
    sgf_property_delete (((SgfPropertyOperationEntry *) entry)->property,
			 tree);
  }
}


/* Works as both undo and redo handler.  Since we just swap values
 * between the property and the undo history entry, it will always do
 * the right thing.
 */
void
sgf_operation_change_property_do_change (SgfUndoHistoryEntry *entry,
					 SgfGameTree *tree)
{
  SgfChangePropertyOperationEntry *const change_entry
    = (SgfChangePropertyOperationEntry *) entry;
  SgfNode     *node	= change_entry->node;
  SgfProperty *property = change_entry->property;
  SgfValue    *value	= &change_entry->value;

  tree->node_to_switch_to = node;

  switch (property_info[property->type].value_type) {
  case SGF_NUMBER:
  case SGF_DOUBLE:
  case SGF_COLOR:
    {
      int temp_number = property->value.number;

      property->value.number = value->number;
      value->number	     = temp_number;
    }

    break;

  case SGF_SIMPLE_TEXT:
  case SGF_FAKE_SIMPLE_TEXT:
  case SGF_TEXT:
    {
      char *temp_text = property->value.text;

      property->value.text = value->text;
      value->text	   = temp_text;
    }

    break;

  case SGF_TYPE_UNKNOWN:
    {
      StringList *temp_unknown_value_list = property->value.unknown_value_list;

      property->value.unknown_value_list = value->unknown_value_list;
      value->unknown_value_list		 = temp_unknown_value_list;
    }

    break;

  case SGF_LIST_OF_POINT:
  case SGF_ELIST_OF_POINT:
    {
      BoardPositionList *temp_position_list = property->value.position_list;

      property->value.position_list = value->position_list;
      value->position_list	    = temp_position_list;
    }

    break;

  case SGF_LIST_OF_VECTOR:
    {
      SgfVectorList *temp_vector_list = property->value.vector_list;

      property->value.vector_list = value->vector_list;
      value->vector_list	  = temp_vector_list;
    }

    break;

  case SGF_LIST_OF_LABEL:
    {
      SgfLabelList *temp_label_list = property->value.label_list;

      property->value.label_list = value->label_list;
      value->label_list		 = temp_label_list;
    }

    break;

  case SGF_FIGURE_DESCRIPTION:
    {
      SgfFigureDescription *temp_figure = property->value.figure;

      property->value.figure = value->figure;
      value->figure	     = temp_figure;
    }

    break;

  case SGF_REAL:
  case SGF_NONE:
  case SGF_NOT_STORED:
    assert (0);
  };
}


void
sgf_operation_change_property_free_data (SgfUndoHistoryEntry *entry,
					 int is_applied, SgfGameTree *tree)
{
  SgfProperty *property
    = ((SgfChangePropertyOperationEntry *) entry)->property;
  SgfValue *value = & ((SgfChangePropertyOperationEntry *) entry)->value;

  UNUSED (tree);
  UNUSED (is_applied);

  /* We free the value unconditionally: if the entry has been undone,
   * it contains the new value, else---the original.
   */
  sgf_property_free_value (property_info[property->type].value_type, value);
}


#if SGF_REAL_VALUES_ALLOCATED_SEPARATELY

/* Works as both undo and redo handler.  Since we just swap values
 * between the property and the undo history entry, it will always do
 * the right thing.
 */
void
sgf_operation_change_real_property_do_change (SgfUndoHistoryEntry *entry,
					      SgfGameTree *tree)
{
  SgfChangeRealPropertyOperationEntry *change_entry
    = (SgfChangeRealPropertyOperationEntry *) entry;
  SgfNode     *node	= change_entry->node;
  SgfProperty *property = change_entry->property;
  double temp_value;

  tree->node_to_switch_to = node;

  temp_value		= *property->value.real;
  *property->value.real = change_entry->value;
  change_entry->value	= temp_value;
}

#endif


void
sgf_operation_custom_undo (SgfUndoHistoryEntry *entry, SgfGameTree *tree)
{
  SgfCustomOperationEntry *custom_entry = (SgfCustomOperationEntry *) entry;

  if (custom_entry->entry_data->undo)
    custom_entry->entry_data->undo (custom_entry->user_data, tree);

  if (custom_entry->node_to_switch_to)
    tree->node_to_switch_to = custom_entry->node_to_switch_to;
}


void
sgf_operation_custom_redo (SgfUndoHistoryEntry *entry, SgfGameTree *tree)
{
  SgfCustomOperationEntry *custom_entry = (SgfCustomOperationEntry *) entry;

  if (custom_entry->entry_data->redo)
    custom_entry->entry_data->redo (custom_entry->user_data, tree);

  if (custom_entry->node_to_switch_to)
    tree->node_to_switch_to = custom_entry->node_to_switch_to;
}


void
sgf_operation_custom_free_data  (SgfUndoHistoryEntry *entry, int is_applied,
				 SgfGameTree *tree)
{
  SgfCustomOperationEntry *custom_entry = (SgfCustomOperationEntry *) entry;

  UNUSED (is_applied);

  if (custom_entry->entry_data->free_data)
    custom_entry->entry_data->free_data (custom_entry->user_data, tree);
}


inline static SgfNode **
find_node_link (SgfNode *parent, const SgfNode *next)
{
  SgfNode **link = &parent->child;

  while (*link != next) {
    assert (*link);
    link = & (*link)->next;
  }

  return link;
}


inline static SgfProperty **
find_property_link (SgfNode *node, const SgfProperty *next)
{
  SgfProperty **link = &node->properties;

  while (*link != next) {
    assert (*link);
    link = & (*link)->next;
  }

  return link;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
