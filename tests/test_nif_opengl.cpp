#include "NifMeshRenderer.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <chrono>

using namespace theseed::mapeditor;

// Explicit opt-in runtime test, requiring a real OpenGL context. The ordinary
// CTest suite remains usable on machines without a display/GPU.
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return 1; }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
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
        std::vector<std::string> names;
        for (const auto& entry : std::filesystem::directory_iterator(argv[1]))
            if (entry.path().extension() == ".nif") names.push_back(entry.path().filename().string());
        std::sort(names.begin(), names.end());
        if (names.empty()) ++failures;
        for (const auto& name : names) {
            auto model = core::LoadNifMesh(std::filesystem::path(argv[1]) / name, false);
            if (!model) { ++failures; continue; }
            const bool expectVisible = std::any_of(model->parts.begin(), model->parts.end(),
                [](const auto& part) { return part.material.alpha > 0.0f; });
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
            core::PlacedObject object; object.modelPath = name; set.AddObject(object);
            std::unordered_map<std::string, core::NifModel> custom;
            custom.emplace(name, std::move(*model));
            renderer.LoadModelsForSet(set, argv[1], &custom);
            if (!renderer.HasRealMesh(0) || glGetError() != GL_NO_ERROR) ++failures;
            glViewport(0, 0, 512, 512);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderer.Draw(set, camera, 512, 512);
            glFinish();
            std::vector<unsigned char> pixels(512 * 512 * 4);
            glReadPixels(0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            const auto error = glGetError();
            std::size_t lit = 0;
            for (std::size_t i = 0; i < pixels.size(); i += 4)
                if (pixels[i] || pixels[i+1] || pixels[i+2]) ++lit;
            std::cout << name << ": visible pixels=" << lit << ", GL error=" << error
                      << ", expected=" << (expectVisible ? "visible" : "transparent (all materials alpha=0)") << '\n';
            if ((expectVisible ? lit < 20 : lit != 0) || error != GL_NO_ERROR) ++failures;
            std::filesystem::create_directories(argv[2]);
            std::ofstream out(std::filesystem::path(argv[2]) / (std::string(name) + ".ppm"), std::ios::binary);
            out << "P6\n512 512\n255\n";
            for (int y = 511; y >= 0; --y) for (int x = 0; x < 512; ++x)
                out.write(reinterpret_cast<const char*>(pixels.data() + (y * 512 + x) * 4), 3);
            const auto begin = std::chrono::steady_clock::now();
            for (int frame = 0; frame < 30; ++frame) {
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                renderer.Draw(set, camera, 512, 512);
            }
            glFinish();
            std::cout << name << ": warm_frame_ms=" <<
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() / 30.0 << '\n';
            // Negative control: hiding the object must leave the framebuffer clear.
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
