#pragma once

#include <string>

#include "Color.hpp"
#include "Math.hpp"

// Façade de rendu 2D du moteur. C'est l'unique surface de dessin exposée
// au code de jeu : on cache complètement la stack graphique (raylib) derrière.
namespace mjv::Graphics {

void clear(Color c);

void drawRectangle(Vec2 pos, Vec2 size, Color c);                          // coin haut-gauche
void drawRectangleCentered(Vec2 center, Vec2 size, Color c, float rotationDeg = 0.0f);
void drawCircle(Vec2 center, float radius, Color c);
void drawRectangleOutlineCentered(Vec2 center, Vec2 size, Color c, float thickness = 2.0f);
void drawText(const std::string& text, Vec2 pos, int fontSize, Color c);

void screenshot(const std::string& path); // enregistre une capture PNG

} // namespace mjv::Graphics
