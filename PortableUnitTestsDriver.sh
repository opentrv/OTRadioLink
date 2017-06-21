#!/bin/sh
#
# Script to be able to run on common Linux and *nix-like OSes (eg macOS)
# to build and run the C++ unit tests under the portableUnitTests directory.
#
# Requires a newish g++ (even if a front-end to Clang for example)
# and with gtest includes and libraries in system paths or under
# /usr/local/{lib,include}.
#
# Intended to be run without arguments from top-level dir of project.
#
# Run as:
#
#     sh ./portableUnitTestsDriver.sh

# Generates a temporary executable at top level.
EXENAME=tmptestexe

# Project source root.
PROJSRCROOT=content/OTRadioLink
# Project source files under test.
PROJSRCS="`find ${PROJSRCROOT} -name '*.cpp' -type f -print`"

# Test source files dir.
TESTSRCDIR=portableUnitTests
# Source files.
TESTSRCS="`find ${TESTSRCDIR} -name '*.cpp' -type f -print`"

# GTest libs (including main()).
GLIBS="-lgtest -lgtest_main -lpthread"
# Other libs.
OTHERLIBS=

# GTest libs (paths).
GLIBDIRS="-L/usr/local/lib"

# Glib includes (paths).
GINCLUDES="-I/usr/local/include"
# Source includes (paths).
INCLUDES="-I${PROJSRCROOT} -I${PROJSRCROOT}/utility"

#echo "Using test sources: $TESTSRCS"
#echo "Using project sources: $PROJSRCS"

EXTRACPPFLAGS=

# If OTAESGCM code is present, add it to the source path,
# and set the flag to allow the extra tests based on it.
# Try via a the 'unpacked master' path first, then a relative path.
OTAESGCMALTDIR="OTAESGCM-master"
OTAESGCMRELDIR="../OTAESGCM"
if [ -d ${OTAESGCMALTDIR} ]; then
    OTAESGCMDIR="${OTAESGCMALTDIR}"
else
    OTAESGCMDIR="${OTAESGCMRELDIR}"
fi
OTAESGCMSRCDIR="${OTAESGCMDIR}/content/OTAESGCM"
if [ -d ${OTAESGCMSRCDIR} ]; then
    echo "Lib source dir ${OTAESGCMSRCDIR} is present and will be used."
    INCLUDES="${INCLUDES} -I${OTAESGCMSRCDIR} -I${OTAESGCMSRCDIR}/utility"
    PROJSRCS="${PROJSRCS} `find ${OTAESGCMSRCDIR} -name '*.cpp' -type f -print`"
    EXTRACPPFLAGS="-DEXT_AVAILABLE_ARDUINO_LIB_OTAESGCM"
else
    echo "*** WARNING: no crypto tests in absence of OTAESCGM ***"
fi

rm -f ${EXENAME}
if g++ -o ${EXENAME} -std=c++0x -O0 -Wall -Werror -fstack-check -fstack-protector-strong ${EXTRACPPFLAGS} ${INCLUDES} ${GINCLUDES} ${PROJSRCS} ${TESTSRCS} ${GLIBDIRS} ${GLIBS} ${OTHERLIBS} ; then
    echo Compiled.
else
    echo Failed to compile.
    exit 2
fi

# Run the tests, repeatedly, shuffled (except first run to make diffs easier).
# Run in increasingly large blocks.
./${EXENAME} --gtest_repeat=1 \
  && ./${EXENAME} --gtest_shuffle --gtest_repeat=10 \
  && ./${EXENAME} --gtest_shuffle --gtest_repeat=100 \
  && echo OK
