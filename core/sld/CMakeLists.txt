cmake_minimum_required(VERSION 3.20)

add_library(sldLib
    SldTypes.h
    SldBuilder.cpp
    SldBuilder.h
    SldManager.cpp
    SldManager.h
    JsonWriter.h
)

# Headers publics (monofolder)
target_include_directories(sldLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(sldLib PUBLIC ${PROJECT_SOURCE_DIR}/core/scl)

# Dépend de sclLib (assurez add_subdirectory(scl) avant add_subdirectory(sld))
target_link_libraries(sldLib PUBLIC sclLib)
