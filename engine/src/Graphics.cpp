#include "mjv/Graphics.hpp"

#include "raylib.h"

namespace {
inline ::Color toRaylib(mjv::Color c) {
    return ::Color{c.r, c.g, c.b, c.a};
}
} // namespace

namespace mjv::Graphics {

void clear(Color c) {
    ClearBackground(toRaylib(c));
}

void drawRectangle(Vec2 pos, Vec2 size, Color c) {
    DrawRectangleV(::Vector2{pos.x, pos.y}, ::Vector2{size.x, size.y}, toRaylib(c));
}

void drawRectangleCentered(Vec2 center, Vec2 size, Color c, float rotationDeg) {
    const ::Rectangle rec{center.x, center.y, size.x, size.y};
    const ::Vector2 origin{size.x * 0.5f, size.y * 0.5f};
    DrawRectanglePro(rec, origin, rotationDeg, toRaylib(c));
}

void drawCircle(Vec2 center, float radius, Color c) {
    DrawCircleV(::Vector2{center.x, center.y}, radius, toRaylib(c));
}

void drawRectangleOutlineCentered(Vec2 center, Vec2 size, Color c, float thickness) {
    const ::Rectangle rec{center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y};
    DrawRectangleLinesEx(rec, thickness, toRaylib(c));
}

void drawText(const std::string& text, Vec2 pos, int fontSize, Color c) {
    DrawText(text.c_str(), static_cast<int>(pos.x), static_cast<int>(pos.y), fontSize, toRaylib(c));
}

} // namespace mjv::Graphics
