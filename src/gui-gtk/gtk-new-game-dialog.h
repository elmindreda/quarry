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


#ifndef QUARRY_GTK_NEW_GAME_DIALOG_H
#define QUARRY_GTK_NEW_GAME_DIALOG_H


#include "gtk-assistant.h"
#include "gtk-games.h"
#include "board.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_NEW_GAME_DIALOG	(gtk_new_game_dialog_get_type ())

#define GTK_NEW_GAME_DIALOG(obj)					\
  GTK_CHECK_CAST ((obj), GTK_TYPE_NEW_GAME_DIALOG, GtkNewGameDialog)
#define GTK_NEW_GAME_DIALOG_CLASS(klass)				\
  GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_NEW_GAME_DIALOG,		\
			GtkNewGameDialogClass)

#define GTK_IS_NEW_GAME_DIALOG(obj)					\
  GTK_CHECK_TYPE ((obj), GTK_TYPE_NEW_GAME_DIALOG)
#define GTK_IS_NEW_GAME_DIALOG_CLASS(klass)				\
  GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_NEW_GAME_DIALOG)

#define GTK_NEW_GAME_DIALOG_GET_CLASS(obj)				\
  GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_NEW_GAME_DIALOG,			\
		       GtkNewGameDialogClass)


typedef struct _NewGameDialogTimeControlData	NewGameDialogTimeControlData;

typedef struct _GtkNewGameDialog		GtkNewGameDialog;
typedef struct _GtkNewGameDialogClass		GtkNewGameDialogClass;


struct _NewGameDialogTimeControlData {
  GtkNotebook	     *notebook;
  GtkToggleButton    *track_total_time_button;

  GtkAdjustment      *game_time_limit;
  GtkAdjustment      *move_time_limit;
  GtkAdjustment	     *main_time;
  GtkAdjustment	     *overtime_period;
  GtkAdjustment	     *moves_per_overtime;
};

struct _GtkNewGameDialog {
  GtkAssistant	      assistant;

  GtkToggleButton    *game_radio_buttons[NUM_SUPPORTED_GAMES];
  GtkWidget	     *game_supported_icons[NUM_SUPPORTED_GAMES];

  GtkToggleButton    *player_radio_buttons[NUM_COLORS][2];
  GtkEntry	     *human_name_entries[NUM_COLORS];
  GtkWidget	     *engine_selectors[NUM_COLORS];

  GtkNotebook	     *games_notebook;

  GtkAdjustment	     *board_sizes[NUM_SUPPORTED_GAMES];
  GtkToggleButton    *handicap_toggle_buttons[2];
  GtkAdjustment	     *handicaps[2];
  GtkAdjustment	     *komi;

  NewGameDialogTimeControlData  time_control_data[NUM_SUPPORTED_GAMES];

  GtpClient	     *players[NUM_COLORS];
};

struct _GtkNewGameDialogClass {
  GtkAssistantClass   parent_class;
};


GType		gtk_new_game_dialog_get_type (void);

void		gtk_new_game_dialog_present (void);


#endif /* QUARRY_GTK_NEW_GAME_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
