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

mat4 toGlm(const aiMatrix4x4& a) {
    // Assimp est row-major, GLM column-major : transposition à la conversion.
    return mat4(a.a1, a.b1, a.c1, a.d1,   //
                a.a2, a.b2, a.c2, a.d2,   //
                a.a3, a.b3, a.c3, a.d3,   //
                a.a4, a.b4, a.c4, a.d4);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Vrai si le nom (insensible à la casse) contient l'une des sous-chaînes.
bool matchesAny(const std::string& nodeName, const std::vector<std::string>& needles) {
    const std::string name = lower(nodeName);
    for (const std::string& needle : needles) {
        if (name.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

constexpr float GLASS_OPACITY = 0.35f;  // translucidité des vitrages

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

void processNode(Model& model, const aiScene* scene, const aiNode* node, const mat4& parent,
                 const std::filesystem::path& baseDir, const std::vector<std::string>& skip,
                 const std::vector<std::string>& transparent) {
    const mat4 global = parent * toGlm(node->mTransformation);

    if (!matchesAny(node->mName.C_Str(), skip)) {
        const bool  isTransparent = matchesAny(node->mName.C_Str(), transparent);
        const float opacity       = isTransparent ? GLASS_OPACITY : 1.0f;
        const mat3  normalMat      = mat3(global);
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

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

            std::vector<unsigned int> indices;
            indices.reserve(mesh->mNumFaces * 3);
            for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                const aiFace& face = mesh->mFaces[f];
                for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                    indices.push_back(face.mIndices[k]);
                }
            }

            if (!indices.empty()) {
                model.addPart(Mesh(vertices, indices),
                              resolveTexture(model, scene, mesh, baseDir), isTransparent,
                              opacity);
            }
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(model, scene, node->mChildren[i], global, baseDir, skip, transparent);
    }
}

}  // namespace

Model ModelLoader::load(const std::filesystem::path& path,
                        const std::vector<std::string>& skipNameSubstrings,
                        const std::vector<std::string>& transparentNameSubstrings) {
    Model model;

    Assimp::Importer importer;
    const aiScene*   scene = importer.ReadFile(
        path.string(),
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals);

    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 ||
        scene->mRootNode == nullptr) {
        std::fprintf(stderr, "[ModelLoader] échec %s : %s\n", path.string().c_str(),
                     importer.GetErrorString());
        return model;
    }

    processNode(model, scene, scene->mRootNode, mat4(1.0f), path.parent_path(),
                skipNameSubstrings, transparentNameSubstrings);
    return model;
}

}  // namespace artouste::render
