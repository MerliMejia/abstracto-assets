#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

struct ImportedNodeTransform {
  glm::vec3 translation{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};
};

struct ImportedSkeletonNode {
  std::string name;
  int parentIndex = -1;
  std::vector<int> childIndices;
  ImportedNodeTransform localBindTransform{};
};

struct ImportedSkinData {
  std::string name;
  int skeletonRootNodeIndex = -1;
  std::vector<int> jointNodeIndices;
  std::vector<glm::mat4> inverseBindMatrices;
};

enum class ImportedAnimationTargetPath {
  Translation,
  Rotation,
  Scale,
};

enum class ImportedAnimationInterpolation {
  Linear,
  Step,
  CubicSpline,
};

struct ImportedNodeAnimationTrack {
  int targetNodeIndex = -1;
  ImportedAnimationTargetPath targetPath =
      ImportedAnimationTargetPath::Translation;
  ImportedAnimationInterpolation interpolation =
      ImportedAnimationInterpolation::Linear;
  std::vector<float> timesSeconds;
  std::vector<glm::vec3> vec3InTangents;
  std::vector<glm::vec3> vec3Values;
  std::vector<glm::vec3> vec3OutTangents;
  std::vector<glm::vec4> quatInTangents;
  std::vector<glm::quat> quatValues;
  std::vector<glm::vec4> quatOutTangents;
};

struct ImportedAnimationClipData {
  std::string name;
  std::vector<ImportedNodeAnimationTrack> tracks;
  float durationSeconds = 0.0f;
};

struct ImportedSkeletonData {
  std::vector<ImportedSkeletonNode> nodes;
  std::vector<int> sceneRootNodeIndices;
  std::vector<ImportedSkinData> skins;
  std::vector<ImportedAnimationClipData> animations;
};
