#include "assets/CharacterModelAsset.h"
#include "assets/GltfModelAsset.h"
#include "assets/ObjModelAsset.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

struct SceneAssetDefinition {
  std::string name;
  std::string assetPath;
  glm::vec4 baseColorTint{1.0f, 1.0f, 1.0f, 1.0f};
  int preferredAnimationIndex = 0;
};

static std::unique_ptr<ModelAsset>
importSceneAsset(const SceneAssetDefinition &definition) {
  const std::string extension =
      std::filesystem::path(definition.assetPath).extension().string();
  if (extension == ".json" &&
      std::filesystem::path(definition.assetPath).stem().extension() ==
          ".character") {
    auto asset = std::make_unique<CharacterModelAsset>();
    asset->load(definition.assetPath);
    return asset;
  }

  if (extension == ".gltf" || extension == ".glb") {
    auto asset = std::make_unique<GltfModelAsset>();
    asset->load(definition.assetPath);
    return asset;
  }

  if (extension == ".obj") {
    auto asset = std::make_unique<ObjModelAsset>();
    asset->load(definition.assetPath);
    return asset;
  }

  throw std::runtime_error("unsupported scene asset format: " + extension);
}

static void applyAuthoringOverrides(const SceneAssetDefinition &definition,
                                    ModelAsset &asset) {
  for (auto &material : asset.mutableMaterials()) {
    material.baseColorFactor *= definition.baseColorTint;
  }
}

static void
printImportedAssetSummary(const SceneAssetDefinition &definition,
                          const ModelAsset &asset) {
  std::cout << "Scene asset: " << definition.name << "\n";
  std::cout << "Source path: " << asset.path() << "\n";
  std::cout << "Mesh vertices: " << asset.mesh().vertexCount() << "\n";
  std::cout << "Mesh indices: " << asset.mesh().indexData().size() << "\n";
  std::cout << "Submeshes: " << asset.submeshes().size() << "\n";
  std::cout << "Materials: " << asset.materials().size() << "\n";

  for (size_t materialIndex = 0; materialIndex < asset.materials().size();
       ++materialIndex) {
    const auto &material = asset.materials()[materialIndex];
    std::cout << "  Material[" << materialIndex << "]: "
              << (material.name.empty() ? "<unnamed>" : material.name)
              << ", baseColor=(" << material.baseColorFactor.r << ", "
              << material.baseColorFactor.g << ", "
              << material.baseColorFactor.b << ", "
              << material.baseColorFactor.a << ")";
    if (material.hasBaseColorTexture()) {
      std::cout << ", baseColorTexture="
                << (material.hasBaseColorTexturePath()
                        ? material.baseColorTexture.resolvedPath
                        : "<embedded>");
    }
    std::cout << "\n";
  }

  const ImportedSkeletonData *skeleton = asset.skeletonAsset();
  if (skeleton == nullptr) {
    std::cout << "Skeleton: none\n";
    return;
  }

  std::cout << "Skeleton nodes: " << skeleton->nodes.size() << "\n";
  std::cout << "Skins: " << skeleton->skins.size() << "\n";
  std::cout << "Animations: " << skeleton->animations.size() << "\n";
  if (!skeleton->animations.empty()) {
    const size_t animationIndex = static_cast<size_t>(std::clamp(
        definition.preferredAnimationIndex, 0,
        static_cast<int>(skeleton->animations.size()) - 1));
    const auto &animation = skeleton->animations[animationIndex];
    std::cout << "Preferred animation: "
              << (animation.name.empty() ? "<unnamed>" : animation.name)
              << " (" << animation.durationSeconds << " s)\n";
  }
}

int main() {
  const std::filesystem::path assetPath = "assets/character_mannequin.character.json";
  SceneAssetDefinition sceneAsset{
      .name = assetPath.stem().string(),
      .assetPath = assetPath.string(),
      .baseColorTint = {1.0f, 1.0f, 1.0f, 1.0f},
      .preferredAnimationIndex = 0,
  };

  try {
    if (!std::filesystem::exists(assetPath)) {
      throw std::runtime_error("expected demo asset at " + assetPath.string());
    }

    std::unique_ptr<ModelAsset> importedAsset = importSceneAsset(sceneAsset);
    applyAuthoringOverrides(sceneAsset, *importedAsset);
    printImportedAssetSummary(sceneAsset, *importedAsset);

    std::cout << "Runtime handoff now belongs in abstracto-engine."
              << std::endl;
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "Failed to import scene asset: " << exception.what()
              << std::endl;
    return 1;
  }
}
