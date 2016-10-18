#!/bin/sh
#
# Script to be able to run on common Linux and *nix-like OSes (eg macOS)
# to build and run the C++ unit tests under the portableUnitTests directory.
#
# Requires a newish gcc (even if a front-end to Clang for example)
# and with gtest includes and libraries in system paths or under
# /usr/local/{lib,include}.
#
# Intended to be run without arguments from top-level dir of project.

# Generates a temporary executable at top level.
EXENAME=tmptestexe

# GTest libs (including main()).
GLIBS="-lgtest -lpthread -lgtest_main"
# Other libs.
OTHERLIBS=

# Glib includes
GINCLUDES=-I/usr/local/include
# Source includes
INCLUDES=-Icontent/OTRadioLink

# Source files dir.
SRCDIR=portableUnitTests
# Source files.
SRCS="`find ${SRCDIR} -name '*.cpp' -type f -print`"

echo "Using sources: $SRCS"

rm -f ${EXENAME}
if gcc -o${EXENAME} ${INCLUDES} ${GINCLUDES} ${SRCS} ${GLIBS} ${OTHERLIBS} ; then
    echo Compiled.
else
    echo Failed to compile.
    exit 2
fi

# FIXME: not implemented
exit 1