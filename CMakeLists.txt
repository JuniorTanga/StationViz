cmake_minimum_required(VERSION 3.20)

project(StationViz VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CORE_DIR core)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Quick QuickControls2 QuickDialogs2 Concurrent)
# Qt6Svg is needed by SldIconAtlas (equipment symbols on the SLD).
find_package(Qt6 6.2 QUIET COMPONENTS Svg)
if(Qt6Svg_FOUND)
    set(STATIONVIZ_HAVE_SVG 1)
else()
    set(STATIONVIZ_HAVE_SVG 0)
    message(WARNING
        "Qt6Svg not found: the SLD will build without equipment symbols. "
        "Install libqt6svg6-dev to enable them.")
endif()

qt_standard_project_setup(REQUIRES 6.2)

include(CTest)

add_subdirectory(core) #Backend
add_subdirectory(ui) #Frontend
add_subdirectory(tests) #Tests

add_executable(${PROJECT_NAME}
    main.cpp
)

# qt_add_qml_module(${PROJECT_NAME}
#     URI ui
#     VERSION 1.0
#     QML_FILES
#         Main.qml
# )

target_link_libraries(${PROJECT_NAME}
    PRIVATE Qt6::Quick
    PRIVATE Qt6::QuickControls2
    PRIVATE sclLib
    PRIVATE sldLib
    PRIVATE stationviz_ui
)

include(GNUInstallDirs)

# install(TARGETS appStationViz
#     BUNDLE DESTINATION .
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )
