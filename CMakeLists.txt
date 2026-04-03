cmake_minimum_required(VERSION 3.20)
project(abstracto_assets LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ABSTRACTO_ASSETS_FETCH_DEPS
       "Fetch GLM, stb, tinyobjloader, and tinygltf when missing"
       ON)

if(ABSTRACTO_ASSETS_FETCH_DEPS)
  include(FetchContent)
endif()

find_package(glm CONFIG QUIET)
if(NOT glm_FOUND AND ABSTRACTO_ASSETS_FETCH_DEPS)
  FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
  )
  FetchContent_MakeAvailable(glm)
endif()

if(DEFINED stb_SOURCE_DIR)
  set(ABSTRACTO_ASSETS_STB_INCLUDE_DIR ${stb_SOURCE_DIR})
elseif(ABSTRACTO_ASSETS_FETCH_DEPS)
  FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
  )
  FetchContent_MakeAvailable(stb)
  set(ABSTRACTO_ASSETS_STB_INCLUDE_DIR ${stb_SOURCE_DIR})
else()
  find_path(ABSTRACTO_ASSETS_STB_INCLUDE_DIR stb_image.h)
endif()

if(DEFINED tinyobjloader_SOURCE_DIR)
  set(ABSTRACTO_ASSETS_TINYOBJ_INCLUDE_DIR ${tinyobjloader_SOURCE_DIR})
elseif(ABSTRACTO_ASSETS_FETCH_DEPS)
  FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
  )
  FetchContent_MakeAvailable(tinyobjloader)
  set(ABSTRACTO_ASSETS_TINYOBJ_INCLUDE_DIR ${tinyobjloader_SOURCE_DIR})
else()
  find_path(ABSTRACTO_ASSETS_TINYOBJ_INCLUDE_DIR tiny_obj_loader.h)
endif()

if(DEFINED tinygltf_SOURCE_DIR)
  set(ABSTRACTO_ASSETS_TINYGLTF_INCLUDE_DIR ${tinygltf_SOURCE_DIR})
elseif(ABSTRACTO_ASSETS_FETCH_DEPS)
  FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.9.5
  )
  FetchContent_MakeAvailable(tinygltf)
  set(ABSTRACTO_ASSETS_TINYGLTF_INCLUDE_DIR ${tinygltf_SOURCE_DIR})
else()
  find_path(ABSTRACTO_ASSETS_TINYGLTF_INCLUDE_DIR tiny_gltf.h)
endif()

if(NOT TARGET glm::glm AND NOT TARGET glm)
  message(FATAL_ERROR "GLM not found. Install it or set ABSTRACTO_ASSETS_FETCH_DEPS=ON.")
endif()

if(NOT ABSTRACTO_ASSETS_STB_INCLUDE_DIR)
  message(FATAL_ERROR "stb not found. Install it or set ABSTRACTO_ASSETS_FETCH_DEPS=ON.")
endif()

if(NOT ABSTRACTO_ASSETS_TINYOBJ_INCLUDE_DIR)
  message(FATAL_ERROR "tinyobjloader not found. Install it or set ABSTRACTO_ASSETS_FETCH_DEPS=ON.")
endif()

if(NOT ABSTRACTO_ASSETS_TINYGLTF_INCLUDE_DIR)
  message(FATAL_ERROR "tinygltf not found. Install it or set ABSTRACTO_ASSETS_FETCH_DEPS=ON.")
endif()

add_library(
  abstracto_assets
  STATIC
    src/CharacterModelAsset.cpp
    src/CharacterRecipeGenerator.cpp
    src/GltfModelAsset.cpp
    src/StbImageImplementation.cpp
    src/TinyObjLoaderImplementation.cpp
)
target_include_directories(abstracto_assets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(
  abstracto_assets
  PUBLIC
    ${ABSTRACTO_ASSETS_STB_INCLUDE_DIR}
    ${ABSTRACTO_ASSETS_TINYOBJ_INCLUDE_DIR}
    ${ABSTRACTO_ASSETS_TINYGLTF_INCLUDE_DIR}
)
if(TARGET glm::glm)
  target_link_libraries(abstracto_assets PUBLIC glm::glm)
else()
  target_link_libraries(abstracto_assets PUBLIC glm)
endif()

add_executable(AbstractoAssetsExample apps/assets_example/main.cpp)
target_link_libraries(AbstractoAssetsExample PRIVATE abstracto_assets)
