cmake_minimum_required(VERSION 3.20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.2 COMPONENTS Quick Concurrent Svg REQUIRED)

add_library(stationviz_ui STATIC
  cpp/AppContext.cpp
  cpp/AppContext.h
  cpp/UiStore.h
  cpp/Models/DiagnosticModel.cpp
  cpp/Models/NodeModel.cpp
  cpp/Models/NodeModel.h
  cpp/Models/EdgeModel.h
  cpp/Graph/SldView.cpp
  cpp/Graph/SldView.h
  cpp/Graph/SldIconAtlas.cpp
  cpp/Graph/SldIconAtlas.h
  qml.qrc
  cpp/Models/IedModel.cpp cpp/Models/IedModel.h
)

target_include_directories(stationviz_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cpp)

# target_include_directories(${CORE_DIR})

# Lier aux backends existants (scl + sld)
# Assure-toi que les cibles 'scl' et 'sld' existent déjà dans ton arbre CMake
target_link_libraries(stationviz_ui
  PRIVATE Qt6::Quick Qt6::Qml Qt6::Svg Qt6::Concurrent
  PUBLIC sclLib
  PUBLIC sldLib
)


			
