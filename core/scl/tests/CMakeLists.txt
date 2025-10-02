cmake_minimum_required(VERSION 3.16)
project(scl_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Ajuste ces chemins vers tes sources du module ---
set(SCL_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../SclParser.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../SclManager.cpp
)
set(SCL_HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/../SclParser.h
    ${CMAKE_CURRENT_LIST_DIR}/../SclManager.h
    ${CMAKE_CURRENT_LIST_DIR}/../SclTypes.h
    ${CMAKE_CURRENT_LIST_DIR}/../EquipmentClassifier.h
    ${CMAKE_CURRENT_LIST_DIR}/../Internet.h
    ${CMAKE_CURRENT_LIST_DIR}/../Result.h
)

# ----- GoogleTest via FetchContent -----
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Pour MSVC: désactiver gtest en tant que sous-projet d'install
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ----- Nlohmann JSON (header-only). Si déjà présent dans ton projet, commente ce bloc. -----
# find_package(nlohmann_json CONFIG REQUIRED) # si tu utilises vcpkg/conan
# Sinon : ajoute l'include path manuellement
# target_include_directories(scl_core PUBLIC /chemin/vers/nlohmann_json/include)

# ----- PugiXML -----
# Assure-toi que ta cible 'pugixml' existe déjà (find_package ou add_subdirectory)
# Sinon, remplace par include_directories + link_libraries statiques selon ton intégration.
find_path(PUGIXML_INCLUDE_DIR pugixml.hpp)
find_library(PUGIXML_LIB NAMES pugixml)
if (NOT PUGIXML_INCLUDE_DIR OR NOT PUGIXML_LIB)
    message(FATAL_ERROR "pugixml not found. Set PUGIXML_INCLUDE_DIR and PUGIXML_LIB.")
endif()

# ----- Librairie core (recompile le module ici pour les tests) -----
add_library(scl_core STATIC ${SCL_SOURCES} ${SCL_HEADERS})
target_include_directories(scl_core PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/..    # headers du module
    ${PUGIXML_INCLUDE_DIR}
)
target_link_libraries(scl_core PUBLIC ${PUGIXML_LIB})

# ----- Binaire de test -----
add_executable(scl_tests
    ${CMAKE_CURRENT_LIST_DIR}/test_scl.cpp
)
target_include_directories(scl_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(scl_tests PRIVATE scl_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(scl_tests)
