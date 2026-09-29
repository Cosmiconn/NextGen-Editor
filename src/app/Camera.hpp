#pragma once

#include <algorithm>
#include <cmath>
// Camera.hpp
// Einfache Orbit-Kamera für die 3D-Vorschau des Heightmap-Editors.
// Bewusst minimal: Yaw/Pitch/Distance um einen Zielpunkt, keine Quaternion-Free-Cam -
// für einen Terrain-Editor ist Orbit um den Bearbeitungsbereich das natürlichere Modell.

namespace theseed::mapeditor::app {

struct Mat4 {
    float m[16]{};
    static Mat4 Identity();
};

Mat4 operator*(const Mat4& a, const Mat4& b);

// Kombinierte View+Projektion für eine orthographische Draufsicht direkt von oben, zentriert
// auf (centerX, centerZ), deckt genau [halfWidth, halfHeight] in Weltraum-Einheiten ab. Kein
// OrbitCamera-Objekt nötig (unabhängig von der 3D-Perspektivkamera) - für den 2D-Editor-
// Hintergrund (siehe HeightmapRenderer::BeginTopDownScene).
[[nodiscard]] Mat4 OrthoTopDownViewProj(float centerX, float centerZ, float halfWidth, float halfHeight, float heightPadding);

class OrbitCamera {
public:
    void OrbitBy(float deltaYawRad, float deltaPitchRad);
    void Zoom(float deltaDistance);
    // Mausrad-Zoom: multiplikativ (jeder Schritt ~12 % der aktuellen Entfernung) - fein genug fuer
    // "ganz nah dran" und schnell genug fuer die ganze Karte. `steps` > 0 = naeher.
    void ZoomSteps(float steps);
    // Freies Umsehen (rechte Maustaste): dreht die Blickrichtung um die AUGENPOSITION (die Kamera
    // bleibt stehen, das Ziel wandert mit) - wie bei einer Ego-Kamera, egal wie klein die Entfernung.
    void LookBy(float deltaYawRad, float deltaPitchRad);
    // Bewegen (WASD): forward/right in Bildschirmrichtung entlang der Horizontalen, up = Hoehe.
    // Einheiten = Welteinheiten (Aufrufer skaliert mit Zeit und Tempo).
    void MoveLocal(float forward, float right, float up);
    // Verschiebt das Zielzentrum entlang der aktuellen Kamera-Rechts-/Auf-Achse (Bildschirmraum-
    // Pan, unabhängig von der aktuellen Blickrichtung) - macht die Kamera tatsächlich beweglich,
    // statt nur um einen fest bei (0,0,0) verankerten Punkt zu rotieren.
    void PanBy(float deltaRight, float deltaUp);
    // Ziel in WELT-Koordinaten. Die Kamera rechnet intern im gespiegelten "Anzeigeraum"
    // (Z -> -Z, siehe ViewMatrix), deshalb wird Z hier umgerechnet - Aufrufer bemerken davon nichts.
    void SetTarget(float x, float y, float z) { targetX_ = x; targetY_ = y; targetZ_ = -z; }

    [[nodiscard]] Mat4 ViewMatrix() const;
    [[nodiscard]] static Mat4 PerspectiveMatrix(float fovYRad, float aspect, float nearZ, float farZ);

    [[nodiscard]] float Yaw() const noexcept { return yaw_; }
    [[nodiscard]] float Pitch() const noexcept { return pitch_; }
    [[nodiscard]] float Distance() const noexcept { return distance_; }
    // Dynamische Clip-Ebenen: nah dran (Entfernung wenige Einheiten) darf die Nahebene nicht bei 10
    // stehen bleiben, sonst wird alles Nahe abgeschnitten.
    [[nodiscard]] float NearPlane() const noexcept { return std::clamp(distance_ * 0.02f, 0.4f, 15.0f); }
    [[nodiscard]] float FarPlane() const noexcept { return 90000.0f; }
    // Augenposition im (gespiegelten) Anzeigeraum - fuer Tests und Positionsanzeige.
    [[nodiscard]] float EyeX() const noexcept { return targetX_ + distance_ * std::cos(pitch_) * std::sin(yaw_); }
    [[nodiscard]] float EyeY() const noexcept { return targetY_ + distance_ * std::sin(pitch_); }
    [[nodiscard]] float EyeZ() const noexcept { return targetZ_ + distance_ * std::cos(pitch_) * std::cos(yaw_); }
    [[nodiscard]] float TargetX() const noexcept { return targetX_; }
    [[nodiscard]] float TargetY() const noexcept { return targetY_; }
    [[nodiscard]] float TargetZ() const noexcept { return -targetZ_; } // Welt-Koordinate

private:
    float yaw_ = 0.7f;
    float pitch_ = 0.6f;
    float distance_ = 6000.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    // Aus grosser Entfernung bleibt die Kamera ueber dem Boden (Pitch >= 0.05); nah am Ziel darf sie
    // nach oben schauen (negativer Pitch = Kamera unterhalb des Ziels).
    [[nodiscard]] float MinPitch() const noexcept { return distance_ > 400.0f ? 0.05f : -1.35f; }
    static constexpr float kMaxPitch = 1.5f;
    static constexpr float kMinDistance = 1.5f;
    static constexpr float kMaxDistance = 70000.0f;
};

} // namespace theseed::mapeditor::app
