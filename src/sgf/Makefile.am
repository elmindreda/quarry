# Process this file with Automake to produce `Makefile.in'.

AM_CPPFLAGS = 				\
	$(QUARRY_WARNINGS)		\
					\
	-I $(top_srcdir)/src		\
	-I $(top_srcdir)/src/board	\
	-I $(top_srcdir)/src/utils


noinst_LIBRARIES = libsgf.a

BUILT_SOURCES =			\
	sgf-errors.c		\
	sgf-properties.c	\
				\
	sgf-errors.h		\
	sgf-properties.h

sgf-errors.c sgf-errors.h : sgf-errors.list parse-sgf-list$(EXEEXT)
	./parse-sgf-list$(EXEEXT) errors sgf-errors.list	\
				  sgf-errors.h sgf-errors.c	\
	  || (rm -f sgf-errors.c sgf-errors.h ; exit 1)

sgf-properties.c sgf-properties.h : sgf-properties.list parse-sgf-list$(EXEEXT)
	./parse-sgf-list$(EXEEXT) properties sgf-properties.list	\
				  sgf-properties.h sgf-properties.c	\
	  || (rm -f sgf-properties.c sgf-properties.h ; exit 1)

libsgf_a_SOURCES =		\
	sgf-diff-utils.c	\
	sgf-parser.c		\
	sgf-tree.c		\
	sgf-utils.c		\
	sgf-writer.c		\
				\
	sgf.h			\
	sgf-parser.h		\
	sgf-privates.h		\
	sgf-writer.h

nodist_libsgf_a_SOURCES = $(BUILT_SOURCES)


noinst_PROGRAMS = parse-sgf-list

parse_sgf_list_SOURCES = parse-sgf-list.c

parse_sgf_list_LDADD =				\
	$(top_srcdir)/src/utils/libparselist.a	\
	$(top_srcdir)/src/utils/libutils.a


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
	$(top_srcdir)/src/board/libboard.a	\
	$(top_srcdir)/src/utils/libutils.a


sgf_test_SOURCES = sgf-test.c

sgf_test_LDADD =				\
	libsgf.a				\
	$(top_srcdir)/src/board/libboard.a	\
	$(top_srcdir)/src/utils/libutils.a


EXTRA_DIST =			\
	sgf-errors.list		\
	sgf-properties.list	\
				\
	$(sgf_test_SOURCES)

DISTCLEANFILES = *~

MOSTLYCLEANFILES = $(BUILT_SOURCES) $(EXTRA_PROGRAMS)
