/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005 Paul Pogonyshev.                             *
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


#ifndef QUARRY_GTK_NEW_GAME_RECORD_DIALOG_H
#define QUARRY_GTK_NEW_GAME_RECORD_DIALOG_H


#include "gtk-freezable-spin-button.h"
#include "gtk-games.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_NEW_GAME_RECORD_DIALOG					\
  (gtk_new_game_record_dialog_get_type ())

#define GTK_NEW_GAME_RECORD_DIALOG(obj)					\
  GTK_CHECK_CAST ((obj), GTK_TYPE_NEW_GAME_RECORD_DIALOG,		\
		  GtkNewGameRecordDialog)
#define GTK_NEW_GAME_RECORD_DIALOG_CLASS(klass)				\
  GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_NEW_GAME_RECORD_DIALOG,	\
			GtkNewGameRecordDialogClass)

#define GTK_IS_NEW_GAME_RECORD_DIALOG(obj)				\
  GTK_CHECK_TYPE ((obj), GTK_TYPE_NEW_GAME_RECORD_DIALOG)
#define GTK_IS_NEW_GAME_RECORD_DIALOG_CLASS(klass)			\
  GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_NEW_GAME_RECORD_DIALOG)

#define GTK_NEW_GAME_RECORD_DIALOG_GET_CLASS(obj)			\
  GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_NEW_GAME_RECORD_DIALOG,		\
		       GtkNewGameRecordDialogClass)


typedef struct _GtkNewGameRecordDialog		GtkNewGameRecordDialog;
typedef struct _GtkNewGameRecordDialogClass	GtkNewGameRecordDialogClass;


struct _GtkNewGameRecordDialog {
  GtkDialog		   dialog;

  GSList		  *game_radio_button_group;

  GtkAdjustment		  *board_sizes[NUM_SUPPORTED_GAMES];

  GtkFreezableSpinButton  *handicap_spin_button;
  GtkToggleButton	  *place_stones;
  GtkFreezableSpinButton  *komi_spin_button;

  GtkEntry		  *white_player;
  GtkEntry		  *black_player;
  GtkEntry		  *game_name;
};

struct _GtkNewGameRecordDialogClass {
  GtkDialogClass   parent_class;
};


GType		gtk_new_game_record_dialog_get_type (void);

void		gtk_new_game_record_dialog_present (void);


#endif /* QUARRY_GTK_NEW_GAME_RECORD_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
