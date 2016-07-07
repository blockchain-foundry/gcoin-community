#!/bin/sh
# Run this to generate all the initial makefiles, etc.
test -n "$srcdir" || srcdir=$(dirname "$0")
test -n "$srcdir" || srcdir=.

if [ -z "${LIBTOOLIZE}" ] && GLIBTOOLIZE="`which glibtoolize 2>/dev/null`"; then
  LIBTOOLIZE="${GLIBTOOLIZE}"
  export LIBTOOLIZE
fi

if [ "$#" = 0 ] && [ -z "$NOCONFIGURE" ]; then
  echo "*** WARNING: I am going to run 'configure' with no arguments." >&2
  echo "*** If you wish to pass any to it, please specify them on the" >&2
  echo "*** '$0' command line." >&2
  echo "" >&2
fi

which autoreconf >/dev/null || \
  { echo "configuration failed, please install autoconf first" && exit 1; }
( cd "$srcdir" && autoreconf --install --force --warnings=all ) || exit 1

if [ -z "$NOCONFIGURE" ]; then
  $srcdir/configure "$@" || exit 1
else
  echo "Skipping configure process."
fi
