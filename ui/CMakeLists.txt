set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(stationviz_ui STATIC
  cpp/AppContext.cpp
  cpp/AppContext.h
  cpp/UiStore.h
  cpp/Models/DiagnosticModel.cpp
  cpp/Models/NodeModel.cpp
  cpp/Models/NodeModel.h
  cpp/Models/EdgeModel.h
  cpp/Models/EdgeModel.cpp
  cpp/Models/IedModel.cpp
  cpp/Models/IedModel.h
  cpp/Graph/SldView.cpp
  cpp/Graph/SldView.h
  cpp/Graph/SldIconAtlas.cpp
  cpp/Graph/SldIconAtlas.h
  qml.qrc
)

target_include_directories(stationviz_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cpp)

target_link_libraries(stationviz_ui
  PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::Concurrent
  PUBLIC sclLib
  PUBLIC sldLib
)

# Equipment symbols on the SLD need Qt6Svg; without it the SLD still renders,
# just without icons.
if(STATIONVIZ_HAVE_SVG)
  target_link_libraries(stationviz_ui PUBLIC Qt6::Svg)
  target_compile_definitions(stationviz_ui PUBLIC STATIONVIZ_HAVE_SVG=1)
endif()

if(MSVC)
  target_compile_options(stationviz_ui PRIVATE /W4)
else()
  target_compile_options(stationviz_ui PRIVATE -Wall -Wextra)
endif()
