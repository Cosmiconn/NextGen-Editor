#include "mapeditor/core/NifNodeAnimation.hpp"

#include <algorithm>
#include <cmath>

namespace theseed::mapeditor::core {

namespace {

// Affine Abbildung v' = m*v + t, m zeilenweise (enthält Rotation und Skalierung).
struct Affine {
    std::array<float, 9> m{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> t{0.0f, 0.0f, 0.0f};
};

Affine Mul(const Affine& a, const Affine& b) {
    Affine out;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            out.m[r * 3 + c] = a.m[r * 3 + 0] * b.m[0 * 3 + c] + a.m[r * 3 + 1] * b.m[1 * 3 + c] + a.m[r * 3 + 2] * b.m[2 * 3 + c];
        }
        out.t[r] = a.m[r * 3 + 0] * b.t[0] + a.m[r * 3 + 1] * b.t[1] + a.m[r * 3 + 2] * b.t[2] + a.t[r];
    }
    return out;
}

Affine Inverse(const Affine& a) {
    const auto& m = a.m;
    const float c00 = m[4] * m[8] - m[5] * m[7], c01 = m[5] * m[6] - m[3] * m[8], c02 = m[3] * m[7] - m[4] * m[6];
    const float det = m[0] * c00 + m[1] * c01 + m[2] * c02;
    if (std::abs(det) < 1e-12f) return {};
    const float inv = 1.0f / det;
    Affine out;
    out.m = {c00 * inv, (m[2] * m[7] - m[1] * m[8]) * inv, (m[1] * m[5] - m[2] * m[4]) * inv,
             c01 * inv, (m[0] * m[8] - m[2] * m[6]) * inv, (m[2] * m[3] - m[0] * m[5]) * inv,
             c02 * inv, (m[1] * m[6] - m[0] * m[7]) * inv, (m[0] * m[4] - m[1] * m[3]) * inv};
    for (int r = 0; r < 3; ++r)
        out.t[r] = -(out.m[r * 3 + 0] * a.t[0] + out.m[r * 3 + 1] * a.t[1] + out.m[r * 3 + 2] * a.t[2]);
    return out;
}

Affine FromNode(const NifAnimNode& n) {
    Affine a;
    for (int i = 0; i < 9; ++i) a.m[i] = n.rotation[i] * n.scale;
    a.t = {n.tx, n.ty, n.tz};
    return a;
}

KfQuat Normalize(KfQuat q) {
    const float n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (n < 1e-8f) return {};
    return {q.w / n, q.x / n, q.y / n, q.z / n};
}

KfQuat Mul(const KfQuat& a, const KfQuat& b) {
    return Normalize({a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w});
}

KfQuat Axis(float x, float y, float z, float angle) {
    const float s = std::sin(angle * 0.5f);
    return Normalize({std::cos(angle * 0.5f), x * s, y * s, z * s});
}

KfQuat Slerp(KfQuat a, KfQuat b, float t) {
    a = Normalize(a);
    b = Normalize(b);
    float dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (dot < 0.0f) { dot = -dot; b = {-b.w, -b.x, -b.y, -b.z}; }
    if (dot > 0.9995f) return Normalize({a.w + (b.w - a.w) * t, a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t});
    const float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
    const float s = std::sin(theta);
    const float wa = std::sin((1.0f - t) * theta) / s, wb = std::sin(t * theta) / s;
    return Normalize({a.w * wa + b.w * wb, a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb});
}

std::array<float, 9> QuatMatrix(KfQuat q) {
    q = Normalize(q);
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    return {1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz), 2.0f * (xz + wy),
            2.0f * (xy + wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz - wx),
            2.0f * (xz - wy), 2.0f * (yz + wx), 1.0f - 2.0f * (xx + yy)};
}

template <class Key>
std::size_t SegmentStart(const std::vector<Key>& keys, float time) {
    const auto it = std::upper_bound(keys.begin(), keys.end(), time, [](float t, const Key& k) { return t < k.time; });
    return it == keys.begin() ? 0 : static_cast<std::size_t>(it - keys.begin()) - 1;
}

float Hermite(float a, float outTan, float b, float inTan, float t) {
    const float t2 = t * t, t3 = t2 * t;
    return a * (2.0f * t3 - 3.0f * t2 + 1.0f) + b * (-2.0f * t3 + 3.0f * t2) + outTan * (t3 - 2.0f * t2 + t) + inTan * (t3 - t2);
}

// Kochanek-Bartels-Tangenten eines Keys aus Vorgänger/Nachfolger (an den Enden gespiegelt).
void TbcTangents(float prev, float cur, float next, float tension, float bias, float continuity, float& in, float& out) {
    const float d0 = cur - prev, d1 = next - cur;
    const float k = 1.0f - tension;
    out = 0.5f * k * ((1.0f + bias) * (1.0f - continuity) * d0 + (1.0f - bias) * (1.0f + continuity) * d1);
    in = 0.5f * k * ((1.0f + bias) * (1.0f + continuity) * d0 + (1.0f - bias) * (1.0f - continuity) * d1);
}

// Gemeinsamer Sampler für Float- und Vec3-Keys; `get(key, c)` liefert Komponente c.
template <class Key, class Value, class Get, class Fwd, class Bwd>
float SampleComponent(std::uint32_t interpolation, const std::vector<Key>& keys, float time, int c, Get get, Fwd fwd, Bwd bwd) {
    if (time <= keys.front().time) return get(keys.front(), c);
    if (time >= keys.back().time) return get(keys.back(), c);
    const std::size_t i = SegmentStart(keys, time);
    const auto& a = keys[i];
    const auto& b = keys[std::min(i + 1, keys.size() - 1)];
    const float span = b.time - a.time;
    const float t = span > 1e-8f ? std::clamp((time - a.time) / span, 0.0f, 1.0f) : 0.0f;
    switch (interpolation) {
        case 5: return get(a, c);
        case 2: return Hermite(get(a, c), fwd(a, c), get(b, c), bwd(b, c), t);
        case 3: {
            const auto& p = keys[i == 0 ? 0 : i - 1];
            const auto& n = keys[std::min(i + 2, keys.size() - 1)];
            float inA, outA, inB, outB;
            TbcTangents(get(p, c), get(a, c), get(b, c), a.tension, a.bias, a.continuity, inA, outA);
            TbcTangents(get(a, c), get(b, c), get(n, c), b.tension, b.bias, b.continuity, inB, outB);
            return Hermite(get(a, c), outA, get(b, c), inB, t);
        }
        default: return get(a, c) + (get(b, c) - get(a, c)) * t;
    }
}

float SampleFloat(const KfKeyGroupFloat& g, float time) {
    return SampleComponent<KfFloatKey, float>(
        g.interpolation, g.keys, time, 0, [](const KfFloatKey& k, int) { return k.value; },
        [](const KfFloatKey& k, int) { return k.forward; }, [](const KfFloatKey& k, int) { return k.backward; });
}

float Comp(const KfVec3& v, int c) { return c == 0 ? v.x : (c == 1 ? v.y : v.z); }

KfVec3 SampleVec3(const KfKeyGroupVec3& g, float time) {
    float out[3];
    for (int c = 0; c < 3; ++c) {
        out[c] = SampleComponent<KfVec3Key, KfVec3>(
            g.interpolation, g.keys, time, c, [](const KfVec3Key& k, int i) { return Comp(k.value, i); },
            [](const KfVec3Key& k, int i) { return Comp(k.forward, i); }, [](const KfVec3Key& k, int i) { return Comp(k.backward, i); });
    }
    return {out[0], out[1], out[2]};
}

bool SampleRotation(const KfTransformKeys& keys, float time, KfQuat& out) {
    if (keys.rotationType == 4) {
        float angle[3] = {0.0f, 0.0f, 0.0f};
        bool any = false;
        for (int axis = 0; axis < 3; ++axis) {
            if (keys.xyzRotation[axis].keys.empty()) continue;
            angle[axis] = SampleFloat(keys.xyzRotation[axis], time);
            any = true;
        }
        if (!any) return false;
        // Gamebryo wendet XYZ-Rotationskeys in der Reihenfolge X, Y, Z an.
        out = Mul(Mul(Axis(0, 0, 1, angle[2]), Axis(0, 1, 0, angle[1])), Axis(1, 0, 0, angle[0]));
        return true;
    }
    const auto& q = keys.quaternionRotation;
    if (q.empty()) return false;
    if (time <= q.front().time) { out = Normalize(q.front().value); return true; }
    if (time >= q.back().time) { out = Normalize(q.back().value); return true; }
    const std::size_t i = SegmentStart(q, time);
    const auto& a = q[i];
    const auto& b = q[std::min(i + 1, q.size() - 1)];
    if (keys.rotationType == 5) { out = Normalize(a.value); return true; }
    const float span = b.time - a.time;
    out = Slerp(a.value, b.value, span > 1e-8f ? std::clamp((time - a.time) / span, 0.0f, 1.0f) : 0.0f);
    return true;
}

} // namespace

float NifNodeAnimation::Duration() const {
    float d = 0.0f;
    for (const auto& t : tracks)
        if (t.frequency > 1e-6f) d = std::max(d, (t.stopTime - t.startTime) / t.frequency);
    return d;
}

float NifControllerTime(const NifNodeTrack& track, float sceneTime) {
    const float lo = std::min(track.startTime, track.stopTime), hi = std::max(track.startTime, track.stopTime);
    const float span = hi - lo;
    float t = sceneTime * track.frequency + track.phase;
    if (!std::isfinite(t) || span <= 1e-6f) return lo;
    switch ((track.flags >> 1) & 3) {
        case 1: { // Reverse: hin und zurück
            float u = std::fmod(t - lo, 2.0f * span);
            if (u < 0.0f) u += 2.0f * span;
            return u <= span ? lo + u : hi - (u - span);
        }
        case 2: return std::clamp(t, lo, hi); // Clamp
        default: {                             // Loop
            float u = std::fmod(t - lo, span);
            if (u < 0.0f) u += span;
            return lo + u;
        }
    }
}

NifAnimNode SampleNifAnimNode(const NifNodeAnimation& animation, std::size_t node, float sceneTime) {
    NifAnimNode out = animation.nodes[node];
    if (out.track < 0 || static_cast<std::size_t>(out.track) >= animation.tracks.size()) return out;
    const auto& track = animation.tracks[static_cast<std::size_t>(out.track)];
    if ((track.flags & 0x0008) == 0) return out; // Controller inaktiv
    const float time = NifControllerTime(track, sceneTime);
    if (!track.keys.translation.keys.empty()) {
        const auto v = SampleVec3(track.keys.translation, time);
        out.tx = v.x; out.ty = v.y; out.tz = v.z;
    } else if (track.poseTranslation) {
        out.tx = track.pose.translation.x; out.ty = track.pose.translation.y; out.tz = track.pose.translation.z;
    }
    if (!track.keys.scale.keys.empty()) out.scale = SampleFloat(track.keys.scale, time);
    else if (track.poseScale) out.scale = track.pose.scale;
    KfQuat q;
    if (SampleRotation(track.keys, time, q)) out.rotation = QuatMatrix(q);
    else if (track.poseRotation) out.rotation = QuatMatrix(track.pose.rotation);
    return out;
}

std::vector<std::array<float, 16>> EvaluateNifNodeAnimation(const NifNodeAnimation& animation, float sceneTime) {
    const std::size_t n = animation.nodes.size();
    std::vector<Affine> bind(n), anim(n);
    std::vector<std::array<float, 16>> out(n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& node = animation.nodes[i];
        const Affine localBind = FromNode(node);
        const Affine localAnim = FromNode(SampleNifAnimNode(animation, i, sceneTime));
        const bool hasParent = node.parent >= 0 && static_cast<std::size_t>(node.parent) < i;
        bind[i] = hasParent ? Mul(bind[static_cast<std::size_t>(node.parent)], localBind) : localBind;
        anim[i] = hasParent ? Mul(anim[static_cast<std::size_t>(node.parent)], localAnim) : localAnim;
        const Affine delta = Mul(anim[i], Inverse(bind[i]));
        // Legacy (x, y, z) -> Editor (x, z, y): P*delta*P mit P = Tausch von y und z.
        constexpr int P[3] = {0, 2, 1};
        auto& m = out[i];
        m.fill(0.0f);
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) m[static_cast<std::size_t>(c * 4 + r)] = delta.m[static_cast<std::size_t>(P[r] * 3 + P[c])];
            m[static_cast<std::size_t>(12 + r)] = delta.t[static_cast<std::size_t>(P[r])];
        }
        m[15] = 1.0f;
    }
    return out;
}

} // namespace theseed::mapeditor::core
