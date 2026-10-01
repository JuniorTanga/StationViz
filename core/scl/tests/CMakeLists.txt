find_package(GTest QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Link the real sclLib so these tests exercise the target the app actually uses.
# The previous version recompiled the sources into a separate scl_core target,
# which meant the tests could pass while the real library was broken.
add_executable(scl_tests
    tests_scl.cpp
    tests_scl_robustness.cpp
)

target_link_libraries(scl_tests PRIVATE sclLib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(scl_tests)
