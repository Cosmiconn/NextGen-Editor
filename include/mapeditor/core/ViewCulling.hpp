#pragma once
// ViewCulling.hpp
// Sichtpyramiden-Test für den Objekt-Renderer: Ebenen aus der View-Projektions-Matrix
// (Gribb/Hartmann) und ein konservativer AABB-Test. Matrizen spaltenweise wie in OpenGL.
// Die Near-Ebene wird für den OpenGL-Bereich -w..w bestimmt; bei einer 0..w-Projektion liegt sie
// damit hinter der echten Near-Ebene - es wird nie etwas Sichtbares verworfen.

#include <array>

namespace theseed::mapeditor::core {

using Mat4f = std::array<float, 16>; // spaltenweise: m[spalte*4 + zeile]
using Vec3f = std::array<float, 3>;

struct FrustumPlanes {
    // a*x + b*y + c*z + d >= 0 liegt innen.
    std::array<std::array<float, 4>, 6> planes{};
};

[[nodiscard]] FrustumPlanes ExtractFrustumPlanes(const Mat4f& viewProjection);

// Welt-AABB eines lokalen AABB unter einer affinen Matrix (Arvo).
void TransformAabb(const Mat4f& model, const Vec3f& localMin, const Vec3f& localMax, Vec3f& worldMin, Vec3f& worldMax);

// false nur, wenn der AABB sicher vollständig außerhalb einer Ebene liegt.
[[nodiscard]] bool AabbInFrustum(const FrustumPlanes& frustum, const Vec3f& worldMin, const Vec3f& worldMax);

} // namespace theseed::mapeditor::core
