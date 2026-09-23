cmake_minimum_required(VERSION 3.21)
project(TheSeedMapEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC interpretiert Quelldateien standardmäßig nach der System-Codepage, nicht UTF-8 - die
# Quelldateien (v.a. src/app/main.cpp) enthalten aber direkt eingebettete deutsche Umlaute in
# String-Literalen (ImGui-Labels). Ohne /utf-8 würden diese unter Windows/MSVC falsch interpretiert
# (Mojibake zur Laufzeit). GCC/Clang gehen ohnehin von UTF-8 aus, daher nur für MSVC nötig.
if (MSVC)
    add_compile_options(/utf-8)
endif()

# -----------------------------------------------------------------------------
# Core-Bibliothek: bewusst frei von GUI-/GL-Abhängigkeiten. Wird von der App und
# von den Tests genutzt - Tests laufen dadurch auch headless (CI-fähig).
# -----------------------------------------------------------------------------
add_library(mapeditor_core
    src/core/Heightmap.cpp
    src/core/HeightmapIO.cpp
    src/core/EditOps.cpp
    src/core/BlendMap.cpp
    src/core/DdsImage.cpp
    src/core/NifModel.cpp
    src/core/TextureLayerStack.cpp
    src/core/TexturePaintOps.cpp
    src/core/TextureLayerIO.cpp
    src/core/WalkGridIO.cpp
    src/core/WalkEditOps.cpp
    src/core/ObjectPlacement.cpp
    src/core/ObjectPlacementIO.cpp
    src/core/legacy/LegacyMapIni.cpp
    src/core/legacy/BmpBlendMap.cpp
    src/core/legacy/LegacyTextureSetIO.cpp
    src/core/legacy/LegacyIdmAid.cpp
    src/core/legacy/LegacyPathResolve.cpp
    src/core/legacy/ShnFile.cpp
    src/core/legacy/LegacyMapProject.cpp
)
target_include_directories(mapeditor_core PUBLIC include)
target_compile_features(mapeditor_core PUBLIC cxx_std_23)

# -----------------------------------------------------------------------------
# Standalone-App (GLFW3 + Dear ImGui [docking, glfw+opengl3-binding] + glad)
# Installation via vcpkg, siehe vcpkg.json.
# -----------------------------------------------------------------------------
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(map_editor
    src/app/main.cpp
    src/app/Camera.cpp
    src/app/Renderer.cpp
    src/app/ObjectMarkerRenderer.cpp
    src/app/NifMeshRenderer.cpp
)
target_include_directories(map_editor PRIVATE src/app)
target_link_libraries(map_editor PRIVATE
    mapeditor_core
    glfw
    imgui::imgui
    glad::glad
    OpenGL::GL
)
if (WIN32)
    # Für den nativen "Ordner wählen"-Dialog (SHBrowseForFolder/CoTaskMemFree) in main.cpp.
    target_link_libraries(map_editor PRIVATE Shell32 Ole32)
endif()

# -----------------------------------------------------------------------------
# Tests - keine GUI-/GL-Abhängigkeit, siehe tests/test_heightmap_core.cpp.
# Direkter g++-Aufruf ohne CMake ist ebenfalls möglich, siehe README.md.
# -----------------------------------------------------------------------------
enable_testing()
add_executable(test_heightmap_core tests/test_heightmap_core.cpp)
target_link_libraries(test_heightmap_core PRIVATE mapeditor_core)
add_test(NAME heightmap_core COMMAND test_heightmap_core)

add_executable(test_texture_layers tests/test_texture_layers.cpp)
target_link_libraries(test_texture_layers PRIVATE mapeditor_core)
add_test(NAME texture_layers COMMAND test_texture_layers
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_legacy_texture_roundtrip tests/test_legacy_texture_roundtrip.cpp)
target_link_libraries(test_legacy_texture_roundtrip PRIVATE mapeditor_core)
add_test(NAME legacy_texture_roundtrip COMMAND test_legacy_texture_roundtrip
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_walk_grid tests/test_walk_grid.cpp)
target_link_libraries(test_walk_grid PRIVATE mapeditor_core)
add_test(NAME walk_grid COMMAND test_walk_grid)

add_executable(test_object_placement tests/test_object_placement.cpp)
target_link_libraries(test_object_placement PRIVATE mapeditor_core)
add_test(NAME object_placement COMMAND test_object_placement)

add_executable(test_legacy_map_project tests/test_legacy_map_project.cpp)
target_link_libraries(test_legacy_map_project PRIVATE mapeditor_core)
add_test(NAME legacy_map_project COMMAND test_legacy_map_project)

add_executable(test_shn_file tests/test_shn_file.cpp)
target_link_libraries(test_shn_file PRIVATE mapeditor_core)

add_executable(test_nif_model tests/test_nif_model.cpp)
target_link_libraries(test_nif_model PRIVATE mapeditor_core)
add_test(NAME nif_model COMMAND test_nif_model)

# Real Fiesta particle-system regression fixtures. These exercise the NiPSysData
# inheritance/alignment path that previously shifted following blocks and made
# downstream NiPixelData/NiTri* blocks fail.
add_test(NAME nif_particle_h_rou_light
         COMMAND test_nif_model "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/H_rou_Light.nif")
add_test(NAME nif_particle_rou_waterwell
         COMMAND test_nif_model "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/rou_waterwell.nif")

add_executable(test_nif_fixture_corpus tests/test_nif_fixture_corpus.cpp)
target_link_libraries(test_nif_fixture_corpus PRIVATE mapeditor_core)
add_test(NAME nif_fixture_corpus
         COMMAND test_nif_fixture_corpus "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
