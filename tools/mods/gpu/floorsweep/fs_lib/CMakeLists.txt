# 3.10 required for gtest_discover_tests()
# 3.14 reccomended in googletest documentation
cmake_minimum_required(VERSION 3.14)

# Note: to set options, run -D(option)=ON/OFF
# cmake (other args) -DCONSUMER_LWGPU=OFF -DCONSUMER_MODS=ON

option(PRODUCTION_OPTIMIZE "Generate fs_lib with optimizations" OFF)

set(default_warnings -W -Wreturn-type -Werror -Wformat -Wall -Wno-unused-parameter)
set(warning_suppressions -Wno-missing-field-initializers)
# if/else block that can contain build options needed for lwgpu or MODS
if(PRODUCTION_OPTIMIZE) 
    message("Building fs_lib with optimizations")
    add_definitions(-DUSEBASEVECTORVIEW -DUSESTATICCONTAINERS -DUSEPOLYMORPHCONTAINER)
else()
    message("Building fs_lib with no optimizations")
    # MODS or LWGPU release specific build option scan go here
endif()

# Let's make the refactored version 1.0
project(fs_lib 
    LANGUAGES CXX 
    VERSION 1.0
    )

# Add compile-time warnings
if (MSVC)
else()
add_compile_options(${default_warnings} ${warning_suppressions})
endif()

# After refactoring, use target_compile_features 
# GCC version used has CXX 11 features by default
# target_compile_features(fs_lib PRIVATE cxx_FEATURE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Define some variables
set(src_dir src)
set(inc_dir include)
set(cwrap_dir c_wrap)
set(test_dir test)
set(obj_dir lib)
set(docs_dir docs)

# Output files into 'lib' directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${obj_dir})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${obj_dir})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${obj_dir})
set(CMAKE_EXELWTABLE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${obj_dir})

# Add the directories with source files
# Runs the CMakeLists.txt in those directories
# src generates the static libfs.a
add_subdirectory(${src_dir})
# cwrap generates the shared fs_lib.so
add_subdirectory(${cwrap_dir})

# Generate googletests by default
option(TEST_FS "Generate fs_lib unit tests" ON) 
option(DUMP_FS "Generate fs_dump" OFF)
if(TEST_FS)
    enable_testing()
endif()
add_subdirectory(test)
add_subdirectory(random_configs)

option(DOXYGEN_DOCS "Add doxygen documentation" ON)
if(DOXYGEN_DOCS)
  # Depricated, but needed due to /home/utils structure
  set(DOXYGEN_EXELWTABLE "/home/utils/doxygen-1.8.8/bin/doxygen")
  set(DOXYGEN_CALLER_GRAPH YES)
  set(DOXYGEN_CALL_GRAPH YES)
  set(DOXYGEN_EXTRACT_PROTECTED YES)
  # Generate documentation in source directory
  set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/${docs_dir})

  find_package(Doxygen 1.8.8
               REQUIRED dot)
  doxygen_add_docs(doxygen-docs 
                   ${src_dir} ${inc_dir} ${cwrap_dir} ${test_dir}
                   ALL)

endif()
