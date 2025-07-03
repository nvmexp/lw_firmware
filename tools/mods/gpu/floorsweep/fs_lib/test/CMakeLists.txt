if(TEST_FS)
  # Fetch GoogleTest
  # Uses hash of latest git commit
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/8d51ffdfab10b3fba636ae69bc03da4b54f8c235.zip
  )
  FetchContent_MakeAvailable(googletest)


  add_exelwtable(
    fs_all_tests
    fs_test_components.cpp
    fs_test_legacy.cpp
    fs_test_utils.h
    fs_test_ada.cpp
    fs_test_downbin_ada.cpp
    fs_test_downbin_hopper.cpp
    fs_test_utils.cpp
  )

  target_compile_features(fs_all_tests PRIVATE cxx_std_17)

  target_link_libraries(
    fs_all_tests
    gtest_main
    fs
  )

  include(GoogleTest)
  gtest_discover_tests(fs_all_tests)
  message("Unit tests generated. Use ctest to run.")
endif()

if(DUMP_FS)
  add_exelwtable(
    fsdump
    fs_dump.cpp
    fs_dump.h
  )
  target_link_libraries(
    fsdump
    fs_lib
  )
  message("fsdump will be generated.")
endif()
