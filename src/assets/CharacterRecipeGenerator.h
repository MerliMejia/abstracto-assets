#pragma once

#include "assets/CharacterRecipe.h"
#include "assets/ImportedModelData.h"
#include "assets/ImportedSkeletonData.h"

struct GeneratedCharacterAssetData {
  ImportedGeometryAsset geometry;
  ImportedSkeletonData skeleton;
};

class CharacterRecipeGenerator {
public:
  static GeneratedCharacterAssetData generate(const CharacterRecipe &recipe);
};
