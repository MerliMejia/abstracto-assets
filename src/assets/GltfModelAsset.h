#pragma once

#include "assets/ImportedSkeletonData.h"
#include "assets/ModelAsset.h"

class GltfModelAsset : public ModelAsset {
public:
  void load(const std::string &path);

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
    return skeletonData.nodes.empty() && skeletonData.skins.empty() &&
                   skeletonData.animations.empty()
               ? nullptr
               : &skeletonData;
  }

private:
  std::string sourcePath;
  ImportedGeometryAsset geometryMesh;
  ImportedSkeletonData skeletonData;
};
