#pragma once

#include "assets/CharacterRecipe.h"
#include "assets/ModelAsset.h"

class CharacterModelAsset : public ModelAsset {
public:
  void load(const std::string &path);
  void setRecipe(CharacterRecipe recipe, std::string sourcePathValue);

  const CharacterRecipe &recipe() const { return characterRecipe; }
  CharacterRecipe &mutableRecipe() { return characterRecipe; }

  ImportedGeometryAsset &mesh() override { return geometryMesh; }
  const ImportedGeometryAsset &mesh() const override { return geometryMesh; }

  const std::vector<ImportedMaterialData> &materials() const override {
    return geometryMesh.materialsData();
  }

  std::vector<ImportedMaterialData> &mutableMaterials() override {
    return geometryMesh.mutableMaterialsData();
  }

  const std::vector<ImportedModelSubmesh> &submeshes() const override {
    return geometryMesh.submeshData();
  }

  const std::string &path() const override { return sourcePath; }

  const ImportedSkeletonData *skeletonAsset() const override {
    return skeletonData.nodes.empty() ? nullptr : &skeletonData;
  }

private:
  void rebuild();

  std::string sourcePath;
  CharacterRecipe characterRecipe = CharacterRecipe::mannequin();
  ImportedGeometryAsset geometryMesh;
  ImportedSkeletonData skeletonData;
};
