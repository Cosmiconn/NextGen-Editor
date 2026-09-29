#include "NifMeshRenderer.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <chrono>
#include <cctype>
#include <thread>

using namespace theseed::mapeditor;

namespace {

std::string SnapshotStem(std::string value) {
    for (char& ch : value) {
        const unsigned char u = static_cast<unsigned char>(ch);
        if (!(std::isalnum(u) || ch == '.' || ch == '_' || ch == '-')) ch = '_';
    }
    return value;
}

bool WritePpm(const std::filesystem::path& path,
              const std::vector<unsigned char>& pixels,
              int width,
              int height) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            out.write(reinterpret_cast<const char*>(
                          pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4),
                      3);
        }
    }
    return static_cast<bool>(out);
}

bool WriteBmp(const std::filesystem::path& path,
              const std::vector<unsigned char>& pixels,
              int width,
              int height) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    const std::uint32_t rowBytes = static_cast<std::uint32_t>(width * 3);
    const std::uint32_t rowStride = (rowBytes + 3u) & ~3u;
    const std::uint32_t imageBytes = rowStride * static_cast<std::uint32_t>(height);
    const std::uint32_t fileBytes = 54u + imageBytes;
    const auto u16 = [&](std::uint16_t v) {
        out.put(static_cast<char>(v & 0xffu));
        out.put(static_cast<char>((v >> 8u) & 0xffu));
    };
    const auto u32 = [&](std::uint32_t v) {
        out.put(static_cast<char>(v & 0xffu));
        out.put(static_cast<char>((v >> 8u) & 0xffu));
        out.put(static_cast<char>((v >> 16u) & 0xffu));
        out.put(static_cast<char>((v >> 24u) & 0xffu));
    };

    out.put('B'); out.put('M');
    u32(fileBytes); u16(0); u16(0); u32(54);
    u32(40); u32(static_cast<std::uint32_t>(width)); u32(static_cast<std::uint32_t>(height));
    u16(1); u16(24); u32(0); u32(imageBytes);
    u32(2835); u32(2835); u32(0); u32(0);

    const std::array<char, 3> padding{0, 0, 0};
    const std::uint32_t padBytes = rowStride - rowBytes;
    // OpenGL readback starts with the bottom row, exactly the order of a positive-height BMP.
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto index = (static_cast<std::size_t>(y) * width + x) * 4;
            out.put(static_cast<char>(pixels[index + 2]));
            out.put(static_cast<char>(pixels[index + 1]));
            out.put(static_cast<char>(pixels[index + 0]));
        }
        out.write(padding.data(), padBytes);
    }
    return static_cast<bool>(out);
}

std::size_t CountVisiblePixels(const std::vector<unsigned char>& pixels) {
    std::size_t lit = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] || pixels[i + 1] || pixels[i + 2]) ++lit;
    return lit;
}

} // namespace

// Explicit opt-in runtime test, requiring a real OpenGL context. The ordinary
// CTest suite remains usable on machines without a display/GPU.
int main(int argc, char** argv) {
    if (argc < 3) return 2;

    const std::filesystem::path modelRoot = argv[1];
    const std::filesystem::path snapshotOutputDir = argv[2];
    std::filesystem::path runtimeMapDir = modelRoot;
    bool explicitRuntimeMapDir = false;
    std::vector<std::string> names;
    for (int arg = 3; arg < argc; ++arg) {
        const std::string value = argv[arg];
        if (value == "--runtime-map-dir") {
            if (arg + 1 >= argc) {
                std::cerr << "--runtime-map-dir requires a directory\n";
                return 2;
            }
            runtimeMapDir = argv[++arg];
            explicitRuntimeMapDir = true;
        } else {
            names.push_back(value);
        }
    }
    if (names.empty()) {
        for (const auto& entry : std::filesystem::directory_iterator(modelRoot))
            if (entry.path().extension() == ".nif") names.push_back(entry.path().filename().string());
        std::sort(names.begin(), names.end());
    }

    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return 1; }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    GLFWwindow* window = glfwCreateWindow(512, 512, "NIF runtime verification", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    std::cout << "GL_VENDOR=" << glGetString(GL_VENDOR) << "\nGL_RENDERER=" << glGetString(GL_RENDERER)
              << "\nGL_VERSION=" << glGetString(GL_VERSION) << '\n';
    int failures = 0;
    {
        app::NifMeshRenderer renderer;
        renderer.Init();
        if (glGetError() != GL_NO_ERROR) ++failures;
        if (names.empty()) ++failures;
        for (const auto& name : names) {
            auto model = core::LoadNifMesh(modelRoot / name, false);
            if (!model) { ++failures; continue; }
            const bool expectStaticVisible = std::any_of(model->parts.begin(), model->parts.end(),
                [](const auto& part) { return part.material.alpha > 0.0f; });
            const bool hasParticleSystems = !model->particleSystems.empty();
            float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
            for (const auto& part : model->parts) for (const auto& p : part.positions) {
                const float v[3] = {p.x, p.y, p.z};
                for (int i = 0; i < 3; ++i) { lo[i] = std::min(lo[i], v[i]); hi[i] = std::max(hi[i], v[i]); }
            }
            app::OrbitCamera camera;
            camera.SetTarget((lo[0]+hi[0])/2, (lo[1]+hi[1])/2, (lo[2]+hi[2])/2);
            const float extent = std::max({hi[0]-lo[0], hi[1]-lo[1], hi[2]-lo[2], 1.0f});
            camera.Zoom(extent * 2.0f - camera.Distance());
            core::ObjectPlacementSet set;
            core::PlacedObject object;
            object.modelPath = explicitRuntimeMapDir
                ? (std::filesystem::path("resmap") / std::filesystem::path(name)).generic_string()
                : name;
            set.AddObject(object);
            // Deliberately reload through the ordinary renderer path instead of injecting a
            // custom model. With --runtime-map-dir the test uses a production-like
            // <Client>/resmap/field/<Map> directory while the selected model still comes from
            // modelRoot; this exercises the real client-root texture resolver across split archives.
            renderer.LoadModelsForSet(set, runtimeMapDir);
            if (!renderer.HasRealMesh(0) || glGetError() != GL_NO_ERROR) ++failures;
            glViewport(0, 0, 512, 512);
            glClearColor(0, 0, 0, 1);
            glClearStencil(0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            renderer.Draw(set, camera, 512, 512);
            glFinish();
            std::vector<unsigned char> pixels(512 * 512 * 4);
            glReadPixels(0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            const auto error = glGetError();
            const std::size_t lit = CountVisiblePixels(pixels);
            std::cout << name << ": visible pixels=" << lit << ", GL error=" << error
                      << ", expected=" << (expectStaticVisible ? "static-visible" :
                          (hasParticleSystems ? "particle/runtime-dependent" : "transparent-static")) << '\n';
            if (error != GL_NO_ERROR ||
                (expectStaticVisible && lit < 20) ||
                (!expectStaticVisible && !hasParticleSystems && lit != 0))
                ++failures;

            const std::filesystem::path outputDir = snapshotOutputDir;
            std::filesystem::create_directories(outputDir);
            const std::string snapshotStem = SnapshotStem(name);
            if (!WritePpm(outputDir / (snapshotStem + ".ppm"), pixels, 512, 512) ||
                !WriteBmp(outputDir / (snapshotStem + ".bmp"), pixels, 512, 512)) {
                std::cerr << name << ": failed to write initial snapshot\n";
                ++failures;
            }

            const auto begin = std::chrono::steady_clock::now();
            for (int frame = 0; frame < 30; ++frame) {
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                renderer.Draw(set, camera, 512, 512);
            }
            glFinish();
            std::cout << name << ": warm_frame_ms=" <<
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() / 30.0 << '\n';

            // A second frame separated in wall-clock time makes authored texture controllers
            // and particle motion visually inspectable instead of only proving frame zero draws.
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            renderer.Draw(set, camera, 512, 512);
            glFinish();
            glReadPixels(0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            const auto animatedError = glGetError();
            const std::size_t animatedLit = CountVisiblePixels(pixels);
            std::cout << name << ": animated visible pixels=" << animatedLit
                      << ", GL error=" << animatedError << '\n';
            if (animatedError != GL_NO_ERROR ||
                (expectStaticVisible && animatedLit < 20) ||
                (!expectStaticVisible && !hasParticleSystems && animatedLit != 0))
                ++failures;
            if (!WritePpm(outputDir / (snapshotStem + "__animated.ppm"), pixels, 512, 512) ||
                !WriteBmp(outputDir / (snapshotStem + "__animated.bmp"), pixels, 512, 512)) {
                std::cerr << name << ": failed to write animated snapshot\n";
                ++failures;
            }
            // Negative control: hiding the object must leave the framebuffer clear.
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            const std::vector<char> hidden{1};
            renderer.Draw(set, camera, 512, 512, &hidden);
            glReadPixels(0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            for (std::size_t i = 0; i < pixels.size(); i += 4)
                if (pixels[i] || pixels[i+1] || pixels[i+2]) { ++failures; break; }
        }
        renderer.Shutdown();
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return failures ? 1 : 0;
}
