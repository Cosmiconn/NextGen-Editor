cmake_minimum_required(VERSION 3.25)
project(NextGenEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_library(mapeditor_core
    src/core/BlendMap.cpp
    src/core/DdsImage.cpp
    src/core/EditOps.cpp
    src/core/Heightmap.cpp
    src/core/HeightmapIO.cpp
    src/core/NifModel.cpp
    src/core/ObjectPlacement.cpp
    src/core/ObjectPlacementIO.cpp
    src/core/ObjectSpatialIndex.cpp
    src/core/TextureLayerIO.cpp
    src/core/TextureLayerStack.cpp
    src/core/TexturePaintOps.cpp
    src/core/WalkEditOps.cpp
    src/core/WalkGridIO.cpp
    src/core/legacy/BmpBlendMap.cpp
    src/core/legacy/LegacyIdmAid.cpp
    src/core/legacy/LegacyMapIni.cpp
    src/core/legacy/LegacyMapProject.cpp
    src/core/legacy/LegacyPathResolve.cpp
    src/core/legacy/LegacyTextureSetIO.cpp
    src/core/legacy/ShnFile.cpp
)

target_include_directories(mapeditor_core PUBLIC include)

target_compile_features(mapeditor_core PUBLIC cxx_std_23)

add_executable(nextgen_editor
    src/app/main.cpp
    src/app/Camera.cpp
    src/app/NifMeshRenderer.cpp
    src/app/ObjectMarkerRenderer.cpp
    src/app/Renderer.cpp
)

target_link_libraries(nextgen_editor PRIVATE mapeditor_core glfw imgui::imgui glad::glad OpenGL::GL)

target_include_directories(nextgen_editor PRIVATE include)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_heightmap_core tests/test_heightmap_core.cpp)
    target_link_libraries(test_heightmap_core PRIVATE mapeditor_core)
    add_test(NAME heightmap_core COMMAND test_heightmap_core)

    add_executable(test_legacy_map_project tests/test_legacy_map_project.cpp)
    target_link_libraries(test_legacy_map_project PRIVATE mapeditor_core)
    add_test(NAME legacy_map_project COMMAND test_legacy_map_project)

    add_executable(test_legacy_texture_roundtrip tests/test_legacy_texture_roundtrip.cpp)
    target_link_libraries(test_legacy_texture_roundtrip PRIVATE mapeditor_core)
    add_test(NAME legacy_texture_roundtrip COMMAND test_legacy_texture_roundtrip)

    add_executable(test_nif_model tests/test_nif_model.cpp)
    target_link_libraries(test_nif_model PRIVATE mapeditor_core)
    add_test(NAME nif_model COMMAND test_nif_model)

    add_executable(test_object_placement tests/test_object_placement.cpp)
    target_link_libraries(test_object_placement PRIVATE mapeditor_core)
    add_test(NAME object_placement COMMAND test_object_placement)

    add_executable(test_shn_file tests/test_shn_file.cpp)
    target_link_libraries(test_shn_file PRIVATE mapeditor_core)
    add_test(NAME shn_file COMMAND test_shn_file)

    add_executable(test_texture_layers tests/test_texture_layers.cpp)
    target_link_libraries(test_texture_layers PRIVATE mapeditor_core)
    add_test(NAME texture_layers COMMAND test_texture_layers)

    add_executable(test_walk_grid tests/test_walk_grid.cpp)
    target_link_libraries(test_walk_grid PRIVATE mapeditor_core)
    add_test(NAME walk_grid COMMAND test_walk_grid)
endif()
