#!/bin/bash
# Usage: docs_gen.sh <output_file_path>

DOC_TOOL='/home/utils/pandoc-1.19.2.1/bin/pandoc'
SCRIPT_PATH=`dirname $0`
SRC_PATH=`readlink -f "$SCRIPT_PATH/../../docs/pcyfile.txt"`
OUT_PATH=`readlink -f "$SCRIPT_PATH/../../docs/pcyfile.htm"`

if [ x$1 != x ]; then
    OUT_PATH=$1
fi

echo "Colwerting $SRC_PATH to HTML..."

$DOC_TOOL -f mediawiki -t HTML -o $OUT_PATH -s -H $SCRIPT_PATH/doc.css --toc --toc-depth=4 $SRC_PATH

if [ $? -ne 0 ]; then
    echo "Failed to colwert"
else
    echo "Done. The HTML file is generated at $OUT_PATH"
fi
