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


#include "utils.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


static void	reallocate_if_needed (StringBuffer *string_buffer,
				      int length_increase);


StringBuffer *
string_buffer_new (int initial_size, int size_increment)
{
  StringBuffer *string_buffer = utils_malloc (sizeof (StringBuffer));

  string_buffer_init (string_buffer, initial_size, size_increment);
  return string_buffer;
}


void
string_buffer_init (StringBuffer *string_buffer,
		    int initial_size, int size_increment)
{
  string_buffer->string	   = utils_malloc (initial_size);
  string_buffer->string[0] = 0;
  string_buffer->length	   = 0;

  string_buffer->current_size = initial_size;
  string_buffer->size_increment = size_increment;
}


void
string_buffer_dispose (StringBuffer *string_buffer)
{
  assert (string_buffer);

  utils_free (string_buffer->string);
}


void
string_buffer_empty (StringBuffer *string_buffer)
{
  string_buffer->string[0] = 0;
  string_buffer->length = 0;
}


void
string_buffer_delete (StringBuffer *string_buffer)
{
  string_buffer_dispose (string_buffer);
  utils_free (string_buffer);
}


void
string_buffer_add_characters (StringBuffer *string_buffer,
			      char character, int num_characters)
{
  reallocate_if_needed (string_buffer, num_characters);

  memset (string_buffer->string + string_buffer->length,
	  character, num_characters);
  string_buffer->length += num_characters;
  string_buffer->string[string_buffer->length] = 0;
}


void
string_buffer_cat_string (StringBuffer *string_buffer, const char *string)
{
  int length;

  assert (string_buffer);
  assert (string);

  length = strlen (string);
  reallocate_if_needed (string_buffer, length);

  memcpy (string_buffer->string + string_buffer->length, string, length + 1);
  string_buffer->length += length;
}


void
string_buffer_cat_strings (StringBuffer *string_buffer, ...)
{
  int k;
  int to_cat_lengths[16];
  int length_increase = 0;
  va_list arguments;

  assert (string_buffer);

  va_start (arguments, string_buffer);
  for (k = 0; ;) {
    int length;
    const char *to_cat = va_arg (arguments, const char *);

    if (!to_cat)
      break;

    length = strlen (to_cat);
    if (k < (int) (sizeof to_cat_lengths / sizeof (int)))
      to_cat_lengths[k++] = length;

    length_increase += length;
  }

  va_end (arguments);

  reallocate_if_needed (string_buffer, length_increase);

  va_start (arguments, string_buffer);
  for (k = 0; ;) {
    int length;
    const char *to_cat = va_arg (arguments, const char *);

    if (!to_cat)
      break;

    length = (k < (int) (sizeof to_cat_lengths / sizeof (int))
	      ? to_cat_lengths[k++] : (int) strlen (to_cat));
    memcpy (string_buffer->string + string_buffer->length, to_cat, length);
    string_buffer->length += length;
  }

  va_end (arguments);

  string_buffer->string[string_buffer->length] = 0;
}


void
string_buffer_cat_as_string (StringBuffer *string_buffer,
			     const char *buffer, int length)
{
  assert (string_buffer);
  assert (buffer);

  reallocate_if_needed (string_buffer, length);

  memcpy (string_buffer->string + string_buffer->length, buffer, length);
  string_buffer->length += length;
  string_buffer->string[string_buffer->length] = 0;
}


void
string_buffer_cat_as_strings (StringBuffer *string_buffer, ...)
{
  int length_increase = 0;
  va_list arguments;

  assert (string_buffer);

  va_start (arguments, string_buffer);
  while (va_arg (arguments, const char *))
    length_increase += va_arg (arguments, int);

  va_end (arguments);

  reallocate_if_needed (string_buffer, length_increase);

  va_start (arguments, string_buffer);
  while (1) {
    int length;
    const char *buffer = va_arg (arguments, const char *);

    if (!buffer)
      break;

    length = va_arg (arguments, int);
    memcpy (string_buffer->string + string_buffer->length, buffer, length);
    string_buffer->length += length;
  }

  va_end (arguments);

  string_buffer->string[string_buffer->length] = 0;
}


void
string_buffer_printf (StringBuffer *string_buffer,
		      const char *format_string, ...)
{
  va_list arguments;

  va_start (arguments, format_string);
  string_buffer_vprintf (string_buffer, format_string, arguments);
  va_end (arguments);
}


void
string_buffer_vprintf (StringBuffer *string_buffer,
		       const char *format_string, va_list arguments)
{
  assert (string_buffer);
  assert (format_string);

  while (1) {
    int length;
    va_list arguments_copy;

    QUARRY_VA_COPY (arguments_copy, arguments);
    length = vsnprintf (string_buffer->string + string_buffer->length,
			string_buffer->current_size - string_buffer->length,
			format_string, arguments_copy);
    va_end (arguments_copy);

    if (length < string_buffer->current_size - string_buffer->length
	&& length != -1) {
      string_buffer->length += length;
      break;
    }

    reallocate_if_needed (string_buffer,
			  (length > -1
			   ? length : string_buffer->size_increment));
  }
}


void
string_buffer_cprintf (StringBuffer *string_buffer,
		       const char *format_string, ...)
{
  va_list arguments;

  va_start (arguments, format_string);
  string_buffer_vcprintf (string_buffer, format_string, arguments);
  va_end (arguments);
}


void
string_buffer_vcprintf (StringBuffer *string_buffer,
			const char *format_string, va_list arguments)
{
  int length;
  va_list arguments_copy;

  assert (string_buffer);
  assert (format_string);

  QUARRY_VA_COPY (arguments_copy, arguments);
  length = utils_vncprintf (string_buffer->string + string_buffer->length,
			    (string_buffer->current_size
			     - string_buffer->length),
			    format_string, arguments_copy);
  va_end (arguments_copy);

  if (length >= string_buffer->current_size - string_buffer->length) {
    reallocate_if_needed (string_buffer, length);
    length = utils_vncprintf (string_buffer->string + string_buffer->length,
			      (string_buffer->current_size
			       - string_buffer->length),
			      format_string, arguments);
  }

  string_buffer->length += length;
}


static void
reallocate_if_needed (StringBuffer *string_buffer, int length_increase)
{
  int new_size = string_buffer->length + length_increase + 1;

  if (new_size > string_buffer->current_size) {
    int num_increments = (((new_size - string_buffer->current_size)
			   + (string_buffer->size_increment - 1))
			  / string_buffer->size_increment);

    string_buffer->current_size += (num_increments
				    * string_buffer->size_increment);
    string_buffer->string = utils_realloc (string_buffer->string,
					   string_buffer->current_size);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
