#pragma once

#include "Color.hpp"
#include "Math.hpp"
#include "Texture.hpp"

// Composants génériques fournis par le moteur. Ce ne sont QUE des données.
// Le Transform 2D vit déjà dans Math.hpp (mjv::Transform2D) et sert de
// composant de position/rotation/échelle.
namespace mjv {

// Vitesse linéaire (pixels/seconde). Intégrée par le système de mouvement.
struct Velocity {
    Vec2 value;
};

// Forme rectangulaire colorée (rendu de debug / blocs simples).
struct RectShape {
    Vec2 size{32.0f, 32.0f};
    Color color = Colors::White;
};

// Sprite : référence une Texture (possédée ailleurs, ex. un futur AssetManager).
// Le composant reste léger : juste un pointeur + une teinte. Position, échelle
// et orientation (échelle X négative = flip) viennent du Transform2D.
struct Sprite {
    const Texture* texture = nullptr;
    Color tint = Colors::White;
};

// Boîte de collision alignée sur les axes (AABB), exprimée en demi-tailles
// autour de la position du Transform.
struct AABB {
    Vec2 halfSize{16.0f, 16.0f};
};

// Corps physique (mini-physique maison). Une entité avec RigidBody + AABB est
// "dynamique" (soumise à la gravité) ; une AABB sans RigidBody est un solide
// statique (sol, plateforme, mur). onGround est mis à jour par le solveur.
struct RigidBody {
    Vec2 velocity{0.0f, 0.0f};
    float gravityScale = 1.0f;
    bool onGround = false;
};

} // namespace mjv
