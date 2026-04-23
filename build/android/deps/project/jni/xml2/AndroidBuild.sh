#!/bin/sh

# Script to generate config.h with ./configure script

if [ \! -f Makefile ] ; then
	rm -f config.status libtool
	../launchConfigureLib.sh --enable-shared --disable-static --with-modules
fi
rm -f libxml2.so
make LIBXML2_VERSION_SCRIPT=-shared # This command will fail to link because of stupid libtool

