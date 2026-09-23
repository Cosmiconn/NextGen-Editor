// test_camera_handedness.cpp
// Prueft, dass die Orbit-Kamera die Welt rechtshaendig darstellt (siehe CHANGELOG [0.44.27]):
// Die Editor-Achsen entstehen aus dem Legacy-Z-up-Format durch reines Vertauschen (x,y,z)->(x,z,y),
// eine Spiegelung - die Kamera spiegelt Z deshalb wieder zurueck. Blickt die Kamera nach Norden
// (+Z der Welt = +Y im Legacy-Format), muss Osten (+X) rechts und Norden oben im Bild liegen.

#include "Camera.hpp"

#include <cmath>
#include <cstdio>

using namespace theseed::mapeditor::app;

namespace {
void Project(const Mat4& vp, float x, float y, float z, float& ox, float& oy) {
    const float cx = vp.m[0] * x + vp.m[4] * y + vp.m[8] * z + vp.m[12];
    const float cy = vp.m[1] * x + vp.m[5] * y + vp.m[9] * z + vp.m[13];
    const float cw = vp.m[3] * x + vp.m[7] * y + vp.m[11] * z + vp.m[15];
    ox = cx / cw;
    oy = cy / cw;
}
} // namespace

int main() {
    OrbitCamera camera;
    camera.SetTarget(1000.0f, 0.0f, 1000.0f);
    camera.OrbitBy(-camera.Yaw(), 0.0f); // Yaw 0: Blick entlang +Z der Welt
    int failures = 0;
    if (std::abs(camera.TargetZ() - 1000.0f) > 0.01f) { std::fprintf(stderr, "[FEHLER] TargetZ() liefert nicht die Welt-Koordinate\n"); ++failures; }
    const Mat4 vp = OrbitCamera::PerspectiveMatrix(0.9f, 1.6f, 10.0f, 50000.0f) * camera.ViewMatrix();
    float cx, cy, ex, ey, nx, ny;
    Project(vp, 1000.0f, 0.0f, 1000.0f, cx, cy);
    Project(vp, 1300.0f, 0.0f, 1000.0f, ex, ey);
    Project(vp, 1000.0f, 0.0f, 1300.0f, nx, ny);
    if (!(ex > cx)) { std::fprintf(stderr, "[FEHLER] Osten (+X) liegt nicht rechts\n"); ++failures; }
    else std::printf("[ok]     Osten (+X) liegt rechts von der Bildmitte\n");
    if (!(ny > cy)) { std::fprintf(stderr, "[FEHLER] Norden (+Z) liegt nicht oben\n"); ++failures; }
    else std::printf("[ok]     Norden (+Z) liegt oberhalb der Bildmitte\n");
    // ---- Ego-Kamera (seit [0.44.32]): Umsehen, Laufen, Zoom, Clip-Ebenen ----
    {
        auto check = [&](bool ok, const char* what) {
            if (ok) std::printf("[ok]     %s\n", what);
            else { std::fprintf(stderr, "[FEHLER] %s\n", what); ++failures; }
        };
        OrbitCamera c;
        c.SetTarget(1000.0f, 50.0f, 2000.0f);
        c.Zoom(-c.Distance() + 300.0f);
        const float ex = c.EyeX(), ey = c.EyeY(), ez = c.EyeZ();
        c.LookBy(0.7f, -0.3f);
        check(std::fabs(c.EyeX() - ex) < 0.05f && std::fabs(c.EyeY() - ey) < 0.05f && std::fabs(c.EyeZ() - ez) < 0.05f,
              "LookBy: die Augenposition bleibt beim Umsehen stehen");
        const float yawBefore = c.Yaw(), pitchBefore = c.Pitch();
        const float bx = c.EyeX(), bz = c.EyeZ();
        c.MoveLocal(100.0f, 0.0f, 0.0f);
        const float moved = std::sqrt((c.EyeX() - bx) * (c.EyeX() - bx) + (c.EyeZ() - bz) * (c.EyeZ() - bz));
        check(std::fabs(moved - 100.0f) < 0.1f && c.Yaw() == yawBefore && c.Pitch() == pitchBefore, "MoveLocal(vorwaerts 100): 100 Einheiten weiter, Blickrichtung unveraendert");
        // Vorwaerts laufen bringt uns naeher an den Punkt, auf den wir schauen (Ziel liegt vor dem Auge).
        const float d0 = std::sqrt((c.TargetX() - c.EyeX()) * (c.TargetX() - c.EyeX()) + (-c.TargetZ() - c.EyeZ()) * (-c.TargetZ() - c.EyeZ()));
        c.MoveLocal(50.0f, 0.0f, 0.0f);
        check(std::fabs(std::sqrt((c.TargetX() - c.EyeX()) * (c.TargetX() - c.EyeX()) + (-c.TargetZ() - c.EyeZ()) * (-c.TargetZ() - c.EyeZ())) - d0) < 0.1f,
              "Ziel wandert mit dem Auge (Abstand bleibt)");
        const float r0x = c.EyeX(), r0z = c.EyeZ();
        c.MoveLocal(0.0f, 100.0f, 0.0f);
        // seitwaerts: senkrecht zur Blickrichtung (Skalarprodukt mit der Blickrichtung ~ 0)
        const float fdx = c.TargetX() - c.EyeX(), fdz = -c.TargetZ() - c.EyeZ(); // Anzeigeraum (Z gespiegelt)
        const float dot = (c.EyeX() - r0x) * fdx + (c.EyeZ() - r0z) * fdz;
        check(std::fabs(dot) < 1.0f, "MoveLocal(rechts): senkrecht zur Blickrichtung");
        OrbitCamera z;
        z.Zoom(-z.Distance() + 1000.0f);
        z.ZoomSteps(1.0f);
        check(std::fabs(z.Distance() - 880.0f) < 1.0f, "ZoomSteps(1): 12 % naeher");
        z.ZoomSteps(200.0f);
        check(z.Distance() >= 1.4f && z.Distance() <= 1.6f, "Zoom stoppt bei ~1.5 Einheiten (ganz nah dran)");
        check(z.NearPlane() < 1.0f && OrbitCamera().NearPlane() > 10.0f, "Nahebene passt sich an: nah < 1, weit > 10");
        OrbitCamera p;
        p.Zoom(-p.Distance() + 100.0f);
        p.OrbitBy(0.0f, -2.0f);
        check(p.Pitch() < 0.0f, "nah am Ziel darf die Kamera nach oben schauen (negativer Pitch)");
        OrbitCamera far;
        far.Zoom(3000.0f);
        far.OrbitBy(0.0f, -2.0f);
        check(far.Pitch() >= 0.05f - 1e-4f, "aus grosser Entfernung bleibt die Kamera ueber dem Boden");
    }
    std::printf("\n%d Fehler.\n", failures);
    return failures == 0 ? 0 : 1;
}
