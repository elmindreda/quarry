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

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct _FileListItem	FileListItem;
typedef struct _FileList	FileList;

struct _FileListItem {
  FileListItem		 *next;
  char			 *filename;

  FILE			 *file;
  int			  line_number;
};

struct _FileList {
  FileListItem		 *first;
  FileListItem		 *last;

  int			  item_size;
  StringListItemDispose	  item_dispose;
};


#define file_list_new()							\
  ((FileList *) string_list_new_derived(sizeof(FileListItem),		\
					((StringListItemDispose)	\
					 file_list_item_dispose)))

#define file_list_init(list)						\
  string_list_init_derived((list), sizeof(FileListItem),		\
			   ((StringListItemDispose)			\
			    file_list_item_dispose))

#define STATIC_FILE_LIST						\
  STATIC_STRING_LIST_DERIVED(FileListItem, file_list_item_dispose)

static void	file_list_item_dispose(FileListItem *item);


#define file_list_get_item(list, item_index)				\
  ((FileListItem *) string_list_get_item((list), (item_index)))

#define file_list_find(list, filename)					\
  ((FileListItem *) string_list_find((list), (filename)))

#define file_list_find_after_notch(list, filename, notch)		\
  ((FileListItem *)							\
   string_list_find_after_notch((list), (filename), (notch)))


static void	print_usage(FILE *where,
			    const ListDescriptionSet *list_sets, int num_sets);

static FILE *	open_file(const char *filename, int for_writing);

static int	do_parse_lists(FILE *h_file, FILE *c_file,
			       const ListDescription *lists);
static void	reuse_last_line(char **current_line);


const char		*tab_string = "\t\t\t\t\t\t\t\t\t\t\t\t";


StringBuffer		 h_file_top;
StringBuffer		 h_file_bottom;

StringBuffer		 c_file_top;
StringBuffer		 c_file_bottom;


static AssociationList   substitutions = STATIC_ASSOCIATION_LIST;

static FileList		 list_files = STATIC_FILE_LIST;

static StringList	 lines = STATIC_STRING_LIST;
static int		 last_line_reusable = 0;


enum {
  OPTION_HELP = UCHAR_MAX + 1
};

static const struct option parse_list_options[] = {
  { "help",		no_argument,		NULL, OPTION_HELP },
  { "define",		required_argument,	NULL, 'D' },
  { "substitute",	required_argument,	NULL, 'D' },
  { NULL,		no_argument,		NULL, 0 }
};

static const char *help_string =
  "\n"
  "Options:\n"
  "  -D, --define SYMBOL=SUBSTITUTION\n"
  "                          define a symbol for `substitute_value' command\n"
  "  --help                  display this help and exit\n";


int
parse_list_main(int argc, char *argv[],
		const ListDescriptionSet *list_sets, int num_sets)
{
  int k;
  int option;
  int result = 255;
  const ListDescription *lists = NULL;
  char *list_file_name = NULL;
  char *h_file_name = NULL;
  char *c_file_name = NULL;

  utils_remember_program_name(argv[0]);

  while ((option = getopt_long(argc, argv, "D:", parse_list_options, NULL))
	 != -1) {
    switch (option) {
    case OPTION_HELP:
      print_usage(stdout, list_sets, num_sets);
      printf(help_string);

      result = 0;
      goto exit_parse_list_main;

    case 'D':
      {
	const char *delimiter = strchr(optarg, '=');

	if (delimiter) {
	  string_list_add_from_buffer(&substitutions,
				      optarg, delimiter - optarg);
	  substitutions.last->association
	    = utils_duplicate_string(delimiter + 1);
	}
	else {
	  string_list_add(&substitutions, optarg);
	  substitutions.last->association = utils_duplicate_string("");
	}
      }

      break;

    default:
      fprintf(stderr, "Try `%s --help' for more information.\n",
	      full_program_name);
      goto exit_parse_list_main;
    }
  }

  if (num_sets > 1 && argc - optind == 4) {
    for (k = 0; k < num_sets; k++) {
      if (strcmp(argv[optind], list_sets[k].command_line_name) == 0)
	lists = list_sets[k].lists;
    }

    if (!lists) {
      fprintf(stderr, "%s: fatal: unknown mode `%s'\n",
	      short_program_name, argv[optind]);
    }

    optind++;
  }
  else if (num_sets == 1 && argc - optind == 3)
    lists = list_sets[0].lists;
  else
    print_usage(stderr, list_sets, num_sets);

  list_file_name = argv[optind];
  h_file_name	 = argv[optind + 1];
  c_file_name	 = argv[optind + 2];

  if (lists) {
    FILE *list_file = open_file(list_file_name, 0);

    if (list_file) {
      FILE *h_file = open_file(h_file_name, 1);

      string_list_prepend(&list_files, list_file_name);
      list_files.first->file = list_file;
      list_files.first->line_number = 0;

      if (h_file) {
	FILE *c_file = open_file(c_file_name, 1);

	if (c_file) {
	  static const char *preamble =
	    "/* This file is automatically generated by `%s'.\n"
	    " * Do not modify it, edit `%s' instead.\n"
	    " */\n";

	  int n;
	  int h_file_name_length = strlen(h_file_name);

	  fprintf(h_file, preamble, short_program_name, list_file_name);
	  fprintf(c_file, preamble, short_program_name, list_file_name);

	  for (k = h_file_name_length; k >= 1; k--) {
	    if (h_file_name[k - 1] == DIRECTORY_SEPARATOR)
	      break;
	  }

	  for (n = 0; k < h_file_name_length; k++) {
	    if (isalnum(h_file_name[k]))
	      h_file_name[n++] = toupper(h_file_name[k]);
	    else if (h_file_name[k] == '.'
		     || h_file_name[k] == '_'
		     || h_file_name[k] == '-')
	      h_file_name[n++] = '_';
	  }

	  h_file_name[n] = 0;
	  fprintf(h_file, "\n\n#ifndef QUARRY_%s\n#define QUARRY_%s\n",
		  h_file_name, h_file_name);

	  result = do_parse_lists(h_file, c_file, lists);

	  fprintf(h_file, "\n\n#endif /* QUARRY_%s */\n", h_file_name);

	  fclose(c_file);
	}

	fclose(h_file);
      }

      string_list_empty(&list_files);
    }
  }
  else {
    fprintf(stderr, "Try `%s --help' for more information.\n",
	    full_program_name);
  }

 exit_parse_list_main:
  string_list_empty(&substitutions);
  utils_free_program_name_strings();

  return result;
}


static void
print_usage(FILE *where, const ListDescriptionSet *list_sets, int num_sets)
{
  fprintf(where, "Usage: %s [OPTION]... ", full_program_name);

  if (num_sets > 1) {
    int k;

    for (k = 0; k < num_sets; k++) {
      fprintf(where, "%c%s", k > 0 ? '|' : '{',
	      list_sets[k].command_line_name);
    }

    fputs("} ", where);
  }

  fputs("LIST_FILE H_FILE C_FILE\n", where);
}


static void
file_list_item_dispose(FileListItem *item)
{
  fclose(item->file);
}


static FILE *
open_file(const char *filename, int for_writing)
{
  FILE *file = fopen(filename, for_writing ? "w" : "r");

  if (!file) {
    fprintf(stderr, "%s: can't open file `%s' for %s\n",
	    short_program_name, filename,
	    for_writing ? "writing" : "reading");
  }

  return file;
}


static int
do_parse_lists(FILE *h_file, FILE *c_file, const ListDescription *lists)
{
  int k;
  int result = 1;
  int had_c_includes = 0;
  int had_h_includes = 0;
  int in_list = 0;
  int equal_to_last = 0;
  int h_file_line_length = 0;
  int num_c_file_array_elements = 0;
  int pending_linefeeds = 0;
  const char *h_file_enum_name = NULL;
  const char *c_file_array_name = NULL;
  char *last_identifier = NULL;
  char *pending_h_comment = NULL;
  char *pending_c_comment = NULL;
  char *pending_eol_comment = NULL;
  StringBuffer c_file_arrays[NUM_LIST_SORT_ORDERS];
  StringBuffer *list_c_file_array = NULL;
  StringBuffer h_file_enums;

  string_buffer_init(&h_file_top, 0x2000, 0x1000);
  string_buffer_init(&h_file_bottom, 0x2000, 0x1000);
  string_buffer_init(&h_file_enums, 0x2000, 0x1000);

  string_buffer_init(&c_file_top, 0x2000, 0x1000);
  string_buffer_init(&c_file_bottom, 0x2000, 0x1000);
  for (k = 0; k < NUM_LIST_SORT_ORDERS; k++)
    string_buffer_init(&c_file_arrays[k], 0x2000, 0x1000);

  while (1) {
    char *line = read_line();

    if (!line) {
      while (lists->name && lists->multiple_lists_allowed)
	lists++;

      if (!lists->name) {
	result = 0;

	if (lists->list_finalizer) {
	  if (lists->list_finalizer(NULL))
	    result = 1;
	}
      }
      else {
	fprintf(stderr,
		"%s: unexpected end of file--list of type `%s' expected\n",
		short_program_name,
		lists->name);
      }

      break;
    }

    if (! *line)
      continue;

    if (line[0] == '#') {
      if (line[1] == '>') {
	line = line + 2;
	while (isspace(*line))
	  line++;

	utils_free(pending_h_comment);
	pending_h_comment = utils_duplicate_string(line);

	utils_free(pending_c_comment);
	pending_c_comment = utils_duplicate_string(line);
      }

      continue;
    }

    if (in_list) {
      if (line[0] != '}') {
	char first_char = line[0];
	const char *identifier = NULL;

	if (first_char != '=' && first_char != '+') {
	  if (lists->line_parser1) {
	    if (lists->line_parser1(&line))
	      break;

	    if (!line)
	      continue;

	    while (*line && isspace(*line))
	      line++;
	  }
	}
	else {
	  if (!h_file_enum_name) {
	    print_error("`+' and `=' directives are not allowed "
			"in lists that don't generate enumerations");
	    break;
	  }

	  do
	    line++;
	  while (isspace(*line));
	}

	if ((!pending_eol_comment || ! *pending_eol_comment)
	    && last_identifier
	    && h_file_enum_name)
	  string_buffer_cat_string(&h_file_enums, ",\n");

	if (pending_eol_comment) {
	  if (*pending_eol_comment && h_file_enum_name) {
	    string_buffer_printf(&h_file_enums, ",%s/* %s */\n",
				 TABBING(7, h_file_line_length + 1),
				 pending_eol_comment);
	  }

	  utils_free(pending_eol_comment);
	  pending_eol_comment = NULL;
	}

	if (pending_h_comment) {
	  if (*pending_h_comment && h_file_enum_name) {
	    if (last_identifier)
	      string_buffer_add_character(&h_file_enums, '\n');
	    string_buffer_cat_strings(&h_file_enums,
				      "  /* ", pending_h_comment, " */\n",
				      NULL);
	  }

	  utils_free(pending_h_comment);
	  pending_h_comment = NULL;
	}

	if (h_file_enum_name) {
	  identifier = parse_thing(IDENTIFIER, &line, "identifier");
	  if (!identifier)
	    break;

	  string_buffer_cat_strings(&h_file_enums, "  ", identifier, NULL);
	  h_file_line_length = 2 + strlen(identifier);

	  if (first_char == '=' || equal_to_last) {
	    string_buffer_cat_strings(&h_file_enums,
				      " = ", last_identifier, NULL);
	    h_file_line_length += 3 + strlen(last_identifier);
	  }

	  utils_free(last_identifier);
	  last_identifier = utils_duplicate_string(identifier);
	}

	if (first_char != '+') {
	  if (first_char != '=') {
	    if (c_file_array_name && *lists->c_file_array_type) {
	      if (num_c_file_array_elements > 0) {
		string_buffer_add_character(list_c_file_array, ',');
		string_buffer_add_characters(list_c_file_array, '\n',
					     1 + pending_linefeeds);
	      }

	      if (pending_c_comment) {
		if (*pending_c_comment) {
		  if (num_c_file_array_elements > 0)
		    string_buffer_add_character(list_c_file_array, '\n');

		  string_buffer_cat_strings(list_c_file_array,
					    "  /* ", pending_c_comment, " */\n",
					    NULL);
		}

		utils_free(pending_c_comment);
		pending_c_comment = NULL;
	      }
	    }

	    if (c_file_array_name)
	      num_c_file_array_elements++;

	    pending_linefeeds = 0;
	    if (lists->line_parser2(list_c_file_array, &line, identifier,
				    &pending_eol_comment, &pending_linefeeds))
	      break;

	    if (*line) {
	      print_error("unexpected characters at the end of line");
	      break;
	    }

	    if (pending_linefeeds < 0) {
	      pending_linefeeds = 0;
	      if (! *line) {
		while (1) {
		  line = read_line();

		  if (line && ! *line)
		    pending_linefeeds++;
		  else {
		    reuse_last_line(&line);
		    break;
		  }
		}
	      }
	    }
	  }

	  equal_to_last = 0;
	}
	else {
	  if (equal_to_last) {
	    print_error("second inserted identifier in a row; did you mean `='?");
	    break;
	  }

	  equal_to_last = 1;
	}
      }
      else {
	if (!last_identifier && num_c_file_array_elements == 0) {
	  print_error("empty list `%s'", lists->name);
	  break;
	}

	if (pending_eol_comment) {
	  if (*pending_eol_comment && h_file_enum_name) {
	    string_buffer_printf(&h_file_enums, "%s/* %s */",
				 TABBING(7, h_file_line_length),
				 pending_eol_comment);
	  }

	  utils_free(pending_eol_comment);
	  pending_eol_comment = NULL;
	}

	if (lists->list_finalizer) {
	  if (lists->list_finalizer(list_c_file_array))
	    break;
	}

	if (h_file_enum_name) {
	  if (strcmp(h_file_enum_name, "unnamed") != 0) {
	    string_buffer_cat_strings(&h_file_enums,
				      "\n} ", h_file_enum_name, ";\n", NULL);
	  }
	  else
	    string_buffer_cat_string(&h_file_enums, "\n};\n");
	}

	if (c_file_array_name && *lists->c_file_array_type)
	  string_buffer_cat_string(list_c_file_array, "\n};\n");

	if (!lists->multiple_lists_allowed)
	  lists++;

	in_list = 0;
      }
    }
    else {
      const char *identifier = parse_thing(IDENTIFIER, &line,
					   "list name or `include'");

      if (!identifier)
	break;

      if (strcmp(identifier, "include") == 0
	  || strcmp(identifier, "c_include") == 0) {
	if (! *line) {
	  print_error("filename expected");
	  break;
	}

	if (!had_c_includes) {
	  fputs("\n\n", c_file);
	  had_c_includes = 1;
	}

	fprintf(c_file, "#include %s\n", line);
      }
      else if (strcmp(identifier, "h_include") == 0) {
	if (! *line) {
	  print_error("filename expected");
	  break;
	}

	if (!had_h_includes) {
	  fputs("\n\n", h_file);
	  had_h_includes = 1;
	}

	fprintf(h_file, "#include %s\n", line);
      }
      else {
	if (!lists->name) {
	  print_error("unexpected list beginning");
	  break;
	}

	if (lists->multiple_lists_allowed
	    && strcmp(identifier, lists->name) != 0
	    && (lists + 1)->name)
	  lists++;

	if (strcmp(identifier, lists->name) == 0) {
	  if (looking_at("-", &line)) {
	    if (lists->enumeration_required) {
	      print_error("enumeration name expected");
	      break;
	    }

	    h_file_enum_name = NULL;
	  }
	  else {
	    h_file_enum_name = parse_thing(IDENTIFIER, &line,
					   "enumeration name");
	    if (!h_file_enum_name)
	      break;
	  }

	  if (!lists->c_file_array_type) {
	    if (!looking_at("-", &line)) {
	      print_error("unexpected array name");
	      break;
	    }

	    c_file_array_name = NULL;
	  }
	  else {
	    if (looking_at("-", &line)) {
	      print_error("array name expected");
	      break;
	    }

	    c_file_array_name = parse_thing(IDENTIFIER, &line,
					    "array name");
	    if (!c_file_array_name)
	      break;
	  }

	  if (*line != '{') {
	    print_error("list opening brace expected");
	    break;
	  }

	  if (*(line + 1)) {
	    print_error("unexpected characters at the end of line");
	    break;
	  }

	  if (pending_h_comment) {
	    if (*pending_h_comment && h_file_enum_name) {
	      string_buffer_cat_strings(&h_file_enums,
					"/* ", pending_h_comment, " */\n",
					NULL);
	    }

	    utils_free(pending_h_comment);
	    pending_h_comment = NULL;
	  }

	  assert(0 <= lists->sort_order
		 && lists->sort_order <= NUM_LIST_SORT_ORDERS);
	  list_c_file_array = &c_file_arrays[lists->sort_order];

	  if (h_file_enum_name) {
	    if (strcmp(h_file_enum_name, "unnamed") != 0)
	      string_buffer_cat_string(&h_file_enums, "\n\ntypedef enum {\n");
	    else
	      string_buffer_cat_string(&h_file_enums, "\n\nenum {\n");
	  }

	  if (c_file_array_name && *lists->c_file_array_type) {
	    string_buffer_cat_strings(list_c_file_array,
				      "\n\n", lists->c_file_array_type,
				      c_file_array_name, "[] = {\n", NULL);
	  }

	  if (lists->list_initializer) {
	    if (lists->list_initializer(list_c_file_array,
					h_file_enum_name, c_file_array_name))
	      break;
	  }

	  in_list		    = 1;
	  equal_to_last		    = 0;
	  num_c_file_array_elements = 0;
	  pending_linefeeds	    = 1;

	  utils_free(last_identifier);
	  last_identifier = NULL;
	}
	else {
	  print_error("list name `%s' expected, got `%s'",
		      lists->name, identifier);
	  break;
	}
      }
    }
  }

  if (h_file_top.length > 0)
    fwrite(h_file_top.string, h_file_top.length, 1, h_file);

  if (h_file_enums.length > 0)
    fwrite(h_file_enums.string, h_file_enums.length, 1, h_file);

  if (h_file_bottom.length > 0)
    fwrite(h_file_bottom.string, h_file_bottom.length, 1, h_file);

  string_buffer_dispose(&h_file_top);
  string_buffer_dispose(&h_file_bottom);
  string_buffer_dispose(&h_file_enums);

  if (c_file_top.length > 0)
    fwrite(c_file_top.string, c_file_top.length, 1, c_file);

  for (k = 0; k < NUM_LIST_SORT_ORDERS; k++) {
    fwrite(c_file_arrays[k].string, c_file_arrays[k].length, 1, c_file);
    string_buffer_dispose(&c_file_arrays[k]);
  }

  if (c_file_bottom.length > 0)
    fwrite(c_file_bottom.string, c_file_bottom.length, 1, c_file);

  string_buffer_dispose(&c_file_top);
  string_buffer_dispose(&c_file_bottom);

  utils_free(last_identifier);
  utils_free(pending_h_comment);
  utils_free(pending_c_comment);
  utils_free(pending_eol_comment);

  string_list_empty(&lines);

  return result;
}


void
reuse_last_line(char **current_line)
{
  static char empty_line[] = "";

  assert(!last_line_reusable);
  last_line_reusable = 1;

  *current_line = empty_line;
}


void
print_error(const char *format_string, ...)
{
  va_list arguments;

  if (!string_list_is_empty(&list_files)) {
    fprintf(stderr, "%s:%d: ",
	    list_files.first->filename, list_files.first->line_number);
  }

  va_start(arguments, format_string);
  vfprintf(stderr, format_string, arguments);
  va_end(arguments);

  fputc('\n', stderr);
}


char *
read_line(void)
{
  if (!last_line_reusable) {
    int length;
    char *line = NULL;
    char *beginning;
    char *end;

    while (!line && !string_list_is_empty(&list_files)) {
      line = utils_fgets(list_files.first->file, &length);
      if (!line)
	string_list_delete_first_item(&list_files);
    }

    if (!line)
      return NULL;

    list_files.first->line_number++;

    for (beginning = line; *beginning; beginning++) {
      if (!isspace(*beginning))
	break;
    }

    for (end = line + length; end > beginning; end--) {
      if (!isspace(*(end - 1)))
	break;
    }

    *end = 0;
    if (*beginning) {
      if (looking_at("include_list", &beginning)) {
	if (*beginning) {
	  FILE *new_list_file = fopen(beginning, "r");

	  if (new_list_file) {
	    string_list_prepend_from_buffer(&list_files,
					    beginning, end - beginning);
	    list_files.first->file = new_list_file;
	    list_files.first->line_number = 0;
	  }
	  else {
	    print_error("can't open file %s for reading", beginning);
	    string_list_empty(&list_files);
	  }
	}
	else {
	print_error("name of file to include is missing");
	string_list_empty(&list_files);
      }

	utils_free(line);
	return read_line();
      }

      if (looking_at("substitute_value", &beginning)) {
	if (*beginning) {
	  char *substitution
	    = association_list_find_association(&substitutions, beginning);

	  if (!substitution) {
	    print_error("undefined substitution symbol `%s'", beginning);
	    string_list_empty(&list_files);
	    utils_free(line);

	    return NULL;
	  }

	  beginning = substitution;
	  end = substitution + strlen(substitution);
	}
	else {
	  print_error("substitution symbol expected");
	  string_list_empty(&list_files);
	  utils_free(line);

	  return NULL;
	}
      }
    }

    string_list_add_from_buffer(&lines, beginning, end - beginning);
    utils_free(line);
  }

  last_line_reusable = 0;
  return lines.last->text;
}


const char *
parse_thing(Thing thing, char **line, const char *type)
{
  const char *value;

  while (*line && ! **line)
    *line = read_line();

  if (! *line) {
    print_error("%s expected", type);
    return NULL;
  }

  value = *line;

  if (thing != STRING && thing != STRING_OR_NULL
      && (thing != STRING_OR_IDENTIFIER || **line != '"')) {
    do {
      int expected_character;

      switch (thing) {
      case IDENTIFIER:
      case STRING_OR_IDENTIFIER:
	expected_character = (isalpha(**line) || **line == '_'
			      || (*line != value && isdigit(**line)));
	break;

      case PROPERTY_IDENTIFIER:
	expected_character = isupper(**line);
	break;

      case FIELD_NAME:
	expected_character = (isalpha(**line)
			      || **line == '_' || ** line == '['
			      || (*line != value && (isdigit(**line)
						     || **line == '.'
						     || **line == ']')));
	break;

      case INTEGER_NUMBER:
	expected_character = isdigit(**line);
	break;

      case FLOATING_POINT_NUMBER:
	expected_character = isdigit(**line) || **line == '.';
	break;

      case TIME_VALUE:
	expected_character = isdigit(**line) || **line == ':' || **line == '.';
	break;

      default:
	assert(0);
      }

      if (!expected_character) {
	print_error("unexpected character '%c' in %s", **line, type);
	return NULL;
      }

      (*line)++;
    } while (**line && !isspace(**line));
  }
  else {
    if (thing == STRING_OR_NULL) {
      char *possible_null = *line;

      if (looking_at("NULL", line)) {
	*(possible_null + 4) = 0;
	return possible_null;
      }
    }

    if (**line != '"') {
      print_error("string%s expected",
		  (thing == STRING ? "" : (thing == STRING_OR_NULL
					   ? " or NULL" : "or identifier")));
      return NULL;
    }

    while (1) {
      (*line)++;

      if (! **line || (**line == '\\' && ! *(*line + 1))) {
	print_error("unterminated string");
	return NULL;
      }

      if (**line == '"') {
	(*line)++;
	break;
      }

      if (**line == '\\')
	(*line)++;
    }
  }

  if (**line) {
    **line = 0;
    do
      (*line)++;
    while (isspace(**line));
  }

  return value;
}


char *
parse_multiline_string(char **line, const char *type,
		       const char *line_separator, int null_allowed)
{
  const char *string_chunk;
  char *string;

  string_chunk = parse_thing(null_allowed ? STRING_OR_NULL : STRING,
			     line, type);
  if (!string_chunk)
    return NULL;

  string = utils_duplicate_string(string_chunk);
  if (*string != '"')
    return string;

  while (! **line) {
    *line = read_line();

    if (! *line) {
      utils_free(string);
      return NULL;
    }

    if (**line == '"') {
      string_chunk = parse_thing(STRING, line, type);
      if (!string_chunk) {
	utils_free(string);
	return NULL;
      }

      string = utils_cat_strings(string, line_separator, string_chunk, NULL);
    }
    else {
      reuse_last_line(line);
      break;
    }
  }

  return string;
}


int
parse_color(char **line, QuarryColor *color, const char *type)
{
  int num_digits;
  int red;
  int green;
  int blue;

  while (*line && ! **line)
    *line = read_line();

  if (! *line || * (*line)++ != '#') {
    print_error("%s expected", type);
    return 0;
  }

  for (num_digits = 0; isxdigit(**line) && num_digits <= 6; num_digits++)
    (*line)++;

  if ((num_digits != 6 && num_digits != 3) || (**line && !isspace(**line))) {
    print_error("%s expected", type);
    return 0;
  }

  if (num_digits == 6)
    sscanf((*line) - 6, "%2x%2x%2x", &red, &green, &blue);
  else {
    sscanf((*line) - 3, "%1x%1x%1x", &red, &green, &blue);
    red   *= 0x11;
    green *= 0x11;
    blue  *= 0x11;
  }

  while (isspace(**line))
    (*line)++;

  color->red   = red;
  color->green = green;
  color->blue  = blue;

  return 1;
}


int
looking_at(const char *what, char **line)
{
  int length = strlen(what);

  while (*line && ! **line)
    *line = read_line();

  if (*line && strncmp(*line, what, length) == 0
      && (!(*line) [length] || isspace((*line) [length]))) {
    *line += length;
    while (isspace(**line))
      (*line)++;

    return 1;
  }

  return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
