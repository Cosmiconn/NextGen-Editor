#include "NifMeshRenderer.hpp"

#include "mapeditor/core/DdsImage.hpp"
#include "mapeditor/core/legacy/LegacyPathResolve.hpp"

#include <glad/glad.h>

#include <cmath>
#include <cstdio>

namespace theseed::mapeditor::app {

namespace {

// Lambert-Shader mit optionalem Textur-Sampling: uHasTexture schaltet zwischen echter
// Diffuse-Textur (falls Datei geladen werden konnte und das Mesh eigene UVs mitbringt) und der
// aus dem Material extrahierten Flächenfarbe um (Fallback, unverändertes Verhalten für Meshes
// ohne Textur-Referenz oder ohne UVs).
const char* kVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

uniform mat4 uViewProj;
uniform mat4 uModel;

out vec3 vNormal;
out vec2 vUv;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vNormal = mat3(uModel) * aNormal;
    vUv = aUv;
    gl_Position = uViewProj * worldPos;
}
)";

const char* kFragmentShaderSrc = R"(
#version 330 core
in vec3 vNormal;
in vec2 vUv;
out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uDiffuseColor;
uniform bool uHasTexture;
uniform sampler2D uDiffuseTex;

void main() {
    vec3 n = normalize(vNormal);
    float diff = max(dot(n, normalize(-uLightDir)), 0.0);
    vec3 base = uHasTexture ? texture(uDiffuseTex, vUv).rgb : uDiffuseColor;
    vec3 color = base * (0.4 + 0.6 * diff);
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
        std::fprintf(stderr, "[NifMeshRenderer] Shader-Kompilierfehler: %s\n", log);
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
        std::fprintf(stderr, "[NifMeshRenderer] Programm-Linkfehler: %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// Berechnet pro Vertex gemittelte Normalen aus den Dreiecken, falls das Mesh keine eigenen
// Normalen mitbringt (in den bisher erfolgreich geladenen Dateien immer vorhanden, aber
// core::NifModel erlaubt leere normals[] - hier defensiv abgefangen).
std::vector<core::NifVec3> ComputeFallbackNormals(const core::NifMeshPart& part) {
    std::vector<core::NifVec3> normals(part.positions.size(), core::NifVec3{0.0f, 0.0f, 0.0f});
    for (std::size_t i = 0; i + 2 < part.triangleIndices.size(); i += 3) {
        const auto ia = part.triangleIndices[i];
        const auto ib = part.triangleIndices[i + 1];
        const auto ic = part.triangleIndices[i + 2];
        if (ia >= part.positions.size() || ib >= part.positions.size() || ic >= part.positions.size()) continue;
        const auto& a = part.positions[ia];
        const auto& b = part.positions[ib];
        const auto& c = part.positions[ic];
        const core::NifVec3 e1{b.x - a.x, b.y - a.y, b.z - a.z};
        const core::NifVec3 e2{c.x - a.x, c.y - a.y, c.z - a.z};
        const core::NifVec3 n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
        for (auto idx : {ia, ib, ic}) {
            normals[idx].x += n.x;
            normals[idx].y += n.y;
            normals[idx].z += n.z;
        }
    }
    for (auto& n : normals) {
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 1e-6f) {
            n.x /= len; n.y /= len; n.z /= len;
        } else {
            n = {0.0f, 1.0f, 0.0f};
        }
    }
    return normals;
}

} // namespace

NifMeshRenderer::~NifMeshRenderer() { Shutdown(); }

void NifMeshRenderer::Init() {
    const std::uint32_t vs = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    const std::uint32_t fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    shaderProgram_ = LinkProgram(vs, fs);
}

void NifMeshRenderer::ReleaseModel(LoadedModel& model) {
    for (auto& sub : model.subMeshes) {
        if (sub.vbo) glDeleteBuffers(1, &sub.vbo);
        if (sub.ebo) glDeleteBuffers(1, &sub.ebo);
        if (sub.vao) glDeleteVertexArrays(1, &sub.vao);
        // sub.diffuseTex wird NICHT hier gelöscht - Texturen leben im textureCache_ und werden
        // ggf. von mehreren SubMeshes/Modellen geteilt, siehe GetOrLoadTexture.
    }
    model.subMeshes.clear();
}

void NifMeshRenderer::Shutdown() {
    for (auto& [path, model] : modelCache_) {
        ReleaseModel(model);
    }
    modelCache_.clear();
    for (auto& [path, tex] : textureCache_) {
        if (tex) glDeleteTextures(1, &tex);
    }
    textureCache_.clear();
    perObjectModel_.clear();
    if (shaderProgram_) glDeleteProgram(shaderProgram_);
    shaderProgram_ = 0;
}

std::uint32_t NifMeshRenderer::GetOrLoadTexture(const std::filesystem::path& resolvedPath) {
    const std::string key = resolvedPath.string();
    auto it = textureCache_.find(key);
    if (it != textureCache_.end()) {
        return it->second;
    }

    std::uint32_t tex = 0;
    auto ddsResult = core::LoadDdsImage(resolvedPath);
    if (ddsResult) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<int>(ddsResult->width), static_cast<int>(ddsResult->height),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, ddsResult->rgba.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else {
        std::fprintf(stderr, "[NifMeshRenderer] Objekt-Textur nicht ladbar (%s): %s\n",
                      resolvedPath.string().c_str(), ddsResult.error().c_str());
    }
    textureCache_.emplace(key, tex);
    return tex;
}

std::uint32_t NifMeshRenderer::GetOrLoadEmbeddedTexture(const core::NifEmbeddedTexture& image,
                                                             const std::string& cacheKey) {
    auto it = textureCache_.find(cacheKey);
    if (it != textureCache_.end()) return it->second;

    std::uint32_t tex = 0;
    if (image.width == 0 || image.height == 0 ||
        image.rgba.size() != static_cast<std::size_t>(image.width) * image.height * 4) {
        std::fprintf(stderr, "[NifMeshRenderer] Ungültige eingebettete Textur: %s\n", cacheKey.c_str());
        textureCache_.emplace(cacheKey, 0);
        return 0;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<int>(image.width), static_cast<int>(image.height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    textureCache_.emplace(cacheKey, tex);
    return tex;
}

void NifMeshRenderer::LoadModelsForSet(const core::ObjectPlacementSet& set, const std::filesystem::path& mapDir) {
    for (auto& [path, model] : modelCache_) {
        ReleaseModel(model);
    }
    modelCache_.clear();
    // Textur-Cache bewusst NICHT geleert - Texturen sind unabhängig vom Modell-Cache gültig
    // und werden oft von Modellen auf verschiedenen Karten wiederverwendet (z.B. "grass.dds").
    perObjectModel_.assign(set.Count(), nullptr);

    for (std::size_t i = 0; i < set.Count(); ++i) {
        const auto& obj = set.At(i);
        if (obj.modelPath.empty()) continue;

        auto resolved = core::legacy::ResolveLegacyAssetPath(mapDir, obj.modelPath);
        if (!resolved) continue;
        const std::string key = resolved->string();

        auto it = modelCache_.find(key);
        if (it == modelCache_.end()) {
            auto nifResult = core::LoadNifMesh(*resolved);
            LoadedModel model;
            if (nifResult) {
                model.subMeshes.reserve(nifResult->parts.size());
                for (const auto& part : nifResult->parts) {
                    if (part.positions.empty() || part.triangleIndices.empty()) continue;

                    const std::vector<core::NifVec3>& normals =
                        part.normals.size() == part.positions.size() ? part.normals : ComputeFallbackNormals(part);
                    const bool hasUvs = part.uvs.size() == part.positions.size();

                    std::vector<float> vertexData;
                    vertexData.reserve(part.positions.size() * 8);
                    for (std::size_t v = 0; v < part.positions.size(); ++v) {
                        const float u = hasUvs ? part.uvs[v].u : 0.0f;
                        const float vv = hasUvs ? part.uvs[v].v : 0.0f;
                        vertexData.insert(vertexData.end(), {
                            part.positions[v].x, part.positions[v].y, part.positions[v].z,
                            normals[v].x, normals[v].y, normals[v].z,
                            u, vv,
                        });
                    }

                    SubMesh sub;
                    sub.diffuseColor = {part.material.diffuse[0], part.material.diffuse[1], part.material.diffuse[2]};
                    sub.indexCount = static_cast<std::uint32_t>(part.triangleIndices.size());

                    // Textur nur verwenden, wenn das Mesh sowohl eine Dateireferenz als auch
                    // eigene UV-Koordinaten hat - ohne UVs würde die Textur nur einen einzelnen
                    // Farbpunkt (0,0) auf die ganze Fläche legen.
                    if (hasUvs && !part.diffuseTexture.empty()) {
                        if (part.embeddedDiffuseTexture) {
                            // Use External=0: der Dateiname in NiSourceTexture ist nur die
                            // ursprüngliche Bezeichnung. Die tatsächlichen Texel kommen aus
                            // dem referenzierten NiPixelData innerhalb derselben NIF.
                            const std::string cacheKey = key + "#embedded:" + part.diffuseTexture;
                            sub.diffuseTex = GetOrLoadEmbeddedTexture(*part.embeddedDiffuseTexture, cacheKey);
                        } else {
                            // Use External=1 bzw. Fallback für ältere/teilweise bekannte NIFs.
                            auto texPath = core::legacy::ResolveLegacyAssetPath(mapDir, part.diffuseTexture);
                            if (texPath) {
                                sub.diffuseTex = GetOrLoadTexture(*texPath);
                            } else {
                                std::fprintf(stderr, "[NifMeshRenderer] Objekt-Textur nicht gefunden: %s\n",
                                              part.diffuseTexture.c_str());
                            }
                        }
                    }

                    glGenVertexArrays(1, &sub.vao);
                    glBindVertexArray(sub.vao);

                    glGenBuffers(1, &sub.vbo);
                    glBindBuffer(GL_ARRAY_BUFFER, sub.vbo);
                    glBufferData(GL_ARRAY_BUFFER, static_cast<long>(vertexData.size() * sizeof(float)), vertexData.data(), GL_STATIC_DRAW);
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(0));
                    glEnableVertexAttribArray(1);
                    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
                    glEnableVertexAttribArray(2);
                    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));

                    glGenBuffers(1, &sub.ebo);
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sub.ebo);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<long>(part.triangleIndices.size() * sizeof(std::uint32_t)),
                                 part.triangleIndices.data(), GL_STATIC_DRAW);

                    glBindVertexArray(0);
                    model.subMeshes.push_back(sub);
                }
            }
            it = modelCache_.emplace(key, std::move(model)).first;
        }

        if (!it->second.subMeshes.empty()) {
            perObjectModel_[i] = &it->second;
        }
    }
}

bool NifMeshRenderer::HasRealMesh(std::size_t objectIndex) const {
    return objectIndex < perObjectModel_.size() && perObjectModel_[objectIndex] != nullptr;
}

std::size_t NifMeshRenderer::RealMeshCount() const {
    std::size_t count = 0;
    for (const auto* m : perObjectModel_) {
        if (m != nullptr) ++count;
    }
    return count;
}

namespace {
Mat4 QuatToMat4Local(float x, float y, float z, float w) {
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

void NifMeshRenderer::Draw(const core::ObjectPlacementSet& set, const OrbitCamera& camera, int width, int height) {
    if (width <= 0 || height <= 0 || perObjectModel_.empty()) return;

    glUseProgram(shaderProgram_);
    const Mat4 view = camera.ViewMatrix();
    const Mat4 proj = OrbitCamera::PerspectiveMatrix(0.9f, static_cast<float>(width) / static_cast<float>(height), 10.0f, 50000.0f);
    const Mat4 viewProj = proj * view;
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram_, "uViewProj"), 1, GL_FALSE, viewProj.m);
    glUniform3f(glGetUniformLocation(shaderProgram_, "uLightDir"), -0.4f, -1.0f, -0.3f);
    glUniform1i(glGetUniformLocation(shaderProgram_, "uDiffuseTex"), 0);

    for (std::size_t i = 0; i < perObjectModel_.size() && i < set.Count(); ++i) {
        const LoadedModel* model = perObjectModel_[i];
        if (model == nullptr) continue;

        const auto& obj = set.At(i);
        Mat4 modelMat = QuatToMat4Local(obj.rotX, obj.rotY, obj.rotZ, obj.rotW);
        for (int col = 0; col < 3; ++col) {
            modelMat.m[col * 4 + 0] *= obj.scale;
            modelMat.m[col * 4 + 1] *= obj.scale;
            modelMat.m[col * 4 + 2] *= obj.scale;
        }
        modelMat.m[12] = obj.posX;
        modelMat.m[13] = obj.posY;
        modelMat.m[14] = obj.posZ;
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram_, "uModel"), 1, GL_FALSE, modelMat.m);

        for (const auto& sub : model->subMeshes) {
            const bool hasTexture = sub.diffuseTex != 0;
            glUniform1i(glGetUniformLocation(shaderProgram_, "uHasTexture"), hasTexture ? 1 : 0);
            if (hasTexture) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sub.diffuseTex);
            } else {
                glUniform3f(glGetUniformLocation(shaderProgram_, "uDiffuseColor"), sub.diffuseColor[0], sub.diffuseColor[1], sub.diffuseColor[2]);
            }
            glBindVertexArray(sub.vao);
            glDrawElements(GL_TRIANGLES, static_cast<int>(sub.indexCount), GL_UNSIGNED_INT, nullptr);
        }
    }
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace theseed::mapeditor::app
