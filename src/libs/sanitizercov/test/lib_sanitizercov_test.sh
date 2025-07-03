#!/bin/bash
set -e
gcc -fsanitize=address -fsanitize=undefined -DLIB_SANITIZER_COV_UNIT_TEST -iquote ../inc -iquote ./ -o lib_sanitizercov_test lib_sanitizercov_test.c ../src/lib_sanitizercov.c
exec ./lib_sanitizercov_test
