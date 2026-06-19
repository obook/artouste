// Outil de diagnostic de modèle 3D (hors simulateur).
//
// Charge un fichier via Assimp, parcourt la hiérarchie de noeuds en cumulant
// les transformations, et affiche : meshes, nombre de sommets/faces, présence
// d'UV, matériaux/textures, et la bounding box assemblée. Sert à connaître
// l'échelle et l'orientation réelles du modèle FlightGear avant de l'intégrer
// au rendu (travail "à l'aveugle" : on lit des chiffres, pas des pixels).
//
// Usage : ./build/bin/model_probe [chemin.ac]

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <glm/glm.hpp>

#include <cstdio>
#include <limits>
#include <string>

namespace {

constexpr const char* DEFAULT_PATH =
    "assets/models/Alouette-II/Models/alouette.ac";

glm::mat4 toGlm(const aiMatrix4x4& a) {
    // Assimp est row-major, GLM column-major : on transpose en convertissant.
    return glm::mat4(a.a1, a.b1, a.c1, a.d1,   //
                     a.a2, a.b2, a.c2, a.d2,   //
                     a.a3, a.b3, a.c3, a.d3,   //
                     a.a4, a.b4, a.c4, a.d4);
}

struct Bounds {
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{std::numeric_limits<float>::lowest()};

    void add(const glm::vec3& p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }
};

void processNode(const aiScene* scene, const aiNode* node, const glm::mat4& parent,
                 Bounds& bounds, int depth) {
    const glm::mat4 global = parent * toGlm(node->mTransformation);

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        Bounds local;
        for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
            const aiVector3D& p = mesh->mVertices[v];
            const glm::vec3   world = glm::vec3(global * glm::vec4(p.x, p.y, p.z, 1.0f));
            bounds.add(world);
            local.add(world);
        }

        const bool hasUV = mesh->HasTextureCoords(0);
        std::string texture = "(aucune)";
        if (mesh->mMaterialIndex < scene->mNumMaterials) {
            aiString path;
            if (scene->mMaterials[mesh->mMaterialIndex]->GetTexture(
                    aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
                texture = path.C_Str();
            }
        }

        std::printf("  %*smesh \"%s\" : %u sommets, %u faces, UV:%s, tex:%s\n", depth * 2, "",
                    mesh->mName.C_Str(), mesh->mNumVertices, mesh->mNumFaces,
                    hasUV ? "oui" : "non", texture.c_str());
        std::printf("  %*s  bbox locale [%.2f %.2f %.2f] -> [%.2f %.2f %.2f]\n", depth * 2, "",
                    static_cast<double>(local.min.x), static_cast<double>(local.min.y),
                    static_cast<double>(local.min.z), static_cast<double>(local.max.x),
                    static_cast<double>(local.max.y), static_cast<double>(local.max.z));
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(scene, node->mChildren[i], global, bounds, depth + 1);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string path = (argc > 1) ? argv[1] : DEFAULT_PATH;

    Assimp::Importer importer;
    const aiScene*   scene = importer.ReadFile(
        path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals);

    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 ||
        scene->mRootNode == nullptr) {
        std::printf("Échec du chargement : %s\n", importer.GetErrorString());
        return 1;
    }

    std::printf("=== %s ===\n", path.c_str());
    std::printf("meshes:%u  matériaux:%u  textures embarquées:%u\n\n", scene->mNumMeshes,
                scene->mNumMaterials, scene->mNumTextures);

    Bounds bounds;
    processNode(scene, scene->mRootNode, glm::mat4(1.0f), bounds, 0);

    const glm::vec3 size = bounds.max - bounds.min;
    std::printf("\nBounding box assemblée :\n");
    std::printf("  min [%.3f %.3f %.3f]\n", static_cast<double>(bounds.min.x),
                static_cast<double>(bounds.min.y), static_cast<double>(bounds.min.z));
    std::printf("  max [%.3f %.3f %.3f]\n", static_cast<double>(bounds.max.x),
                static_cast<double>(bounds.max.y), static_cast<double>(bounds.max.z));
    std::printf("  dimensions X:%.3f  Y:%.3f  Z:%.3f (m ?)\n", static_cast<double>(size.x),
                static_cast<double>(size.y), static_cast<double>(size.z));
    return 0;
}
