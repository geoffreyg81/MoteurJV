#pragma once

// Types mathematiques de base du moteur MoteurJV.
// On reste volontairement minimal : un Vec2 et un Transform 2D.

namespace mjv {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
};

// Position / rotation (degres) / echelle d'un objet dans la scene 2D.
struct Transform2D {
    Vec2 position;
    float rotation = 0.0f;   // en degres
    Vec2 scale{1.0f, 1.0f};
};

// Rectangle (coin haut-gauche + taille), en pixels. Sert notamment a designer
// une region source dans une spritesheet.
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

} // namespace mjv
