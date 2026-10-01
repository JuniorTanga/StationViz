add_library(sclLib
    SclManager.h
    SclParser.h
    SclParser.cpp
    SclManager.cpp
    SclTypes.h
    Result.h
    Result.cpp
    Internet.h
    EquipmentClassifier.h
)

add_subdirectory(pugixml)

# PUBLIC: sldLib resolves <nlohmann/json.hpp> through this include path
target_include_directories(sclLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(sclLib PUBLIC pugixmlLib)

if(MSVC)
    target_compile_options(sclLib PRIVATE /W4)
else()
    target_compile_options(sclLib PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
