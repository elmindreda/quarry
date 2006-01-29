/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005, 2006 Paul Pogonyshev.           *
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


#ifndef QUARRY_GTK_GAMES_H
#define QUARRY_GTK_GAMES_H


#include "gtk-configuration.h"
#include "gtp-client.h"
#include "board.h"
#include "game-info.h"
#include "quarry.h"

#include <gtk/gtk.h>


/* These two macros don't exactly concern games, but there seem to be
 * no better place.  Define minimal and maximal board sizes so that
 * there is no chance to conflict with any of the modules.
 */
#define GTK_MIN_BOARD_SIZE				\
  MAX (MAX (BOARD_MIN_WIDTH, BOARD_MIN_HEIGHT),		\
       MAX (SGF_MIN_BOARD_SIZE, GTP_MIN_BOARD_SIZE))

#define GTK_MAX_BOARD_SIZE				\
  MIN (MIN (BOARD_MAX_WIDTH, BOARD_MAX_HEIGHT),		\
       MIN (SGF_MAX_BOARD_SIZE, GTP_MAX_BOARD_SIZE))


typedef enum {
  GTK_GAME_GO,
  GTK_GAME_AMAZONS,
  GTK_GAME_REVERSI,

  NUM_SUPPORTED_GAMES,
  GTK_GAME_UNSUPPORTED = NUM_SUPPORTED_GAMES,
  GTK_GAME_ANY	       = NUM_SUPPORTED_GAMES
} GtkGameIndex;


extern const gchar  *game_labels[NUM_SUPPORTED_GAMES];
extern const gchar  *game_rules_labels[NUM_SUPPORTED_GAMES];
extern const Game    index_to_game[NUM_SUPPORTED_GAMES];


#define INDEX_TO_GAME_NAME(game_index)		\
  (game_info[index_to_game[game_index]].name)


const gchar *	 gtk_games_get_capitalized_name (Game game);

gboolean	 gtk_games_engine_supports_game
		   (GtpEngineListItem *engine_data, GtkGameIndex game_index);

gint		 gtk_games_name_to_index (const gchar *game_name,
					  gboolean case_sensitive);

GtkGameIndex	 gtk_games_get_game_index (Game game);

GtkAdjustment *	 gtk_games_create_board_size_adjustment
		   (GtkGameIndex game_index, gint initial_value);
GtkAdjustment *	 gtk_games_create_handicap_adjustment (gint initial_value);
GtkAdjustment *	 gtk_games_create_komi_adjustmet (gdouble initial_value);

void		 gtk_games_set_handicap_adjustment_limits
		   (gint board_width, gint board_height,
		    GtkAdjustment *fixed_handicap_adjustment,
		    GtkAdjustment *free_handicap_adjustment);


#endif /* QUARRY_GTK_GAMES_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
