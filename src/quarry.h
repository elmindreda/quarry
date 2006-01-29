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

/* The main purpose of this file is to use stuff in `config.h' in a
 * include-once protected way.  There are also some commonly used
 * things, like i18n macros.
 *
 * It should be included in every other `.h' file.
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

#define QUARRY_VA_COPY(dest, src)	va_copy ((dest), (src))

#elif defined __va_copy

#define QUARRY_VA_COPY(dest, src)	__va_copy ((dest), (src))

#else


#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


/* This is recommended by Autoconf manual. */
#define QUARRY_VA_COPY(dest, src)		\
  memcpy (&(dest), &(src), sizeof (va_list))


#endif


/* A set of useful macros. */

#undef MIN
#define MIN(a, b)		((a) < (b) ? (a) : (b))

#undef MAX
#define MAX(a, b)		((a) > (b) ? (a) : (b))

#undef ROUND_DOWN
#define ROUND_DOWN(value, power_of_2)			\
  ((value) & ~((power_of_2) - 1))

#undef ROUND_UP
#define ROUND_UP(value, power_of_2)			\
  (((value) + (power_of_2) - 1) & ~((power_of_2) - 1))


/* I18n macros. The `gettext.h' header disables NLS itself, if
 * requested by `configure' script.
 */
#include "gettext.h"

#define _(string)		gettext (string)
#define N_(string)		gettext_noop (string)

/* Function utils_gettext_with_context() is similar to GLib's
 * g_strip_context(), but also contains the call to gettext() itself.
 * Since the Q_() macro has the same signature, as GLib's variant, we
 * use the same name.
 */
#define Q_(string)		utils_gettext_with_context (string)


/* Determine if given byte (`character') starts an UTF-8 character,
 * probably a multi-byte one.
 */
#define IS_UTF8_STARTER(character)			\
  ((signed char) (character) >= (signed char) 0xc0)


#endif /* QUARRY_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
