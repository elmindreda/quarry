/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2006 Paul Pogonyshev.                             *
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


#ifndef QUARRY_QUARRY_MOVE_NUMBER_DIALOG_H
#define QUARRY_QUARRY_MOVE_NUMBER_DIALOG_H


#include "quarry.h"

#include <gtk/gtk.h>
#include <stdarg.h>


#define QUARRY_TYPE_MOVE_NUMBER_DIALOG					\
  (quarry_move_number_dialog_get_type ())

#define QUARRY_MOVE_NUMBER_DIALOG(object)				\
  GTK_CHECK_CAST ((object), QUARRY_TYPE_MOVE_NUMBER_DIALOG,		\
		  QuarryMoveNumberDialog)

#define QUARRY_MOVE_NUMBER_DIALOG_CLASS(class)				\
  GTK_CHECK_CLASS_CAST ((class), QUARRY_TYPE_MOVE_NUMBER_DIALOG,	\
			QuarryMoveNumberDialogClass)

#define QUARRY_IS_MOVE_NUMBER_DIALOG(object)				\
  GTK_CHECK_TYPE ((object), QUARRY_TYPE_MOVE_NUMBER_DIALOG)

#define QUARRY_IS_MOVE_NUMBER_DIALOG_CLASS(class)			\
  GTK_CHECK_CLASS_TYPE ((class), QUARRY_TYPE_MOVE_NUMBER_DIALOG)

#define QUARRY_MOVE_NUMBER__DIALOG_GET_CLASS(object)			\
  GTK_CHECK_GET_CLASS ((object), QUARRY_TYPE_MOVE_NUMBER_DIALOG,	\
		       QuarryMoveNumberDialogClass)


typedef struct _QuarryMoveNumberDialog		QuarryMoveNumberDialog;
typedef struct _QuarryMoveNumberDialogClass	QuarryMoveNumberDialogClass;

struct _QuarryMoveNumberDialog {
  GtkDialog	    dialog;

  GtkToggleButton  *toggle_buttons[2];
  GtkAdjustment	   *move_number_adjustment;

  gint		    sequential_move_number;
};

struct _QuarryMoveNumberDialogClass {
  GtkDialogClass    parent_class;
};


GType		quarry_move_number_dialog_get_type (void);

GtkWidget *	quarry_move_number_dialog_new (void);

gboolean	quarry_move_number_dialog_get_use_sequential_move_number
		  (const QuarryMoveNumberDialog *dialog);
gint		quarry_move_number_dialog_get_specific_move_number
		  (const QuarryMoveNumberDialog *dialog);

void		quarry_move_number_dialog_set_sequential_move_number
		  (QuarryMoveNumberDialog *dialog, gint move_number);
void		quarry_move_number_dialog_set_specific_move_number
		  (QuarryMoveNumberDialog *dialog, gint move_number);
void		quarry_move_number_dialog_set_use_sequential_move_number
		  (QuarryMoveNumberDialog *dialog,
		   gboolean use_sequential_move_number);


#endif /* QUARRY_QUARRY_MOVE_NUMBER_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
