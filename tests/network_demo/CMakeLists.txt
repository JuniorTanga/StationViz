cmake_minimum_required(VERSION 3.18)

project(network_demo CXX)

add_executable(network_demo
  main.cpp
)

# Inclure les headers du projet
target_include_directories(network_demo PRIVATE
  ${CMAKE_SOURCE_DIR}/core
)

# Link contre le module network (et vos modules scl/sld si exposés en cibles)
target_link_libraries(network_demo PRIVATE network)

# Si vos modules scl/sld existent comme cibles:
# target_link_libraries(network_demo PRIVATE network scl sld)

# Sous Linux: pcap peut être nécessaire si vous utilisez PcapReplayer depuis le binaire de démo
find_package(PCAP)
if(PCAP_FOUND)
  target_link_libraries(network_demo PRIVATE PCAP::PCAP)
endif()
