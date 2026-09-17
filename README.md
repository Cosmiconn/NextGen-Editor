# NextGen-Editor

Standalone Fiesta asset editor. The map-editor core remains GUI-free so format handling and tests can be reused independently of the desktop UI.

## Current focus

- Legacy map editing core (heightmaps, textures, Block&Walk, object placement)
- SHN client/server editor
- SHN files are kept in their original binary format and can be loaded, edited and saved again
- Client SHNs are discovered below `<Client>/ressystem`
- Server SHNs are discovered below `<Server>/9Data/Shine`

The project stores the **Client root** and **Server root** only; the SHN subdirectories are derived automatically by the editor.

## Build

The application uses C++23, CMake and vcpkg for GLFW, Dear ImGui, glad and OpenGL dependencies. GUI-free core tests can be built independently with a C++23 compiler.

See `CMakeLists.txt` and the documentation files for the current build and format details.

## Repository layout

- `include/mapeditor/core/` — GUI-free editor/format APIs
- `src/core/` — core implementations
- `src/app/` — desktop UI and rendering
- `tests/` — core regression tests
- `docs/` — format and implementation documentation
- `SHN_EDITOR.md` — SHN editor architecture and workflow
