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


#include "parse-list.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>


typedef struct _ValueTypeListItem	ValueTypeListItem;
typedef struct _ValueTypeList		ValueTypeList;

struct _ValueTypeListItem {
  ValueTypeListItem	 *next;
  char			 *infile_name;

  const char		 *c_name;
  const char		 *value_writer_function;
};

struct _ValueTypeList {
  ValueTypeListItem	 *first;
  ValueTypeListItem	 *last;

  int			  item_size;
  StringListItemDispose	  item_dispose;
};


#define value_type_list_new()						\
  string_list_new_derived(sizeof(ValueTypeListItem), NULL)

#define value_type_list_init(list)					\
  string_list_init_derived((list), sizeof(ValueTypeListItem), NULL)

#define STATIC_VALUE_TYPE_LIST						\
  STATIC_STRING_LIST_DERIVED(ValueTypeListItem, NULL)


#define value_type_list_get_item(list, item_index)			\
  ((ValueTypeListItem *) string_list_get_item((list), (item_index)))

#define value_type_list_find(list, infile_name)				\
  ((ValueTypeListItem *) string_list_find((list), (infile_name)))

#define value_type_list_find_after_notch(list, infile_name, notch)	\
  ((ValueTypeListItem *)\						\
   string_list_find_after_notch((list), (infile_name), (notch)))


typedef struct _TreeNode	TreeNode;

struct _TreeNode {
  int		index;
  char	       *identifiers[1 + ('Z' - 'A')];
  TreeNode     *children[1 + ('Z' - 'A')];
};


static int	value_type_list_parse_type1(char **line);
static int	value_type_list_parse_type2(StringBuffer *c_file_arrays,
					    char **line,
					    const char *identifier,
					    char **pending_eol_comment,
					    int *pending_linefeeds);

static int	property_list_parse_type1(char **line);
static int	property_list_parse_type2(StringBuffer *c_file_arrays,
					  char **line,
					  const char *identifier,
					  char **pending_eol_comment,
					  int *pending_linefeeds);
static int	property_list_finalize(StringBuffer *c_file_arrays);


static int	error_list_parse_error2(StringBuffer *c_file_arrays,
					char **line,
					const char *identifier,
					char **pending_eol_comment,
					int *pending_linefeeds);


static void	enumerate_nodes(TreeNode *node);
static void	print_nodes(const TreeNode *node, const char *identifier);
static void	delete_nodes(TreeNode *node);


static const ListDescription properties_lists[] = {
  { "value_types", 0, SORT_NORMAL, 1, NULL,
    NULL, value_type_list_parse_type1, value_type_list_parse_type2, NULL },
  { "properties", 0, SORT_NORMAL, 1, "const SgfPropertyInfo ",
    NULL, property_list_parse_type1, property_list_parse_type2,
    property_list_finalize },
  { NULL, 0, SORT_NORMAL, 0, NULL, NULL, NULL, NULL, NULL }
};

static const ListDescription errors_list[] = {
  { "errors", 0, SORT_NORMAL, 0, "const char *",
    NULL, NULL, error_list_parse_error2, NULL },
  { NULL, 0, SORT_NORMAL, 0, NULL, NULL, NULL, NULL, NULL }
};

const ListDescriptionSet list_sets[] = {
  { "properties", properties_lists },
  { "errors", errors_list },
};


static ValueTypeList  value_types = STATIC_VALUE_TYPE_LIST;


static const char    *property_id;
static const char    *total;
static const char    *unknown;

static int	      long_names = 0;

static TreeNode	     *root = NULL;


int
main(int argc, char *argv[])
{
  int result;

  result = parse_list_main(argc, argv, list_sets,
			   sizeof(list_sets) / sizeof(ListDescriptionSet));

  string_list_empty(&value_types);

  return result;
}



static int
value_type_list_parse_type1(char **line)
{
  const char *infile_name;

  PARSE_IDENTIFIER(infile_name, line, "value type");
  string_list_add(&value_types, infile_name);

  return 0;
}


static int
value_type_list_parse_type2(StringBuffer *c_file_arrays,
			    char **line, const char *identifier,
			    char **pending_eol_comment, int *pending_linefeeds)
{
  UNUSED(c_file_arrays);
  UNUSED(pending_eol_comment);
  UNUSED(pending_linefeeds);

  value_types.last->c_name = identifier;

  PARSE_IDENTIFIER(value_types.last->value_writer_function, line,
		   "writer function");

  return 0;
}


static int
property_list_parse_type1(char **line)
{
  if (looking_at("unknown", line)) {
    PARSE_IDENTIFIER(unknown, line, "`unknown' identifier");
    *line = NULL;
  }
  else if (looking_at("total", line)) {
    PARSE_IDENTIFIER(total, line, "`total' identifier");
    *line = NULL;
  }
  else {
    if (looking_at("-", line)) {
      static const char null_string[] = "";

      property_id = null_string;
    }
    else
      PARSE_THING(property_id, PROPERTY_IDENTIFIER, line, "property id");

    if (strlen(property_id) > 2) {
      print_error("warning: strangely long property name `%s'", property_id);
      if (strlen(property_id) > 3)
	long_names = 1;
    }
  }

  return 0;
}


static int
property_list_parse_type2(StringBuffer *c_file_arrays,
			  char **line, const char *identifier,
			  char **pending_eol_comment, int *pending_linefeeds)
{
  const char *infile_value_type;
  const char *parser_function;
  ValueTypeListItem *value_type;
  TreeNode *node;

  UNUSED(pending_linefeeds);

  PARSE_IDENTIFIER(infile_value_type, line, "value type");

  value_type = value_type_list_find(&value_types, infile_value_type);
  if (!value_type) {
    print_error("unknown property value type `%s'", infile_value_type);
    return 1;
  }

  PARSE_IDENTIFIER(parser_function, line, "parser function");

  *pending_eol_comment = utils_duplicate_string(property_id);

  string_buffer_cprintf(c_file_arrays, "  { \"%s\", %s, %s, %s }",
			property_id, value_type->c_name, parser_function,
			value_type->value_writer_function);

  if (*property_id) {
    const char *pointer;

    if (!root)
      root = (TreeNode *) utils_malloc0(sizeof(TreeNode));

    for (pointer = property_id, node = root; *(pointer + 1); pointer++) {
      TreeNode **child = &node->children[*pointer - 'A'];

      if (! *child)
	*child = (TreeNode *) utils_malloc0(sizeof(TreeNode));

      node = *child;
    }

    if (node->identifiers[*pointer - 'A']) {
      print_error("duplicated property `%s'", property_id);
      return 1;
    }

    node->identifiers[*pointer - 'A'] = utils_duplicate_string(identifier);
  }

  return 0;
}


static int
property_list_finalize(StringBuffer *c_file_arrays)
{
  UNUSED(c_file_arrays);

  if (! *total) {
    print_error("`total' identifier missed");
    return 1;
  }

  if (! *unknown) {
    print_error("`unknown' identifier missed");
    return 1;
  }

  string_buffer_cprintf(&h_file_bottom,
			"\n\n#define SGF_LONG_NAMES\t\t%d\n", long_names);

  enumerate_nodes(root);

  string_buffer_cat_string(&c_file_bottom,
			   ("\nconst SgfType"
			    " property_tree[][1 + ('Z' - 'A' + 1)] = {"));
  print_nodes(root, NULL);
  string_buffer_cat_string(&c_file_bottom, "\n  }\n};\n");

  delete_nodes(root);

  return 0;
}


static void
enumerate_nodes(TreeNode *node)
{
  static int node_index = 0;
  int k;

  node->index = node_index++;

  for (k = 0; k < 'Z' - 'A' + 1; k++) {
    if (node->children[k])
      enumerate_nodes(node->children[k]);
  }
}


static void
print_nodes(const TreeNode *node, const char *identifier)
{
  int k;

  if (node->index)
    string_buffer_cat_string(&c_file_bottom, "\n  },");
  string_buffer_cat_strings(&c_file_bottom,
			    "\n  {\n    ", identifier ? identifier : unknown,
			    NULL);

  for (k = 0; k < 'Z' - 'A' + 1; k++) {
    if (node->children[k]) {
      string_buffer_cprintf(&c_file_bottom, ",\n    %s + %d",
			    total, node->children[k]->index);
    }
    else {
      string_buffer_cat_strings(&c_file_bottom,
				",\n    ",
				(node->identifiers[k]
				 ? node->identifiers[k] : unknown),
				NULL);
    }
  }

  for (k = 0; k < 'Z' - 'A' + 1; k++) {
    if (node->children[k])
      print_nodes(node->children[k], node->identifiers[k]);
  }
}


static void
delete_nodes(TreeNode *node)
{
  int k;

  for (k = 0; k < 'Z' - 'A' + 1; k++) {
    if (node->identifiers[k])
      utils_free(node->identifiers[k]);
    if (node->children[k])
      delete_nodes(node->children[k]);
  }

  utils_free(node);
}



static int
error_list_parse_error2(StringBuffer *c_file_arrays,
			char **line, const char *identifier,
			char **pending_eol_comment, int *pending_linefeeds)
{
  char *string;

  UNUSED(identifier);
  UNUSED(pending_eol_comment);

  string = parse_multiline_string(line, "error string or NULL", "\n  ", 1);
  if (!string)
    return 1;

  string_buffer_cat_strings(c_file_arrays, "  ", string, NULL);
  utils_free(string);

  *pending_linefeeds = -1;
  return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
