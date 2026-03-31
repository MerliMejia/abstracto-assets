# abstracto-assets

`abstracto-assets` is the asset bridge layer for the Abstracto workspace.

It sits between imported content and the renderer-facing runtime. The module
loads model files, extracts mesh/material/skeleton data, and packages them into
structures that can later be uploaded and rendered by higher-level runtime code.

## What This Repo Owns

- Importing `.obj`, `.gltf`, and `.glb` model assets
- Building `ModelAsset` implementations for imported content
- Exposing mesh, material, submesh, and skeleton metadata
- Bridging imported assets into renderer-ready `RenderableModel` instances
- Preparing animation-aware skin palette bindings for animated models

## Main Types

- `ModelAsset`
  Common interface for imported model data. Exposes mesh geometry, materials,
  submeshes, source path, and optional skeleton data.
- `ObjModelAsset`
  Imports `.obj` content into `ModelAsset`.
- `GltfModelAsset`
  Imports `.gltf` and `.glb` content into `ModelAsset`, including skeleton and
  animation source data when present.
- `RenderableModel`
  Runtime-facing wrapper that uploads imported data to GPU resources, builds
  renderer `RenderItem`s, supports material overrides, and updates skinning
  state for animated meshes.

## Features

- Flat public include surface from `src/`
- OBJ and glTF/GLB model loading
- Material extraction, including texture source metadata
- Submesh-aware rendering data
- Skeleton, skin, and animation source extraction from glTF assets
- Runtime material overrides during asset load
- Renderer-ready render item generation per pass
- Per-frame animated skin palette updates

## Repository Layout

```text
src/
  ModelAsset.h
  ObjModelAsset.h
  GltfModelAsset.h
  RenderableModel.h

apps/assets_example/
  main.cpp

assets/
  tree.glb
```

## Build

From the repo root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DABSTRACTO_FETCH_DEPS=ON
cmake --build build --target AbstractoAssetsExample -j8
```

Notes:

- The build uses the repo-local `build/` directory.
- On first configure, the repo fetches the renderer dependency it needs if
  `abstracto_renderer` is not already available.
- For workspace development, you can point the build at a sibling renderer
  checkout with `-DABSTRACTO_ASSETS_USE_LOCAL_RENDERER=ON`.
- The fetched renderer dependency can in turn fetch third-party packages when
  `ABSTRACTO_FETCH_DEPS=ON`.

## Example App

The example target imports the bundled `assets/tree.glb`, applies a simple
authoring-time material tint, and prints a summary of the imported asset:

- mesh vertex and index counts
- submesh count
- material information
- skeleton, skin, and animation counts when present

Run it from the repo root:

```bash
./build/AbstractoAssetsExample
```

## Using The Library

### Import asset data only

Use `GltfModelAsset` or `ObjModelAsset` when you want imported content and
metadata without performing GPU upload yet.

```cpp
#include "GltfModelAsset.h"

GltfModelAsset asset;
asset.load("assets/tree.glb");

const auto &materials = asset.materials();
const auto &submeshes = asset.submeshes();
const SkeletonAssetData *skeleton = asset.skeletonAsset();
```

### Build a runtime renderable

Use `RenderableModel` when renderer state is available and the asset should be
uploaded and turned into renderable items.

```cpp
#include "RenderableModel.h"

RenderableModel model;
model.loadFromFile(path,
                   commandContext,
                   deviceContext,
                   descriptorSetLayout,
                   secondaryDescriptorSetLayout,
                   frameGeometryUniforms,
                   sampler,
                   framesInFlight);
```

You can also provide a material override callback during load to adjust imported
materials before the renderer-side material set is created.

## Integration Pattern

The intended usage split is:

1. Import content through `ModelAsset` implementations when inspecting or
   preprocessing assets.
2. Hand the asset path and any authoring overrides to `RenderableModel` once
   renderer state is ready.
3. Build render items per target pass.
4. Advance animation playback and update skin palettes every frame for skinned
   assets.

This keeps asset import concerns in `abstracto-assets` while leaving scene/app
composition to the consuming runtime.
