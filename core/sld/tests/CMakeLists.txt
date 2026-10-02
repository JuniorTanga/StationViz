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

add_executable(sld_tests tests_sld_acceptance.cpp)

target_link_libraries(sld_tests PRIVATE sldLib sclLib GTest::gtest_main)

# Fixtures are addressed relative to the source tree, not the working
# directory, so the test does not depend on where ctest is invoked from.
target_compile_definitions(sld_tests PRIVATE
    TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/scl_sld_demo/data")

include(GoogleTest)
gtest_discover_tests(sld_tests)