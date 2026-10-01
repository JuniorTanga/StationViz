# Headless smoke test of the model pipeline the QML UI depends on. Runs the real
# AppContext (SCL parse -> SLD plan -> Node/Edge/Diagnostic models) and asserts
# the invariants the UI assumes: populated models, unique ids, no duplicated
# edges, every node positioned.
add_executable(ui_smoke main.cpp)

target_link_libraries(ui_smoke PRIVATE stationviz_ui Qt6::Core Qt6::Gui)

# Runs the model pipeline over every fixture. IED-only and equipment-less
# documents are expected to produce no SLD graph, so the test only asserts
# per-document invariants (populated models, unique ids, no duplicate edges).
set(STATIONVIZ_SMOKE_FIXTURES
    ${CMAKE_SOURCE_DIR}/tests/scl_sld_demo/data
    ${CMAKE_SOURCE_DIR}/tests/tests_files
)

set(_smoke_cases "")
foreach(_dir ${STATIONVIZ_SMOKE_FIXTURES})
    file(GLOB _scds "${_dir}/*.scd")
    list(APPEND _smoke_cases ${_scds})
endforeach()

foreach(_scd ${_smoke_cases})
    get_filename_component(_name ${_scd} NAME_WE)
    add_test(NAME ui_smoke.${_name} COMMAND ui_smoke ${_scd})
    set_tests_properties(ui_smoke.${_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
