/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005, 2006 Paul Pogonyshev.           *
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

/* This file contains run-time code that deals with configuration.  It
 * includes both configuration file parser and writer.  The parser is
 * implemented to be very forgiving.  For instance, it just ignores
 * lines it does not understand and allows random junk at line ends.
 */

/* FIXME: Clean it up, some functions have become overcomplicated. */


#include "configuration.h"
#include "utils.h"

#include <assert.h>
#include <string.h>


/* Cannot use is*() functions since we don't want to depend on the
 * current locale.
 */
#define IS_ACCEPTABLE_NAME_CHARACTER(character)				\
  (('A' <= (character) && (character) <= 'Z')				\
   || ('a' <= (character) && (character) <= 'z')			\
   || ('0' <= (character) && (character) <= '9')			\
   || (character) == '-' || (character) == '+' || (character) == '\'')


static const char *configuration_file_intro_comment =
  "# This is Quarry configuration file.\n#\n"
  "# You probably don't really want to edit it directly as you can tweak\n"
  "# all the settings from Quarry itself.  However, editing the file is\n"
  "# absolutely OK, just make sure that its general format is preserved.\n";


static char *	parse_string (char **line_scan, char fuzzy_terminator);
static int	parse_integer (const char *integer_string, int *result);

static void	write_section (BufferedWriter *writer,
			       const ConfigurationSection *section,
			       const void *section_structure,
			       const char *section_text_field);
static void	write_string (BufferedWriter *writer, const char *string);


void
configuration_init (const ConfigurationSection *sections, int num_sections)
{
  int k;

  assert (sections);

  for (k = 0; k < num_sections; k++, sections++) {
    if (!sections->is_repeatable && sections->section_structure_init)
      sections->section_structure_init (sections->section_structure);
  }
}


void
configuration_dispose (const ConfigurationSection *sections, int num_sections)
{
  int k;

  assert (sections);

  for (k = 0; k < num_sections; k++, sections++) {
    if (sections->section_structure_dispose)
      sections->section_structure_dispose (sections->section_structure);
  }
}


int
configuration_read_from_file (const ConfigurationSection *sections,
			      int num_sections, const char *filename)
{
  FILE *file = fopen (filename, "r");

  if (file) {
    const ConfigurationSection *current_section = NULL;
    void *current_section_structure = NULL;
    char *line;

    while ((line = utils_fgets (file, NULL)) != NULL) {
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

      if (IS_ACCEPTABLE_NAME_CHARACTER (*scan)) {
	const char *name = scan;
	char *name_pointer = scan;
	char name_terminating_character;

	while (1) {
	  do
	    *name_pointer++ = *scan++;
	  while (IS_ACCEPTABLE_NAME_CHARACTER (*scan));

	  while (*scan == ' ' || *scan == '\t')
	    scan++;

	  if (IS_ACCEPTABLE_NAME_CHARACTER (*scan))
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
	    if (strcasecmp (value->name, name) == 0)
	      break;
	  }

	  if (k < current_section->num_values) {
	    void *field = (((char *) current_section_structure)
			   + value->field_offset);

	    scan++;

	    if (value->type & VALUE_TYPE_IS_NULLABLE) {
	      const char *null_scan = scan;
	      int is_null = 0;

	      while (*null_scan == ' ' || *null_scan == '\t')
		null_scan++;

	      if (strncasecmp (null_scan, "null", 4) == 0) {
		null_scan += 4;
		while (*null_scan == ' ' || *null_scan == '\t')
		  null_scan++;

		if (!*null_scan || *null_scan == '\n' || *null_scan == '\r') {
		  /* FIXME: Not exactly right since we don't always
		   *	    assign a value below.
		   */
		  is_null = 1;
		}
	      }

	      /* FIXME: Not nice. */
	      * ((int *) (field) - 1) = is_null;
	    }

	    if (! (value->type & VALUE_TYPE_IS_NULLABLE
		   && * ((int *) (field) - 1))) {
	      if ((value->type & ~VALUE_TYPE_IS_NULLABLE)
		  != VALUE_TYPE_STRING_LIST) {
		char *string = parse_string (&scan, 0);

		if (string) {
		  if ((value->type & ~VALUE_TYPE_IS_NULLABLE)
		      == VALUE_TYPE_STRING) {
		    utils_free (* (char **) field);
		    * (char **) field = string;
		  }
		  else {
		    char *actual_contents = NULL;
		    char *whitespace_scan;
		    int enumeration_value = 0;

		    for (actual_contents = string;
			 *actual_contents == ' ' || *actual_contents == '\t';)
		      actual_contents++;

		    if ((value->type & ~VALUE_TYPE_IS_NULLABLE)
			== VALUE_TYPE_ENUMERATION) {
		      const char *value_string;
		      int actual_contents_length;

		      /* Trim trailing whitespace. */
		      for (whitespace_scan
			     = (actual_contents
				+ (strlen (actual_contents) - 1));
			   (whitespace_scan >= actual_contents
			    && (*whitespace_scan == ' '
				|| *whitespace_scan == '\t'));)
			whitespace_scan--;

		      *(whitespace_scan + 1) = 0;
		      actual_contents_length = ((whitespace_scan + 1)
						- actual_contents);

		      for (value_string = value->enumeration_values_as_strings,
			     enumeration_value = 0;
			   *value_string; enumeration_value++) {
			do {
			  int value_string_length = strlen (value_string);

			  if (actual_contents_length == value_string_length
			      && (strcasecmp (actual_contents, value_string)
				  == 0)) {
			    * (int *) field = enumeration_value;
			    goto enumeration_value_found;
			  }

			  value_string += value_string_length + 1;
			} while (*value_string);

			value_string++;
		      }
		    }

		    /* Find first whitespace character and break line
		     * at its position.
		     */
		    for (whitespace_scan = actual_contents;
			 (*whitespace_scan != ' ' && *whitespace_scan != '\t'
			  && *whitespace_scan != 0);)
		      whitespace_scan++;

		    *whitespace_scan = 0;

		    switch (value->type & ~VALUE_TYPE_IS_NULLABLE) {
		    case VALUE_TYPE_BOOLEAN:
		    case VALUE_TYPE_BOOLEAN_WRITE_TRUE_ONLY:
		      if (strcasecmp (actual_contents, "true") == 0
			  || strcasecmp (actual_contents, "yes") == 0
			  || strcmp (actual_contents, "1") == 0)
			* (int *) field = 1;
		      else if (strcasecmp (actual_contents, "false") == 0
			       || strcasecmp (actual_contents, "no") == 0
			       || strcmp (actual_contents, "0") == 0)
			* (int *) field = 0;

		      break;

		    case VALUE_TYPE_INT:
		      parse_integer (actual_contents, (int *) field);
		      break;

		    case VALUE_TYPE_ENUMERATION:
		      {
			int numeric_value;

			/* A second attempt, try to parse as numeric
			 * value.  Placed separately, because we have
			 * a different `actual_contents' here, more
			 * suited for numeric parsing.
			 */
			if (parse_integer (actual_contents, &numeric_value)
			    && 0 <= numeric_value
			    && numeric_value < enumeration_value)
			  * (int *) field = numeric_value;
		      }

		      break;

		    case VALUE_TYPE_REAL:
		      utils_parse_double (actual_contents, (double *) field);
		      break;

		    case VALUE_TYPE_COLOR:
		      {
			int num_digits;

			if (*actual_contents == '#')
			  actual_contents++;

			for (num_digits = 0;
			     ((('0' <= *actual_contents
				&& *actual_contents <= '9')
			       || ('a' <= *actual_contents
				   && *actual_contents <= 'f')
			       || ('A' <= *actual_contents
				   && *actual_contents <= 'F'))
			      && num_digits <= 6);
			     num_digits++)
			  actual_contents++;

			if (num_digits == 6 || num_digits == 3) {
			  int red;
			  int green;
			  int blue;

			  if (num_digits == 6) {
			    sscanf (actual_contents - 6, "%2x%2x%2x",
				    &red, &green, &blue);
			  }
			  else {
			    sscanf (actual_contents - 3, "%1x%1x%1x",
				    &red, &green, &blue);
			    red   *= 0x11;
			    green *= 0x11;
			    blue  *= 0x11;
			  }

			  ((QuarryColor *) field)->red   = red;
			  ((QuarryColor *) field)->green = green;
			  ((QuarryColor *) field)->blue  = blue;
			}
		      }

		      break;

		    case VALUE_TYPE_TIME:
		      {
			int seconds = utils_parse_time (actual_contents);

			if (seconds >= 0)
			  * (int *) field = seconds;
		      }

		      break;

		    default:
		      assert (0);
		    }

		  enumeration_value_found:

		    utils_free (string);
		  }
		}
	      }
	      else {
		int first_value = 1;

		do {
		  char *string = parse_string (&scan, ',');

		  if (string) {
		    if (first_value) {
		      string_list_empty (field);
		      first_value = 0;
		    }

		    string_list_add_ready (field, string);
		  }
		  else
		    break;
		} while (*scan++ == ',');
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
	    if (strcasecmp (section->name, name) == 0)
	      break;
	  }

	  if (k < num_sections) {
	    if (section->is_repeatable) {
	      char *section_name = NULL;

	      if (name_terminating_character == '"') {
		*scan = name_terminating_character;
		section_name = parse_string (&scan, '"');
	      }

	      if (*scan == ']') {
		string_list_add_ready (section->section_structure,
				       section_name);
		current_section = section;
		current_section_structure = ((StringList *)
					     section->section_structure)->last;

		if (section->section_structure_init)
		  section->section_structure_init (current_section_structure);
	      }
	    }
	    else if (!section->is_repeatable
		     && name_terminating_character == ']') {
	      current_section = section;
	      current_section_structure = section->section_structure;
	    }
	  }
	}
      }

      utils_free (line);
    }

    fclose (file);
    return 1;
  }

  return 0;
}


static char *
parse_string (char **line_scan, char fuzzy_terminator)
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
      while (**line_scan && **line_scan != fuzzy_terminator
	     && **line_scan != '#')
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

  return utils_duplicate_as_string (buffer, buffer_pointer - buffer);
}


int
parse_integer (const char *integer_string, int *result)
{
  /* FIXME: Can be improved. */
  const char *digit_scan = integer_string;

  if (*digit_scan == '+' || *digit_scan == '-')
    digit_scan++;

  if ('0' <= *integer_string && *integer_string <= '9') {
    *result = atoi (integer_string);
    return 1;
  }

  return 0;
}


int
configuration_write_to_file (const ConfigurationSection *sections,
			     int num_sections, const char *filename)
{
  BufferedWriter writer;

  if (buffered_writer_init (&writer, filename, 0x1000)) {
    int k;

    buffered_writer_cat_string (&writer, configuration_file_intro_comment);

    for (k = 0; k < num_sections; k++, sections++) {
      if (sections->is_repeatable) {
	StringList *abstract_list = (StringList *) sections->section_structure;
	StringListItem *abstract_item;

	for (abstract_item = abstract_list->first; abstract_item;
	     abstract_item = abstract_item->next) {
	  write_section (&writer, sections,
			 abstract_item, abstract_item->text);
	}
      }
      else
	write_section (&writer, sections, sections->section_structure, NULL);
    }

    return buffered_writer_dispose (&writer);
  }

  return 0;
}


static void
write_section (BufferedWriter *writer, const ConfigurationSection *section,
	       const void *section_structure, const char *section_text_field)
{
  const ConfigurationValue *value;
  int k;

  buffered_writer_cat_strings (writer, "\n[", section->name, NULL);

  if (section_text_field) {
    buffered_writer_add_character (writer, ' ');
    write_string (writer, section_text_field);
  }

  buffered_writer_cat_string (writer, "]\n");

  for (k = 0, value = section->values; k < section->num_values; k++, value++) {
    const void *field = (((const char *) section_structure)
			 + value->field_offset);
    ConfigurationValueType value_type = value->type;
    int is_null = 0;

    if (value_type & VALUE_TYPE_IS_NULLABLE) {
      /* FIXME: Not nice. */
      is_null	  = (* (((const int *) field) - 1));
      value_type &= ~VALUE_TYPE_IS_NULLABLE;
    }

    if (!is_null
	&& ((value_type == VALUE_TYPE_STRING
	     && ! * (char *const *) field)
	    || (value_type == VALUE_TYPE_STRING_LIST
		&& string_list_is_empty ((const StringList *) field))
	    || (value_type == VALUE_TYPE_BOOLEAN_WRITE_TRUE_ONLY
		&& ! * (const int *) field)))
      continue;

    buffered_writer_cat_string (writer, value->name);

    if (writer->column < 24) {
      do
	buffered_writer_add_character (writer, '\t');
      while (writer->column < 24);
    }
    else
      buffered_writer_add_character (writer, ' ');

    buffered_writer_cat_string (writer, "= ");

    if (is_null)
      buffered_writer_cat_string (writer, "null");
    else {
      switch (value_type) {
      case VALUE_TYPE_STRING:
	write_string (writer, * (char *const *) field);
	break;

      case VALUE_TYPE_STRING_LIST:
	{
	  StringListItem *item;

	  for (item = ((const StringList *) field)->first; item;
	       item = item->next) {
	    write_string (writer, item->text);
	    if (item->next)
	      buffered_writer_cat_string (writer, ", ");
	  }
	}

	break;

      case VALUE_TYPE_BOOLEAN:
      case VALUE_TYPE_BOOLEAN_WRITE_TRUE_ONLY:
	buffered_writer_cat_string (writer,
				    * (const int *) field ? "true" : "false");
	break;

      case VALUE_TYPE_INT:
	buffered_writer_cprintf (writer, "%d", * (const int *) field);
	break;

      case VALUE_TYPE_ENUMERATION:
	{
	  const char *value_string;
	  int enumeration_value;

	  for (value_string = value->enumeration_values_as_strings,
		 enumeration_value = * (const int *) field;
	       enumeration_value--; ) {
	    do
	      value_string += strlen (value_string) + 1;
	    while (*value_string);

	    value_string++;
	  }

	  write_string (writer, value_string);
	}

	break;

      case VALUE_TYPE_REAL:
	buffered_writer_cprintf (writer, "%.f", * (const double *) field);
	break;

      case VALUE_TYPE_COLOR:
	/* I believe this cannot be locale-dependent, right? */
	buffered_writer_printf (writer, "\"#%02x%02x%02x\"",
				((const QuarryColor *) field)->red,
				((const QuarryColor *) field)->green,
				((const QuarryColor *) field)->blue);
	break;

      case VALUE_TYPE_TIME:
	if (* (const int *) field < 60 * 60) {
	  buffered_writer_cprintf (writer, "%02d:%02d",
				   (* (const int *) field) / 60,
				   (* (const int *) field) % 60);
	}
	else {
	  buffered_writer_cprintf (writer, "%d:%02d:%02d",
				   (* (const int *) field) / (60 * 60),
				   ((* (const int *) field) / 60) % 60,
				   (* (const int *) field) % 60);
	}

	break;

      default:
	assert (0);
      }
    }

    buffered_writer_add_newline (writer);
  }
}


static void
write_string (BufferedWriter *writer, const char *string)
{
  buffered_writer_add_character (writer, '"');

  for (; *string; string++) {
    if (*string == '\n' || *string == '\r'
	|| *string == '"' || *string == '\\')
      buffered_writer_add_character (writer, '\\');

    if (*string != '\n' && *string != '\r')
      buffered_writer_add_character (writer, *string);
    else
      buffered_writer_add_character (writer, *string == '\n' ? 'n' : 'r');
  }

  buffered_writer_add_character (writer, '"');
}


void
configuration_combine_string_lists (void *main_configuration,
				    void *site_configuration,
				    int tag_field_offset)
{
  StringList *main_configuration_list = (StringList *) main_configuration;
  StringList *site_configuration_list = (StringList *) site_configuration;
  StringListItem *main_configuration_item;

  for (main_configuration_item = main_configuration_list->first;
       main_configuration_item; ) {
    /* Required since we may delete some items. */
    StringListItem *next_main_configuration_item
      = main_configuration_item->next;

    const char *tag = * ((const char **)
			 ((char *) main_configuration_item
			  + tag_field_offset));

    if (tag && !string_list_find (site_configuration_list, tag)) {
      string_list_delete_item (main_configuration_list,
			       main_configuration_item);
    }

    main_configuration_item = next_main_configuration_item;
  }

  while (!string_list_is_empty (site_configuration_list)) {
    StringListItem *site_configuration_item
      = string_list_steal_first_item (site_configuration_list);

    if (site_configuration_item->text) {
      for (main_configuration_item = main_configuration_list->first;
	   main_configuration_item;
	   main_configuration_item = main_configuration_item->next) {
	const char *tag = * ((const char **)
			     ((char *) main_configuration_item
			      + tag_field_offset));

	if (tag && strcmp (tag, site_configuration_item->text) == 0)
	  break;
      }

      if (!main_configuration_item) {
	* ((char **) ((char *) site_configuration_item + tag_field_offset))
	  = utils_duplicate_string (site_configuration_item->text);
	string_list_add_ready_item (main_configuration_list,
				    site_configuration_item);

	continue;
      }
    }

    string_list_dispose_item (site_configuration_list,
			      site_configuration_item);
  }
}


void
configuration_init_repeatable_section (const ConfigurationSection *section,
				       void *abstract_list_item)
{
  assert (section);
  assert (section->is_repeatable);
  assert (abstract_list_item);

  if (section->section_structure_init)
    section->section_structure_init (abstract_list_item);
}


void
configuration_set_string_value (char **configuration_variable,
				const char *string)
{
  assert (configuration_variable);

  utils_free (*configuration_variable);
  *configuration_variable = utils_duplicate_string (string);
}


void
configuration_set_string_list_value (StringList *configuration_variable,
				     const StringList *string_list)
{
  assert (configuration_variable);
  assert (string_list);

  string_list_empty (configuration_variable);
  string_list_duplicate_items (configuration_variable, string_list);
}


void
configuration_set_string_list_value_steal_strings
  (StringList *configuration_variable, StringList *string_list)
{
  assert (configuration_variable);
  assert (string_list);

  string_list_empty (configuration_variable);
  string_list_steal_items (configuration_variable, string_list);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
