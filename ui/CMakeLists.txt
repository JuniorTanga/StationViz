cmake_minimum_required(VERSION 3.20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.2 COMPONENTS Quick REQUIRED)

add_executable(stationviz_demo
    main.cpp
    SldFacade.cpp
    SldFacade.h
)

target_link_libraries(stationviz_demo PRIVATE
    Qt6::Quick
    sldLib
    sclLib
)

# Inclure ce dossier et hériter des include dirs exportés par sclLib/sldLib
target_include_directories(stationviz_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(stationviz_demo PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

# Copier le dossier QML au runtime
add_custom_command(TARGET stationviz_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/qml
            $<TARGET_FILE_DIR:stationviz_demo>/qml)
