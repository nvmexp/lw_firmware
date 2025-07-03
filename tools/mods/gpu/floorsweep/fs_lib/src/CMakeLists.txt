set(cpp_files fs_lib.cpp fs_config.cpp fs_chip.cpp fs_chip_core.cpp fs_chip_factory.cpp fs_chip_hopper.cpp fs_chip_ada.cpp fs_chip_with_cgas.cpp fs_compute_only_gpc.cpp SkylineSpareSelector.cpp)
set(headers fs_chip.h fs_utils.h ${CMAKE_SOURCE_DIR}/${inc_dir}/fs_lib.h)
set(lib_sources ${cpp_files} ${headers})

# Make static libfs.a
add_library(
    fs
    STATIC
    ${lib_sources}
)

target_compile_features(fs PRIVATE cxx_std_17)

target_include_directories(fs PUBLIC 
    "${CMAKE_LWRRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/${inc_dir}"
    "${CMAKE_SOURCE_DIR}/chips/"
    "${CMAKE_SOURCE_DIR}/external_libraries/etl-20.19.0/include"
    )

