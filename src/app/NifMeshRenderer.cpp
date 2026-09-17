#include "NifMeshRenderer.hpp"

#include "mapeditor/core/DdsImage.hpp"
#include "mapeditor/core/legacy/LegacyPathResolve.hpp"

#include <glad/glad.h>

#include <cmath>
#include <cstdio>

namespace theseed::mapeditor::app {

namespace {

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
    FragColor = vec4(base * (0.4 + 0.6 * diff), 1.0);
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

std::vector<core::NifVec3> ComputeFallbackNormals(const core::NifMeshPart& part) {
    std::vector<core::NifVec3> normals(part.positions.size(), {0.0f, 0.0f, 0.0f});
    for (std::size_t i = 0; i + 2 < part.triangleIndices.size(); i += 3) {
        const auto ia = part.triangleIndices[i], ib = part.triangleIndices[i + 1], ic = part.triangleIndices[i + 2];
        if (ia >= part.positions.size() || ib >= part.positions.size() || ic >= part.positions.size()) continue;
        const auto& a = part.positions[ia]; const auto& b = part.positions[ib]; const auto& c = part.positions[ic];
        const core::NifVec3 e1{b.x-a.x,b.y-a.y,b.z-a.z};
        const core::NifVec3 e2{c.x-a.x,c.y-a.y,c.z-a.z};
        const core::NifVec3 n{e1.y*e2.z-e1.z*e2.y,e1.z*e2.x-e1.x*e2.z,e1.x*e2.y-e1.y*e2.x};
        for (auto idx : {ia, ib, ic}) { normals[idx].x += n.x; normals[idx].y += n.y; normals[idx].z += n.z; }
    }
    for (auto& n : normals) {
        const float len = std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
        if (len > 1e-6f) { n.x/=len; n.y/=len; n.z/=len; } else n = {0.0f,1.0f,0.0f};
    }
    return normals;
}

} // namespace

NifMeshRenderer::~NifMeshRenderer() { Shutdown(); }

void NifMeshRenderer::Init() {
    shaderProgram_ = LinkProgram(CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc), CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc));
}

void NifMeshRenderer::ReleaseModel(LoadedModel& model) {
    for (auto& sub : model.subMeshes) {
        if (sub.vbo) glDeleteBuffers(1, &sub.vbo);
        if (sub.ebo) glDeleteBuffers(1, &sub.ebo);
        if (sub.vao) glDeleteVertexArrays(1, &sub.vao);
    }
    model.subMeshes.clear();
}

void NifMeshRenderer::Shutdown() {
    for (auto& [path, model] : modelCache_) ReleaseModel(model);
    modelCache_.clear();
    for (auto& [path, tex] : textureCache_) if (tex) glDeleteTextures(1, &tex);
    textureCache_.clear();
    perObjectModel_.clear();
    if (shaderProgram_) glDeleteProgram(shaderProgram_);
    shaderProgram_ = 0;
}

std::uint32_t NifMeshRenderer::GetOrLoadTexture(const std::filesystem::path& resolvedPath) {
    const std::string key = resolvedPath.string();
    if (auto it = textureCache_.find(key); it != textureCache_.end()) return it->second;
    std::uint32_t tex = 0;
    auto result = core::LoadDdsImage(resolvedPath);
    if (result) {
        glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)result->width, (int)result->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, result->rgba.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else std::fprintf(stderr, "[NifMeshRenderer] Objekt-Textur nicht ladbar (%s): %s\n", key.c_str(), result.error().c_str());
    textureCache_.emplace(key, tex); return tex;
}

std::uint32_t NifMeshRenderer::GetOrLoadEmbeddedTexture(const core::NifEmbeddedTexture& image, const std::string& cacheKey) {
    if (auto it = textureCache_.find(cacheKey); it != textureCache_.end()) return it->second;
    if (!image.width || !image.height || image.rgba.size() != (std::size_t)image.width * image.height * 4) {
        std::fprintf(stderr, "[NifMeshRenderer] Ungültige eingebettete Textur: %s\n", cacheKey.c_str()); textureCache_.emplace(cacheKey, 0); return 0;
    }
    std::uint32_t tex = 0; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)image.width, (int)image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    textureCache_.emplace(cacheKey, tex); return tex;
}

void NifMeshRenderer::LoadModelsForSet(const core::ObjectPlacementSet& set, const std::filesystem::path& mapDir) {
    for (auto& [path, model] : modelCache_) ReleaseModel(model);
    modelCache_.clear(); perObjectModel_.assign(set.Count(), nullptr);
    for (std::size_t i = 0; i < set.Count(); ++i) {
        const auto& obj = set.At(i); if (obj.modelPath.empty()) continue;
        auto resolved = core::legacy::ResolveLegacyAssetPath(mapDir, obj.modelPath); if (!resolved) continue;
        const std::string key = resolved->string(); auto it = modelCache_.find(key);
        if (it == modelCache_.end()) {
            LoadedModel model; auto nif = core::LoadNifMesh(*resolved);
            if (nif) for (const auto& part : nif->parts) {
                if (part.positions.empty() || part.triangleIndices.empty()) continue;
                const auto normals = part.normals.size() == part.positions.size() ? part.normals : ComputeFallbackNormals(part);
                const bool hasUvs = part.uvs.size() == part.positions.size(); std::vector<float> data; data.reserve(part.positions.size()*8);
                for (std::size_t v=0; v<part.positions.size(); ++v) { const float u=hasUvs?part.uvs[v].u:0, vv=hasUvs?part.uvs[v].v:0; data.insert(data.end(), {part.positions[v].x,part.positions[v].y,part.positions[v].z,normals[v].x,normals[v].y,normals[v].z,u,vv}); }
                SubMesh sub; sub.diffuseColor={part.material.diffuse[0],part.material.diffuse[1],part.material.diffuse[2]}; sub.indexCount=(std::uint32_t)part.triangleIndices.size();
                if (hasUvs && !part.diffuseTexture.empty()) {
                    if (part.embeddedDiffuseTexture) sub.diffuseTex=GetOrLoadEmbeddedTexture(*part.embeddedDiffuseTexture,key+"#embedded:"+part.diffuseTexture);
                    else if (auto texPath=core::legacy::ResolveLegacyAssetPath(mapDir,part.diffuseTexture)) sub.diffuseTex=GetOrLoadTexture(*texPath);
                }
                glGenVertexArrays(1,&sub.vao); glBindVertexArray(sub.vao); glGenBuffers(1,&sub.vbo); glBindBuffer(GL_ARRAY_BUFFER,sub.vbo); glBufferData(GL_ARRAY_BUFFER,(long)(data.size()*sizeof(float)),data.data(),GL_STATIC_DRAW);
                glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0); glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));
                glGenBuffers(1,&sub.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,sub.ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER,(long)(part.triangleIndices.size()*sizeof(std::uint32_t)),part.triangleIndices.data(),GL_STATIC_DRAW); glBindVertexArray(0); model.subMeshes.push_back(sub);
            }
            it=modelCache_.emplace(key,std::move(model)).first;
        }
        if (!it->second.subMeshes.empty()) perObjectModel_[i]=&it->second;
    }
}

bool NifMeshRenderer::HasRealMesh(std::size_t objectIndex) const { return objectIndex < perObjectModel_.size() && perObjectModel_[objectIndex] != nullptr; }
std::size_t NifMeshRenderer::RealMeshCount() const { std::size_t count=0; for (const auto* m:perObjectModel_) if(m) ++count; return count; }

namespace { Mat4 QuatToMat4Local(float x,float y,float z,float w) { Mat4 m=Mat4::Identity(); m.m[0]=1-2*(y*y+z*z); m.m[1]=2*(x*y+w*z); m.m[2]=2*(x*z-w*y); m.m[4]=2*(x*y-w*z); m.m[5]=1-2*(x*x+z*z); m.m[6]=2*(y*z+w*x); m.m[8]=2*(x*z+w*y); m.m[9]=2*(y*z-w*x); m.m[10]=1-2*(x*x+y*y); return m; } }

void NifMeshRenderer::Draw(const core::ObjectPlacementSet& set, const OrbitCamera& camera, int width, int height) {
    if (width<=0 || height<=0 || perObjectModel_.empty()) return;
    glUseProgram(shaderProgram_); const Mat4 view=camera.ViewMatrix(); const Mat4 proj=OrbitCamera::PerspectiveMatrix(0.9f,(float)width/(float)height,10.0f,50000.0f); const Mat4 viewProj=proj*view;
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram_,"uViewProj"),1,GL_FALSE,viewProj.m); glUniform3f(glGetUniformLocation(shaderProgram_,"uLightDir"),-0.4f,-1.0f,-0.3f); glUniform1i(glGetUniformLocation(shaderProgram_,"uDiffuseTex"),0);
    for (std::size_t i=0;i<perObjectModel_.size() && i<set.Count();++i) { const auto* model=perObjectModel_[i]; if(!model) continue; const auto& obj=set.At(i); Mat4 modelMat=QuatToMat4Local(obj.rotX,obj.rotY,obj.rotZ,obj.rotW); for(int col=0;col<3;++col) for(int row=0;row<3;++row) modelMat.m[col*4+row]*=obj.scale; modelMat.m[12]=obj.posX; modelMat.m[13]=obj.posY; modelMat.m[14]=obj.posZ; glUniformMatrix4fv(glGetUniformLocation(shaderProgram_,"uModel"),1,GL_FALSE,modelMat.m);
        for(const auto& sub:model->subMeshes) { const bool textured=sub.diffuseTex!=0; glUniform1i(glGetUniformLocation(shaderProgram_,"uHasTexture"),textured?1:0); if(textured){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,sub.diffuseTex);} else glUniform3f(glGetUniformLocation(shaderProgram_,"uDiffuseColor"),sub.diffuseColor[0],sub.diffuseColor[1],sub.diffuseColor[2]); glBindVertexArray(sub.vao); glDrawElements(GL_TRIANGLES,(int)sub.indexCount,GL_UNSIGNED_INT,nullptr); }
    }
    glBindVertexArray(0); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,0);
}

} // namespace theseed::mapeditor::app
