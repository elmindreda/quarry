/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003 Paul Pogonyshev.                             *
 * Copyright (C) 2004 Paul Pogonyshev and Martin Holters.          *
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


#include "utils.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_FLOAT_H
#include <float.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


#define FORMAT_DOUBLE_DECIMALS	6


#if ENABLE_MEMORY_PROFILING

static unsigned int	num_mallocs = 0;
static unsigned int	num_reallocs = 0;
static unsigned int	num_frees = 0;

static unsigned int	num_bytes_allocated = 0;
static unsigned int	num_bytes_on_heap = 0;

#endif


char	       *full_program_name = NULL;
char	       *short_program_name = NULL;
char	       *program_directory = NULL;


/* Call malloc() and check that it succeeds.  Panic if it does not.
 * This function can also log some profiling information when
 * ENABLE_MEMORY_PROFILING is nonzero.
 *
 * This is a good place to store critical information (if any) when
 * out of memory.  No critical information to store at present,
 * though, so this is not implemented.
 */
void *
utils_malloc(size_t size)
{
  void *pointer = malloc(size);

#if ENABLE_MEMORY_PROFILING

  num_mallocs++;
  num_bytes_allocated += size;
  num_bytes_on_heap += (sizeof(void *)
			+ (((size + sizeof(int) - 1) / sizeof(int))
			   * sizeof(int)));

#endif

  if (pointer)
    return pointer;

  fprintf(stderr, ("%s: fatal error: out of memory "
		   "(failed to allocate %d bytes)\n"),
	  short_program_name, size);
  exit(255);
}


/* Convenience function.  Allocate memory chunk and initialize all its
 * bytes to zero.
 */
void *
utils_malloc0(size_t size)
{
  void *pointer = utils_malloc(size);

  memset(pointer, 0, size);
  return pointer;
}


/* Same as utils_malloc(), but based on realloc() instead of malloc().
 *
 * When memory profiling is enabled, utils_realloc(NULL, ...) and
 * utils_realloc(..., 0) calls are counted as calls to utils_malloc()
 * and utils_free() respectively.
 */
void *
utils_realloc(void *pointer, size_t size)
{
#if ENABLE_MEMORY_PROFILING

  if (pointer && size)
    num_reallocs++;
  else {
    if (!pointer)
      num_mallocs++;
    else
      num_frees++;
  }

#endif

  pointer = realloc(pointer, size);
  if (pointer)
    return pointer;

  fprintf(stderr, ("%s: fatal error: out of memory "
		   "(failed to allocate %d bytes)\n"),
	  short_program_name, size);
  exit(255);
}


#if ENABLE_MEMORY_PROFILING


/* Free given memory block and log profiling information. */
void
utils_free(void *pointer)
{
  if (pointer) {
    num_frees++;
    free(pointer);
  }
}


/* Dump profiling information and warn if any allocated memory has not
 * been freed.
 */
void
utils_print_memory_profiling_info(void)
{
  fputs("Memory profiling information:\n\n", stderr);
  fprintf(stderr, ("Numbers of calls:\n  utils_malloc():  %10u\n"
		   "  utils_realloc(): %10u\n  utils_free():    %10u\n\n"),
	  num_mallocs, num_reallocs, num_frees);

  if (num_mallocs > num_frees) {
    fprintf(stderr, "%u memory blocks were not freed!\n\n",
	    num_mallocs - num_frees);
  }

  fprintf(stderr, ("Total %u bytes allocated\n"
		   "Guessed total dynamic memory footprint: %u bytes\n\n"),
	  num_bytes_allocated, num_bytes_on_heap);

#if ENABLE_MEMORY_POOLS

  fprintf(stderr, "Total %d memory pools initialized, %d flushed\n\n",
	  num_pools_initialized, num_pools_flushed);

  if (num_pools_initialized > num_pools_flushed) {
    fprintf(stderr, "%d memory pools were not flushed!\n\n",
	    num_pools_initialized - num_pools_flushed);
  }

#endif
}


#endif /* ENABLE_MEMORY_PROFILING */



/* Allocate a copy of given string on heap.  Analogous to strdup(),
 * which does not belong to most accepted standards (neither ASCII C,
 * nor POSIX).
 */
char *
utils_duplicate_string(const char *string)
{
  if (string) {
    int length = strlen(string);
    char *new_string = utils_malloc(length + 1);

    memcpy(new_string, string, length + 1);
    return new_string;
  }

  return NULL;
}


/* Allocate a copy of given buffer (pointer + length) on heap. */
void *
utils_duplicate_buffer(const void *buffer, int length)
{
  char *buffer_copy = utils_malloc(length);

  memcpy(buffer_copy, buffer, length);
  return buffer_copy;
}


/* Similar to utils_duplicate_buffer(), but also appends a zero byte
 * to the copy, thus making it a valid ASCIIZ string.
 */
char *
utils_duplicate_as_string(const char *buffer, int length)
{
  char *string = utils_malloc(length + 1);

  memcpy(string, buffer, length);
  string[length] = 0;

  return string;
}


/* Append second string to first.  The first string is reallocated to
 * fit the concatenation.  It must be a result of a previous heap
 * allocation or NULL.  In the latter case, this function behaves just
 * like `utils_duplicate_string(to_cat)'.
 */
char *
utils_cat_string(char *string, const char *to_cat)
{
  int current_length = (string ? strlen(string) : 0);
  int to_cat_length = strlen(to_cat);
  char *new_string = utils_realloc(string, current_length + to_cat_length + 1);

  memcpy(new_string + current_length, to_cat, to_cat_length + 1);
  return new_string;
}


/* Append one or more strings to a given `string'.  Last parameter
 * must be NULL (serves as parameter list terminator).
 *
 * Amount of space needed to store the concatenation of all strings is
 * calculated dynamically and `string' is reallocated accordingly.  If
 * `string' is NULL, all the rest of strings are concatenated and are
 * stored in a newly allocated buffer.
 */
char *
utils_cat_strings(char *string, ...)
{
  int k;
  int current_length = (string ? strlen(string) : 0);
  int to_cat_lengths[16];
  int length_increase = 0;
  char *new_string;
  va_list arguments;

  va_start(arguments, string);
  for (k = 0; ;) {
    int length;
    const char *to_cat = va_arg(arguments, const char *);

    if (!to_cat)
      break;

    length = strlen(to_cat);
    if (k < (int) (sizeof(to_cat_lengths) / sizeof(int)))
      to_cat_lengths[k++] = length;

    length_increase += length;
  }

  va_end(arguments);

  new_string = utils_realloc(string, current_length + length_increase + 1);

  va_start(arguments, string);
  for (k = 0; ;) {
    int length;
    const char *to_cat = va_arg(arguments, const char *);

    if (!to_cat)
      break;

    length = (k < (int) (sizeof(to_cat_lengths) / sizeof(int))
	      ? to_cat_lengths[k++] : (int) strlen(to_cat));
    memcpy(new_string + current_length, to_cat, length);
    current_length += length;
  }

  va_end(arguments);

  new_string[current_length] = 0;
  return new_string;
}


/* Same as utils_cat_string(), but the second parameter is any buffer
 * and need not be a zero-terminated string.  Result is still a valid
 * string, of course.
 *
 * The `string' can be NULL, in which case calling this function is
 * identical to calling `utils_duplicate_as_string(buffer, length)'.
 */
char *
utils_cat_as_string(char *string, const char *buffer, int length)
{
  int current_length = (string ? strlen(string) : 0);
  char *new_string = utils_realloc(string, current_length + length + 1);

  memcpy(new_string + current_length, buffer, length);
  new_string[current_length + length] = 0;

  return new_string;
}


/* Similar to utils_cat_strings() except that all parameters except
 * the first might pointer to non-terminated buffers (not necessarily
 * ASCIIZ strings).  Each buffer parameter, except for terminating
 * NULL, must be followed by its length.
 *
 * As for all other `utils_cat_*' functions, `string' can be NULL.
 */
char *
utils_cat_as_strings(char *string, ...)
{
  int current_length = (string ? strlen(string) : 0);
  int length_increase = 0;
  char *new_string;
  va_list arguments;

  va_start(arguments, string);
  while (va_arg(arguments, const char *))
    length_increase += va_arg(arguments, int);

  va_end(arguments);

  new_string = utils_realloc(string, current_length + length_increase + 1);

  va_start(arguments, string);
  while (1) {
    int length;
    const char *buffer = va_arg(arguments, const char *);

    if (!buffer)
      break;

    length = va_arg(arguments, int);
    memcpy(new_string + current_length, buffer, length);
    current_length += length;
  }

  va_end(arguments);

  new_string[current_length] = 0;
  return new_string;
}



/* Store program name for future use (on heap).  This function
 * initializes `full_program_name', `short_program_name' and
 * `program_directory' variables.
 *
 * FIXME: DIRECTORY_SEPARATOR is still always '/'.
 */
void
utils_remember_program_name(const char *argv0)
{
  int short_name_pos;
  int length;

  length = strlen(argv0);
  for (short_name_pos = length; short_name_pos >= 0; short_name_pos--) {
    if (argv0[short_name_pos] == DIRECTORY_SEPARATOR)
      break;
  }

  short_name_pos++;

  full_program_name  = utils_duplicate_as_string(argv0, length);
  short_program_name = utils_duplicate_as_string(argv0 + short_name_pos,
						 length - short_name_pos);
  program_directory  = utils_duplicate_as_string(argv0, short_name_pos);
}


/* Convenience function.  Free the three program name strings stored
 * by utils_remember_program_name().  This can safely be called
 * without really storing them, though there is probably no reason not
 * to do that in the first place.
 */
void
utils_free_program_name_strings(void)
{
  utils_free(full_program_name);
  utils_free(short_program_name);
  utils_free(program_directory);
}



char *
utils_printf(const char *format_string, ...)
{
  char *string;
  va_list arguments;

  va_start(arguments, format_string);
  string = utils_vprintf(format_string, arguments);
  va_end(arguments);

  return string;
}


char *
utils_vprintf(const char *format_string, va_list arguments)
{
  char buffer[0x1000];
  char *string = NULL;
  int length;
  va_list arguments_copy;

  QUARRY_VA_COPY(arguments_copy, arguments);
  length = vsnprintf(buffer, sizeof(buffer), format_string, arguments_copy);
  va_end(arguments_copy);

  if (length < (int) sizeof(buffer))
    return utils_duplicate_as_string(buffer, length);

  if (length <= -1)
    length = 2 * sizeof(buffer) - 1;

  while (1) {
    int required_length;

    string = utils_realloc(string, length);

    QUARRY_VA_COPY(arguments_copy, arguments);
    required_length = vsnprintf(string, length, format_string, arguments_copy);
    va_end(arguments_copy);

    if (-1 < required_length && required_length < length)
      return string;

    length = (required_length > -1 ? required_length + 1 : 2 * length);
  }
}


char *
utils_special_printf(const char *format_string, ...)
{
  char *string;
  va_list arguments;

  va_start(arguments, format_string);
  string = utils_special_vprintf(format_string, arguments);
  va_end(arguments);

  return string;
}


char *
utils_special_vprintf(const char *format_string, va_list arguments)
{
  char *string = NULL;
  const char *chunk_beginning = format_string;
  const char *string_scan = format_string;

  do {
    if (*string_scan == '%' || ! *string_scan) {
      if (string_scan > chunk_beginning) {
	if (string) {
	  string = utils_cat_as_string(string, chunk_beginning,
				       string_scan - chunk_beginning);
	}
	else {
	  string = utils_duplicate_as_string(chunk_beginning,
					     string_scan - chunk_beginning);
	}
      }

      if (*string_scan == '%' && * ++string_scan) {
	va_list arguments_copy;
	int format_character;

	chunk_beginning = string_scan;

	QUARRY_VA_COPY(arguments_copy, arguments);
	while ((format_character = va_arg(arguments_copy, int)) != 0) {
	  const char *substitution = va_arg(arguments_copy, const char *);

	  if (format_character == (int) *string_scan) {
	    if (substitution)
	      string = utils_cat_string(string, substitution);

	    chunk_beginning++;

	    break;
	  }
	}

	va_end(arguments_copy);
      }
    }
  } while (*string_scan++);

  return string;
}



/* Read a line from file and allocate it dynamically as a single
 * string.  If `length' is not NULL, it will be set to the length of
 * the resulting line.
 *
 * If the function is not able to read a single byte, NULL is returned
 * and `*length' is set to zero.
 */
char *
utils_fgets(FILE *file, int *length)
{
  char buffer[0x1000];
  char *string = NULL;

  if (length)
    *length = 0;

  while (fgets(buffer, sizeof(buffer), file)) {
    int chunk_length = strlen(buffer);

    if (!string)
      string = utils_duplicate_as_string(buffer, chunk_length);
    else
      string = utils_cat_as_string(string, buffer, chunk_length);

    if (length)
      *length += chunk_length;

    if (buffer[chunk_length - 1] == '\n')
      break;
  }

  return string;
}



/* A function suitable for passing as fourth argument to qsort() for
 * sorting array of `int's in ascending order.
 */
int
utils_compare_ints(const void *first_int, const void *second_int)
{
  return * (const int *) first_int - * (const int *) second_int;
}



/* Format a double number, so that it doesn't have trailing zeros
 * after decimal point (with the only exception of an integer number).
 * So, 1.2 would get formatted as "1.2", not "1.20", but 1 would be
 * represented as "1.0", not "1.".  At most FORMAT_DOUBLE_DECIMALS
 * digits after the decimal point are produced.  Locale is not
 * considered.
 *
 * The function returns address of a static buffer, so if you need to
 * save the value, copy it away.
 */
const char *
utils_format_double(double value)
{
#ifdef DBL_MAX_10_EXP

  /* 20 is a safety margin. */
  static char buffer[1 + DBL_MAX_10_EXP + 1 + FORMAT_DOUBLE_DECIMALS + 20];

#else

  static char buffer[0x400];

#endif

  char *buffer_pointer = buffer;
  int shift;
  int leading_digit;

  /* Start with a '-' if value is negative. */
  if(value < 0) {
    *buffer_pointer++ = '-';
    value = -value;
  }

  /* Now we can limit ourselves to positive values. */

  /* After this addition, proper rounding becomes simple
   * truncation.
   */
  value += 0.5 * pow(10, -FORMAT_DOUBLE_DECIMALS);

  if (value >= 1.0) {
    shift  = floor(log10(value));
    value /= pow(10, (double) shift);
  }
  else
    shift = 0;

  /* Now value is less than 10.0. */

  do {
    leading_digit = floor(value);
    *buffer_pointer++ = (char) leading_digit + '0';

    if (shift-- == 0)
      *buffer_pointer++ = '.';

    value = (value - leading_digit) * 10.0;
  } while ((value * pow(10, (double) FORMAT_DOUBLE_DECIMALS + shift) >= 1.0
	    && shift >= -FORMAT_DOUBLE_DECIMALS)
	   || shift >= -1);

  *buffer_pointer = 0;
  return buffer;
}



/*
 * Parse a double number in a locale independent way. The string may
 * contain an optional '-', followed by digits, optionally followed by
 * a '.', optionally followed by more digits. If the first character
 * other then those is not '\0', a parsing error is assumed and false
 * is returned. Otherwise, true is returned. Parsing an empty string
 * produces 0.0 and returns true.
 */
int
utils_parse_double(const char *float_string, double *result)
{
  const char *scan = float_string;
  int is_negative = 0;

  *result = 0.0;

  if (*scan == '-') {
    is_negative = 1;
    scan++;
  }

  while (*scan >= '0' && *scan <= '9')
    *result = *result * 10 + (double) (*scan++ - '0');

  if (*(scan++) == '.') {
    double factor = 0.1;
    while (*scan >= '0' && *scan <= '9') {
      *result += (double) (*(scan++) - '0') * factor;
      factor /= 10.0;
    }
  }

  if (is_negative)
    *result = - *result;

  return *scan == '\0';
}


int
utils_parse_time(const char *time_string)
{
  const char *scan;
  int num_colons = 0;
  int digits_and_colons_only = 1;

  for (scan = time_string; *scan; scan++) {
    if (*scan == ':') {
      if (++num_colons > 2 || !digits_and_colons_only)
	return -1;
    }
    else if (*scan < '0' || '9' < *scan) {
      digits_and_colons_only = 0;
      if (num_colons != 0)
	return -1;
    }
  }

  if (num_colons > 0) {
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int num_chars_eaten;

    if (num_colons == 2) {
      if (*time_string != ':') {
	sscanf(time_string, "%d:%n", &hours, &num_chars_eaten);
	time_string += num_chars_eaten;
      }
      else
	time_string++;
    }

    if (*time_string != ':') {
      sscanf(time_string, "%d:%n", &minutes, &num_chars_eaten);
      time_string += num_chars_eaten;
    }
    else
      time_string++;

    sscanf(time_string, "%d", &seconds);

    return (hours * 60 + minutes) * 60 + seconds;
  }
  else {
    char *minutes_end;
    double minutes_double = strtod(time_string, &minutes_end);

    if (minutes_end == time_string
	|| minutes_double < 0.0 || minutes_double > (INT_MAX / 60.0) - 1.0)
      return -1;

    return (int) (60.0 * minutes_double + 0.5);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
