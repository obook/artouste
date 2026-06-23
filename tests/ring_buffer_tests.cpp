/*
 * ring_buffer_tests.cpp
 * Vérifie le tampon circulaire thread-safe (audio::RingBuffer) : aller-retour
 * simple, bouclage (wrap-around), tampon vide, et déblocage par close().
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "audio/RingBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <cstring>
#include <thread>

using artouste::audio::RingBuffer;

TEST_CASE("aller-retour : on relit ce qu'on a écrit", "[ringbuffer]") {
    RingBuffer rb(16);
    const unsigned char in[] = {1, 2, 3, 4, 5};
    REQUIRE(rb.write(in, 5));
    REQUIRE(rb.available() == 5);

    std::array<unsigned char, 5> out{};
    REQUIRE(rb.read(out.data(), 5) == 5);
    REQUIRE(std::memcmp(in, out.data(), 5) == 0);
    REQUIRE(rb.available() == 0);
}

TEST_CASE("lecture sur tampon vide : renvoie 0", "[ringbuffer]") {
    RingBuffer rb(8);
    std::array<unsigned char, 4> out{};
    REQUIRE(rb.read(out.data(), 4) == 0);
}

TEST_CASE("bouclage : écriture/lecture par-dessus la fin du tableau", "[ringbuffer]") {
    RingBuffer rb(8);
    const unsigned char a[] = {10, 11, 12, 13, 14, 15};
    REQUIRE(rb.write(a, 6));
    std::array<unsigned char, 4> tmp{};
    REQUIRE(rb.read(tmp.data(), 4) == 4);   /* head avance a 4, size = 2 */

    const unsigned char b[] = {20, 21, 22, 23};  /* va boucler par-dessus la fin */
    REQUIRE(rb.write(b, 4));
    REQUIRE(rb.available() == 6);

    std::array<unsigned char, 6> out{};
    REQUIRE(rb.read(out.data(), 6) == 6);
    const unsigned char expected[] = {14, 15, 20, 21, 22, 23};
    REQUIRE(std::memcmp(expected, out.data(), 6) == 0);
}

TEST_CASE("close() débloque un write en attente", "[ringbuffer]") {
    RingBuffer rb(4);
    const unsigned char fill[] = {1, 2, 3, 4};
    REQUIRE(rb.write(fill, 4));   /* tampon plein */

    /* Ce write va bloquer (plus de place) ; close() doit le libérer. */
    std::atomic<bool> result{true};
    std::thread writer([&rb, &result] {
        const unsigned char more[] = {5, 6};
        result = rb.write(more, 2);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    rb.close();
    writer.join();
    REQUIRE_FALSE(result.load());   /* assertion dans le thread principal */
}
