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


#ifndef QUARRY_SGF_WRITER_H
#define QUARRY_SGF_WRITER_H


#include "sgf.h"
#include "utils.h"
#include "quarry.h"

#include <iconv.h>
#include <stdio.h>


#define FILL_COLUMN		72
#define FILL_BREAK_POINT	(FILL_COLUMN - 15)


typedef struct _SgfWritingData	SgfWritingData;

struct _SgfWritingData {
  BufferedWriter   writer;

  SgfGameTree	  *tree;
  iconv_t	   utf8_to_tree_encoding;
  void (* do_write_move) (SgfWritingData *data, SgfNode *node);
};


#define DECLARE_VALUE_WRITER(name)				\
  void		name (SgfWritingData *data, SgfValue value)


DECLARE_VALUE_WRITER (sgf_write_none);
DECLARE_VALUE_WRITER (sgf_write_number);
DECLARE_VALUE_WRITER (sgf_write_real);
DECLARE_VALUE_WRITER (sgf_write_double);
DECLARE_VALUE_WRITER (sgf_write_color);
DECLARE_VALUE_WRITER (sgf_write_simple_text);
DECLARE_VALUE_WRITER (sgf_write_fake_simple_text);
DECLARE_VALUE_WRITER (sgf_write_text);

DECLARE_VALUE_WRITER (sgf_write_list_of_point);
DECLARE_VALUE_WRITER (sgf_write_list_of_vector);
DECLARE_VALUE_WRITER (sgf_write_list_of_label);

DECLARE_VALUE_WRITER (sgf_write_figure_description);

DECLARE_VALUE_WRITER (sgf_write_unknown);


#endif /* QUARRY_SGF_WRITER_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
