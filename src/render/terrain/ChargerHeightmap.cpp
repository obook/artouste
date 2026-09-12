/*
 * ChargerHeightmap.cpp
 * Lecture du relief d'ensemble (voir ChargerHeightmap.hpp).
 *
 * Auteur : O. Booklage
 * Date : septembre 2026
 * Licence : GPL v2
 */

#include "render/terrain/ChargerHeightmap.hpp"

#include <stb_image.h>

#include <cstdio>
#include <fstream>
#include <system_error>

namespace artouste::render {

namespace {

/* Le .bin fait foi ou il échoue, pas de repli. */
std::vector<float> lireBin(const std::filesystem::path& chemin, std::size_t points) {
    const std::uintmax_t attendu = static_cast<std::uintmax_t>(points) * sizeof(float);
    std::error_code      ec;
    const std::uintmax_t taille = std::filesystem::file_size(chemin, ec);
    if (ec || taille != attendu) {
        std::fprintf(stderr, "[Terrain] %s : %llu octets, attendu %llu.\n",
                     chemin.string().c_str(), static_cast<unsigned long long>(taille),
                     static_cast<unsigned long long>(attendu));
        return {};
    }

    std::ifstream in(chemin, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "[Terrain] %s illisible.\n", chemin.string().c_str());
        return {};
    }
    std::vector<float> altitudes(points);
    in.read(reinterpret_cast<char*>(altitudes.data()), static_cast<std::streamsize>(attendu));
    if (static_cast<std::uintmax_t>(in.gcount()) != attendu) {
        std::fprintf(stderr, "[Terrain] %s tronqué à la lecture.\n", chemin.string().c_str());
        return {};
    }
    return altitudes;
}

/* Sans retournement vertical : la rangée 0 reste au nord. */
std::vector<float> lirePng(const std::filesystem::path& chemin, int cols, int rows,
                           float elevMin, float elevMax) {
    stbi_set_flip_vertically_on_load(0);
    int             w = 0, h = 0, canaux = 0;
    unsigned short* pixels = stbi_load_16(chemin.string().c_str(), &w, &h, &canaux, 1);
    if (pixels == nullptr || w != cols || h != rows) {
        std::fprintf(stderr, "[Terrain] %s illisible ou de taille inattendue.\n",
                     chemin.string().c_str());
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return {};
    }

    const float        etendue = elevMax - elevMin;
    std::vector<float> altitudes(static_cast<std::size_t>(cols) *
                                 static_cast<std::size_t>(rows));
    for (std::size_t k = 0; k < altitudes.size(); ++k) {
        altitudes[k] = elevMin + (static_cast<float>(pixels[k]) / 65535.0f) * etendue;
    }
    stbi_image_free(pixels);
    return altitudes;
}

} /* namespace */

std::vector<float> chargerHeightmap(const std::filesystem::path& dossierCarte, int cols,
                                    int rows, float elevMin, float elevMax) {
    if (cols < 2 || rows < 2) {
        return {};
    }
    const std::size_t points =
        static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);

    const std::filesystem::path bin = dossierCarte / "heightmap.bin";
    if (std::filesystem::exists(bin)) {
        return lireBin(bin, points);
    }

    const std::filesystem::path png = dossierCarte / "heightmap.png";
    if (std::filesystem::exists(png)) {
        return lirePng(png, cols, rows, elevMin, elevMax);
    }

    std::fprintf(stderr, "[Terrain] relief absent (ni heightmap.bin ni heightmap.png) dans %s.\n",
                 dossierCarte.string().c_str());
    return {};
}

} /* namespace artouste::render */
