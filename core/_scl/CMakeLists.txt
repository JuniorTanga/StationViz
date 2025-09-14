file (GLOB MODELS_HEADERS "sclNodes/*.h")

add_library(sclLib STATIC
sclParser.h
sclParser.cpp
sclManager.h
sclManager.cpp
graph.h
graph.cpp
${MODELS_HEADERS}
)

target_include_directories(sclLib
    PRIVATE
        sclNodes/
)

add_subdirectory(pugixml)
find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(sclLib
    PRIVATE Qt6::Core
    PRIVATE pugixmlLib
)
