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


#ifndef QUARRY_SGF_UNDO_H
#define QUARRY_SGF_UNDO_H


#include "sgf.h"
#include "sgf-undo-operations.h"
#include "quarry.h"


typedef struct _SgfUndoOperationInfo	SgfUndoOperationInfo;

struct _SgfUndoOperationInfo {
  void (* undo) (SgfUndoHistoryEntry *entry, SgfGameTree *tree);
  void (* redo) (SgfUndoHistoryEntry *entry, SgfGameTree *tree);

  void (* free_data) (SgfUndoHistoryEntry *entry, int is_applied,
		      SgfGameTree *tree);
};


struct _SgfUndoHistoryEntry {
  SgfUndoHistoryEntry  *next;
  SgfUndoHistoryEntry  *previous;

  unsigned char		operation_index;
  char			is_last_in_action;
  char			is_hidden;
};


typedef struct _SgfNodeOperationEntry		SgfNodeOperationEntry;
typedef struct _SgfTwoNodesOperationEntry	SgfTwoNodesOperationEntry;
typedef struct _SgfChangeNodeInlinedColorOperationEntry
		SgfChangeNodeInlinedColorOperationEntry;

typedef struct _SgfPropertyOperationEntry	SgfPropertyOperationEntry;
typedef struct _SgfChangePropertyOperationEntry
		SgfChangePropertyOperationEntry;
typedef struct _SgfChangeRealPropertyOperationEntry
		SgfChangeRealPropertyOperationEntry;

typedef struct _SgfCustomOperationEntry		SgfCustomOperationEntry;

struct _SgfNodeOperationEntry {
  SgfUndoHistoryEntry	entry;

  SgfNode	       *node;
  SgfNode	       *parent_current_variation;
};

struct _SgfTwoNodesOperationEntry {
  SgfUndoHistoryEntry	entry;

  SgfNode	       *node1;
  SgfNode	       *node2;
  SgfNode	       *parent_current_variation;
};

struct _SgfChangeNodeInlinedColorOperationEntry {
  SgfUndoHistoryEntry	entry;

  SgfNode	       *node;
  int			color;
  int			side_effect;
};

struct _SgfPropertyOperationEntry {
  SgfUndoHistoryEntry	entry;

  SgfNode	       *node;
  SgfProperty	       *property;
  int			side_effect;
};

struct _SgfChangePropertyOperationEntry {
  SgfUndoHistoryEntry	entry;

  SgfNode	       *node;
  SgfProperty	       *property;
  SgfValue		value;
  int			side_effect;
};

struct _SgfChangeRealPropertyOperationEntry {
  SgfUndoHistoryEntry	entry;

  SgfNode	       *node;
  SgfProperty	       *property;
  double		value;
  int			side_effect;
};

struct _SgfCustomOperationEntry {
  SgfUndoHistoryEntry	entry;

  const SgfCustomUndoHistoryEntryData  *entry_data;
  void		       *user_data;

  SgfNode	       *node_to_switch_to;
};


/* Undo operations. */
extern const SgfUndoOperationInfo    sgf_undo_operations[];


#define DECLARE_UNDO_OR_REDO_FUNCTION(name)				\
  void		name (SgfUndoHistoryEntry *entry, SgfGameTree *tree);

#define DECLARE_FREE_DATA_FUNCTION(name)				\
  void		name (SgfUndoHistoryEntry *entry, int is_applied,	\
		      SgfGameTree *tree);


/* Used both for node adding and removing undo/redo. */
DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_add_node);
DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_delete_node);

DECLARE_FREE_DATA_FUNCTION (sgf_operation_new_node_free_data);

DECLARE_FREE_DATA_FUNCTION (sgf_operation_delete_node_free_data);

DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_delete_node_children_undo);
DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_delete_node_children_redo);
DECLARE_FREE_DATA_FUNCTION (sgf_operation_delete_node_children_free_data);

/* Used both for node adding and removing undo/redo. */
DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_swap_nodes_do_swap);

DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_change_node_move_color_do_change);
DECLARE_UNDO_OR_REDO_FUNCTION
  (sgf_operation_change_node_to_play_color_do_change);

/* Used both for node adding and removing undo/redo. */
DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_add_property);
DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_delete_property);

DECLARE_FREE_DATA_FUNCTION (sgf_operation_new_property_free_data);
DECLARE_FREE_DATA_FUNCTION (sgf_operation_delete_property_free_data);

/* Used as both undo and redo handler. */
DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_change_property_do_change);

DECLARE_FREE_DATA_FUNCTION (sgf_operation_change_property_free_data);


#if SGF_REAL_VALUES_ALLOCATED_SEPARATELY

/* Used as both undo and redo handler. */
DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_change_real_property_do_change);

#endif


DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_custom_undo);
DECLARE_UNDO_OR_REDO_FUNCTION (sgf_operation_custom_redo);
DECLARE_FREE_DATA_FUNCTION (sgf_operation_custom_free_data);


void		       sgf_utils_apply_undo_history_entry
			 (SgfGameTree *tree, SgfUndoHistoryEntry *entry);


SgfUndoHistoryEntry *  sgf_new_node_undo_history_entry_new (SgfNode *new_node);
SgfUndoHistoryEntry *  sgf_delete_node_undo_history_entry_new (SgfNode *node);
SgfUndoHistoryEntry *  sgf_delete_node_children_undo_history_entry_new
			 (SgfNode *node);
SgfUndoHistoryEntry *  sgf_swap_nodes_undo_history_entry_new (SgfNode *node1,
							      SgfNode *node2);
SgfUndoHistoryEntry *  sgf_change_node_inlined_color_undo_history_entry_new
			 (SgfNode *node, SgfUndoOperation operation,
			  int new_color, int side_effect);

SgfUndoHistoryEntry *  sgf_new_property_undo_history_entry_new
			 (SgfGameTree *tree, SgfNode *node, SgfProperty **link,
			  SgfType type, int side_effect);
SgfUndoHistoryEntry *  sgf_delete_property_undo_history_entry_new
			 (SgfNode *node, SgfProperty *property, int side_effect);
SgfUndoHistoryEntry *  sgf_change_property_undo_history_entry_new
			 (SgfNode *node, SgfProperty *property, int side_effect);
SgfUndoHistoryEntry *  sgf_change_real_property_undo_history_entry_new
			 (SgfNode *node, SgfProperty *property,
			  double new_value, int side_effect);

SgfUndoHistoryEntry *  sgf_custom_undo_history_entry_new
			 (const SgfCustomUndoHistoryEntryData *entry_data,
			  void *user_data, SgfNode *node_to_switch_to);


#endif /* QUARRY_SGF_UNDO_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
