/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2005, 2006 Paul Pogonyshev.                       *
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


#ifndef QUARRY_GTK_GO_TO_NAMED_NODE_DIALOG_H
#define QUARRY_GTK_GO_TO_NAMED_NODE_DIALOG_H


#include "gtk-utils.h"


#if GTK_2_4_OR_LATER


#include "sgf.h"

#include <gtk/gtk.h>


#define GTK_TYPE_GO_TO_NAMED_NODE_DIALOG				\
  (gtk_go_to_named_node_dialog_get_type ())

#define GTK_GO_TO_NAMED_NODE_DIALOG(object)				\
  GTK_CHECK_CAST ((object), GTK_TYPE_GO_TO_NAMED_NODE_DIALOG,		\
		  GtkGoToNamedNodeDialog)

#define GTK_GO_TO_NAMED_NODE_DIALOG_CLASS(class)			\
  GTK_CHECK_CLASS_CAST ((class), GTK_TYPE_GO_TO_NAMED_NODE_DIALOG,	\
			GtkGoToNamedNodeDialogClass)

#define GTK_IS_GO_TO_NAMED_NODE_DIALOG(object)				\
  GTK_CHECK_TYPE ((object), GTK_TYPE_GO_TO_NAMED_NODE_DIALOG)

#define GTK_IS_GO_TO_NAMED_NODE_DIALOG_CLASS(class)			\
  GTK_CHECK_CLASS_TYPE ((class), GTK_TYPE_GO_TO_NAMED_NODE_DIALOG)

#define GTK_GO_TO_NAMED_NODE_DIALOG_GET_CLASS(object)			\
  GTK_CHECK_GET_CLASS ((object), GTK_TYPE_GO_TO_NAMED_NODE_DIALOG,	\
		       GtkGoToNamedNodeDialogClass)


typedef struct _GtkGoToNamedNodeDialog		GtkGoToNamedNodeDialog;
typedef struct _GtkGoToNamedNodeDialogClass	GtkGoToNamedNodeDialogClass;

struct _GtkGoToNamedNodeDialog {
  GtkDialog	       dialog;

  GtkEntryCompletion  *entry_completion;
  GTree		      *completion_tree;
  gulong	       entry_changed_handler_id;

  GtkTextBuffer	      *comment_buffer;
  GtkWidget	      *comment_widgets;

  SgfNode	      *selected_node;
};

struct _GtkGoToNamedNodeDialogClass {
  GtkDialogClass       parent_class;
};


GType		gtk_go_to_named_node_dialog_get_type (void);

GtkWidget *	gtk_go_to_named_node_dialog_new (SgfGameTree *sgf_tree);


#endif /* GTK_2_4_OR_LATER */


#endif /* QUARRY_GTK_GO_TO_NAMED_NODE_DIALOG_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
