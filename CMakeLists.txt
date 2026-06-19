cmake_minimum_required(VERSION 3.24)

project(Artouste
    VERSION     0.1.0
    DESCRIPTION "Simulateur 3D Alouette II"
    LANGUAGES   CXX C
)

# --- Standard C++ ------------------------------------------------------------

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# --- Type de build par défaut ------------------------------------------------

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Type de build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

# --- Options du projet -------------------------------------------------------

option(ARTOUSTE_BUILD_TESTS "Compiler les tests Catch2" ON)
option(ARTOUSTE_BUILD_TOOLS "Compiler les outils de diagnostic" ON)
option(ARTOUSTE_WARNINGS_AS_ERRORS "Traiter les warnings comme erreurs" OFF)

# --- Sortie ------------------------------------------------------------------

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# compile_commands.json pour clangd/clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Cible "warnings" partagée par tout le projet ---------------------------

add_library(artouste_warnings INTERFACE)

if(MSVC)
    target_compile_options(artouste_warnings INTERFACE
        /W4 /permissive- /Zc:__cplusplus
    )
    if(ARTOUSTE_WARNINGS_AS_ERRORS)
        target_compile_options(artouste_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(artouste_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast
        -Wcast-align -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion -Wdouble-promotion
        -Wformat=2
    )
    if(ARTOUSTE_WARNINGS_AS_ERRORS)
        target_compile_options(artouste_warnings INTERFACE -Werror)
    endif()
endif()

# --- Dépendances tierces (FetchContent) -------------------------------------

include(cmake/Dependencies.cmake)

# --- Sous-répertoires --------------------------------------------------------

add_subdirectory(src)

if(ARTOUSTE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Installation et packaging (M7) ------------------------------------------
# Le binaire et les shaders sont installes cote a cote ; le modele et les sons
# (tiers, GPL) ne sont pas redistribues : l'application retombe sur le rendu
# procedural et reste silencieuse s'ils sont absents.

install(TARGETS artouste RUNTIME DESTINATION .)
install(DIRECTORY assets/shaders DESTINATION assets)
install(FILES README.md DESTINATION . OPTIONAL)

set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME "artouste")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "artouste-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
include(CPack)

# --- Récapitulatif lisible à la configuration --------------------------------

message(STATUS "")
message(STATUS "artouste ${PROJECT_VERSION}")
message(STATUS "  build type           : ${CMAKE_BUILD_TYPE}")
message(STATUS "  compilateur          : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  tests                : ${ARTOUSTE_BUILD_TESTS}")
message(STATUS "  warnings as errors   : ${ARTOUSTE_WARNINGS_AS_ERRORS}")
message(STATUS "")
