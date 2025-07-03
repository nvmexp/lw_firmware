#!/bin/sh

RETVAL=0
while [ $# -gt 0 ]; do
    if [ -x "$1" ]; then
        echo "Error: File $1 is exelwtable!"
        RETVAL=1
    fi
    shift
done

[ $RETVAL -ne 0 ] && echo "*** Please remove the x flag from P4 file type, or update exelwtable_files in Makefiles ***"

exit $RETVAL
