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

/* This file contains run-time code that deals with configuration.  It
 * includes both configuration file parser and writer.  The parser is
 * implemented to be very forgiving.  For instance, it just ignores
 * lines it does not understand and allows random junk at line ends.
 */


#include "configuration.h"
#include "utils.h"

#include <string.h>
#include <assert.h>


static const char *configuration_file_intro_comment =
  "# This is Quarry configuration file.\n#\n"
  "# You probably don't really want to edit it directly as you can tweak\n"
  "# all the settings from Quarry itself.  However, editing the file is\n"
  "# absolutely OK, just make sure that its general format is preserved.\n";


static char *	parse_string(char **line_scan, char fuzzy_terminator);

static void	write_section(BufferedWriter *writer,
			      const ConfigurationSection *section,
			      const void *section_structure,
			      const char *section_text_field);
static void	write_string(BufferedWriter *writer, const char *string);


void
configuration_init(const ConfigurationSection *sections, int num_sections)
{
  int k;

  assert(sections);

  for (k = 0; k < num_sections; k++, sections++) {
    if (!sections->is_repeatable && sections->section_structure_init)
      sections->section_structure_init(sections->section_structure);
  }
}


void
configuration_dispose(const ConfigurationSection *sections, int num_sections)
{
  int k;

  assert(sections);

  for (k = 0; k < num_sections; k++, sections++) {
    if (sections->section_structure_dispose)
      sections->section_structure_dispose(sections->section_structure);
  }
}


int
configuration_read_from_file(const ConfigurationSection *sections,
			     int num_sections, const char *filename)
{
  FILE *file = fopen(filename, "r");

  if (file) {
    const ConfigurationSection *current_section = NULL;
    void *current_section_structure = NULL;
    char *line;

    while ((line = utils_fgets(file, NULL)) != NULL) {
      char *scan;
      int parsing_section_name = 0;

      for (scan = line; *scan == ' ' || *scan == '\t';)
	scan++;

      if (*scan == '[') {
	do
	  scan++;
	while (*scan == ' ' || *scan == '\t');

	parsing_section_name = 1;
      }

      if (('A' <= *scan && *scan <= 'Z') || ('a' <= *scan && *scan <= 'z')) {
	const char *name = scan;
	char *name_pointer = scan;
	char name_terminating_character;

	while (1) {
	  do
	    *name_pointer++ = *scan++;
	  while (('A' <= *scan && *scan <= 'Z')
		 || ('a' <= *scan && *scan <= 'z'));

	  while (*scan == ' ' || *scan == '\t')
	    scan++;

	  if (('A' <= *scan && *scan <= 'Z') || ('a' <= *scan && *scan <= 'z'))
	    *name_pointer++ = ' ';
	  else
	    break;
	}

	name_terminating_character = *scan;
	*name_pointer = 0;

	if (!parsing_section_name
	    && name_terminating_character == '='
	    && current_section) {
	  const ConfigurationValue *value;
	  int k;

	  for (k = 0, value = current_section->values;
	       k < current_section->num_values; k++, value++) {
	    if (strcasecmp(value->name, name) == 0)
	      break;
	  }

	  if (k < current_section->num_values) {
	    void *field = (((char *) current_section_structure)
			   + value->field_offset);
	    char *string;

	    scan++;

	    if (value->type != VALUE_TYPE_STRING_LIST)
	      string = parse_string(&scan, 0);

	    if (value->type == VALUE_TYPE_STRING_LIST || string) {
	      switch (value->type) {
	      case VALUE_TYPE_STRING:
		utils_free(* (char **) field);
		* (char **) field = string;
		break;

	      case VALUE_TYPE_STRING_LIST:
		{
		  int first_value = 1;

		  do {
		    string = parse_string(&scan, ',');

		    if (string) {
		      if (first_value) {
			string_list_empty(field);
			first_value = 0;
		      }

		      string_list_add_ready(field, string);
		    }
		    else
		      break;
		  } while (*scan++ == ',');
		}

		break;

	      case VALUE_TYPE_BOOLEAN:
		if (strcasecmp(string, "true") == 0
		    || strcasecmp(string, "yes") == 0
		    || strcmp(string, "1") == 0)
		  * (int *) field = 1;
		else if (strcasecmp(string, "false") == 0
			 || strcasecmp(string, "no") == 0
			 || strcmp(string, "0") == 0)
		  * (int *) field = 0;

		utils_free(string);
		break;

	      case VALUE_TYPE_INT:
	      case VALUE_TYPE_REAL:
		{
		  const char *contents = string;
		  const char *digit_scan;

		  while (*contents == ' ' || *contents == '\t')
		    contents++;

		  digit_scan = contents;
		  if (*digit_scan == '+' || *digit_scan == '-')
		    digit_scan++;

		  if (('0' <= *digit_scan && *digit_scan <= '9')
		      || (value->type == VALUE_TYPE_REAL
			  && *digit_scan == '.')) {
		    if (value->type == VALUE_TYPE_INT)
		      * (int *) field = atoi(contents);
		    else
		      * (double *) field = atof(contents);
		  }
		}

		utils_free(string);
		break;

	      default:
		assert(0);
	      }
	    }
	  }
	}
	else if (parsing_section_name
		 && (name_terminating_character == ']'
		     || name_terminating_character == '"')) {
	  int k;
	  const ConfigurationSection *section;

	  current_section = NULL;

	  for (k = 0, section = sections; k < num_sections; k++, section++) {
	    if (strcasecmp(section->name, name) == 0)
	      break;
	  }

	  if (k < num_sections) {
	    if (section->is_repeatable) {
	      char *section_name = NULL;

	      if (name_terminating_character == '"') {
		*scan = name_terminating_character;
		section_name = parse_string(&scan, '"');
	      }

	      if (*scan == ']') {
		string_list_add_ready(section->section_structure,
				      section_name);
		current_section = section;
		current_section_structure = ((StringList *)
					     section->section_structure)->last;

		if (section->section_structure_init)
		  section->section_structure_init(current_section_structure);
	      }
	    }
	    else if (!section->is_repeatable && name_terminating_character == ']') {
	      current_section = section;
	      current_section_structure = section->section_structure;
	    }
	  }
	}
      }

      utils_free(line);
    }
  }

  return 0;
}


static char *
parse_string(char **line_scan, char fuzzy_terminator)
{
  char *buffer = *line_scan;
  char *buffer_pointer = buffer;

  while (**line_scan == ' ' || **line_scan == '\t')
    (*line_scan)++;

  if (**line_scan == '"') {
    while (* ++(*line_scan) != '"') {
      if (**line_scan == '\\') {
	(*line_scan)++;

	if (**line_scan == 'n' || **line_scan == '\r') {
	  *buffer_pointer++ = (**line_scan == 'n' ? '\n' : '\r');
	  continue;
	}
      }

      if (! **line_scan)
	return NULL;

      *buffer_pointer++ = **line_scan;
    }

    (*line_scan)++;
  }
  else {
    if (fuzzy_terminator != '"') {
      while (**line_scan && **line_scan != fuzzy_terminator)
	*buffer_pointer++ = * (*line_scan)++;

      while (buffer_pointer > buffer
	     && (*(buffer_pointer - 1) == ' '
		  || *(buffer_pointer - 1) == '\t'
		  || *(buffer_pointer - 1) == '\n'
		  || *(buffer_pointer - 1) == '\r'))
	buffer_pointer--;
    }
    else
      return NULL;
  }

  while (**line_scan == ' ' || **line_scan == '\t')
    (*line_scan)++;

  return utils_duplicate_as_string(buffer, buffer_pointer - buffer);
}


int
configuration_write_to_file(const ConfigurationSection *sections,
			    int num_sections, const char *filename)
{
  BufferedWriter writer;

  if (buffered_writer_init(&writer, filename, 0x1000)) {
    int k;

    buffered_writer_cat_string(&writer, configuration_file_intro_comment);

    for (k = 0; k < num_sections; k++, sections++) {
      if (sections->is_repeatable) {
	StringList *abstract_list = (StringList *) sections->section_structure;
	StringListItem *abstract_item;

	for (abstract_item = abstract_list->first; abstract_item;
	     abstract_item = abstract_item->next)
	  write_section(&writer, sections, abstract_item, abstract_item->text);
      }
      else
	write_section(&writer, sections, sections->section_structure, NULL);
    }

    return buffered_writer_dispose(&writer);
  }

  return 0;
}


static void
write_section(BufferedWriter *writer, const ConfigurationSection *section,
	      const void *section_structure, const char *section_text_field)
{
  const ConfigurationValue *value;
  int k;

  buffered_writer_cat_strings(writer, "\n[", section->name, NULL);

  if (section_text_field) {
    buffered_writer_add_character(writer, ' ');
    write_string(writer, section_text_field);
  }

  buffered_writer_cat_string(writer, "]\n");

  for (k = 0, value = section->values; k < section->num_values; k++, value++) {
    const void *field = (((const char *) section_structure)
			 + value->field_offset);

    if ((value->type == VALUE_TYPE_STRING && ! * (char *const *) field)
	|| (value->type == VALUE_TYPE_STRING_LIST
	    && string_list_is_empty(field)))
      continue;

    buffered_writer_cat_string(writer, value->name);

    if (writer->column < 16) {
      buffered_writer_add_character(writer, '\t');
      if (writer->column < 16)
	buffered_writer_add_character(writer, '\t');
    }
    else
      buffered_writer_add_character(writer, ' ');

    buffered_writer_cat_string(writer, "= ");

    switch (value->type) {
    case VALUE_TYPE_STRING:
      write_string(writer, * (char *const *) field);
      break;

    case VALUE_TYPE_STRING_LIST:
      {
	StringListItem *item;

	for (item = ((const StringList *) field)->first; item;
	     item = item->next) {
	  write_string(writer, item->text);
	  if (item->next)
	    buffered_writer_cat_string(writer, ", ");
	}
      }

      break;

    case VALUE_TYPE_BOOLEAN:
      buffered_writer_cat_string(writer,
				 * (const int *) field ? "true" : "false");
      break;

    case VALUE_TYPE_INT:
      buffered_writer_printf(writer, "%d", * (const int *) field);
      break;

    case VALUE_TYPE_REAL:
      buffered_writer_cat_string(writer,
				 format_double(* (const double *) field));
      break;

    default:
      assert(0);
    }

    buffered_writer_add_newline(writer);
  }
}


static void
write_string(BufferedWriter *writer, const char *string)
{
  buffered_writer_add_character(writer, '"');

  for (; *string; string++) {
    if (*string == '\n' || *string == '\r'
	|| *string == '"' || *string == '\\')
      buffered_writer_add_character(writer, '\\');

    if (*string != '\n' && *string != '\r')
      buffered_writer_add_character(writer, *string);
    else
      buffered_writer_add_character(writer, *string == '\n' ? 'n' : 'r');
  }

  buffered_writer_add_character(writer, '"');
}


void
configuration_init_repeatable_section(const ConfigurationSection *section,
				      void *abstract_list_item)
{
  assert(section);
  assert(section->is_repeatable);
  assert(abstract_list_item);

  if (section->section_structure_init)
    section->section_structure_init(abstract_list_item);
}


void
configuration_set_section_values(const ConfigurationSection *section, ...)
{
  void *section_structure;
  int value_index;
  va_list arguments;

  assert(section);

  va_start(arguments, section);

  if (section->is_repeatable)
    section_structure = va_arg(arguments, void *);
  else
    section_structure = section->section_structure;

  while ((value_index = va_arg(arguments, int)) != -1) {
    const ConfigurationValue *value;
    void *field;

    assert(0 <= value_index && value_index <= section->num_values);

    value = section->values + value_index;
    field = ((char *) section_structure) + value->field_offset;

    switch (value->type) {
    case VALUE_TYPE_STRING:
      {
	const char *string = va_arg(arguments, const char *);

	utils_free(* (char **) field);
	* (char **) field = (string ? utils_duplicate_string(string) : NULL);
      }

      break;

    case VALUE_TYPE_STRING_LIST:
      {
	StringList *string_list = va_arg(arguments, StringList *);

	string_list_empty(field);

	if (va_arg(arguments, int))
	  string_list_steal_items(field, string_list);
	else
	  string_list_duplicate_items(field, string_list);
      }

      break;

    case VALUE_TYPE_BOOLEAN:
    case VALUE_TYPE_INT:
      * (int *) field = va_arg(arguments, int);
      break;

    case VALUE_TYPE_REAL:
      * (double *) field = va_arg(arguments, double);
      break;

    default:
      assert(0);
    }
  }

  va_end(arguments);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
