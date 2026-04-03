#include "assets/CharacterModelAsset.h"
#include "assets/CharacterRecipeGenerator.h"
#include "assets/CharacterRecipeIO.h"

void CharacterModelAsset::load(const std::string &path) {
  sourcePath = path;
  characterRecipe = CharacterRecipeIO::load(path);
  rebuild();
}

void CharacterModelAsset::setRecipe(CharacterRecipe recipe,
                                    std::string sourcePathValue) {
  sourcePath = std::move(sourcePathValue);
  characterRecipe = clampCharacterRecipe(std::move(recipe));
  rebuild();
}

void CharacterModelAsset::rebuild() {
  GeneratedCharacterAssetData generated =
      CharacterRecipeGenerator::generate(characterRecipe);
  geometryMesh = std::move(generated.geometry);
  skeletonData = std::move(generated.skeleton);
}
