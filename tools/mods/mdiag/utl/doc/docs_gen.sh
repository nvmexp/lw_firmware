#!/bin/sh

# Usage: docs_gen.sh <MODS_RUNSPACE> <SW_TOOLS_PATH> <UTL_LIBRARY_PATH>
export UTL_DOCS_DIR=$(pwd)
$1/mods mdiag.js -check_utl_scripts -utl_library_path "$3" -utl_script "$UTL_DOCS_DIR/docs_gen.py $2 $3"
if (( $?)); then
    exit 1
fi
/home/tools/continuum/Anaconda3-5.1.0/bin/python $UTL_DOCS_DIR/docs_up.py 384128666 $UTL_DOCS_DIR/_build/api.conf
/home/tools/continuum/Anaconda3-5.1.0/bin/python $UTL_DOCS_DIR/docs_up.py 1078090455 $UTL_DOCS_DIR/_build/lib.conf
