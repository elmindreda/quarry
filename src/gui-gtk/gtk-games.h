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


#ifndef QUARRY_GTK_GAMES_H
#define QUARRY_GTK_GAMES_H


#include "gtk-configuration.h"
#include "game-info.h"
#include "quarry.h"

#include <gtk/gtk.h>


typedef enum {
  GTK_GAME_GO,
  GTK_GAME_AMAZONS,
  GTK_GAME_OTHELLO,

  NUM_SUPPORTED_GAMES
} GtkGameIndex;


extern const gchar  *game_labels[NUM_SUPPORTED_GAMES];
extern const Game    index_to_game[NUM_SUPPORTED_GAMES];


#define INDEX_TO_GAME_NAME(game_index)		\
  (game_info[index_to_game[game_index]].name)


gboolean	gtk_games_engine_supports_game(GtpEngineListItem *engine_data,
					       GtkGameIndex game_index);

gint		gtk_games_name_to_index(const gchar *game_name,
					gboolean case_sensitive);


#endif /* QUARRY_GTK_GAMES_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
