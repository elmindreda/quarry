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


#include "gtk-games.h"

#include "gui-back-end.h"
#include "board.h"
#include "game-info.h"

#include <gtk/gtk.h>
#include <string.h>


const gchar *game_labels[NUM_SUPPORTED_GAMES] = {
  N_("_Go"),
  N_("_Amazons"),
  N_("_Reversi")
};

const gchar *game_rules_labels[NUM_SUPPORTED_GAMES] = {
  N_("Go Rules"),
  N_("Amazons Rules"),
  N_("Reversi Rules"),
};

const Game index_to_game[NUM_SUPPORTED_GAMES] = {
  GAME_GO, GAME_AMAZONS, GAME_REVERSI
};


const gchar *
gtk_games_get_capitalized_name (Game game)
{
  static gchar *capitalized_names[LAST_GAME + 1];
  static gboolean first_call = TRUE;

  g_return_val_if_fail (FIRST_GAME <= game && game <= LAST_GAME, NULL);

  if (first_call) {
    memset (capitalized_names, 0, sizeof capitalized_names);
    first_call = FALSE;
  }

  if (!capitalized_names[game]) {
    const gchar *game_name = _(game_info[game].name);
    gunichar first_character_capitalized
      = g_unichar_toupper (g_utf8_get_char (game_name));
    gchar *first_character_string
      = g_ucs4_to_utf8 (&first_character_capitalized, 1, NULL, NULL, NULL);

    capitalized_names[game] = g_strconcat (first_character_string,
					   g_utf8_next_char (game_name), NULL);
    gui_back_end_register_pointer_to_free (capitalized_names[game]);
  }

  return capitalized_names[game];
}


gboolean
gtk_games_engine_supports_game (GtpEngineListItem *engine_data,
				GtkGameIndex game_index)
{
  g_return_val_if_fail (0 <= game_index && game_index < NUM_SUPPORTED_GAMES,
			FALSE);

  return (!engine_data
	  || (string_list_find (&engine_data->supported_games,
				INDEX_TO_GAME_NAME (game_index))
	      != NULL));
}


gint
gtk_games_name_to_index (const gchar *game_name, gboolean case_sensitive)
{
  return gtk_games_get_game_index (game_from_game_name (game_name,
							case_sensitive));
}


GtkGameIndex
gtk_games_get_game_index (Game game)
{
  switch (game) {
  case GAME_GO:
    return GTK_GAME_GO;

  case GAME_AMAZONS:
    return GTK_GAME_AMAZONS;

  case GAME_REVERSI:
    return GTK_GAME_REVERSI;

  default:
    return GTK_GAME_UNSUPPORTED;
  }
}


GtkAdjustment *
gtk_games_create_board_size_adjustment (GtkGameIndex game_index,
					gint initial_value)
{
  g_return_val_if_fail (0 <= game_index && game_index < NUM_SUPPORTED_GAMES,
			NULL);

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

  case GTK_GAME_REVERSI:
    return ((GtkAdjustment *)
	    gtk_adjustment_new (initial_value,
				ROUND_UP (GTK_MIN_BOARD_SIZE, 2),
				ROUND_DOWN (GTK_MAX_BOARD_SIZE, 2),
				2, 4, 0));

  default:
    g_assert_not_reached ();
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
