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


#include "utils.h"

#include <stdio.h>

#if HAVE_FLOAT_H
#include <float.h>
#endif


const char *
format_double(double value)
{
#ifdef DBL_MAX_10_EXP

  /* 20 is a safety margin. */
  static char buffer[1 + DBL_MAX_10_EXP + 1 + 6 + 20];
  char *buffer_pointer = buffer + sprintf(buffer, "%f", value);

#else

  static char buffer[0x400];
  char *buffer_pointer = buffer + snprintf(buffer, 0x400, "%f", value);

#endif

  while (buffer_pointer > buffer && *(buffer_pointer - 1) == '0')
    buffer_pointer--;

  if (buffer_pointer > buffer && *(buffer_pointer - 1) == '.')
    buffer_pointer++;

  *buffer_pointer = 0;
  return buffer;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
