# Tests Catch2 - pour l'instant un simple placeholder qui vérifie que
# l'infrastructure (Catch2, ctest) est en place. Le vrai test du modèle
# de vol arrivera au jalon M3.

add_executable(artouste_tests
    placeholder_tests.cpp
    arcade_flight_tests.cpp
    flight_model_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/ArcadeFlight.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/RigidBody.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/FlightModel.cpp
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

include(Catch)
catch_discover_tests(artouste_tests)
