cmake_minimum_required(VERSION 3.16)

project(StationViz VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.8)

add_subdirectory(core) #Backend

add_executable(${PROJECT_NAME}
    main.cpp
    SldFacade.cpp
    SldFacade.h
)

qt_add_qml_module(${PROJECT_NAME}
    URI ui
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE Qt6::Quick
    PRIVATE sclLib
    PRIVATE sldLib
)

include(GNUInstallDirs)

# install(TARGETS appStationViz
#     BUNDLE DESTINATION .
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )
