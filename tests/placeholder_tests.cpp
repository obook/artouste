// Placeholder M0 : confirme que Catch2 est correctement câblé.
// Sera supprimé à l'arrivée des vrais tests (flight_model_tests.cpp, M3).

#include <catch2/catch_test_macros.hpp>

TEST_CASE("infrastructure Catch2 opérationnelle", "[meta]") {
    REQUIRE(2 + 2 == 4);
}
