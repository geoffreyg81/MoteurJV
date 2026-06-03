#pragma once

#include <cmath>

#include "Math.hpp"

// Collision 2D par boîtes alignées sur les axes (AABB).
// Les boîtes sont décrites par un centre et des demi-tailles (halfSize).
namespace mjv {

// Vrai si les deux boîtes se chevauchent.
inline bool aabbOverlap(Vec2 centerA, Vec2 halfA, Vec2 centerB, Vec2 halfB) {
    return std::fabs(centerB.x - centerA.x) < (halfA.x + halfB.x) &&
           std::fabs(centerB.y - centerA.y) < (halfA.y + halfB.y);
}

// Calcule le "Minimum Translation Vector" : le plus petit déplacement à
// appliquer à A pour qu'elle ne chevauche plus B. Renvoie false si pas de
// collision. Pousse toujours sur l'axe de moindre pénétration.
inline bool aabbResolve(Vec2 centerA, Vec2 halfA, Vec2 centerB, Vec2 halfB, Vec2& mtv) {
    const float dx = centerB.x - centerA.x;
    const float px = (halfA.x + halfB.x) - std::fabs(dx); // recouvrement en X
    if (px <= 0.0f) return false;

    const float dy = centerB.y - centerA.y;
    const float py = (halfA.y + halfB.y) - std::fabs(dy); // recouvrement en Y
    if (py <= 0.0f) return false;

    if (px < py) {
        mtv = {dx < 0.0f ? px : -px, 0.0f}; // éloigner A de B horizontalement
    } else {
        mtv = {0.0f, dy < 0.0f ? py : -py}; // ... ou verticalement
    }
    return true;
}

} // namespace mjv
