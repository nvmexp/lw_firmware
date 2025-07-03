#!/bin/bash
find . -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\|c\)' -exec clang-format -i {} \;
