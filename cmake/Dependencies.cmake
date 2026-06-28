# Dépendances tierces récupérées via FetchContent.
# But : aucun paquet système requis hors libs X11/OpenGL nécessaires à GLFW.
# Cibles attendues en fin de fichier :
#   glfw, glad, glm::glm, imgui, stb_image, Catch2::Catch2WithMain

include(FetchContent)

# Téléchargement silencieux du progrès, sources extraites mais pas installées.
set(FETCHCONTENT_QUIET FALSE)

# ---------------------------------------------------------------------------
# GLFW (fenêtre / contexte OpenGL / entrées)
# ---------------------------------------------------------------------------
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
# On ne compile que le backend X11 sous Linux : evite de dependre de
# wayland-scanner et des protocoles Wayland sur la machine de build (les serveurs
# d'integration n'ont que X11). XWayland couvre les sessions Wayland a l'execution.
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# GLM (vecteurs, matrices, quaternions)
# ---------------------------------------------------------------------------
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# Dear ImGui (HUD et panneau debug)
# ---------------------------------------------------------------------------
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# Catch2 (tests unitaires)
# ---------------------------------------------------------------------------
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# Récupération effective
# ---------------------------------------------------------------------------
FetchContent_MakeAvailable(glfw glm Catch2)

# GLM déclenche des -Wsign-conversion dans ses propres templates : on traite ses
# en-têtes comme "système" pour ne pas polluer nos warnings stricts. On utilise
# le chemin concret des sources (pas l'expression génératrice de la cible).
if(TARGET glm AND glm_SOURCE_DIR)
    target_include_directories(glm SYSTEM INTERFACE ${glm_SOURCE_DIR})
endif()

# ImGui ne fournit pas de CMakeLists : on récupère les sources et on construit
# notre propre cible.
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

# ---------------------------------------------------------------------------
# GLAD - loader OpenGL 4.1 core. Les sources sont pré-générées et versionnées
# dans third_party/glad (profil 4.1 core), plutôt que générées au build par le
# script Python de Dav1dde/glad. On évite ainsi toute dépendance à Python et au
# réseau pendant la compilation : build reproductible et portable. La génération
# au build échouait d'ailleurs sur certains serveurs d'intégration, en récupérant
# un spec OpenGL XML invalide.
# ---------------------------------------------------------------------------
add_library(glad STATIC ${CMAKE_SOURCE_DIR}/third_party/glad/src/glad.c)
target_include_directories(glad SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third_party/glad/include)
target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})

# ---------------------------------------------------------------------------
# stb_image - header-only, on en fait une INTERFACE
# ---------------------------------------------------------------------------
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()
add_library(stb_image INTERFACE)
target_include_directories(stb_image SYSTEM INTERFACE ${stb_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Assimp - import du modèle 3D. On ne compile QUE l'importeur AC3D (.ac, format
# du modèle FlightGear) pour garder un build raisonnable, et aucun exporteur.
# ---------------------------------------------------------------------------
set(ASSIMP_BUILD_TESTS                       OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL                           OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS                OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES                     OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT                         ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT    OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT    OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_AC_IMPORTER                 ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DRACO                       OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS                OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL_PDB                       OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB                        ON  CACHE BOOL "" FORCE)

FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.4.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(assimp)

# ---------------------------------------------------------------------------
# miniaudio - audio (header-only). On en fait une INTERFACE ; l'implémentation
# est compilée dans un seul .cpp côté projet.
# ---------------------------------------------------------------------------
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.21
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio SYSTEM INTERFACE ${miniaudio_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Flite - synthèse vocale (TTS) des messages radio. Pas de build CMake amont
# (Flite se compile en autotools) : on récupère les sources et on construit
# nous-mêmes une cible statique minimale et MULTIPLATEFORME (cœur + anglais US
# + voix diphone cmu_us_kal). Backend audio "none" (on ne fait que synthétiser
# le PCM en mémoire, pas de lecture) et CST_NO_SOCKETS (aucune dépendance
# réseau), pour rester portable Linux comme Windows.
# ---------------------------------------------------------------------------
FetchContent_Declare(flite
    GIT_REPOSITORY https://github.com/festvox/flite.git
    GIT_TAG        v2.2
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(flite)
if(NOT flite_POPULATED)
    FetchContent_Populate(flite)
endif()

# Cœur : tous les .c de ces dossiers se compilent tels quels. La voix naturelle
# cmu_us_slt est une voix "clustergen" (src/cg) : plus naturelle que les diphones.
file(GLOB FLITE_SRC
    ${flite_SOURCE_DIR}/src/cg/*.c
    ${flite_SOURCE_DIR}/src/hrg/*.c
    ${flite_SOURCE_DIR}/src/lexicon/*.c
    ${flite_SOURCE_DIR}/src/regex/*.c
    ${flite_SOURCE_DIR}/src/speech/*.c
    ${flite_SOURCE_DIR}/src/stats/*.c
    ${flite_SOURCE_DIR}/src/synth/*.c
    ${flite_SOURCE_DIR}/src/wavesynth/*.c
    ${flite_SOURCE_DIR}/lang/usenglish/*.c
    ${flite_SOURCE_DIR}/lang/cmu_us_slt/*.c
)
# utils : seulement l'IO stdio et le mmap "none" (portables) ; on écarte les
# variantes palmos/wince/posix/win32.
list(APPEND FLITE_SRC
    ${flite_SOURCE_DIR}/src/utils/cst_alloc.c
    ${flite_SOURCE_DIR}/src/utils/cst_args.c
    ${flite_SOURCE_DIR}/src/utils/cst_endian.c
    ${flite_SOURCE_DIR}/src/utils/cst_error.c
    ${flite_SOURCE_DIR}/src/utils/cst_features.c
    ${flite_SOURCE_DIR}/src/utils/cst_file_stdio.c
    ${flite_SOURCE_DIR}/src/utils/cst_mmap_none.c
    ${flite_SOURCE_DIR}/src/utils/cst_socket.c
    ${flite_SOURCE_DIR}/src/utils/cst_string.c
    ${flite_SOURCE_DIR}/src/utils/cst_tokenstream.c
    ${flite_SOURCE_DIR}/src/utils/cst_url.c
    ${flite_SOURCE_DIR}/src/utils/cst_val.c
    ${flite_SOURCE_DIR}/src/utils/cst_val_const.c
    ${flite_SOURCE_DIR}/src/utils/cst_val_user.c
    ${flite_SOURCE_DIR}/src/utils/cst_wchar.c
)
# audio : backend "none" seulement (PCM récupéré en mémoire, aucune lecture).
list(APPEND FLITE_SRC
    ${flite_SOURCE_DIR}/src/audio/audio.c
    ${flite_SOURCE_DIR}/src/audio/au_none.c
    ${flite_SOURCE_DIR}/src/audio/au_streaming.c
)
# cmulex : ces .c sont #inclus par d'autres (pas compilés seuls). On les retire
# pour éviter des doublons de symboles.
file(GLOB FLITE_CMULEX ${flite_SOURCE_DIR}/lang/cmulex/*.c)
list(REMOVE_ITEM FLITE_CMULEX
    ${flite_SOURCE_DIR}/lang/cmulex/cmu_lex_data_raw.c
    ${flite_SOURCE_DIR}/lang/cmulex/cmu_lex_num_bytes.c
    ${flite_SOURCE_DIR}/lang/cmulex/cmu_lex_entries_huff_table.c
    ${flite_SOURCE_DIR}/lang/cmulex/cmu_lex_phones_huff_table.c
)
list(APPEND FLITE_SRC ${FLITE_CMULEX})

add_library(flite STATIC ${FLITE_SRC})
target_include_directories(flite SYSTEM PUBLIC ${flite_SOURCE_DIR}/include)
target_include_directories(flite PRIVATE
    ${flite_SOURCE_DIR}/lang/cmulex
    ${flite_SOURCE_DIR}/lang/usenglish
    ${flite_SOURCE_DIR}/lang/cmu_us_slt
)
target_compile_definitions(flite PRIVATE CST_NO_SOCKETS)
set_target_properties(flite PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# OpenGL (système) et threads (requis par miniaudio sous Linux)
# ---------------------------------------------------------------------------
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# libcurl (système, OBLIGATOIRE) - lecture du flux radio internet.
# Absente : le build s'arrête. La radio fait partie du cockpit ; on refuse de
# produire un binaire muet plutôt que de la désactiver en silence. On ne la
# récupère PAS via FetchContent (elle tire OpenSSL) : elle doit venir du système.
# ---------------------------------------------------------------------------
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "libcurl trouvée : radio internet activée.")
else()
    message(FATAL_ERROR
        "libcurl introuvable : la radio internet du cockpit ne peut pas être "
        "compilée.\n"
        "Installe le paquet de développement puis relance la compilation :\n"
        "  - Debian/Ubuntu : sudo apt install libcurl4-openssl-dev\n"
        "  - Fedora        : sudo dnf install libcurl-devel\n"
        "  - Arch          : sudo pacman -S curl\n"
        "  - Windows/vcpkg : vcpkg install curl")
endif()
