#include "mapeditor/core/AvatarPreview.hpp"

#include "mapeditor/core/DdsImage.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <unordered_map>

namespace theseed::mapeditor::core {
namespace {

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Groesse-/Schreibweisen-unabhaengige Dateisuche in EINEM Ordner (Windows ist es, die Zip-Daten
// mischen "Body.DDS"/"body.dds").
class DirIndex {
public:
    explicit DirIndex(const std::filesystem::path& dir) : dir_(dir) {
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(dir, ec)) files_[Lower(e.path().filename().string())] = e.path();
    }
    [[nodiscard]] bool Valid() const { return !files_.empty(); }
    [[nodiscard]] std::filesystem::path Find(const std::string& name) const {
        const auto it = files_.find(Lower(name));
        return it == files_.end() ? std::filesystem::path() : it->second;
    }
    // Textur zu einem Namen mit/ohne Endung: probiert .dds
    [[nodiscard]] std::filesystem::path FindTexture(std::string name) const {
        if (name.empty() || name == "-") return {};
        if (auto p = Find(name); !p.empty()) return p;
        const auto dot = name.rfind('.');
        if (dot != std::string::npos) name.resize(dot);
        return Find(name + ".dds");
    }
private:
    std::filesystem::path dir_;
    std::unordered_map<std::string, std::filesystem::path> files_;
};

std::shared_ptr<const AvatarTexture> LoadTex(const std::filesystem::path& file, std::unordered_map<std::string, std::shared_ptr<const AvatarTexture>>& cache) {
    if (file.empty()) return nullptr;
    const std::string key = file.string();
    if (const auto it = cache.find(key); it != cache.end()) return it->second;
    std::shared_ptr<const AvatarTexture> out;
    if (auto img = LoadDdsImage(file)) {
        auto t = std::make_shared<AvatarTexture>();
        t->width = img->width; t->height = img->height; t->rgba = std::move(img->rgba);
        out = t;
    }
    cache[key] = out;
    return out;
}

// Slot-Erkennung der Koerper-Teile an der Hoehe des SCHWERPUNKTS (Bindepose, Y nach oben, Figur ~50
// Einheiten hoch): Schuhe ~5, Beine ~21, Koerper ~36. (Die Obergrenze der Schuhe schwankt: Stiefel
// reichen bis 14.4 - der Schwerpunkt ist robuster als die Oberkante.)
enum class Slot { Shoes, Legs, Body, Other };
Slot ClassifyByHeight(const NifMeshPart& part) {
    if (part.positions.empty()) return Slot::Other;
    double sum = 0.0;
    for (const auto& v : part.positions) sum += v.y;
    const double centroid = sum / static_cast<double>(part.positions.size());
    if (centroid < 12.0) return Slot::Shoes;
    if (centroid < 29.0) return Slot::Legs;
    return Slot::Body;
}

AvatarPart ToPart(const NifMeshPart& src, const std::string& label, float dx = 0.0f, float dy = 0.0f, float dz = 0.0f, float scale = 1.0f) {
    AvatarPart p;
    p.label = label;
    p.positions.reserve(src.positions.size());
    for (const auto& v : src.positions) p.positions.push_back({v.x * scale + dx, v.y * scale + dy, v.z * scale + dz});
    p.normals = src.normals;
    p.uvs = src.uvs;
    p.triangleIndices = src.triangleIndices;
    return p;
}

// Waehlt je Textur den Teil mit den meisten Vertices (hoechste Detailstufe); ungetexturierte Teile
// (Knochen-Huellkoerper) werden verworfen.
std::vector<const NifMeshPart*> PickHighestDetail(const NifModel& model) {
    std::map<std::string, const NifMeshPart*> best;
    for (const auto& part : model.parts) {
        if (part.diffuseTexture.empty() || part.triangleIndices.empty()) continue;
        const std::string key = Lower(part.diffuseTexture);
        const auto it = best.find(key);
        if (it == best.end() || part.positions.size() > it->second->positions.size()) best[key] = &part;
    }
    std::vector<const NifMeshPart*> out;
    for (const auto& [k, v] : best) out.push_back(v);
    return out;
}

// Teil im lokalen Raum eines Knochens (Vertices im Editor-Rahmen) an dessen Weltposition setzen:
// Editor -> Legacy tauschen, mit der Weltrotation des Knotens (zeilenweise, Legacy-Rahmen) drehen, zurueck
// tauschen, verschieben. `rotation == nullptr`: nur verschieben/skalieren.
AvatarPart ToPartBone(const NifMeshPart& src, const std::string& label, const std::array<float, 9>* rotation,
                      float dx, float dy, float dz, float scale = 1.0f) {
    AvatarPart p;
    p.label = label;
    auto xf = [&](const NifVec3& v, bool point) -> NifVec3 {
        float x = v.x, y = v.z, z = v.y; // Editor (x,y_up,z) -> Legacy (x,y,z_up)
        if (rotation != nullptr) {
            const auto& R = *rotation;
            const float nx = R[0] * x + R[1] * y + R[2] * z, ny = R[3] * x + R[4] * y + R[5] * z, nz = R[6] * x + R[7] * y + R[8] * z;
            x = nx; y = ny; z = nz;
        }
        NifVec3 o{x, z, y}; // Legacy -> Editor
        if (point) { o.x = o.x * scale + dx; o.y = o.y * scale + dy; o.z = o.z * scale + dz; }
        return o;
    };
    p.positions.reserve(src.positions.size());
    for (const auto& v : src.positions) p.positions.push_back(xf(v, true));
    for (const auto& n : src.normals) p.normals.push_back(xf(n, false));
    p.uvs = src.uvs;
    p.triangleIndices = src.triangleIndices;
    return p;
}

const NifNodeInfo* FindNode(const NifModel& model, const std::string& name) {
    for (const auto& n : model.nodes) if (n.name == name) return &n;
    return nullptr;
}

} // namespace

std::string AvatarClassFolder(int classIdx) {
    switch (classIdx) {
        case 0: return "Fighter";
        case 2: return "Cleric";
        case 3: return "Mage";
        case 4: return "Joker";
        case 5: return "Sentinel";
        default: return {};
    }
}

std::expected<AvatarModel, std::string> BuildAvatarModel(const AvatarRequest& req) {
    const std::string cls = AvatarClassFolder(req.classIdx);
    if (cls.empty()) return std::unexpected("Für diese Klasse gibt es keine Charaktermodelle in reschar (Archer fehlt).");
    const std::string folderName = cls + (req.male ? "-m" : "-f");
    const std::filesystem::path dir = req.charRoot / folderName;
    DirIndex index(dir);
    if (!index.Valid()) return std::unexpected("Ordner nicht gefunden: " + dir.string());
    const auto bodyFile = index.Find(folderName + ".nif");
    if (bodyFile.empty()) return std::unexpected(folderName + ".nif fehlt");
    auto body = LoadNifMesh(bodyFile);
    if (!body) return std::unexpected(folderName + ".nif: " + body.error());

    AvatarModel model;
    std::unordered_map<std::string, std::shared_ptr<const AvatarTexture>> texCache;

    // ---- Koerper-Teile je Slot: Basis, dann ggf. durch Set-Geometrie ersetzt
    struct SlotPart { NifMeshPart part; std::string textureName; };
    std::map<int, SlotPart> slots; // Slot -> Teil
    for (const NifMeshPart* p : PickHighestDetail(*body)) {
        const Slot s = ClassifyByHeight(*p);
        if (s == Slot::Other) continue;
        slots[static_cast<int>(s)] = {*p, p->diffuseTexture};
    }
    auto slotOfItem = [](const std::string& slot) { return slot == "Body" ? Slot::Body : slot == "Leg" ? Slot::Legs : slot == "Shoes" ? Slot::Shoes : Slot::Other; };
    for (const auto& item : req.items) {
        const Slot s = slotOfItem(item.slot);
        if (s == Slot::Other) continue;
        SlotPart& target = slots[static_cast<int>(s)];
        if (item.setNo > 0) {
            char setName[32];
            std::snprintf(setName, sizeof(setName), "set%03d.nif", item.setNo);
            if (const auto setFile = index.Find(setName); !setFile.empty()) {
                if (auto setModel = LoadNifMesh(setFile)) {
                    for (const NifMeshPart* p : PickHighestDetail(*setModel)) {
                        if (ClassifyByHeight(*p) == s) { target.part = *p; target.textureName = p->diffuseTexture; break; }
                    }
                } else model.notes.push_back(std::string(setName) + ": " + setModel.error());
            } else model.notes.push_back(std::string(setName) + " nicht vorhanden - Standard-Körperteil");
        }
        if (!item.textureFile.empty() && item.textureFile != "-") target.textureName = item.textureFile;
    }
    for (auto& [slotId, sp] : slots) {
        AvatarPart part = ToPart(sp.part, slotId == static_cast<int>(Slot::Body) ? "Körper" : slotId == static_cast<int>(Slot::Legs) ? "Beine" : "Schuhe");
        part.texture = LoadTex(index.FindTexture(sp.textureName), texCache);
        if (!part.texture) {
            // Rueckfall: Textur des Standardteils
            part.texture = LoadTex(index.FindTexture(sp.part.diffuseTexture), texCache);
        }
        if (!part.texture) { part.color = {0.86f, 0.72f, 0.62f}; model.notes.push_back("Textur '" + sp.textureName + "' nicht gefunden (Hautfarbe als Ersatz)"); }
        model.parts.push_back(std::move(part));
    }

    // ---- Kopf: Gesicht + Augen am Knoten "Bip01 Head"
    float hx = 0.0f, hy = 45.0f, hz = 0.0f;
    if (const NifNodeInfo* head = FindNode(*body, "Bip01 Head")) { hx = head->position.x; hy = head->position.y; hz = head->position.z; }
    {
        char faceName[32];
        std::snprintf(faceName, sizeof(faceName), "Face%03d.nif", std::max(1, req.faceShape));
        std::filesystem::path faceFile = index.Find(faceName);
        if (faceFile.empty()) faceFile = index.Find("Face001.nif");
        if (!faceFile.empty()) {
            if (auto face = LoadNifMesh(faceFile)) {
                for (const auto& p : face->parts) {
                    if (p.triangleIndices.empty()) continue;
                    // KEINE Knochenrotation: Gesicht/Haare sind bereits im Charakter-Weltrahmen modelliert
                    // (Augen weiter vorn bei -z und oberhalb der Haut) - nur an den Kopfknochen verschieben.
                    AvatarPart part = ToPartBone(p, "Gesicht", nullptr, hx, hy, hz);
                    part.texture = LoadTex(index.FindTexture(p.diffuseTexture), texCache);
                    if (!part.texture) {
                        // Haut-Teil ohne auffindbare Textur ("1.tga"): Standard-Gesichtstextur der Klasse
                        char skin[64];
                        std::snprintf(skin, sizeof(skin), "%s-face001.dds", folderName.c_str());
                        part.texture = LoadTex(index.FindTexture(skin), texCache);
                    }
                    if (!part.texture) part.color = {0.93f, 0.78f, 0.68f};
                    model.parts.push_back(std::move(part));
                }
            } else model.notes.push_back(std::string(faceName) + ": " + face.error());
        } else model.notes.push_back("Kein Face%03d.nif für dieses Gesicht");
    }

    // ---- Haare
    for (const std::string& hairName : {req.hairFront, req.hairBottom, req.hairTop}) {
        if (hairName.empty() || hairName == "-") continue;
        std::string name = hairName;
        std::filesystem::path hairFile = index.Find(name + ".nif");
        if (hairFile.empty() && name.size() > 3 && name[0] == '_' && name[2] == '_') hairFile = index.Find(name.substr(3) + ".nif");
        if (hairFile.empty()) continue;
        if (auto hair = LoadNifMesh(hairFile)) {
            for (const auto& p : hair->parts) {
                if (p.triangleIndices.empty()) continue;
                AvatarPart part = ToPartBone(p, "Haare", nullptr, hx, hy, hz);
                std::string texName = req.hairTexture.empty() ? p.diffuseTexture : req.hairTexture;
                part.texture = LoadTex(index.FindTexture(texName), texCache);
                if (!part.texture && texName.size() > 3 && texName[0] == '_' && texName[2] == '_') part.texture = LoadTex(index.FindTexture(texName.substr(3)), texCache);
                if (!part.texture) part.texture = LoadTex(index.FindTexture(p.diffuseTexture), texCache);
                if (!part.texture) part.color = {0.35f, 0.22f, 0.12f};
                model.parts.push_back(std::move(part));
            }
        }
    }

    // ---- Waffen/Schilde (Handposition geschaetzt: Bindepose = Arme seitlich ausgestreckt)
    if (!req.itemRoot.empty()) {
        DirIndex itemIndex(req.itemRoot);
        for (const auto& item : req.items) {
            const bool right = item.slot == "RightHand", left = item.slot == "LeftHand";
            if ((!right && !left) || item.linkFile.empty() || item.linkFile == "-") continue;
            const auto file = itemIndex.Find(item.linkFile + ".nif");
            if (file.empty()) { model.notes.push_back(item.linkFile + ".nif nicht in resitem gefunden"); continue; }
            auto weapon = LoadNifMesh(file);
            if (!weapon) { model.notes.push_back(item.linkFile + ".nif: " + weapon.error()); continue; }
            // Hand = Aussenkante des Koerpers auf Handhoehe; Rechts = negatives X (Knoten "Bip01 R Hand").
            float handX = right ? -16.0f : 16.0f, handY = 27.5f, handZ = 0.0f;
            std::array<float, 9> handRot = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            bool haveHandRot = false;
            if (const NifNodeInfo* h = FindNode(*body, right ? "Bip01 R Hand" : "Bip01 L Hand")) { handZ = h->position.z; handRot = h->rotation; haveHandRot = true; }
            for (const auto& p : weapon->parts) {
                if (p.triangleIndices.empty()) continue;
                AvatarPart part = ToPartBone(p, right ? "Rechte Hand" : "Linke Hand", haveHandRot ? &handRot : nullptr, handX, handY, handZ, 0.55f);
                part.texture = nullptr;
                part.color = {0.78f, 0.78f, 0.84f};
                if (p.embeddedDiffuseTexture) {
                    auto t = std::make_shared<AvatarTexture>();
                    t->width = p.embeddedDiffuseTexture->width; t->height = p.embeddedDiffuseTexture->height; t->rgba = p.embeddedDiffuseTexture->rgba;
                    part.texture = t;
                }
                model.parts.push_back(std::move(part));
            }
        }
    }
    if (model.parts.empty()) return std::unexpected("Keine darstellbaren Teile");
    return model;
}

void SimplifyCharacterModel(NifModel& model) {
    bool anyTextured = false;
    for (const auto& p : model.parts) if (!p.diffuseTexture.empty() && !p.triangleIndices.empty()) anyTextured = true;
    if (!anyTextured) return;
    struct Box { float mn[3], mx[3]; };
    auto boxOf = [](const NifMeshPart& p) {
        Box b{{1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, -1e30f}};
        for (const auto& v : p.positions) {
            b.mn[0] = std::min(b.mn[0], v.x); b.mx[0] = std::max(b.mx[0], v.x);
            b.mn[1] = std::min(b.mn[1], v.y); b.mx[1] = std::max(b.mx[1], v.y);
            b.mn[2] = std::min(b.mn[2], v.z); b.mx[2] = std::max(b.mx[2], v.z);
        }
        return b;
    };
    // Ueberlappung = Schnittvolumen / kleineres Volumen (mit Mindestdicke, damit flache Teile zaehlen).
    auto overlap = [](const Box& a, const Box& b) {
        float inter = 1.0f, va = 1.0f, vb = 1.0f;
        for (int i = 0; i < 3; ++i) {
            const float lo = std::max(a.mn[i], b.mn[i]), hi = std::min(a.mx[i], b.mx[i]);
            if (hi <= lo) return 0.0f;
            inter *= std::max(hi - lo, 0.5f);
            va *= std::max(a.mx[i] - a.mn[i], 0.5f);
            vb *= std::max(b.mx[i] - b.mn[i], 0.5f);
        }
        return inter / std::min(va, vb);
    };
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        if (model.parts[i].diffuseTexture.empty() || model.parts[i].triangleIndices.empty()) continue;
        order.push_back(i);
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return model.parts[a].positions.size() > model.parts[b].positions.size(); });
    std::vector<std::size_t> keptIdx;
    std::vector<Box> keptBox;
    for (const std::size_t i : order) {
        const Box b = boxOf(model.parts[i]);
        bool duplicate = false;
        for (std::size_t k = 0; k < keptIdx.size(); ++k) {
            if (Lower(model.parts[keptIdx[k]].diffuseTexture) == Lower(model.parts[i].diffuseTexture) && overlap(keptBox[k], b) > 0.6f) { duplicate = true; break; }
        }
        if (!duplicate) { keptIdx.push_back(i); keptBox.push_back(b); }
    }
    std::sort(keptIdx.begin(), keptIdx.end());
    std::vector<NifMeshPart> out;
    out.reserve(keptIdx.size());
    for (const std::size_t i : keptIdx) out.push_back(std::move(model.parts[i]));
    model.parts = std::move(out);
}

NifModel AvatarToNifModel(const AvatarModel& model) {
    NifModel out;
    out.rootName = "avatar";
    for (const auto& p : model.parts) {
        NifMeshPart part;
        part.name = p.label;
        part.positions = p.positions;
        part.normals = p.normals;
        part.uvs = p.uvs;
        part.triangleIndices = p.triangleIndices;
        part.material.diffuse[0] = p.color[0];
        part.material.diffuse[1] = p.color[1];
        part.material.diffuse[2] = p.color[2];
        if (p.texture && p.texture->width > 0) {
            part.diffuseTexture = "avatar:" + p.label;
            auto tex = std::make_shared<NifEmbeddedTexture>();
            tex->width = p.texture->width;
            tex->height = p.texture->height;
            tex->rgba = p.texture->rgba;
            part.embeddedDiffuseTexture = tex;
        }
        out.parts.push_back(std::move(part));
    }
    return out;
}

std::vector<std::uint8_t> RenderAvatarModel(const AvatarModel& model, float yaw, int width, int height) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(width) * height * 4, 0);
    for (std::size_t i = 0; i < out.size(); i += 4) { out[i] = 24; out[i + 1] = 28; out[i + 2] = 36; out[i + 3] = 255; }
    if (model.parts.empty() || width < 8 || height < 8) return out;
    float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
    for (const auto& p : model.parts) for (const auto& v : p.positions) {
        mn[0] = std::min(mn[0], v.x); mx[0] = std::max(mx[0], v.x); mn[1] = std::min(mn[1], v.y); mx[1] = std::max(mx[1], v.y); mn[2] = std::min(mn[2], v.z); mx[2] = std::max(mx[2], v.z);
    }
    const float cx = (mn[0] + mx[0]) * 0.5f, cz = (mn[2] + mx[2]) * 0.5f;
    // Skalierung so, dass Hoehe UND Breite (Arme, Waffen) ins Bild passen; Drehung um die Hochachse
    // braucht den groesseren der beiden horizontalen Ausdehnungen.
    const float horiz = std::max(mx[0] - mn[0], mx[2] - mn[2]);
    const float scale = std::min((static_cast<float>(height) - 24.0f) / std::max(1.0f, mx[1] - mn[1]),
                                 (static_cast<float>(width) - 16.0f) / std::max(1.0f, horiz * 1.05f));
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    std::vector<float> zbuf(static_cast<std::size_t>(width) * height, 1e30f);
    for (const auto& p : model.parts) {
        const std::size_t n = p.positions.size();
        std::vector<float> sx(n), sy2(n), sz(n);
        for (std::size_t i = 0; i < n; ++i) {
            const float x = p.positions[i].x - cx, z = p.positions[i].z - cz;
            const float rx = x * cy + z * sy, rz = -x * sy + z * cy;
            sx[i] = rx * scale + static_cast<float>(width) * 0.5f;
            sy2[i] = static_cast<float>(height) - 12.0f - (p.positions[i].y - mn[1]) * scale;
            sz[i] = rz;
        }
        for (std::size_t t = 0; t + 2 < p.triangleIndices.size(); t += 3) {
            const auto a = p.triangleIndices[t], b = p.triangleIndices[t + 1], c = p.triangleIndices[t + 2];
            if (a >= n || b >= n || c >= n) continue;
            // Flaechennormale (Zweiseitig) fuer einfache Beleuchtung
            const float ux = p.positions[b].x - p.positions[a].x, uy = p.positions[b].y - p.positions[a].y, uz = p.positions[b].z - p.positions[a].z;
            const float vx = p.positions[c].x - p.positions[a].x, vy = p.positions[c].y - p.positions[a].y, vz = p.positions[c].z - p.positions[a].z;
            float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len < 1e-9f) continue;
            nx /= len; ny /= len; nz /= len;
            const float rnz = -nx * sy + nz * cy; // Normale in Kamerarichtung
            const float light = 0.55f + 0.45f * std::fabs(rnz);
            const float minx = std::min({sx[a], sx[b], sx[c]}), maxx = std::max({sx[a], sx[b], sx[c]});
            const float miny = std::min({sy2[a], sy2[b], sy2[c]}), maxy = std::max({sy2[a], sy2[b], sy2[c]});
            const int x0 = std::max(0, static_cast<int>(std::floor(minx))), x1 = std::min(width - 1, static_cast<int>(std::ceil(maxx)));
            const int y0 = std::max(0, static_cast<int>(std::floor(miny))), y1 = std::min(height - 1, static_cast<int>(std::ceil(maxy)));
            const float den = (sy2[b] - sy2[c]) * (sx[a] - sx[c]) + (sx[c] - sx[b]) * (sy2[a] - sy2[c]);
            if (std::fabs(den) < 1e-9f) continue;
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    const float px = static_cast<float>(x) + 0.5f, py = static_cast<float>(y) + 0.5f;
                    const float l1 = ((sy2[b] - sy2[c]) * (px - sx[c]) + (sx[c] - sx[b]) * (py - sy2[c])) / den;
                    const float l2 = ((sy2[c] - sy2[a]) * (px - sx[c]) + (sx[a] - sx[c]) * (py - sy2[c])) / den;
                    const float l3 = 1.0f - l1 - l2;
                    if (l1 < -0.001f || l2 < -0.001f || l3 < -0.001f) continue;
                    const float z = l1 * sz[a] + l2 * sz[b] + l3 * sz[c];
                    float r = p.color[0], g = p.color[1], bl = p.color[2];
                    if (p.texture && p.uvs.size() == n) {
                        const float u = l1 * p.uvs[a].u + l2 * p.uvs[b].u + l3 * p.uvs[c].u;
                        const float v = l1 * p.uvs[a].v + l2 * p.uvs[b].v + l3 * p.uvs[c].v;
                        // LoadDdsImage liefert die Zeilen in OpenGL-Ausrichtung (unterste Zeile zuerst) - NIF-UVs
                        // zaehlen dagegen von oben (D3D): Zeile = (1 - v) * Hoehe. (Ohne das stand das Gesicht
                        // kopfueber, CHANGELOG [0.44.30].)
                        const float fu = u - std::floor(u), fv = 1.0f - (v - std::floor(v));
                        const auto tx = std::min<std::uint32_t>(p.texture->width - 1, static_cast<std::uint32_t>(fu * static_cast<float>(p.texture->width)));
                        const auto ty = std::min<std::uint32_t>(p.texture->height - 1, static_cast<std::uint32_t>(fv * static_cast<float>(p.texture->height)));
                        const std::uint8_t* tp = &p.texture->rgba[(static_cast<std::size_t>(ty) * p.texture->width + tx) * 4];
                        if (tp[3] < 96) continue; // Alpha-Test (Haare, Wimpern)
                        r = static_cast<float>(tp[0]) / 255.0f; g = static_cast<float>(tp[1]) / 255.0f; bl = static_cast<float>(tp[2]) / 255.0f;
                    }
                    const std::size_t zi = static_cast<std::size_t>(y) * width + x;
                    if (z >= zbuf[zi]) continue;
                    zbuf[zi] = z;
                    std::uint8_t* o = &out[zi * 4];
                    o[0] = static_cast<std::uint8_t>(std::clamp(r * light, 0.0f, 1.0f) * 255.0f);
                    o[1] = static_cast<std::uint8_t>(std::clamp(g * light, 0.0f, 1.0f) * 255.0f);
                    o[2] = static_cast<std::uint8_t>(std::clamp(bl * light, 0.0f, 1.0f) * 255.0f);
                }
            }
        }
    }
    return out;
}

} // namespace theseed::mapeditor::core
