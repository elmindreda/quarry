# Process this file with Automake to produce `Makefile.in'.

AM_CPPFLAGS = 				\
	$(QUARRY_WARNINGS)		\
					\
	-I $(top_srcdir)/src		\
	-I $(top_srcdir)/src/board	\
	-I $(top_builddir)/src/board	\
	-I $(top_srcdir)/src/utils


noinst_LIBRARIES = libsgf.a

LIST_FILES =				\
	sgf-errors.list			\
	sgf-properties.list		\
	sgf-undo-operations.list

PARSE_LIST_COMMAND = ./parse-sgf-list$(EXEEXT)
PARSE_LIST_FLAGS   =

# Avoid problems with parallel builds.

sgf-errors.c: sgf-errors.h

sgf-properties.c: sgf-properties.h

sgf-undo-operations.c: sgf-undo-operations.h

include $(top_srcdir)/aux/list.make

libsgf_a_SOURCES =		\
	sgf-diff-utils.c	\
	sgf-parser.c		\
	sgf-tree.c		\
	sgf-tree-map.c		\
	sgf-undo.c		\
	sgf-utils.c		\
	sgf-writer.c		\
				\
	sgf.h			\
	sgf-parser.h		\
	sgf-privates.h		\
	sgf-undo.h		\
	sgf-writer.h		\
				\
	$(LIST_FILES)

nodist_libsgf_a_SOURCES = $(LIST_GENERATED_FILES)

BUILT_SOURCES = $(LIST_STAMP_FILES)


noinst_PROGRAMS = parse-sgf-list

parse_sgf_list_SOURCES = parse-sgf-list.c

parse_sgf_list_LDADD =					\
	$(top_builddir)/src/utils/libparselist.a	\
	$(top_builddir)/src/utils/libutils.a


# `sgf-diff' can be made optional with `configure'.
EXTRA_PROGRAMS =	\
	sgf-diff	\
	sgf-test


if BUILD_SGF_UTILS
  bin_PROGRAMS = sgf-diff
else
  bin_PROGRAMS =
endif

sgf_diff_SOURCES = sgf-diff.c

sgf_diff_LDADD =				\
	libsgf.a				\
	$(top_builddir)/src/board/libboard.a	\
	$(top_builddir)/src/utils/libutils.a


sgf_test_SOURCES = sgf-test.c

sgf_test_LDADD =				\
	libsgf.a				\
	$(top_builddir)/src/board/libboard.a	\
	$(top_builddir)/src/utils/libutils.a


DISTCLEANFILES = *~

CLEANFILES = $(EXTRA_PROGRAMS)

MOSTLYCLEANFILES =		\
	$(LIST_STAMP_FILES)	\
	$(LIST_GENERATED_FILES)
