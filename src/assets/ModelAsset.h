#pragma once

#include "assets/ImportedModelData.h"
#include "assets/ImportedSkeletonData.h"

class ModelAsset {
public:
  virtual ~ModelAsset() = default;

  virtual ImportedGeometryAsset &mesh() = 0;
  virtual const ImportedGeometryAsset &mesh() const = 0;
  virtual std::vector<ImportedMaterialData> &mutableMaterials() = 0;
  virtual const std::vector<ImportedMaterialData> &materials() const = 0;
  virtual const std::vector<ImportedModelSubmesh> &submeshes() const = 0;
  virtual const std::string &path() const = 0;
  virtual const ImportedSkeletonData *skeletonAsset() const { return nullptr; }
};
