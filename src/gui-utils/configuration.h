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
 * Software Foundation, Inc., 59 Temple Place - Suite 330,         *
 * Boston, MA 02111-1307, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef QUARRY_CONFIGURATION_H
#define QUARRY_CONFIGURATION_H


#include "utils.h"
#include "quarry.h"


typedef enum {
  VALUE_TYPE_INVALID = -1,
  VALUE_TYPE_STRING  =	0,
  VALUE_TYPE_STRING_LIST,
  VALUE_TYPE_BOOLEAN,
  VALUE_TYPE_BOOLEAN_WRITE_TRUE_ONLY,
  VALUE_TYPE_INT,
  VALUE_TYPE_ENUMERATION,
  VALUE_TYPE_REAL,
  VALUE_TYPE_COLOR,
  VALUE_TYPE_TIME,
  NUM_VALUE_TYPES
} ConfigurationValueType;


typedef struct _ConfigurationSection	ConfigurationSection;
typedef struct _ConfigurationValue	ConfigurationValue;

struct _ConfigurationSection {
  const char		    *name;
  int			     is_repeatable;

  void			    *section_structure;

  void (* section_structure_init)    (void *section_structure);
  void (* section_structure_dispose) (void *section_structure);

  const ConfigurationValue  *values;
  int			     num_values;
};

struct _ConfigurationValue {
  const char		    *name;
  ConfigurationValueType     type;
  int			     field_offset;

  /* Used only for `VALUE_TYPE_ENUMERATION'. */
  const char		    *enumeration_values_as_strings;
};


void		configuration_init (const ConfigurationSection *sections,
				    int num_sections);
void		configuration_dispose (const ConfigurationSection *sections,
				       int num_sections);

int		configuration_read_from_file
		  (const ConfigurationSection *sections, int num_sections,
		   const char *filename);
int		configuration_write_to_file
		  (const ConfigurationSection *sections, int num_sections,
		   const char *filename);


void		configuration_combine_string_lists
		  (void *main_configuration, void *site_configuration,
		   int tag_field_offset);


void		configuration_init_repeatable_section
		  (const ConfigurationSection *section,
		   void *abstract_list_item);

void		configuration_set_string_value (char **configuration_variable,
						const char *string);
void		configuration_set_string_list_value
		  (StringList *configuration_variable,
		   const StringList *string_list);
void		configuration_set_string_list_value_steal_strings
		  (StringList *configuration_variable,
		   StringList *string_list);


#endif /* QUARRY_CONFIGURATION_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
