# Process this file with Automake produce `Makefile.in'.

AM_CPPFLAGS =				\
	$(QUARRY_WARNINGS)		\
					\
	-I $(top_srcdir)/src		\
	-I $(top_srcdir)/src/utils


noinst_LIBRARIES = libboard.a

LIST_FILES = games.list

PARSE_LIST_COMMAND = ./parse-game-list$(EXEEXT)
PARSE_LIST_FLAGS   =

# Avoid problems with parallel builds.
games.c: games.h

include $(top_srcdir)/build/list.make

libboard_a_SOURCES =		\
	amazons.c		\
	board.c			\
	go.c			\
	reversi.c		\
				\
	amazons.h		\
	board.h			\
	board-topology.h	\
	board-internals.h	\
	game-info.h		\
	go.h			\
	reversi.h		\
				\
	$(LIST_FILES)

nodist_libboard_a_SOURCES = $(LIST_GENERATED_FILES)

BUILT_SOURCES =	$(LIST_STAMP_FILES)


noinst_PROGRAMS = parse-game-list

parse_game_list_SOURCES = parse-game-list.c

parse_game_list_LDADD =					\
	$(top_builddir)/src/utils/libparselist.a	\
	$(top_builddir)/src/utils/libutils.a


DISTCLEANFILES = *~

MOSTLYCLEANFILES =		\
	$(LIST_STAMP_FILES)	\
	$(LIST_GENERATED_FILES)
