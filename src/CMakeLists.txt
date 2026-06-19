# Cible exécutable principale du simulateur.

add_executable(artouste
    main.cpp
    app/Application.cpp
    input/Keyboard.cpp
    input/Gamepad.cpp
    input/InputSystem.cpp
    physics/RigidBody.cpp
    physics/FlightModel.cpp
    render/Shader.cpp
    render/Mesh.cpp
    render/Primitives.cpp
    render/Camera.cpp
    render/Terrain.cpp
    render/HelicopterModel.cpp
    render/Skybox.cpp
    render/Texture.cpp
    render/Model.cpp
    render/ModelLoader.cpp
    render/LoadedHelicopter.cpp
    ui/Hud.cpp
    audio/AudioEngine.cpp
)

# Racine des includes du projet : permet les chemins "render/...", "util/...".
target_include_directories(artouste PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Chemin des ressources (shaders, modèles) connu à la compilation. Pratique en
# développement ; un packaging Release (M7) copiera les assets à côté du binaire.
target_compile_definitions(artouste PRIVATE
    ARTOUSTE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
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

# clangd / IDE
set_target_properties(artouste PROPERTIES
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

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
