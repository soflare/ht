#!/bin/sh

if [[ $1 == "clean" ]]; then
    rm -f ht bin2c *.o hthelp.info
    rm -f config.h lex.yy.c evalparse.h evalparse.tab.c htdoc.c htdoc.h
    exit 0
fi

CPPFLAGS="-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H=1"
CFLAGS="-Wall -fsigned-char"
LDFLAGS=
if [[ $1 == "-d" ]]; then
    CFLAGS="$CFLAGS -g -fsanitize=address"
    LDFLAGS="$LDFLAGS -fsanitize=address"
else
    CFLAGS="$CFLAGS -O3 -fomit-frame-pointer"
fi
CXXFLAGS="$CFLAGS -Woverloaded-virtual -Wnon-virtual-dtor"

CINCS="-Ieval -I."
CXXINCS="-Ianalyser -Iasm -Ieval -Iinfo -Iio -Iio/posix -Ioutput -I."
CSRC="\
	eval/evalx.c \
	minilzo/minilzo.c \
	cp-demangle.c \
	cplus-dem.c \
	defreg.c \
	evalparse.tab.c \
	htdoc.c \
	lex.yy.c \
	regex.c"
CXXSRC="\
	analyser/*.cc \
	asm/*.cc \
	eval/eval.cc \
	info/infoview.cc \
	io/display.cc io/keyb.cc io/sys.cc io/file.cc io/posix/*.cc \
	output/out.cc output/out_ht.cc output/out_html.cc output/out_sym.cc output/out_txt.cc \
	*.cc"

test ! -f config.h && cat > config.h << EOF
#ifndef _CONFIG_H
#define _CONFIG_H

/* for MacOS X */

#define CURSES_HDR                      <ncurses.h>
#define SYSTEM_OSAPI_SPECIFIC_TYPES_HDR "io/posix/types.h"

#define HAVE_ASINH       1
#define HAVE_ACOSH       1
#define HAVE_ATANH       1
#define HAVE_ASINH       1
#define HAVE_ACOSH       1
#define HAVE_ATANH       1

#define USE_MINILZO      1
#undef  HAVE_LZO1X_H
#undef  HAVE_LZO_LZO1X_H

#define HAVE_ALLOCA_H    1
#define HAVE_BCOPY       1
#define HAVE_BZERO       1
#define HAVE_ISASCII     1
#undef  HAVE_LIBINTL
#define HAVE_LONG_DOUBLE 1
#define HAVE_LONG_LONG   1
#define HAVE_STDLIB_H    1
#define HAVE_STRING_H    1

#endif
EOF

if [[ ! -f tools/bin2c || tools/bin2c -ot tools/bin2c.c ]]; then
    cc -pipe -O2 -o bin2c tools/bin2c.c || exit
fi
if [[ ! -f hthelp.info || hthelp.info -ot doc/ht.texi ]]; then
    makeinfo --no-split --fill-column=64 --output hthelp.info doc/ht.texi || exit
    ./bin2c -Nhtinfo hthelp.info htdoc.c htdoc.h
fi

if [[ ! -f lex.yy.c || lex.yy.c -ot eval/lex.l ]]; then
    flex -o lex.yy.c eval/lex.l || exit
fi
if [[ ! -f evalparse.h || evalparse.h -ot eval/evalparse.y ]]; then
    yacc -d -b evalparse eval/evalparse.y || exit
    mv evalparse.tab.h evalparse.h
fi

cc -std=c11 -pipe -c $CFLAGS $CPPFLAGS $CINCS $CSRC

c++ -std=c++14 -pipe -c $CXXFLAGS $CPPFLAGS $CXXINCS $CXXSRC

c++ $LDFLAGS -o ht *.o -lncurses #-lm
