/*
 * model_probe.cpp
 * Petit outil de diagnostic de modèle 3D, indépendant du simulateur. Il charge
 * un fichier avec la bibliothèque Assimp, parcourt la hiérarchie de noeuds en
 * cumulant les transformations, puis affiche : les meshes, leur nombre de
 * sommets et de faces, la présence de coordonnées de texture (UV), les
 * matériaux et textures, et la boîte englobante de l'ensemble. Il sert à
 * connaître l'échelle et l'orientation réelles du modèle FlightGear avant de
 * l'intégrer au rendu : on lit des chiffres, pas encore des pixels.
 *
 * Avec un second argument, il réécrit le modèle en Wavefront OBJ (positions, UV
 * et normales, sans matériaux). Blender ne lit pas l'AC3D, et c'est le seul
 * chemin pour y cuire une occlusion ambiante sur le dépliage d'origine. On
 * réutilise ainsi le chargeur du simulateur au lieu d'écrire un second
 * analyseur de fichier ; Assimp saurait exporter, mais le projet le compile
 * sans ses exportateurs (ASSIMP_NO_EXPORT).
 *
 * Usage : ./build/bin/model_probe [chemin.ac] [sortie.obj]
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <glm/glm.hpp>

#include <cctype>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr const char* DEFAULT_PATH =
    "assets/models/Alouette-II/Models/alouette.ac";

glm::mat4 toGlm(const aiMatrix4x4& a) {
    /*
     * Assimp range ses matrices par lignes, GLM par colonnes : on transpose
     * donc au moment de la conversion.
     */
    return glm::mat4(a.a1, a.b1, a.c1, a.d1,   /* */
                     a.a2, a.b2, a.c2, a.d2,   /* */
                     a.a3, a.b3, a.c3, a.d3,   /* */
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

/* Écriture Wavefront OBJ au fil du parcours : chaque maillage est aplati dans le
   repère du modèle, avec ses UV d'origine (indispensables : la cuisson doit
   retomber sur le même atlas que la texture peinte) et ses normales. Les
   indices OBJ commencent à 1 et se suivent d'un maillage à l'autre, d'où le
   compteur 'base'. */
struct ObjWriter {
    std::FILE*   out  = nullptr;
    unsigned int base = 1;

    void add(const aiMesh* mesh, const glm::mat4& global) {
        const glm::mat3 normalMat = glm::mat3(global);
        for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
            const aiVector3D& p = mesh->mVertices[v];
            const glm::vec3   w = glm::vec3(global * glm::vec4(p.x, p.y, p.z, 1.0f));
            std::fprintf(out, "v %.6f %.6f %.6f\n", static_cast<double>(w.x),
                         static_cast<double>(w.y), static_cast<double>(w.z));
        }
        for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
            /* Un maillage sans UV reçoit tout de même une coordonnée, sans quoi
               la numérotation des faces se décalerait pour les suivants. */
            const aiVector3D uv =
                mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][v] : aiVector3D(0, 0, 0);
            std::fprintf(out, "vt %.6f %.6f\n", static_cast<double>(uv.x),
                         static_cast<double>(uv.y));
        }
        for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
            const aiVector3D& n = mesh->mNormals[v];
            const glm::vec3   w = glm::normalize(normalMat * glm::vec3(n.x, n.y, n.z));
            std::fprintf(out, "vn %.6f %.6f %.6f\n", static_cast<double>(w.x),
                         static_cast<double>(w.y), static_cast<double>(w.z));
        }
        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            std::fprintf(out, "f");
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                const unsigned int idx = base + face.mIndices[k];
                std::fprintf(out, " %u/%u/%u", idx, idx, idx);
            }
            std::fprintf(out, "\n");
        }
        base += mesh->mNumVertices;
    }
};

/* Nœuds écartés de l'export, désignés par sous-chaîne de leur nom (insensible à
   la casse), comme le fait le simulateur au chargement. Sans ce filtre, le plan
   flou du rotor (un grand disque au-dessus de la cellule) et les flotteurs
   optionnels seraient présents à la cuisson et couvriraient tout l'appareil
   d'une ombre qui n'existe pas au rendu. */
std::vector<std::string> g_skip;

bool skipped(const std::string& name) {
    std::string lower = name;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (const std::string& needle : g_skip) {
        if (!needle.empty() && lower.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void processNode(const aiScene* scene, const aiNode* node, const glm::mat4& parent,
                 Bounds& bounds, int depth, ObjWriter* obj = nullptr) {
    const glm::mat4 global = parent * toGlm(node->mTransformation);
    const bool      ecarte = skipped(node->mName.C_Str());

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

        if (obj != nullptr && !ecarte) {
            obj->add(mesh, global);
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(scene, node->mChildren[i], global, bounds, depth + 1, obj);
    }
}

}  /* namespace */

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

    /* Second argument : on réécrit le modèle en OBJ pendant le parcours. Le
       troisième, facultatif, liste les nœuds à écarter, séparés par des virgules
       (par exemple "blur,disc,flotteur,barre,roue"). */
    ObjWriter obj;
    if (argc > 3) {
        std::string reste = argv[3];
        while (!reste.empty()) {
            const std::size_t virgule = reste.find(',');
            g_skip.push_back(reste.substr(0, virgule));
            reste = (virgule == std::string::npos) ? "" : reste.substr(virgule + 1);
        }
    }
    if (argc > 2) {
        obj.out = std::fopen(argv[2], "w");
        if (obj.out == nullptr) {
            std::printf("Écriture impossible : %s\n", argv[2]);
            return 1;
        }
    }

    Bounds bounds;
    processNode(scene, scene->mRootNode, glm::mat4(1.0f), bounds, 0,
                obj.out != nullptr ? &obj : nullptr);

    if (obj.out != nullptr) {
        std::fclose(obj.out);
        std::printf("\nOBJ écrit : %s (%u sommets)\n", argv[2], obj.base - 1);
    }

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
