#! /usr/bin/elw bash
export PATH="/home/utils/binutils-2.37/bin:/home/utils/perl-5.8.8/bin:$PATH"

export LD_PRELOAD=/home/utils/gperftools-2.8/gcc-9.2.0/lib/libprofiler.so
export CPUPROFILE=fslib.prof
export CPUPROFILE_FREQUENCY=1000

build/lib/fs_all_tests --gtest_repeat=5 --gtest_brief=1

unset LD_PRELOAD
unset CPUPROFILE

/home/utils/gperftools-2.8/gcc-9.2.0/bin/pprof --text build/lib/fs_all_tests fslib.prof > fs_lib_profile.txt
/home/utils/gperftools-2.8/gcc-9.2.0/bin/pprof --pdf build/lib/fs_all_tests fslib.prof > fs_lib_profile.pdf
