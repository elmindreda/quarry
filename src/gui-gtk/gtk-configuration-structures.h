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


#ifndef QUARRY_GTK_CONFIGURATION_STRUCTURES_H
#define QUARRY_GTK_CONFIGURATION_STRUCTURES_H


#include "board.h"
#include "quarry.h"


typedef struct _NewGameConfiguration	NewGameConfiguration;

struct _NewGameConfiguration {
  char	       *game_name;

  char	       *player_names[NUM_COLORS];
  int		player_is_computer[NUM_COLORS];
  char	       *engine_names[NUM_COLORS];
};


void		new_game_configuration_dispose(void *section_structure);


#endif /* QUARRY_GTK_CONFIGURATION_STRUCTURES_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
