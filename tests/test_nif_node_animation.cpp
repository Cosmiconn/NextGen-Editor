// test_nif_node_animation.cpp
// Eingebettete NiTransformController-Animationen: Controller-Zeit, Key-Interpolation,
// Hierarchie und echte Fiesta-Modelle (Leuchtturm, Hund).

#include "mapeditor/core/NifModel.hpp"
#include "mapeditor/core/NifNodeAnimation.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace theseed::mapeditor::core;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "[FEHLER] %s\n", what);
        ++g_failures;
    } else {
        std::printf("[ok]     %s\n", what);
    }
}

bool Near(float a, float b, float eps = 1e-3f) { return std::abs(a - b) <= eps; }

std::array<float, 3> Apply(const std::array<float, 16>& m, float x, float y, float z) {
    return {m[0] * x + m[4] * y + m[8] * z + m[12], m[1] * x + m[5] * y + m[9] * z + m[13], m[2] * x + m[6] * y + m[10] * z + m[14]};
}

bool IsIdentity(const std::array<float, 16>& m, float eps = 1e-3f) {
    for (int i = 0; i < 16; ++i)
        if (!Near(m[static_cast<std::size_t>(i)], (i % 5 == 0) ? 1.0f : 0.0f, eps)) return false;
    return true;
}

// Ähnlichkeitsabbildung: Spalten orthogonal und gleich lang (gleichmäßige Skalierung erlaubt),
// keine Spiegelung.
bool IsRigid(const std::array<float, 16>& m) {
    float len[3];
    for (int a = 0; a < 3; ++a)
        len[a] = std::sqrt(m[static_cast<std::size_t>(a * 4)] * m[static_cast<std::size_t>(a * 4)] + m[static_cast<std::size_t>(a * 4 + 1)] * m[static_cast<std::size_t>(a * 4 + 1)] +
                           m[static_cast<std::size_t>(a * 4 + 2)] * m[static_cast<std::size_t>(a * 4 + 2)]);
    if (len[0] < 1e-4f || !Near(len[1] / len[0], 1.0f, 2e-3f) || !Near(len[2] / len[0], 1.0f, 2e-3f)) return false;
    for (int a = 0; a < 3; ++a) {
        for (int b = a + 1; b < 3; ++b) {
            float dot = 0.0f;
            for (int r = 0; r < 3; ++r) dot += m[static_cast<std::size_t>(a * 4 + r)] * m[static_cast<std::size_t>(b * 4 + r)];
            if (!Near(dot / (len[a] * len[b]), 0.0f, 2e-3f)) return false;
        }
    }
    const float det = m[0] * (m[5] * m[10] - m[9] * m[6]) - m[4] * (m[1] * m[10] - m[9] * m[2]) + m[8] * (m[1] * m[6] - m[5] * m[2]);
    return det > 0.0f;
}

void TestControllerTime() {
    NifNodeTrack t;
    t.startTime = 1.0f;
    t.stopTime = 3.0f;
    t.flags = 0x0008; // Loop
    Check(Near(NifControllerTime(t, 1.5f), 1.5f) && Near(NifControllerTime(t, 3.5f), 1.5f) && Near(NifControllerTime(t, 0.5f), 2.5f),
          "Loop wiederholt [start, stop]");
    t.flags = 0x0008 | (1 << 1); // Reverse
    Check(Near(NifControllerTime(t, 3.5f), 2.5f) && Near(NifControllerTime(t, 5.5f), 1.5f), "Reverse läuft hin und zurück");
    t.flags = 0x0008 | (2 << 1); // Clamp
    Check(Near(NifControllerTime(t, 9.0f), 3.0f) && Near(NifControllerTime(t, -2.0f), 1.0f), "Clamp hält an den Enden");
    t.flags = 0x0008;
    t.frequency = 2.0f;
    t.phase = 0.5f;
    Check(Near(NifControllerTime(t, 0.5f), 1.5f), "Frequenz und Phase: t*f + phase");
}

void TestInterpolation() {
    NifNodeAnimation anim;
    NifNodeTrack track;
    track.stopTime = 1.0f;
    track.flags = 0x0008 | (2 << 1);
    track.keys.translation.interpolation = 1;
    track.keys.translation.keys = {{0.0f, {0.0f, 0.0f, 0.0f}}, {1.0f, {10.0f, 0.0f, 0.0f}}};
    track.keys.scale.interpolation = 2; // quadratisch mit Tangenten
    KfFloatKey s0, s1;
    s0.time = 0.0f; s0.value = 1.0f; s0.forward = 0.0f;
    s1.time = 1.0f; s1.value = 2.0f; s1.backward = 0.0f;
    track.keys.scale.keys = {s0, s1};
    anim.tracks.push_back(track);
    NifAnimNode node;
    node.track = 0;
    anim.nodes.push_back(node);
    const auto mid = SampleNifAnimNode(anim, 0, 0.5f);
    Check(Near(mid.tx, 5.0f), "Linear: halbe Strecke zur halben Zeit");
    // Hermite mit Tangenten 0: s(0.5) = 1.5, s(0.25) = 1 + (3*0.0625 - 2*0.015625) = 1.15625
    Check(Near(mid.scale, 1.5f) && Near(SampleNifAnimNode(anim, 0, 0.25f).scale, 1.15625f), "Quadratisch: Hermite mit gespeicherten Tangenten");
    anim.tracks[0].keys.translation.interpolation = 5;
    Check(Near(SampleNifAnimNode(anim, 0, 0.9f).tx, 0.0f), "Konstant: Wert des vorigen Keys");
    anim.tracks[0].flags = 0; // inaktiv
    Check(Near(SampleNifAnimNode(anim, 0, 0.5f).tx, 0.0f) && Near(SampleNifAnimNode(anim, 0, 0.5f).scale, 1.0f), "Inaktiver Controller bleibt in der Bind-Pose");
}

void TestHierarchy() {
    // Wurzel dreht sich in 1 s um 90° um die Legacy-Z-Achse (= Editor-Hochachse Y);
    // das Kind sitzt in der Bind-Pose bei Legacy (10, 0, 0).
    NifNodeAnimation anim;
    NifNodeTrack track;
    track.stopTime = 1.0f;
    track.flags = 0x0008 | (2 << 1);
    track.keys.rotationType = 4;
    track.keys.xyzRotation[2].interpolation = 1;
    KfFloatKey a, b;
    a.time = 0.0f; a.value = 0.0f;
    b.time = 1.0f; b.value = 3.14159265f * 0.5f;
    track.keys.xyzRotation[2].keys = {a, b};
    anim.tracks.push_back(track);
    NifAnimNode root;
    root.track = 0;
    NifAnimNode child;
    child.parent = 0;
    child.tx = 10.0f;
    anim.nodes = {root, child};

    const auto at0 = EvaluateNifNodeAnimation(anim, 0.0f);
    Check(at0.size() == 2 && IsIdentity(at0[1]), "Zur Startzeit ist die Animationsmatrix die Einheit");
    const auto at1 = EvaluateNifNodeAnimation(anim, 1.0f);
    // Ein Vertex des Kindes an Editor (10, 0, 0) wandert nach Legacy (0, 10, 0) = Editor (0, 0, 10).
    const auto p = Apply(at1[1], 10.0f, 0.0f, 0.0f);
    Check(Near(p[0], 0.0f) && Near(p[1], 0.0f) && Near(p[2], 10.0f), "Eltern-Rotation bewegt das Kind (Legacy -> Editor-Rahmen)");
    // Ein Vertex auf der Drehachse bleibt stehen.
    const auto axis = Apply(at1[1], 0.0f, 7.0f, 0.0f);
    Check(Near(axis[0], 0.0f) && Near(axis[1], 7.0f) && Near(axis[2], 0.0f), "Punkte auf der Drehachse bleiben stehen");
    Check(IsRigid(at1[1]) && Near(at1[1][0] * at1[1][0] + at1[1][1] * at1[1][1] + at1[1][2] * at1[1][2], 1.0f), "Matrix ist starr");
}

void TestRealModel(const std::string& path, std::size_t expectTracks) {
    const auto model = LoadNifMesh(path);
    Check(model.has_value(), ("Modell geladen: " + path).c_str());
    if (!model) return;
    const auto& anim = model->nodeAnimation;
    std::size_t animatedParts = 0;
    for (const auto& part : model->parts)
        if (part.animNode >= 0) ++animatedParts;
    Check(!anim.Empty() && animatedParts > 0, "Animierte Parts erkannt");
    if (expectTracks) Check(anim.tracks.size() == expectTracks, "Anzahl NiTransformController-Spuren");
    bool parentsFirst = true;
    for (std::size_t i = 0; i < anim.nodes.size(); ++i)
        if (anim.nodes[i].parent >= static_cast<std::int32_t>(i)) parentsFirst = false;
    Check(parentsFirst, "Eltern stehen vor ihren Kindern");

    const float duration = anim.Duration();
    const auto start = EvaluateNifNodeAnimation(anim, 0.0f);
    const auto quarter = EvaluateNifNodeAnimation(anim, duration * 0.25f);
    const auto looped = EvaluateNifNodeAnimation(anim, duration * 0.25f + duration * 3.0f);
    bool moves = false, rigid = true, loops = true;
    for (const auto& part : model->parts) {
        if (part.animNode < 0) continue;
        const auto n = static_cast<std::size_t>(part.animNode);
        if (!IsIdentity(quarter[n])) moves = true;
        if (!IsRigid(quarter[n])) rigid = false;
        for (int k = 0; k < 16; ++k)
            if (!Near(quarter[n][static_cast<std::size_t>(k)], looped[n][static_cast<std::size_t>(k)], 0.05f)) loops = false;
    }
    Check(moves, "Animierte Parts bewegen sich");
    Check(rigid, "Animationsmatrizen verzerren nicht (nur Drehung, Verschiebung, gleichmäßige Skalierung)");
    Check(loops, "Endlosschleife: gleiche Pose nach ganzen Durchläufen");
    (void)start;
}

} // namespace

int main(int argc, char** argv) {
    TestControllerTime();
    TestInterpolation();
    TestHierarchy();
    if (argc > 1) TestRealModel(argv[1], 2); // rou_lighthouse.nif: zwei drehende Lichtkegel
    if (argc > 2) TestRealModel(argv[2], 0); // dogAnd.nif
    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
