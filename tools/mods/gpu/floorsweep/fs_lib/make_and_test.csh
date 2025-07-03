#!/bin/tcsh

if ($#argv == 0) then
  set optimize = OFF
else
  set optimize = $argv[1] 
endif

if ($optimize == OFF) then
  set gcc_version = '7.3.0'
else
  set gcc_version = '7.3.0'
endif

set gcc_path = /home/utils/gcc-$gcc_version
setelw CC $gcc_path/bin/gcc
setelw CXX $gcc_path/bin/g++

setelw LD_LIBRARY_PATH $gcc_path/lib64:LD_LIBRARY_PATH

setelw GTEST_COLOR 1

mkdir -p build
cd build

/home/utils/cmake-3.21.3/bin/cmake .. -DPRODUCTION_OPTIMIZE=$optimize  -DDOXYGEN_DOCS=OFF -DCMAKE_BUILD_TYPE=Release -DDUMP_FS=ON
make

if ($status == 0) then
  /home/utils/cmake-3.21.3/bin/ctest -V
endif
