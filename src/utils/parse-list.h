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


#ifndef QUARRY_PARSE_LIST_H
#define QUARRY_PARSE_LIST_H


#include "utils.h"
#include "quarry.h"

#include <stdio.h>


#define TABBING(tab_position, from)				\
  ((tab_position) > (from) / 8					\
   ? tab_string + 12 - ((tab_position) - ((from) / 8)) : " ")


typedef enum {
  SORT_FIRST,
  SORT_NORMAL,
  SORT_LAST,

  NUM_LIST_SORT_ORDERS  
} ListSortOrder;


typedef struct _ListDescription		ListDescription;
typedef struct _ListDescriptionSet	ListDescriptionSet;

struct _ListDescription {
  const char		   *name;
  int			    multiple_lists_allowed;
  ListSortOrder		    sort_order;

  int			    enumeration_required;
  const char		   *c_file_array_type;

  int (* list_initializer) (StringBuffer *c_file_array,
			    const char *h_file_enum_name,
			    const char *c_file_array_name);
  int (* line_parser1)	   (char **line);
  int (* line_parser2)	   (StringBuffer *c_file_array,
			    char **line,
			    const char *identifier,
			    char **pending_eol_comment,
			    int *pending_linefeeds);
  int (* list_finalizer)   (StringBuffer *c_file_array);
};

struct _ListDescriptionSet {
  const char		 *command_line_name;
  const ListDescription	 *lists;
};


typedef enum {
  IDENTIFIER,
  PROPERTY_IDENTIFIER,
  FIELD_NAME,
  INTEGER_NUMBER,
  FLOATING_POINT_NUMBER,
  STRING,
  STRING_OR_NULL,
  STRING_OR_IDENTIFIER,
} Thing;


int		parse_list_main(int argc, char *argv[],
				const ListDescriptionSet *list_sets,
				int num_sets);

void		print_error(const char *format_string, ...);

char *		read_line(void);

const char *	parse_thing(Thing thing, char **line, const char *type);
char *		parse_multiline_string(char **line, const char *type,
				       const char *line_separator,
				       int null_allowed);

#define PARSE_THING(store_in, thing, line, type)		\
  do {								\
    (store_in) = parse_thing((thing), (line), (type));		\
    if (!(store_in))						\
      return 1;							\
  } while (0)

#define PARSE_IDENTIFIER(store_in, line, type)	\
  PARSE_THING((store_in), IDENTIFIER, (line), (type))


int		looking_at(const char *what, char **line);


extern StringBuffer   h_file_top;
extern StringBuffer   h_file_bottom;

extern StringBuffer   c_file_top;
extern StringBuffer   c_file_bottom;


extern const char    *tab_string;


#endif /* QUARRY_PARSE_LIST_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
