/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005 Paul Pogonyshev.                 *
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


#include "gtk-games.h"

#include "board.h"
#include "game-info.h"

#include <assert.h>
#include <gtk/gtk.h>
#include <string.h>


const gchar *game_labels[NUM_SUPPORTED_GAMES] = {
  N_("_Go"),
  N_("_Amazons"),
  N_("_Othello")
};

const gchar *game_rules_labels[NUM_SUPPORTED_GAMES] = {
  N_("Go Rules"),
  N_("Amazons Rules"),
  N_("Othello Rules"),
};

const Game index_to_game[NUM_SUPPORTED_GAMES] = {
  GAME_GO, GAME_AMAZONS, GAME_OTHELLO
};


gboolean
gtk_games_engine_supports_game (GtpEngineListItem *engine_data,
				GtkGameIndex game_index)
{
  assert (0 <= game_index && game_index < NUM_SUPPORTED_GAMES);

  return (!engine_data
	  || (string_list_find (&engine_data->supported_games,
				INDEX_TO_GAME_NAME (game_index))
	      != NULL));
}


gint
gtk_games_name_to_index (const gchar *game_name, gboolean case_sensitive)
{
  int game_index;

  if (game_name) {
    for (game_index = 0; game_index < NUM_SUPPORTED_GAMES; game_index++) {
      const char *this_name = game_info[index_to_game[game_index]].name;

      if (case_sensitive
	  ? strcmp (game_name, this_name) == 0
	  : strcasecmp (game_name, this_name) == 0)
	return game_index;
    }
  }

  return -1;
}


GtkGameIndex
gtk_games_get_game_index (Game game)
{
  switch (game) {
  case GAME_GO:
    return GTK_GAME_GO;

  case GAME_AMAZONS:
    return GTK_GAME_AMAZONS;

  case GAME_OTHELLO:
    return GTK_GAME_OTHELLO;

  default:
    return GTK_GAME_UNSUPPORTED;
  }
}


GtkAdjustment *
gtk_games_create_board_size_adjustment (GtkGameIndex game_index,
					gint initial_value)
{
  assert (0 <= game_index && game_index < NUM_SUPPORTED_GAMES);

  if (initial_value <= 0)
    initial_value = game_info[index_to_game[game_index]].default_board_size;

  switch (game_index) {
  case GTK_GAME_GO:
    return ((GtkAdjustment *)
	    gtk_adjustment_new (initial_value,
				GTK_MIN_BOARD_SIZE, GTK_MAX_BOARD_SIZE,
				1, 2, 0));

  case GTK_GAME_AMAZONS:
    return ((GtkAdjustment *)
	    gtk_adjustment_new (initial_value,
				GTK_MIN_BOARD_SIZE, GTK_MAX_BOARD_SIZE,
				1, 2, 0));

  case GTK_GAME_OTHELLO:
    return ((GtkAdjustment *)
	    gtk_adjustment_new (initial_value,
				ROUND_UP (GTK_MIN_BOARD_SIZE, 2),
				ROUND_DOWN (GTK_MAX_BOARD_SIZE, 2),
				2, 4, 0));

  default:
    assert (0);
  }
}


GtkAdjustment *
gtk_games_create_handicap_adjustment (gint initial_value)
{
  return ((GtkAdjustment *)
	  gtk_adjustment_new (initial_value,
			      0, GTK_MAX_BOARD_SIZE * GTK_MAX_BOARD_SIZE,
			      1, 2, 0));
}


GtkAdjustment *
gtk_games_create_komi_adjustmet (gdouble initial_value)
{
  return ((GtkAdjustment *)
	  gtk_adjustment_new (initial_value, -999.5, 999.5, 1.0, 5.0, 0.0));
}


void
gtk_games_set_handicap_adjustment_limits
  (gint board_width, gint board_height,
   GtkAdjustment *fixed_handicap_adjustment,
   GtkAdjustment *free_handicap_adjustment)
{
  if (fixed_handicap_adjustment) {
    int max_fixed_handicap = go_get_max_fixed_handicap (board_width,
							board_height);

    fixed_handicap_adjustment->upper = (gdouble) max_fixed_handicap;
    gtk_adjustment_changed (fixed_handicap_adjustment);
  }

  if (free_handicap_adjustment) {
    free_handicap_adjustment->upper = ((gdouble)
				       (board_width * board_height - 1));
    gtk_adjustment_changed (free_handicap_adjustment);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
