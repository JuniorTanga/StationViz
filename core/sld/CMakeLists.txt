add_library(sldLib
    SldTypes.h
    SldConfig.h
    SldBuilder.cpp
    SldBuilder.h
    SldManager.cpp
    SldManager.h
)

target_include_directories(sldLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# nlohmann/json.hpp is vendored under core/scl and resolved via <nlohmann/json.hpp>
target_link_libraries(sldLib PUBLIC sclLib)

if(MSVC)
    target_compile_options(sldLib PRIVATE /W4)
else()
    target_compile_options(sldLib PRIVATE -Wall -Wextra)
endif()
