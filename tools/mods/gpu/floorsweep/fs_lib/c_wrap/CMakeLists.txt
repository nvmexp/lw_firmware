# Make shared fs_lib
add_library(
    fs_lib
    SHARED
    fs_c_wrap.cpp
    fs_c_wrap.def
)

set_target_properties(fs_lib PROPERTIES PREFIX "")
target_link_libraries(fs_lib PUBLIC fs -static-libgcc -static-libstdc++) # static link is ok since mods uses a different makefile
#target_link_libraries(fs_lib PUBLIC fs stdc++)
