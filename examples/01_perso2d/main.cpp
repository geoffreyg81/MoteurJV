// Démo 01 — Platformer 2D : un personnage animé, gravité, saut et plateformes.
//
// Montre le moteur MoteurJV de bout en bout : ECS, mini-physique maison
// (gravité + collisions AABB), sprites animés, audio. Tout passe par l'API mjv::.

#include <cmath>
#include <string>
#include <utility>

#include "mjv/mjv.hpp"

#ifndef MJV_ASSET_DIR
#define MJV_ASSET_DIR "assets"
#endif

using namespace mjv;

namespace {
constexpr float kGravity = 1800.0f; // px/s²
}

// --- Composants spécifiques au jeu -------------------------------------------
struct PlayerControl {
    float speed = 240.0f;      // vitesse horizontale (px/s)
    float jumpSpeed = 720.0f;  // impulsion de saut (px/s)
    bool faceLeft = false;
    bool walking = false;      // au sol et en mouvement (pour l'animation)
};

struct HeroTag {}; // repli : dessin géométrique si pas de sprite

// --- Systems ------------------------------------------------------------------

// Déplacement horizontal + orientation. Le saut est géré dans onUpdate (il a
// besoin du son et de l'état au sol).
void inputSystem(Registry& reg) {
    reg.view<PlayerControl, RigidBody, Transform2D>([](Entity, PlayerControl& pc, RigidBody& rb, Transform2D& tr) {
        float move = 0.0f;
        if (Input::isDown(Key::Left)  || Input::isDown(Key::A) || Input::isDown(Key::Q)) move -= 1.0f;
        if (Input::isDown(Key::Right) || Input::isDown(Key::D))                          move += 1.0f;

        rb.velocity.x = move * pc.speed;
        if (move != 0.0f) pc.faceLeft = move < 0.0f;
        tr.scale.x = pc.faceLeft ? -1.0f : 1.0f; // flip horizontal du sprite
        tr.scale.y = 1.0f;
        pc.walking = (move != 0.0f) && rb.onGround;
    });
}

void animatorControlSystem(Registry& reg) {
    reg.view<PlayerControl, Animator>([](Entity, PlayerControl& pc, Animator& a) {
        a.play(pc.walking ? "walk" : "idle");
    });
}

void animationSystem(Registry& reg, float dt) {
    reg.view<Animator>([&](Entity, Animator& a) {
        const AnimationClip* c = a.clip();
        if (!c || c->frameCount <= 1 || c->frameDelay <= 0.0f) return;
        a.timer += dt;
        while (a.timer >= c->frameDelay) {
            a.timer -= c->frameDelay;
            if (++a.frame >= c->frameCount) {
                a.frame = c->loop ? 0 : c->frameCount - 1;
                if (!c->loop) a.finished = true;
            }
        }
    });
}

// Dessine les solides (RectShape : sol, plateformes) puis, en repli, le héros
// géométrique s'il n'a pas de sprite.
void renderSystem(Registry& reg, float time) {
    reg.view<Transform2D, RectShape>([](Entity, Transform2D& tr, RectShape& shape) {
        Graphics::drawRectangleCentered(tr.position, shape.size, shape.color, tr.rotation);
    });

    reg.view<HeroTag, Transform2D, PlayerControl>([&](Entity, HeroTag&, Transform2D& tr, PlayerControl& pc) {
        const float bob = pc.walking ? std::sin(time * 12.0f) * 3.0f : 0.0f;
        const Vec2 c{tr.position.x, tr.position.y + bob};
        Graphics::drawCircle({c.x, c.y + 28.0f}, 18.0f, Color{0, 0, 0, 60});
        Graphics::drawRectangleCentered({c.x, c.y}, {34.0f, 44.0f}, Colors::Red);
        Graphics::drawCircle({c.x, c.y - 34.0f}, 16.0f, Color{255, 220, 180, 255});
        const float ex = pc.faceLeft ? -6.0f : 6.0f;
        Graphics::drawCircle({c.x + ex, c.y - 36.0f}, 3.0f, Colors::Black);
    });
}

// Dessine les entités avec Sprite ; si Animator, découpe la frame courante.
void spriteRenderSystem(Registry& reg) {
    reg.view<Transform2D, Sprite>([&](Entity e, Transform2D& tr, Sprite& spr) {
        if (!spr.texture) return;
        Graphics::drawCircle({tr.position.x, tr.position.y + 30.0f}, 16.0f, Color{0, 0, 0, 60});
        if (reg.has<Animator>(e)) {
            Animator& a = reg.get<Animator>(e);
            const AnimationClip* c = a.clip();
            const int row = c ? c->row : 0;
            const Rect src{static_cast<float>(a.frame * a.frameWidth),
                           static_cast<float>(row * a.frameHeight),
                           static_cast<float>(a.frameWidth),
                           static_cast<float>(a.frameHeight)};
            spr.texture->drawRegion(tr, src, spr.tint);
        } else {
            spr.texture->draw(tr, spr.tint);
        }
    });
}

void debugDrawSystem(Registry& reg) {
    reg.view<AABB, Transform2D>([](Entity, AABB& box, Transform2D& tr) {
        Graphics::drawRectangleOutlineCentered(tr.position, box.halfSize * 2.0f,
                                               Color{255, 0, 80, 220}, 2.0f);
    });
}

// --- L'application ------------------------------------------------------------
class Jeu : public Application {
public:
    Jeu() : Application(makeConfig()) {}

private:
    static Config makeConfig() {
        Config c;
        c.width = 960;
        c.height = 540;
        c.title = "MoteurJV - Platformer 2D";
        c.clearColor = Colors::SkyBlue;
        return c;
    }

    Registry m_reg;
    Texture m_heroTex;
    Sound m_step;
    Sound m_jump;
    Sound m_land;
    Music m_music;
    Entity m_hero = NullEntity;
    float m_time = 0.0f;
    float m_stepTimer = 0.0f;
    bool m_debug = false;
    bool m_wasOnGround = true;
    bool m_musicOn = true;

    // Crée un solide statique (sol, plateforme) : Transform + RectShape + AABB.
    void spawnSolid(Vec2 center, Vec2 size, Color color) {
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{center});
        m_reg.add<RectShape>(e, RectShape{size, color});
        m_reg.add<AABB>(e, AABB{size * 0.5f});
    }

    void onStart() override {
        const Vec2 ws = windowSize();
        const Color brown{150, 110, 70, 255};

        // Sol + plateformes à sauter.
        spawnSolid({ws.x * 0.5f, ws.y - 24.0f}, {ws.x, 48.0f}, Colors::Green);
        spawnSolid({250.0f, ws.y - 150.0f}, {160.0f, 24.0f}, brown);
        spawnSolid({560.0f, ws.y - 250.0f}, {150.0f, 24.0f}, brown);
        spawnSolid({780.0f, ws.y - 150.0f}, {150.0f, 24.0f}, brown);

        // Le héros : une entité = des composants.
        m_hero = m_reg.create();
        m_reg.add<Transform2D>(m_hero, Transform2D{{ws.x * 0.5f, 120.0f}, 0.0f, {1, 1}});
        m_reg.add<RigidBody>(m_hero, RigidBody{});
        m_reg.add<PlayerControl>(m_hero, PlayerControl{});
        m_reg.add<AABB>(m_hero, AABB{{20.0f, 30.0f}});

        const bool hasSprite = m_heroTex.load(std::string(MJV_ASSET_DIR) + "/hero_sheet.png");
        if (hasSprite) {
            m_reg.add<Sprite>(m_hero, Sprite{&m_heroTex});
            Animator anim;
            anim.frameWidth = 48;
            anim.frameHeight = 64;
            anim.add("idle", AnimationClip{0, 2, 0.40f, true});
            anim.add("walk", AnimationClip{1, 4, 0.12f, true});
            anim.play("idle");
            m_reg.add<Animator>(m_hero, std::move(anim));
        } else {
            m_reg.add<HeroTag>(m_hero);
        }

        // Audio (le périphérique est initialisé par Application).
        const std::string dir = std::string(MJV_ASSET_DIR) + "/";
        m_step.load(dir + "step.wav");
        m_step.setVolume(0.5f);
        m_jump.load(dir + "jump.wav");
        m_land.load(dir + "bump.wav");
        if (m_music.load(dir + "music.wav")) {
            m_music.setLooping(true);
            m_music.setVolume(0.6f);
            m_music.play();
        }
    }

    void onUpdate(float dt) override {
        m_time += dt;
        if (Input::isPressed(Key::Tab)) m_debug = !m_debug;
        if (Input::isPressed(Key::P) || Input::isPressed(Key::M)) {
            m_musicOn = !m_musicOn;
            if (m_musicOn) m_music.resume(); else m_music.pause();
        }

        inputSystem(m_reg);

        // Saut : seulement si au sol (état issu du pas de physique précédent).
        RigidBody& rb = m_reg.get<RigidBody>(m_hero);
        const bool jumpPressed = Input::isPressed(Key::Space) || Input::isPressed(Key::Up) ||
                                 Input::isPressed(Key::W) || Input::isPressed(Key::Z);
        if (jumpPressed && rb.onGround) {
            rb.velocity.y = -m_reg.get<PlayerControl>(m_hero).jumpSpeed;
            m_jump.play();
        }

        animatorControlSystem(m_reg);
        physicsStep(m_reg, dt, {0.0f, kGravity});
        animationSystem(m_reg, dt);

        // Garder le héros dans l'écran horizontalement.
        Transform2D& tr = m_reg.get<Transform2D>(m_hero);
        const Vec2 ws = windowSize();
        if (tr.position.x < 24.0f)        tr.position.x = 24.0f;
        if (tr.position.x > ws.x - 24.0f) tr.position.x = ws.x - 24.0f;

        // --- Sons liés au mouvement ---
        const bool onGround = rb.onGround;
        if (m_reg.get<PlayerControl>(m_hero).walking) {
            m_stepTimer -= dt;
            if (m_stepTimer <= 0.0f) {
                m_step.play();
                m_stepTimer = 0.3f;
            }
        } else {
            m_stepTimer = 0.0f;
        }
        if (onGround && !m_wasOnGround) m_land.play(); // atterrissage
        m_wasOnGround = onGround;
    }

    void onRender() override {
        renderSystem(m_reg, m_time);
        spriteRenderSystem(m_reg);
        if (m_debug) debugDrawSystem(m_reg);
        Graphics::drawText("Platformer : ECS + physique + sprite + audio", {16.0f, 16.0f}, 20, Colors::Black);
        Graphics::drawText("Gauche/Droite : ZQSD ou Fleches   Saut : Espace", {16.0f, 40.0f}, 18, Colors::DarkGray);
        Graphics::drawText("Tab : collisions   P : musique", {16.0f, 62.0f}, 18, Colors::DarkGray);
        Graphics::drawText("MoteurJV v0.7", {16.0f, windowSize().y - 28.0f}, 18, Colors::White);
    }
};

int main() {
    Jeu jeu;
    jeu.run();
    return 0;
}
