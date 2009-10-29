#!/bin/sh
# autogen.sh - generates configure using the autotools

OS=`uname`
if [ "$OS" = "Darwin" ]; then
    : ${LIBTOOLIZE=glibtoolize}
elif [ "$OS" = "Linux" ]; then
    : ${LIBTOOLIZE=libtoolize}
fi

: ${ACLOCAL=aclocal}
: ${AUTOHEADER=autoheader}
: ${AUTOMAKE=automake}
: ${AUTOCONF=autoconf}


${LIBTOOLIZE} --force --copy
${ACLOCAL}
${AUTOMAKE} --add-missing --copy


${AUTOCONF}

SILENT=`which ${LIBTOOLIZE} ${ACLOCAL} ${AUTOHEADER} ${AUTOMAKE} ${AUTOCONF}`
case "$?" in
    0 )
        echo All build tools found.
        ;;
    1)
        echo
        echo "--------------------------------------------------------------"
        echo "          >>> Some build tools are missing! <<<"
        echo Please make sure your system has the GNU autoconf and automake
        echo toolchains installed.
        echo "--------------------------------------------------------------"
        exit 1
        ;;
esac

echo 
echo "---------------------------------------------"
echo "autogen finished running, now run ./configure"
echo "---------------------------------------------"
