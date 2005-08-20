# Process this file with Automake to produce `Makefile.in'.

SUBDIRS =		\
	markup-themes	\
	textures


FILES =		\
	gtkrc

filesdir     = $(datadir)/quarry
files_DATA   = $(FILES)


DESKTOP_FILE = quarry.desktop

desktopdir   = $(prefix)/share/applications
desktop_DATA = $(DESKTOP_FILE)


EXTRA_DIST = $(FILES) $(DESKTOP_FILE:.desktop=.desktop.in)


DISTCLEANFILES = *~
