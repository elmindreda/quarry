# Process this file with Automake to produce `Makefile.in'.

AM_CPPFLAGS = 				\
	$(QUARRY_WARNINGS)		\
					\
	-I $(top_srcdir)/src		\
	-I $(top_srcdir)/src/board	\
	-I $(top_srcdir)/src/utils


noinst_LIBRARIES = libguiutils.a

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
	time-control.h


noinst_PROGRAMS = parse-configuration

parse_configuration_SOURCES = parse-configuration.c

parse_configuration_LDADD =			\
	$(top_srcdir)/src/utils/libparselist.a	\
	$(top_srcdir)/src/utils/libutils.a


EXTRA_DIST = 					\
	common-configuration-sections.list	\
	common-configuration-values.list	\
	common-configuration-defaults.list

DISTCLEANFILES = *~
