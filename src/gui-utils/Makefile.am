# Process this file with Automake to produce `Makefile.in'.

AM_CPPFLAGS =					\
	-D PACKAGE_DATA_DIR=\"$(pkgdatadir)\"	\
						\
	$(QUARRY_WARNINGS)			\
						\
	-I $(top_srcdir)/src			\
	-I $(top_srcdir)/src/sgf		\
	-I $(top_builddir)/src/sgf		\
	-I $(top_srcdir)/src/board		\
	-I $(top_builddir)/src/board		\
	-I $(top_srcdir)/src/utils


noinst_LIBRARIES = libguiutils.a

LIST_FILES = markup-theme-configuration.list

PARSE_LIST_COMMAND = ./parse-configuration$(EXEEXT)
PARSE_LIST_FLAGS   =

# Avoid problems with parallel builds.
markup-theme-configuration.c: markup-theme-configuration.h

include $(top_srcdir)/aux/list.make

libguiutils_a_SOURCES =			\
	configuration.c			\
	gui-utils.c			\
	tile-renderer.c			\
	time-control.c			\
					\
	configuration.h			\
	configuration-internals.h	\
	gui-utils.h			\
	tile-renderer.h			\
	time-control.h			\
					\
	$(LIST_FILES)

nodist_libguiutils_a_SOURCES = $(LIST_GENERATED_FILES)

BUILT_SOURCES = $(LIST_STAMP_FILES)


noinst_PROGRAMS = parse-configuration

parse_configuration_SOURCES = parse-configuration.c

parse_configuration_LDADD =				\
	$(top_builddir)/src/utils/libparselist.a	\
	$(top_builddir)/src/utils/libutils.a


EXTRA_DIST = 					\
	common-configuration-sections.list	\
	common-configuration-values.list	\
	common-configuration-defaults.list

DISTCLEANFILES = *~

MOSTLYCLEANFILES =		\
	$(LIST_STAMP_FILES)	\
	$(LIST_GENERATED_FILES)
