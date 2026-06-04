#pragma once

#include <cmath>
#include <vector>

#include "Collision.hpp"
#include "Components.hpp"
#include "Math.hpp"
#include "ecs/Registry.hpp"

// Mini-physique 2D "maison" (gravité + collisions AABB). Volontairement simple
// et lisible : le but est de comprendre la mécanique avant d'envisager une vraie
// lib (Box2D). Le composant RigidBody pourra être rebranché sur b2Body plus tard
// sans changer le reste du moteur.
namespace mjv {

// Avance la physique d'un pas (dt secondes) sous une gravité donnée (px/s²).
//
//   - Corps dynamiques : entités avec RigidBody + AABB + Transform2D.
//   - Solides statiques : entités avec AABB + Transform2D mais SANS RigidBody.
//
// Résolution "axe par axe" (X puis Y) : c'est la technique classique des
// platformers, qui donne un comportement propre contre les murs et le sol, et
// permet de détecter l'appui au sol (onGround).
inline void physicsStep(Registry& reg, float dt, Vec2 gravity) {
    struct Box { Vec2 center; Vec2 half; bool oneWay; };
    std::vector<Box> statics;
    reg.view<AABB, Transform2D>([&](Entity e, AABB& box, Transform2D& tr) {
        if (!reg.has<RigidBody>(e)) statics.push_back({tr.position, box.halfSize, reg.has<OneWay>(e)});
    });

    // Corps dynamiques (pour la résolution dynamique <-> dynamique ensuite).
    struct Dyn { Transform2D* tr; Vec2 half; RigidBody* rb; };
    std::vector<Dyn> dyns;

    reg.view<RigidBody, AABB, Transform2D>([&](Entity, RigidBody& rb, AABB& box, Transform2D& tr) {
        rb.velocity += gravity * (rb.gravityScale * dt);

        // --- Déplacement horizontal puis résolution sur X ---
        tr.position.x += rb.velocity.x * dt;
        for (const Box& s : statics) {
            if (s.oneWay) continue; // traversable horizontalement
            if (aabbOverlap(tr.position, box.halfSize, s.center, s.half)) {
                const float dx = tr.position.x - s.center.x;
                const float ox = (box.halfSize.x + s.half.x) - std::fabs(dx);
                tr.position.x += (dx < 0.0f ? -ox : ox);
                rb.velocity.x = 0.0f;
            }
        }

        // --- Déplacement vertical puis résolution sur Y ---
        rb.onGround = false;
        tr.position.y += rb.velocity.y * dt;
        for (const Box& s : statics) {
            if (!aabbOverlap(tr.position, box.halfSize, s.center, s.half)) continue;
            const float dy = tr.position.y - s.center.y;
            const float oy = (box.halfSize.y + s.half.y) - std::fabs(dy);
            if (s.oneWay) {
                // one-way : on ne se pose que par le dessus, en descendant
                if (dy < 0.0f && rb.velocity.y >= 0.0f) {
                    tr.position.y -= oy;
                    rb.onGround = true;
                    rb.velocity.y = 0.0f;
                }
            } else {
                tr.position.y += (dy < 0.0f ? -oy : oy);
                if (dy < 0.0f) rb.onGround = true; // posé sur le dessus d'un solide
                rb.velocity.y = 0.0f;
            }
        }
        dyns.push_back({&tr, box.halfSize, &rb});
    });

    // --- Résolution dynamique <-> dynamique (les corps se poussent) ----------
    for (std::size_t i = 0; i < dyns.size(); ++i) {
        for (std::size_t j = i + 1; j < dyns.size(); ++j) {
            Dyn& a = dyns[i];
            Dyn& b = dyns[j];
            if (!aabbOverlap(a.tr->position, a.half, b.tr->position, b.half)) continue;
            const float dx = b.tr->position.x - a.tr->position.x;
            const float px = (a.half.x + b.half.x) - std::fabs(dx);
            const float dy = b.tr->position.y - a.tr->position.y;
            const float py = (a.half.y + b.half.y) - std::fabs(dy);
            if (px <= 0.0f || py <= 0.0f) continue;
            if (px < py) {
                const float s = (dx < 0.0f) ? 1.0f : -1.0f; // pousse a, oppose pour b
                a.tr->position.x += s * px * 0.5f;
                b.tr->position.x -= s * px * 0.5f;
                a.rb->velocity.x = 0.0f;
                b.rb->velocity.x = 0.0f;
            } else {
                const float s = (dy < 0.0f) ? 1.0f : -1.0f;
                a.tr->position.y += s * py * 0.5f;
                b.tr->position.y -= s * py * 0.5f;
                // celui du dessus est "au sol" (posé sur l'autre)
                if (dy > 0.0f) a.rb->onGround = true; else b.rb->onGround = true;
                a.rb->velocity.y = 0.0f;
                b.rb->velocity.y = 0.0f;
            }
        }
    }
}

} // namespace mjv
