#include "ObjectMarkerRenderer.hpp"

#include <glad/glad.h>

#include <cmath>
#include <cstdio>
#include <unordered_set>
#include <vector>

namespace theseed::mapeditor::app {

namespace {

// Grund-Marker-Größe in Weltraum-Einheiten (wird mit obj.scale multipliziert) - so groß gewählt,
// dass Objekte bei typischer Kartenausdehnung (mehrere Tausend Einheiten) sichtbar bleiben, ohne
// das Terrain zu überdecken.
constexpr float kBaseMarkerSize = 120.0f;

const char* kVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aModelCol0;
layout(location = 3) in vec4 aModelCol1;
layout(location = 4) in vec4 aModelCol2;
layout(location = 5) in vec4 aModelCol3;
layout(location = 6) in float aHighlight;

uniform mat4 uViewProj;

out vec3 vNormal;
out float vHighlight;

void main() {
    mat4 model = mat4(aModelCol0, aModelCol1, aModelCol2, aModelCol3);
    vec4 worldPos = model * vec4(aPos, 1.0);
    vNormal = mat3(model) * aNormal;
    vHighlight = aHighlight;
    gl_Position = uViewProj * worldPos;
}
)";

const char* kFragmentShaderSrc = R"(
#version 330 core
in vec3 vNormal;
in float vHighlight;
out vec4 FragColor;

uniform vec3 uLightDir;

void main() {
    vec3 n = normalize(vNormal);
    float diff = max(dot(n, normalize(-uLightDir)), 0.0);
    // Blau (unselektiert) -> Weiß (selektiert) - passend zum Schwarz/Blau/Grau/Weiß-Theme
    // (siehe CHANGELOG), konsistent mit den Markern der 2D-Ansicht.
    vec3 baseColor = mix(vec3(0.25, 0.45, 0.78), vec3(1.0, 1.0, 1.0), vHighlight);
    vec3 color = baseColor * (0.5 + 0.5 * diff);
    FragColor = vec4(color, 1.0);
}
)";

std::uint32_t CompileShader(std::uint32_t type, const char* src) {
    const std::uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[ObjectMarkerRenderer] Shader-Kompilierfehler: %s\n", log);
    }
    return shader;
}

std::uint32_t LinkProgram(std::uint32_t vs, std::uint32_t fs) {
    const std::uint32_t program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[ObjectMarkerRenderer] Programm-Linkfehler: %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

struct Vec3 { float x, y, z; };

Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vec3 Normalize(Vec3 v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return len > 1e-6f ? Vec3{v.x / len, v.y / len, v.z / len} : Vec3{0, 1, 0};
}

void PushVertex(std::vector<float>& out, Vec3 p, Vec3 n) {
    out.insert(out.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
}

void PushTriangle(std::vector<float>& out, Vec3 a, Vec3 b, Vec3 c) {
    const Vec3 n = Normalize(Cross(Sub(b, a), Sub(c, a)));
    PushVertex(out, a, n);
    PushVertex(out, b, n);
    PushVertex(out, c, n);
}

// Baut das Unit-Marker-Mesh: eine "Stecknadel" (Pyramide, zeigt Position) + ein flacher Pfeil
// am Boden entlang lokal +Z (zeigt Blickrichtung/Rotation an).
std::vector<float> BuildMarkerMesh() {
    std::vector<float> verts;

    const Vec3 apex{0.0f, 1.0f, 0.0f};
    const Vec3 b0{-0.22f, 0.06f, -0.22f};
    const Vec3 b1{0.22f, 0.06f, -0.22f};
    const Vec3 b2{0.22f, 0.06f, 0.22f};
    const Vec3 b3{-0.22f, 0.06f, 0.22f};

    // Pyramiden-Seitenflächen.
    PushTriangle(verts, b0, b1, apex);
    PushTriangle(verts, b1, b2, apex);
    PushTriangle(verts, b2, b3, apex);
    PushTriangle(verts, b3, b0, apex);

    // Boden der Pyramide (kein Backface-Culling aktiv, Wicklung daher unkritisch).
    PushTriangle(verts, b0, b2, b1);
    PushTriangle(verts, b0, b3, b2);

    // Richtungspfeil am Boden, zeigt lokal +Z (Blickrichtung des Objekts).
    const Vec3 tip{0.0f, 0.03f, 0.55f};
    const Vec3 left{-0.28f, 0.03f, -0.15f};
    const Vec3 right{0.28f, 0.03f, -0.15f};
    PushTriangle(verts, left, right, tip);

    return verts;
}

Mat4 QuatToMat4(float x, float y, float z, float w) {
    Mat4 m = Mat4::Identity();
    m.m[0] = 1 - 2 * (y * y + z * z);
    m.m[1] = 2 * (x * y + w * z);
    m.m[2] = 2 * (x * z - w * y);

    m.m[4] = 2 * (x * y - w * z);
    m.m[5] = 1 - 2 * (x * x + z * z);
    m.m[6] = 2 * (y * z + w * x);

    m.m[8] = 2 * (x * z + w * y);
    m.m[9] = 2 * (y * z - w * x);
    m.m[10] = 1 - 2 * (x * x + y * y);
    return m;
}

} // namespace

ObjectMarkerRenderer::~ObjectMarkerRenderer() { Shutdown(); }

void ObjectMarkerRenderer::Init() {
    const std::vector<float> mesh = BuildMarkerMesh();

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<long>(mesh.size() * sizeof(float)), mesh.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    // "indexCount_" hier als Vertex-Anzahl (kein Index-Buffer nötig, Mesh ist winzig).
    indexCount_ = static_cast<std::uint32_t>(mesh.size() / 6);

    glGenBuffers(1, &instanceVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    // Layout pro Instanz: mat4 (4x vec4) + float highlight = 17 floats.
    constexpr std::size_t kFloatsPerInstance = 17;
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(2 + i);
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE,
                               static_cast<GLsizei>(kFloatsPerInstance * sizeof(float)),
                               reinterpret_cast<void*>(static_cast<std::size_t>(i) * 4 * sizeof(float)));
        glVertexAttribDivisor(2 + i, 1);
    }
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE,
                           static_cast<GLsizei>(kFloatsPerInstance * sizeof(float)),
                           reinterpret_cast<void*>(16 * sizeof(float)));
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);

    const std::uint32_t vs = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    const std::uint32_t fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    shaderProgram_ = LinkProgram(vs, fs);
}

void ObjectMarkerRenderer::Shutdown() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (instanceVbo_) glDeleteBuffers(1, &instanceVbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (shaderProgram_) glDeleteProgram(shaderProgram_);
    vbo_ = instanceVbo_ = ebo_ = vao_ = shaderProgram_ = 0;
}

void ObjectMarkerRenderer::RebuildInstances(const core::ObjectPlacementSet& set, int selectedIndex,
                                             const std::function<bool(std::size_t)>& skipIndex) {
    std::vector<int> selected;
    if (selectedIndex >= 0) selected.push_back(selectedIndex);
    RebuildInstances(set, selected, skipIndex);
}

void ObjectMarkerRenderer::RebuildInstances(const core::ObjectPlacementSet& set,
                                             const std::vector<int>& selectedIndices,
                                             const std::function<bool(std::size_t)>& skipIndex) {
    const std::unordered_set<int> selectedSet(selectedIndices.begin(), selectedIndices.end());
    std::vector<float> instances;
    instances.reserve(set.Count() * 17);

    std::uint32_t kept = 0;
    for (std::size_t i = 0; i < set.Count(); ++i) {
        if (skipIndex && skipIndex(i)) continue;
        const auto& obj = set.At(i);
        Mat4 model = QuatToMat4(obj.rotX, obj.rotY, obj.rotZ, obj.rotW);
        const float s = obj.scale * kBaseMarkerSize;
        // Skalierung in die Rotationsmatrix einrechnen (Spalten skalieren = vor der Rotation
        // anwenden, da Spalten hier die rotierten Basisvektoren sind).
        for (int col = 0; col < 3; ++col) {
            model.m[col * 4 + 0] *= s;
            model.m[col * 4 + 1] *= s;
            model.m[col * 4 + 2] *= s;
        }
        model.m[12] = obj.posX;
        model.m[13] = obj.posY;
        model.m[14] = obj.posZ;

        const float highlight = selectedSet.contains(static_cast<int>(i)) ? 1.0f : 0.0f;
        for (int v = 0; v < 16; ++v) instances.push_back(model.m[v]);
        instances.push_back(highlight);
        ++kept;
    }

    instanceCount_ = kept;

    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    if (!instances.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<long>(instances.size() * sizeof(float)), instances.data(), GL_DYNAMIC_DRAW);
    } else {
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    }
}

void ObjectMarkerRenderer::Draw(const OrbitCamera& camera, int width, int height) {
    if (instanceCount_ == 0 || width <= 0 || height <= 0) return;

    glUseProgram(shaderProgram_);
    const Mat4 view = camera.ViewMatrix();
    const Mat4 proj = OrbitCamera::PerspectiveMatrix(0.9f, static_cast<float>(width) / static_cast<float>(height), camera.NearPlane(), camera.FarPlane());
    const Mat4 viewProj = proj * view;
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram_, "uViewProj"), 1, GL_FALSE, viewProj.m);
    glUniform3f(glGetUniformLocation(shaderProgram_, "uLightDir"), -0.4f, -1.0f, -0.3f);

    glBindVertexArray(vao_);
    glDrawArraysInstanced(GL_TRIANGLES, 0, static_cast<int>(indexCount_), static_cast<int>(instanceCount_));
    glBindVertexArray(0);
}

} // namespace theseed::mapeditor::app
