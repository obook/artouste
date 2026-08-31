cmake_minimum_required(VERSION 3.24)

project(Artouste
    VERSION     0.46.0
    DESCRIPTION "Simulateur 3D Alouette II"
    LANGUAGES   CXX C
)

# --- Standard C++ ------------------------------------------------------------

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# --- Build autonome (un seul exécutable à distribuer) ------------------------
# Les bibliothèques tierces sont liées en statique pour qu'il n'y ait aucune DLL
# ni .so à livrer à côté du binaire. Sous MSVC, on lie aussi le runtime C++ en
# statique (/MT) : la release ne dépend alors pas du redistribuable Visual C++.

set(BUILD_SHARED_LIBS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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
        # Sources et chaînes en UTF-8 : indispensable pour que les accents français
        # (commentaires, libellés du HUD, messages) soient corrects avec MSVC.
        /utf-8
    )
    # getenv et consorts sont standard et portables : on coupe les avertissements
    # MSVC qui poussent vers leurs variantes _s non portables.
    target_compile_definitions(artouste_warnings INTERFACE _CRT_SECURE_NO_WARNINGS)
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
# Release complète et jouable : le binaire et l'ensemble des ressources sont
# installés côte à côte (shaders, modèle 3D, sons, terrain, textures). Le modèle
# et les sons FlightGear (GPL) sont redistribués avec leur fichier COPYING, déjà
# présent dans assets/models/Alouette-II/. Le terrain (IGN) est sous Licence
# Ouverte Etalab. Au lancement, le binaire trouve "assets" à côté de lui.

install(TARGETS artouste RUNTIME DESTINATION .)

# TOUTES les cartes voyagent dans l'archive, décision du 28/07/2026. Elles sont en
# basse résolution : les neuf pèsent 134 Mo sur le disque, une centaine dans
# l'archive, ce qui reste un seul téléchargement et évite au joueur d'aller
# chercher des zips séparés carte par carte. Seules les TUILES de détail, elles,
# se téléchargent depuis le jeu auprès de l'IGN (voir docs/DISTRIBUTION.md).
#
# Le moteur recense les cartes en parcourant assets/terrain/
# (ApplicationMenuMaps.cpp) : rien à déclarer ici quand on en ajoute une.
#
# Deux exclusions, qui ne coûtent rien sur la forge (dépôt tout neuf) mais
# comptent pour qui empaquette depuis sa machine : les jeux de tuiles rangés dans
# une carte pèsent des gigaoctets et se retéléchargent, et config.txt est la
# configuration PERSONNELLE de celui qui compile, que le modèle recrée au premier
# lancement de toute façon.
install(DIRECTORY assets/ DESTINATION assets
        PATTERN "tuiles" EXCLUDE
        PATTERN "config.txt" EXCLUDE)
install(FILES README.md LICENSE DESTINATION . OPTIONAL)
# Notice PDF des commandes, à la racine de l'archive. Le fichier est versionné
# (la CI ne compile pas le LaTeX) : penser à le recompiler et le recommiter
# après toute modification de docs/notice.tex.
install(FILES docs/notice.pdf DESTINATION . OPTIONAL)

# Paquets des cartes, un carte-<nom>.zip chacune : "cmake --build build --target
# cartes". Le zip contient le dossier de la carte, donc le dézipper dans
# assets/terrain/ suffit.
#
# Ces paquets NE VONT PLUS à la release depuis le 28/07/2026 : toutes les cartes
# voyagent maintenant dans l'archive du jeu, en un seul téléchargement. La cible
# reste pour l'usage local, passer une carte à quelqu'un sans lui envoyer les
# cent mégaoctets de l'archive complète.
#
# La liste est établie à la configuration : après avoir ajouté une carte dans
# assets/terrain/, relancer cmake avant de reconstruire la cible.
add_custom_target(cartes COMMENT "Paquets des cartes")
file(GLOB ARTOUSTE_TERRAINS RELATIVE ${CMAKE_SOURCE_DIR}/assets/terrain
     ${CMAKE_SOURCE_DIR}/assets/terrain/*)
foreach(carte IN LISTS ARTOUSTE_TERRAINS)
    if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/assets/terrain/${carte})
        add_custom_command(TARGET cartes POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E tar cf
                    ${CMAKE_BINARY_DIR}/carte-${carte}.zip --format=zip -- ${carte}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/assets/terrain
            COMMENT "carte-${carte}.zip"
            VERBATIM)
    endif()
endforeach()

# Paquets des tuiles de détail : "cmake --build build --target tuiles" écrit un
# tuiles-<nom>.zip par carte qui en a un jeu. Le zip contient le dossier de la
# carte, à dézipper soit dans assets/terrain/<nom>/ (le jeu y cherche un
# sous-dossier "tuiles"), soit dans le dossier désigné par ARTOUSTE_TUILES.
#
# Ces paquets ne vont PAS à la release : depuis le 28/07/2026 les tuiles ne
# s'obtiennent que de l'IGN, par le gestionnaire de cartes du jeu (voir
# docs/DISTRIBUTION.md). À 0,25 m/px une carte pèse de 1 à 37 Go, bien au-delà du
# plafond de 2 Go d'un fichier de release, et le zip ne gagne presque rien sur du
# BC7. La cible reste pour l'usage local : recopier un jeu d'une machine à
# l'autre, ou l'archiver avant de refaire les tuiles autrement.
#
# Les tuiles ne sont pas versionnées non plus : c'est de la donnée dérivée
# régénérable (tools/terrain/fetch_tuiles.py), sans commune mesure avec le dépôt.
# On indique donc où elles ont été produites :
#   cmake -S . -B build -DARTOUSTE_TUILES_DIR=/media/disque/tuiles
set(ARTOUSTE_TUILES_DIR "" CACHE PATH
    "Dossier des jeux de tuiles de détail (un sous-dossier par carte)")
add_custom_target(tuiles COMMENT "Paquets des tuiles de détail")
if(ARTOUSTE_TUILES_DIR AND IS_DIRECTORY ${ARTOUSTE_TUILES_DIR})
    file(GLOB ARTOUSTE_JEUX_TUILES RELATIVE ${ARTOUSTE_TUILES_DIR}
         ${ARTOUSTE_TUILES_DIR}/*)
    foreach(jeu IN LISTS ARTOUSTE_JEUX_TUILES)
        # Un index.txt atteste d'un jeu de tuiles exploitable : un dossier laissé
        # à moitié rempli par une génération interrompue n'en a pas.
        if(IS_DIRECTORY ${ARTOUSTE_TUILES_DIR}/${jeu} AND
           EXISTS ${ARTOUSTE_TUILES_DIR}/${jeu}/index.txt)
            add_custom_command(TARGET tuiles POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E tar cf
                        ${CMAKE_BINARY_DIR}/tuiles-${jeu}.zip --format=zip -- ${jeu}
                WORKING_DIRECTORY ${ARTOUSTE_TUILES_DIR}
                COMMENT "tuiles-${jeu}.zip"
                VERBATIM)
        endif()
    endforeach()
else()
    add_custom_command(TARGET tuiles POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo
                "Aucun jeu de tuiles : passer -DARTOUSTE_TUILES_DIR=<dossier> a cmake."
        VERBATIM)
endif()

# Plus de script de lancement : le choix de la carte et du démarrage turbine se fait
# désormais dans un menu intégré à l'exécutable (voir src/app/ApplicationMenu.cpp). On
# lance donc directement artouste (Linux) ou artouste.exe (Windows).

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
    # Pas de sous-dossier enveloppant dans le zip Windows : les fichiers sont
    # directement à la racine une fois dézippés (CPack en crée un par défaut).
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
else()
    set(CPACK_GENERATOR "TGZ")
endif()
set(CPACK_PACKAGE_NAME "artouste")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "artouste-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
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
