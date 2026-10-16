// test_view_culling.cpp
// Sichtpyramiden-Test des Objekt-Renderers.

#include "mapeditor/core/ViewCulling.hpp"

#include <cmath>
#include <cstdio>

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

// OpenGL-Perspektive (Blick entlang -Z), spaltenweise.
Mat4f Perspective(float fovY, float aspect, float n, float f) {
    const float t = 1.0f / std::tan(fovY * 0.5f);
    Mat4f m{};
    m[0] = t / aspect;
    m[5] = t;
    m[10] = (f + n) / (n - f);
    m[11] = -1.0f;
    m[14] = 2.0f * f * n / (n - f);
    return m;
}

Mat4f Ortho(float halfW, float halfH, float n, float f) {
    Mat4f m{};
    m[0] = 1.0f / halfW;
    m[5] = 1.0f / halfH;
    m[10] = -2.0f / (f - n);
    m[14] = -(f + n) / (f - n);
    m[15] = 1.0f;
    return m;
}

bool Box(const FrustumPlanes& f, float cx, float cy, float cz, float h) {
    return AabbInFrustum(f, {cx - h, cy - h, cz - h}, {cx + h, cy + h, cz + h});
}

} // namespace

int main() {
    const auto persp = ExtractFrustumPlanes(Perspective(1.0f, 1.5f, 1.0f, 1000.0f));
    Check(Box(persp, 0, 0, -100, 1), "Box vor der Kamera ist sichtbar");
    Check(!Box(persp, 0, 0, 100, 1), "Box hinter der Kamera wird verworfen");
    Check(!Box(persp, 0, 0, -2000, 1), "Box hinter der Fern-Ebene wird verworfen");
    Check(!Box(persp, 500, 0, -100, 1), "Box weit rechts außerhalb wird verworfen");
    Check(Box(persp, 80, 0, -100, 10), "Box, die den Bildrand schneidet, bleibt sichtbar");
    Check(Box(persp, 0, 0, 0, 5), "Box um die Kamera bleibt sichtbar");

    const auto ortho = ExtractFrustumPlanes(Ortho(100.0f, 50.0f, 1.0f, 500.0f));
    Check(Box(ortho, 90, 40, -10, 1) && !Box(ortho, 0, 70, -10, 5), "Orthografisch: Rechteck-Grenzen");

    // Gedrehte, verschobene Box: Welt-AABB umschließt alle Ecken.
    Mat4f model{};
    const float c = std::cos(0.5f), s = std::sin(0.5f);
    model[0] = c; model[1] = 0; model[2] = -s;
    model[4] = 0; model[5] = 2; model[6] = 0;
    model[8] = s; model[9] = 0; model[10] = c;
    model[12] = 10; model[13] = 20; model[14] = 30; model[15] = 1;
    Vec3f lo, hi;
    TransformAabb(model, {-1, -1, -1}, {1, 1, 1}, lo, hi);
    bool contains = true;
    for (int i = 0; i < 8; ++i) {
        const float x = (i & 1) ? 1.f : -1.f, y = (i & 2) ? 1.f : -1.f, z = (i & 4) ? 1.f : -1.f;
        const float wx = model[0] * x + model[4] * y + model[8] * z + model[12];
        const float wy = model[1] * x + model[5] * y + model[9] * z + model[13];
        const float wz = model[2] * x + model[6] * y + model[10] * z + model[14];
        if (wx < lo[0] - 1e-4f || wx > hi[0] + 1e-4f || wy < lo[1] - 1e-4f || wy > hi[1] + 1e-4f || wz < lo[2] - 1e-4f || wz > hi[2] + 1e-4f)
            contains = false;
    }
    Check(contains && std::abs(hi[1] - 22.0f) < 1e-4f && std::abs(lo[1] - 18.0f) < 1e-4f, "Welt-AABB umschließt die gedrehte und skalierte Box");

    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
