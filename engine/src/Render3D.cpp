#include "mjv/Render3D.hpp"

#include "raylib.h"

namespace {
inline ::Vector3 v3(mjv::Vec3 v) { return ::Vector3{v.x, v.y, v.z}; }
inline ::Color toRaylib(mjv::Color c) { return ::Color{c.r, c.g, c.b, c.a}; }
} // namespace

namespace mjv::Graphics3D {

void begin(const Camera3D& cam) {
    ::Camera3D rc{};
    rc.position = v3(cam.position);
    rc.target = v3(cam.target);
    rc.up = v3(cam.up);
    rc.fovy = cam.fovy;
    rc.projection = CAMERA_PERSPECTIVE;
    BeginMode3D(rc);
}

void end() { EndMode3D(); }

void drawCube(Vec3 p, Vec3 s, Color c) { DrawCubeV(v3(p), v3(s), toRaylib(c)); }
void drawCubeWires(Vec3 p, Vec3 s, Color c) { DrawCubeWiresV(v3(p), v3(s), toRaylib(c)); }
void drawSphere(Vec3 ctr, float r, Color c) { DrawSphere(v3(ctr), r, toRaylib(c)); }
void drawPlane(Vec3 ctr, float w, float d, Color c) { DrawPlane(v3(ctr), ::Vector2{w, d}, toRaylib(c)); }
void drawGrid(int slices, float spacing) { DrawGrid(slices, spacing); }

} // namespace mjv::Graphics3D
