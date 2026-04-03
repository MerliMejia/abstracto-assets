#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <stdexcept>
#include <string>
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <vector>

struct ImportedTextureSource {
  std::string resolvedPath;
  std::vector<uint8_t> rgba;
  int width = 0;
  int height = 0;

  bool hasPath() const { return !resolvedPath.empty(); }
  bool hasEmbeddedRgba() const { return !rgba.empty() && width > 0 && height > 0; }
};

struct ImportedMaterialData {
  std::string name;
  glm::vec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
  ImportedTextureSource baseColorTexture;
  ImportedTextureSource metallicRoughnessTexture;
  ImportedTextureSource emissiveTexture;
  ImportedTextureSource occlusionTexture;
  float metallicFactor = 0.0f;
  float roughnessFactor = 1.0f;
  glm::vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};
  float occlusionStrength = 1.0f;
  tinyobj::material_t raw{};
  bool hasObjMaterial = false;

  bool hasBaseColorTexture() const {
    return baseColorTexture.hasPath() || baseColorTexture.hasEmbeddedRgba();
  }

  bool hasBaseColorTexturePath() const { return baseColorTexture.hasPath(); }
};

struct ImportedModelSubmesh {
  std::string name;
  uint32_t indexOffset = 0;
  uint32_t indexCount = 0;
  int materialIndex = -1;
  uint32_t shapeIndex = 0;
  glm::mat4 transform{1.0f};
  int nodeIndex = -1;
  int skinIndex = -1;
};

struct ImportedGeometryVertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 texCoord;
  glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 tangent = {1.0f, 0.0f, 0.0f, 1.0f};
  glm::uvec4 jointIndices{0, 0, 0, 0};
  glm::vec4 jointWeights{1.0f, 0.0f, 0.0f, 0.0f};

  bool operator==(const ImportedGeometryVertex &other) const {
    return pos == other.pos && normal == other.normal &&
           texCoord == other.texCoord && color == other.color &&
           tangent == other.tangent &&
           jointIndices == other.jointIndices &&
           jointWeights == other.jointWeights;
  }
};

template <> struct std::hash<ImportedGeometryVertex> {
  size_t operator()(ImportedGeometryVertex const &vertex) const noexcept {
    size_t seed = hash<glm::vec3>()(vertex.pos);
    seed ^= hash<glm::vec3>()(vertex.normal) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
    seed ^= hash<glm::vec2>()(vertex.texCoord) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
    seed ^= hash<glm::vec4>()(vertex.color) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
    seed ^= hash<glm::vec4>()(vertex.tangent) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
    seed ^= hash<glm::uvec4>()(vertex.jointIndices) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
    seed ^= hash<glm::vec4>()(vertex.jointWeights) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
    return seed;
  }
};

class ImportedGeometryAsset {
public:
  void setImportedGeometry(std::vector<ImportedGeometryVertex> meshVertices,
                           std::vector<uint32_t> meshIndices,
                           std::vector<ImportedModelSubmesh> meshSubmeshes,
                           std::vector<ImportedMaterialData> meshMaterials) {
    computeTangents(meshVertices, meshIndices);
    vertices = std::move(meshVertices);
    indices = std::move(meshIndices);
    submeshes = std::move(meshSubmeshes);
    materials = std::move(meshMaterials);
  }

  size_t vertexCount() const { return vertices.size(); }
  const std::vector<ImportedGeometryVertex> &vertexData() const { return vertices; }
  const std::vector<uint32_t> &indexData() const { return indices; }
  const std::vector<ImportedMaterialData> &materialsData() const { return materials; }
  std::vector<ImportedMaterialData> &mutableMaterialsData() { return materials; }
  const std::vector<ImportedModelSubmesh> &submeshData() const { return submeshes; }

private:
  static void computeTangents(std::vector<ImportedGeometryVertex> &vertices,
                              const std::vector<uint32_t> &indices) {
    if (vertices.empty() || indices.size() < 3) {
      return;
    }

    std::vector<glm::vec3> tangentAccum(vertices.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> bitangentAccum(vertices.size(), glm::vec3(0.0f));

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
      const uint32_t i0 = indices[i + 0];
      const uint32_t i1 = indices[i + 1];
      const uint32_t i2 = indices[i + 2];
      if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
        continue;
      }

      const auto &v0 = vertices[i0];
      const auto &v1 = vertices[i1];
      const auto &v2 = vertices[i2];
      const glm::vec3 edge1 = v1.pos - v0.pos;
      const glm::vec3 edge2 = v2.pos - v0.pos;
      const glm::vec2 deltaUv1 = v1.texCoord - v0.texCoord;
      const glm::vec2 deltaUv2 = v2.texCoord - v0.texCoord;
      const float det = deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;
      if (std::abs(det) < 1e-6f) {
        continue;
      }

      const float invDet = 1.0f / det;
      const glm::vec3 tangent =
          (edge1 * deltaUv2.y - edge2 * deltaUv1.y) * invDet;
      const glm::vec3 bitangent =
          (edge2 * deltaUv1.x - edge1 * deltaUv2.x) * invDet;

      tangentAccum[i0] += tangent;
      tangentAccum[i1] += tangent;
      tangentAccum[i2] += tangent;
      bitangentAccum[i0] += bitangent;
      bitangentAccum[i1] += bitangent;
      bitangentAccum[i2] += bitangent;
    }

    for (size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex) {
      const glm::vec3 normal = glm::normalize(vertices[vertexIndex].normal);
      glm::vec3 tangent = tangentAccum[vertexIndex];

      if (glm::dot(tangent, tangent) < 1e-6f) {
        tangent = std::abs(normal.z) < 0.999f
                      ? glm::normalize(
                            glm::cross(normal, glm::vec3(0.0f, 0.0f, 1.0f)))
                      : glm::normalize(
                            glm::cross(normal, glm::vec3(0.0f, 1.0f, 0.0f)));
        vertices[vertexIndex].tangent = glm::vec4(tangent, 1.0f);
        continue;
      }

      tangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));
      const glm::vec3 bitangent = bitangentAccum[vertexIndex];
      const float handedness =
          glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f ? -1.0f
                                                                  : 1.0f;
      vertices[vertexIndex].tangent = glm::vec4(tangent, handedness);
    }
  }

  std::vector<ImportedGeometryVertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<ImportedMaterialData> materials;
  std::vector<ImportedModelSubmesh> submeshes;
};

struct ImportedObjData {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
};

inline std::string
resolveImportedObjAssetPath(const std::filesystem::path &objPath,
                            const std::string &assetPath) {
  if (assetPath.empty()) {
    return {};
  }

  const std::filesystem::path path(assetPath);
  if (path.is_absolute()) {
    return path.lexically_normal().string();
  }

  return (objPath.parent_path() / path).lexically_normal().string();
}

inline std::vector<ImportedMaterialData>
buildImportedObjMaterials(const ImportedObjData &objData,
                          const std::filesystem::path &objPath) {
  std::vector<ImportedMaterialData> materials;
  materials.reserve(objData.materials.size());

  for (const auto &material : objData.materials) {
    materials.push_back(ImportedMaterialData{
        .name = material.name,
        .baseColorFactor = {material.diffuse[0], material.diffuse[1],
                            material.diffuse[2], material.dissolve},
        .baseColorTexture = {.resolvedPath = resolveImportedObjAssetPath(
                                 objPath, material.diffuse_texname)},
        .metallicFactor = 0.0f,
        .roughnessFactor = 1.0f,
        .emissiveFactor = {material.emission[0], material.emission[1],
                           material.emission[2]},
        .raw = material,
        .hasObjMaterial = true,
    });
  }

  return materials;
}

inline std::string buildImportedSubmeshName(const tinyobj::shape_t &shape,
                                            size_t shapeIndex,
                                            size_t partIndex) {
  const std::string baseName =
      shape.name.empty() ? "shape_" + std::to_string(shapeIndex) : shape.name;
  if (partIndex == 0) {
    return baseName;
  }
  return baseName + "_part_" + std::to_string(partIndex);
}

class ImportedObjVertexRef {
public:
  ImportedObjVertexRef(const tinyobj::attrib_t &attribData,
                       tinyobj::index_t objIndex)
      : attrib(&attribData), index(objIndex) {}

  glm::vec3 position() const {
    if (index.vertex_index < 0) {
      throw std::runtime_error("OBJ vertex is missing a position index");
    }

    return {attrib->vertices[3 * index.vertex_index + 0],
            attrib->vertices[3 * index.vertex_index + 1],
            attrib->vertices[3 * index.vertex_index + 2]};
  }

  glm::vec2 texCoord() const {
    if (!hasTexCoord()) {
      return {0.0f, 0.0f};
    }

    return {attrib->texcoords[2 * index.texcoord_index + 0],
            1.0f - attrib->texcoords[2 * index.texcoord_index + 1]};
  }

  glm::vec3 normal() const {
    if (!hasNormal()) {
      return {0.0f, 0.0f, 1.0f};
    }

    return {attrib->normals[3 * index.normal_index + 0],
            attrib->normals[3 * index.normal_index + 1],
            attrib->normals[3 * index.normal_index + 2]};
  }

private:
  bool hasTexCoord() const {
    return index.texcoord_index >= 0 &&
           (2 * index.texcoord_index + 1) <
               static_cast<int>(attrib->texcoords.size());
  }

  bool hasNormal() const {
    return index.normal_index >= 0 &&
           (3 * index.normal_index + 2) <
               static_cast<int>(attrib->normals.size());
  }

  const tinyobj::attrib_t *attrib = nullptr;
  tinyobj::index_t index{};
};

inline ImportedObjData loadImportedObjData(const std::string &path) {
  ImportedObjData data;
  std::string warn;
  std::string err;
  std::filesystem::path objPath(path);
  std::string basePath = objPath.parent_path().string();
  if (!basePath.empty() && basePath.back() != '/' && basePath.back() != '\\') {
    basePath.push_back(std::filesystem::path::preferred_separator);
  }

  if (!LoadObj(&data.attrib, &data.shapes, &data.materials, &warn, &err,
               path.c_str(), basePath.empty() ? nullptr : basePath.c_str())) {
    throw std::runtime_error(warn + err);
  }

  return data;
}

template <typename TVertex> struct BuiltObjGeometryData {
  std::vector<TVertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<ImportedModelSubmesh> submeshes;
  std::vector<ImportedMaterialData> materials;
};

template <typename TVertex, typename TVertexFactory>
BuiltObjGeometryData<TVertex>
buildImportedGeometryFromObj(const ImportedObjData &objData,
                             const std::filesystem::path &objPath,
                             TVertexFactory &&vertexFactory) {
  BuiltObjGeometryData<TVertex> meshData;
  meshData.materials = buildImportedObjMaterials(objData, objPath);
  std::unordered_map<TVertex, uint32_t> uniqueVertices;

  for (size_t shapeIndex = 0; shapeIndex < objData.shapes.size(); ++shapeIndex) {
    const auto &shape = objData.shapes[shapeIndex];
    size_t runningIndex = 0;
    size_t partIndex = 0;
    ImportedModelSubmesh *currentSubmesh = nullptr;

    for (size_t faceIndex = 0; faceIndex < shape.mesh.num_face_vertices.size();
         ++faceIndex) {
      const uint32_t faceVertexCount = shape.mesh.num_face_vertices[faceIndex];
      const int materialIndex = faceIndex < shape.mesh.material_ids.size()
                                    ? shape.mesh.material_ids[faceIndex]
                                    : -1;

      if (currentSubmesh == nullptr ||
          currentSubmesh->materialIndex != materialIndex) {
        meshData.submeshes.push_back(ImportedModelSubmesh{
            .name = buildImportedSubmeshName(shape, shapeIndex, partIndex++),
            .indexOffset = static_cast<uint32_t>(meshData.indices.size()),
            .indexCount = 0,
            .materialIndex = materialIndex,
            .shapeIndex = static_cast<uint32_t>(shapeIndex),
        });
        currentSubmesh = &meshData.submeshes.back();
      }

      for (uint32_t vertexIndex = 0; vertexIndex < faceVertexCount;
           ++vertexIndex) {
        const auto &index = shape.mesh.indices[runningIndex++];
        ImportedObjVertexRef objVertex(objData.attrib, index);
        TVertex vertex = vertexFactory(objVertex);
        auto [it, inserted] = uniqueVertices.try_emplace(
            vertex, static_cast<uint32_t>(meshData.vertices.size()));
        if (inserted) {
          meshData.vertices.push_back(vertex);
        }
        meshData.indices.push_back(it->second);
        currentSubmesh->indexCount++;
      }
    }
  }

  return meshData;
}

class ImportedObjGeometryAsset : public ImportedGeometryAsset {
public:
  void loadModel(const std::string &path) {
    auto objData = loadImportedObjData(path);
    auto mesh = buildImportedGeometryFromObj<ImportedGeometryVertex>(
        objData, std::filesystem::path(path),
        [](const ImportedObjVertexRef &vertex) {
          glm::vec3 normal = vertex.normal();
          if (glm::length(normal) < 0.0001f) {
            normal = {0.0f, 0.0f, 1.0f};
          } else {
            normal = glm::normalize(normal);
          }

          return ImportedGeometryVertex{
              .pos = vertex.position(),
              .normal = normal,
              .texCoord = vertex.texCoord(),
          };
        });

    setImportedGeometry(std::move(mesh.vertices), std::move(mesh.indices),
                        std::move(mesh.submeshes), std::move(mesh.materials));
  }
};
