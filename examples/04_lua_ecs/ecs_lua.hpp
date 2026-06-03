#pragma once

// Binding du moteur MoteurJV vers Lua (via sol2).
//
// IMPORTANT : ce binding vit côté APPLICATION, pas dans le moteur — le cœur
// `mjv` ne dépend pas de Lua. C'est ici qu'on "traduit" l'ECS typé C++ en une
// API par chaînes utilisable depuis Lua : reg:create(), reg:add(e, "RigidBody"),
// reg:get(e, "Transform2D"), reg:view2("Transform2D", "RectShape", fn)...

#include <functional>
#include <string>

#include <sol/sol.hpp>

#include "mjv/mjv.hpp"

namespace mjvlua {

using namespace mjv;

// Petit wrapper pour permettre la syntaxe Lua `reg:methode(...)`.
struct LuaReg {
    Registry* reg;
};

inline bool keyDown(const std::string& k) {
    if (k == "left")  return Input::isDown(Key::Left) || Input::isDown(Key::A) || Input::isDown(Key::Q);
    if (k == "right") return Input::isDown(Key::Right) || Input::isDown(Key::D);
    if (k == "up")    return Input::isDown(Key::Up) || Input::isDown(Key::W) || Input::isDown(Key::Z);
    if (k == "down")  return Input::isDown(Key::Down) || Input::isDown(Key::S);
    if (k == "space") return Input::isDown(Key::Space);
    return false;
}
inline bool keyPressed(const std::string& k) {
    if (k == "space") return Input::isPressed(Key::Space);
    if (k == "enter") return Input::isPressed(Key::Enter);
    if (k == "up")    return Input::isPressed(Key::Up) || Input::isPressed(Key::W) || Input::isPressed(Key::Z);
    return false;
}

// Expose tout le moteur nécessaire à Lua : types, composants, ECS, physique,
// rendu, entrées.
inline void bindEngineLua(sol::state& lua, Registry& reg, int sw, int sh) {
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);

    // --- Types de base ---
    lua.new_usertype<Vec2>("Vec2",
        "new", sol::factories([](float x, float y) { return Vec2{x, y}; }, []() { return Vec2{}; }),
        "x", &Vec2::x, "y", &Vec2::y);

    lua.new_usertype<Color>("Color",
        "new", sol::factories([](int r, int g, int b) {
            return Color{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                         static_cast<std::uint8_t>(b), 255};
        }),
        "r", &Color::r, "g", &Color::g, "b", &Color::b, "a", &Color::a);

    // --- Composants (données pures) ---
    lua.new_usertype<Transform2D>("Transform2D",
        "position", &Transform2D::position, "rotation", &Transform2D::rotation, "scale", &Transform2D::scale);
    lua.new_usertype<Velocity>("Velocity", "value", &Velocity::value);
    lua.new_usertype<RectShape>("RectShape", "size", &RectShape::size, "color", &RectShape::color);
    lua.new_usertype<AABB>("AABB", "halfSize", &AABB::halfSize);
    lua.new_usertype<RigidBody>("RigidBody",
        "velocity", &RigidBody::velocity, "gravityScale", &RigidBody::gravityScale, "onGround", &RigidBody::onGround);

    // --- Dispatch composant par nom (le pont typé C++ <-> chaînes Lua) ---
    auto addComp = [&reg, &lua](Entity e, const std::string& n) -> sol::object {
        if (n == "Transform2D") return sol::make_object(lua, std::ref(reg.add<Transform2D>(e)));
        if (n == "Velocity")    return sol::make_object(lua, std::ref(reg.add<Velocity>(e)));
        if (n == "RectShape")   return sol::make_object(lua, std::ref(reg.add<RectShape>(e)));
        if (n == "AABB")        return sol::make_object(lua, std::ref(reg.add<AABB>(e)));
        if (n == "RigidBody")   return sol::make_object(lua, std::ref(reg.add<RigidBody>(e)));
        return sol::lua_nil;
    };
    auto getComp = [&reg, &lua](Entity e, const std::string& n) -> sol::object {
        if (n == "Transform2D") return sol::make_object(lua, std::ref(reg.get<Transform2D>(e)));
        if (n == "Velocity")    return sol::make_object(lua, std::ref(reg.get<Velocity>(e)));
        if (n == "RectShape")   return sol::make_object(lua, std::ref(reg.get<RectShape>(e)));
        if (n == "AABB")        return sol::make_object(lua, std::ref(reg.get<AABB>(e)));
        if (n == "RigidBody")   return sol::make_object(lua, std::ref(reg.get<RigidBody>(e)));
        return sol::lua_nil;
    };
    auto hasComp = [&reg](Entity e, const std::string& n) -> bool {
        if (n == "Transform2D") return reg.has<Transform2D>(e);
        if (n == "Velocity")    return reg.has<Velocity>(e);
        if (n == "RectShape")   return reg.has<RectShape>(e);
        if (n == "AABB")        return reg.has<AABB>(e);
        if (n == "RigidBody")   return reg.has<RigidBody>(e);
        return false;
    };
    auto removeComp = [&reg](Entity e, const std::string& n) {
        if (n == "Transform2D") reg.remove<Transform2D>(e);
        else if (n == "Velocity")  reg.remove<Velocity>(e);
        else if (n == "RectShape") reg.remove<RectShape>(e);
        else if (n == "AABB")      reg.remove<AABB>(e);
        else if (n == "RigidBody") reg.remove<RigidBody>(e);
    };
    auto eachEntity = [&reg](const std::string& n, const std::function<void(Entity)>& cb) {
        if (n == "Transform2D") reg.view<Transform2D>([&](Entity e, Transform2D&) { cb(e); });
        else if (n == "Velocity")  reg.view<Velocity>([&](Entity e, Velocity&) { cb(e); });
        else if (n == "RectShape") reg.view<RectShape>([&](Entity e, RectShape&) { cb(e); });
        else if (n == "AABB")      reg.view<AABB>([&](Entity e, AABB&) { cb(e); });
        else if (n == "RigidBody") reg.view<RigidBody>([&](Entity e, RigidBody&) { cb(e); });
    };

    // --- L'objet `reg` (Registry) côté Lua ---
    lua.new_usertype<LuaReg>("Registry",
        "create", [](LuaReg& s) { return s.reg->create(); },
        "destroy", [](LuaReg& s, Entity e) { s.reg->destroy(e); },
        "valid", [](LuaReg& s, Entity e) { return s.reg->valid(e); },
        "add", [addComp](LuaReg&, Entity e, const std::string& n) { return addComp(e, n); },
        "get", [getComp](LuaReg&, Entity e, const std::string& n) { return getComp(e, n); },
        "has", [hasComp](LuaReg&, Entity e, const std::string& n) { return hasComp(e, n); },
        "remove", [removeComp](LuaReg&, Entity e, const std::string& n) { removeComp(e, n); },
        "view", [eachEntity, getComp](LuaReg&, const std::string& n, sol::protected_function f) {
            eachEntity(n, [&](Entity e) { f(e, getComp(e, n)); });
        },
        "view2", [eachEntity, getComp, hasComp](LuaReg&, const std::string& a, const std::string& b,
                                                sol::protected_function f) {
            eachEntity(a, [&](Entity e) {
                if (hasComp(e, b)) f(e, getComp(e, a), getComp(e, b));
            });
        });
    lua["reg"] = LuaReg{&reg};

    // --- Moteur : physique ---
    auto m = lua.create_named_table("mjv");
    m.set_function("physics", [&reg](float dt, float gx, float gy) {
        physicsStep(reg, dt, {gx, gy});
    });

    // --- Rendu ---
    auto g = lua.create_named_table("Graphics");
    g.set_function("rect", [](float x, float y, float w, float h, int r, int gg, int b) {
        Graphics::drawRectangle({x, y}, {w, h},
                                Color{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(gg),
                                      static_cast<std::uint8_t>(b), 255});
    });
    g.set_function("circle", [](float x, float y, float rad, int r, int gg, int b) {
        Graphics::drawCircle({x, y}, rad,
                             Color{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(gg),
                                   static_cast<std::uint8_t>(b), 255});
    });
    g.set_function("text", [](const std::string& s, float x, float y, int size, int r, int gg, int b) {
        Graphics::drawText(s, {x, y}, size,
                           Color{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(gg),
                                 static_cast<std::uint8_t>(b), 255});
    });
    g.set_function("outline", [](float x, float y, float w, float h, int r, int gg, int b) {
        Graphics::drawRectangleOutlineCentered({x + w / 2.0f, y + h / 2.0f}, {w, h},
                                               Color{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(gg),
                                                     static_cast<std::uint8_t>(b), 255}, 2.0f);
    });

    // --- Entrées ---
    auto in = lua.create_named_table("Input");
    in.set_function("down", &keyDown);
    in.set_function("pressed", &keyPressed);

    lua["SCREEN_W"] = static_cast<float>(sw);
    lua["SCREEN_H"] = static_cast<float>(sh);
}

} // namespace mjvlua
