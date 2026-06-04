// MoteurJV — Éditeur visuel no-code (v1.0).
//
// Construis un jeu ENTIÈREMENT à la souris, sans écrire de code :
//   - menu "Creer" : Joueur, Plateforme, Caisse, Ennemi (entités prêtes)
//   - glisse des images depuis le panneau Assets dans la scène
//   - règle tout dans l'Inspecteur (position, taille, couleur, vitesse, saut…)
//   - "Play" : la scène devient jouable (gravité, joueur au clavier, ennemis qui
//     patrouillent, caméra qui suit le joueur). "Stop" restaure la scène d'avant.
//   - Sauvegarde / charge la scène en JSON.
//
// L'éditeur s'appuie uniquement sur l'API publique mjv:: (ECS, physique, rendu).

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"

#include "mjv/mjv.hpp"

#ifndef MJV_EDITOR_DIR
#define MJV_EDITOR_DIR "."
#endif
#ifndef MJV_ASSET_DIR
#define MJV_ASSET_DIR "."
#endif

using namespace mjv;
using json = nlohmann::json;
namespace fs = std::filesystem;

// --- Composants propres à l'éditeur -----------------------------------------
struct SpriteAsset { std::string path; Vec2 size; };          // affiche une image
struct Controllable { float speed = 300.0f; float jumpForce = 820.0f; }; // "joueur"
struct Patrol {                                               // ennemi va-et-vient
    float speed = 90.0f;
    float range = 140.0f;
    float dir = 1.0f;
    float originX = 0.0f;
    bool init = false;
};
struct Collectible { int points = 100; };                     // pièce à ramasser
struct Goal {};                                               // objectif = fin de niveau
struct Health { int lives = 3; };                             // vies du joueur

class Editor : public Application {
public:
    Editor() : Application(makeConfig()) {}
    ~Editor() override {
        for (auto& [name, tex] : m_thumbs) UnloadTexture(tex);
        UnloadRenderTexture(m_target);
        rlImGuiShutdown();
    }

private:
    static Config makeConfig() {
        Config c;
        c.width = 1600;
        c.height = 900;
        c.title = "MoteurJV - Editeur";
        c.clearColor = mjv::Color{30, 32, 38, 255};
        c.resizable = true;
        return c;
    }

    static constexpr int kWorldW = 1280;
    static constexpr int kWorldH = 720;
    static constexpr float kGravity = 1800.0f;

    Registry m_reg;
    Entity m_selected = NullEntity;
    bool m_playing = false;
    bool m_dragging = false;
    Vec2 m_dragOffset;
    std::string m_status;
    float m_statusTimer = 0.0f;
    json m_snapshot; // scène sauvegardée au lancement du Play (pour Stop)
    int m_score = 0;
    bool m_won = false;
    bool m_lost = false;
    Vec2 m_playerSpawn;
    float m_invuln = 0.0f;       // invincibilité après un coup (s)
    float m_timeLimit = 0.0f;    // limite de temps du niveau (0 = aucune)
    float m_timeLeft = 0.0f;
    int m_levelNum = 1;          // niveau courant (niveauN.json)
    mjv::Sound m_sfxCoin, m_sfxWin, m_sfxLose, m_sfxHit;

    ::RenderTexture2D m_target{};
    bool m_layoutInit = false;
    std::unordered_map<std::string, ::Texture2D> m_thumbs;
    mjv::Sound m_preview;

    Vec2 m_vpPos, m_vpSize;
    bool m_vpHovered = false;

    // Navigation et édition de la scène.
    float m_zoom = 1.0f;
    Vec2 m_camTarget{kWorldW * 0.5f, kWorldH * 0.5f};
    bool m_snap = false;
    float m_grid = 32.0f;
    int m_resizeHandle = -1; // coin en cours de redimensionnement (-1 = aucun)
    Vec2 m_fixedCorner;

    void onStart() override {
        rlImGuiSetup(true);
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        applyDarkTheme();
        m_target = LoadRenderTexture(kWorldW, kWorldH);
        const std::string dir = std::string(MJV_ASSET_DIR) + "/";
        m_sfxCoin.load(dir + "coin.wav");
        m_sfxWin.load(dir + "win.wav");
        m_sfxLose.load(dir + "lose.wav");
        m_sfxHit.load(dir + "bump.wav");
        // Au 1er lancement, crée deux niveaux de démo sur le disque.
        if (!loadLevel(1)) {
            buildScene();   saveLevel();      // niveau 1
            m_levelNum = 2; buildLevel2(); saveLevel(); // niveau 2
            m_levelNum = 1; loadLevel(1);
        }
    }

    // ----------------------------------------------------------------- thème
    void applyDarkTheme() {
        ImGui::StyleColorsDark();
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 5.0f; s.FrameRounding = 4.0f; s.GrabRounding = 4.0f;
        s.TabRounding = 4.0f; s.WindowPadding = ImVec2(10, 10);
        s.FramePadding = ImVec2(8, 4); s.ItemSpacing = ImVec2(8, 6);
        s.WindowBorderSize = 0.0f;
        ImVec4* c = s.Colors;
        const ImVec4 bg = ImVec4(0.13f, 0.14f, 0.17f, 1.0f);
        const ImVec4 panel = ImVec4(0.17f, 0.18f, 0.22f, 1.0f);
        const ImVec4 accent = ImVec4(0.20f, 0.50f, 0.90f, 1.0f);
        c[ImGuiCol_WindowBg] = bg;
        c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.0f);
        c[ImGuiCol_Header] = panel; c[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.27f, 0.32f, 1.0f);
        c[ImGuiCol_HeaderActive] = accent;
        c[ImGuiCol_Button] = panel; c[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.45f, 0.75f, 1.0f);
        c[ImGuiCol_ButtonActive] = accent;
        c[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.0f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.18f, 0.22f, 1.0f);
        c[ImGuiCol_Tab] = ImVec4(0.12f, 0.13f, 0.16f, 1.0f);
        c[ImGuiCol_TabActive] = accent; c[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.45f, 0.75f, 1.0f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.13f, 0.16f, 1.0f);
        c[ImGuiCol_CheckMark] = accent; c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SeparatorHovered] = accent;
    }

    // ----------------------------------------------------------- presets
    Entity newEntity() {
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{{400.0f, 200.0f}});
        m_selected = e;
        return e;
    }
    std::string assetPath(const char* name) const { return std::string(MJV_ASSET_DIR) + "/" + name; }
    // Crée une entité affichant un sprite (sa taille = taille de l'image).
    Entity makeSpriteEntity(const char* name, Vec2 pos) {
        const std::string path = assetPath(name);
        ::Texture2D& t = thumbnail(path);
        const Vec2 size{static_cast<float>(t.width), static_cast<float>(t.height)};
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{pos});
        m_reg.add<SpriteAsset>(e, SpriteAsset{path, size});
        return e;
    }
    Entity spawnPlatform(Vec2 pos) {
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{pos});
        m_reg.add<RectShape>(e, RectShape{{220.0f, 28.0f}, mjv::Color{150, 110, 70, 255}});
        m_reg.add<AABB>(e, AABB{{110.0f, 14.0f}});
        m_selected = e; setStatus("Plateforme creee"); return e;
    }
    Entity spawnCrate(Vec2 pos) {
        Entity e = makeSpriteEntity("crate.png", pos);
        m_reg.add<AABB>(e, AABB{m_reg.get<SpriteAsset>(e).size * 0.5f});
        m_reg.add<RigidBody>(e, RigidBody{});
        m_selected = e; setStatus("Caisse creee"); return e;
    }
    // Tuile de sol : solide statique. À stamper en grille pour bâtir des plateformes.
    Entity spawnTile(Vec2 pos) {
        Entity e = makeSpriteEntity("tile.png", pos);
        m_reg.add<AABB>(e, AABB{m_reg.get<SpriteAsset>(e).size * 0.5f});
        m_selected = e; setStatus("Tuile creee"); return e;
    }
    Entity spawnPlayer(Vec2 pos) {
        Entity e = makeSpriteEntity("player.png", pos);
        m_reg.add<AABB>(e, AABB{m_reg.get<SpriteAsset>(e).size * 0.5f});
        m_reg.add<RigidBody>(e, RigidBody{});
        m_reg.add<Controllable>(e, Controllable{});
        m_reg.add<Health>(e, Health{});
        m_selected = e; setStatus("Joueur cree (jouable en Play)"); return e;
    }
    Entity spawnEnemy(Vec2 pos) {
        Entity e = makeSpriteEntity("enemy.png", pos);
        m_reg.add<AABB>(e, AABB{m_reg.get<SpriteAsset>(e).size * 0.5f});
        m_reg.add<RigidBody>(e, RigidBody{});
        m_reg.add<Patrol>(e, Patrol{});
        m_selected = e; setStatus("Ennemi cree (patrouille en Play)"); return e;
    }
    // Pièce à ramasser : sprite SANS AABB (pas un mur solide, juste un trigger).
    Entity spawnCoin(Vec2 pos) {
        Entity e = makeSpriteEntity("coin.png", pos);
        m_reg.add<Collectible>(e, Collectible{});
        m_selected = e; setStatus("Piece creee"); return e;
    }
    // Objectif de fin de niveau (trigger, sans collision physique).
    Entity spawnGoal(Vec2 pos) {
        Entity e = makeSpriteEntity("flag.png", pos);
        m_reg.add<Goal>(e, Goal{});
        m_selected = e; setStatus("Objectif cree"); return e;
    }
    Entity spawnSprite(const std::string& path, Vec2 pos) {
        ::Texture2D& t = thumbnail(path);
        const Vec2 size{static_cast<float>(t.width), static_cast<float>(t.height)};
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{pos});
        m_reg.add<AABB>(e, AABB{size * 0.5f});
        m_reg.add<SpriteAsset>(e, SpriteAsset{path, size});
        m_selected = e; setStatus("Image ajoutee a la scene"); return e;
    }

    // Un petit niveau de départ (qui défile).
    void buildScene() {
        m_reg.clear();
        m_selected = NullEntity;
        // Sol long.
        Entity ground = m_reg.create();
        m_reg.add<Transform2D>(ground, Transform2D{{1300.0f, 690.0f}});
        m_reg.add<RectShape>(ground, RectShape{{2800.0f, 60.0f}, mjv::Color{0, 170, 90, 255}});
        m_reg.add<AABB>(ground, AABB{{1400.0f, 30.0f}});
        // Plateformes.
        spawnPlatform({420.0f, 520.0f});
        spawnPlatform({780.0f, 410.0f});
        spawnPlatform({1150.0f, 500.0f});
        spawnPlatform({1550.0f, 380.0f});
        // Caisses, ennemi.
        spawnCrate({900.0f, 200.0f});
        spawnCrate({950.0f, 120.0f});
        spawnEnemy({1150.0f, 440.0f});
        // Pièces à ramasser le long du parcours.
        spawnCoin({420.0f, 470.0f});
        spawnCoin({780.0f, 360.0f});
        spawnCoin({1150.0f, 450.0f});
        spawnCoin({1550.0f, 330.0f});
        // Objectif de fin, et le joueur au départ.
        spawnGoal({2000.0f, 600.0f});
        spawnPlayer({200.0f, 200.0f});
        m_timeLimit = 0.0f;
    }

    // Deuxième niveau de démo (plus long, chronométré, plus d'ennemis).
    void buildLevel2() {
        m_reg.clear(); m_selected = NullEntity;
        Entity ground = m_reg.create();
        m_reg.add<Transform2D>(ground, Transform2D{{1600.0f, 690.0f}});
        m_reg.add<RectShape>(ground, RectShape{{3400.0f, 60.0f}, mjv::Color{0, 160, 100, 255}});
        m_reg.add<AABB>(ground, AABB{{1700.0f, 30.0f}});
        spawnPlatform({360.0f, 540.0f}); spawnPlatform({700.0f, 430.0f});
        spawnPlatform({1050.0f, 330.0f}); spawnPlatform({1450.0f, 440.0f});
        spawnPlatform({1850.0f, 360.0f}); spawnPlatform({2250.0f, 480.0f});
        spawnEnemy({700.0f, 390.0f}); spawnEnemy({1450.0f, 400.0f}); spawnEnemy({1850.0f, 320.0f});
        spawnCoin({360.0f, 500.0f}); spawnCoin({700.0f, 390.0f}); spawnCoin({1050.0f, 290.0f});
        spawnCoin({1450.0f, 400.0f}); spawnCoin({1850.0f, 320.0f}); spawnCoin({2250.0f, 440.0f});
        spawnGoal({2750.0f, 600.0f});
        spawnPlayer({180.0f, 200.0f});
        m_timeLimit = 45.0f;
    }

    Vec2 halfExtents(Entity e) {
        if (m_reg.has<RectShape>(e))   return m_reg.get<RectShape>(e).size * 0.5f;
        if (m_reg.has<AABB>(e))        return m_reg.get<AABB>(e).halfSize;
        if (m_reg.has<SpriteAsset>(e)) return m_reg.get<SpriteAsset>(e).size * 0.5f;
        return {20.0f, 20.0f};
    }

    // ----------------------------------------------------------- Play/Stop
    void play() {
        m_snapshot = sceneToJson();
        m_playing = true;
        m_score = 0; m_won = false; m_lost = false; m_invuln = 0.0f;
        m_timeLeft = m_timeLimit;
        Vec2 sp; if (firstPlayer(sp)) m_playerSpawn = sp;
        setStatus("Lecture - Stop pour revenir a l'edition");
    }
    void stop() {
        m_playing = false;
        jsonToScene(m_snapshot);
        setStatus("Edition");
    }

    bool firstPlayer(Vec2& out) {
        bool found = false;
        m_reg.view<Controllable, Transform2D>([&](Entity, Controllable&, Transform2D& t) {
            if (!found) { out = t.position; found = true; }
        });
        return found;
    }

    // ----------------------------------------------------------- systèmes (Play)
    void controlSystem() {
        if (m_won || m_lost) return; // partie terminée : on fige le joueur
        if (ImGui::GetIO().WantCaptureKeyboard) return;
        const int L = (Input::isDown(Key::Left)  || Input::isDown(Key::A) || Input::isDown(Key::Q)) ? 1 : 0;
        const int R = (Input::isDown(Key::Right) || Input::isDown(Key::D)) ? 1 : 0;
        const bool jump = Input::isPressed(Key::Space) || Input::isPressed(Key::Up) ||
                          Input::isPressed(Key::W) || Input::isPressed(Key::Z);
        m_reg.view<Controllable, RigidBody>([&](Entity, Controllable& ctrl, RigidBody& rb) {
            rb.velocity.x = static_cast<float>(R - L) * ctrl.speed;
            if (jump && rb.onGround) rb.velocity.y = -ctrl.jumpForce;
        });
    }
    void patrolSystem(float dt) {
        m_reg.view<Patrol, Transform2D>([&](Entity e, Patrol& p, Transform2D& t) {
            if (!p.init) { p.originX = t.position.x; p.init = true; }
            if (t.position.x > p.originX + p.range) p.dir = -1.0f;
            if (t.position.x < p.originX - p.range) p.dir = 1.0f;
            if (m_reg.has<RigidBody>(e)) m_reg.get<RigidBody>(e).velocity.x = p.dir * p.speed;
            else                         t.position.x += p.dir * p.speed * dt;
        });
    }

    void win()  { if (!m_won && !m_lost) { m_won = true; m_sfxWin.play(); } }
    void lose() { if (!m_won && !m_lost) { m_lost = true; m_sfxLose.play(); } }

    // Le joueur est touché : perd une vie (et respawn) ou perd la partie.
    void hitPlayer(Entity e) {
        m_invuln = 1.4f;
        if (m_reg.has<Health>(e)) {
            Health& hp = m_reg.get<Health>(e);
            if (--hp.lives <= 0) { lose(); return; }
            m_sfxHit.play();
            if (m_reg.has<Transform2D>(e)) m_reg.get<Transform2D>(e).position = m_playerSpawn;
            if (m_reg.has<RigidBody>(e))   m_reg.get<RigidBody>(e).velocity = {0.0f, 0.0f};
        } else {
            lose();
        }
    }

    // Ramassage des pièces, contact ennemi, objectif atteint.
    void gameplaySystem() {
        if (m_won || m_lost) return;
        struct P { Entity e; Vec2 c; Vec2 h; };
        std::vector<P> players;
        m_reg.view<Controllable, Transform2D>([&](Entity e, Controllable&, Transform2D& t) {
            players.push_back({e, t.position, halfExtents(e)});
        });
        if (players.empty()) return;

        std::vector<Entity> picked;
        m_reg.view<Collectible, Transform2D>([&](Entity e, Collectible& c, Transform2D& t) {
            const Vec2 h = halfExtents(e);
            for (const P& p : players)
                if (aabbOverlap(p.c, p.h, t.position, h)) { m_score += c.points; m_sfxCoin.play(); picked.push_back(e); break; }
        });
        for (Entity e : picked) m_reg.destroy(e);

        if (m_invuln <= 0.0f) {
            m_reg.view<Patrol, Transform2D>([&](Entity e, Patrol&, Transform2D& t) {
                const Vec2 h = halfExtents(e);
                for (const P& p : players) if (aabbOverlap(p.c, p.h, t.position, h)) { hitPlayer(p.e); return; }
            });
        }
        m_reg.view<Goal, Transform2D>([&](Entity e, Goal&, Transform2D& t) {
            const Vec2 h = halfExtents(e);
            for (const P& p : players) if (aabbOverlap(p.c, p.h, t.position, h)) win();
        });
    }

    // ----------------------------------------------------------------- update
    void onUpdate(float dt) override {
        if (m_statusTimer > 0.0f) m_statusTimer -= dt;
        if (!m_playing) handleViewportInteraction();

        if (m_playing) {
            // Fin de partie (Entree) : niveau suivant si gagné et s'il existe, sinon rejouer.
            if ((m_won || m_lost) && Input::isPressed(Key::Enter)) {
                if (m_won && hasNextLevel()) advanceLevel();
                else { stop(); play(); }
                renderSceneToTexture(); return;
            }
            if (m_invuln > 0.0f) m_invuln -= dt;
            // Limite de temps.
            if (m_timeLimit > 0.0f && !m_won && !m_lost) {
                m_timeLeft -= dt;
                if (m_timeLeft <= 0.0f) { m_timeLeft = 0.0f; lose(); }
            }
            controlSystem();
            patrolSystem(dt);
            physicsStep(m_reg, std::min(dt, 0.033f), {0.0f, kGravity});
            gameplaySystem();
        }
        renderSceneToTexture();
    }

    int playerLives() {
        int lives = -1;
        m_reg.view<Controllable, Health>([&](Entity, Controllable&, Health& h) { if (lives < 0) lives = h.lives; });
        return lives;
    }

    // Écran -> coordonnées monde, en tenant compte de la caméra d'édition.
    Vec2 screenToWorld(Vec2 s) const {
        if (m_vpSize.x <= 0.0f || m_vpSize.y <= 0.0f) return s;
        const float tx = (s.x - m_vpPos.x) * (kWorldW / m_vpSize.x);
        const float ty = (s.y - m_vpPos.y) * (kWorldH / m_vpSize.y);
        return {(tx - kWorldW * 0.5f) / m_zoom + m_camTarget.x,
                (ty - kWorldH * 0.5f) / m_zoom + m_camTarget.y};
    }
    Vec2 snap(Vec2 p) const {
        if (!m_snap) return p;
        return {std::round(p.x / m_grid) * m_grid, std::round(p.y / m_grid) * m_grid};
    }
    // Index du coin de redimensionnement sous le point (ou -1).
    int handleAt(Vec2 w) {
        if (!m_reg.valid(m_selected) ||
            !(m_reg.has<RectShape>(m_selected) || m_reg.has<SpriteAsset>(m_selected))) return -1;
        const Vec2 c = m_reg.get<Transform2D>(m_selected).position;
        const Vec2 h = halfExtents(m_selected);
        const Vec2 cs[4] = {{c.x - h.x, c.y - h.y}, {c.x + h.x, c.y - h.y}, {c.x + h.x, c.y + h.y}, {c.x - h.x, c.y + h.y}};
        const float thr = 12.0f / m_zoom;
        for (int i = 0; i < 4; ++i)
            if (std::abs(w.x - cs[i].x) <= thr && std::abs(w.y - cs[i].y) <= thr) return i;
        return -1;
    }
    Entity pickEntity(Vec2 p) {
        Entity hit = NullEntity;
        m_reg.view<Transform2D>([&](Entity e, Transform2D& tr) {
            const Vec2 h = halfExtents(e);
            if (std::abs(p.x - tr.position.x) <= h.x && std::abs(p.y - tr.position.y) <= h.y) hit = e;
        });
        return hit;
    }
    void handleViewportInteraction() {
        if (m_vpHovered) {
            // Zoom à la molette.
            const float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) { m_zoom *= (wheel > 0 ? 1.1f : 0.9f); m_zoom = std::min(3.0f, std::max(0.25f, m_zoom)); }
            // Pan au clic droit.
            if (Input::mouseDown(Mouse::Right)) {
                const Vec2 d = Input::mouseDelta();
                m_camTarget.x -= d.x * (kWorldW / m_vpSize.x) / m_zoom;
                m_camTarget.y -= d.y * (kWorldH / m_vpSize.y) / m_zoom;
            }
        }
        if (!m_vpHovered) { if (Input::mouseReleased(Mouse::Left)) { m_dragging = false; m_resizeHandle = -1; } return; }

        const Vec2 w = screenToWorld(Input::mousePosition());
        if (Input::mousePressed(Mouse::Left)) {
            const int handle = handleAt(w);
            if (handle >= 0) {
                m_resizeHandle = handle;
                const Vec2 c = m_reg.get<Transform2D>(m_selected).position;
                const Vec2 h = halfExtents(m_selected);
                const Vec2 cs[4] = {{c.x - h.x, c.y - h.y}, {c.x + h.x, c.y - h.y}, {c.x + h.x, c.y + h.y}, {c.x - h.x, c.y + h.y}};
                m_fixedCorner = cs[(handle + 2) % 4];
            } else {
                const Entity hit = pickEntity(w);
                if (hit != NullEntity) { m_selected = hit; m_dragging = true; m_dragOffset = m_reg.get<Transform2D>(hit).position - w; }
            }
        }

        if (m_resizeHandle >= 0 && Input::mouseDown(Mouse::Left) && m_reg.valid(m_selected)) {
            const Vec2 nc = snap(w);
            const Vec2 center = (nc + m_fixedCorner) * 0.5f;
            const Vec2 half{std::max(8.0f, std::abs(nc.x - m_fixedCorner.x) * 0.5f),
                            std::max(8.0f, std::abs(nc.y - m_fixedCorner.y) * 0.5f)};
            m_reg.get<Transform2D>(m_selected).position = center;
            if (m_reg.has<RectShape>(m_selected))   m_reg.get<RectShape>(m_selected).size = half * 2.0f;
            if (m_reg.has<SpriteAsset>(m_selected))  m_reg.get<SpriteAsset>(m_selected).size = half * 2.0f;
            if (m_reg.has<AABB>(m_selected))         m_reg.get<AABB>(m_selected).halfSize = half;
        } else if (m_dragging && Input::mouseDown(Mouse::Left) && m_reg.valid(m_selected) && m_reg.has<Transform2D>(m_selected)) {
            m_reg.get<Transform2D>(m_selected).position = snap(w + m_dragOffset);
            if (m_reg.has<RigidBody>(m_selected)) m_reg.get<RigidBody>(m_selected).velocity = {0.0f, 0.0f};
        }
        if (Input::mouseReleased(Mouse::Left)) { m_dragging = false; m_resizeHandle = -1; }
    }

    void renderSceneToTexture() {
        BeginTextureMode(m_target);
        Graphics::clear(Colors::SkyBlue);

        ::Camera2D cam{};
        cam.offset = {kWorldW * 0.5f, kWorldH * 0.5f};
        cam.zoom = m_zoom;
        cam.target = {m_camTarget.x, m_camTarget.y};
        if (m_playing) {
            Vec2 p;
            if (firstPlayer(p)) { cam.target = {p.x, p.y}; cam.zoom = 1.0f; }
        }
        BeginMode2D(cam);
        if (!m_playing && m_snap) drawGrid();

        m_reg.view<Transform2D, RectShape>([&](Entity, Transform2D& tr, RectShape& s) {
            Graphics::drawRectangleCentered(tr.position, s.size, s.color, tr.rotation);
            Graphics::drawRectangleOutlineCentered(tr.position, s.size, mjv::Color{20, 20, 28, 255}, 1.5f);
        });
        m_reg.view<Transform2D, SpriteAsset>([&](Entity, Transform2D& tr, SpriteAsset& sp) {
            ::Texture2D& t = thumbnail(sp.path);
            const ::Rectangle src{0, 0, static_cast<float>(t.width), static_cast<float>(t.height)};
            const ::Rectangle dst{tr.position.x, tr.position.y, sp.size.x * tr.scale.x, sp.size.y * tr.scale.y};
            DrawTexturePro(t, src, dst, ::Vector2{dst.width * 0.5f, dst.height * 0.5f}, tr.rotation, ::WHITE);
        });
        if (!m_playing && m_reg.valid(m_selected) && m_reg.has<Transform2D>(m_selected)) {
            const Vec2 h = halfExtents(m_selected);
            Graphics::drawRectangleOutlineCentered(m_reg.get<Transform2D>(m_selected).position,
                                                   h * 2.0f + Vec2{8, 8}, Colors::Yellow, 2.5f);
            drawHandles(m_selected);
        }
        EndMode2D();
        if (m_playing) drawHud();
        EndTextureMode();
    }

    void drawGrid() {
        const float left = m_camTarget.x - (kWorldW * 0.5f) / m_zoom;
        const float right = m_camTarget.x + (kWorldW * 0.5f) / m_zoom;
        const float top = m_camTarget.y - (kWorldH * 0.5f) / m_zoom;
        const float bottom = m_camTarget.y + (kWorldH * 0.5f) / m_zoom;
        const ::Color col{255, 255, 255, 28};
        const int x0 = static_cast<int>(std::floor(left / m_grid)), x1 = static_cast<int>(std::ceil(right / m_grid));
        const int y0 = static_cast<int>(std::floor(top / m_grid)), y1 = static_cast<int>(std::ceil(bottom / m_grid));
        if (x1 - x0 < 500) for (int i = x0; i <= x1; ++i) DrawLine(static_cast<int>(i * m_grid), static_cast<int>(top), static_cast<int>(i * m_grid), static_cast<int>(bottom), col);
        if (y1 - y0 < 500) for (int i = y0; i <= y1; ++i) DrawLine(static_cast<int>(left), static_cast<int>(i * m_grid), static_cast<int>(right), static_cast<int>(i * m_grid), col);
    }

    void drawHandles(Entity e) {
        if (!(m_reg.has<RectShape>(e) || m_reg.has<SpriteAsset>(e))) return;
        const Vec2 c = m_reg.get<Transform2D>(e).position;
        const Vec2 h = halfExtents(e);
        const float s = 9.0f / m_zoom;
        const Vec2 cs[4] = {{c.x - h.x, c.y - h.y}, {c.x + h.x, c.y - h.y}, {c.x + h.x, c.y + h.y}, {c.x - h.x, c.y + h.y}};
        for (const Vec2& p : cs)
            DrawRectangle(static_cast<int>(p.x - s), static_cast<int>(p.y - s), static_cast<int>(2 * s), static_cast<int>(2 * s), ::Color{60, 180, 255, 255});
    }

    // Texte centré horizontalement (police par défaut : ~0.5*taille par caractère).
    void drawCentered(const std::string& s, float y, int size, mjv::Color col) {
        const float x = kWorldW * 0.5f - s.size() * size * 0.25f;
        Graphics::drawText(s, {x, y}, size, col);
    }

    void drawHud() {
        const mjv::Color white{245, 245, 245, 255};
        const mjv::Color shadow{0, 0, 0, 140};
        // Score (avec ombre pour la lisibilité).
        Graphics::drawText("Score : " + std::to_string(m_score), {26.0f, 22.0f}, 38, shadow);
        Graphics::drawText("Score : " + std::to_string(m_score), {24.0f, 20.0f}, 38, white);
        // Vies.
        const int lives = playerLives();
        if (lives >= 0) Graphics::drawText("Vies : " + std::to_string(lives), {24.0f, 66.0f}, 30, white);
        // Temps.
        if (m_timeLimit > 0.0f)
            Graphics::drawText("Temps : " + std::to_string(static_cast<int>(std::ceil(m_timeLeft))),
                               {24.0f, 104.0f}, 30, white);
        // Niveau (en haut à droite).
        Graphics::drawText("Niveau " + std::to_string(m_levelNum), {kWorldW - 210.0f, 22.0f}, 30, white);

        // Écran de fin stylé.
        if (m_won || m_lost) {
            Graphics::drawRectangle({0.0f, 0.0f}, {kWorldW, kWorldH}, mjv::Color{12, 14, 20, 175});
            const float cy = kWorldH * 0.5f;
            const bool next = m_won && hasNextLevel();
            if (next)        drawCentered("Niveau termine !", cy - 110.0f, 90, mjv::Color{120, 230, 120, 255});
            else if (m_won)  drawCentered("VICTOIRE !", cy - 110.0f, 96, mjv::Color{120, 230, 120, 255});
            else             drawCentered("PERDU", cy - 110.0f, 96, mjv::Color{235, 90, 90, 255});
            drawCentered("Score : " + std::to_string(m_score), cy + 10.0f, 44, white);
            drawCentered(next ? "Entree : niveau suivant       Stop : editer"
                              : "Entree : rejouer       Stop : editer",
                         cy + 80.0f, 28, mjv::Color{180, 185, 200, 255});
        }
    }

    // ----------------------------------------------------------------- UI
    void onRender() override {
        rlImGuiBegin();
        drawMenuBar();
        setupDockspace();
        drawHierarchy();
        drawInspector();
        drawViewport();
        drawAssets();
        rlImGuiEnd();
    }

    void setupDockspace() {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const ImGuiID dockId = ImGui::DockSpaceOverViewport(0, vp, ImGuiDockNodeFlags_None);
        if (!m_layoutInit) {
            m_layoutInit = true;
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockId, vp->Size);
            ImGuiID center = dockId;
            ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
            ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, nullptr, &center);
            ImGui::DockBuilderDockWindow("Hierarchie", left);
            ImGui::DockBuilderDockWindow("Inspecteur", right);
            ImGui::DockBuilderDockWindow("Assets", bottom);
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderFinish(dockId);
        }
    }

    void drawMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Fichier")) {
                if (ImGui::MenuItem("Nouveau")) buildScene();
                if (ImGui::MenuItem("Ouvrir ce niveau")) loadLevel(m_levelNum);
                if (ImGui::MenuItem("Sauvegarder ce niveau")) saveLevel();
                ImGui::Separator();
                if (ImGui::MenuItem("Quitter")) std::exit(0);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Creer")) {
                if (ImGui::MenuItem("Joueur"))     spawnPlayer({250.0f, 200.0f});
                if (ImGui::MenuItem("Plateforme")) spawnPlatform({400.0f, 350.0f});
                if (ImGui::MenuItem("Tuile"))      spawnTile(snap(m_camTarget));
                if (ImGui::MenuItem("Caisse"))     spawnCrate({450.0f, 150.0f});
                if (ImGui::MenuItem("Ennemi"))     spawnEnemy({500.0f, 200.0f});
                if (ImGui::MenuItem("Piece"))      spawnCoin({550.0f, 300.0f});
                if (ImGui::MenuItem("Objectif"))   spawnGoal({600.0f, 250.0f});
                ImGui::Separator();
                if (ImGui::MenuItem("Entite vide")) newEntity();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edition")) {
                if (ImGui::MenuItem("Supprimer la selection", "Suppr", false, m_reg.valid(m_selected))) {
                    m_reg.destroy(m_selected); m_selected = NullEntity;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Vue")) {
                ImGui::Checkbox("Grille magnetique", &m_snap);
                ImGui::SetNextItemWidth(120);
                ImGui::DragFloat("pas de grille", &m_grid, 1.0f, 4.0f, 256.0f, "%.0f");
                ImGui::Separator();
                if (ImGui::MenuItem("Recentrer la vue")) { m_camTarget = {kWorldW * 0.5f, kWorldH * 0.5f}; m_zoom = 1.0f; }
                ImGui::TextDisabled("Clic droit : deplacer la vue");
                ImGui::TextDisabled("Molette : zoom");
                ImGui::TextDisabled("Poignees bleues : redimensionner");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Niveau")) {
                ImGui::SetNextItemWidth(120);
                if (ImGui::InputInt("numero", &m_levelNum) && m_levelNum < 1) m_levelNum = 1;
                if (ImGui::MenuItem("Charger ce numero")) loadLevel(m_levelNum);
                if (ImGui::MenuItem("Sauvegarder sous ce numero")) saveLevel();
                ImGui::Separator();
                ImGui::SetNextItemWidth(160);
                ImGui::DragFloat("Temps limite (s)", &m_timeLimit, 1.0f, 0.0f, 999.0f, "%.0f");
                ImGui::TextDisabled("0 = pas de limite");
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::Button(m_playing ? " Stop " : " Play ")) { m_playing ? stop() : play(); }
            ImGui::SameLine();
            if (m_statusTimer > 0.0f)
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", m_status.c_str());
            else if (m_playing)
                ImGui::TextDisabled("Fleches/ZQSD : bouger   Espace : sauter   (Stop pour editer)");
            else
                ImGui::TextDisabled("Menu Creer pour ajouter - glisse une image depuis Assets - Play pour jouer");
            ImGui::SameLine(ImGui::GetWindowWidth() - 90.0f);
            ImGui::Text("%.0f FPS", ImGui::GetIO().Framerate);
            ImGui::EndMainMenuBar();
        }
    }

    void drawViewport() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");
        rlImGuiImageRenderTextureFit(&m_target, true);
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        m_vpPos = {mn.x, mn.y};
        m_vpSize = {mx.x - mn.x, mx.y - mn.y};
        m_vpHovered = ImGui::IsItemHovered();
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_IMG")) {
                const std::string path(static_cast<const char*>(pl->Data));
                spawnSprite(path, screenToWorld(Input::mousePosition()));
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    std::string componentSummary(Entity e) {
        std::string s = "  [";
        if (m_reg.has<Controllable>(e)) s += "J";
        if (m_reg.has<Patrol>(e))       s += "E";
        if (m_reg.has<Collectible>(e))  s += "$";
        if (m_reg.has<Goal>(e))         s += "O";
        if (m_reg.has<Health>(e))       s += "H";
        if (m_reg.has<RigidBody>(e))    s += "P";
        if (m_reg.has<SpriteAsset>(e))  s += "S";
        if (m_reg.has<RectShape>(e))    s += "R";
        return s + "]";
    }

    void drawHierarchy() {
        ImGui::Begin("Hierarchie");
        const std::vector<Entity> ents = m_reg.entities();
        ImGui::TextDisabled("%d entites", static_cast<int>(ents.size()));
        ImGui::Separator();
        for (Entity e : ents) {
            std::string label = "Entite " + std::to_string(e) + componentSummary(e);
            if (ImGui::Selectable(label.c_str(), e == m_selected)) m_selected = e;
        }
        ImGui::End();
    }

    void drawInspector() {
        ImGui::Begin("Inspecteur");
        if (!m_reg.valid(m_selected)) {
            ImGui::TextDisabled("Aucune entite selectionnee.");
            ImGui::TextDisabled("Menu 'Creer', ou clique une entite.");
            ImGui::End();
            return;
        }
        ImGui::Text("Entite %u", m_selected);
        ImGui::SameLine();
        if (ImGui::SmallButton("Supprimer")) { m_reg.destroy(m_selected); m_selected = NullEntity; ImGui::End(); return; }
        ImGui::Separator();

        if (m_reg.has<Transform2D>(m_selected) && ImGui::CollapsingHeader("Position / taille", ImGuiTreeNodeFlags_DefaultOpen)) {
            Transform2D& t = m_reg.get<Transform2D>(m_selected);
            ImGui::DragFloat2("position", &t.position.x, 1.0f);
            ImGui::DragFloat("rotation", &t.rotation, 0.5f);
            ImGui::DragFloat2("echelle", &t.scale.x, 0.01f);
        }
        if (m_reg.has<RectShape>(m_selected) && ImGui::CollapsingHeader("Apparence (rectangle)", ImGuiTreeNodeFlags_DefaultOpen)) {
            RectShape& r = m_reg.get<RectShape>(m_selected);
            ImGui::DragFloat2("taille", &r.size.x, 1.0f, 1.0f, 4000.0f);
            float col[3] = {r.color.r / 255.0f, r.color.g / 255.0f, r.color.b / 255.0f};
            if (ImGui::ColorEdit3("couleur", col)) {
                r.color.r = (std::uint8_t)(col[0] * 255); r.color.g = (std::uint8_t)(col[1] * 255); r.color.b = (std::uint8_t)(col[2] * 255);
            }
        }
        if (m_reg.has<SpriteAsset>(m_selected) && ImGui::CollapsingHeader("Image", ImGuiTreeNodeFlags_DefaultOpen)) {
            SpriteAsset& sp = m_reg.get<SpriteAsset>(m_selected);
            ImGui::DragFloat2("taille", &sp.size.x, 1.0f, 1.0f, 4000.0f);
            ImGui::TextDisabled("Changer l'image (skin) :");
            int n = 0;
            for (const std::string& img : imageAssets()) {
                ::Texture2D& t = thumbnail(img);
                ImGui::PushID(n);
                if (rlImGuiImageButtonSize("skin", &t, ::Vector2{38, 38})) {
                    sp.path = img;
                    sp.size = {static_cast<float>(t.width), static_cast<float>(t.height)};
                }
                ImGui::PopID();
                if (++n % 5 != 0) ImGui::SameLine();
            }
            ImGui::NewLine();
        }
        if (m_reg.has<RigidBody>(m_selected) && ImGui::CollapsingHeader("Physique (RigidBody)", ImGuiTreeNodeFlags_DefaultOpen)) {
            RigidBody& rb = m_reg.get<RigidBody>(m_selected);
            ImGui::DragFloat("gravite x echelle", &rb.gravityScale, 0.05f);
        }
        if (m_reg.has<Controllable>(m_selected) && ImGui::CollapsingHeader("Joueur (controle clavier)", ImGuiTreeNodeFlags_DefaultOpen)) {
            Controllable& cc = m_reg.get<Controllable>(m_selected);
            ImGui::DragFloat("vitesse", &cc.speed, 2.0f, 0.0f, 1000.0f);
            ImGui::DragFloat("force de saut", &cc.jumpForce, 5.0f, 0.0f, 2000.0f);
        }
        if (m_reg.has<Patrol>(m_selected) && ImGui::CollapsingHeader("Ennemi (patrouille)", ImGuiTreeNodeFlags_DefaultOpen)) {
            Patrol& p = m_reg.get<Patrol>(m_selected);
            ImGui::DragFloat("vitesse", &p.speed, 1.0f, 0.0f, 600.0f);
            ImGui::DragFloat("portee", &p.range, 1.0f, 0.0f, 1200.0f);
        }
        if (m_reg.has<Collectible>(m_selected) && ImGui::CollapsingHeader("Piece (a ramasser)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragInt("points", &m_reg.get<Collectible>(m_selected).points, 1.0f, 0, 100000);
        }
        if (m_reg.has<Goal>(m_selected) && ImGui::CollapsingHeader("Objectif (fin de niveau)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("Le joueur gagne en le touchant.");
        }
        if (m_reg.has<Health>(m_selected) && ImGui::CollapsingHeader("Vies", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragInt("nombre de vies", &m_reg.get<Health>(m_selected).lives, 1.0f, 1, 99);
        }

        ImGui::Separator();
        ImGui::TextDisabled("Ajouter un comportement :");
        if (!m_reg.has<RigidBody>(m_selected)    && ImGui::Button("+ Physique"))  m_reg.add<RigidBody>(m_selected);
        ImGui::SameLine();
        if (!m_reg.has<Controllable>(m_selected) && ImGui::Button("+ Joueur"))    { m_reg.add<Controllable>(m_selected); if (!m_reg.has<RigidBody>(m_selected)) m_reg.add<RigidBody>(m_selected); }
        ImGui::SameLine();
        if (!m_reg.has<Patrol>(m_selected)       && ImGui::Button("+ Ennemi"))    { m_reg.add<Patrol>(m_selected); if (!m_reg.has<RigidBody>(m_selected)) m_reg.add<RigidBody>(m_selected); }
        if (!m_reg.has<Collectible>(m_selected)  && ImGui::Button("+ Piece"))     m_reg.add<Collectible>(m_selected);
        ImGui::SameLine();
        if (!m_reg.has<Goal>(m_selected)         && ImGui::Button("+ Objectif"))  m_reg.add<Goal>(m_selected);
        ImGui::SameLine();
        if (!m_reg.has<Health>(m_selected)       && ImGui::Button("+ Vies"))      m_reg.add<Health>(m_selected);
        if (!m_reg.has<RectShape>(m_selected)     && ImGui::Button("+ Rectangle")) m_reg.add<RectShape>(m_selected, RectShape{{40, 40}, Colors::Blue});
        if (!m_reg.has<AABB>(m_selected)          && ImGui::Button("+ Collision")) m_reg.add<AABB>(m_selected, AABB{halfExtents(m_selected)});
        ImGui::End();
    }

    ::Texture2D& thumbnail(const std::string& path) {
        auto it = m_thumbs.find(path);
        if (it != m_thumbs.end()) return it->second;
        return m_thumbs.emplace(path, LoadTexture(path.c_str())).first->second;
    }

    // Toutes les images du dossier d'assets (récursif → supporte les packs/sous-dossiers).
    std::vector<std::string> imageAssets() {
        std::vector<std::string> v;
        std::error_code ec;
        for (const fs::directory_entry& f : fs::recursive_directory_iterator(MJV_ASSET_DIR, ec)) {
            if (!f.is_regular_file()) continue;
            const std::string ext = f.path().extension().string();
            if (ext == ".png" || ext == ".jpg") v.push_back(f.path().string());
        }
        return v;
    }

    void drawAssets() {
        ImGui::Begin("Assets");
        ImGui::TextDisabled("Glisse une image dans la scene, ou clique un son pour l'ecouter");
        ImGui::Separator();
        std::error_code ec;
        const fs::path dir(MJV_ASSET_DIR);
        if (!fs::exists(dir, ec)) { ImGui::TextDisabled("(dossier introuvable)"); ImGui::End(); return; }
        const float cell = 92.0f;
        const int perRow = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
        int i = 0;
        for (const fs::directory_entry& f : fs::recursive_directory_iterator(dir, ec)) {
            if (!f.is_regular_file()) continue;
            const std::string path = f.path().string();
            const std::string name = f.path().filename().string();
            const std::string ext = f.path().extension().string();
            ImGui::PushID(i);
            ImGui::BeginGroup();
            if (ext == ".png" || ext == ".jpg") {
                ::Texture2D& t = thumbnail(path);
                if (rlImGuiImageButtonSize(name.c_str(), &t, ::Vector2{64, 64})) spawnSprite(path, {kWorldW * 0.5f, kWorldH * 0.5f});
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("ASSET_IMG", path.c_str(), path.size() + 1);
                    rlImGuiImageSize(&t, 48, 48); ImGui::Text("%s", name.c_str());
                    ImGui::EndDragDropSource();
                }
            } else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") {
                if (ImGui::Button((">##" + name).c_str(), ImVec2(64, 64))) { if (m_preview.load(path)) m_preview.play(); }
            } else {
                ImGui::Button(("?##" + name).c_str(), ImVec2(64, 64));
            }
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::EndGroup();
            ImGui::PopID();
            if (++i % perRow != 0) ImGui::SameLine();
        }
        ImGui::End();
    }

    // ------------------------------------------------------------- JSON
    static json vecToJ(Vec2 v) { return json::array({v.x, v.y}); }
    static Vec2 jToVec(const json& j) { return {j[0].get<float>(), j[1].get<float>()}; }
    std::string levelPath(int n) const { return std::string(MJV_EDITOR_DIR) + "/niveau" + std::to_string(n) + ".json"; }
    bool hasNextLevel() { std::ifstream f(levelPath(m_levelNum + 1)); return f.good(); }

    void saveLevel() {
        std::ofstream f(levelPath(m_levelNum));
        if (f) { f << sceneToJson().dump(2); setStatus("Niveau " + std::to_string(m_levelNum) + " sauvegarde"); }
        else setStatus("Echec sauvegarde");
    }
    bool loadLevel(int n) {
        std::ifstream f(levelPath(n));
        if (!f) { setStatus("Niveau " + std::to_string(n) + " introuvable"); return false; }
        json doc;
        try { f >> doc; } catch (...) { setStatus("niveau invalide"); return false; }
        jsonToScene(doc); m_levelNum = n; setStatus("Niveau " + std::to_string(n) + " charge"); return true;
    }
    // Passe au niveau suivant en pleine partie (en gardant le score).
    void advanceLevel() {
        const int keep = m_score;
        loadLevel(m_levelNum + 1);
        m_snapshot = sceneToJson();
        m_won = false; m_lost = false; m_invuln = 0.0f;
        m_timeLeft = m_timeLimit;
        Vec2 sp; if (firstPlayer(sp)) m_playerSpawn = sp;
        m_score = keep;
    }

    json sceneToJson() {
        json doc; doc["entities"] = json::array();
        doc["timeLimit"] = m_timeLimit;
        for (Entity e : m_reg.entities()) {
            json je;
            if (m_reg.has<Transform2D>(e)) { Transform2D& t = m_reg.get<Transform2D>(e); je["Transform2D"] = {{"position", vecToJ(t.position)}, {"rotation", t.rotation}, {"scale", vecToJ(t.scale)}}; }
            if (m_reg.has<RectShape>(e))   { RectShape& r = m_reg.get<RectShape>(e); je["RectShape"] = {{"size", vecToJ(r.size)}, {"color", json::array({r.color.r, r.color.g, r.color.b})}}; }
            if (m_reg.has<AABB>(e))        je["AABB"] = {{"halfSize", vecToJ(m_reg.get<AABB>(e).halfSize)}};
            if (m_reg.has<RigidBody>(e))   je["RigidBody"] = {{"gravityScale", m_reg.get<RigidBody>(e).gravityScale}};
            if (m_reg.has<Controllable>(e)){ Controllable& c = m_reg.get<Controllable>(e); je["Controllable"] = {{"speed", c.speed}, {"jumpForce", c.jumpForce}}; }
            if (m_reg.has<Patrol>(e))      { Patrol& p = m_reg.get<Patrol>(e); je["Patrol"] = {{"speed", p.speed}, {"range", p.range}}; }
            if (m_reg.has<Collectible>(e)) je["Collectible"] = {{"points", m_reg.get<Collectible>(e).points}};
            if (m_reg.has<Goal>(e))        je["Goal"] = true;
            if (m_reg.has<Health>(e))      je["Health"] = {{"lives", m_reg.get<Health>(e).lives}};
            if (m_reg.has<SpriteAsset>(e)) { SpriteAsset& sp = m_reg.get<SpriteAsset>(e); je["SpriteAsset"] = {{"path", sp.path}, {"size", vecToJ(sp.size)}}; }
            doc["entities"].push_back(je);
        }
        return doc;
    }

    void jsonToScene(const json& doc) {
        m_reg.clear(); m_selected = NullEntity;
        m_timeLimit = doc.value("timeLimit", 0.0f);
        if (!doc.contains("entities")) return;
        for (const json& je : doc["entities"]) {
            Entity e = m_reg.create();
            if (je.contains("Transform2D")) { const json& j = je["Transform2D"]; m_reg.add<Transform2D>(e, Transform2D{jToVec(j["position"]), j["rotation"].get<float>(), jToVec(j["scale"])}); }
            if (je.contains("RectShape"))   { const json& j = je["RectShape"]; const json& c = j["color"]; m_reg.add<RectShape>(e, RectShape{jToVec(j["size"]), mjv::Color{c[0].get<std::uint8_t>(), c[1].get<std::uint8_t>(), c[2].get<std::uint8_t>(), 255}}); }
            if (je.contains("AABB"))        m_reg.add<AABB>(e, AABB{jToVec(je["AABB"]["halfSize"])});
            if (je.contains("RigidBody"))   { RigidBody rb; rb.gravityScale = je["RigidBody"]["gravityScale"].get<float>(); m_reg.add<RigidBody>(e, rb); }
            if (je.contains("Controllable")){ const json& j = je["Controllable"]; m_reg.add<Controllable>(e, Controllable{j["speed"].get<float>(), j["jumpForce"].get<float>()}); }
            if (je.contains("Patrol"))      { const json& j = je["Patrol"]; Patrol p; p.speed = j["speed"].get<float>(); p.range = j["range"].get<float>(); m_reg.add<Patrol>(e, p); }
            if (je.contains("Collectible")) m_reg.add<Collectible>(e, Collectible{je["Collectible"]["points"].get<int>()});
            if (je.contains("Goal"))        m_reg.add<Goal>(e, Goal{});
            if (je.contains("Health"))      m_reg.add<Health>(e, Health{je["Health"]["lives"].get<int>()});
            if (je.contains("SpriteAsset")) { const json& j = je["SpriteAsset"]; m_reg.add<SpriteAsset>(e, SpriteAsset{j["path"].get<std::string>(), jToVec(j["size"])}); }
        }
    }

    void setStatus(const std::string& s) { m_status = s; m_statusTimer = 3.0f; }
};

int main() {
    Editor app;
    app.run();
    return 0;
}
