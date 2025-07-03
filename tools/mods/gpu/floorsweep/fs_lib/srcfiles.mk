# This file contains ONLY the source files and header files for the
# fs_lib library.  It should NOT contain other values such as the
# compiler, compiler flags, object-file directory, object file names,
# or build rules.
#
# The reason is that the fs_lib files are shared with multiple
# projects, all of which have different build elwironments.  So we
# need a shared file that can be included by all build elwironments,
# without also including values that are specific to just one build
# environment.

FS_LIB_SOURCES := fs_lib.cpp fs_config.cpp fs_chip.cpp fs_chip_core.cpp fs_chip_factory.cpp fs_chip_hopper.cpp fs_chip_ada.cpp SkylineSpareSelector.cpp fs_chip_with_cgas.cpp fs_compute_only_gpc.cpp

FS_LIB_HEADERS := ./include/fs_lib.h ./src/fs_chip.h ./src/fs_chip_core.h ./src/fs_chip_lwstom.h ./src/fs_utils.h ./src/SkylineSpareSelector.h ./src/fs_chip_with_cgas.h ./src/fs_compute_only_gpc.h
