# Tests Catch2 du simulateur : vol (arcade, modèle physique, assistance),
# atterrissage automatique, configuration, recherche de mise à jour, tampon
# audio, cache de textures,
# tuiles d'orthophoto et mode zombie (combat, découpé par sous-système en
# plusieurs fichiers ci-dessous).

find_package(Threads REQUIRED)

add_executable(artouste_tests
    arcade_flight_tests.cpp
    flight_model_tests.cpp
    flight_assist_tests.cpp
    config_tests.cpp
    cycle_jour_nuit_tests.cpp
    mise_a_jour_tests.cpp
    ring_buffer_tests.cpp
    landing_autopilot_tests.cpp
    raycast_tests.cpp
    weapon_tests.cpp
    rocket_system_tests.cpp
    projectile_system_tests.cpp
    zombie_horde_tests.cpp
    wave_manager_tests.cpp
    ground_impact_tests.cpp
    texture_cache_tests.cpp
    ortho_tiles_tests.cpp
    fabrique_tuiles_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/app/Config.cpp
    ${CMAKE_SOURCE_DIR}/src/app/CycleJourNuit.cpp
    # Compilé sans ARTOUSTE_HAS_CURL : seules les fonctions d'analyse (tag et
    # numéros de version) entrent dans les tests, pas la requête réseau.
    ${CMAKE_SOURCE_DIR}/src/app/MiseAJour.cpp
    ${CMAKE_SOURCE_DIR}/src/app/cartes/FabriqueTuiles.cpp
    ${CMAKE_SOURCE_DIR}/src/app/LandingAutopilot.cpp
    ${CMAKE_SOURCE_DIR}/src/app/combat/CombatMode.cpp
    ${CMAKE_SOURCE_DIR}/src/app/combat/ZombieHorde.cpp
    ${CMAKE_SOURCE_DIR}/src/app/combat/Weapon.cpp
    ${CMAKE_SOURCE_DIR}/src/app/combat/RocketSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/app/combat/ProjectileSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/app/combat/WaveManager.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/RingBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/ArcadeFlight.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/RigidBody.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/FlightModel.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/FlightAssist.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/Turbine.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/Raycast.cpp
    ${CMAKE_SOURCE_DIR}/src/render/Dds.cpp
    ${CMAKE_SOURCE_DIR}/src/render/Bc7.cpp
    ${CMAKE_SOURCE_DIR}/src/render/tuiles/Pyramide.cpp
)

# physics/ se teste sans contexte graphique : on ne lie que GLM (maths pures).
target_include_directories(artouste_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated  # app/ConfigModele.hpp, fabriqué par CMake
)

# Le vrai modèle de configuration livré avec le jeu : les tests de cohérence
# comparent ses clés à celles que le chargeur sait lire (app::clesConnues), pour
# qu'une option ne puisse pas exister d'un côté sans l'autre.
target_compile_definitions(artouste_tests PRIVATE
    ARTOUSTE_CONFIG_MODELE="${CMAKE_SOURCE_DIR}/assets/config.default.txt"
)

target_link_libraries(artouste_tests
    PRIVATE
        artouste_warnings
        glm::glm
        stb_image
        bc7enc
        Catch2::Catch2WithMain
        Threads::Threads
)

set_target_properties(artouste_tests PROPERTIES
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

# On enregistre toute la suite comme un seul test ctest (un seul appel du binaire),
# plutôt qu'un test par cas (catch_discover_tests). Cela évite la réinvocation par
# nom de CTest, qui sous Windows corrompt les noms de tests accentués (é, è, à) dans
# les arguments de ligne de commande et fait échouer le filtrage.
add_test(NAME artouste_tests COMMAND artouste_tests)
