/*
 * placeholder_tests.cpp
 * Test temporaire qui vérifie que Catch2 est bien branché et compile.
 * Il sera retiré une fois les vrais tests en place (flight_model_tests.cpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include <catch2/catch_test_macros.hpp>

TEST_CASE("infrastructure Catch2 opérationnelle", "[meta]") {
    REQUIRE(2 + 2 == 4);
}
