/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004, 2005, 2006 Paul Pogonyshev.                 *
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


#ifndef QUARRY_GTK_GAME_INFO_DIALOG_H
#define QUARRY_GTK_GAME_INFO_DIALOG_H


#include "gtk-freezable-spin-button.h"
#include "sgf.h"
#include "board.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define GTK_TYPE_GAME_INFO_DIALOG					\
  (gtk_game_info_dialog_get_type ())

#define GTK_GAME_INFO_DIALOG(object)					\
  GTK_CHECK_CAST ((object), GTK_TYPE_GAME_INFO_DIALOG,			\
		  GtkGameInfoDialog)

#define GTK_GAME_INFO_DIALOG_CLASS(class)				\
  GTK_CHECK_CLASS_CAST ((class), GTK_TYPE_GAME_INFO_DIALOG,		\
			GtkGameInfoDialogClass)

#define GTK_IS_GAME_INFO_DIALOG(object)					\
  GTK_CHECK_TYPE ((object), GTK_TYPE_GAME_INFO_DIALOG)

#define GTK_IS_GAME_INFO_DIALOG_CLASS(class)				\
  GTK_CHECK_CLASS_TYPE ((class), GTK_TYPE_GAME_INFO_DIALOG)

#define GTK_GAME_INFO_DIALOG_GET_CLASS(object)				\
  GTK_CHECK_GET_CLASS ((object), GTK_TYPE_GAME_INFO_DIALOG,		\
		       GtkGameInfoDialogClass)


typedef struct _GtkGameInfoDialog	GtkGameInfoDialog;
typedef struct _GtkGameInfoDialogClass	GtkGameInfoDialogClass;

struct _GtkGameInfoDialog {
  GtkDialog		   dialog;

  SgfGameTree		  *sgf_tree;
  SgfNode		  *sgf_node;
  SgfUndoHistory	  *sgf_undo_history;

  GtkNotebook		  *pages;

  GtkEntry		  *player_names[NUM_COLORS];
  GtkEntry		  *player_teams[NUM_COLORS];
  GtkEntry		  *player_ranks[NUM_COLORS];

  GtkEntry		  *game_name;
  GtkEntry		  *place;
  GtkEntry		  *date;
  GtkEntry		  *event;
  GtkEntry		  *round;

  GtkEntry		  *rule_set;
  GtkAdjustment		  *handicap;
  GtkFreezableSpinButton  *handicap_spin_button;
  GtkAdjustment		  *komi;
  GtkFreezableSpinButton  *komi_spin_button;
  GtkAdjustment		  *main_time;
  GtkFreezableSpinButton  *main_time_spin_button;
  GtkEntry		  *overtime;

  GtkWidget		  *handicap_box;
  GtkWidget		  *komi_box;

  GtkEntry		  *result;
  GtkEntry		  *opening;
  GtkTextBuffer		  *game_comment;
  GtkWidget		  *game_comment_text_view;

  GtkEntry		  *copyright;
  GtkEntry		  *annotator;
  GtkEntry		  *source;
  GtkEntry		  *user;

  void *		   simple_undo_field;
  void *		   simple_redo_field;
  gchar *		   simple_redo_value;

  SgfType		   modified_property_type;
};

struct _GtkGameInfoDialogClass {
  GtkDialogClass	   parent_class;

  void (* property_changed) (GtkGameInfoDialog *game_info_dialog,
			     SgfType sgf_property_type);
};


GType		gtk_game_info_dialog_get_type (void);

GtkWidget *	gtk_game_info_dialog_new (void);

void		gtk_game_info_dialog_set_node (GtkGameInfoDialog *dialog,
					       SgfGameTree *sgf_tree,
					       SgfNode *sgf_node);


#endif /* QUARRY_GTK_GAME_INFO_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
