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


#ifndef QUARRY_BOARD_H
#define QUARRY_BOARD_H


#include "board-topology.h"
#include "games.h"
#include "quarry.h"

#include <stdarg.h>



#define EMPTY			0
#define FIRST_COLOR		1
#define BLACK			1
#define WHITE			2
#define NUM_COLORS		2

/* NOTE: padding for game-specific on-grid values (like arrows for
 *	 Amazons).
 */
#define SPECIAL_ON_GRID_VALUE	3

#define NUM_ON_GRID_VALUES	4
#define OFF_GRID		NUM_ON_GRID_VALUES
#define NUM_VALID_BOARD_VALUES	5

#define COLOR_INDEX(color)	((color) - FIRST_COLOR)
#define BLACK_INDEX		COLOR_INDEX(BLACK)
#define WHITE_INDEX		COLOR_INDEX(WHITE)

/* "Stone" is Go terminology. */
#define IS_STONE(color)		((color) == BLACK || (color) == WHITE)

#define OTHER_COLOR(color)	(BLACK + WHITE - (color))
#define OTHER_INDEX(index)	(BLACK_INDEX + WHITE_INDEX - (index))


typedef struct _BoardPoint	BoardPoint;

struct _BoardPoint {
  char		x;
  char		y;
};



/* Go-specific definitions. */

#define GO_STRING_RING_SIZE	(BOARD_MAX_POSITIONS			\
				 + BOARD_MAX_WIDTH + BOARD_MAX_HEIGHT)

#define PASS_X			NULL_X
#define PASS_Y			NULL_Y
#define PASS_MOVE		NULL_POSITION
#define IS_PASS(x, y)		IS_NULL_POINT((x), (y))


typedef struct _GoBoardData	GoBoardData;

struct _GoBoardData{
  int		ko_master;
  int		ko_position;
  int		prisoners[NUM_COLORS];

  int		last_string_number;
  int		string_number[BOARD_GRID_SIZE];
  int		liberties[GO_STRING_RING_SIZE];

  unsigned int	position_mark;
  unsigned int	string_mark;
  unsigned int	marked_positions[BOARD_GRID_SIZE];
  unsigned int	marked_strings[GO_STRING_RING_SIZE];
};



/* Amazons-specific definition. */

#define ARROW			SPECIAL_ON_GRID_VALUE


typedef struct _BoardAmazonsMoveData	BoardAmazonsMoveData;
typedef union _BoardAbstractMoveData	BoardAbstractMoveData;

struct _BoardAmazonsMoveData {
  BoardPoint		 from;
  BoardPoint		 shoot_arrow_to;
};

union _BoardAbstractMoveData {
  BoardAmazonsMoveData	 amazons;
};



typedef enum {
  FIRST_RULE_SET,
  RULE_SET_SGF = FIRST_RULE_SET,
  RULE_SET_DEFAULT,

  GO_RULE_SET_SGF = RULE_SET_SGF,
  GO_RULE_SET_DEFAULT = RULE_SET_DEFAULT,
  NUM_GO_RULE_SETS,

  OTHELLO_RULE_SET_SGF = RULE_SET_SGF,
  OTHELLO_RULE_SET_DEFAULT = RULE_SET_DEFAULT,
  NUM_OTHELLO_RULE_SETS,

  AMAZONS_RULE_SET_SGF = RULE_SET_SGF,
  AMAZONS_RULE_SET_DEFAULT = RULE_SET_DEFAULT,
  NUM_AMAZONS_RULE_SETS
} BoardRuleSet;


typedef struct _BoardChangeStackEntry	BoardChangeStackEntry;
typedef struct _Board			Board;

typedef int (* BoardIsLegalMoveFunction) (const Board *board,
					  BoardRuleSet rule_set,
					  int color, va_list move);

typedef void (* BoardPlayMoveFunction) (Board *board, int color, va_list move);
typedef void (* BoardUndoFunction) (Board *board);

struct _Board {
  Game			     game;
  int			     width;
  int			     height;

  unsigned int		     move_number;

  char			     grid[BOARD_FULL_GRID_SIZE];

  void			    *move_stack;
  void			    *move_stack_pointer;
  void			    *move_stack_end;

  BoardChangeStackEntry	    *change_stack;
  BoardChangeStackEntry	    *change_stack_pointer;
  BoardChangeStackEntry	    *change_stack_end;

  BoardIsLegalMoveFunction   is_legal_move;
  BoardPlayMoveFunction	     play_move;
  BoardUndoFunction	     undo;

  union {
    GoBoardData		     go;
  } data;
};


typedef struct _BoardPositionList	BoardPositionList;

struct _BoardPositionList {
  int		num_positions;
  int		positions[BOARD_MAX_POSITIONS];
};


Board *		board_new(Game game, int width, int height);
void		board_delete(Board *board);

Board *		board_duplicate_without_stacks(const Board *board);

void		board_set_parameters(Board *board, Game game,
				     int width, int height);
#define board_clear(board)						\
  board_set_parameters((board), (board)->game,				\
		       (board)->width, (board)->height)

int		board_adjust_color_to_play(const Board *board,
					   BoardRuleSet rule_set, int color);
int		board_is_game_over(const Board *board, BoardRuleSet rule_set,
				   int color_to_play);

inline int	board_is_legal_move(const Board *board, BoardRuleSet rule_set,
				    int color, ...);

inline void	board_play_move(Board *board, int color, ...);
void		board_apply_changes
		  (Board *board,
		   const BoardPositionList *const
		     change_lists[NUM_ON_GRID_VALUES]);
inline void	board_add_dummy_move_entry(Board *board);

inline int	board_undo(Board *board, int num_moves);


inline void	board_dump(const Board *board);
inline void	board_validate(const Board *board);


BoardPositionList *  board_position_list_new(const int *positions,
					     int num_positions);
BoardPositionList *  board_position_list_new_empty(int num_positions);

#define board_position_list_delete(position_list)			\
  do {									\
    assert(position_list);						\
    utils_free(position_list);						\
  } while (0)

#define board_position_list_duplicate(position_list)			\
  ((BoardPositionList *)						\
   utils_duplicate_buffer((position_list),				\
			  (sizeof(BoardPositionList)			\
			   - ((BOARD_MAX_POSITIONS -			\
			       (position_list)->num_positions)		\
			      * sizeof(int)))))

BoardPositionList *  board_position_list_union(BoardPositionList *list1,
					       BoardPositionList *list2);

void		     board_position_list_sort(BoardPositionList *list);

int		     board_position_lists_are_equal
		       (const BoardPositionList *first_list,
			const BoardPositionList *second_list);
int		     board_position_lists_overlap
		       (const BoardPositionList *first_list,
			const BoardPositionList *second_list);
int		     board_position_list_find_position
		       (const BoardPositionList *list, int pos);

void		     board_position_list_mark_on_grid
		       (const BoardPositionList *list,
			char grid[BOARD_GRID_SIZE], char value);


#define SUGGESTED_POSITION_LIST_BUFFER_SIZE	(4 * BOARD_MAX_POSITIONS)


int		game_format_point(Game game, int board_width, int board_height,
				  char *buffer, int x, int y);
int		game_format_position_list
		  (Game game, int board_width, int board_height,
		   char *buffer, const BoardPositionList *position_list);
int		game_format_move(Game game, int board_width, int board_height,
				 char *buffer, ...);
int		game_format_move_valist(Game game,
					int board_width, int board_height,
					char *buffer, va_list move);

int		game_parse_point(Game game, int board_width, int board_height,
				 const char *point_string, int *x, int *y);
BoardPositionList *
		game_parse_position_list(Game game,
					 int board_width, int board_height,
					 const char *positions_string);
int		game_parse_move(Game game, int board_width, int board_height,
				const char *move_string,
				int *x, int *y,
				BoardAbstractMoveData *move_data);


inline int	game_get_default_setup(Game game, int width, int height,
				       BoardPositionList **black_stones,
				       BoardPositionList **white_stones);


void		board_fill_grid(Board *board, char grid[BOARD_GRID_SIZE],
				char value);
void		board_fill_int_grid(Board *board, int grid[BOARD_GRID_SIZE],
				    int value);
#define board_fill_uint_grid(board, grid, value)	\
  board_fill_int_grid((board), (int *) grid, value)



/* Go-specific functions. */

int		     go_get_max_fixed_handicap(int board_width,
					       int board_height);
BoardPositionList *  go_get_fixed_handicap_stones(int board_width,
						  int board_height,
						  int num_stones);

BoardPositionList *  go_get_string_stones(Board *board, int x, int y);
BoardPositionList *  go_get_logically_dead_stones(Board *board, int x, int y);

void		     go_score_game(Board *board, const char *dead_stones,
				   double komi,
				   double *score, char **detailed_score,
				   BoardPositionList **black_territory,
				   BoardPositionList **white_territory);
void		     go_mark_territory_on_grid(Board *board, char *grid,
					       const char *dead_stones,
					       char black_territory_mark,
					       char white_territory_mark);



/* Othello-specific function. */
void		     othello_count_disks(const Board *board,
					 int *num_black_disks,
					 int *num_white_disks);


#endif /* QUARRY_BOARD_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
