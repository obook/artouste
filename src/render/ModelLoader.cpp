/*
 * ModelLoader.cpp
 * Implémentation du chargement d'un modèle 3D avec Assimp : parcours récursif
 * des nœuds, cumul des transformations, construction des maillages et résolution
 * des textures.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/ModelLoader.hpp"

#include "util/Math.hpp"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace artouste::render {

namespace {

/* Convertit une matrice Assimp en matrice GLM. Les deux bibliothèques rangent
   leurs coefficients différemment (lignes pour Assimp, colonnes pour GLM) : la
   conversion revient donc à transposer la matrice. */
mat4 toGlm(const aiMatrix4x4& a) {
    return mat4(a.a1, a.b1, a.c1, a.d1,   /* */
                a.a2, a.b2, a.c2, a.d2,   /* */
                a.a3, a.b3, a.c3, a.d3,   /* */
                a.a4, a.b4, a.c4, a.d4);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

/* Renvoie vrai si le nom du nœud correspond à l'un des motifs cherchés
   (comparaison insensible à la casse). Sert à repérer les nœuds à ignorer ou
   à rendre translucides d'après leur nom. Par défaut le motif est une sous-chaîne ;
   préfixé de '=', il impose une correspondance exacte du nom complet. C'est utile
   quand un nom est sous-chaîne d'un autre (par exemple "brasG" dans "avantbrasG") :
   on peut alors viser le bras seul sans emporter l'avant-bras. */
bool matchesAny(const std::string& nodeName, const std::vector<std::string>& needles) {
    const std::string name = lower(nodeName);
    for (const std::string& raw : needles) {
        const std::string needle = lower(raw);
        if (!needle.empty() && needle.front() == '=') {
            if (name == needle.substr(1)) {
                return true;
            }
        } else if (name.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

/* Opacité des vitrages : volontairement faible pour que la verrière de la bulle
   se comporte comme un vrai verre (on voit nettement au travers) plutôt que
   comme un voile gris laiteux, surtout au bas du panneau vu de la cabine. */
constexpr float GLASS_OPACITY = 0.12f;

/* Retrouve la texture associée au matériau d'un maillage. Renvoie nullptr si le
   maillage n'a pas de matériau ou pas de texture diffuse. Le chemin est exprimé
   par rapport au dossier du fichier modèle (baseDir). */
const Texture* resolveTexture(Model& model, const aiScene* scene, const aiMesh* mesh,
                              const std::filesystem::path& baseDir) {
    if (mesh->mMaterialIndex >= scene->mNumMaterials) {
        return nullptr;
    }
    aiString path;
    if (scene->mMaterials[mesh->mMaterialIndex]->GetTexture(aiTextureType_DIFFUSE, 0, &path) !=
        AI_SUCCESS) {
        return nullptr;
    }
    return model.acquireTexture(baseDir / path.C_Str());
}

/* Traite un nœud de l'arbre du modèle, puis ses enfants (récursivement). Chaque
   nœud porte une transformation locale ; en la combinant avec celle de son
   parent, on obtient la position finale de sa géométrie dans le monde. */
void processNode(Model& model, const aiScene* scene, const aiNode* node, const mat4& parent,
                 const std::filesystem::path& baseDir, const std::vector<std::string>& skip,
                 const std::vector<std::string>& transparent) {
    /* Transformation cumulée depuis la racine jusqu'à ce nœud. */
    const mat4 global = parent * toGlm(node->mTransformation);

    /* On ne construit la géométrie que si le nœud n'est pas dans la liste à ignorer. */
    if (!matchesAny(node->mName.C_Str(), skip)) {
        const bool  isTransparent = matchesAny(node->mName.C_Str(), transparent);
        const float opacity       = isTransparent ? GLASS_OPACITY : 1.0f;
        const mat3  normalMat      = mat3(global);
        /* Un nœud peut contenir plusieurs maillages : on les traite tous. */
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

            /* On recopie chaque sommet d'Assimp vers notre propre format Vertex,
               en appliquant tout de suite la transformation globale du nœud. */
            std::vector<Vertex> vertices;
            vertices.reserve(mesh->mNumVertices);
            for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
                Vertex vert;
                const aiVector3D& p = mesh->mVertices[v];
                vert.position       = vec3(global * vec4(p.x, p.y, p.z, 1.0f));
                if (mesh->mNormals != nullptr) {
                    const aiVector3D& n = mesh->mNormals[v];
                    vert.normal         = glm::normalize(normalMat * vec3(n.x, n.y, n.z));
                }
                if (mesh->HasTextureCoords(0)) {
                    vert.uv = vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
                }
                vert.color = vec3(1.0f);
                vertices.push_back(vert);
            }

            /* Chaque face est un triangle (Assimp a triangulé le modèle) ;
               on récupère les trois indices de ses sommets. */
            std::vector<unsigned int> indices;
            indices.reserve(mesh->mNumFaces * 3);
            for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                const aiFace& face = mesh->mFaces[f];
                for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                    indices.push_back(face.mIndices[k]);
                }
            }

            /* On n'ajoute la partie que si elle contient vraiment de la géométrie. */
            if (!indices.empty()) {
                model.recordVertices(vertices);
                model.addPart(Mesh(vertices, indices),
                              resolveTexture(model, scene, mesh, baseDir), isTransparent,
                              opacity);
            }
        }
    }

    /* On descend dans les nœuds enfants en leur transmettant notre transformation. */
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(model, scene, node->mChildren[i], global, baseDir, skip, transparent);
    }
}

}  /* namespace */

Model ModelLoader::load(const std::filesystem::path& path,
                        const std::vector<std::string>& skipNameSubstrings,
                        const std::vector<std::string>& transparentNameSubstrings) {
    Model model;

    /* Assimp lit le fichier et nous renvoie la scène. Les options demandent de
       découper les faces en triangles, de fusionner les sommets identiques et
       de calculer des normales lissées (utiles pour l'éclairage). */
    Assimp::Importer importer;
    const aiScene*   scene = importer.ReadFile(
        path.string(),
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals);

    /* On vérifie que le chargement a réussi et que la scène est exploitable. */
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 ||
        scene->mRootNode == nullptr) {
        std::fprintf(stderr, "[ModelLoader] échec %s : %s\n", path.string().c_str(),
                     importer.GetErrorString());
        return model;
    }

    /* On parcourt l'arbre à partir de sa racine, avec la matrice identité comme
       transformation de départ et le dossier du fichier comme base pour les
       textures. */
    processNode(model, scene, scene->mRootNode, mat4(1.0f), path.parent_path(),
                skipNameSubstrings, transparentNameSubstrings);
    return model;
}

}  /* namespace artouste::render */
