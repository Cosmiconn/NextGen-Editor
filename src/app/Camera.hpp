#pragma once
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
    // Verschiebt das Zielzentrum entlang der aktuellen Kamera-Rechts-/Auf-Achse (Bildschirmraum-
    // Pan, unabhängig von der aktuellen Blickrichtung) - macht die Kamera tatsächlich beweglich,
    // statt nur um einen fest bei (0,0,0) verankerten Punkt zu rotieren.
    void PanBy(float deltaRight, float deltaUp);
    void SetTarget(float x, float y, float z) { targetX_ = x; targetY_ = y; targetZ_ = z; }

    [[nodiscard]] Mat4 ViewMatrix() const;
    [[nodiscard]] static Mat4 PerspectiveMatrix(float fovYRad, float aspect, float nearZ, float farZ);

    [[nodiscard]] float Yaw() const noexcept { return yaw_; }
    [[nodiscard]] float Pitch() const noexcept { return pitch_; }
    [[nodiscard]] float Distance() const noexcept { return distance_; }
    [[nodiscard]] float TargetX() const noexcept { return targetX_; }
    [[nodiscard]] float TargetY() const noexcept { return targetY_; }
    [[nodiscard]] float TargetZ() const noexcept { return targetZ_; }

private:
    float yaw_ = 0.7f;
    float pitch_ = 0.6f;
    float distance_ = 6000.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    static constexpr float kMinPitch = 0.05f;
    static constexpr float kMaxPitch = 1.5f;
    static constexpr float kMinDistance = 100.0f;
};

} // namespace theseed::mapeditor::app
