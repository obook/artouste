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
    app/scene/ApplicationSceneShaders.cpp
    app/scene/ApplicationSceneConfig.cpp
    app/loop/ApplicationLoop.cpp
    app/loop/ApplicationControls.cpp
    app/loop/ApplicationCameraAudio.cpp
    app/loop/ApplicationRotorRadio.cpp
    app/input/ApplicationInputKeyboard.cpp
    app/input/ApplicationInputButtons.cpp
    app/render/ApplicationSun.cpp
    app/render/ApplicationRender.cpp
    app/render/ApplicationRenderWorld.cpp
    app/render/ApplicationRenderActors.cpp
    app/render/ApplicationRenderEffects.cpp
    app/monuments/ApplicationMonuments.cpp
    app/ground/ApplicationGround.cpp
    app/ground/ApplicationGroundShadow.cpp
    app/ground/ApplicationGroundHapi.cpp
    app/hud/ApplicationHudInstruments.cpp
    app/hud/ApplicationHudLanding.cpp
    app/hud/ApplicationHudNav.cpp
    app/ApplicationCapture.cpp
    app/menu/ApplicationMenu.cpp
    app/menu/ApplicationMenuMaps.cpp
    app/menu/ApplicationMenuCartes.cpp
    app/cartes/FabriqueTuiles.cpp
    app/menu/ApplicationLoadingScreen.cpp
    app/Config.cpp
    app/CycleJourNuit.cpp
    app/DemoPilot.cpp
    app/LandingAutopilot.cpp
    app/LigneCommande.cpp
    app/SouffleRotor.cpp
    app/MiseAJour.cpp
    app/combat/CombatMode.cpp
    app/combat/ZombieHorde.cpp
    app/combat/Weapon.cpp
    app/combat/RocketSystem.cpp
    app/combat/ProjectileSystem.cpp
    app/combat/WaveManager.cpp
    audio/AudioEngineControl.cpp
    audio/AudioEngineRadio.cpp
    input/Keyboard.cpp
    input/Gamepad.cpp
    input/GamepadAxes.cpp
    input/GamepadButtons.cpp
    input/InputSystem.cpp
    physics/RigidBody.cpp
    physics/FlightModel.cpp
    physics/FlightAssist.cpp
    physics/Raycast.cpp
    physics/Turbine.cpp
    render/Shader.cpp
    render/Dds.cpp
    render/Bc7.cpp
    render/Mesh.cpp
    render/Primitives.cpp
    render/Camera.cpp
    render/Terrain.cpp
    render/TerrainSetup.cpp
    render/TerrainQuery.cpp
    render/tuiles/Pyramide.cpp
    render/tuiles/Fenetre.cpp
    render/hapi/Hapi.cpp
    render/buildings/BuildingsMesh.cpp
    render/buildings/BuildingsDraw.cpp
    render/vegetation/Vegetation.cpp
    render/vegetation/VegetationMasks.cpp
    render/vegetation/VegetationWaterMask.cpp
    render/vegetation/VegetationBuildingMask.cpp
    render/vegetation/VegetationForestMask.cpp
    render/vegetation/VegetationScatter.cpp
    render/SkinnedModelLoad.cpp
    render/SkinnedModelAnim.cpp
    render/ExplosionModel.cpp
    render/combat/SkinnedZombies.cpp
    render/combat/ExplosionFx.cpp
    render/combat/Projectiles.cpp
    render/combat/ZombieEyes.cpp
    render/SouffleFx.cpp
    render/Clouds.cpp
    render/HelicopterModel.cpp
    render/Skybox.cpp
    render/Texture.cpp
    render/TextureCache.cpp
    render/Model.cpp
    render/ModelLoader.cpp
    render/LoadedHelicopter.cpp
    render/LoadedHelicopterInstruments.cpp
    render/LoadedHelicopterDraw.cpp
    ui/Hud.cpp
    ui/hud/HudSetup.cpp
    ui/hud/HudCorners.cpp
    ui/hud/HudAlerts.cpp
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
# Le second chemin donne accès à app/ConfigModele.hpp, la copie du modèle de
# configuration fabriquée par CMake (voir le CMakeLists.txt racine).
target_include_directories(artouste PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

# Chemin des ressources (shaders, modèles) connu à la compilation. Pratique en
# développement ; un packaging Release (M7) copiera les assets à côté du binaire.
target_compile_definitions(artouste PRIVATE
    ARTOUSTE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
    ARTOUSTE_VERSION_STRING="${ARTOUSTE_VERSION_STRING}"
    # Numéro de version seul (champ VERSION du projet, ex. 0.28.0), comparé au
    # tag de la dernière release publiée par la recherche de mise à jour (voir
    # app/MiseAJour.cpp). Contrairement à ARTOUSTE_VERSION_STRING, il ne dépend
    # pas de git : une archive source hors dépôt garde le bon numéro.
    ARTOUSTE_VERSION_SEMVER="${PROJECT_VERSION}"
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
        bc7enc
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

    # Découpage d'une orthophoto en tuiles BC7 (jeu de détail d'une carte). Ne
    # touche pas à OpenGL : compression et conteneur DDS sont du calcul pur, ce
    # qui laisse l'outil utilisable sur une machine sans carte graphique.
    add_executable(orthotuiles
        tools/orthotuiles.cpp
        render/Bc7.cpp
        render/Dds.cpp
        render/tuiles/Pyramide.cpp
    )
    target_include_directories(orthotuiles PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(orthotuiles PRIVATE
        artouste_warnings
        stb_image
        bc7enc
        Threads::Threads
    )
    set_target_properties(orthotuiles PROPERTIES
        CXX_STANDARD          20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS        OFF
    )
endif()
