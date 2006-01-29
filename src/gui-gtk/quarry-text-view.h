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


#ifndef QUARRY_QUARRY_TEXT_VIEW_H
#define QUARRY_QUARRY_TEXT_VIEW_H


#include "quarry.h"

#include <gtk/gtk.h>


#define QUARRY_TYPE_TEXT_VIEW	(quarry_text_view_get_type ())

#define QUARRY_TEXT_VIEW(object)					\
  GTK_CHECK_CAST ((object), QUARRY_TYPE_TEXT_VIEW, QuarryTextView)

#define QUARRY_TEXT_VIEW_CLASS(class)					\
  GTK_CHECK_CLASS_CAST ((class), QUARRY_TYPE_TEXT_VIEW,			\
			QuarryTextViewClass)

#define QUARRY_IS_TEXT_VIEW(object)					\
  GTK_CHECK_TYPE ((object), QUARRY_TYPE_TEXT_VIEW)

#define QUARRY_IS_TEXT_VIEW_CLASS(class)				\
  GTK_CHECK_CLASS_TYPE ((class), QUARRY_TYPE_TEXT_VIEW)

#define QUARRY_TEXT_VIEW_GET_CLASS(object)				\
  GTK_CHECK_GET_CLASS ((object), QUARRY_TYPE_TEXT_VIEW,			\
		       QuarryTextViewClass)


typedef struct _QuarryTextView		QuarryTextView;
typedef struct _QuarryTextViewClass	QuarryTextViewClass;

struct _QuarryTextView {
  GtkTextView	    text_view;
};

struct _QuarryTextViewClass {
  GtkTextViewClass  parent_class;

  void (* undo) (QuarryTextView *view);
  void (* redo) (QuarryTextView *view);
};


GType		quarry_text_view_get_type (void);

GtkWidget *	quarry_text_view_new (void);
GtkWidget *	quarry_text_view_new_with_buffer (GtkTextBuffer *buffer);

void		quarry_text_view_undo (QuarryTextView *view);
void		quarry_text_view_redo (QuarryTextView *view);


#endif /* QUARRY_QUARRY_TEXT_VIEW_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
