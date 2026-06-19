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
# GLAD - loader OpenGL 4.1 core.
# Le repo Dav1dde/glad embarque un CMakeLists qui génère les sources à la
# configure-time via un script Python (python3 requis sur la machine de
# build). On force le profil 4.1 core, suffisant pour ce projet.
# ---------------------------------------------------------------------------
set(GLAD_PROFILE      "core"   CACHE STRING "" FORCE)
set(GLAD_API          "gl=4.1" CACHE STRING "" FORCE)
set(GLAD_GENERATOR    "c"      CACHE STRING "" FORCE)
set(GLAD_NO_LOADER    OFF      CACHE BOOL   "" FORCE)
set(GLAD_REPRODUCIBLE ON       CACHE BOOL   "" FORCE)

FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glad)

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
# OpenGL (système) et threads (requis par miniaudio sous Linux)
# ---------------------------------------------------------------------------
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
