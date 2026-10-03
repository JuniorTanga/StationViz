# Headless checks on the IEC 61850 layer that do not need a real IED: MAC/APPID
# parsing, and building the network map from an SCD. Most defects in networkLib
# were silent, so the parse and map contracts are pinned here.
add_executable(network_smoke main.cpp)

target_link_libraries(network_smoke PRIVATE networkLib sclLib)

set(STATIONVIZ_NET_FIXTURES
    ${CMAKE_SOURCE_DIR}/tests/scl_sld_demo/data/SCD_NET_GOOD.scd
    ${CMAKE_SOURCE_DIR}/tests/tests_files/scl.scd
    ${CMAKE_SOURCE_DIR}/tests/tests_files/substation.scd
)
foreach(_scd ${STATIONVIZ_NET_FIXTURES})
    if(EXISTS ${_scd})
        get_filename_component(_name ${_scd} NAME_WE)
        add_test(NAME network_smoke.${_name} COMMAND network_smoke ${_scd})
    endif()
endforeach()

# The parse-only checks must run even when no fixture is present.
add_test(NAME network_smoke.parse COMMAND network_smoke)
