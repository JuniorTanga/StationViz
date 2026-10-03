# StationViz/core/network/CMakeLists.txt
#
# networkLib talks to real IEDs over MMS, GOOSE and Sampled Values using
# libiec61850. It was not in the build and had never compiled; see docs/PLAN.md
# section 5.

add_library(networkLib
  NetworkTypes.h
  EventBus.h EventBus.cpp
  StateStore.h StateStore.cpp
  ObjectRefMapper.h ObjectRefMapper.cpp
  DatasetResolver.h DatasetResolver.cpp
  NetworkMapBuilder.h NetworkMapBuilder.cpp
  TagRegistry.h TagRegistry.cpp
  NetUtils.h NetUtils.cpp
  MmsSession.h MmsSession.cpp
  GooseEngine.h GooseEngine.cpp
  SVEngine.h SVEngine.cpp
  CommandEngine.h CommandEngine.cpp
  PcapReplayer.h PcapReplayer.cpp
  InterfaceManager.h InterfaceManager.cpp
  NetworkManager.h NetworkManager.cpp
)

target_include_directories(networkLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---- libiec61850 ------------------------------------------------------------
#
# Consumed as a subproject rather than find_package: libiec61850 ships no config
# package and is not installed here. It exports exactly two targets, `iec61850`
# (static) and `iec61850-shared`.
#
# BUILD_EXAMPLES defaults to ON upstream and drags in ~25 example binaries plus
# a Findsqlite lookup, so it is forced off.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
add_subdirectory(libiec61850 EXCLUDE_FROM_ALL)

# PUBLIC, not PRIVATE: our public headers take IedConnection, GooseSubscriber and
# SVSubscriber in their signatures.
target_link_libraries(networkLib PUBLIC iec61850)

# libiec61850 sets its include path with include_directories() at the top of its
# own CMakeLists, which is directory-scoped and does not reach a sibling target.
# Its generated stack_config.h lives in its binary config dir, which the
# generator writes during configure.
set(IEC61850_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libiec61850)
target_include_directories(networkLib PUBLIC
  ${IEC61850_DIR}/src/common/inc
  ${IEC61850_DIR}/src/goose
  ${IEC61850_DIR}/src/sampled_values
  ${IEC61850_DIR}/hal/inc
  ${IEC61850_DIR}/src/iec61850/inc
  ${IEC61850_DIR}/src/mms/inc
  ${IEC61850_DIR}/src/logging
  ${IEC61850_DIR}/src/mms/iso_mms/asn1c
  ${CMAKE_CURRENT_BINARY_DIR}/libiec61850/config
)

if(NOT MSVC)
    target_compile_options(networkLib PRIVATE -Wall -Wextra)
else()
    target_compile_options(networkLib PRIVATE /W4)
endif()

# ---- libpcap (optional) ----------------------------------------------------
#
# Only PcapReplayer needs it; GOOSE and SV on Linux use libiec61850's own raw
# socket HAL. CMake 3.28 on this box has no FindPCAP.cmake, so find_package(PCAP)
# can only ever fail in CONFIG mode and warn. pkg-config is the reliable route.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PCAP QUIET libpcap)
endif()

if(PCAP_FOUND)
    target_include_directories(networkLib PRIVATE ${PCAP_INCLUDE_DIRS})
    target_link_libraries(networkLib PRIVATE ${PCAP_LIBRARIES})
    target_link_directories(networkLib PRIVATE ${PCAP_LIBRARY_DIRS})
    target_compile_definitions(networkLib PRIVATE HAVE_PCAP=1)
    message(STATUS "libpcap ${PCAP_VERSION} found: PcapReplayer enabled")
else()
    target_compile_definitions(networkLib PRIVATE STATIONVIZ_NO_PCAP=1)
    message(STATUS "libpcap not found: PcapReplayer disabled")
endif()