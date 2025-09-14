cmake_minimum_required(VERSION 3.20)

add_library(sclLib
    SclManager.h
    SclParser.h
    SclParser.cpp
    SclManager.cpp
    SclTypes.h
    Result.h
)

add_subdirectory(pugixml)

# Entêtes publics
# target_include_directories(sclLib
#     PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
# )

# C++ standard
#set_target_properties(sclLib PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

target_link_libraries(sclLib PRIVATE pugixmlLib)

# Warnings utiles
# if (MSVC)
#     target_compile_options(sclLib PRIVATE /W4)
# else()
#     target_compile_options(sclLib PRIVATE -Wall -Wextra -Wpedantic)
# endif()
