# Tests Catch2 - pour l'instant un simple placeholder qui vérifie que
# l'infrastructure (Catch2, ctest) est en place. Le vrai test du modèle
# de vol arrivera au jalon M3.

add_executable(artouste_tests
    placeholder_tests.cpp
    arcade_flight_tests.cpp
    flight_model_tests.cpp
    flight_assist_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/ArcadeFlight.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/RigidBody.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/FlightModel.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/FlightAssist.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/Turbine.cpp
)

# physics/ se teste sans contexte graphique : on ne lie que GLM (maths pures).
target_include_directories(artouste_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(artouste_tests
    PRIVATE
        artouste_warnings
        glm::glm
        Catch2::Catch2WithMain
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
