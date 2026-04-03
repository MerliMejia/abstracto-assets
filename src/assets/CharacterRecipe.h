#pragma once

#include <algorithm>
#include <glm/glm.hpp>
#include <string>

enum class CharacterTorsoPreset {
  Box = 0,
  Barrel = 1,
  Tapered = 2,
};

enum class CharacterHeadPreset {
  Round = 0,
  Block = 1,
  Wide = 2,
};

enum class CharacterLimbPreset {
  Slim = 0,
  Standard = 1,
  Heavy = 2,
};

struct CharacterTorsoDimensions {
  float width = 0.74f;
  float height = 1.06f;
  float depth = 0.34f;
};

struct CharacterHeadGroup {
  int count = 1;
  CharacterHeadPreset preset = CharacterHeadPreset::Round;
  glm::vec3 size{0.52f, 0.68f, 0.48f};
  float spacing = 0.7f;
  glm::vec3 offset{0.0f, 0.12f, 0.0f};
  glm::vec4 tint{0.86f, 0.86f, 0.86f, 1.0f};
};

struct CharacterArmGroup {
  int count = 2;
  CharacterLimbPreset preset = CharacterLimbPreset::Slim;
  float upperLength = 0.74f;
  float lowerLength = 0.76f;
  float handLength = 0.18f;
  float thickness = 0.18f;
  float spacing = 0.98f;
  glm::vec3 offset{0.0f, 0.0f, 0.0f};
  glm::vec4 tint{0.82f, 0.82f, 0.82f, 1.0f};
};

struct CharacterLegGroup {
  int count = 2;
  CharacterLimbPreset preset = CharacterLimbPreset::Standard;
  float upperLength = 1.02f;
  float lowerLength = 0.96f;
  float footLength = 0.42f;
  float thickness = 0.20f;
  float spacing = 0.64f;
  glm::vec3 offset{0.0f, 0.0f, 0.0f};
  glm::vec4 tint{0.82f, 0.82f, 0.82f, 1.0f};
};

struct CharacterPelvisShape {
  float width = 0.60f;
  float height = 0.34f;
  float depth = 0.42f;
  float topScale = 0.76f;
  float bottomScale = 1.0f;
  float forwardOffset = 0.02f;
};

struct CharacterConnectorShape {
  float length = 0.16f;
  float baseRadius = 0.12f;
  float tipRadius = 0.08f;
  float lateralFlare = 1.18f;
};

struct CharacterHandShape {
  float length = 0.26f;
  float width = 0.18f;
  float height = 0.12f;
  float wristScale = 0.56f;
  float palmScale = 1.0f;
  float taper = 0.34f;
};

struct CharacterFootShape {
  float length = 0.42f;
  float width = 0.20f;
  float height = 0.14f;
  float ankleScale = 0.54f;
  float toeScale = 1.0f;
  float pitchOffset = 0.16f;
};

struct CharacterRecipe {
  std::string name = "Character";
  CharacterTorsoPreset torsoPreset = CharacterTorsoPreset::Box;
  CharacterTorsoDimensions torsoDimensions{};
  glm::vec4 torsoTint{0.78f, 0.78f, 0.78f, 1.0f};
  CharacterHeadGroup headGroup{};
  CharacterArmGroup armGroup{};
  CharacterLegGroup legGroup{};
  CharacterPelvisShape pelvisShape{};
  CharacterConnectorShape armConnectorShape{};
  CharacterConnectorShape legConnectorShape{
      .length = 0.19f,
      .baseRadius = 0.14f,
      .tipRadius = 0.09f,
      .lateralFlare = 1.22f,
  };
  CharacterHandShape handShape{};
  CharacterFootShape footShape{};

  static CharacterRecipe mannequin() { return CharacterRecipe{}; }
};

inline glm::vec4 clampCharacterTint(glm::vec4 tint) {
  tint.r = glm::clamp(tint.r, 0.0f, 1.0f);
  tint.g = glm::clamp(tint.g, 0.0f, 1.0f);
  tint.b = glm::clamp(tint.b, 0.0f, 1.0f);
  tint.a = glm::clamp(tint.a, 0.0f, 1.0f);
  return tint;
}

inline float clampPositive(float value, float minimum, float maximum) {
  return std::clamp(value, minimum, maximum);
}

inline CharacterRecipe clampCharacterRecipe(CharacterRecipe recipe) {
  recipe.torsoDimensions.width =
      clampPositive(recipe.torsoDimensions.width, 0.2f, 4.0f);
  recipe.torsoDimensions.height =
      clampPositive(recipe.torsoDimensions.height, 0.3f, 4.5f);
  recipe.torsoDimensions.depth =
      clampPositive(recipe.torsoDimensions.depth, 0.2f, 4.0f);
  recipe.torsoTint = clampCharacterTint(recipe.torsoTint);

  recipe.headGroup.count = std::clamp(recipe.headGroup.count, 0, 4);
  recipe.headGroup.size.x = clampPositive(recipe.headGroup.size.x, 0.1f, 2.5f);
  recipe.headGroup.size.y = clampPositive(recipe.headGroup.size.y, 0.1f, 2.5f);
  recipe.headGroup.size.z = clampPositive(recipe.headGroup.size.z, 0.1f, 2.5f);
  recipe.headGroup.spacing = clampPositive(recipe.headGroup.spacing, 0.0f, 2.5f);
  recipe.headGroup.offset.x =
      std::clamp(recipe.headGroup.offset.x, -2.5f, 2.5f);
  recipe.headGroup.offset.y =
      std::clamp(recipe.headGroup.offset.y, -2.5f, 2.5f);
  recipe.headGroup.offset.z =
      std::clamp(recipe.headGroup.offset.z, -2.5f, 2.5f);
  recipe.headGroup.tint = clampCharacterTint(recipe.headGroup.tint);

  recipe.armGroup.count = std::clamp(recipe.armGroup.count, 0, 6);
  recipe.armGroup.upperLength =
      clampPositive(recipe.armGroup.upperLength, 0.1f, 3.0f);
  recipe.armGroup.lowerLength =
      clampPositive(recipe.armGroup.lowerLength, 0.1f, 3.0f);
  recipe.armGroup.handLength =
      clampPositive(recipe.armGroup.handLength, 0.05f, 1.5f);
  recipe.armGroup.thickness =
      clampPositive(recipe.armGroup.thickness, 0.04f, 1.5f);
  recipe.armGroup.spacing = clampPositive(recipe.armGroup.spacing, 0.0f, 2.5f);
  recipe.armGroup.offset.x =
      std::clamp(recipe.armGroup.offset.x, -2.5f, 2.5f);
  recipe.armGroup.offset.y =
      std::clamp(recipe.armGroup.offset.y, -2.5f, 2.5f);
  recipe.armGroup.offset.z =
      std::clamp(recipe.armGroup.offset.z, -2.5f, 2.5f);
  recipe.armGroup.tint = clampCharacterTint(recipe.armGroup.tint);

  recipe.legGroup.count = std::clamp(recipe.legGroup.count, 0, 6);
  recipe.legGroup.upperLength =
      clampPositive(recipe.legGroup.upperLength, 0.1f, 3.5f);
  recipe.legGroup.lowerLength =
      clampPositive(recipe.legGroup.lowerLength, 0.1f, 3.5f);
  recipe.legGroup.footLength =
      clampPositive(recipe.legGroup.footLength, 0.05f, 2.0f);
  recipe.legGroup.thickness =
      clampPositive(recipe.legGroup.thickness, 0.04f, 1.6f);
  recipe.legGroup.spacing = clampPositive(recipe.legGroup.spacing, 0.0f, 2.5f);
  recipe.legGroup.offset.x =
      std::clamp(recipe.legGroup.offset.x, -2.5f, 2.5f);
  recipe.legGroup.offset.y =
      std::clamp(recipe.legGroup.offset.y, -2.5f, 2.5f);
  recipe.legGroup.offset.z =
      std::clamp(recipe.legGroup.offset.z, -2.5f, 2.5f);
  recipe.legGroup.tint = clampCharacterTint(recipe.legGroup.tint);

  recipe.pelvisShape.width = clampPositive(recipe.pelvisShape.width, 0.1f, 3.0f);
  recipe.pelvisShape.height =
      clampPositive(recipe.pelvisShape.height, 0.08f, 2.0f);
  recipe.pelvisShape.depth = clampPositive(recipe.pelvisShape.depth, 0.08f, 3.0f);
  recipe.pelvisShape.topScale =
      clampPositive(recipe.pelvisShape.topScale, 0.2f, 2.0f);
  recipe.pelvisShape.bottomScale =
      clampPositive(recipe.pelvisShape.bottomScale, 0.2f, 2.0f);
  recipe.pelvisShape.forwardOffset =
      std::clamp(recipe.pelvisShape.forwardOffset, -1.5f, 1.5f);

  recipe.armConnectorShape.length =
      clampPositive(recipe.armConnectorShape.length, 0.02f, 1.5f);
  recipe.armConnectorShape.baseRadius =
      clampPositive(recipe.armConnectorShape.baseRadius, 0.01f, 1.2f);
  recipe.armConnectorShape.tipRadius =
      clampPositive(recipe.armConnectorShape.tipRadius, 0.01f, 1.2f);
  recipe.armConnectorShape.lateralFlare =
      clampPositive(recipe.armConnectorShape.lateralFlare, 0.5f, 2.5f);

  recipe.legConnectorShape.length =
      clampPositive(recipe.legConnectorShape.length, 0.02f, 1.5f);
  recipe.legConnectorShape.baseRadius =
      clampPositive(recipe.legConnectorShape.baseRadius, 0.01f, 1.2f);
  recipe.legConnectorShape.tipRadius =
      clampPositive(recipe.legConnectorShape.tipRadius, 0.01f, 1.2f);
  recipe.legConnectorShape.lateralFlare =
      clampPositive(recipe.legConnectorShape.lateralFlare, 0.5f, 2.5f);

  recipe.handShape.length = clampPositive(recipe.handShape.length, 0.04f, 2.0f);
  recipe.handShape.width = clampPositive(recipe.handShape.width, 0.02f, 1.5f);
  recipe.handShape.height = clampPositive(recipe.handShape.height, 0.02f, 1.5f);
  recipe.handShape.wristScale =
      clampPositive(recipe.handShape.wristScale, 0.2f, 2.0f);
  recipe.handShape.palmScale =
      clampPositive(recipe.handShape.palmScale, 0.2f, 2.0f);
  recipe.handShape.taper = clampPositive(recipe.handShape.taper, 0.0f, 1.0f);

  recipe.footShape.length = clampPositive(recipe.footShape.length, 0.05f, 3.0f);
  recipe.footShape.width = clampPositive(recipe.footShape.width, 0.02f, 1.8f);
  recipe.footShape.height = clampPositive(recipe.footShape.height, 0.02f, 1.8f);
  recipe.footShape.ankleScale =
      clampPositive(recipe.footShape.ankleScale, 0.2f, 2.0f);
  recipe.footShape.toeScale =
      clampPositive(recipe.footShape.toeScale, 0.2f, 2.0f);
  recipe.footShape.pitchOffset =
      std::clamp(recipe.footShape.pitchOffset, -1.0f, 1.0f);
  return recipe;
}
