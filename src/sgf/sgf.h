/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005 Paul Pogonyshev                  *
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


#ifndef QUARRY_SGF_H
#define QUARRY_SGF_H


#include "sgf-properties.h"
#include "utils.h"
#include "board.h"
#include "quarry.h"



/* `sgf-tree.c' global declarations and functions. */

#define SETUP_NODE		SPECIAL_ON_GRID_VALUE


enum {
  SGF_MARKUP_CROSS,
  SGF_MARKUP_CIRCLE,
  SGF_MARKUP_SQUARE,
  SGF_MARKUP_TRIANGLE,
  SGF_MARKUP_SELECTED,
  NUM_SGF_MARKUPS,

  SGF_MARKUP_NONE = NUM_SGF_MARKUPS
};


enum {
  SGF_STYLE_CURRENT_NODE_VARIATIONS = 1 << 0,
  SGF_STYLE_NO_BOARD_MARKUP	    = 1 << 1,

  SGF_STYLE_FLAGS_MASK		    = 0x3
};

enum {
  SGF_FIGURE_SHOW_COORDINATES  = 1 << 0,
  SGF_FIGURE_SHOW_DIAGRAM_NAME = 1 << 1,
  SGF_FIGURE_LIST_MISSED_MOVES = 1 << 2,
  SGF_FIGURE_REMOVE_CAPTURES   = 1 << 8,
  SGF_FIGURE_SHOW_HOSHI_POINTS = 1 << 9,
  SGF_FIGURE_USE_DEFAULTS      = 1 << 15,

  SGF_FIGURE_FLAGS_MASK	       = 0x8307
};

enum {
  SGF_PRINT_MODE_NO_MOVE_NUMBERS,
  SGF_PRINT_MODE_SHOW_MOVE_NUMBERS,
  SGF_PRINT_MODE_MOVE_NUMBERS_MODULO_100,

  NUM_SGF_PRINT_MODES
};


typedef enum {
  SGF_ABOUT_TO_MODIFY_MAP,
  SGF_ABOUT_TO_MODIFY_TREE,
  SGF_ABOUT_TO_CHANGE_CURRENT_NODE,

  SGF_CURRENT_NODE_CHANGED,
  SGF_TREE_MODIFIED,
  SGF_MAP_MODIFIED,

  SGF_GAME_TREE_DELETED
} SgfNotificationCode;


typedef struct _SgfVector		SgfVector;
typedef struct _SgfVectorList		SgfVectorList;

typedef struct _SgfLabel		SgfLabel;
typedef struct _SgfLabelList		SgfLabelList;

typedef struct _SgfFigureDescription	SgfFigureDescription;

typedef union  _SgfValue		SgfValue;
typedef struct _SgfProperty		SgfProperty;

typedef struct _SgfNode			SgfNode;
typedef struct _SgfNodeGeneric		SgfNodeGeneric;
typedef struct _SgfNodeGeneric		SgfNodeGo;
typedef struct _SgfNodeGeneric		SgfNodeOthello;
typedef struct _SgfNodeAmazons		SgfNodeAmazons;

typedef struct _SgfBoardState		SgfBoardState;

typedef struct _SgfUndoHistoryEntry	SgfUndoHistoryEntry;
typedef struct _SgfUndoHistory		SgfUndoHistory;

typedef struct _SgfGameTreeMapData	SgfGameTreeMapData;
typedef struct _SgfGameTreeMapLine	SgfGameTreeMapLine;
typedef struct _SgfGameTree		SgfGameTree;

typedef void (* SgfNotificationCallback)
  (SgfGameTree *tree, SgfNotificationCode notification_code, void *user_data);


typedef struct _SgfCollection		SgfCollection;


struct _SgfVector {
  BoardPoint		    from_point;
  BoardPoint		    to_point;
};

struct _SgfVectorList {
  /* This field is private to the implementation. */
  int			    allocated_num_vectors;

  int			    num_vectors;
  SgfVector		    vectors[1];
};


struct _SgfLabel {
  BoardPoint		    point;
  char			   *text;
};

struct _SgfLabelList {
  int			    num_labels;
  SgfLabel		    labels[1];
};


struct _SgfFigureDescription {
  int			    flags;
  char			   *diagram_name;
};


/* Union is a very handy thing, but unfortunately, standard C doesn't
 * allow casting to them.  The main problem is size of members: while
 * on any sane 32-bit machine they are all 32 bits, that's not true in
 * general case.  Therefore, casting from pointer to member type to
 * pointer to union (which is allowed by compiler) is a no-no thing.
 */
union _SgfValue {
  int			    number;

  /* Floats have so low precision, that there will be problems with
   * storing fractional number of seconds in it.  Will have to
   * allocate doubles on heap :(
   */
  double		   *real;

  int			    emphasized;
  int			    color;
  void			   *memory_block;
  char			   *text;
  BoardPositionList	   *position_list;
  SgfVectorList		   *vector_list;
  SgfLabelList		   *label_list;
  SgfFigureDescription	   *figure;

  /* For unknown properties.  First string stores identifier, the
   * rest---property values.
   */
  StringList		   *unknown_value_list;
};


#define SGF_TYPE_STORAGE_BITS	8

/* AFAIK, enums are unsigned when possible, but we better be careful
 * to avoid random bugs jumping out of nowhere.
 */
#if SGF_NUM_PROPERTIES > (1 << (SGF_TYPE_STORAGE_BITS - 1))
#error Too many property types.  Increase SGF_TYPE_STORAGE_BITS.
#endif

struct _SgfProperty {
  MEMORY_POOL_ITEM_INDEX;

  SgfType		    type : SGF_TYPE_STORAGE_BITS;
  SgfProperty		   *next;
  SgfValue		    value;
};


/* This strucuture is used for game-independent node access.  However,
 * accessing to game-specific fields (in `data' union) is only allowed
 * if the node belongs to game tree for corresponding game.  Otherwise
 * nasty things like memory corruption or access violation will
 * happen.
 */
struct _SgfNode {
  MEMORY_POOL_ITEM_INDEX;

  unsigned int		    is_collapsed : 1;
  unsigned int		    has_intermediate_map_data : 1;

  unsigned int		    move_color : 2;
  BoardPoint		    move_point;

  SgfNode		   *parent;
  SgfNode		   *child;
  SgfNode		   *next;
  SgfNode		   *current_variation;

  SgfProperty		   *properties;

  BoardAbstractMoveData	    data;
};

/* These two structure are only used in sgf_game_tree_set_game() to
 * determine the size of node required for given game.  For huge SGF
 * trees four bytes wasted on BoardAmazonsMoveData when it is not
 * required might lead to significant memory footprint increase.
 */
struct _SgfNodeGeneric {
  MEMORY_POOL_ITEM_INDEX;

  unsigned int		    is_collapsed : 1;
  unsigned int		    has_intermediate_map_data : 1;

  unsigned int		    move_color : 2;
  BoardPoint		    move_point;

  SgfNode		   *parent;
  SgfNode		   *child;
  SgfNode		   *next;
  SgfNode		   *current_variation;

  SgfProperty		   *properties;
};

struct _SgfNodeAmazons {
  MEMORY_POOL_ITEM_INDEX;

  unsigned int		    is_collapsed : 1;
  unsigned int		    has_intermediate_map_data : 1;

  unsigned int		    move_color : 2;
  BoardPoint		    move_point;

  SgfNode		   *parent;
  SgfNode		   *child;
  SgfNode		   *next;
  SgfNode		   *current_variation;

  SgfProperty		   *properties;

  BoardAmazonsMoveData	    amazons;
};


/* An SgfBoardState structure is associated with a tree (much like a
 * board.)  It is only used and kept valid by `sgf-utils' module.
 *
 * Fields must not be changed from outside `sgf-utils' module.
 */
struct _SgfBoardState {
  int		color_to_play;
  int		last_move_x;
  int		last_move_y;

  SgfNode      *game_info_node;
  int		game_info_node_depth;

  SgfNode      *last_move_node;

  /* Will be NULL if in main variation. */
  SgfNode      *last_main_variation_node;

  /* Time control data as stored in the game record.  Negative values
   * mean that the property is not set.
   */
  double	time_left[NUM_COLORS];
  int		moves_left[NUM_COLORS];

  /* Private fields.  Must be of no interest outside `sgf-utils'. */
  int		sgf_color_to_play;
};


struct _SgfUndoHistory {
  SgfUndoHistoryEntry	   *first_entry;
  SgfUndoHistoryEntry	   *last_entry;
  SgfUndoHistoryEntry	   *last_applied_entry;
};


struct _SgfGameTreeMapData {
  SgfGameTreeMapData	   *next;

  SgfNode		   *node;
  int			    x;
  int			   *y_level;
  int			    last_valid_y_level;

  int			    largest_x_so_far;
};

struct _SgfGameTreeMapLine {
  int			    x0;
  int			    y0;
  int			    y1;
  int			    x2;
  int			    x3;
};

struct _SgfGameTree {
  SgfGameTree		   *previous;
  SgfGameTree		   *next;

  SgfNode		   *root;
  SgfNode		   *current_node;
  int			    current_node_depth;

  int			    game;
  int			    board_width;
  int			    board_height;
  Board			   *board;
  SgfBoardState		   *board_state;

  SgfUndoHistory	   *undo_history;

  int			    file_format;
  char			   *char_set;
  char			   *application_name;
  char			   *application_version;
  int			    style_is_set;
  int			    style;

  MemoryPool		    node_pool;
  MemoryPool		    property_pool;

  void			   *user_data;
  SgfNotificationCallback   notification_callback;

  /* The ``map.'' */
  int			    map_width;
  int			    map_height;

  SgfGameTreeMapData	   *map_data_list;
  SgfGameTreeMapData	   *last_valid_data_point;

  int			    view_port_x0;
  int			    view_port_y0;
  int			    view_port_x1;
  int			    view_port_y1;

  SgfNode		  **view_port_nodes;

  int			    num_view_port_lines;
  SgfGameTreeMapLine	   *view_port_lines;
};

struct _SgfCollection {
  int			    num_trees;
  SgfGameTree		   *first_tree;
  SgfGameTree		   *last_tree;
};


typedef struct _SgfGameTreeState	SgfGameTreeState;

struct _SgfGameTreeState {
  Board			   *board;
  SgfBoardState		   *board_state;

  SgfNode		   *current_node;
  int			    current_node_depth;
};


typedef enum {
  SGF_RESULT_WIN,
  SGF_RESULT_BLACK_WIN		      = SGF_RESULT_WIN + BLACK_INDEX,
  SGF_RESULT_WHITE_WIN		      = SGF_RESULT_WIN + WHITE_INDEX,

  SGF_RESULT_WIN_BY_FORFEIT,
  SGF_RESULT_BLACK_WIN_BY_FORFEIT     = (SGF_RESULT_WIN_BY_FORFEIT
					 + BLACK_INDEX),
  SGF_RESULT_WHITE_WIN_BY_FORFEIT     = (SGF_RESULT_WIN_BY_FORFEIT
					 + WHITE_INDEX),

  SGF_RESULT_WIN_BY_RESIGNATION,
  SGF_RESULT_BLACK_WIN_BY_RESIGNATION = (SGF_RESULT_WIN_BY_RESIGNATION
					 + BLACK_INDEX),
  SGF_RESULT_WHITE_WIN_BY_RESIGNATION = (SGF_RESULT_WIN_BY_RESIGNATION
					 + WHITE_INDEX),

  SGF_RESULT_WIN_BY_SCORE,
  SGF_RESULT_BLACK_WIN_BY_SCORE	      = SGF_RESULT_WIN_BY_SCORE + BLACK_INDEX,
  SGF_RESULT_WHITE_WIN_BY_SCORE	      = SGF_RESULT_WIN_BY_SCORE + WHITE_INDEX,

  SGF_RESULT_WIN_BY_TIME,
  SGF_RESULT_BLACK_WIN_BY_TIME	      = SGF_RESULT_WIN_BY_TIME + BLACK_INDEX,
  SGF_RESULT_WHITE_WIN_BY_TIME	      = SGF_RESULT_WIN_BY_TIME + WHITE_INDEX,

  SGF_RESULT_UNKNOWN,
  SGF_RESULT_DRAW,
  SGF_RESULT_VOID,
  SGF_RESULT_NOT_SET,
  SGF_RESULT_INVALID
} SgfResult;


SgfCollection *	 sgf_collection_new (void);
void		 sgf_collection_delete (SgfCollection *collection);
void		 sgf_collection_add_game_tree (SgfCollection *collection,
					       SgfGameTree *tree);


SgfGameTree *	 sgf_game_tree_new (void);
SgfGameTree *	 sgf_game_tree_new_with_root (Game game,
					      int board_width,
					      int board_height,
					      int provide_default_setup);
void		 sgf_game_tree_delete (SgfGameTree *tree);

void		 sgf_game_tree_set_game (SgfGameTree *tree, Game game);
void		 sgf_game_tree_set_state (SgfGameTree *tree,
					  const SgfGameTreeState *state);
void		 sgf_game_tree_get_state (SgfGameTree *tree,
					  SgfGameTreeState *state);

SgfGameTree *	 sgf_game_tree_duplicate (const SgfGameTree *tree);
SgfGameTree *	 sgf_game_tree_duplicate_with_nodes (const SgfGameTree *tree);

SgfNode *	 sgf_game_tree_traverse_forward (const SgfGameTree *tree);
SgfNode *	 sgf_game_tree_traverse_backward (const SgfGameTree *tree);

int		 sgf_game_tree_count_nodes (const SgfGameTree *tree);

void		 sgf_game_tree_set_notification_callback
		   (SgfGameTree *tree,
		    SgfNotificationCallback callback, void *user_data);


SgfNode *	 sgf_node_new (SgfGameTree *tree, SgfNode *parent);
void		 sgf_node_delete (SgfNode *node, SgfGameTree *tree);

SgfNode *	 sgf_node_get_last_child (const SgfNode *node);

SgfNode *	 sgf_node_duplicate (const SgfNode *node,
				     SgfGameTree *tree, SgfNode *parent);
SgfNode *	 sgf_node_duplicate_recursively (const SgfNode *node,
						 SgfGameTree *tree,
						 SgfNode *parent);
SgfNode *	 sgf_node_duplicate_to_given_depth (const SgfNode *node,
						    SgfGameTree *tree,
						    SgfNode *parent,
						    int depth);

int		 sgf_node_find_property (SgfNode *node, SgfType type,
					 SgfProperty ***link);
int		 sgf_node_find_unknown_property (SgfNode *node,
						 char *id, int length,
						 SgfProperty ***link);

int		 sgf_node_is_game_info_node (const SgfNode *node);


int		 sgf_node_get_number_property_value (const SgfNode *node,
						     SgfType type,
						     int *number);
int		 sgf_node_get_double_property_value (const SgfNode *node,
						     SgfType type);
int		 sgf_node_get_color_property_value (const SgfNode *node,
						    SgfType type);
int		 sgf_node_get_real_property_value (const SgfNode *node,
						   SgfType type,
						   double *value);
const char *	 sgf_node_get_text_property_value (const SgfNode *node,
						   SgfType type);
const BoardPositionList *
		 sgf_node_get_list_of_point_property_value
		   (const SgfNode *node, SgfType type);
const SgfVectorList *
		 sgf_node_get_list_of_vector_property_value
		   (const SgfNode *node, SgfType type);
const SgfLabelList *
		 sgf_node_get_list_of_label_property_value
		   (const SgfNode *node, SgfType type);

int		 sgf_node_get_handicap (const SgfNode *node);
int		 sgf_node_get_komi (const SgfNode *node, double *komi);
SgfResult	 sgf_node_get_result (const SgfNode *node, double *score);
int		 sgf_node_get_time_limit (const SgfNode *node,
					  double *time_limit);


int		 sgf_node_add_none_property (SgfNode *node, SgfGameTree *tree,
					     SgfType type);
int		 sgf_node_add_number_property (SgfNode *node,
					       SgfGameTree *tree,
					       SgfType type, int number,
					       int overwrite);
int		 sgf_node_add_real_property (SgfNode *node, SgfGameTree *tree,
					     SgfType type, double value,
					     int overwrite);
int		 sgf_node_add_pointer_property (SgfNode *node,
						SgfGameTree *tree,
						SgfType type, void *pointer,
						int overwrite);

#define sgf_node_add_double_property(node, tree, type, emphasized,	\
				     overwrite)				\
  sgf_node_add_number_property ((node), (tree), (type), (emphasized),	\
				(overwrite))

#define sgf_node_add_color_property(node, tree, type, color, overwrite)	\
  sgf_node_add_number_property ((node), (tree), (type), (color),	\
				(overwrite))

#define sgf_node_add_text_property(node, tree, type, text, overwrite)	\
  sgf_node_add_pointer_property ((node), (tree), (type), (text),	\
				 (overwrite))

#define sgf_node_add_list_of_point_property(node, tree,			\
					    type, position_list,	\
					    overwrite)			\
  sgf_node_add_pointer_property ((node), (tree),			\
				 (type), (position_list), (overwrite))

#define sgf_node_add_list_of_label_property(node, tree,			\
					    type, label_list,		\
					    overwrite)			\
  sgf_node_add_pointer_property ((node), (tree), (type), (label_list),	\
				 (overwrite))


int		 sgf_node_add_score_result (SgfNode *node, SgfGameTree *tree,
					    double score, int overwrite);

int		 sgf_node_append_text_property (SgfNode *node,
						SgfGameTree *tree,
						SgfType type,
						char *text,
						const char *separator);


int		 sgf_node_delete_property (SgfNode *node, SgfGameTree *tree,
					   SgfType type);

void		 sgf_node_split (SgfNode *node, SgfGameTree *tree);


SgfNode *	 sgf_node_traverse_forward (const SgfNode *node);
SgfNode *	 sgf_node_traverse_backward (const SgfNode *node);


int		 sgf_node_count_subtree_nodes (const SgfNode *node);


inline SgfProperty *
		 sgf_property_new (SgfGameTree *tree, SgfType type,
				   SgfProperty *next);
inline void	 sgf_property_delete (SgfProperty *property,
				      SgfGameTree *tree);
void		 sgf_property_delete_at_link (SgfProperty **link,
					      SgfGameTree *tree);

SgfProperty *	 sgf_property_duplicate (const SgfProperty *property,
					 SgfGameTree *tree, SgfProperty *next);


SgfVectorList *	 sgf_vector_list_new (int num_vectors);

#define sgf_vector_list_delete(list)		\
  do {						\
    assert (list);				\
    utils_free (list);				\
  } while (0)


SgfVectorList *	 sgf_vector_list_shrink (SgfVectorList *list);

SgfVectorList *	 sgf_vector_list_add_vector (SgfVectorList *list,
					     BoardPoint from_point,
					     BoardPoint to_point);
int		 sgf_vector_list_has_vector (const SgfVectorList *list,
					     BoardPoint from_point,
					     BoardPoint to_point);

SgfVectorList *	 sgf_vector_list_duplicate (const SgfVectorList *list);


SgfLabelList *	 sgf_label_list_new (int num_labels,
				     BoardPoint *points, char **labels);
SgfLabelList *	 sgf_label_list_new_empty (int num_labels);
void		 sgf_label_list_delete (SgfLabelList *list);

SgfLabelList *	 sgf_label_list_duplicate (const SgfLabelList *list);


SgfFigureDescription *
		 sgf_figure_description_new (int flags, char *diagram_name);

void		 sgf_figure_description_delete (SgfFigureDescription *figure);

SgfFigureDescription *
		 sgf_figure_description_duplicate
		   (const SgfFigureDescription *figure);



/* `sgf-parser.c' global declarations, functions and variables. */

#define SGF_MIN_BOARD_SIZE	 1
#define SGF_MAX_BOARD_SIZE	52


enum {
  SGF_PARSED,
  SGF_PARSING_CANCELLED,
  SGF_ERROR_READING_FILE,
  SGF_INVALID_FILE
};


typedef struct _SgfParserParameters	SgfParserParameters;

struct _SgfParserParameters {
  int		max_buffer_size;
  int		buffer_refresh_margin;
  int		buffer_size_increment;

  int		first_column;
};


typedef struct _SgfErrorListItem	SgfErrorListItem;
typedef struct _SgfErrorList		SgfErrorList;

struct _SgfErrorListItem {
  SgfErrorListItem	 *next;
  char			 *text;

  int			  line;
  int			  column;
};

struct _SgfErrorList {
  SgfErrorListItem	 *first;
  SgfErrorListItem	 *last;

  int			  item_size;
  StringListItemDispose	  item_dispose;
};


#define sgf_error_list_new()						\
  ((SgfErrorList *) string_list_new_derived (sizeof (SgfErrorListItem),	\
					     NULL))

#define sgf_error_list_init(list)					\
  string_list_init_derived ((list), sizeof (SgfErrorListItem), NULL)

#define STATIC_SGF_ERROR_LIST						\
  STATIC_STRING_LIST_DERIVED (SgfErrorListItem, NULL)


#define sgf_error_list_get_item(list, item_index)			\
  ((SgfErrorListItem *) string_list_get_item ((list), (item_index)))

#define sgf_error_list_find(list, text)					\
  ((SgfErrorListItem *) string_list_find ((list), (text)))

#define sgf_error_list_find_after_notch(list, text, notch)		\
  ((SgfErrorListItem *)							\
   string_list_find_after_notch ((list), (text) (notch)))


int		 sgf_parse_file (const char *filename,
				 SgfCollection **collection,
				 SgfErrorList **error_list,
				 const SgfParserParameters *parameters,
				 int *file_size, int *bytes_parsed,
				 const int *cancellation_flag);
int		 sgf_parse_buffer (char *buffer, int size,
				   SgfCollection **collection,
				   SgfErrorList **error_list,
				   const SgfParserParameters *parameters,
				   int *bytes_parsed,
				   const int *cancellation_flag);


extern const SgfParserParameters	sgf_parser_defaults;



/* `sgf-writer.c' global functions. */

int		 sgf_write_file (const char *filename,
				 SgfCollection *collection, int force_utf8);



/* `sgf-utils.c' global declarations and functions. */

typedef enum {
  SGF_NEXT,
  SGF_PREVIOUS
} SgfDirection;


inline void   sgf_utils_play_node_move (const SgfNode *node, Board *board);
int	      sgf_utils_format_node_move (const SgfGameTree *tree,
					  const SgfNode *node,
					  char *buffer,
					  const char *black_string,
					  const char *white_string,
					  const char *pass_string);

void	      sgf_utils_enter_tree (SgfGameTree *tree, Board *board,
				    SgfBoardState *board_state);

void	      sgf_utils_go_down_in_tree (SgfGameTree *tree, int num_nodes);
void	      sgf_utils_go_up_in_tree (SgfGameTree *tree, int num_nodes);

int	      sgf_utils_count_variations
		(const SgfGameTree *tree, int of_current_node,
		 int black_variations[BOARD_GRID_SIZE],
		 int white_variations[BOARD_GRID_SIZE],
		 int *other_variations);
SgfNode *     sgf_utils_find_variation_at_position (SgfGameTree *tree,
						    int x, int y,
						    SgfDirection direction,
						    int after_current);

void	      sgf_utils_switch_to_variation (SgfGameTree *tree,
					     SgfDirection direction);
void	      sgf_utils_switch_to_given_variation (SgfGameTree *tree,
						   SgfNode *node);
void	      sgf_utils_switch_to_given_node (SgfGameTree *tree,
					      SgfNode *node);

void	      sgf_utils_append_variation (SgfGameTree *tree,
					  int color, ...);

void	      sgf_utils_delete_current_node (SgfGameTree *tree);
void	      sgf_utils_delete_current_node_children (SgfGameTree *tree);


#define sgf_utils_can_undo(tree)					\
  ((tree)->undo_history && (tree)->undo_history->last_applied_entry)

#define sgf_utils_can_redo(tree)					\
  ((tree)->undo_history							\
   && ((tree)->undo_history->last_applied_entry				\
       != (tree)->undo_history->last_entry))

void	      sgf_utils_undo (SgfGameTree *tree);
void	      sgf_utils_redo (SgfGameTree *tree);


void	      sgf_utils_set_node_is_collapsed (SgfGameTree *tree,
					       SgfNode *node,
					       int is_collapsed);

void	      sgf_utils_get_markup (const SgfGameTree *tree,
				    char markup[BOARD_GRID_SIZE]);
void	      sgf_utils_mark_territory_on_grid (const SgfGameTree *tree,
						char grid[BOARD_GRID_SIZE],
						char black_territory_mark,
						char white_territory_mark);

void	      sgf_utils_set_handicap (SgfGameTree *tree,
				      int handicap, int is_fixed);
void	      sgf_utils_add_free_handicap_stones
		(SgfGameTree *tree, BoardPositionList *handicap_stones);


char *	      sgf_utils_normalize_text (const char *text, int is_simple_text);


SgfUndoHistory *
	      sgf_undo_history_new (void);
void	      sgf_undo_history_delete (SgfUndoHistory *history,
				       SgfGameTree *associated_tree);



/* `sgf-diff-utils.c' global functions. */

SgfCollection *	 sgf_diff (const SgfCollection *from_collection,
			   const SgfCollection *to_collection);



/* `sgf-tree-map.c' global declarations and functions. */

enum {
  SGF_NO_NODE,
  SGF_NON_CURRENT_BRANCH_NODE,
  SGF_CURRENT_BRANCH_HEAD_NODE,
  SGF_CURRENT_NODE,
  SGF_CURRENT_BRANCH_TAIL_NODE
};


void		sgf_game_tree_invalidate_map (SgfGameTree *tree,
					      SgfNode *node);

void		sgf_game_tree_get_map_dimensions (SgfGameTree *tree,
						  int *map_width,
						  int *map_height);
void		sgf_game_tree_fill_map_view_port
		  (SgfGameTree *tree,
		   int view_port_x0, int view_port_y0,
		   int view_port_x1, int view_port_y1,
		   SgfNode ***view_port_nodes,
		   SgfGameTreeMapLine **view_port_lines,
		   int *num_view_port_lines);
char *		sgf_game_tree_get_current_branch_marks (SgfGameTree *tree,
							int view_port_x0,
							int view_port_y0,
							int view_port_x1,
							int view_port_y1);

int		sgf_game_tree_get_node_coordinates (SgfGameTree *tree,
						    const SgfNode *node,
						    int *node_x, int *node_y);
int		sgf_game_tree_node_is_within_view_port (SgfGameTree *tree,
							const SgfNode *node,
							int view_port_x0,
							int view_port_y0,
							int view_port_x1,
							int view_port_y1,
							int *node_x,
							int *node_y);


#endif /* QUARRY_SGF_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
