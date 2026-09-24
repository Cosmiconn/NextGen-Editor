cmake_minimum_required(VERSION 3.21)
project(TheSeedMapEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    src/core/KfmFile.cpp
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
    src/core/legacy/ShineText.cpp
    src/core/legacy/QuestData.cpp
    src/core/AvatarPreview.cpp
    src/core/Manual.cpp
    src/core/ManualData.cpp
    src/core/legacy/LegacyMapProject.cpp
)
target_include_directories(mapeditor_core PUBLIC include)
target_compile_features(mapeditor_core PUBLIC cxx_std_23)

# -----------------------------------------------------------------------------
option(NEXTGEN_EDITOR_BUILD_GUI "Build the GLFW/OpenGL GUI application" OFF)

# Standalone-App (GLFW3 + Dear ImGui [docking, glfw+opengl3-binding] + glad)
# Installation via vcpkg, siehe vcpkg.json.
# -----------------------------------------------------------------------------
if(NEXTGEN_EDITOR_BUILD_GUI)
find_package(glfw3 CONFIG QUIET)
if(NOT TARGET glfw)
    include(FetchContent)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.5.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)
endif()
find_package(imgui CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(map_editor
    src/app/main.cpp
    src/app/KfmPanel.cpp
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
    target_link_libraries(map_editor PRIVATE Shell32 Ole32 Windowscodecs)
endif()
endif() # NEXTGEN_EDITOR_BUILD_GUI

# Tests - keine GUI-/GL-Abhängigkeit, siehe tests/test_heightmap_core.cpp.
# Direkter g++-Aufruf ohne CMake ist ebenfalls möglich, siehe README.md.
# -----------------------------------------------------------------------------
enable_testing()
add_executable(nif_probe tools/nif_probe.cpp)
target_link_libraries(nif_probe PRIVATE mapeditor_core)
add_executable(nif_benchmark tools/nif_benchmark.cpp)
target_link_libraries(nif_benchmark PRIVATE mapeditor_core)
add_executable(fiesta_audit tools/fiesta_audit.cpp)
target_link_libraries(fiesta_audit PRIVATE mapeditor_core)
add_executable(test_process_runner tests/test_process_runner.cpp)
add_test(NAME process_runner COMMAND test_process_runner)
add_executable(test_heightmap_core tests/test_heightmap_core.cpp)
target_link_libraries(test_heightmap_core PRIVATE mapeditor_core)
add_test(NAME heightmap_core COMMAND test_heightmap_core
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Rou.HTD ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Rou.HTDG)

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
add_test(NAME walk_grid COMMAND test_walk_grid ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Rou.shbd)

add_executable(test_object_placement tests/test_object_placement.cpp)
target_link_libraries(test_object_placement PRIVATE mapeditor_core)
add_test(NAME object_placement COMMAND test_object_placement
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Rou.shmd ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Rou.idm
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Rou.aid)

add_executable(test_legacy_map_project tests/test_legacy_map_project.cpp)
target_link_libraries(test_legacy_map_project PRIVATE mapeditor_core)
add_test(NAME legacy_map_project COMMAND test_legacy_map_project ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Rou.ini)

add_executable(test_shn_file tests/test_shn_file.cpp)

add_executable(test_shinetext tests/test_shinetext.cpp)
add_executable(test_questdata tests/test_questdata.cpp)
target_link_libraries(test_shn_file PRIVATE mapeditor_core)
target_link_libraries(test_shinetext PRIVATE mapeditor_core)
target_link_libraries(test_questdata PRIVATE mapeditor_core)
add_test(NAME shn_file COMMAND test_shn_file ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/data/ChrBasicEquip.shn)
add_test(NAME shinetext COMMAND test_shinetext ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/data/NPC.txt)
add_test(NAME questdata COMMAND test_questdata ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/data/QuestData.shn)

add_executable(test_kfm_file tests/test_kfm_file.cpp)
target_link_libraries(test_kfm_file PRIVATE mapeditor_core)
add_test(NAME kfm_file COMMAND test_kfm_file)

add_executable(test_packed_images tests/test_packed_images.cpp)
target_link_libraries(test_packed_images PRIVATE mapeditor_core)
add_test(NAME packed_images COMMAND test_packed_images)

add_executable(test_nif_extensions tests/test_nif_extensions.cpp)
target_link_libraries(test_nif_extensions PRIVATE mapeditor_core)
add_test(NAME nif_extensions COMMAND test_nif_extensions ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/nif-extensions)

add_executable(test_nif_model tests/test_nif_model.cpp)
target_link_libraries(test_nif_model PRIVATE mapeditor_core)
add_test(NAME nif_model COMMAND test_nif_model ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tree05.nif)

add_executable(test_nif_particles tests/test_nif_particles.cpp)
target_link_libraries(test_nif_particles PRIVATE mapeditor_core)
add_test(NAME nif_particles COMMAND test_nif_particles ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/particles)

add_executable(test_camera_handedness tests/test_camera_handedness.cpp src/app/Camera.cpp)
target_include_directories(test_camera_handedness PRIVATE src/app)
add_test(NAME camera_handedness COMMAND test_camera_handedness)

add_executable(test_avatar_preview tests/test_avatar_preview.cpp)
target_link_libraries(test_avatar_preview PRIVATE mapeditor_core)
add_test(NAME avatar_preview COMMAND test_avatar_preview)

add_executable(test_manual tests/test_manual.cpp)
target_link_libraries(test_manual PRIVATE mapeditor_core)
add_test(NAME manual COMMAND test_manual)

if(NEXTGEN_EDITOR_BUILD_GUI)
# Bindet src/app/main.cpp direkt ein (wie map_editor) - braucht dieselben GUI-/GL-Abhaengigkeiten,
# laeuft aber dank g_disableGlUploads GL-frei. Regressionstest fuer CHANGELOG [0.44.34]
# (NPC-Blickrichtung: Vorzeichen/Versatz).
add_executable(test_npc_orientation
    tests/test_npc_orientation.cpp
    src/app/KfmPanel.cpp
    src/app/Camera.cpp
    src/app/Renderer.cpp
    src/app/ObjectMarkerRenderer.cpp
    src/app/NifMeshRenderer.cpp
)
target_include_directories(test_npc_orientation PRIVATE src/app)
target_link_libraries(test_npc_orientation PRIVATE
    mapeditor_core
    glfw
    imgui::imgui
    glad::glad
    OpenGL::GL
)
if (WIN32)
    target_link_libraries(test_npc_orientation PRIVATE Shell32 Ole32 Windowscodecs)
endif()
add_test(NAME npc_orientation COMMAND test_npc_orientation)

add_executable(test_kfm_opengl tests/test_kfm_opengl.cpp src/app/KfmPanel.cpp)
target_include_directories(test_kfm_opengl PRIVATE src/app)
target_link_libraries(test_kfm_opengl PRIVATE mapeditor_core glfw imgui::imgui glad::glad OpenGL::GL)

add_executable(test_nif_opengl tests/test_nif_opengl.cpp src/app/Camera.cpp src/app/NifMeshRenderer.cpp)
target_include_directories(test_nif_opengl PRIVATE src/app)
target_link_libraries(test_nif_opengl PRIVATE mapeditor_core glfw glad::glad OpenGL::GL)
if(WIN32)
    target_link_libraries(test_nif_opengl PRIVATE Ole32 Windowscodecs)
endif()
option(NEXTGEN_EDITOR_ENABLE_GL_TESTS "Run tests requiring a real OpenGL context" OFF)
if(NEXTGEN_EDITOR_ENABLE_GL_TESTS)
    add_test(NAME nif_opengl COMMAND test_nif_opengl
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/particles ${CMAKE_CURRENT_BINARY_DIR}/gl-output)
endif()


endif() # GUI-only regression test

# Assertions are test checks, including in optimized builds. NDEBUG must never
# silently turn the SHN/Quest/ShineText roundtrip tests into no-ops.
get_property(editor_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(test_target IN LISTS editor_targets)
    if(test_target MATCHES "^test_")
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${test_target} PRIVATE -UNDEBUG)
        endif()
    endif()
endforeach()
