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

/* This module provides a buffered file writer with a few useful
 * features:
 *
 * - Allows on-the-fly re-encoding of incoming text.  Moreover,
 *   required encoding can change arbitrary between writes to the
 *   buffer (use buffered_writer_set_iconv_handle() macro to change
 *   it).  Incoming text must come in UTF-8.
 *
 * - Tracks current column in the output stream, thus making output
 *   formatting easier for higher level code.
 */


#include "utils.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

/* For memrchr(), if it is present at all. */
#define __USE_GNU
#include <string.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


static void	flush_buffer(BufferedWriter *writer);
static void	update_column(BufferedWriter *writer,
			      const char *buffer, int length);


int
buffered_writer_init(BufferedWriter *writer,
		     const char *filename, int buffer_size)
{
  assert(writer);
  assert(buffer_size > MB_LEN_MAX);

  if (filename) {
    writer->file = fopen(filename, "w");
    if (!writer->file)
      return 0;
  }
  else
    writer->file = stdout;

  writer->buffer	 = utils_malloc(buffer_size);
  writer->buffer_pointer = writer->buffer;
  writer->buffer_end	 = writer->buffer + buffer_size;

  writer->iconv_handle = NULL;

  writer->column = 0;

  writer->successful = 1;

  return 1;
}


int
buffered_writer_dispose(BufferedWriter *writer)
{
  assert(writer);

  if (writer->buffer_pointer != writer->buffer)
    flush_buffer(writer);

  if (writer->file != stdout)
    fclose(writer->file);

  utils_free(writer->buffer);

  return writer->successful;
}


void
buffered_writer_add_character(BufferedWriter *writer, char character)
{
  assert(writer);

  /* The character must not be a multi-byte UTF-8 character. */
  assert(!(character & 0x80) && character != '\n');

  if (!writer->iconv_handle)
    *writer->buffer_pointer++ = character;
  else {
    char *input_text = &character;
    int input_bytes_left = 1;
    int output_bytes_left = writer->buffer_end - writer->buffer_pointer;

    iconv(writer->iconv_handle,
	  &input_text, &input_bytes_left,
	  &writer->buffer_pointer, &output_bytes_left);

    if (input_bytes_left) {
      flush_buffer(writer);

      iconv(writer->iconv_handle,
	    &input_text, &input_bytes_left,
	    &writer->buffer_pointer, &output_bytes_left);
    }
  }

  if (writer->buffer_pointer == writer->buffer_end)
    flush_buffer(writer);

  writer->column++;
  if (character == '\t')
    writer->column = ROUND_UP(writer->column, 8);
}


/* FIXME: Output system-specific line terminator. */
void
buffered_writer_add_newline(BufferedWriter *writer)
{
  assert(writer);

  if (!writer->iconv_handle)
    *writer->buffer_pointer++ = '\n';
  else {
    char input_character = '\n';
    char *input_text = &input_character;
    int input_bytes_left = 1;
    int output_bytes_left = writer->buffer_end - writer->buffer_pointer;

    iconv(writer->iconv_handle,
	  &input_text, &input_bytes_left,
	  &writer->buffer_pointer, &output_bytes_left);

    if (input_bytes_left) {
      flush_buffer(writer);

      iconv(writer->iconv_handle,
	    &input_text, &input_bytes_left,
	    &writer->buffer_pointer, &output_bytes_left);
    }
  }

  if (writer->buffer_pointer == writer->buffer_end)
    flush_buffer(writer);

  writer->column = 0;
}


void
buffered_writer_cat_string(BufferedWriter *writer, const char *string)
{
  buffered_writer_cat_as_string(writer, string, strlen(string));
}


void
buffered_writer_cat_strings(BufferedWriter *writer, ...)
{
  va_list arguments;
  const char *string;

  va_start(arguments, writer);

  while ((string = va_arg(arguments, const char *)) != NULL)
    buffered_writer_cat_as_string(writer, string, strlen(string));

  va_end(arguments);
}


void
buffered_writer_cat_as_string(BufferedWriter *writer,
			      const char *buffer, int length)
{
  assert(writer);
  assert(buffer);

  update_column(writer, buffer, length);

  if (!writer->iconv_handle) {
    while (length > 0) {
      int chunk_size = MIN(length,
			   writer->buffer_end - writer->buffer_pointer);

      memcpy(writer->buffer_pointer, buffer, chunk_size);

      writer->buffer_pointer += chunk_size;
      if (writer->buffer_pointer == writer->buffer_end)
	flush_buffer(writer);

      buffer += chunk_size;
      length -= chunk_size;
    }
  }
  else {
    while (length > 0) {
      int output_bytes_left = writer->buffer_end - writer->buffer_pointer;

      /* Dumb <iconv.h> doesn't apply `const' to input buffer?!  This
       * is nasty, but a warning for nothing is even worse.
       */
      iconv(writer->iconv_handle,
	    (char **) (void *) &buffer, &length,
	    &writer->buffer_pointer, &output_bytes_left);

      if (writer->buffer_pointer == writer->buffer_end)
	flush_buffer(writer);
    }
  }
}


void
buffered_writer_cat_as_strings(BufferedWriter *writer, ...)
{
  va_list arguments;
  const char *buffer;

  va_start(arguments, writer);

  while ((buffer = va_arg(arguments, const char *)) != NULL)
    buffered_writer_cat_as_string(writer, buffer, va_arg(arguments, int));

  va_end(arguments);
}


void
buffered_writer_printf(BufferedWriter *writer, const char *format_string, ...)
{
  va_list arguments;

  va_start(arguments, format_string);
  buffered_writer_vprintf(writer, format_string, arguments);
  va_end(arguments);
}


void
buffered_writer_vprintf(BufferedWriter *writer,
			const char *format_string, va_list arguments)
{
  va_list arguments_copy;
  int characters_written;
  char *string;

  assert(writer);
  assert(format_string);

  if (!writer->iconv_handle
      && (writer->buffer_end - writer->buffer_pointer
	  >= strlen(format_string))) {
    /* No encoding conversion is requested.  Try to print the string
     * directly in the buffer.
     */
    QUARRY_VA_COPY(arguments_copy, arguments);
    characters_written = vsnprintf(writer->buffer_pointer,
				   writer->buffer_end - writer->buffer_pointer,
				   format_string, arguments_copy);
    va_end(arguments_copy);

    if (characters_written < writer->buffer_end - writer->buffer_pointer
	&& characters_written != -1) {
      update_column(writer, writer->buffer_pointer, characters_written);
      writer->buffer_pointer += characters_written;

      return;
    }
  }

  string = utils_vprintf(format_string, arguments);
  buffered_writer_cat_string(writer, string);
  utils_free(string);
}


void
buffered_writer_cprintf(BufferedWriter *writer, const char *format_string, ...)
{
  va_list arguments;

  va_start(arguments, format_string);
  buffered_writer_vcprintf(writer, format_string, arguments);
  va_end(arguments);
}


void
buffered_writer_vcprintf(BufferedWriter *writer,
			 const char *format_string, va_list arguments)
{
  char *string;

  assert(writer);
  assert(format_string);

  if (!writer->iconv_handle
      && (writer->buffer_end - writer->buffer_pointer
	  >= strlen(format_string))) {
    int characters_written;
    va_list arguments_copy;
    
    /* No encoding conversion is requested.  Try to print the string
     * directly in the buffer.
     */
    QUARRY_VA_COPY(arguments_copy, arguments);
    characters_written = utils_vncprintf(writer->buffer_pointer,
					 (writer->buffer_end
					  - writer->buffer_pointer),
					 format_string, arguments_copy);
    va_end(arguments_copy);

    if (characters_written < writer->buffer_end - writer->buffer_pointer) {
      update_column(writer, writer->buffer_pointer, characters_written);
      writer->buffer_pointer += characters_written;

      return;
    }
  }

  string = utils_vcprintf(format_string, arguments);
  buffered_writer_cat_string(writer, string);
  utils_free(string);
}


static void
flush_buffer(BufferedWriter *writer)
{
  if (writer->successful) {
    writer->successful = fwrite(writer->buffer,
				writer->buffer_pointer - writer->buffer, 1,
				writer->file);
    writer->buffer_pointer = writer->buffer;
  }
}


static void
update_column(BufferedWriter *writer, const char *buffer, int length)
{
  const char *last_line;
  const char *buffer_end = buffer + length;

#ifdef HAVE_MEMRCHR

  last_line = memrchr(buffer, '\n', length);
  if (last_line) {
    writer->column = 0;
    last_line++;
  }
  else
    last_line = buffer;

#else

  int k;

  for (k = length; --k >= 0; k++) {
    if (buffer[k] == '\n') {
      writer->column = 0;
      break;
    }
  }

  last_line = buffer + (k + 1);

#endif

  while (last_line < buffer_end) {
    if (IS_UTF8_STARTER(*last_line)) {
      writer->column++;
      if (*last_line == '\t')
	writer->column = ROUND_UP(writer->column, 8);
    }

    last_line++;
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
