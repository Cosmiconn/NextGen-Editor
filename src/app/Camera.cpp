#include "Camera.hpp"

#include <algorithm>
#include <cmath>

namespace theseed::mapeditor::app {

namespace {

struct Vec3 {
    float x, y, z;
};

Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 Normalize(Vec3 v) {
    const float len = std::sqrt(Dot(v, v));
    if (len < 1e-6f) return {0.0f, 0.0f, 0.0f};
    return {v.x / len, v.y / len, v.z / len};
}

} // namespace

Mat4 Mat4::Identity() {
    Mat4 result;
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

// Spaltenweise Multiplikation (Column-Major, OpenGL-Konvention: m[col*4+row]).
Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 out;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            out.m[col * 4 + row] = sum;
        }
    }
    return out;
}

void OrbitCamera::OrbitBy(float deltaYawRad, float deltaPitchRad) {
    yaw_ += deltaYawRad;
    pitch_ = std::clamp(pitch_ + deltaPitchRad, kMinPitch, kMaxPitch);
}

void OrbitCamera::Zoom(float deltaDistance) {
    distance_ = std::max(kMinDistance, distance_ + deltaDistance);
}

void OrbitCamera::PanBy(float deltaRight, float deltaUp) {
    const Vec3 eyeOffset{
        distance_ * std::cos(pitch_) * std::sin(yaw_),
        distance_ * std::sin(pitch_),
        distance_ * std::cos(pitch_) * std::cos(yaw_),
    };
    const Vec3 f = Normalize(Vec3{-eyeOffset.x, -eyeOffset.y, -eyeOffset.z});
    const Vec3 up{0.0f, 1.0f, 0.0f};
    const Vec3 s = Normalize(Cross(f, up));
    const Vec3 u = Cross(s, f);

    targetX_ += s.x * deltaRight + u.x * deltaUp;
    targetY_ += s.y * deltaRight + u.y * deltaUp;
    targetZ_ += s.z * deltaRight + u.z * deltaUp;
}

Mat4 OrbitCamera::ViewMatrix() const {
    const Vec3 eye{
        targetX_ + distance_ * std::cos(pitch_) * std::sin(yaw_),
        targetY_ + distance_ * std::sin(pitch_),
        targetZ_ + distance_ * std::cos(pitch_) * std::cos(yaw_),
    };
    const Vec3 center{targetX_, targetY_, targetZ_};
    const Vec3 up{0.0f, 1.0f, 0.0f};

    const Vec3 f = Normalize(Sub(center, eye));
    const Vec3 s = Normalize(Cross(f, up));
    const Vec3 u = Cross(s, f);

    Mat4 view = Mat4::Identity();
    view.m[0] = s.x; view.m[4] = s.y; view.m[8] = s.z;  view.m[12] = -Dot(s, eye);
    view.m[1] = u.x; view.m[5] = u.y; view.m[9] = u.z;  view.m[13] = -Dot(u, eye);
    view.m[2] = -f.x; view.m[6] = -f.y; view.m[10] = -f.z; view.m[14] = Dot(f, eye);
    view.m[3] = 0.0f; view.m[7] = 0.0f; view.m[11] = 0.0f; view.m[15] = 1.0f;
    return view;
}

Mat4 OrbitCamera::PerspectiveMatrix(float fovYRad, float aspect, float nearZ, float farZ) {
    Mat4 proj{};
    const float tanHalfFovy = std::tan(fovYRad / 2.0f);
    proj.m[0] = 1.0f / (aspect * tanHalfFovy);
    proj.m[5] = 1.0f / tanHalfFovy;
    proj.m[10] = -(farZ + nearZ) / (farZ - nearZ);
    proj.m[11] = -1.0f;
    proj.m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
    return proj;
}

Mat4 OrthoTopDownViewProj(float centerX, float centerZ, float halfWidth, float halfHeight, float heightPadding) {
    const Vec3 eye{centerX, heightPadding, centerZ};
    constexpr Vec3 f{0.0f, -1.0f, 0.0f};
    constexpr Vec3 upRef{0.0f, 0.0f, -1.0f};
    const Vec3 s = Normalize(Cross(f, upRef));
    const Vec3 u = Cross(s, f);

    Mat4 view = Mat4::Identity();
    view.m[0] = s.x; view.m[4] = s.y; view.m[8] = s.z;   view.m[12] = -Dot(s, eye);
    view.m[1] = u.x; view.m[5] = u.y; view.m[9] = u.z;   view.m[13] = -Dot(u, eye);
    view.m[2] = -f.x; view.m[6] = -f.y; view.m[10] = -f.z; view.m[14] = Dot(f, eye);
    view.m[3] = 0.0f; view.m[7] = 0.0f; view.m[11] = 0.0f; view.m[15] = 1.0f;

    const float nearZ = 1.0f;
    const float farZ = heightPadding + 20000.0f;
    Mat4 proj{};
    proj.m[0] = 1.0f / halfWidth;
    proj.m[5] = 1.0f / halfHeight;
    proj.m[10] = -2.0f / (farZ - nearZ);
    proj.m[14] = -(farZ + nearZ) / (farZ - nearZ);
    proj.m[15] = 1.0f;

    return proj * view;
}

} // namespace theseed::mapeditor::app
