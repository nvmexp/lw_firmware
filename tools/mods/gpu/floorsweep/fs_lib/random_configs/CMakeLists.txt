add_exelwtable(
    fs_random_configs
    random_configs.cpp
)

target_link_libraries(
    fs_random_configs
    fs
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
target_link_libraries(
    fs_random_configs
    pthread
)
endif()


target_include_directories(fs_random_configs PUBLIC 
    "${CMAKE_LWRRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/${inc_dir}"
    "${CMAKE_SOURCE_DIR}/src"
)

target_compile_features(fs_random_configs PRIVATE cxx_std_17)

# option parsing library
include(FetchContent)
    FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG ad2d1a6b5fd3fbb7bb0e7bc11f28a427494aea45
)
FetchContent_MakeAvailable(cxxopts)

target_include_directories(fs_random_configs PUBLIC ${cxxopts_SOURCE_DIR}/include)