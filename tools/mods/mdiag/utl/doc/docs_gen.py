# Usage: docs_gen.py <SW_TOOLS_PATH> <UTL_LIBRARY_PATH>
import utl
import arch
import mods
import sys
import os
from sphinx.cmd import build as sphinx_build
from sphinx.ext import apidoc

sys.path.insert(0, utl.GetScriptArgs()[1])

DOC_DIR = os.getcwd()
BUILD_DIR = os.path.join(DOC_DIR, '_build')
LIB_DIR_NAME = 'lib'
LIB_DIR = os.path.join(DOC_DIR, LIB_DIR_NAME)

# Run sphinx-apidoc to generate document templates for each library
# Need to call twice, once for each of arch and mods
# Switches:
#   -f: overwrite previously generated files
#   --implicit-namespace: use implicit namespaces for modules
#   -T: do not generate table of contents (TOC) file
#   -s inc: output .inc files instead of .rst to avoid Sphinx autoprocessing them
sphinx_args = ['-f', '--implicit-namespaces', '-T', '-s', 'inc', '-o', LIB_DIR, os.path.join(utl.GetScriptArgs()[2], 'arch')]
apidoc.main(sphinx_args)
sphinx_args[-1] = os.path.join(utl.GetScriptArgs()[2], 'mods')
apidoc.main(sphinx_args)
# Concatenate into single lib.rst
with open('lib.rst', 'w+') as outfile:
    outfile.write('.. CAUTION:: DO NOT EDIT - this page is automatically updated by MODS\n\n')
    for fname in os.listdir(LIB_DIR):
        outfile.write(f'.. include:: {LIB_DIR_NAME}/{fname}\n')

# Run sphinx
# NOTE: Most exceptions are nonfatal and will be raised as warnings instead of errors.
#       This prevents errors in user libraries from breaking other documentation but will
#       silently fail when generating documentation for broken libraries.
sphinx_args = ['-b', 'confluence', DOC_DIR, BUILD_DIR]
sphinx_status = sphinx_build.main(sphinx_args)

# Escalate errors to MODS
if sphinx_status != 0:
    raise RuntimeError
