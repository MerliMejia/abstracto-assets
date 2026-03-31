#pragma once

#include "assets/ModelAsset.h"

class ObjModelAsset : public ModelAsset {
public:
  void load(const std::string &path) {
    sourcePath = path;
    geometryMesh.loadModel(path);
  }

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

private:
  std::string sourcePath;
  ImportedObjGeometryAsset geometryMesh;
};
