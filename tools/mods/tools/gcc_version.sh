#!/bin/bash

DIR=`mktemp -d`
SOURCE="$DIR/test.c"
DEST="$DIR/ver"
echo "#include <stdio.h>" > "$SOURCE"
echo 'int main(){printf("%d.%d\n",__GNUC__,__GNUC_MINOR__);return 0;}' >> "$SOURCE"
"$1" -o "$DEST" "$SOURCE" || exit $?
"$DEST"
ERR=$?
rm -rf "$DIR"
exit $ERR
