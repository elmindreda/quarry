# Process this file with Automake produce `Makefile.in'.

AM_CPPFLAGS =				\
	$(QUARRY_WARNINGS)		\
					\
	-I $(top_srcdir)/src		\
	-I $(top_srcdir)/src/utils


noinst_LIBRARIES = libboard.a

BUILT_SOURCES =		\
	games.c		\
			\
	games.h

games.c games.h : games.list parse-game-list$(EXEEXT)
	./parse-game-list$(EXEEXT) games.list games.h games.c	\
	  || (rm -f games.c games.h ; exit 1)

libboard_a_SOURCES =		\
	amazons.c		\
	board.c			\
	go.c			\
	othello.c		\
				\
	amazons.h		\
	board.h			\
	board-topology.h	\
	board-internals.h	\
	game-info.h		\
	go.h			\
	othello.h

nodist_libboard_a_SOURCES = $(BUILT_SOURCES)


noinst_PROGRAMS = parse-game-list

parse_game_list_SOURCES = parse-game-list.c

parse_game_list_LDADD =				\
	$(top_srcdir)/src/utils/libparselist.a	\
	$(top_srcdir)/src/utils/libutils.a


EXTRA_DIST = 		\
	games.list

DISTCLEANFILES = *~

MOSTLYCLEANFILES = $(BUILT_SOURCES)
