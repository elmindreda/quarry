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


typedef struct _SgfLabel	SgfLabel;
typedef struct _SgfLabelList	SgfLabelList;

typedef union  _SgfValue	SgfValue;
typedef struct _SgfProperty	SgfProperty;

typedef struct _SgfNode		SgfNode;
typedef struct _SgfNodeGeneric	SgfNodeGeneric;
typedef struct _SgfNodeGeneric	SgfNodeGo;
typedef struct _SgfNodeGeneric	SgfNodeOthello;
typedef struct _SgfNodeAmazons	SgfNodeAmazons;


typedef struct _SgfGameTree	SgfGameTree;
typedef struct _SgfCollection	SgfCollection;

struct _SgfLabel {
  BoardPoint		  point;
  char			 *text;
};

struct _SgfLabelList {
  int			  num_labels;
  SgfLabel		  labels[1];
};

/* Union is a very handy thing, but unfortunately, standard C doesn't
 * allow casting to them.  The main problem is size of members: while
 * on any sane 32-bit machine they are all 32 bits, that's not true in
 * general case.  Therefore, casting from pointer to member type to
 * pointer to union (which is allowed by compiler) is a no-no thing.
 */
union _SgfValue {
  int			  number;

  /* Floats have so low precision, that there will be problems with
   * storing fractional number of seconds in it.  Will have to
   * allocate doubles on heap :(
   */
  double		 *real;

  int			  emphasized;
  int			  color;
  void			 *memory_block;
  char			 *text;
  BoardPositionList	 *position_list;
  SgfLabelList		 *label_list;
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

  SgfType		  type : SGF_TYPE_STORAGE_BITS;
  SgfProperty		 *next;
  SgfValue		  value;
};


/* This strucuture is used for game-independent node access.  However,
 * accessing to game-specific fields (in `data' union) is only allowed
 * if the node belongs to game tree for corresponding game.  Otherwise
 * nasty things like memory corruption or access violation will
 * happen.
 */
struct _SgfNode {
  MEMORY_POOL_ITEM_INDEX;

  char			  move_color;
  BoardPoint		  move_point;

  SgfNode		 *parent;
  SgfNode		 *child;
  SgfNode		 *next;
  SgfNode		 *current_variation;

  SgfProperty		 *properties;

  BoardAbstractMoveData	  data;
};

/* These two structure are only used in sgf_game_tree_set_game() to
 * determine the size of node required for given game.  For huge SGF
 * trees four bytes wasted on BoardAmazonsMoveData when it is not
 * required might lead to significant memory footprint increase.
 */
struct _SgfNodeGeneric {
  MEMORY_POOL_ITEM_INDEX;

  char			 move_color;
  BoardPoint		 move_point;

  SgfNode		*parent;
  SgfNode		*child;
  SgfNode		*next;
  SgfNode		*current_variation;

  SgfProperty		*properties;
};

struct _SgfNodeAmazons {
  MEMORY_POOL_ITEM_INDEX;

  char			  move_color;
  BoardPoint		  move_point;

  SgfNode		 *parent;
  SgfNode		 *child;
  SgfNode		 *next;
  SgfNode		 *current_variation;

  SgfProperty		 *properties;

  BoardAmazonsMoveData	  amazons;
};


struct _SgfGameTree {
  SgfGameTree		*previous;
  SgfGameTree		*next;

  SgfNode		*root;
  SgfNode		*current_node;

  int			 game;
  int			 board_width;
  int			 board_height;
  Board			*board;

  int			 file_format;
  char			*char_set;
  char			*application_name;
  char			*application_version;
  int			 variation_style;

  MemoryPool		 node_pool;
  MemoryPool		 property_pool;
};

struct _SgfCollection {
  int			 num_trees;
  SgfGameTree		*first_tree;
  SgfGameTree		*last_tree;
};


typedef struct _SgfGameTreeState	SgfGameTreeState;

struct _SgfGameTreeState {
  Board	       *board;
  SgfNode      *current_node;
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


SgfCollection *	 sgf_collection_new(void);
void		 sgf_collection_delete(SgfCollection *collection);
void		 sgf_collection_add_game_tree(SgfCollection *collection,
					      SgfGameTree *tree);


SgfGameTree *	 sgf_game_tree_new(void);
SgfGameTree *	 sgf_game_tree_new_with_root(Game game,
					     int board_width,
					     int board_height,
					     int provide_default_setup);
void		 sgf_game_tree_delete(SgfGameTree *tree);

void		 sgf_game_tree_set_game(SgfGameTree *tree, Game game);
void		 sgf_game_tree_set_state(SgfGameTree *tree,
					 Board *board, SgfNode *node,
					 SgfGameTreeState *old_state);

SgfGameTree *	 sgf_game_tree_duplicate(const SgfGameTree *tree);
SgfGameTree *	 sgf_game_tree_duplicate_with_nodes(const SgfGameTree *tree);

int		 sgf_game_tree_count_nodes(const SgfGameTree *tree);


SgfNode *	 sgf_node_new(SgfGameTree *tree, SgfNode *parent);
void		 sgf_node_delete(SgfNode *node, SgfGameTree *tree);

SgfNode *	 sgf_node_append_child(SgfNode *node, SgfGameTree *tree);

SgfNode *	 sgf_node_duplicate(const SgfNode *node,
				    SgfGameTree *tree, SgfNode *parent);
SgfNode *	 sgf_node_duplicate_recursively(const SgfNode *node,
						SgfGameTree *tree,
						SgfNode *parent);
SgfNode *	 sgf_node_duplicate_to_given_depth(const SgfNode *node,
						   SgfGameTree *tree,
						   SgfNode *parent,
						   int depth);

int		 sgf_node_find_property(SgfNode *node, SgfType type,
					SgfProperty ***link);
int		 sgf_node_find_unknown_property(SgfNode *node,
						char *id, int length,
						SgfProperty ***link);

int		 sgf_node_is_game_info_node(const SgfNode *node);


int		 sgf_node_get_number_property_value(const SgfNode *node,
						    SgfType type, int *number);
int		 sgf_node_get_double_property_value(const SgfNode *node,
						    SgfType type);
int		 sgf_node_get_color_property_value(const SgfNode *node,
						   SgfType type);
int		 sgf_node_get_real_property_value(const SgfNode *node,
						  SgfType type, double *value);
const char *	 sgf_node_get_text_property_value(const SgfNode *node,
						       SgfType type);
const BoardPositionList *
		 sgf_node_get_list_of_point_property_value(const SgfNode *node,
							   SgfType type);
const SgfLabelList *
		 sgf_node_get_list_of_label_property_value(const SgfNode *node,
							   SgfType type);

int		 sgf_node_get_handicap(const SgfNode *node);
int		 sgf_node_get_komi(const SgfNode *node, double *komi);
SgfResult	 sgf_node_get_result(const SgfNode *node, double *score);
int		 sgf_node_get_time_limit(const SgfNode *node,
					 double *time_limit);


int		 sgf_node_add_none_property(SgfNode *node, SgfGameTree *tree,
					    SgfType type);
int		 sgf_node_add_number_property(SgfNode *node, SgfGameTree *tree,
					      SgfType type, int number,
					      int overwrite);
int		 sgf_node_add_real_property(SgfNode *node, SgfGameTree *tree,
					    SgfType type, double value,
					    int overwrite);
int		 sgf_node_add_pointer_property(SgfNode *node, SgfGameTree *tree,
					       SgfType type, void *pointer,
					       int overwrite);

#define sgf_node_add_double_property(node, tree, type, emphasized,	\
				     overwrite)				\
  sgf_node_add_number_property((node), (tree), (type), (emphasized),	\
			       (overwrite))

#define sgf_node_add_color_property(node, tree, type, color, overwrite)	\
  sgf_node_add_number_property((node), (tree), (type), (color),		\
			       (overwrite))

#define sgf_node_add_text_property(node, tree, type, text, overwrite)	\
  sgf_node_add_pointer_property((node), (tree), (type), (text),		\
				(overwrite))

#define sgf_node_add_list_of_point_property(node, tree,			\
					    type, position_list,	\
					    overwrite)			\
  sgf_node_add_pointer_property((node), (tree),				\
				(type), (position_list), (overwrite))

#define sgf_node_add_list_of_label_property(node, tree,			\
					    type, label_list,		\
					    overwrite)			\
  sgf_node_add_pointer_property((node), (tree), (type), (label_list),	\
				(overwrite))


int		 sgf_node_add_score_result(SgfNode *node, SgfGameTree *tree,
					   double score, int overwrite);

int		 sgf_node_append_text_property(SgfNode *node,
					       SgfGameTree *tree,
					       SgfType type,
					       char *text,
					       const char *separator);


int		 sgf_node_delete_property(SgfNode *node, SgfGameTree *tree,
					  SgfType type);

void		 sgf_node_split(SgfNode *node, SgfGameTree *tree);


int		 sgf_node_count_subtree_nodes(const SgfNode *node);


inline SgfProperty *
		 sgf_property_new(SgfGameTree *tree, SgfType type,
				  SgfProperty *next);
inline void	 sgf_property_delete(SgfProperty *property, SgfGameTree *tree);
void		 sgf_property_delete_at_link(SgfProperty **link,
					     SgfGameTree *tree);

SgfProperty *	 sgf_property_duplicate(const SgfProperty *property,
					SgfGameTree *tree, SgfProperty *next);


SgfLabelList *	 sgf_label_list_new(int num_labels,
				    BoardPoint *points, char **labels);
SgfLabelList *	 sgf_label_list_new_empty(int num_labels);
void		 sgf_label_list_delete(SgfLabelList *list);

SgfLabelList *	 sgf_label_list_duplicate(const SgfLabelList *list);



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
  ((SgfErrorList *) string_list_new_derived(sizeof(SgfErrorListItem),	\
					    NULL))

#define sgf_error_list_init(list)					\
  string_list_init_derived((list), sizeof(SgfErrorListItem), NULL)

#define STATIC_SGF_ERROR_LIST						\
  STATIC_STRING_LIST_DERIVED(SgfErrorListItem, NULL)


#define sgf_error_list_get_item(list, item_index)			\
  ((SgfErrorListItem *) string_list_get_item((list), (item_index)))

#define sgf_error_list_find(list, text)					\
  ((SgfErrorListItem *) string_list_find((list), (text)))

#define sgf_error_list_find_after_notch(list, text, notch)		\
  ((SgfErrorListItem *)							\
   string_list_find_after_notch((list), (text) (notch)))


int		 sgf_parse_file(const char *filename,
				SgfCollection **collection,
				SgfErrorList **error_list,
				const SgfParserParameters *parameters,
				int *file_size, int *bytes_parsed,
				const int *cancellation_flag);
int		 sgf_parse_buffer(char *buffer, int size,
				  SgfCollection **collection,
				  SgfErrorList **error_list,
				  const SgfParserParameters *parameters,
				  int *bytes_parsed,
				  const int *cancellation_flag);


extern const SgfParserParameters	sgf_parser_defaults;



/* `sgf-writer.c' global functions. */

int		 sgf_write_file(const char *filename,
				SgfCollection *collection, int force_utf8);



/* `sgf-utils.c' global declarations and functions. */

typedef struct _SgfBoardState	SgfBoardState;

/* Fields must not be changed from outside `sgf-utils' module. */
struct _SgfBoardState {
  int		color_to_play;
  int		last_move_x;
  int		last_move_y;

  SgfNode      *game_info_node;
  SgfNode      *last_move_node;

  /* Will be NULL if in main variation. */
  SgfNode      *last_main_variation_node;

  /* Private fields.  Must be of no interest outside `sgf-utils'. */
  int		sgf_color_to_play;
};


typedef enum {
  SGF_NEXT,
  SGF_PREVIOUS
} SgfDirection;


inline void   sgf_utils_play_node_move(const SgfNode *node, Board *board);
int	      sgf_utils_format_node_move(const SgfGameTree *tree,
					 const SgfNode *node,
					 char *buffer,
					 const char *black_string,
					 const char *white_string,
					 const char *pass_string);

void	      sgf_utils_enter_tree(SgfGameTree *tree, Board *board,
				   SgfBoardState *board_state);

void	      sgf_utils_go_down_in_tree(SgfGameTree *tree, int num_nodes,
					SgfBoardState *board_state);
void	      sgf_utils_go_up_in_tree(SgfGameTree *tree, int num_nodes,
				      SgfBoardState *board_state);

int	      sgf_utils_count_variations(const SgfGameTree *tree,
					 int of_current_node,
					 int black_variations[BOARD_GRID_SIZE],
					 int white_variations[BOARD_GRID_SIZE],
					 int *other_variations);
SgfNode *     sgf_utils_find_variation_at_position(SgfGameTree *tree,
						   int x, int y,
						   SgfDirection direction,
						   int after_current);

void	      sgf_utils_switch_to_variation(SgfGameTree *tree,
					    SgfDirection direction,
					    SgfBoardState *board_state);
void	      sgf_utils_switch_to_given_variation(SgfGameTree *tree,
						  SgfNode *node,
						  SgfBoardState *board_state);

void	      sgf_utils_append_variation(SgfGameTree *tree,
					 SgfBoardState *board_state,
					 int color, ...);

void	      sgf_utils_get_markup(const SgfGameTree *tree,
				   char markup[BOARD_GRID_SIZE]);
void	      sgf_utils_mark_territory_on_grid(const SgfGameTree *tree,
					       char grid[BOARD_GRID_SIZE],
					       char black_territory_mark,
					       char white_territory_mark);

void	      sgf_utils_set_handicap(SgfGameTree *tree,
				     int handicap, int is_fixed);
void	      sgf_utils_add_free_handicap_stones
		(SgfGameTree *tree, BoardPositionList *handicap_stones);


char *	      sgf_utils_normalize_text(const char *text);



/* `sgf-diff-utils.c' global functions. */

SgfCollection *	 sgf_diff(const SgfCollection *from_collection,
			  const SgfCollection *to_collection);


#endif /* QUARRY_SGF_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
