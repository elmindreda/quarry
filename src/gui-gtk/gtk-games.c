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


#include "gtk-games.h"
#include "game-info.h"

#include <assert.h>
#include <gtk/gtk.h>
#include <string.h>


const gchar *game_labels[NUM_SUPPORTED_GAMES] = {
  "_Go", "_Amazons", "_Othello"
};

const Game index_to_game[NUM_SUPPORTED_GAMES] = {
  GAME_GO, GAME_AMAZONS, GAME_OTHELLO
};


gboolean
gtk_games_engine_supports_game(GtpEngineListItem *engine_data,
			       GtkGameIndex game_index)
{
  assert(0 <= game_index && game_index < NUM_SUPPORTED_GAMES);

  return (!engine_data
	  || (string_list_find(&engine_data->supported_games,
			       INDEX_TO_GAME_NAME(game_index))
	      != NULL));
}


gint
gtk_games_name_to_index(const gchar *game_name, gboolean case_sensitive)
{
  int game_index;

  if (game_name) {
    for (game_index = 0; game_index < NUM_SUPPORTED_GAMES; game_index++) {
      const char *this_name = game_info[index_to_game[game_index]].name;

      if (case_sensitive
	  ? strcmp(game_name, this_name) == 0
	  : strcasecmp(game_name, this_name) == 0)
	return game_index;
    }
  }

  return -1;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
