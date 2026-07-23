# Tests Catch2 du simulateur : vol (arcade, modèle physique, assistance),
# atterrissage automatique, configuration, tampon audio et mode zombie
# (combat, découpé par sous-système en plusieurs fichiers ci-dessous).

find_package(Threads REQUIRED)

add_executable(artouste_tests
    arcade_flight_tests.cpp
    flight_model_tests.cpp
    flight_assist_tests.cpp
    config_tests.cpp
    ring_buffer_tests.cpp
    landing_autopilot_tests.cpp
    raycast_tests.cpp
    weapon_tests.cpp
    rocket_system_tests.cpp
    projectile_system_tests.cpp
    zombie_horde_tests.cpp
    wave_manager_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/app/Config.cpp
    ${CMAKE_SOURCE_DIR}/src/app/LandingAutopilot.cpp
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
)

# physics/ se teste sans contexte graphique : on ne lie que GLM (maths pures).
target_include_directories(artouste_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(artouste_tests
    PRIVATE
        artouste_warnings
        glm::glm
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
