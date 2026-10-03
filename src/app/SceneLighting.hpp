#pragma once
// SceneLighting.hpp
// Gemeinsame Beleuchtungs-/Nebelparameter für Terrain- und NIF-Renderer. Quelle ist die
// SHMD-Umgebung der Karte (Rou.shmd: GlobalLight, DirectionLightAmbient/-Diffuse, Fog,
// BackGroundColor, Frustum); siehe docs/RENDERER.md für Datenherkunft und Näherungen.

#include "mapeditor/core/ObjectPlacement.hpp"

#include <algorithm>
#include <cmath>

namespace theseed::mapeditor::app {

struct SceneLighting {
    // false: bisherige Editor-Beleuchtung (keine Kartendaten vorhanden).
    bool fromMapData = false;
    float ambient[3] = {1.0f, 1.0f, 1.0f};  // GlobalLight + DirectionLightAmbient
    float sun[3] = {1.0f, 1.0f, 1.0f};      // DirectionLightDiffuse
    float lightDir[3] = {-0.4f, -1.0f, -0.3f}; // Richtung ist in den Daten nicht enthalten
    bool fogEnabled = false;
    float fogColor[3] = {0.0f, 0.0f, 0.0f};
    float fogStart = 0.0f;
    float fogEnd = 1.0f;
    float background[3] = {0.10f, 0.11f, 0.13f};
    bool backgroundFromMapData = false;
};

// Fog-Zeile der SHMD: "Fog <Tiefe> <R> <G> <B>" (Rou: 0.57 0.0706 0.5412 0.9294). Der Farbanteil
// ist eindeutig eine Farbe (Himmelblau wie BackGroundColor). Die Tiefe wird wie Gamebryos
// NiFogProperty-"depth" als Anteil der Sichtweite (Frustum) gelesen, über den - vom fernen Ende
// aus gemessen - der Nebel aufgebaut wird: Start = Frustum * (1 - Tiefe), Ende = Frustum.
// Diese Zuordnung ist eine dokumentierte Editor-Näherung, nicht aus Client-Code belegt.
inline SceneLighting SceneLightingFromEnvironment(const core::SceneEnvironment& env, bool hasLightingFooter) {
    SceneLighting l;
    l.fromMapData = true;
    for (int c = 0; c < 3; ++c) {
        const float extraAmbient = hasLightingFooter ? env.directionLightAmbient[c] : 0.0f;
        l.ambient[c] = std::clamp(env.globalLight[c] + extraAmbient, 0.0f, 2.0f);
        l.sun[c] = hasLightingFooter ? std::clamp(env.directionLightDiffuse[c], 0.0f, 2.0f) : 1.0f;
        l.fogColor[c] = std::clamp(env.fog[c + 1], 0.0f, 1.0f);
        l.background[c] = std::clamp(env.backgroundColor[c], 0.0f, 1.0f);
    }
    l.backgroundFromMapData = true;
    const float far = env.frustumFar > 1.0f && std::isfinite(env.frustumFar) ? env.frustumFar : 5000.0f;
    const float depth = std::clamp(env.fog[0], 0.0f, 1.0f);
    l.fogEnd = far;
    l.fogStart = far * (1.0f - depth);
    if (l.fogEnd - l.fogStart < 1.0f) l.fogStart = l.fogEnd - 1.0f;
    return l;
}

} // namespace theseed::mapeditor::app
