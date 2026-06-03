// Exemple 02 — Un petit jeu complet avec MoteurJV.
//
// Un platformer de 3 niveaux : ramasse les pièces, saute sur les ennemis pour
// les éliminer, évite-les sur le côté, et atteins le drapeau. Score + vies.
//
// Objectif : se servir vraiment du moteur de bout en bout pour révéler ses
// manques. Tout passe par l'API mjv:: (ECS, physique, sprites, audio).

#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "mjv/mjv.hpp"

#ifndef MJV_ASSET_DIR
#define MJV_ASSET_DIR "assets"
#endif

using namespace mjv;

namespace {
constexpr float kGravity = 1800.0f;
constexpr int kLevelCount = 3;
} // namespace

// --- Composants du jeu --------------------------------------------------------
struct PlayerControl {
    float speed = 240.0f;
    float jumpSpeed = 720.0f;
    bool faceLeft = false;
    bool walking = false;
};
struct Enemy {
    Vec2 half{18.0f, 16.0f};
    float minX = 0.0f, maxX = 0.0f;
    float dir = 1.0f;
    float speed = 80.0f;
};
struct Coin {
    float radius = 12.0f;
};
struct Goal {
    Vec2 half{14.0f, 42.0f};
};

// --- Description d'un niveau (données pures) ----------------------------------
struct Solid { Vec2 center; Vec2 size; Color color; };
struct EnemySpawn { Vec2 pos; float minX; float maxX; };
struct LevelData {
    Vec2 start;
    std::vector<Solid> solids;
    std::vector<EnemySpawn> enemies;
    std::vector<Vec2> coins;
    Vec2 goal;
};

// --- Systèmes sans état (fonctions libres) ------------------------------------
void inputSystem(Registry& reg) {
    reg.view<PlayerControl, RigidBody, Transform2D>([](Entity, PlayerControl& pc, RigidBody& rb, Transform2D& tr) {
        float move = 0.0f;
        if (Input::isDown(Key::Left)  || Input::isDown(Key::A) || Input::isDown(Key::Q)) move -= 1.0f;
        if (Input::isDown(Key::Right) || Input::isDown(Key::D))                          move += 1.0f;
        rb.velocity.x = move * pc.speed;
        if (move != 0.0f) pc.faceLeft = move < 0.0f;
        tr.scale.x = pc.faceLeft ? -1.0f : 1.0f;
        tr.scale.y = 1.0f;
        pc.walking = (move != 0.0f) && rb.onGround;
    });
}

void enemySystem(Registry& reg, float dt) {
    reg.view<Enemy, Transform2D>([&](Entity, Enemy& en, Transform2D& tr) {
        tr.position.x += en.dir * en.speed * dt;
        if (tr.position.x < en.minX) { tr.position.x = en.minX; en.dir = 1.0f; }
        if (tr.position.x > en.maxX) { tr.position.x = en.maxX; en.dir = -1.0f; }
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
            if (++a.frame >= c->frameCount) a.frame = c->loop ? 0 : c->frameCount - 1;
        }
    });
}

// --- L'application / le jeu ----------------------------------------------------
class Jeu : public Application {
public:
    Jeu() : Application(makeConfig()) {}

private:
    enum class State { Playing, Win, GameOver };

    static Config makeConfig() {
        Config c;
        c.width = 960;
        c.height = 540;
        c.title = "MoteurJV - Petit jeu (platformer)";
        c.clearColor = Colors::SkyBlue;
        return c;
    }

    Registry m_reg;
    Texture m_heroTex;
    Sound m_step, m_jump, m_land, m_coin, m_hit;
    Music m_music;

    Entity m_player = NullEntity;
    State m_state = State::Playing;
    int m_level = 0;
    int m_score = 0;
    int m_lives = 3;
    float m_time = 0.0f;
    float m_stepTimer = 0.0f;
    bool m_wasOnGround = true;
    bool m_debug = false;
    bool m_musicOn = true;

    // ----- mise en place -----
    void onStart() override {
        const std::string dir = std::string(MJV_ASSET_DIR) + "/";
        m_heroTex.load(dir + "hero_sheet.png");
        m_step.load(dir + "step.wav"); m_step.setVolume(0.5f);
        m_jump.load(dir + "jump.wav");
        m_land.load(dir + "bump.wav");
        m_coin.load(dir + "coin.wav");
        m_hit.load(dir + "bump.wav"); m_hit.setVolume(0.9f);
        if (m_music.load(dir + "music.wav")) { m_music.setLooping(true); m_music.setVolume(0.5f); m_music.play(); }

        restartGame();
    }

    void restartGame() {
        m_level = 0; m_score = 0; m_lives = 3; m_state = State::Playing;
        loadLevel(m_level);
    }

    LevelData makeLevel(int idx) const {
        const Vec2 ws = windowSize();
        const float gTop = ws.y - 48.0f;        // haut du sol
        const float eY = gTop - 16.0f;          // centre vertical d'un ennemi au sol
        const Color ground = Colors::Green;
        const Color brown{150, 110, 70, 255};
        LevelData lv;
        lv.start = {80.0f, ws.y - 200.0f};
        lv.solids.push_back({{ws.x * 0.5f, ws.y - 24.0f}, {ws.x, 48.0f}, ground}); // sol

        if (idx == 0) {
            lv.solids.push_back({{320.0f, 400.0f}, {150.0f, 24.0f}, brown});
            lv.solids.push_back({{580.0f, 320.0f}, {150.0f, 24.0f}, brown});
            lv.enemies.push_back({{520.0f, eY}, 430.0f, 700.0f});
            lv.coins = {{320.0f, 360.0f}, {580.0f, 280.0f}, {760.0f, eY - 10.0f}};
            lv.goal = {900.0f, gTop - 42.0f};
        } else if (idx == 1) {
            lv.solids.push_back({{240.0f, 410.0f}, {130.0f, 24.0f}, brown});
            lv.solids.push_back({{470.0f, 330.0f}, {130.0f, 24.0f}, brown});
            lv.solids.push_back({{700.0f, 250.0f}, {130.0f, 24.0f}, brown});
            lv.enemies.push_back({{300.0f, eY}, 180.0f, 420.0f});
            lv.enemies.push_back({{640.0f, eY}, 560.0f, 820.0f});
            lv.coins = {{240.0f, 370.0f}, {470.0f, 290.0f}, {700.0f, 210.0f}, {840.0f, eY - 10.0f}};
            lv.goal = {880.0f, gTop - 42.0f};
        } else {
            lv.solids.push_back({{180.0f, 400.0f}, {110.0f, 24.0f}, brown});
            lv.solids.push_back({{380.0f, 300.0f}, {110.0f, 24.0f}, brown});
            lv.solids.push_back({{600.0f, 360.0f}, {110.0f, 24.0f}, brown});
            lv.solids.push_back({{800.0f, 240.0f}, {120.0f, 24.0f}, brown});
            lv.enemies.push_back({{250.0f, eY}, 120.0f, 360.0f});
            lv.enemies.push_back({{500.0f, eY}, 420.0f, 700.0f});
            lv.enemies.push_back({{380.0f, 300.0f - 16.0f}, 330.0f, 430.0f});
            lv.coins = {{180.0f, 360.0f}, {380.0f, 260.0f}, {600.0f, 320.0f}, {800.0f, 200.0f}, {870.0f, eY - 10.0f}};
            lv.goal = {880.0f, 240.0f - 12.0f - 42.0f};
        }
        return lv;
    }

    void loadLevel(int idx) {
        m_reg.clear();
        const LevelData lv = makeLevel(idx);

        for (const Solid& s : lv.solids) {
            Entity e = m_reg.create();
            m_reg.add<Transform2D>(e, Transform2D{s.center});
            m_reg.add<RectShape>(e, RectShape{s.size, s.color});
            m_reg.add<AABB>(e, AABB{s.size * 0.5f});
        }
        for (const EnemySpawn& es : lv.enemies) {
            Entity e = m_reg.create();
            m_reg.add<Transform2D>(e, Transform2D{es.pos});
            Enemy en; en.minX = es.minX; en.maxX = es.maxX;
            m_reg.add<Enemy>(e, en);
        }
        for (const Vec2& c : lv.coins) {
            Entity e = m_reg.create();
            m_reg.add<Transform2D>(e, Transform2D{c});
            m_reg.add<Coin>(e, Coin{});
        }
        {
            Entity e = m_reg.create();
            m_reg.add<Transform2D>(e, Transform2D{lv.goal});
            m_reg.add<Goal>(e, Goal{});
        }
        createPlayer(lv.start);
    }

    void createPlayer(Vec2 start) {
        m_player = m_reg.create();
        m_reg.add<Transform2D>(m_player, Transform2D{start, 0.0f, {1, 1}});
        m_reg.add<RigidBody>(m_player, RigidBody{});
        m_reg.add<PlayerControl>(m_player, PlayerControl{});
        m_reg.add<AABB>(m_player, AABB{{20.0f, 30.0f}});
        if (m_heroTex.valid()) {
            m_reg.add<Sprite>(m_player, Sprite{&m_heroTex});
            Animator anim;
            anim.frameWidth = 48; anim.frameHeight = 64;
            anim.add("idle", AnimationClip{0, 2, 0.40f, true});
            anim.add("walk", AnimationClip{1, 4, 0.12f, true});
            anim.play("idle");
            m_reg.add<Animator>(m_player, std::move(anim));
        }
    }

    // ----- boucle -----
    void onUpdate(float dt) override {
        m_time += dt;
        if (Input::isPressed(Key::Tab)) m_debug = !m_debug;
        if (Input::isPressed(Key::P) || Input::isPressed(Key::M)) {
            m_musicOn = !m_musicOn;
            if (m_musicOn) m_music.resume(); else m_music.pause();
        }

        if (m_state != State::Playing) {
            if (Input::isPressed(Key::Enter)) restartGame();
            return;
        }

        inputSystem(m_reg);

        RigidBody& rb = m_reg.get<RigidBody>(m_player);
        const bool jumpPressed = Input::isPressed(Key::Space) || Input::isPressed(Key::Up) ||
                                 Input::isPressed(Key::W) || Input::isPressed(Key::Z);
        if (jumpPressed && rb.onGround) { rb.velocity.y = -m_reg.get<PlayerControl>(m_player).jumpSpeed; m_jump.play(); }

        animatorControlSystem(m_reg);
        enemySystem(m_reg, dt);
        physicsStep(m_reg, dt, {0.0f, kGravity});
        animationSystem(m_reg, dt);

        const Vec2 ws = windowSize();
        Transform2D& ptr = m_reg.get<Transform2D>(m_player);
        if (ptr.position.x < 24.0f)        ptr.position.x = 24.0f;
        if (ptr.position.x > ws.x - 24.0f) ptr.position.x = ws.x - 24.0f;
        if (ptr.position.y > ws.y + 120.0f) loseLife(); // tombé dans le vide

        handleCoins();
        handleEnemies();
        handleGoal();

        // Sons de déplacement.
        if (m_reg.get<PlayerControl>(m_player).walking) {
            m_stepTimer -= dt;
            if (m_stepTimer <= 0.0f) { m_step.play(); m_stepTimer = 0.3f; }
        } else {
            m_stepTimer = 0.0f;
        }
        const bool onGround = m_reg.get<RigidBody>(m_player).onGround;
        if (onGround && !m_wasOnGround) m_land.play();
        m_wasOnGround = onGround;
    }

    // ----- gameplay -----
    Vec2 playerPos() { return m_reg.get<Transform2D>(m_player).position; }
    Vec2 playerHalf() { return m_reg.get<AABB>(m_player).halfSize; }

    void handleCoins() {
        const Vec2 pp = playerPos();
        const Vec2 ph = playerHalf();
        std::vector<Entity> taken;
        m_reg.view<Coin, Transform2D>([&](Entity e, Coin& c, Transform2D& tr) {
            if (aabbOverlap(pp, ph, tr.position, {c.radius, c.radius})) taken.push_back(e);
        });
        for (Entity e : taken) { m_reg.destroy(e); m_score += 100; m_coin.play(); }
    }

    void handleEnemies() {
        const Vec2 ph = playerHalf();
        RigidBody& rb = m_reg.get<RigidBody>(m_player);
        const Vec2 pp = m_reg.get<Transform2D>(m_player).position;
        std::vector<Entity> killed;
        bool hurt = false;
        m_reg.view<Enemy, Transform2D>([&](Entity e, Enemy& en, Transform2D& tr) {
            if (!aabbOverlap(pp, ph, tr.position, en.half)) return;
            // Saut sur la tête : le joueur tombe et son centre est au-dessus de l'ennemi.
            if (rb.velocity.y > 0.0f && pp.y < tr.position.y) {
                killed.push_back(e);
            } else {
                hurt = true;
            }
        });
        for (Entity e : killed) { m_reg.destroy(e); m_score += 200; rb.velocity.y = -460.0f; m_jump.play(); }
        if (hurt) loseLife();
    }

    void handleGoal() {
        const Vec2 pp = playerPos();
        const Vec2 ph = playerHalf();
        bool reached = false;
        m_reg.view<Goal, Transform2D>([&](Entity, Goal& g, Transform2D& tr) {
            if (aabbOverlap(pp, ph, tr.position, g.half)) reached = true;
        });
        if (reached) nextLevel();
    }

    void loseLife() {
        m_hit.play();
        if (--m_lives <= 0) { m_state = State::GameOver; return; }
        // Réapparition au début du niveau courant.
        Transform2D& tr = m_reg.get<Transform2D>(m_player);
        tr.position = makeLevel(m_level).start;
        m_reg.get<RigidBody>(m_player).velocity = {0.0f, 0.0f};
    }

    void nextLevel() {
        if (++m_level >= kLevelCount) { m_state = State::Win; return; }
        loadLevel(m_level);
    }

    // ----- rendu -----
    void onRender() override {
        // Solides (sol, plateformes).
        m_reg.view<Transform2D, RectShape>([](Entity, Transform2D& tr, RectShape& s) {
            Graphics::drawRectangleCentered(tr.position, s.size, s.color, tr.rotation);
        });
        // Drapeau d'arrivée.
        m_reg.view<Goal, Transform2D>([](Entity, Goal& g, Transform2D& tr) {
            Graphics::drawRectangle({tr.position.x - 3.0f, tr.position.y - g.half.y}, {6.0f, g.half.y * 2.0f}, Color{90, 70, 50, 255});
            Graphics::drawRectangleCentered({tr.position.x + 18.0f, tr.position.y - g.half.y + 14.0f}, {30.0f, 22.0f}, Colors::Red);
        });
        // Pièces (légère pulsation).
        m_reg.view<Coin, Transform2D>([&](Entity, Coin& c, Transform2D& tr) {
            const float r = c.radius * (1.0f + 0.08f * std::sin(m_time * 6.0f));
            Graphics::drawCircle(tr.position, r, Colors::Yellow);
            Graphics::drawCircle(tr.position, r * 0.55f, Color{255, 235, 130, 255});
        });
        // Ennemis.
        m_reg.view<Enemy, Transform2D>([](Entity, Enemy& en, Transform2D& tr) {
            Graphics::drawCircle({tr.position.x, tr.position.y + en.half.y - 2.0f}, en.half.x, Color{0, 0, 0, 50});
            Graphics::drawRectangleCentered(tr.position, en.half * 2.0f, Color{150, 60, 180, 255});
            const float ex = en.dir > 0.0f ? 6.0f : -6.0f;
            Graphics::drawCircle({tr.position.x + ex, tr.position.y - 3.0f}, 4.0f, Colors::White);
            Graphics::drawCircle({tr.position.x + ex, tr.position.y - 3.0f}, 2.0f, Colors::Black);
        });
        // Joueur (sprite).
        m_reg.view<Transform2D, Sprite>([&](Entity e, Transform2D& tr, Sprite& spr) {
            if (!spr.texture) return;
            Graphics::drawCircle({tr.position.x, tr.position.y + 30.0f}, 16.0f, Color{0, 0, 0, 60});
            if (m_reg.has<Animator>(e)) {
                Animator& a = m_reg.get<Animator>(e);
                const AnimationClip* c = a.clip();
                const int row = c ? c->row : 0;
                const Rect src{static_cast<float>(a.frame * a.frameWidth), static_cast<float>(row * a.frameHeight),
                               static_cast<float>(a.frameWidth), static_cast<float>(a.frameHeight)};
                spr.texture->drawRegion(tr, src, spr.tint);
            } else {
                spr.texture->draw(tr, spr.tint);
            }
        });

        if (m_debug) {
            m_reg.view<AABB, Transform2D>([](Entity, AABB& b, Transform2D& tr) {
                Graphics::drawRectangleOutlineCentered(tr.position, b.halfSize * 2.0f, Color{255, 0, 80, 220}, 2.0f);
            });
        }

        drawHud();
    }

    void drawHud() {
        const Vec2 ws = windowSize();
        Graphics::drawText("Score: " + std::to_string(m_score), {16.0f, 14.0f}, 22, Colors::Black);
        Graphics::drawText("Niveau " + std::to_string(m_level + 1) + "/" + std::to_string(kLevelCount), {ws.x * 0.5f - 50.0f, 14.0f}, 22, Colors::Black);
        Graphics::drawText("Vies: " + std::to_string(m_lives), {ws.x - 110.0f, 14.0f}, 22, Colors::Red);
        Graphics::drawText("Fleches/ZQSD + Espace : sauter sur les ennemis, ramasse les pieces, atteins le drapeau",
                           {16.0f, ws.y - 26.0f}, 16, Colors::DarkGray);

        if (m_state == State::Win) {
            drawCenterBanner("BRAVO ! Tu as fini les 3 niveaux", "Entree : rejouer", Colors::Green);
        } else if (m_state == State::GameOver) {
            drawCenterBanner("GAME OVER", "Entree : recommencer", Colors::Red);
        }
    }

    void drawCenterBanner(const std::string& title, const std::string& sub, Color color) {
        const Vec2 ws = windowSize();
        Graphics::drawRectangle({0.0f, ws.y * 0.5f - 60.0f}, {ws.x, 120.0f}, Color{0, 0, 0, 150});
        Graphics::drawText(title, {ws.x * 0.5f - 200.0f, ws.y * 0.5f - 40.0f}, 32, color);
        Graphics::drawText("Score final : " + std::to_string(m_score), {ws.x * 0.5f - 110.0f, ws.y * 0.5f + 2.0f}, 22, Colors::White);
        Graphics::drawText(sub, {ws.x * 0.5f - 90.0f, ws.y * 0.5f + 30.0f}, 20, Colors::White);
    }
};

int main() {
    Jeu jeu;
    jeu.run();
    return 0;
}
