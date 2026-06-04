#pragma once

// Système d'entrées (clavier + souris). On expose nos propres touches/boutons
// pour ne pas fuiter les constantes de la lib bas niveau dans le code de jeu.
#include "Math.hpp"

namespace mjv {

enum class Key {
    Up, Down, Left, Right,
    W, A, S, D, Z, Q, E,
    Space, Enter, Escape, Tab, M, P
};

enum class Mouse { Left, Right, Middle };

namespace Input {
bool isDown(Key k);      // touche maintenue
bool isPressed(Key k);   // touche enfoncée à cette frame
bool isReleased(Key k);  // touche relâchée à cette frame

Vec2 mousePosition();    // position du curseur (pixels écran)
Vec2 mouseDelta();       // déplacement du curseur depuis la frame précédente
bool mouseDown(Mouse b);
bool mousePressed(Mouse b);
bool mouseReleased(Mouse b);
float mouseWheel();      // défilement molette (+haut / -bas)
} // namespace Input

} // namespace mjv
