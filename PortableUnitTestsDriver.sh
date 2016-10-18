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

# GTest libs (including main()).
GLIBS="-lgtest -lpthread -lgtest_main"
# Other libs.
OTHERLIBS=

# Source files dir.
SRCDIR=portableUnitTests
# Source files.
SRCS="`find ${SRCDIR} -name '*.cpp' -type f -print`"

echo "Using sources: $SRCS"


# FIXME: not implemented
exit 1