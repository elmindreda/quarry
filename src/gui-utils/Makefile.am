# Process this file with Automake to produce `Makefile.in'.

AM_CPPFLAGS =						\
	-D PACKAGE_DATA_DIR=\"$(datadir)/quarry\"	\
							\
	$(QUARRY_WARNINGS)				\
							\
	-I $(top_srcdir)/src				\
	-I $(top_srcdir)/src/board			\
	-I $(top_srcdir)/src/utils


noinst_LIBRARIES = libguiutils.a

BUILT_SOURCES =				\
	markup-theme-configuration.c	\
					\
	markup-theme-configuration.h


markup-theme-configuration.c markup-theme-configuration.h		\
  : markup-theme-configuration.list parse-configuration$(EXEEXT)
	./parse-configuration$(EXEEXT)					\
	    markup-theme-configuration.list				\
	    markup-theme-configuration.h markup-theme-configuration.c	\
	  || (rm -f markup-theme-configuration.c			\
		    markup-theme-configuration.h ;			\
	      exit 1)


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

nodist_libguiutils_a_SOURCES = $(BUILT_SOURCES)


noinst_PROGRAMS = parse-configuration

parse_configuration_SOURCES = parse-configuration.c

parse_configuration_LDADD =			\
	$(top_srcdir)/src/utils/libparselist.a	\
	$(top_srcdir)/src/utils/libutils.a


EXTRA_DIST = 					\
	common-configuration-sections.list	\
	common-configuration-values.list	\
	common-configuration-defaults.list	\
	markup-theme-configuration.list

DISTCLEANFILES = *~
