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


#include "configuration.h"
#include "parse-list.h"
#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>


#define VALUE_TYPE_STRUCTURE	NUM_VALUE_TYPES


typedef struct _SectionData		SectionData;

struct _SectionData {
  const char    *section_name;

  int		 is_repeatable;
  int		 is_new_structure;

  const char    *structure_type;
  const char    *structure_name;
  const char	*dispose_function_name;
  const char    *static_list_macro;
  const char    *list_text_field_name;
  const char    *list_function_prefix;
};


typedef struct _SectionArrayListItem	SectionArrayListItem;
typedef struct _SectionArrayList	SectionArrayList;

struct _SectionArrayListItem {
  SectionArrayListItem	 *next;
  char			 *array;

  SectionData		  section_data;
};

struct _SectionArrayList {
  SectionArrayListItem	 *first;
  SectionArrayListItem	 *last;

  int			  item_size;
  StringListItemDispose	  item_dispose;
};


#define section_array_list_new()					\
  string_list_new_derived(sizeof(SectionArrayListItem), NULL)

#define section_array_list_init(list)					\
  string_list_init_derived((list), sizeof(SectionArrayListItem), NULL)

#define STATIC_SECTION_ARRAY_LIST					\
  STATIC_STRING_LIST_DERIVED(SectionArrayListItem, NULL)


#define section_array_list_get_item(list, item_index)			\
  ((SectionArrayListItem *) string_list_get_item((list), (item_index)))

#define section_array_list_find(list, array)				\
  ((SectionArrayListItem *) string_list_find((list), (array)))

#define section_array_list_find_after_notch(list, array, notch)		\
  ((SectionArrayListItem *)						\
   string_list_find_after_notch((list), (array), (notch)))


typedef struct _SubscriptionStackItem	SubscriptionStackItem;

struct _SubscriptionStackItem {
  SubscriptionStackItem	  *previous;

  const char		  *field_name_part;
  int			   is_array_subscription;

  ConfigurationValueType   array_element_type;
  int			   num_array_dimensions;

  int			   declaring_new_structure;
  StringBuffer		   structure_declaration;
  StringBuffer		   dispose_function;
  const char		  *structure_type;
  const char		  *dispose_function_name;
};


static int	configuration_initialize_sections
		  (StringBuffer *c_file_arrays,
		   const char *h_file_enum_name,
		   const char *c_file_array_name);
static int	configuration_parse_sections2(StringBuffer *c_file_arrays,
					      char **line,
					      const char *identifier,
					      char **pending_eol_comment,
					      int *pending_linefeeds);

static int	configuration_initialize_values
		  (StringBuffer *c_file_arrays,
		   const char *h_file_enum_name,
		   const char *c_file_array_name);
static int	configuration_parse_values2(StringBuffer *c_file_arrays,
					    char **line,
					    const char *identifier,
					    char **pending_eol_comment,
					    int *pending_linefeeds);

static int	configuration_initialize_defaults
		  (StringBuffer *c_file_arrays,
		   const char *h_file_enum_name,
		   const char *c_file_array_name);
static int	configuration_parse_defaults2(StringBuffer *c_file_arrays,
					      char **line,
					      const char *identifier,
					      char **pending_eol_comment,
					      int *pending_linefeeds);
static int	configuration_finalize_defaults(StringBuffer *c_file_arrays);

static int	configuration_finalize(StringBuffer *c_file_arrays);


static int	section_datum_equal(const SectionData *first_data,
				    const SectionData *second_data);

static void	push_subscription_stack(int is_array_subscription,
					int is_new_structure,
					int do_start_structure_declaration,
					const char *structure_type,
					const char *dispose_function_name);
static SubscriptionStackItem *
		pop_subscription_stack
		  (int do_write_dispose_function_prototype);
static void	recursively_build_full_field_name
		  (const SubscriptionStackItem *stack_item,
		   char **full_field_name);

static void	add_field_declaration_if_needed
		  (SubscriptionStackItem *stack_item,
		   ConfigurationValueType field_type,
		   const char *field_name, const char *structure_type,
		   const char *array_dimensions);
static void	add_field_dispose_call_if_needed
		  (SubscriptionStackItem *stack_item,
		   const char *dispose_field_function,
		   const char *dispose_field_prefix,
		   const char *field_name);

static void	open_initialization_braces(int num_levels);
static void	close_initialization_braces(int num_levels);
static void	add_initialization_indentation(int indentation_level);

static void	finish_function(StringBuffer *prototypes_buffer,
				const char *function_name,
				const char *function_name_suffix,
				int function_defined,
				int *last_function_defined);


static const ListDescription configuration_lists[] = {
  { "section_list", 0, SORT_LAST, 1, "const ConfigurationSection ",
    configuration_initialize_sections, NULL, configuration_parse_sections2,
    NULL },
  { "values", 1, SORT_NORMAL, 0, "static const ConfigurationValue ",
    configuration_initialize_values, NULL, configuration_parse_values2, NULL },
  { "defaults", 1, SORT_FIRST, 0, "",
    configuration_initialize_defaults, NULL, configuration_parse_defaults2,
    configuration_finalize_defaults },
  { NULL, 0, SORT_NORMAL, 0, NULL, NULL, NULL, NULL, configuration_finalize }
};

const ListDescriptionSet list_set = { NULL, configuration_lists };


static SectionArrayList	       value_arrays = STATIC_SECTION_ARRAY_LIST;
static SectionArrayList	       defaults_lists = STATIC_SECTION_ARRAY_LIST;

static const SectionData      *current_values_data;
static const SectionData      *current_defaults_data;

static const char	      *current_defaults_list;

static StringBuffer	       initialization_values;
static StringBuffer	       dispose_function_prototypes;
static StringBuffer	       dispose_functions;

static SubscriptionStackItem  *stack_pointer = NULL;
static StringList	       declared_structures = STATIC_STRING_LIST;
static StringList	       empty_dispose_functions = STATIC_STRING_LIST;

static int		       any_fields_to_init;

static const char	      *last_array_index;
static int		       current_subscription_level;


int
main(int argc, char *argv[])
{
  int result;

  string_buffer_init(&initialization_values, 0x400, 0x100);
  string_buffer_init(&dispose_function_prototypes, 0x400, 0x100);
  string_buffer_init(&dispose_functions, 0x1000, 0x400);

  result = parse_list_main(argc, argv, &list_set, 1);

  string_list_empty(&value_arrays);
  string_list_empty(&defaults_lists);
  string_list_empty(&declared_structures);
  string_list_empty(&empty_dispose_functions);

  string_buffer_dispose(&initialization_values);
  string_buffer_dispose(&dispose_function_prototypes);
  string_buffer_dispose(&dispose_functions);

  return result;
}


static int
configuration_initialize_sections(StringBuffer *c_file_arrays,
				  const char *h_file_enum_name,
				  const char *c_file_array_name)

{
  UNUSED(c_file_arrays);
  UNUSED(h_file_enum_name);

  string_buffer_cat_strings(&h_file_bottom,
			    "\n\nextern const ConfigurationSection\t",
			    c_file_array_name, "[];\n\n", NULL);
  return 0;
}


static int
configuration_parse_sections2(StringBuffer *c_file_arrays,
			      char **line, const char *identifier,
			      char **pending_eol_comment,
			      int *pending_linefeeds)
{
  SectionData section_data
    = { NULL, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL };
  const char *section_values_array;
  const char *section_defaults_list;
  SectionArrayListItem *existing_section_array;

  UNUSED(identifier);
  UNUSED(pending_eol_comment);
  UNUSED(pending_linefeeds);

  PARSE_THING(section_data.section_name, STRING, line, "section name");

  if (looking_at("repeatable", line))
    section_data.is_repeatable = 1;
  else if (looking_at("single", line))
    section_data.is_repeatable = 0;
  else {
    print_error("section type (`single' or `repeatable') expected");
    return 1;
  }

  if (looking_at("new", line))
    section_data.is_new_structure = 1;
  else if (looking_at("existing", line))
    section_data.is_new_structure = 0;
  else {
    print_error("new structure flag (`new' or `existing') expected");
    return 1;
  }

  PARSE_IDENTIFIER(section_data.structure_type, line,
		   "section structure type");

  if (!section_data.is_repeatable) {
    PARSE_IDENTIFIER(section_data.dispose_function_name, line,
		     "section structure dispose function");
  }
  else {
    PARSE_IDENTIFIER(section_data.list_text_field_name, line,
		     "section list text field name");
    PARSE_IDENTIFIER(section_data.list_function_prefix, line,
		     "section list function prefix");
    PARSE_IDENTIFIER(section_data.static_list_macro, line,
		     "section static list macro");
  }

  PARSE_IDENTIFIER(section_data.structure_name, line,
		   "section structure name");
  PARSE_IDENTIFIER(section_values_array, line, "section values array name");
  PARSE_IDENTIFIER(section_defaults_list, line, "section defaults list name");

  existing_section_array = section_array_list_find(&value_arrays,
						   section_values_array);
  if (!existing_section_array) {
    string_list_add(&value_arrays, section_values_array);
    value_arrays.last->section_data = section_data;
  }
  else {
    if (!section_datum_equal(&section_data,
			     &existing_section_array->section_data)) {
      print_error("sections %s and %s values arrays match, but not structures",
		  existing_section_array->section_data.section_name,
		  section_data.section_name);
      return 1;
    }
  }

  existing_section_array = section_array_list_find(&defaults_lists,
						   section_defaults_list);
  if (!existing_section_array) {
    string_list_add(&defaults_lists, section_defaults_list);
    defaults_lists.last->section_data = section_data;
  }
  else {
    if (!section_datum_equal(&section_data,
			     &existing_section_array->section_data)) {
      print_error("sections %s and %s defaults lists match, but not structures",
		  existing_section_array->section_data.section_name,
		  section_data.section_name);
      return 1;
    }
  }

  string_buffer_printf(c_file_arrays,
		       ("  { %s, %d,\n    &%s,\n    %s_init, %s,\n    %s,\n"
			"    sizeof(%s) / sizeof(ConfigurationValue) }"),
		       section_data.section_name, section_data.is_repeatable,
		       section_data.structure_name,
		       section_defaults_list,
		       (section_data.is_repeatable
			? "string_list_empty"
			: section_data.dispose_function_name),
		       section_values_array, section_values_array);

  return 0;
}


static int
configuration_initialize_values(StringBuffer *c_file_arrays,
				const char *h_file_enum_name,
				const char *c_file_array_name)
{
  SectionArrayListItem *value_array
    = section_array_list_find(&value_arrays, c_file_array_name);

  UNUSED(c_file_arrays);
  UNUSED(h_file_enum_name);

  if (!value_array) {
    print_error("section value list `%s' was not described in section list",
		c_file_array_name);
    return 1;
  }

  current_values_data = &value_array->section_data;
  return 0;
}


static int
configuration_parse_values2(StringBuffer *c_file_arrays,
			    char **line, const char *identifier,
			    char **pending_eol_comment,
			    int *pending_linefeeds)
{
  const char *value_name;
  const char *field_name;
  const char *value_type;

  UNUSED(identifier);
  UNUSED(pending_eol_comment);
  UNUSED(pending_linefeeds);

  PARSE_THING(value_name, STRING, line, "value name");
  PARSE_THING(field_name, FIELD_NAME, line, "field name");

  if (looking_at("string", line))
    value_type = "VALUE_TYPE_STRING";
  else if (looking_at("string_list", line))
    value_type = "VALUE_TYPE_STRING_LIST";
  else if (looking_at("boolean", line))
    value_type = "VALUE_TYPE_BOOLEAN";
  else if (looking_at("int", line))
    value_type = "VALUE_TYPE_INT";
  else if (looking_at("real", line))
    value_type = "VALUE_TYPE_REAL";
  else if (looking_at("color", line))
    value_type = "VALUE_TYPE_COLOR";
  else if (looking_at("time", line))
    value_type = "VALUE_TYPE_TIME";
  else {
    print_error("value/field type expected");
    return 1;
  }

  string_buffer_printf(c_file_arrays,
		       "  { %s, %s,\n    STRUCTURE_FIELD_OFFSET(%s%s, %s) }",
		       value_name, value_type,
		       current_values_data->structure_type,
		       current_values_data->is_repeatable ? "Item" : "",
		       field_name);

  return 0;
}


static int
configuration_initialize_defaults(StringBuffer *c_file_arrays,
				  const char *h_file_enum_name,
				  const char *c_file_array_name)
{
  SectionArrayListItem *defaults_list
    = section_array_list_find(&defaults_lists, c_file_array_name);

  UNUSED(c_file_arrays);
  UNUSED(h_file_enum_name);

  if (!defaults_list) {
    print_error("defaults list `%s' was not described in section list",
		c_file_array_name);
    return 1;
  }

  current_defaults_list = c_file_array_name;
  current_defaults_data = &defaults_list->section_data;

  push_subscription_stack(0, current_defaults_data->is_new_structure,
			  !current_defaults_data->is_repeatable,
			  current_defaults_data->structure_type,
			  current_defaults_data->dispose_function_name);

  if (current_defaults_data->is_repeatable) {
    int structure_type_length = strlen(current_defaults_data->structure_type);

    string_buffer_printf(&stack_pointer->structure_declaration,
			 ("\n\ntypedef struct _%sItem%s%sItem;\n"
			  "typedef struct _%s%s%s;\n\n"),
			 current_defaults_data->structure_type,
			 TABBING(5, 16 + structure_type_length + 4),
			 current_defaults_data->structure_type,
			 current_defaults_data->structure_type,
			 TABBING(5, 16 + structure_type_length),
			 current_defaults_data->structure_type);

    string_buffer_printf(&stack_pointer->structure_declaration,
			 ("struct _%sItem {\n"
			  "  %sItem%s*next;\n  char%s*%s;\n\n"),
			 current_defaults_data->structure_type,
			 current_defaults_data->structure_type,
			 TABBING(4, 2 + structure_type_length + 4),
			 TABBING(4, 6),
			 current_defaults_data->list_text_field_name);
  }

  string_buffer_empty(&initialization_values);

  any_fields_to_init = 0;

  last_array_index = NULL;
  current_subscription_level = 0;

  return 0;
}


static int
configuration_parse_defaults2(StringBuffer *c_file_arrays,
			      char **line, const char *identifier,
			      char **pending_eol_comment,
			      int *pending_linefeeds)
{
  const char *field_name;

  UNUSED(c_file_arrays);
  UNUSED(identifier);
  UNUSED(pending_eol_comment);
  UNUSED(pending_linefeeds);

  PARSE_THING(field_name,
	      (stack_pointer->is_array_subscription ? FIELD_NAME : IDENTIFIER),
	      line, "field name");

  if (strcmp(field_name, "end") != 0) {
    char *full_field_name = NULL;
    int full_field_subscription_length;
    ConfigurationValueType field_type = VALUE_TYPE_INVALID;
    const char *structure_type;
    const char *dispose_field_function = NULL;
    const char *dispose_field_prefix = "";
    const char *default_value = NULL;
    char *multiline_string = NULL;
    const char *string_to_duplicate = NULL;
    char buffer[64];

    recursively_build_full_field_name(stack_pointer, &full_field_name);
    full_field_name = utils_cat_string(full_field_name, field_name);
    full_field_subscription_length = 13 + strlen(full_field_name);

    if (stack_pointer->is_array_subscription
	? stack_pointer->array_element_type == VALUE_TYPE_STRING
	: looking_at("string", line)) {
      while (*line && ! **line)
	*line = read_line();

      if (*line && **line == '"') {
	multiline_string
	  = parse_multiline_string(line,
				   ("string constant, `char *' variable, "
				    "or NULL"),
				   (full_field_subscription_length > 32
				    ? "\n\t\t\t     " : "\n\t\t\t\t\t\t\t "),
				   0);
	string_to_duplicate = multiline_string;
      }
      else {
	PARSE_IDENTIFIER(string_to_duplicate, line,
			 "string constant, `char *' variable or NULL");
	if (strcmp(string_to_duplicate, "NULL") == 0)
	  string_to_duplicate = NULL;
      }

      default_value	     = "NULL";
      field_type	     = VALUE_TYPE_STRING;
      dispose_field_function = "utils_free";
    }
    else if (stack_pointer->is_array_subscription
	     ? stack_pointer->array_element_type == VALUE_TYPE_STRING_LIST
	     : looking_at("string_list", line)) {
      if (!looking_at("-", line)) {
	/* FIXME */
      }

      default_value = "STATIC_STRING_LIST";

      field_type	     = VALUE_TYPE_STRING_LIST;
      dispose_field_function = "string_list_empty";
      dispose_field_prefix   = "&";
    }
    else if (stack_pointer->is_array_subscription
	     ? stack_pointer->array_element_type == VALUE_TYPE_BOOLEAN
	     : looking_at("boolean", line)) {
      if (looking_at("true", line))
	default_value = "1";
      else if (looking_at("false", line))
	default_value = "0";
      else {
	print_error("`true' or `false' expected");
	utils_free(full_field_name);
	return 1;
      }

      field_type = VALUE_TYPE_BOOLEAN;
    }
    else if (stack_pointer->is_array_subscription
	     ? stack_pointer->array_element_type == VALUE_TYPE_INT
	     : looking_at("int", line)) {
      PARSE_THING(default_value, INTEGER_NUMBER, line, "integer number");

      field_type = VALUE_TYPE_INT;
    }
    else if (stack_pointer->is_array_subscription
	     ? stack_pointer->array_element_type == VALUE_TYPE_REAL
	     : looking_at("real", line)) {
      PARSE_THING(default_value, FLOATING_POINT_NUMBER, line, "real number");

      field_type = VALUE_TYPE_REAL;
    }
    else if (stack_pointer->is_array_subscription
	     ? stack_pointer->array_element_type == VALUE_TYPE_COLOR
	     : looking_at("color", line)) {
      QuarryColor color;

      if (!parse_color(line, &color, "color")) {
	utils_free(full_field_name);
	return 1;
      }

      sprintf(buffer, "{ %d, %d, %d }", color.red, color.green, color.blue);
      default_value = buffer;

      field_type = VALUE_TYPE_COLOR;
    }
    else if (stack_pointer->is_array_subscription
	     ? stack_pointer->array_element_type == VALUE_TYPE_TIME
	     : looking_at("time", line)) {
      int seconds;

      PARSE_THING(default_value, TIME_VALUE, line, "time value");

      seconds = utils_parse_time(default_value);
      if (seconds < 0) {
	utils_free(full_field_name);
	return 1;
      }

      sprintf(buffer, "%d", seconds);
      default_value = buffer;

      field_type = VALUE_TYPE_TIME;
    }
    else if (stack_pointer->is_array_subscription
	     ? stack_pointer->array_element_type == VALUE_TYPE_STRUCTURE
	     : looking_at("structure", line)) {
      int is_new_structure;

      utils_free(full_field_name);

      if (!stack_pointer->is_array_subscription) {
	if (looking_at("new", line))
	  is_new_structure = 1;
	else if (looking_at("existing", line))
	  is_new_structure = 0;
	else {
	  print_error("new structure flag (`new' or `existing') expected");
	  return 1;
	}

	PARSE_IDENTIFIER(structure_type, line, "structure field type");
	if (!looking_at("-", line))
	  PARSE_IDENTIFIER(dispose_field_function, line, "dispose function");
      }
      else {
	is_new_structure       = stack_pointer->declaring_new_structure;
	structure_type	       = stack_pointer->structure_type;
	dispose_field_function = stack_pointer->dispose_function_name;
      }

      push_subscription_stack(0, is_new_structure, 1,
			      structure_type, dispose_field_function);
      stack_pointer->field_name_part = field_name;

      if (!current_defaults_data->is_repeatable)
	open_initialization_braces(1);

      add_field_declaration_if_needed(stack_pointer->previous,
				      VALUE_TYPE_STRUCTURE,
				      field_name, structure_type, NULL);
      return 0;
    }
    else if (!stack_pointer->is_array_subscription && looking_at("array", line)) {
      const char *dimensions;

      utils_free(full_field_name);

      push_subscription_stack(1, 0, 0, NULL, NULL);
      stack_pointer->field_name_part = field_name;

      PARSE_THING(dimensions, FIELD_NAME, line, "array dimensions");

      if (looking_at("string", line))
	stack_pointer->array_element_type = VALUE_TYPE_STRING;
      else if (looking_at("string_list", line))
	stack_pointer->array_element_type = VALUE_TYPE_STRING_LIST;
      else if (looking_at("boolean", line))
	stack_pointer->array_element_type = VALUE_TYPE_BOOLEAN;
      else if (looking_at("int", line))
	stack_pointer->array_element_type = VALUE_TYPE_INT;
      else if (looking_at("real", line))
	stack_pointer->array_element_type = VALUE_TYPE_REAL;
      else if (looking_at("color", line))
	stack_pointer->array_element_type = VALUE_TYPE_COLOR;
      else if (looking_at("time", line))
	stack_pointer->array_element_type = VALUE_TYPE_TIME;
      else if (looking_at("structure", line)) {
	stack_pointer->array_element_type = VALUE_TYPE_STRUCTURE;

	if (looking_at("new", line))
	  stack_pointer->declaring_new_structure = 1;
	else if (looking_at("existing", line))
	  stack_pointer->declaring_new_structure = 0;
	else {
	  print_error("new structure flag (`new' or `existing') expected");
	  return 1;
	}

	PARSE_IDENTIFIER(stack_pointer->structure_type, line,
			 "type of structure array elements");
	if (!looking_at("-", line)) {
	  PARSE_IDENTIFIER(stack_pointer->dispose_function_name, line,
			   "dispose function");
	}
	else
	  stack_pointer->dispose_function_name = NULL;
      }
      else {
	print_error("array element type expected");
	return 1;
      }

      add_field_declaration_if_needed(stack_pointer->previous,
				      stack_pointer->array_element_type,
				      field_name,
				      stack_pointer->structure_type,
				      dimensions);

      if (!current_defaults_data->is_repeatable) {
	for (stack_pointer->num_array_dimensions = 0; *dimensions;
	     dimensions++) {
	  if (*dimensions == '[')
	    stack_pointer->num_array_dimensions++;
	}

	open_initialization_braces(stack_pointer->num_array_dimensions);
      }

      return 0;
    }

    if (current_defaults_data->is_repeatable || string_to_duplicate) {
      if (!any_fields_to_init) {
	const char *type_suffix = (current_defaults_data->is_repeatable
				   ? "Item" : "");

	string_buffer_printf(&c_file_bottom,
			     ("\n\nstatic void\n"
			      "%s_init(void *section_structure)\n{\n"
			      "  %s%s *structure = (%s%s *) section_structure;"
			      "\n\n"),
			     current_defaults_list,
			     current_defaults_data->structure_type,
			     type_suffix,
			     current_defaults_data->structure_type,
			     type_suffix);

	any_fields_to_init = 1;
      }

      if (field_type != VALUE_TYPE_STRING_LIST) {
	string_buffer_printf(&c_file_bottom, "  structure->%s%s= ",
			     full_field_name,
			     (full_field_subscription_length > 32
			      ? "\n    "
			      : TABBING(4, full_field_subscription_length)));

	if (!string_to_duplicate) {
	  string_buffer_cat_strings(&c_file_bottom,
				    default_value, ";\n", NULL);
	}
	else {
	  string_buffer_cat_strings(&c_file_bottom,
				    "utils_duplicate_string(",
				    string_to_duplicate, ");\n",
				    NULL);
	}
      }
      else {
	string_buffer_cat_strings(&c_file_bottom,
				  "  string_list_init(&structure->",
				  full_field_name, ");\n",
				  NULL);
      }
    }

    if (!current_defaults_data->is_repeatable) {
      if (last_array_index) {
	int subscription_level_difference
	  = stack_pointer->num_array_dimensions;
	const char *field_name_scan;
	const char *last_array_index_scan;

	for (field_name_scan = field_name,
	       last_array_index_scan = last_array_index;
	     (*field_name_scan == *last_array_index_scan
	      && *field_name_scan != 0);
	     field_name_scan++, last_array_index_scan++) {
	  if (*field_name_scan == '[')
	    subscription_level_difference--;
	}

	close_initialization_braces(subscription_level_difference);
	open_initialization_braces(subscription_level_difference);
      }

      if (initialization_values.length > 0
	  && (initialization_values.string[initialization_values.length - 1]
	      != '{'))
	string_buffer_add_character(&initialization_values, ',');

      string_buffer_add_character(&initialization_values, '\n'); 
      add_initialization_indentation(current_subscription_level + 1);
      string_buffer_cat_string(&initialization_values, default_value);

      if (stack_pointer->is_array_subscription)
	last_array_index = field_name;
    }

    add_field_declaration_if_needed(stack_pointer, field_type,
				    field_name, NULL, NULL);
    add_field_dispose_call_if_needed(stack_pointer,
				     dispose_field_function,
				     dispose_field_prefix,
				     field_name);

    utils_free(full_field_name);
    utils_free(multiline_string);
  }
  else {
    SubscriptionStackItem *stack_item = pop_subscription_stack(1);

    if (!stack_pointer) {
      print_error("unmatched `end'");
      utils_free(stack_item);
      return 1;
    }

    if (!stack_item->is_array_subscription) {
      add_field_dispose_call_if_needed(stack_pointer,
				       stack_item->dispose_function_name,
				       "&", stack_item->field_name_part);
    }

    if (!current_defaults_data->is_repeatable) {
      close_initialization_braces(stack_item->is_array_subscription
				  ? stack_item->num_array_dimensions : 1);
      last_array_index = NULL;
    }

    if (stack_pointer->is_array_subscription)
      stack_pointer->declaring_new_structure = 0;

    utils_free(stack_item);
  }

  return 0;
}


static int
configuration_finalize_defaults(StringBuffer *c_file_arrays)
{
  static int last_init_function_defined = 0;

  SectionArrayListItem *defaults_list = NULL;
  int structure_type_length = strlen(current_defaults_data->structure_type);
  int any_fields_disposed = (stack_pointer->dispose_function.length > 0);

  string_buffer_add_character(c_file_arrays, '\n');
  string_buffer_add_character(&h_file_bottom, '\n');

  close_initialization_braces(0);

  while ((defaults_list
	  = section_array_list_find_after_notch(&defaults_lists,
						current_defaults_list,
						defaults_list))
	  != NULL) {
    if (current_defaults_data->is_repeatable) {
      string_buffer_printf(c_file_arrays, "\n%s %s = %s;\n",
			   current_defaults_data->structure_type,
			   defaults_list->section_data.structure_name,
			   current_defaults_data->static_list_macro);
    }
    else {
      string_buffer_printf(c_file_arrays, "\n%s %s = {%s\n};\n",
			   current_defaults_data->structure_type,
			   defaults_list->section_data.structure_name,
			   initialization_values.string);
    }

    string_buffer_printf(&h_file_bottom, "extern %s%s%s;\n",
			 current_defaults_data->structure_type,
			 TABBING(5, 7 + structure_type_length),
			 defaults_list->section_data.structure_name);
  }

  if (any_fields_to_init)
    string_buffer_cat_string(&c_file_bottom, "}\n");

  finish_function(&c_file_top, current_defaults_list, "_init",
		  any_fields_to_init, &last_init_function_defined);

  utils_free(pop_subscription_stack(!current_defaults_data->is_repeatable));
  if (stack_pointer) {
    print_error("`end' expected, unterminated %s",
		stack_pointer->is_array_subscription ? "array" : "structure");
    do
      utils_free(pop_subscription_stack(0));
    while (stack_pointer);

    return 1;
  }

  if (current_defaults_data->is_new_structure
      && current_defaults_data->is_repeatable) {
    int list_type_length = strlen(current_defaults_data->structure_type);
    int list_text_field_name_length
      = strlen(current_defaults_data->list_text_field_name);
    int list_function_prefix_length
      = strlen(current_defaults_data->list_function_prefix);
    int static_list_macro_length
      = strlen(current_defaults_data->static_list_macro);

    string_buffer_printf(&h_file_top,
			 ("\nstruct _%s {\n"
			  "  %sItem%s*first;\n  %sItem%s*last;\n\n"
			  "  int%s item_size;\n"
			  "  StringListItemDispose%s item_dispose;\n};\n\n\n"),
			 current_defaults_data->structure_type,
			 current_defaults_data->structure_type,
			 TABBING(4, 2 + list_type_length + 4),
			 current_defaults_data->structure_type,
			 TABBING(4, 2 + list_type_length + 4),
			 TABBING(4, 5), TABBING(4, 23));

    string_buffer_printf(&h_file_top,
			 ("#define %s_new()%s\\\n"
			  "  string_list_new_derived(sizeof(%sItem),"),
			 current_defaults_data->list_function_prefix,
			 TABBING(9, 8 + list_function_prefix_length + 6),
			 current_defaults_data->structure_type);

    if (any_fields_disposed) {
      string_buffer_printf(&h_file_top,
			   ("%s\\\n\t\t\t  ((StringListItemDispose)%s\\\n"
			    "\t\t\t   %s_item_dispose))\n\n"),
			   TABBING(9, 33 + list_type_length + 6),
			   TABBING(9, 50),
			   current_defaults_data->list_function_prefix);
    }
    else
      string_buffer_cat_string(&h_file_top, " NULL)\n\n");

    string_buffer_printf(&h_file_top,
			 ("#define %s_init(list)%s\\\n"
			  "  string_list_init_derived((list), "
			  "sizeof(%sItem),"),
			 current_defaults_data->list_function_prefix,
			 TABBING(9, 8 + list_function_prefix_length + 11),
			 current_defaults_data->structure_type);

    if (any_fields_disposed) {
      string_buffer_printf(&h_file_top,
			   ("%s\\\n\t\t\t   ((StringListItemDispose)%s\\\n"
			    "\t\t\t    %s_item_dispose))\n\n"),
			   TABBING(9, 42 + list_type_length + 6),
			   TABBING(9, 51),
			   current_defaults_data->list_function_prefix);
    }
    else
      string_buffer_cat_string(&h_file_top, " NULL)\n\n");

    string_buffer_printf(&h_file_top,
			 ("#define %s%s\\\n"
			  "  STATIC_STRING_LIST_DERIVED(%sItem,"),
			 current_defaults_data->static_list_macro,
			 TABBING(9, 8 + static_list_macro_length),
			 current_defaults_data->structure_type);

    if (any_fields_disposed) {
      string_buffer_printf(&h_file_top,
			   "%s\\\n\t\t\t     %s_item_dispose)\n\n",
			   TABBING(9, 29 + list_type_length + 5),
			   current_defaults_data->list_function_prefix);
    }
    else
      string_buffer_cat_string(&h_file_top, " NULL)\n\n");

    if (any_fields_disposed) {
      string_buffer_printf(&h_file_top,
			   "void\t\t%s_item_dispose(%sItem *item);\n\n",
			   current_defaults_data->list_function_prefix,
			   current_defaults_data->structure_type);
    }

    string_buffer_printf(&h_file_top,
			 ("\n#define "
			  "%s_get_item(list, item_index)%s\\\n"
			  "  ((%sItem *)%s\\\n"
			  "   string_list_get_item((list), (item_index)))\n"),
			 current_defaults_data->list_function_prefix,
			 TABBING(9, 8 + list_function_prefix_length + 27),
			 current_defaults_data->structure_type,
			 TABBING(9, 4 + list_type_length + 7));

    string_buffer_printf(&h_file_top,
			 ("\n#define %s_find(list, %s)%s\\\n"
			  "  ((%sItem *) string_list_find((list), (%s)))\n"),
			 current_defaults_data->list_function_prefix,
			 current_defaults_data->list_text_field_name,
			 TABBING(9, (8 + list_function_prefix_length + 12
				     + list_text_field_name_length + 1)),
			 current_defaults_data->structure_type,
			 current_defaults_data->list_text_field_name);

    string_buffer_printf(&h_file_top,
			 ("\n#define "
			  "%s_find_after_notch(list, %s, notch)%s\\\n"
			  "  ((%sItem *)%s\\\n"
			  "   string_list_find_after_notch((list), "
			  "(%s), (notch)))\n"),
			 current_defaults_data->list_function_prefix,
			 current_defaults_data->list_text_field_name,
			 TABBING(9, (8 + list_function_prefix_length + 24
				     + list_text_field_name_length + 8)),
			 current_defaults_data->structure_type,
			 TABBING(9, 4 + list_type_length + 7),
			 current_defaults_data->list_text_field_name);
  }

  return 0;
}


static int
configuration_finalize(StringBuffer *c_file_arrays)
{
  UNUSED(c_file_arrays);

  if (dispose_function_prototypes.length > 0) {
    string_buffer_cat_as_string(&c_file_top,
				dispose_function_prototypes.string,
				dispose_function_prototypes.length);
  }

  if (dispose_functions.length > 0) {
    string_buffer_cat_as_string(&c_file_bottom,
				dispose_functions.string,
				dispose_functions.length);
  }

  return 0;
}


static int
section_datum_equal(const SectionData *first_data,
		    const SectionData *second_data)
{
  if (first_data->is_repeatable != second_data->is_repeatable
      || strcmp(first_data->structure_type, second_data->structure_type) != 0)
    return 0;

  if (first_data->is_repeatable) {
    return (strcmp(first_data->list_text_field_name,
		   second_data->list_text_field_name) == 0
	    && strcmp(first_data->list_function_prefix,
		      second_data->list_function_prefix) == 0
	    && strcmp(first_data->static_list_macro,
		      second_data->static_list_macro) == 0);
  }
  else {
    return (strcmp(first_data->dispose_function_name,
		   second_data->dispose_function_name) == 0);
  }
}


static void
push_subscription_stack(int is_array_subscription,
			int is_new_structure,
			int do_start_structure_declaration,
			const char *structure_type,
			const char *dispose_function_name)
{
  SubscriptionStackItem *new_stack_item
    = utils_malloc(sizeof(SubscriptionStackItem));

  new_stack_item->previous = stack_pointer;
  stack_pointer = new_stack_item;

  if (is_new_structure
      && string_list_find(&declared_structures, structure_type))
    is_new_structure = 0;

  stack_pointer->is_array_subscription	 = is_array_subscription;
  stack_pointer->declaring_new_structure = is_new_structure;
  stack_pointer->structure_type		 = structure_type;

  if (is_new_structure) {
    string_buffer_init(&stack_pointer->structure_declaration, 0x400, 0x100);
    string_buffer_init(&stack_pointer->dispose_function, 0x400, 0x100);

    if (do_start_structure_declaration) {
      string_buffer_printf(&stack_pointer->structure_declaration,
			   "\n\ntypedef struct _%s%s%s;\n\nstruct _%s {\n",
			   structure_type,
			   TABBING(5, 16 + strlen(structure_type)),
			   structure_type, structure_type);
    }

    string_list_add(&declared_structures, structure_type);
  }

  stack_pointer->dispose_function_name = dispose_function_name;
}


static SubscriptionStackItem *
pop_subscription_stack(int do_write_dispose_function_prototype)
{
  static int last_dispose_function_defined = 0;
  SubscriptionStackItem *current_stack_item = stack_pointer;

  if (stack_pointer->declaring_new_structure) {
    string_buffer_cat_as_strings(&h_file_top,
				 stack_pointer->structure_declaration.string,
				 stack_pointer->structure_declaration.length,
				 "};\n", 3, NULL);

    if (stack_pointer->dispose_function.length > 0) {
      string_buffer_cat_as_strings(&dispose_functions,
				   stack_pointer->dispose_function.string,
				   stack_pointer->dispose_function.length,
				   "}\n", 2, NULL);
    }
    else {
      string_list_add(&empty_dispose_functions,
		      stack_pointer->dispose_function_name);
    }

    if (do_write_dispose_function_prototype) {
      finish_function(&dispose_function_prototypes,
		      stack_pointer->dispose_function_name, "",
		      stack_pointer->dispose_function.length > 0,
		      &last_dispose_function_defined);
    }

    string_buffer_dispose(&stack_pointer->structure_declaration);
    string_buffer_dispose(&stack_pointer->dispose_function);
  }

  stack_pointer = stack_pointer->previous;
  return current_stack_item;
}


static void
recursively_build_full_field_name(const SubscriptionStackItem *stack_item,
				  char **full_field_name)
{
  if (stack_item->previous) {
    recursively_build_full_field_name(stack_item->previous, full_field_name);
    *full_field_name = utils_cat_strings(*full_field_name,
					 stack_item->field_name_part,
					 (stack_item->is_array_subscription
					  ? NULL : "."),
					 NULL);
  }
}


static void
add_field_declaration_if_needed(SubscriptionStackItem *stack_item,
				ConfigurationValueType field_type,
				const char *field_name,
				const char *structure_type,
				const char *array_dimensions)
{
  if (!stack_item->is_array_subscription
      && stack_item->declaring_new_structure) {
    const char *field_type_string;

    switch (field_type) {
    case VALUE_TYPE_STRING:
      field_type_string = "char";
      break;

    case VALUE_TYPE_STRING_LIST:
      field_type_string = "StringList";
      break;

    case VALUE_TYPE_BOOLEAN:
    case VALUE_TYPE_INT:
    case VALUE_TYPE_TIME:
      field_type_string = "int";
      break;

    case VALUE_TYPE_REAL:
      field_type_string = "double";
      break;

    case VALUE_TYPE_COLOR:
      field_type_string = "QuarryColor";
      break;

    default:
      field_type_string = structure_type;
    }

    string_buffer_printf(&stack_item->structure_declaration, "  %s%s%c%s%s;\n",
			 field_type_string,
			 TABBING(4, 2 + strlen(field_type_string)),
			 (field_type != VALUE_TYPE_STRING ? ' ' : '*'),
			 field_name,
			 (array_dimensions ? array_dimensions : ""));
  }
}


static void
add_field_dispose_call_if_needed(SubscriptionStackItem *stack_item,
				 const char *dispose_field_function,
				 const char *dispose_field_prefix,
				 const char *field_name)
{
  if (stack_item->is_array_subscription)
    stack_item = stack_item->previous;

  if (stack_item->declaring_new_structure
      && dispose_field_function
      && !string_list_find(&empty_dispose_functions, dispose_field_function)) {
    int is_repeatable = (!stack_item->previous
			 && current_defaults_data->is_repeatable);

    if (stack_item->dispose_function.length == 0) {
      if (is_repeatable) {
	string_buffer_printf(&stack_item->dispose_function,
			     ("\n\nvoid\n"
			      "%s_item_dispose(%sItem *item)\n{\n"),
			     current_defaults_data->list_function_prefix,
			     current_defaults_data->structure_type);
      }
      else {
	string_buffer_printf(&stack_item->dispose_function,
			     ("\n\nstatic void\n"
			      "%s(void *section_structure)\n{\n"
			      "  %s *structure = (%s *) section_structure;"
			      "\n\n"),
			     stack_item->dispose_function_name,
			     stack_item->structure_type,
			     stack_item->structure_type);
      }
    }

    string_buffer_printf(&stack_item->dispose_function,
			 "  %s(%s%s->%s%s);\n",
			 dispose_field_function, dispose_field_prefix,
			 (is_repeatable ? "item" : "structure"),
			 (stack_pointer->is_array_subscription
			  ? stack_pointer->field_name_part : ""),
			 field_name);
  }
}


static void
open_initialization_braces(int num_levels)
{
  if (num_levels > 0) {
    int k;

    if (initialization_values.length > 0
	&& (initialization_values.string[initialization_values.length - 1]
	    != '{'))
      string_buffer_add_character(&initialization_values, ',');

    for (k = 0; k < num_levels; k++) {
      string_buffer_add_character(&initialization_values, '\n');
      add_initialization_indentation(++current_subscription_level);
      string_buffer_add_character(&initialization_values, '{');
    }
  }
}


static void
close_initialization_braces(int num_levels)
{
  int k;

  for (k = 0; k < num_levels; k++) {
    string_buffer_add_character(&initialization_values, '\n');
    add_initialization_indentation(current_subscription_level--);
    string_buffer_add_character(&initialization_values, '}');
  }
}


static void
add_initialization_indentation(int indentation_level)
{
  static const char *six_spaces = "      ";

  if (indentation_level >= 4) {
    string_buffer_cat_string(&initialization_values,
			     TABBING(indentation_level / 4, 0));
  }

  if (indentation_level % 4) {
    string_buffer_cat_string(&initialization_values,
			     six_spaces + 2 * (3 - (indentation_level % 4)));
  }
}


static void
finish_function(StringBuffer *prototypes_buffer,
		const char *function_name, const char *function_name_suffix,
		int function_defined, int *last_function_defined)
{
  if (prototypes_buffer->length > 0) {
    if (*last_function_defined != function_defined)
      string_buffer_add_character(prototypes_buffer, '\n');
  }
  else
    string_buffer_add_characters(prototypes_buffer, '\n', 2);

  if (function_defined) {
    string_buffer_cat_strings(prototypes_buffer,
			      "static void\t",
			      function_name, function_name_suffix,
			      "(void *section_structure);\n",
			      NULL);
  }
  else {
    int function_name_length = (strlen(function_name)
				+ strlen(function_name_suffix));

    string_buffer_cat_strings(prototypes_buffer,
			      "#define\t\t",
			      function_name, function_name_suffix,
			      TABBING(8, 16 + function_name_length),
			      "NULL\n",
			      NULL);
  }

  *last_function_defined = function_defined;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
