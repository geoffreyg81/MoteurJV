#include "mjv/Input.hpp"

#include "raylib.h"

namespace mjv {
namespace {

int toRaylibKey(Key k) {
    switch (k) {
        case Key::Up:     return KEY_UP;
        case Key::Down:   return KEY_DOWN;
        case Key::Left:   return KEY_LEFT;
        case Key::Right:  return KEY_RIGHT;
        case Key::W:      return KEY_W;
        case Key::A:      return KEY_A;
        case Key::S:      return KEY_S;
        case Key::D:      return KEY_D;
        case Key::Z:      return KEY_Z;
        case Key::Q:      return KEY_Q;
        case Key::E:      return KEY_E;
        case Key::Space:  return KEY_SPACE;
        case Key::Enter:  return KEY_ENTER;
        case Key::Escape: return KEY_ESCAPE;
        case Key::Tab:    return KEY_TAB;
        case Key::M:      return KEY_M;
        case Key::P:      return KEY_P;
    }
    return 0;
}

} // namespace

namespace Input {

bool isDown(Key k)     { return IsKeyDown(toRaylibKey(k)); }
bool isPressed(Key k)  { return IsKeyPressed(toRaylibKey(k)); }
bool isReleased(Key k) { return IsKeyReleased(toRaylibKey(k)); }

} // namespace Input
} // namespace mjv
