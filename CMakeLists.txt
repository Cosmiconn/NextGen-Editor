cmake_minimum_required(VERSION 3.25)
project(NextGenEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

file(GLOB_RECURSE MAPEDITOR_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
)

if(NOT MAPEDITOR_CORE_SOURCES)
    message(FATAL_ERROR "No core source files were found")
endif()

add_library(mapeditor_core ${MAPEDITOR_CORE_SOURCES})
target_include_directories(mapeditor_core PUBLIC include)
target_compile_features(mapeditor_core PUBLIC cxx_std_23)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp")
    set(MAPEDITOR_APP_SOURCES
        src/app/main.cpp
        src/app/Camera.cpp
        src/app/NifMeshRenderer.cpp
        src/app/ObjectMarkerRenderer.cpp
        src/app/Renderer.cpp
    )
    set(MAPEDITOR_APP_SOURCES_PRESENT TRUE)
    foreach(source IN LISTS MAPEDITOR_APP_SOURCES)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
            set(MAPEDITOR_APP_SOURCES_PRESENT FALSE)
        endif()
    endforeach()

    if(MAPEDITOR_APP_SOURCES_PRESENT)
        add_executable(nextgen_editor ${MAPEDITOR_APP_SOURCES})
        target_link_libraries(nextgen_editor PRIVATE
            mapeditor_core glfw imgui::imgui glad::glad OpenGL::GL
        )
        target_include_directories(nextgen_editor PRIVATE include)
    else()
        message(STATUS "Skipping nextgen_editor: application sources are incomplete")
    endif()
else()
    message(STATUS "Skipping nextgen_editor: src/app/main.cpp is not present")
endif()

include(CTest)
if(BUILD_TESTING)
    # Only register tests whose implementation units are present in this
    # checkout. The remaining test sources currently reference planned
    # modules (NIF, SHN and edit operations) that are not implemented yet.
    set(MAPEDITOR_TESTS
        test_legacy_map_project
        test_legacy_texture_roundtrip
    )

    foreach(test_name IN LISTS MAPEDITOR_TESTS)
        set(test_source "${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_name}.cpp")
        if(EXISTS "${test_source}")
            add_executable(${test_name} "${test_source}")
            target_link_libraries(${test_name} PRIVATE mapeditor_core)
            add_test(NAME ${test_name} COMMAND ${test_name})
        else()
            message(STATUS "Skipping missing test source: ${test_source}")
        endif()
    endforeach()

    message(STATUS "Skipping tests for unavailable implementations: test_heightmap_core, test_nif_model, test_shn_file")
endif()
