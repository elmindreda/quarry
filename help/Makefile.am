# Process this file with Automake to produce `Makefile.in'.

SUBDIRS = C de


# Convert DocBook help to HTML, which is used as fallback if `yelp' is
# not available.
#
# This should be done by maintainer only.  No need to do it when just
# building Quarry.
html:
	for directory in $(SUBDIRS); do					\
	  if test -e $$directory/quarry.xml; then			\
	    xmlto xhtml-nochunks $$directory/quarry.xml -o $$directory;	\
	    if test -e $$directory/quarry.html; then			\
	      sed -f quarry-help.sed					\
		     < $$directory/quarry.html > quarry.html.tmp;	\
	      mv quarry.html.tmp $$directory/quarry.html;		\
	    fi;								\
	  fi;								\
	done


cssdir = $(datadir)/quarry/help
css_DATA = quarry-help.css


EXTRA_DIST =		\
	quarry-help.css	\
	quarry-help.sed


DISTCLEANFILES = *~
