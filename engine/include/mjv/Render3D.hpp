#pragma once

#include <string>

#include "Color.hpp"
#include "Math.hpp"

// Couche 3D du moteur MoteurJV (raylib caché derrière l'API mjv::).
// Caméra perspective, primitives, chargement de modèles (.obj/.gltf/.glb) et un
// éclairage directionnel simple (shader). Le rendu 2D reste dispo par-dessus (HUD).
namespace mjv {

struct Camera3D {
    Vec3 position{0.0f, 10.0f, 12.0f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float fovy = 60.0f; // champ de vision vertical (degrés)
};

// Modèle 3D chargé depuis un fichier. Implémentation cachée (PIMPL).
class Model {
public:
    Model() = default;
    ~Model();
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&& o) noexcept;
    Model& operator=(Model&& o) noexcept;

    bool load(const std::string& path); // .obj / .gltf / .glb
    void unload();
    bool valid() const;

    // Dessine le modèle (échelle uniforme, rotation autour de l'axe Y en degrés).
    void draw(Vec3 position, float scale, float yawDeg, Color tint = Colors::White) const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

namespace Graphics3D {
void begin(const Camera3D& cam); // ouvre le mode 3D (dans onRender)
void end();                      // ferme le mode 3D

// Configure la lumière directionnelle (direction, couleur, lumière ambiante 0..1).
void setLight(Vec3 direction, Color color, float ambient);

void drawCube(Vec3 position, Vec3 size, Color c);
void drawCubeWires(Vec3 position, Vec3 size, Color c);
void drawSphere(Vec3 center, float radius, Color c);
void drawPlane(Vec3 center, float width, float depth, Color c);
void drawGrid(int slices, float spacing);
} // namespace Graphics3D

} // namespace mjv
