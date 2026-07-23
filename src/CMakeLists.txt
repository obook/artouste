# Cible exécutable principale du simulateur.

# Version affichée dans le pied du menu de démarrage : dernière release (tag
# v*, sans le "v") suivie du nombre total de commits, comme la notice PDF
# (voir build.sh, section "Chaine de version affichee dans la notice"). Calculée
# à la configuration : un ./cmake après un nouveau commit ou tag la rafraîchit,
# mais pas un simple rebuild sans reconfiguration.
set(ARTOUSTE_VERSION_STRING "version de développement")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 --match "v*"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE ARTOUSTE_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(ARTOUSTE_GIT_TAG)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE ARTOUSTE_GIT_COMMIT_COUNT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        string(REGEX REPLACE "^v" "" ARTOUSTE_GIT_TAG "${ARTOUSTE_GIT_TAG}")
        set(ARTOUSTE_VERSION_STRING "v${ARTOUSTE_GIT_TAG}.${ARTOUSTE_GIT_COMMIT_COUNT}")
    endif()
endif()
message(STATUS "  version (pied de menu) : ${ARTOUSTE_VERSION_STRING}")

add_executable(artouste
    main.cpp
    app/lifecycle/ApplicationLifecycle.cpp
    app/lifecycle/ApplicationWindow.cpp
    app/lifecycle/ApplicationSession.cpp
    app/scene/ApplicationAssets.cpp
    app/scene/ApplicationScene.cpp
    app/loop/ApplicationLoop.cpp
    app/loop/ApplicationRotorRadio.cpp
    app/ApplicationInput.cpp
    app/render/ApplicationSun.cpp
    app/render/ApplicationRender.cpp
    app/render/ApplicationRenderEffects.cpp
    app/ground/ApplicationGround.cpp
    app/ground/ApplicationGroundShadow.cpp
    app/ground/ApplicationGroundHapi.cpp
    app/hud/ApplicationHudInstruments.cpp
    app/hud/ApplicationHudNav.cpp
    app/ApplicationCapture.cpp
    app/menu/ApplicationMenu.cpp
    app/menu/ApplicationLoadingScreen.cpp
    app/Config.cpp
    app/DemoPilot.cpp
    app/LandingAutopilot.cpp
    app/combat/CombatMode.cpp
    app/combat/ZombieHorde.cpp
    app/combat/Weapon.cpp
    app/combat/RocketSystem.cpp
    app/combat/ProjectileSystem.cpp
    app/combat/WaveManager.cpp
    audio/AudioEngineControl.cpp
    input/Keyboard.cpp
    input/Gamepad.cpp
    input/InputSystem.cpp
    physics/RigidBody.cpp
    physics/FlightModel.cpp
    physics/FlightAssist.cpp
    physics/Raycast.cpp
    physics/Turbine.cpp
    render/Shader.cpp
    render/Mesh.cpp
    render/Primitives.cpp
    render/Camera.cpp
    render/Terrain.cpp
    render/TerrainSetup.cpp
    render/hapi/Hapi.cpp
    render/Buildings.cpp
    render/vegetation/Vegetation.cpp
    render/vegetation/VegetationMasks.cpp
    render/vegetation/VegetationScatter.cpp
    render/SkinnedModel.cpp
    render/ExplosionModel.cpp
    render/combat/SkinnedZombies.cpp
    render/combat/ExplosionFx.cpp
    render/combat/Projectiles.cpp
    render/Clouds.cpp
    render/HelicopterModel.cpp
    render/Skybox.cpp
    render/Texture.cpp
    render/Model.cpp
    render/ModelLoader.cpp
    render/LoadedHelicopter.cpp
    render/LoadedHelicopterInstruments.cpp
    render/LoadedHelicopterDraw.cpp
    ui/Hud.cpp
    ui/hud/HudSuperOverlay.cpp
    ui/hud/HudPadGuidance.cpp
    ui/hud/HudLabelsMinimap.cpp
    ui/hud/HudCombat.cpp
    audio/AudioEngine.cpp
    audio/AudioEngineCombat.cpp
    audio/RingBuffer.cpp
    audio/RadioStream.cpp
)

# Racine des includes du projet : permet les chemins "render/...", "util/...".
target_include_directories(artouste PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Chemin des ressources (shaders, modèles) connu à la compilation. Pratique en
# développement ; un packaging Release (M7) copiera les assets à côté du binaire.
target_compile_definitions(artouste PRIVATE
    ARTOUSTE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
    ARTOUSTE_VERSION_STRING="${ARTOUSTE_VERSION_STRING}"
)

target_link_libraries(artouste
    PRIVATE
        artouste_warnings
        glfw
        glad
        glm::glm
        OpenGL::GL
        assimp::assimp
        stb_image
        imgui
        miniaudio
        Threads::Threads
        ${CMAKE_DL_LIBS}
)

# Radio internet : libcurl optionnelle. Si présente, on l'ajoute et on définit
# ARTOUSTE_HAS_CURL ; sinon audio/RadioStream.cpp se compile en coquille vide.
if(CURL_FOUND)
    target_link_libraries(artouste PRIVATE CURL::libcurl)
    target_compile_definitions(artouste PRIVATE ARTOUSTE_HAS_CURL)
endif()

# Synthèse vocale (TTS) des messages radio : Flite, compilé par notre wrapper
# CMake (voir cmake/Dependencies.cmake). Toujours disponible (multiplateforme).
target_link_libraries(artouste PRIVATE flite)
target_compile_definitions(artouste PRIVATE ARTOUSTE_HAS_FLITE)

# clangd / IDE
set_target_properties(artouste PROPERTIES
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

# Sous Windows, on construit une application graphique (sous-système GUI) et non
# une application console : sinon une fenêtre d'invite de commande s'ouvre derrière
# le jeu. WIN32_EXECUTABLE passe en /SUBSYSTEM:WINDOWS ; comme on garde un int main()
# classique (et non WinMain), on force l'entrée standard du CRT (mainCRTStartup)
# pour que main() reste le point d'entrée. Les messages console ne s'affichent plus
# dans une fenêtre dédiée ; pour les voir en debug, lancer depuis un terminal.
if(WIN32)
    set_target_properties(artouste PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(artouste PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()

# --- Outils de diagnostic ----------------------------------------------------

if(ARTOUSTE_BUILD_TOOLS)
    # Sonde manette : console autonome, ne dépend que de GLFW.
    add_executable(gamepad_probe tools/gamepad_probe.cpp)
    target_link_libraries(gamepad_probe PRIVATE artouste_warnings glfw)
    set_target_properties(gamepad_probe PROPERTIES
        CXX_STANDARD          20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS        OFF
    )

    # Sonde modèle 3D : charge un fichier via Assimp et dump sa structure.
    add_executable(model_probe tools/model_probe.cpp)
    target_link_libraries(model_probe PRIVATE artouste_warnings assimp::assimp glm::glm)
    set_target_properties(model_probe PROPERTIES
        CXX_STANDARD          20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS        OFF
    )
endif()
