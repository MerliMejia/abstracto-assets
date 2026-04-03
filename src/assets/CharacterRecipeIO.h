#pragma once

#include "assets/CharacterRecipe.h"
#include <filesystem>
#include <fstream>
#include <json.hpp>
#include <stdexcept>

class CharacterRecipeIO {
public:
  using json = nlohmann::json;

  static CharacterRecipe load(const std::filesystem::path &path) {
    std::ifstream input(path);
    if (!input.is_open()) {
      throw std::runtime_error("failed to open character recipe: " +
                               path.string());
    }

    json parsed;
    input >> parsed;
    return fromJson(parsed);
  }

  static void save(const std::filesystem::path &path,
                   const CharacterRecipe &recipe) {
    std::error_code error;
    if (path.has_parent_path()) {
      std::filesystem::create_directories(path.parent_path(), error);
    }

    std::ofstream output(path);
    if (!output.is_open()) {
      throw std::runtime_error("failed to write character recipe: " +
                               path.string());
    }

    output << toJson(recipe).dump(2);
    if (!output.good()) {
      throw std::runtime_error("failed to flush character recipe: " +
                               path.string());
    }
  }

  static CharacterRecipe fromJson(const json &value) {
    CharacterRecipe recipe = CharacterRecipe::mannequin();
    recipe.name = value.value("name", recipe.name);
    recipe.torsoPreset =
        torsoPresetFromString(value.value("torsoPreset", "Box"));

    if (value.contains("torsoDimensions") && value["torsoDimensions"].is_object()) {
      const json &dims = value["torsoDimensions"];
      recipe.torsoDimensions.width =
          dims.value("width", recipe.torsoDimensions.width);
      recipe.torsoDimensions.height =
          dims.value("height", recipe.torsoDimensions.height);
      recipe.torsoDimensions.depth =
          dims.value("depth", recipe.torsoDimensions.depth);
    }
    recipe.torsoTint =
        vec4FromJson(value.value("torsoTint", json::array()), recipe.torsoTint);

    if (value.contains("headGroup") && value["headGroup"].is_object()) {
      const json &group = value["headGroup"];
      recipe.headGroup.count = group.value("count", recipe.headGroup.count);
      recipe.headGroup.preset =
          headPresetFromString(group.value("preset", "Round"));
      recipe.headGroup.size =
          vec3FromJson(group.value("size", json::array()), recipe.headGroup.size);
      recipe.headGroup.spacing =
          group.value("spacing", recipe.headGroup.spacing);
      recipe.headGroup.offset = vec3FromJson(group.value("offset", json::array()),
                                             recipe.headGroup.offset);
      recipe.headGroup.tint =
          vec4FromJson(group.value("tint", json::array()), recipe.headGroup.tint);
    }

    if (value.contains("armGroup") && value["armGroup"].is_object()) {
      const json &group = value["armGroup"];
      recipe.armGroup.count = group.value("count", recipe.armGroup.count);
      recipe.armGroup.preset =
          limbPresetFromString(group.value("preset", "Standard"));
      recipe.armGroup.upperLength =
          group.value("upperLength", recipe.armGroup.upperLength);
      recipe.armGroup.lowerLength =
          group.value("lowerLength", recipe.armGroup.lowerLength);
      recipe.armGroup.handLength =
          group.value("handLength", recipe.armGroup.handLength);
      recipe.armGroup.thickness =
          group.value("thickness", recipe.armGroup.thickness);
      recipe.armGroup.spacing =
          group.value("spacing", recipe.armGroup.spacing);
      recipe.armGroup.offset =
          vec3FromJson(group.value("offset", json::array()), recipe.armGroup.offset);
      recipe.armGroup.tint =
          vec4FromJson(group.value("tint", json::array()), recipe.armGroup.tint);
    }

    if (value.contains("legGroup") && value["legGroup"].is_object()) {
      const json &group = value["legGroup"];
      recipe.legGroup.count = group.value("count", recipe.legGroup.count);
      recipe.legGroup.preset =
          limbPresetFromString(group.value("preset", "Standard"));
      recipe.legGroup.upperLength =
          group.value("upperLength", recipe.legGroup.upperLength);
      recipe.legGroup.lowerLength =
          group.value("lowerLength", recipe.legGroup.lowerLength);
      recipe.legGroup.footLength =
          group.value("footLength", recipe.legGroup.footLength);
      recipe.legGroup.thickness =
          group.value("thickness", recipe.legGroup.thickness);
      recipe.legGroup.spacing =
          group.value("spacing", recipe.legGroup.spacing);
      recipe.legGroup.offset =
          vec3FromJson(group.value("offset", json::array()), recipe.legGroup.offset);
      recipe.legGroup.tint =
          vec4FromJson(group.value("tint", json::array()), recipe.legGroup.tint);
    }

    if (value.contains("pelvisShape") && value["pelvisShape"].is_object()) {
      const json &shape = value["pelvisShape"];
      recipe.pelvisShape.width =
          shape.value("width", recipe.pelvisShape.width);
      recipe.pelvisShape.height =
          shape.value("height", recipe.pelvisShape.height);
      recipe.pelvisShape.depth =
          shape.value("depth", recipe.pelvisShape.depth);
      recipe.pelvisShape.topScale =
          shape.value("topScale", recipe.pelvisShape.topScale);
      recipe.pelvisShape.bottomScale =
          shape.value("bottomScale", recipe.pelvisShape.bottomScale);
      recipe.pelvisShape.forwardOffset =
          shape.value("forwardOffset", recipe.pelvisShape.forwardOffset);
    }

    if (value.contains("armConnectorShape") &&
        value["armConnectorShape"].is_object()) {
      const json &shape = value["armConnectorShape"];
      recipe.armConnectorShape.length =
          shape.value("length", recipe.armConnectorShape.length);
      recipe.armConnectorShape.baseRadius =
          shape.value("baseRadius", recipe.armConnectorShape.baseRadius);
      recipe.armConnectorShape.tipRadius =
          shape.value("tipRadius", recipe.armConnectorShape.tipRadius);
      recipe.armConnectorShape.lateralFlare =
          shape.value("lateralFlare", recipe.armConnectorShape.lateralFlare);
    }

    if (value.contains("legConnectorShape") &&
        value["legConnectorShape"].is_object()) {
      const json &shape = value["legConnectorShape"];
      recipe.legConnectorShape.length =
          shape.value("length", recipe.legConnectorShape.length);
      recipe.legConnectorShape.baseRadius =
          shape.value("baseRadius", recipe.legConnectorShape.baseRadius);
      recipe.legConnectorShape.tipRadius =
          shape.value("tipRadius", recipe.legConnectorShape.tipRadius);
      recipe.legConnectorShape.lateralFlare =
          shape.value("lateralFlare", recipe.legConnectorShape.lateralFlare);
    }

    if (value.contains("handShape") && value["handShape"].is_object()) {
      const json &shape = value["handShape"];
      recipe.handShape.length = shape.value("length", recipe.handShape.length);
      recipe.handShape.width = shape.value("width", recipe.handShape.width);
      recipe.handShape.height = shape.value("height", recipe.handShape.height);
      recipe.handShape.wristScale =
          shape.value("wristScale", recipe.handShape.wristScale);
      recipe.handShape.palmScale =
          shape.value("palmScale", recipe.handShape.palmScale);
      recipe.handShape.taper = shape.value("taper", recipe.handShape.taper);
    }

    if (value.contains("footShape") && value["footShape"].is_object()) {
      const json &shape = value["footShape"];
      recipe.footShape.length = shape.value("length", recipe.footShape.length);
      recipe.footShape.width = shape.value("width", recipe.footShape.width);
      recipe.footShape.height = shape.value("height", recipe.footShape.height);
      recipe.footShape.ankleScale =
          shape.value("ankleScale", recipe.footShape.ankleScale);
      recipe.footShape.toeScale =
          shape.value("toeScale", recipe.footShape.toeScale);
      recipe.footShape.pitchOffset =
          shape.value("pitchOffset", recipe.footShape.pitchOffset);
    }

    return clampCharacterRecipe(std::move(recipe));
  }

  static json toJson(const CharacterRecipe &sourceRecipe) {
    const CharacterRecipe recipe = clampCharacterRecipe(sourceRecipe);
    return {
        {"name", recipe.name},
        {"torsoPreset", torsoPresetToString(recipe.torsoPreset)},
        {"torsoDimensions",
         {
             {"width", recipe.torsoDimensions.width},
             {"height", recipe.torsoDimensions.height},
             {"depth", recipe.torsoDimensions.depth},
         }},
        {"torsoTint", vec4ToJson(recipe.torsoTint)},
        {"headGroup",
         {
             {"count", recipe.headGroup.count},
             {"preset", headPresetToString(recipe.headGroup.preset)},
             {"size", vec3ToJson(recipe.headGroup.size)},
             {"spacing", recipe.headGroup.spacing},
             {"offset", vec3ToJson(recipe.headGroup.offset)},
             {"tint", vec4ToJson(recipe.headGroup.tint)},
         }},
        {"armGroup",
         {
             {"count", recipe.armGroup.count},
             {"preset", limbPresetToString(recipe.armGroup.preset)},
             {"upperLength", recipe.armGroup.upperLength},
             {"lowerLength", recipe.armGroup.lowerLength},
             {"handLength", recipe.armGroup.handLength},
             {"thickness", recipe.armGroup.thickness},
             {"spacing", recipe.armGroup.spacing},
             {"offset", vec3ToJson(recipe.armGroup.offset)},
             {"tint", vec4ToJson(recipe.armGroup.tint)},
         }},
        {"legGroup",
         {
             {"count", recipe.legGroup.count},
             {"preset", limbPresetToString(recipe.legGroup.preset)},
             {"upperLength", recipe.legGroup.upperLength},
             {"lowerLength", recipe.legGroup.lowerLength},
             {"footLength", recipe.legGroup.footLength},
             {"thickness", recipe.legGroup.thickness},
             {"spacing", recipe.legGroup.spacing},
             {"offset", vec3ToJson(recipe.legGroup.offset)},
             {"tint", vec4ToJson(recipe.legGroup.tint)},
         }},
        {"pelvisShape",
         {
             {"width", recipe.pelvisShape.width},
             {"height", recipe.pelvisShape.height},
             {"depth", recipe.pelvisShape.depth},
             {"topScale", recipe.pelvisShape.topScale},
             {"bottomScale", recipe.pelvisShape.bottomScale},
             {"forwardOffset", recipe.pelvisShape.forwardOffset},
         }},
        {"armConnectorShape",
         {
             {"length", recipe.armConnectorShape.length},
             {"baseRadius", recipe.armConnectorShape.baseRadius},
             {"tipRadius", recipe.armConnectorShape.tipRadius},
             {"lateralFlare", recipe.armConnectorShape.lateralFlare},
         }},
        {"legConnectorShape",
         {
             {"length", recipe.legConnectorShape.length},
             {"baseRadius", recipe.legConnectorShape.baseRadius},
             {"tipRadius", recipe.legConnectorShape.tipRadius},
             {"lateralFlare", recipe.legConnectorShape.lateralFlare},
         }},
        {"handShape",
         {
             {"length", recipe.handShape.length},
             {"width", recipe.handShape.width},
             {"height", recipe.handShape.height},
             {"wristScale", recipe.handShape.wristScale},
             {"palmScale", recipe.handShape.palmScale},
             {"taper", recipe.handShape.taper},
         }},
        {"footShape",
         {
             {"length", recipe.footShape.length},
             {"width", recipe.footShape.width},
             {"height", recipe.footShape.height},
             {"ankleScale", recipe.footShape.ankleScale},
             {"toeScale", recipe.footShape.toeScale},
             {"pitchOffset", recipe.footShape.pitchOffset},
         }},
    };
  }

private:
  static json vec3ToJson(const glm::vec3 &value) {
    return json::array({value.x, value.y, value.z});
  }

  static json vec4ToJson(const glm::vec4 &value) {
    return json::array({value.x, value.y, value.z, value.w});
  }

  static glm::vec3 vec3FromJson(const json &value, const glm::vec3 &fallback) {
    if (!value.is_array() || value.size() != 3) {
      return fallback;
    }
    return {value[0].get<float>(), value[1].get<float>(),
            value[2].get<float>()};
  }

  static glm::vec4 vec4FromJson(const json &value, const glm::vec4 &fallback) {
    if (!value.is_array() || value.size() != 4) {
      return fallback;
    }
    return {value[0].get<float>(), value[1].get<float>(),
            value[2].get<float>(), value[3].get<float>()};
  }

  static const char *torsoPresetToString(CharacterTorsoPreset preset) {
    switch (preset) {
    case CharacterTorsoPreset::Box:
      return "Box";
    case CharacterTorsoPreset::Barrel:
      return "Barrel";
    case CharacterTorsoPreset::Tapered:
      return "Tapered";
    }
    return "Box";
  }

  static CharacterTorsoPreset torsoPresetFromString(const std::string &value) {
    if (value == "Barrel") {
      return CharacterTorsoPreset::Barrel;
    }
    if (value == "Tapered") {
      return CharacterTorsoPreset::Tapered;
    }
    return CharacterTorsoPreset::Box;
  }

  static const char *headPresetToString(CharacterHeadPreset preset) {
    switch (preset) {
    case CharacterHeadPreset::Round:
      return "Round";
    case CharacterHeadPreset::Block:
      return "Block";
    case CharacterHeadPreset::Wide:
      return "Wide";
    }
    return "Round";
  }

  static CharacterHeadPreset headPresetFromString(const std::string &value) {
    if (value == "Block") {
      return CharacterHeadPreset::Block;
    }
    if (value == "Wide") {
      return CharacterHeadPreset::Wide;
    }
    return CharacterHeadPreset::Round;
  }

  static const char *limbPresetToString(CharacterLimbPreset preset) {
    switch (preset) {
    case CharacterLimbPreset::Slim:
      return "Slim";
    case CharacterLimbPreset::Standard:
      return "Standard";
    case CharacterLimbPreset::Heavy:
      return "Heavy";
    }
    return "Standard";
  }

  static CharacterLimbPreset limbPresetFromString(const std::string &value) {
    if (value == "Slim") {
      return CharacterLimbPreset::Slim;
    }
    if (value == "Heavy") {
      return CharacterLimbPreset::Heavy;
    }
    return CharacterLimbPreset::Standard;
  }
};
