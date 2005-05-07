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
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,     *
 * Boston, MA 02110-1301, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef QUARRY_SGF_PRIVATES_H
#define QUARRY_SGF_PRIVATES_H


#include "sgf.h"
#include "sgf-properties.h"
#include "sgf-parser.h"
#include "sgf-writer.h"
#include "sgf-errors.h"
#include "quarry.h"


typedef struct _SgfPropertyInfo	SgfPropertyInfo;

struct _SgfPropertyInfo {
#if !SGF_LONG_NAMES
  char		   name[4];
#else
  char		  *name;
#endif

  SgfValueType	   value_type;

  SgfError (* value_parser) (SgfParsingData *data);
  void (* value_writer)	    (SgfWritingData *data, SgfValue value);
};


/* Information about each property type. */
extern const SgfPropertyInfo	property_info[];


#endif /* QUARRY_SGF_PRIVATES_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
