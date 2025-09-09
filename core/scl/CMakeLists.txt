file (GLOB MODELS_HEADERS "models/*.h")

add_library(sclLib STATIC
sclParser.h
sclParser.cpp
sclManager.h
sclManager.cpp
${MODELS_HEADERS}
)

target_include_directories(sclLib
    PRIVATE
        models/
)

add_subdirectory(pugixml)
find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(sclLib
    PRIVATE Qt6::Core
    PRIVATE pugixmlLib
)
