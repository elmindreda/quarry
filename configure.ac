dnl  Process this file with Autoconf to produce a `configure' script.

dnl  Package name and version.
AC_INIT([Quarry], [0.1.1], [quarry@gna.org])
AC_PREREQ(2.57)

AM_INIT_AUTOMAKE

dnl  Check if the script is running in the proper directory.  Alarm if
dnl  it is not.
AC_CONFIG_SRCDIR(src/quarry.c)

AC_CONFIG_HEADER(config.h)

AH_TEMPLATE(PACKAGE, [The name of the package.])
AH_TEMPLATE(VERSION, [The version of the distribution.])

dnl  Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

AC_SUBST(QUARRY_WARNINGS)
AC_SUBST(QUARRY_WARNINGS_GTK)
AC_SUBST(QUARRY_WARNINGS_REDUCED)

if test "$GCC" = "yes"; then
  dnl  I would like to use `-W' too, but, unfortunately, it generates
  dnl  a few warnings which I cannot stand.  I find it cleaner to
  dnl  write `FIRST_XXX <= xxx && xxx <= LAST_XXX' even if `xxx' is
  dnl  unsigned and `FIRST_XXX' is zero.  It also complains about
  dnl  missing initializers of private fields of GTK+ structures,
  dnl  which I don't want to touch.

  QUARRY_WARNINGS='-Wall -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wunused-parameter -Wwrite-strings'

  dnl  `-Wstrict-prototypes' and `-Wshadow' launch alarms on GTK+
  dnl  headers, so they are excluded.

  QUARRY_WARNINGS_GTK='-Wall -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wredundant-decls -Wunused-parameter -Wwrite-strings'

  dnl  QUARRY_WARNINGS_REDUCED is for a few files that would generate
  dnl  unwanted warnings even with QUARRY_WARNINGS_GTK (because of
  dnl  `-Wwrite-strings').

  QUARRY_WARNINGS_REDUCED='-Wall -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wredundant-decls -Wunused-parameter'

else
  QUARRY_WARNINGS=''
  QUARRY_WARNINGS_GTK=''
  QUARRY_WARNINGS_REDUCED=''
fi

dnl  Optionally disable deprecated GTK+ code (for maintainers--to
dnl  check if some code should be replaced with newer).
AC_ARG_ENABLE(gtk-deprecated,
	      AC_HELP_STRING([--disable-gtk-deprecated],
			     [disable deprecated GTK+ features (for maintainers; don't use for normal builds)]))

AC_SUBST(QUARRY_GTK_DEPRECATED_FLAGS)

if test "$enable_gtk_deprecated" = "no"; then

  QUARRY_GTK_DEPRECATED_FLAGS='-D G_DISABLE_DEPRECATED -D GDK_DISABLE_DEPRECATED -D GDK_PIXBUF_DISABLE_DEPRECATED -D GTK_DISABLE_DEPRECATED'

else
  QUARRY_GTK_DEPRECATED_FLAGS=''
fi

dnl  Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(limits.h float.h)

dnl  Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T

dnl  Checks for library functions.
AC_FUNC_VPRINTF
AC_FUNC_MEMCMP
AC_CHECK_FUNCS(memrchr)

dnl  Require GTK+ 2.0.0 or later.
dnl
dnl  FIXME: since threads are optional in Quarry, the below call is
dnl	    probably too restrictive.
PKG_CHECK_MODULES(QUARRY_GTK, gtk+-2.0 >= 2.0.0 gthread-2.0 >= 2.0.0)
AC_SUBST(QUARRY_GTK_CFLAGS)
AC_SUBST(QUARRY_GTK_LIBS)

AC_SUBST(GLIB_GENMARSHAL, [`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`])

dnl  Write `Makefile's.
AC_CONFIG_FILES(Makefile
		data/Makefile
		data/textures/Makefile
		src/Makefile
		src/board/Makefile
		src/gtp/Makefile
		src/gui-gtk/Makefile
		src/gui-utils/Makefile
		src/sgf/Makefile
		src/utils/Makefile)

dnl  The end.
AC_OUTPUT
