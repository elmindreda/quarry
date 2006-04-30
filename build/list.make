SUFFIXES = .list .h

# `LIST_FILES' must be set by the includer.  The includer can use
# `LIST_STAMP_FILES' and `LIST_GENERATED_FILES' and assume they are
# set just as here.
#
LIST_STAMP_FILES     = $(LIST_FILES:.list=.stamp)
LIST_GENERATED_FILES = $(LIST_FILES:.list=.c) $(LIST_FILES:.list=.h)

# Make everything depend on the `Makefile' and the list parser.
$(LIST_STAMP_FILES) : Makefile $(PARSE_LIST_COMMAND)

# `PARSE_LIST_COMMAND' and `PARSE_LIST_FLAGS' should be set by the
# includer.
#
PARSE_LIST = $(PARSE_LIST_COMMAND) $(PARSE_LIST_FLAGS)

# We use `cmp' here to avoid unneeded recompilations of files that
# depend on generated ones (only really useful for `.h' files.)
#
PARSE_LIST_BUILD_RULE =						\
	if $(PARSE_LIST) `test -f '$<' || echo '$(srcdir)/'`$<	\
			 $*.h.new $*.c.new; then		\
	  if cmp -s $*.c.new $*.c;				\
	    then rm -f $*.c.new; else mv -f $*.c.new $*.c;	\
	  fi;							\
	  if cmp -s $*.h.new $*.h;				\
	    then rm -f $*.h.new; else mv -f $*.h.new $*.h;	\
	  fi;							\
	  echo timestamp > $@;					\
	else							\
	  (rm -f $*.c $*.c.new $*.h $*.h.new ; exit 1)		\
	fi

.list.stamp:
	$(PARSE_LIST_BUILD_RULE)

# Since $(LIST_GENERATED_FILES) defined by the includer don't (at
# least shouldn't) have any dependencies, if this rule is being
# executed, it probably means that one of the files was removed.
# Then all we can do is to force rebuilding of corresponding stamp
# file, which builds the required sources ``by side-effect.''
#
$(LIST_GENERATED_FILES):
	rm -f $*.stamp;
	$(MAKE) $(AM_MAKEFLAGS) $*.stamp
