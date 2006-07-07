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


#ifndef QUARRY_QUARRY_FIND_DIALOG_H
#define QUARRY_QUARRY_FIND_DIALOG_H


#include "sgf.h"
#include "quarry.h"

#include <gtk/gtk.h>


#define QUARRY_TYPE_FIND_DIALOG	(quarry_find_dialog_get_type ())

#define QUARRY_FIND_DIALOG(object)					\
  GTK_CHECK_CAST ((object), QUARRY_TYPE_FIND_DIALOG, QuarryFindDialog)

#define QUARRY_FIND_DIALOG_CLASS(class)					\
  GTK_CHECK_CLASS_CAST ((class), QUARRY_TYPE_FIND_DIALOG,		\
			QuarryFindDialogClass)

#define QUARRY_IS_FIND_DIALOG(object)					\
  GTK_CHECK_TYPE ((object), QUARRY_TYPE_FIND_DIALOG)

#define QUARRY_IS_FIND_DIALOG_CLASS(class)				\
  GTK_CHECK_CLASS_TYPE ((class), QUARRY_TYPE_FIND_DIALOG)

#define QUARRY_FIND_DIALOG_GET_CLASS(object)				\
  GTK_CHECK_GET_CLASS ((object), QUARRY_TYPE_FIND_DIALOG,		\
		       QuarryFindDialogClass)


typedef struct _QuarryFindDialog	QuarryFindDialog;
typedef struct _QuarryFindDialogClass	QuarryFindDialogClass;

struct _QuarryFindDialog {
  GtkDialog	    dialog;

  GtkEntry	   *search_for_entry;
  GtkToggleButton  *case_sensitive_toggle_button;
  GtkToggleButton  *wrap_around_toggle_button;
  GtkToggleButton  *whole_words_only_toggle_button;
  GtkToggleButton  *close_automatically_toggle_button;
  GtkToggleButton  *search_current_node_only_toggle_button;
  GtkToggleButton  *search_scope_toggle_buttons[3];
};

struct _QuarryFindDialogClass {
  GtkDialogClass    parent_class;
};


typedef enum {
  QUARRY_FIND_DIALOG_FIND_NEXT = 1,
  QUARRY_FIND_DIALOG_FIND_PREVIOUS
} QuarryFindDialogSearchDirection;


typedef void (* QuarryFindDialogSwitchToGivenNode) (void *user_data,
						    SgfNode *sgf_node);


GType		quarry_find_dialog_get_type (void);

GtkWidget *	quarry_find_dialog_new (const gchar *title);


const gchar *	quarry_find_dialog_get_text_to_find (QuarryFindDialog *dialog);

gboolean	quarry_find_dialog_get_case_sensitive
		  (QuarryFindDialog *dialog);
gboolean	quarry_find_dialog_get_whole_words_only
		  (QuarryFindDialog *dialog);
gboolean	quarry_find_dialog_get_wrap_around
		  (QuarryFindDialog *dialog);
gboolean	quarry_find_dialog_get_search_whole_game_tree
		  (QuarryFindDialog *dialog);
gint		quarry_find_dialog_get_search_in
		  (QuarryFindDialog *dialog);
gboolean	quarry_find_dialog_get_close_automatically
		  (QuarryFindDialog *dialog);

void		quarry_find_dialog_set_text_to_find
		  (QuarryFindDialog *dialog, const gchar *text_to_find);
void		quarry_find_dialog_set_search_history
		  (QuarryFindDialog *dialog, const StringList *strings);

void		quarry_find_dialog_set_case_sensitive
		  (QuarryFindDialog *dialog, gboolean case_sensitive);
void		quarry_find_dialog_set_whole_words_only
		  (QuarryFindDialog *dialog, gboolean whole_words_only);
void		quarry_find_dialog_set_wrap_around
		  (QuarryFindDialog *dialog, gboolean wrap_around);
void		quarry_find_dialog_set_search_whole_game_tree
		  (QuarryFindDialog *dialog, gboolean search_whole_game_tree);
void		quarry_find_dialog_set_search_in
		  (QuarryFindDialog *dialog, gint search_in);
void		quarry_find_dialog_set_close_automatically
		  (QuarryFindDialog *dialog, gboolean close_automatically);


gboolean	quarry_find_text
		  (const gchar *text_to_find,
		   QuarryFindDialogSearchDirection direction,
		   gboolean case_sensitive,
		   gboolean whole_words_only,
		   gboolean wrap_around,
		   gboolean search_whole_game_tree,
		   gint search_in,
		   GtkTextBuffer *text_buffer,
		   gboolean node_name_inserted,
		   SgfGameTree *sgf_tree,
		   QuarryFindDialogSwitchToGivenNode switch_to_given_node,
		   void *user_data);


#endif /* QUARRY_QUARRY_SAVE_CONFIRMATION_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
