#include "assets/CharacterRecipeGenerator.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <array>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <utility>

constexpr uint32_t CHARACTER_TORSO_MATERIAL_INDEX = 0;
constexpr uint32_t CHARACTER_HEAD_MATERIAL_INDEX = 1;
constexpr uint32_t CHARACTER_ARM_MATERIAL_INDEX = 2;
constexpr uint32_t CHARACTER_LEG_MATERIAL_INDEX = 3;
constexpr float CHARACTER_SOCKET_EPSILON = 0.03f;

struct WeightedJoints {
  glm::uvec4 indices{0, 0, 0, 0};
  glm::vec4 weights{1.0f, 0.0f, 0.0f, 0.0f};
};

struct SubmeshRange {
  std::string name;
  uint32_t indexOffset = 0;
  uint32_t indexCount = 0;
  int materialIndex = -1;
};

struct MeshBuilder {
  std::vector<ImportedGeometryVertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<SubmeshRange> ranges;

  void beginSubmesh(const std::string &name, int materialIndex) {
    ranges.push_back(SubmeshRange{
        .name = name,
        .indexOffset = static_cast<uint32_t>(indices.size()),
        .indexCount = 0,
        .materialIndex = materialIndex,
    });
  }

  void addTriangle(uint32_t a, uint32_t b, uint32_t c) {
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
    if (!ranges.empty()) {
      ranges.back().indexCount += 3;
    }
  }

  void finishSubmesh(std::vector<ImportedModelSubmesh> &submeshes,
                     int skinIndex = 0) {
    if (ranges.empty()) {
      return;
    }

    const SubmeshRange range = ranges.back();
    ranges.pop_back();
    if (range.indexCount == 0) {
      return;
    }

    submeshes.push_back(ImportedModelSubmesh{
        .name = range.name,
        .indexOffset = range.indexOffset,
        .indexCount = range.indexCount,
        .materialIndex = range.materialIndex,
        .shapeIndex = static_cast<uint32_t>(submeshes.size()),
        .skinIndex = skinIndex,
    });
  }
};

struct CharacterRigState {
  ImportedSkeletonData skeleton;
  int rootIndex = -1;
  int pelvisIndex = -1;
  int spineIndex = -1;
  int torsoTopIndex = -1;
  std::vector<int> neckIndices;
  std::vector<int> headIndices;
  std::vector<int> shoulderIndices;
  std::vector<int> upperArmIndices;
  std::vector<int> lowerArmIndices;
  std::vector<int> handIndices;
  std::vector<int> hipIndices;
  std::vector<int> upperLegIndices;
  std::vector<int> lowerLegIndices;
  std::vector<int> footIndices;
};

static glm::mat4 composeTransform(const ImportedNodeTransform &transform) {
  return glm::translate(glm::mat4(1.0f), transform.translation) *
         glm::toMat4(transform.rotation) *
         glm::scale(glm::mat4(1.0f), transform.scale);
}

static std::vector<float> distributedBandOffsets(int count, float spacing) {
  std::vector<float> offsets;
  offsets.reserve(static_cast<size_t>(std::max(count, 0)));
  const float center = 0.5f * static_cast<float>(count - 1);
  for (int index = 0; index < count; ++index) {
    offsets.push_back((static_cast<float>(index) - center) * spacing);
  }
  return offsets;
}

static int addNode(CharacterRigState &rig, const std::string &name, int parentIndex,
                   const glm::vec3 &translation) {
  const int nodeIndex = static_cast<int>(rig.skeleton.nodes.size());
  rig.skeleton.nodes.push_back(ImportedSkeletonNode{
      .name = name,
      .parentIndex = parentIndex,
      .localBindTransform =
          ImportedNodeTransform{.translation = translation},
  });
  if (parentIndex >= 0) {
    rig.skeleton.nodes[static_cast<size_t>(parentIndex)].childIndices.push_back(
        nodeIndex);
  } else {
    rig.skeleton.sceneRootNodeIndices.push_back(nodeIndex);
  }
  return nodeIndex;
}

static std::vector<glm::mat4>
computeWorldTransforms(const ImportedSkeletonData &skeleton) {
  std::vector<glm::mat4> worldTransforms(skeleton.nodes.size(), glm::mat4(1.0f));
  for (size_t nodeIndex = 0; nodeIndex < skeleton.nodes.size(); ++nodeIndex) {
    glm::mat4 local = composeTransform(
        skeleton.nodes[nodeIndex].localBindTransform);
    int parentIndex = skeleton.nodes[nodeIndex].parentIndex;
    if (parentIndex >= 0) {
      worldTransforms[nodeIndex] =
          worldTransforms[static_cast<size_t>(parentIndex)] * local;
    } else {
      worldTransforms[nodeIndex] = local;
    }
  }
  return worldTransforms;
}

static glm::vec3 positionFromMatrix(const glm::mat4 &matrix) {
  return glm::vec3(matrix[3]);
}

static glm::vec3 safeNormalize(const glm::vec3 &value,
                               const glm::vec3 &fallback) {
  const float length = glm::length(value);
  if (length <= 1e-5f) {
    return fallback;
  }
  return value / length;
}

static WeightedJoints singleJoint(uint32_t jointIndex) {
  return WeightedJoints{
      .indices = glm::uvec4(jointIndex, 0, 0, 0),
      .weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
  };
}

static WeightedJoints blendJoints(uint32_t firstJoint, uint32_t secondJoint,
                                  float blendFactor) {
  const float clampedBlend = glm::clamp(blendFactor, 0.0f, 1.0f);
  return WeightedJoints{
      .indices = glm::uvec4(firstJoint, secondJoint, 0, 0),
      .weights = glm::vec4(1.0f - clampedBlend, clampedBlend, 0.0f, 0.0f),
  };
}

static ImportedGeometryVertex makeVertex(const glm::vec3 &position,
                                         const glm::vec3 &normal,
                                         const glm::vec2 &uv,
                                         const WeightedJoints &weights) {
  return ImportedGeometryVertex{
      .pos = position,
      .normal = glm::length(normal) > 1e-6f ? glm::normalize(normal)
                                            : glm::vec3(0.0f, 1.0f, 0.0f),
      .texCoord = uv,
      .jointIndices = weights.indices,
      .jointWeights = weights.weights,
  };
}

template <typename WeightFn>
static void appendBox(MeshBuilder &builder, const glm::mat4 &transform,
                      const glm::vec3 &halfExtents, WeightFn &&weightForPosition) {
  const std::array<glm::vec3, 6> normals = {
      glm::vec3(1.0f, 0.0f, 0.0f),  glm::vec3(-1.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 1.0f, 0.0f),  glm::vec3(0.0f, -1.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, 1.0f),  glm::vec3(0.0f, 0.0f, -1.0f),
  };
  const std::array<std::array<glm::vec3, 4>, 6> faceVertices = {{
      {{{1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, -1.0f}}},
      {{{-1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, 1.0f}}},
      {{{-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f}}},
      {{{-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, -1.0f},
        {-1.0f, -1.0f, -1.0f}}},
      {{{-1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f}}},
      {{{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, -1.0f}}},
  }};
  const std::array<glm::vec2, 4> uvs = {
      glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
      glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)};
  const glm::mat3 normalTransform = glm::inverseTranspose(glm::mat3(transform));

  for (size_t faceIndex = 0; faceIndex < faceVertices.size(); ++faceIndex) {
    const uint32_t baseIndex = static_cast<uint32_t>(builder.vertices.size());
    const glm::vec3 transformedNormal =
        glm::normalize(normalTransform * normals[faceIndex]);
    for (size_t cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
      const glm::vec3 localPosition =
          faceVertices[faceIndex][cornerIndex] * halfExtents;
      const glm::vec3 worldPosition =
          glm::vec3(transform * glm::vec4(localPosition, 1.0f));
      builder.vertices.push_back(makeVertex(
          worldPosition, transformedNormal, uvs[cornerIndex],
          weightForPosition(localPosition)));
    }
    builder.addTriangle(baseIndex + 0, baseIndex + 1, baseIndex + 2);
    builder.addTriangle(baseIndex + 0, baseIndex + 2, baseIndex + 3);
  }
}

template <typename WeightFn>
static void appendTaperedTorso(MeshBuilder &builder, const glm::vec3 &center,
                               const glm::vec3 &bottomHalfExtents,
                               const glm::vec3 &topHalfExtents,
                               WeightFn &&weightForPosition) {
  const float halfHeight = std::max(bottomHalfExtents.y, topHalfExtents.y);
  const std::array<glm::vec3, 8> corners = {
      glm::vec3(-bottomHalfExtents.x, -halfHeight, -bottomHalfExtents.z),
      glm::vec3(bottomHalfExtents.x, -halfHeight, -bottomHalfExtents.z),
      glm::vec3(bottomHalfExtents.x, -halfHeight, bottomHalfExtents.z),
      glm::vec3(-bottomHalfExtents.x, -halfHeight, bottomHalfExtents.z),
      glm::vec3(-topHalfExtents.x, halfHeight, -topHalfExtents.z),
      glm::vec3(topHalfExtents.x, halfHeight, -topHalfExtents.z),
      glm::vec3(topHalfExtents.x, halfHeight, topHalfExtents.z),
      glm::vec3(-topHalfExtents.x, halfHeight, topHalfExtents.z),
  };
  const std::array<std::array<uint32_t, 4>, 6> faces = {{
      {{1, 2, 6, 5}},
      {{3, 0, 4, 7}},
      {{4, 5, 6, 7}},
      {{0, 3, 2, 1}},
      {{2, 3, 7, 6}},
      {{0, 1, 5, 4}},
  }};
  const std::array<glm::vec2, 4> uvs = {
      glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
      glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)};

  for (const auto &face : faces) {
    const glm::vec3 p0 = corners[face[0]];
    const glm::vec3 p1 = corners[face[1]];
    const glm::vec3 p2 = corners[face[2]];
    const glm::vec3 faceNormal =
        glm::normalize(glm::cross(p1 - p0, p2 - p0));
    const uint32_t baseIndex = static_cast<uint32_t>(builder.vertices.size());

    for (size_t cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
      const glm::vec3 localPosition = corners[face[cornerIndex]];
      builder.vertices.push_back(makeVertex(
          center + localPosition, faceNormal, uvs[cornerIndex],
          weightForPosition(localPosition)));
    }

    builder.addTriangle(baseIndex + 0, baseIndex + 1, baseIndex + 2);
    builder.addTriangle(baseIndex + 0, baseIndex + 2, baseIndex + 3);
  }
}

template <typename WeightFn>
static void appendTaperedPrism(MeshBuilder &builder, const glm::mat4 &transform,
                               const glm::vec3 &bottomHalfExtents,
                               const glm::vec3 &topHalfExtents,
                               WeightFn &&weightForPosition) {
  const float bottomHeight = bottomHalfExtents.y;
  const float topHeight = topHalfExtents.y;
  const std::array<glm::vec3, 8> corners = {
      glm::vec3(-bottomHalfExtents.x, -bottomHeight, -bottomHalfExtents.z),
      glm::vec3(bottomHalfExtents.x, -bottomHeight, -bottomHalfExtents.z),
      glm::vec3(bottomHalfExtents.x, -bottomHeight, bottomHalfExtents.z),
      glm::vec3(-bottomHalfExtents.x, -bottomHeight, bottomHalfExtents.z),
      glm::vec3(-topHalfExtents.x, topHeight, -topHalfExtents.z),
      glm::vec3(topHalfExtents.x, topHeight, -topHalfExtents.z),
      glm::vec3(topHalfExtents.x, topHeight, topHalfExtents.z),
      glm::vec3(-topHalfExtents.x, topHeight, topHalfExtents.z),
  };
  const std::array<std::array<uint32_t, 4>, 6> faces = {{
      {{1, 2, 6, 5}},
      {{3, 0, 4, 7}},
      {{4, 5, 6, 7}},
      {{0, 3, 2, 1}},
      {{2, 3, 7, 6}},
      {{0, 1, 5, 4}},
  }};
  const std::array<glm::vec2, 4> uvs = {
      glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
      glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)};
  const glm::mat3 normalTransform = glm::inverseTranspose(glm::mat3(transform));

  for (const auto &face : faces) {
    const glm::vec3 p0 = corners[face[0]];
    const glm::vec3 p1 = corners[face[1]];
    const glm::vec3 p2 = corners[face[2]];
    const glm::vec3 localNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
    const glm::vec3 transformedNormal =
        glm::normalize(normalTransform * localNormal);
    const uint32_t baseIndex = static_cast<uint32_t>(builder.vertices.size());

    for (size_t cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
      const glm::vec3 localPosition = corners[face[cornerIndex]];
      const glm::vec3 worldPosition =
          glm::vec3(transform * glm::vec4(localPosition, 1.0f));
      builder.vertices.push_back(makeVertex(
          worldPosition, transformedNormal, uvs[cornerIndex],
          weightForPosition(localPosition)));
    }

    builder.addTriangle(baseIndex + 0, baseIndex + 1, baseIndex + 2);
    builder.addTriangle(baseIndex + 0, baseIndex + 2, baseIndex + 3);
  }
}

template <typename WeightFn>
static void appendSphere(MeshBuilder &builder, const glm::vec3 &center,
                         const glm::vec3 &radii, int latitudeSegments,
                         int longitudeSegments, WeightFn &&weightForPosition) {
  for (int latitude = 0; latitude <= latitudeSegments; ++latitude) {
    const float v = static_cast<float>(latitude) /
                    static_cast<float>(latitudeSegments);
    const float theta = v * glm::pi<float>();
    const float sinTheta = std::sin(theta);
    const float cosTheta = std::cos(theta);

    for (int longitude = 0; longitude <= longitudeSegments; ++longitude) {
      const float u = static_cast<float>(longitude) /
                      static_cast<float>(longitudeSegments);
      const float phi = u * glm::two_pi<float>();
      const float sinPhi = std::sin(phi);
      const float cosPhi = std::cos(phi);

      const glm::vec3 unitPosition(sinTheta * cosPhi, cosTheta,
                                   sinTheta * sinPhi);
      const glm::vec3 localPosition = unitPosition * radii;
      const glm::vec3 normal =
          glm::normalize(unitPosition / glm::max(radii, glm::vec3(0.001f)));
      builder.vertices.push_back(makeVertex(center + localPosition, normal,
                                            glm::vec2(u, 1.0f - v),
                                            weightForPosition(localPosition)));
    }
  }

  const uint32_t rowVertexCount =
      static_cast<uint32_t>(longitudeSegments + 1);
  const uint32_t baseIndex =
      static_cast<uint32_t>(builder.vertices.size()) -
      static_cast<uint32_t>((latitudeSegments + 1) * (longitudeSegments + 1));
  for (int latitude = 0; latitude < latitudeSegments; ++latitude) {
    for (int longitude = 0; longitude < longitudeSegments; ++longitude) {
      const uint32_t i0 =
          baseIndex + static_cast<uint32_t>(latitude) * rowVertexCount +
          static_cast<uint32_t>(longitude);
      const uint32_t i1 = i0 + 1;
      const uint32_t i2 = i0 + rowVertexCount;
      const uint32_t i3 = i2 + 1;
      builder.addTriangle(i0, i2, i1);
      builder.addTriangle(i1, i2, i3);
    }
  }
}

template <typename WeightFn>
static void appendLimbSegment(MeshBuilder &builder, const glm::vec3 &start,
                              const glm::vec3 &end, float radius,
                              WeightFn &&weightForPosition) {
  const glm::vec3 axis = end - start;
  const float length = glm::length(axis);
  if (length <= 1e-5f || radius <= 1e-5f) {
    return;
  }

  const glm::vec3 forward = axis / length;
  const glm::vec3 upHint =
      std::abs(glm::dot(forward, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.96f
          ? glm::vec3(0.0f, 0.0f, 1.0f)
          : glm::vec3(0.0f, 1.0f, 0.0f);
  const glm::vec3 right = glm::normalize(glm::cross(upHint, forward));
  const glm::vec3 up = glm::normalize(glm::cross(forward, right));
  const std::array<float, 5> stations = {0.0f, 0.22f, 0.50f, 0.78f, 1.0f};
  const std::array<float, 5> radiiScale = {0.96f, 1.0f, 1.0f, 1.0f, 0.96f};
  constexpr int radialSegments = 8;

  const uint32_t baseIndex = static_cast<uint32_t>(builder.vertices.size());
  for (size_t stationIndex = 0; stationIndex < stations.size(); ++stationIndex) {
    const float station = stations[stationIndex];
    const glm::vec3 center = start + forward * (length * station);
    const float stationRadius = radius * radiiScale[stationIndex];
    for (int radialIndex = 0; radialIndex <= radialSegments; ++radialIndex) {
      const float angle =
          glm::two_pi<float>() * static_cast<float>(radialIndex) /
          static_cast<float>(radialSegments);
      const glm::vec3 radialDirection =
          std::cos(angle) * right + std::sin(angle) * up;
      const glm::vec3 worldPosition = center + radialDirection * stationRadius;
      builder.vertices.push_back(makeVertex(
          worldPosition, radialDirection,
          glm::vec2(static_cast<float>(radialIndex) /
                        static_cast<float>(radialSegments),
                    station),
          weightForPosition(station)));
    }
  }

  const uint32_t ringVertexCount = radialSegments + 1;
  for (size_t stationIndex = 0; stationIndex + 1 < stations.size();
       ++stationIndex) {
    for (int radialIndex = 0; radialIndex < radialSegments; ++radialIndex) {
      const uint32_t i0 = baseIndex +
                          static_cast<uint32_t>(stationIndex) * ringVertexCount +
                          static_cast<uint32_t>(radialIndex);
      const uint32_t i1 = i0 + 1;
      const uint32_t i2 = i0 + ringVertexCount;
      const uint32_t i3 = i2 + 1;
      builder.addTriangle(i0, i2, i1);
      builder.addTriangle(i1, i2, i3);
    }
  }
}

static glm::mat4 basisTransform(const glm::vec3 &center, const glm::vec3 &forward,
                                const glm::vec3 &halfExtents) {
  const glm::vec3 normalizedForward =
      glm::length(forward) > 1e-5f ? glm::normalize(forward)
                                   : glm::vec3(0.0f, 0.0f, 1.0f);
  const glm::vec3 upHint =
      std::abs(glm::dot(normalizedForward, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.96f
          ? glm::vec3(0.0f, 0.0f, 1.0f)
          : glm::vec3(0.0f, 1.0f, 0.0f);
  const glm::vec3 right = glm::normalize(glm::cross(upHint, normalizedForward));
  const glm::vec3 up = glm::normalize(glm::cross(normalizedForward, right));

  glm::mat4 transform(1.0f);
  transform[0] = glm::vec4(right * halfExtents.x, 0.0f);
  transform[1] = glm::vec4(up * halfExtents.y, 0.0f);
  transform[2] = glm::vec4(normalizedForward * halfExtents.z, 0.0f);
  transform[3] = glm::vec4(center, 1.0f);
  return transform;
}

static glm::mat4 axialBasisTransform(const glm::vec3 &center,
                                     const glm::vec3 &axis,
                                     const glm::vec3 &halfExtents) {
  const glm::vec3 normalizedAxis = safeNormalize(axis, glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::vec3 upHint =
      std::abs(glm::dot(normalizedAxis, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.96f
          ? glm::vec3(0.0f, 0.0f, 1.0f)
          : glm::vec3(0.0f, 1.0f, 0.0f);
  const glm::vec3 right = glm::normalize(glm::cross(upHint, normalizedAxis));
  const glm::vec3 forward = glm::normalize(glm::cross(normalizedAxis, right));

  glm::mat4 transform(1.0f);
  transform[0] = glm::vec4(right * halfExtents.x, 0.0f);
  transform[1] = glm::vec4(normalizedAxis * halfExtents.y, 0.0f);
  transform[2] = glm::vec4(forward * halfExtents.z, 0.0f);
  transform[3] = glm::vec4(center, 1.0f);
  return transform;
}

template <typename WeightFn>
static void appendJointConnector(MeshBuilder &builder,
                                 const CharacterConnectorShape &shape,
                                 const glm::vec3 &jointPosition,
                                 const glm::vec3 &axis, WeightFn &&weightForLocal) {
  const glm::vec3 direction = safeNormalize(axis, glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::vec3 center = jointPosition + direction * (shape.length * 0.5f);
  const float baseLateral = shape.baseRadius * shape.lateralFlare;
  const float tipLateral = shape.tipRadius * shape.lateralFlare;
  appendTaperedPrism(
      builder,
      axialBasisTransform(center, direction,
                          glm::vec3(1.0f, shape.length * 0.5f, 1.0f)),
      glm::vec3(baseLateral, 1.0f, shape.baseRadius),
      glm::vec3(tipLateral, 1.0f, shape.tipRadius),
      std::forward<WeightFn>(weightForLocal));
}

static CharacterRigState buildRig(const CharacterRecipe &recipe) {
  CharacterRigState rig;
  const float pelvisHeight =
      recipe.legGroup.upperLength + recipe.legGroup.lowerLength + 0.12f;
  const float torsoHeight = recipe.torsoDimensions.height;
  const float shoulderYOffset = torsoHeight * 0.18f + recipe.armGroup.offset.y;
  const float headNeckLength = 0.10f + recipe.headGroup.size.y * 0.08f;

  rig.rootIndex = addNode(rig, "root", -1, glm::vec3(0.0f));
  rig.pelvisIndex =
      addNode(rig, "pelvis", rig.rootIndex, glm::vec3(0.0f, pelvisHeight, 0.0f));
  rig.spineIndex = addNode(rig, "spine", rig.pelvisIndex,
                           glm::vec3(0.0f, torsoHeight * 0.55f, 0.0f));
  rig.torsoTopIndex = addNode(rig, "torso_top_anchor", rig.spineIndex,
                              glm::vec3(0.0f, torsoHeight * 0.45f, 0.0f));

  const std::vector<float> headOffsets =
      distributedBandOffsets(recipe.headGroup.count, recipe.headGroup.spacing);
  for (int headIndex = 0; headIndex < recipe.headGroup.count; ++headIndex) {
    const glm::vec3 neckOffset(headOffsets[static_cast<size_t>(headIndex)] +
                                   recipe.headGroup.offset.x,
                               recipe.headGroup.offset.y, recipe.headGroup.offset.z);
    const int neckNode = addNode(
        rig, "neck_" + std::to_string(headIndex), rig.torsoTopIndex, neckOffset);
    const int headNode = addNode(
        rig, "head_" + std::to_string(headIndex), neckNode,
        glm::vec3(0.0f, headNeckLength + recipe.headGroup.size.y * 0.45f, 0.0f));
    rig.neckIndices.push_back(neckNode);
    rig.headIndices.push_back(headNode);
  }

  const std::vector<float> armOffsets =
      distributedBandOffsets(recipe.armGroup.count, recipe.armGroup.spacing);
  for (int armIndex = 0; armIndex < recipe.armGroup.count; ++armIndex) {
    const float bandOffset = armOffsets[static_cast<size_t>(armIndex)] +
                             recipe.armGroup.offset.x;
    const glm::vec3 shoulderTranslation(bandOffset, shoulderYOffset,
                                        recipe.armGroup.offset.z);
    glm::vec3 direction = std::abs(bandOffset) <= CHARACTER_SOCKET_EPSILON
                              ? glm::vec3(0.0f, 0.0f, 1.0f)
                              : glm::vec3(glm::sign(bandOffset), 0.0f, 0.0f);
    const int shoulderNode = addNode(rig, "shoulder_" + std::to_string(armIndex),
                                     rig.spineIndex, shoulderTranslation);
    const int upperArmNode =
        addNode(rig, "upper_arm_" + std::to_string(armIndex), shoulderNode,
                direction * recipe.armGroup.upperLength);
    const int lowerArmNode =
        addNode(rig, "lower_arm_" + std::to_string(armIndex), upperArmNode,
                direction * recipe.armGroup.lowerLength);
    const int handNode =
        addNode(rig, "hand_" + std::to_string(armIndex), lowerArmNode,
                direction * recipe.armGroup.handLength);
    rig.shoulderIndices.push_back(shoulderNode);
    rig.upperArmIndices.push_back(upperArmNode);
    rig.lowerArmIndices.push_back(lowerArmNode);
    rig.handIndices.push_back(handNode);
  }

  const std::vector<float> legOffsets =
      distributedBandOffsets(recipe.legGroup.count, recipe.legGroup.spacing);
  for (int legIndex = 0; legIndex < recipe.legGroup.count; ++legIndex) {
    const glm::vec3 hipTranslation(
        legOffsets[static_cast<size_t>(legIndex)] + recipe.legGroup.offset.x,
        recipe.legGroup.offset.y, recipe.legGroup.offset.z);
    const int hipNode = addNode(rig, "hip_" + std::to_string(legIndex),
                                rig.pelvisIndex, hipTranslation);
    const int upperLegNode =
        addNode(rig, "upper_leg_" + std::to_string(legIndex), hipNode,
                glm::vec3(0.0f, -recipe.legGroup.upperLength, 0.0f));
    const int lowerLegNode =
        addNode(rig, "lower_leg_" + std::to_string(legIndex), upperLegNode,
                glm::vec3(0.0f, -recipe.legGroup.lowerLength, 0.0f));
    const int footNode =
        addNode(rig, "foot_" + std::to_string(legIndex), lowerLegNode,
                glm::vec3(0.0f, -0.05f, recipe.legGroup.footLength * 0.72f));
    rig.hipIndices.push_back(hipNode);
    rig.upperLegIndices.push_back(upperLegNode);
    rig.lowerLegIndices.push_back(lowerLegNode);
    rig.footIndices.push_back(footNode);
  }

  return rig;
}

static float limbPresetRadiusScale(CharacterLimbPreset preset) {
  switch (preset) {
  case CharacterLimbPreset::Slim:
    return 0.82f;
  case CharacterLimbPreset::Standard:
    return 1.0f;
  case CharacterLimbPreset::Heavy:
    return 1.24f;
  }
  return 1.0f;
}

static glm::vec3 headPresetScale(CharacterHeadPreset preset) {
  switch (preset) {
  case CharacterHeadPreset::Round:
    return {0.84f, 1.22f, 0.92f};
  case CharacterHeadPreset::Block:
    return {0.88f, 1.02f, 0.90f};
  case CharacterHeadPreset::Wide:
    return {1.10f, 1.08f, 0.96f};
  }
  return {1.0f, 1.0f, 1.0f};
}

static void buildMaterials(const CharacterRecipe &recipe,
                           std::vector<ImportedMaterialData> &materials) {
  materials = {
      ImportedMaterialData{
          .name = "Torso",
          .baseColorFactor = recipe.torsoTint,
          .metallicFactor = 0.02f,
          .roughnessFactor = 0.92f,
      },
      ImportedMaterialData{
          .name = "Head",
          .baseColorFactor = recipe.headGroup.tint,
          .metallicFactor = 0.01f,
          .roughnessFactor = 0.96f,
      },
      ImportedMaterialData{
          .name = "Arms",
          .baseColorFactor = recipe.armGroup.tint,
          .metallicFactor = 0.02f,
          .roughnessFactor = 0.94f,
      },
      ImportedMaterialData{
          .name = "Legs",
          .baseColorFactor = recipe.legGroup.tint,
          .metallicFactor = 0.03f,
          .roughnessFactor = 0.88f,
      },
  };
}

static void buildTorsoGeometry(
    MeshBuilder &builder, const CharacterRecipe &recipe,
    const std::unordered_map<int, uint32_t> &jointSlots,
    const std::vector<glm::mat4> &worldTransforms, const CharacterRigState &rig,
    std::vector<ImportedModelSubmesh> &submeshes) {
  const glm::vec3 pelvisPosition =
      positionFromMatrix(worldTransforms[static_cast<size_t>(rig.pelvisIndex)]);
  const glm::vec3 spinePosition =
      positionFromMatrix(worldTransforms[static_cast<size_t>(rig.spineIndex)]);
  const glm::vec3 halfExtents(recipe.torsoDimensions.width * 0.5f,
                              recipe.torsoDimensions.height * 0.5f,
                              recipe.torsoDimensions.depth * 0.5f);
  const glm::vec3 torsoBlockCenter =
      spinePosition + glm::vec3(0.0f, halfExtents.y * 0.18f, 0.0f);
  const glm::vec3 pelvisCenter(
      pelvisPosition.x, pelvisPosition.y + recipe.pelvisShape.height * 0.55f,
      pelvisPosition.z + recipe.pelvisShape.forwardOffset);
  const uint32_t pelvisJoint = jointSlots.at(rig.pelvisIndex);
  const uint32_t spineJoint = jointSlots.at(rig.spineIndex);
  const uint32_t torsoTopJoint = jointSlots.at(rig.torsoTopIndex);

  auto torsoWeights = [&](const glm::vec3 &localPosition) {
    const float normalizedY =
        glm::clamp((localPosition.y + halfExtents.y) / (halfExtents.y * 2.0f),
                   0.0f, 1.0f);
    if (normalizedY < 0.35f) {
      return blendJoints(pelvisJoint, spineJoint, normalizedY / 0.35f);
    }
    return blendJoints(spineJoint, torsoTopJoint,
                       (normalizedY - 0.35f) / 0.65f);
  };

  builder.beginSubmesh("torso", CHARACTER_TORSO_MATERIAL_INDEX);
  if (recipe.torsoPreset == CharacterTorsoPreset::Barrel) {
    appendSphere(builder, torsoBlockCenter,
                 glm::vec3(halfExtents.x * 0.90f, halfExtents.y * 0.94f,
                           halfExtents.z * 0.82f),
                 6, 10, torsoWeights);
  } else if (recipe.torsoPreset == CharacterTorsoPreset::Tapered) {
    appendTaperedTorso(builder, torsoBlockCenter,
                       glm::vec3(halfExtents.x * 0.92f, halfExtents.y * 0.96f,
                                 halfExtents.z * 0.90f),
                       glm::vec3(halfExtents.x * 0.82f, halfExtents.y * 0.84f,
                                 halfExtents.z * 0.78f),
                       torsoWeights);
  } else {
    glm::mat4 torsoTransform(1.0f);
    torsoTransform[0] = glm::vec4(halfExtents.x, 0.0f, 0.0f, 0.0f);
    torsoTransform[1] = glm::vec4(0.0f, halfExtents.y, 0.0f, 0.0f);
    torsoTransform[2] = glm::vec4(0.0f, 0.0f, halfExtents.z, 0.0f);
    torsoTransform[3] = glm::vec4(torsoBlockCenter, 1.0f);
    appendBox(builder, torsoTransform, glm::vec3(1.0f), torsoWeights);
  }

  const glm::vec3 pelvisBottomHalfExtents(
      recipe.pelvisShape.width * 0.5f * recipe.pelvisShape.bottomScale,
      recipe.pelvisShape.height * 0.5f,
      recipe.pelvisShape.depth * 0.5f * recipe.pelvisShape.bottomScale);
  const glm::vec3 pelvisTopHalfExtents(
      recipe.pelvisShape.width * 0.5f * recipe.pelvisShape.topScale,
      recipe.pelvisShape.height * 0.5f,
      recipe.pelvisShape.depth * 0.5f * recipe.pelvisShape.topScale);
  appendTaperedPrism(
      builder,
      axialBasisTransform(
          pelvisCenter, glm::vec3(0.0f, 1.0f, 0.0f),
          glm::vec3(1.0f, recipe.pelvisShape.height * 0.5f, 1.0f)),
      glm::vec3(pelvisBottomHalfExtents.x, 1.0f, pelvisBottomHalfExtents.z),
      glm::vec3(pelvisTopHalfExtents.x, 1.0f, pelvisTopHalfExtents.z),
      [&](const glm::vec3 &local) {
        const float blend = glm::clamp((local.y + 1.0f) * 0.5f, 0.0f, 1.0f);
        return blendJoints(pelvisJoint, spineJoint, blend * 0.82f);
      });
  builder.finishSubmesh(submeshes);
}

static void buildHeadGeometry(
    MeshBuilder &builder, const CharacterRecipe &recipe,
    const std::unordered_map<int, uint32_t> &jointSlots,
    const std::vector<glm::mat4> &worldTransforms, const CharacterRigState &rig,
    std::vector<ImportedModelSubmesh> &submeshes) {
  if (rig.headIndices.empty()) {
    return;
  }

  const glm::vec3 presetScale = headPresetScale(recipe.headGroup.preset);
  const glm::vec3 headRadii = recipe.headGroup.size * 0.5f * presetScale;

  builder.beginSubmesh("heads", CHARACTER_HEAD_MATERIAL_INDEX);
  for (size_t headIndex = 0; headIndex < rig.headIndices.size(); ++headIndex) {
    const int neckNode = rig.neckIndices[headIndex];
    const int headNode = rig.headIndices[headIndex];
    const glm::vec3 neckPosition =
        positionFromMatrix(worldTransforms[static_cast<size_t>(neckNode)]);
    const glm::vec3 headPosition =
        positionFromMatrix(worldTransforms[static_cast<size_t>(headNode)]) +
        glm::vec3(0.0f, 0.0f, headRadii.z * 0.08f);
    const uint32_t neckJoint = jointSlots.at(neckNode);
    const uint32_t headJoint = jointSlots.at(headNode);

    appendLimbSegment(builder, neckPosition, headPosition,
                      std::min(headRadii.x, headRadii.z) * 0.22f,
                      [&](float station) {
                        return blendJoints(neckJoint, headJoint, station);
                      });

    if (recipe.headGroup.preset == CharacterHeadPreset::Block) {
      glm::mat4 transform(1.0f);
      transform[0] = glm::vec4(headRadii.x, 0.0f, 0.0f, 0.0f);
      transform[1] = glm::vec4(0.0f, headRadii.y, 0.0f, 0.0f);
      transform[2] = glm::vec4(0.0f, 0.0f, headRadii.z, 0.0f);
      transform[3] = glm::vec4(headPosition, 1.0f);
      appendBox(builder, transform, glm::vec3(1.0f),
                [&](const glm::vec3 &local) {
                  const float blend =
                      glm::clamp((local.y + headRadii.y) / (headRadii.y * 2.0f),
                                 0.0f, 1.0f);
                  return blendJoints(neckJoint, headJoint, blend);
                });
    } else {
      appendSphere(builder, headPosition, headRadii, 6, 10,
                   [&](const glm::vec3 &local) {
                     const float blend = glm::clamp((local.y + headRadii.y) /
                                                        (headRadii.y * 2.0f),
                                                    0.0f, 1.0f);
                     return blendJoints(neckJoint, headJoint, blend);
                   });
    }
  }
  builder.finishSubmesh(submeshes);
}

static void buildArmGeometry(
    MeshBuilder &builder, const CharacterRecipe &recipe,
    const std::unordered_map<int, uint32_t> &jointSlots,
    const std::vector<glm::mat4> &worldTransforms, const CharacterRigState &rig,
    std::vector<ImportedModelSubmesh> &submeshes) {
  if (rig.shoulderIndices.empty()) {
    return;
  }

  const float radius =
      recipe.armGroup.thickness * 0.5f * limbPresetRadiusScale(recipe.armGroup.preset);
  builder.beginSubmesh("arms", CHARACTER_ARM_MATERIAL_INDEX);

  for (size_t armIndex = 0; armIndex < rig.shoulderIndices.size(); ++armIndex) {
    const int shoulderNode = rig.shoulderIndices[armIndex];
    const int upperArmNode = rig.upperArmIndices[armIndex];
    const int lowerArmNode = rig.lowerArmIndices[armIndex];
    const int handNode = rig.handIndices[armIndex];

    const glm::vec3 shoulderPosition =
        positionFromMatrix(worldTransforms[static_cast<size_t>(shoulderNode)]);
    const glm::vec3 upperArmPosition =
        positionFromMatrix(worldTransforms[static_cast<size_t>(upperArmNode)]);
    const glm::vec3 lowerArmPosition =
        positionFromMatrix(worldTransforms[static_cast<size_t>(lowerArmNode)]);
    const glm::vec3 handPosition =
        positionFromMatrix(worldTransforms[static_cast<size_t>(handNode)]);
    const glm::vec3 shoulderDirection = upperArmPosition - shoulderPosition;
    const glm::vec3 elbowDirection = lowerArmPosition - upperArmPosition;
    const glm::vec3 wristDirection = handPosition - lowerArmPosition;

    appendJointConnector(builder, recipe.armConnectorShape, shoulderPosition,
                         shoulderDirection, [&](const glm::vec3 &local) {
                           const float blend =
                               glm::clamp((local.y + 1.0f) * 0.5f, 0.0f, 1.0f);
                           return blendJoints(jointSlots.at(shoulderNode),
                                              jointSlots.at(upperArmNode), blend);
                         });
    appendLimbSegment(builder, shoulderPosition, upperArmPosition, radius,
                      [&](float station) {
                        return blendJoints(jointSlots.at(shoulderNode),
                                           jointSlots.at(upperArmNode), station);
                      });
    appendJointConnector(builder, recipe.armConnectorShape, upperArmPosition,
                         elbowDirection, [&](const glm::vec3 &local) {
                           const float blend =
                               glm::clamp((local.y + 1.0f) * 0.5f, 0.0f, 1.0f);
                           return blendJoints(jointSlots.at(upperArmNode),
                                              jointSlots.at(lowerArmNode), blend);
                         });
    appendLimbSegment(builder, upperArmPosition, lowerArmPosition, radius * 0.92f,
                      [&](float station) {
                        return blendJoints(jointSlots.at(upperArmNode),
                                           jointSlots.at(lowerArmNode), station);
                      });
    appendJointConnector(builder, recipe.armConnectorShape, lowerArmPosition,
                         wristDirection, [&](const glm::vec3 &local) {
                           const float blend =
                               glm::clamp((local.y + 1.0f) * 0.5f, 0.0f, 1.0f);
                           return blendJoints(jointSlots.at(lowerArmNode),
                                              jointSlots.at(handNode), blend);
                         });

    const glm::vec3 handDirection =
        safeNormalize(wristDirection, glm::vec3(1.0f, 0.0f, 0.0f));
    const float handLength =
        std::max(recipe.handShape.length, recipe.armGroup.handLength * 0.85f);
    const glm::vec3 handCenter =
        handPosition + handDirection * (handLength * 0.34f);
    const glm::vec3 wristHalfExtents(
        recipe.handShape.width * 0.5f * recipe.handShape.wristScale,
        recipe.handShape.height * 0.5f * recipe.handShape.wristScale,
        handLength * 0.5f * (1.0f - recipe.handShape.taper * 0.18f));
    const glm::vec3 palmHalfExtents(
        recipe.handShape.width * 0.5f * recipe.handShape.palmScale,
        recipe.handShape.height * 0.5f,
        handLength * 0.5f * (1.0f + recipe.handShape.taper * 0.10f));
    appendTaperedPrism(builder,
                       basisTransform(handCenter, handDirection, glm::vec3(1.0f)),
                       wristHalfExtents, palmHalfExtents,
                       [&](const glm::vec3 &) {
                         return singleJoint(jointSlots.at(handNode));
                       });
  }

  builder.finishSubmesh(submeshes);
}

static void buildLegGeometry(
    MeshBuilder &builder, const CharacterRecipe &recipe,
    const std::unordered_map<int, uint32_t> &jointSlots,
    const std::vector<glm::mat4> &worldTransforms, const CharacterRigState &rig,
    std::vector<ImportedModelSubmesh> &submeshes) {
  if (rig.hipIndices.empty()) {
    return;
  }

  const float radius =
      recipe.legGroup.thickness * 0.5f * limbPresetRadiusScale(recipe.legGroup.preset);
  builder.beginSubmesh("legs", CHARACTER_LEG_MATERIAL_INDEX);

  for (size_t legIndex = 0; legIndex < rig.hipIndices.size(); ++legIndex) {
    const int hipNode = rig.hipIndices[legIndex];
    const int upperLegNode = rig.upperLegIndices[legIndex];
    const int lowerLegNode = rig.lowerLegIndices[legIndex];
    const int footNode = rig.footIndices[legIndex];

    const glm::vec3 hipPosition =
        positionFromMatrix(worldTransforms[static_cast<size_t>(hipNode)]);
    const glm::vec3 upperLegPosition =
        positionFromMatrix(worldTransforms[static_cast<size_t>(upperLegNode)]);
    const glm::vec3 lowerLegPosition =
        positionFromMatrix(worldTransforms[static_cast<size_t>(lowerLegNode)]);
    const glm::vec3 footPosition =
        positionFromMatrix(worldTransforms[static_cast<size_t>(footNode)]);
    const glm::vec3 hipDirection = upperLegPosition - hipPosition;
    const glm::vec3 kneeDirection = lowerLegPosition - upperLegPosition;
    const glm::vec3 ankleDirection = footPosition - lowerLegPosition;

    appendJointConnector(builder, recipe.legConnectorShape, hipPosition,
                         hipDirection, [&](const glm::vec3 &local) {
                           const float blend =
                               glm::clamp((local.y + 1.0f) * 0.5f, 0.0f, 1.0f);
                           return blendJoints(jointSlots.at(hipNode),
                                              jointSlots.at(upperLegNode), blend);
                         });
    appendLimbSegment(builder, hipPosition, upperLegPosition, radius,
                      [&](float station) {
                        return blendJoints(jointSlots.at(hipNode),
                                           jointSlots.at(upperLegNode), station);
                      });
    appendJointConnector(builder, recipe.legConnectorShape, upperLegPosition,
                         kneeDirection, [&](const glm::vec3 &local) {
                           const float blend =
                               glm::clamp((local.y + 1.0f) * 0.5f, 0.0f, 1.0f);
                           return blendJoints(jointSlots.at(upperLegNode),
                                              jointSlots.at(lowerLegNode), blend);
                         });
    appendLimbSegment(builder, upperLegPosition, lowerLegPosition, radius * 0.94f,
                      [&](float station) {
                        return blendJoints(jointSlots.at(upperLegNode),
                                           jointSlots.at(lowerLegNode), station);
                      });
    appendJointConnector(builder, recipe.legConnectorShape, lowerLegPosition,
                         ankleDirection, [&](const glm::vec3 &local) {
                           const float blend =
                               glm::clamp((local.y + 1.0f) * 0.5f, 0.0f, 1.0f);
                           return blendJoints(jointSlots.at(lowerLegNode),
                                              jointSlots.at(footNode), blend);
                         });

    const float footLength =
        std::max(recipe.footShape.length, recipe.legGroup.footLength);
    const glm::vec3 footForward = footPosition - lowerLegPosition;
    const glm::vec3 footDirection =
        safeNormalize(footForward + glm::vec3(0.0f, -recipe.footShape.pitchOffset,
                                              footLength * 0.24f),
                      glm::vec3(0.0f, -0.1f, 1.0f));
    const glm::vec3 footCenter = footPosition + footDirection * (footLength * 0.26f) +
                                 glm::vec3(0.0f, -recipe.footShape.height * 0.28f,
                                           0.0f);
    const glm::vec3 ankleHalfExtents(
        recipe.footShape.width * 0.5f * recipe.footShape.ankleScale,
        recipe.footShape.height * 0.5f * recipe.footShape.ankleScale,
        footLength * 0.5f * 0.68f);
    const glm::vec3 toeHalfExtents(
        recipe.footShape.width * 0.5f * recipe.footShape.toeScale,
        recipe.footShape.height * 0.5f,
        footLength * 0.5f);
    appendTaperedPrism(builder, basisTransform(footCenter, footDirection,
                                               glm::vec3(1.0f)),
                       ankleHalfExtents, toeHalfExtents,
                       [&](const glm::vec3 &) {
                         return singleJoint(jointSlots.at(footNode));
                       });
  }

  builder.finishSubmesh(submeshes);
}

static std::unordered_map<int, uint32_t>
buildJointSlots(const ImportedSkinData &skin) {
  std::unordered_map<int, uint32_t> jointSlots;
  for (size_t jointIndex = 0; jointIndex < skin.jointNodeIndices.size();
       ++jointIndex) {
    jointSlots.emplace(skin.jointNodeIndices[jointIndex],
                       static_cast<uint32_t>(jointIndex));
  }
  return jointSlots;
}

GeneratedCharacterAssetData
CharacterRecipeGenerator::generate(const CharacterRecipe &sourceRecipe) {
  const CharacterRecipe recipe = clampCharacterRecipe(sourceRecipe);
  CharacterRigState rig = buildRig(recipe);
  std::vector<glm::mat4> worldTransforms = computeWorldTransforms(rig.skeleton);

  ImportedSkinData skin{
      .name = "Character Skin",
      .skeletonRootNodeIndex = rig.rootIndex,
  };
  skin.jointNodeIndices.reserve(rig.skeleton.nodes.size());
  skin.inverseBindMatrices.reserve(rig.skeleton.nodes.size());
  for (size_t nodeIndex = 0; nodeIndex < rig.skeleton.nodes.size(); ++nodeIndex) {
    if (static_cast<int>(nodeIndex) == rig.rootIndex) {
      continue;
    }
    skin.jointNodeIndices.push_back(static_cast<int>(nodeIndex));
    skin.inverseBindMatrices.push_back(glm::inverse(worldTransforms[nodeIndex]));
  }

  rig.skeleton.skins.push_back(skin);
  const std::unordered_map<int, uint32_t> jointSlots =
      buildJointSlots(rig.skeleton.skins.front());

  MeshBuilder builder;
  std::vector<ImportedModelSubmesh> submeshes;
  buildTorsoGeometry(builder, recipe, jointSlots, worldTransforms, rig, submeshes);
  buildHeadGeometry(builder, recipe, jointSlots, worldTransforms, rig, submeshes);
  buildArmGeometry(builder, recipe, jointSlots, worldTransforms, rig, submeshes);
  buildLegGeometry(builder, recipe, jointSlots, worldTransforms, rig, submeshes);

  std::vector<ImportedMaterialData> materials;
  buildMaterials(recipe, materials);

  GeneratedCharacterAssetData generated;
  generated.geometry.setImportedGeometry(std::move(builder.vertices),
                                         std::move(builder.indices),
                                         std::move(submeshes),
                                         std::move(materials));
  generated.skeleton = std::move(rig.skeleton);
  return generated;
}
