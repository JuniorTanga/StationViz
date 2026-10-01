# StationViz/core/network/CMakeLists.txt
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
  SvEngine.h SvEngine.cpp
  CommandEngine.h CommandEngine.cpp
  PcapReplayer.h PcapReplayer.cpp
  NetworkManager.h NetworkManager.cpp
)
add_subdirectory(libiec61850)
target_include_directories(networkLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# libIEC61850
set (IEC61850_INCLUDE_DIR libiec61850/src/iec61850/inc)
#find_path(IEC61850_INCLUDE_DIR NAMES iec61850_client.h)
#find_library(IEC61850_LIB NAMES iec61850)

target_include_directories(networkLib PUBLIC ${IEC61850_INCLUDE_DIR})
target_link_libraries(networkLib PRIVATE ${IEC61850_LIB})

#WinPCAP
find_package(PCAP)
if(PCAP_FOUND)
  target_link_libraries(networkLib PRIVATE PCAP::PCAP)
  target_compile_definitions(networkLib PRIVATE HAVE_PCAP=1)
else()
  message(STATUS "libpcap not found - PcapReplayer will not be usable.")
endif()

# Windows: NPcap/wpcap
# if(WIN32 AND NOT PCAP_FOUND)
  # find_path(PCAP_INCLUDE_DIR wpcap.h PATH_SUFFIXES Include)
  # find_library(PCAP_LIBRARY wpcap PATH_SUFFIXES Lib)
  # if(PCAP_INCLUDE_DIR AND PCAP_LIBRARY)
    # target_include_directories(networkLib PRIVATE ${PCAP_INCLUDE_DIR})
    # target_link_libraries(networkLib PRIVATE ${PCAP_LIBRARY})
  # endif()
# endif()
