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

/* The main purpose of this file is to use stuff in `config.h' in a
 * include-once protected way.  There are also a few commonly used
 * things.
 *
 * It should be included in every other `.h' file (mainly to cope with
 * missing `const' / `inline' compiler features).
 */


#ifndef QUARRY_H
#define QUARRY_H


#include "config.h"

#include <stdarg.h>


/* I prefer this to be stated explicitly.  Also saves from warnings in
 * some compilers (and this warning is asked from GCC by `configure'
 * and Makefiles just to be aware of the problem).
 */
#define UNUSED(parameter)	((void) (parameter))


/* More portable va_copy(). */
#if defined va_copy

#define QUARRY_VA_COPY(dest, src)	va_copy((dest), (src))

#elif defined __va_copy

#define QUARRY_VA_COPY(dest, src)	__va_copy((dest), (src))

#else


#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


/* This is recommended by Autoconf manual. */
#define QUARRY_VA_COPY(dest, src)		\
  memcpy(&(dest), &(src), sizeof(va_list))


#endif


#define ROUND_UP(value, power_of_2)			\
  (((value) + (power_of_2) - 1) & ~((power_of_2) - 1))


#endif /* QUARRY_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
