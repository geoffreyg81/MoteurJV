#pragma once

// Système d'entrées clavier. On expose nos propres touches (enum Key) pour ne
// pas fuiter les constantes de la lib bas niveau dans le code de jeu.
namespace mjv {

enum class Key {
    Up, Down, Left, Right,
    W, A, S, D, Z, Q, E,
    Space, Enter, Escape, Tab, M, P
};

namespace Input {
bool isDown(Key k);      // touche maintenue
bool isPressed(Key k);   // touche enfoncée à cette frame
bool isReleased(Key k);  // touche relâchée à cette frame
} // namespace Input

} // namespace mjv
