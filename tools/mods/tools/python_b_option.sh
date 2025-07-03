#!/bin/bash

# This script checks whether the Python interpreter (first argument to this script)
# supports the -B option.  If it does, the script prints the -B option, otherwise
# it does not print anything.

if "$1" -B -c 'print "hello"' > /dev/null 2> /dev/null; then
    echo "-B"
fi
