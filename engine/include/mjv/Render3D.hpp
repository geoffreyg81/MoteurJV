#pragma once

#include "Color.hpp"
#include "Math.hpp"

// Couche 3D du moteur MoteurJV (raylib caché derrière l'API mjv::).
// Volontairement minimale : une caméra perspective + des primitives. De quoi
// poser les bases de la 3D (le rendu 2D reste disponible par-dessus, pour le HUD).
namespace mjv {

struct Camera3D {
    Vec3 position{0.0f, 10.0f, 12.0f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float fovy = 60.0f; // champ de vision vertical (degrés)
};

namespace Graphics3D {
void begin(const Camera3D& cam); // ouvre le mode 3D (à appeler dans onRender)
void end();                      // ferme le mode 3D

void drawCube(Vec3 position, Vec3 size, Color c);
void drawCubeWires(Vec3 position, Vec3 size, Color c);
void drawSphere(Vec3 center, float radius, Color c);
void drawPlane(Vec3 center, float width, float depth, Color c); // sol (plan XZ)
void drawGrid(int slices, float spacing);
} // namespace Graphics3D

} // namespace mjv
