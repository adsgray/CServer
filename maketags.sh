#!/bin/sh

rm TAGS
find `pwd` -name "*.c" | egrep -v "RCS|SCCS|CVS" | etags -a -
find `pwd` -name "*.h" | egrep -v "RCS|SCCS|CVS" | etags -a -
