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
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
struct Controllable {                                         // "joueur"
    float speed = 300.0f;
    float jumpForce = 820.0f;
    int maxJumps = 1;        // 2 = double saut
    // runtime :
    float coyote = 0.0f, buffer = 0.0f;
    int jumpsLeft = 0;
    bool wasGround = true;
};
struct Particle { Vec2 pos, vel; float life = 0, maxLife = 0; mjv::Color color; };
struct Tile {};                                               // tuile peinte (pour effacer/repérer)
struct Hazard {};                                             // piège : contact = perte de vie
struct Chase { float speed = 130.0f; float range = 360.0f; bool jump = true; };  // poursuit le joueur
struct Shooter { float interval = 1.6f; float bulletSpeed = 330.0f; float range = 480.0f; float timer = 0.0f; }; // tire
struct Projectile { Vec2 vel; float life = 3.0f; };           // tir (runtime)
struct Spawner { float interval = 3.0f; float timer = 0.0f; int maxAlive = 4; int kind = 0; }; // génère des ennemis
struct Patrol {                                               // ennemi va-et-vient
    float speed = 90.0f;
    float range = 140.0f;
    float dir = 1.0f;
    float originX = 0.0f;
    bool init = false;
};
struct AnimSprite {                                           // sprite animé (spritesheet)
    std::string path;
    int frameW = 48, frameH = 64;
    int idleRow = 0, idleFrames = 2;
    int walkRow = 1, walkFrames = 4;
    float frameTime = 0.12f;
    // runtime :
    int frame = 0;
    float timer = 0.0f;
    bool walking = false;
    bool faceLeft = false;
};
struct Collectible { int points = 100; };                     // pièce à ramasser
struct Goal {};                                               // objectif = fin de niveau
struct Health { int lives = 3; };                             // vies du joueur
struct Name { std::string value; };                           // nom affiché
struct EditorFlags { bool locked = false; bool hidden = false; }; // verrou / masque (édition)

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
    bool m_titleActive = false;   // écran-titre au lancement du Play
    bool m_paused = false;
    int m_menuIndex = 0;
    std::string m_levelName = "Mon niveau";
    bool m_titleScreen = true;
    Vec2 m_playerSpawn;
    float m_invuln = 0.0f;       // invincibilité après un coup (s)
    float m_timeLimit = 0.0f;    // limite de temps du niveau (0 = aucune)
    float m_timeLeft = 0.0f;
    int m_levelNum = 1;          // niveau courant (niveauN.json)
    mjv::Sound m_sfxCoin, m_sfxWin, m_sfxLose, m_sfxHit, m_sfxShoot;
    mjv::Music m_music;
    float m_musicVol = 0.5f, m_sfxVol = 0.85f;
    bool m_muted = false;
    bool m_musicStarted = false, m_musicPlaying = false;
    // Options / réglages.
    bool m_showOptions = false;
    int m_lang = 0;            // 0 = Francais, 1 = English
    int m_keyLeft = KEY_LEFT, m_keyRight = KEY_RIGHT, m_keyJump = KEY_SPACE;
    int m_rebinding = 0;       // 0=aucun, 1=gauche, 2=droite, 3=saut

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
    bool m_rotating = false;
    char m_hierFilter[48] = {0};
    std::vector<json> m_undo, m_redo;
    json m_clipboard;
    bool m_hasClipboard = false;
    std::vector<Particle> m_particles;
    float m_shake = 0.0f;
    bool m_paintMode = false;
    bool m_eraseMode = false;
    std::string m_paintTile = std::string(MJV_ASSET_DIR) + "/tile.png";

    void onStart() override {
        SetExitKey(KEY_NULL); // Echap sert au menu pause, pas à fermer
        std::srand(1234);
        rlImGuiSetup(true);
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        applyDarkTheme();
        m_target = LoadRenderTexture(kWorldW, kWorldH);
        const std::string dir = std::string(MJV_ASSET_DIR) + "/";
        m_sfxCoin.load(dir + "coin.wav");
        m_sfxWin.load(dir + "win.wav");
        m_sfxLose.load(dir + "lose.wav");
        m_sfxHit.load(dir + "bump.wav");
        m_sfxShoot.load(dir + "shoot.wav");
        m_music.load(dir + "music.wav");
        m_music.setLooping(true);
        applyVolumes();
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
        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = 1.18f; // interface un peu plus grande/lisible

        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 7.0f; s.ChildRounding = 7.0f; s.PopupRounding = 7.0f;
        s.FrameRounding = 5.0f; s.GrabRounding = 5.0f; s.TabRounding = 6.0f;
        s.ScrollbarRounding = 9.0f;
        s.WindowPadding = ImVec2(12, 12); s.FramePadding = ImVec2(9, 5);
        s.ItemSpacing = ImVec2(9, 8); s.ItemInnerSpacing = ImVec2(7, 5);
        s.WindowBorderSize = 1.0f; s.FrameBorderSize = 0.0f; s.PopupBorderSize = 1.0f;
        s.ScrollbarSize = 13.0f; s.GrabMinSize = 11.0f;
        s.WindowTitleAlign = ImVec2(0.02f, 0.5f);
        s.WindowMenuButtonPosition = ImGuiDir_None;

        auto rgb = [](int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); };
        const ImVec4 base   = rgb(24, 26, 33);
        const ImVec4 panel  = rgb(33, 36, 45);
        const ImVec4 panel2 = rgb(42, 46, 57);
        const ImVec4 accent = rgb(78, 154, 241);
        const ImVec4 accentH= rgb(108, 176, 255);
        const ImVec4 text   = rgb(226, 229, 238);
        const ImVec4 textD  = rgb(126, 132, 148);
        const ImVec4 border = rgb(54, 58, 72);

        ImVec4* c = s.Colors;
        c[ImGuiCol_Text] = text;             c[ImGuiCol_TextDisabled] = textD;
        c[ImGuiCol_WindowBg] = base;         c[ImGuiCol_ChildBg] = base;
        c[ImGuiCol_PopupBg] = rgb(28, 30, 38);
        c[ImGuiCol_Border] = border;         c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_MenuBarBg] = rgb(20, 22, 28);
        c[ImGuiCol_FrameBg] = panel;         c[ImGuiCol_FrameBgHovered] = panel2; c[ImGuiCol_FrameBgActive] = panel2;
        c[ImGuiCol_TitleBg] = rgb(20, 22, 28); c[ImGuiCol_TitleBgActive] = rgb(20, 22, 28); c[ImGuiCol_TitleBgCollapsed] = rgb(20, 22, 28);
        c[ImGuiCol_Header] = panel2;         c[ImGuiCol_HeaderHovered] = rgb(56, 62, 78); c[ImGuiCol_HeaderActive] = accent;
        c[ImGuiCol_Button] = panel2;         c[ImGuiCol_ButtonHovered] = rgb(58, 96, 150); c[ImGuiCol_ButtonActive] = accent;
        c[ImGuiCol_CheckMark] = accent;      c[ImGuiCol_SliderGrab] = accent; c[ImGuiCol_SliderGrabActive] = accentH;
        c[ImGuiCol_Separator] = border;      c[ImGuiCol_SeparatorHovered] = accent; c[ImGuiCol_SeparatorActive] = accentH;
        c[ImGuiCol_Tab] = panel;             c[ImGuiCol_TabHovered] = accentH; c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = panel;    c[ImGuiCol_TabUnfocusedActive] = panel2;
        c[ImGuiCol_ScrollbarBg] = base;      c[ImGuiCol_ScrollbarGrab] = panel2; c[ImGuiCol_ScrollbarGrabHovered] = rgb(70, 76, 94);
        c[ImGuiCol_ResizeGrip] = panel2;     c[ImGuiCol_ResizeGripHovered] = accent; c[ImGuiCol_ResizeGripActive] = accentH;
        c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.5f);
        c[ImGuiCol_DragDropTarget] = rgb(255, 200, 80);
    }

    // Couleur d'une entité selon son comportement (pour la hiérarchie).
    ImVec4 entityColor(Entity e) {
        if (m_reg.has<Controllable>(e)) return ImVec4(0.45f, 0.70f, 1.00f, 1.0f); // joueur
        if (m_reg.has<Chase>(e) || m_reg.has<Patrol>(e) || m_reg.has<Shooter>(e)) return ImVec4(1.00f, 0.45f, 0.45f, 1.0f); // ennemi
        if (m_reg.has<Collectible>(e)) return ImVec4(1.00f, 0.82f, 0.30f, 1.0f); // piece
        if (m_reg.has<Goal>(e))        return ImVec4(0.50f, 0.92f, 0.55f, 1.0f); // objectif
        if (m_reg.has<Spawner>(e))     return ImVec4(0.74f, 0.55f, 1.00f, 1.0f); // generateur
        if (m_reg.has<Tile>(e))        return ImVec4(0.80f, 0.66f, 0.45f, 1.0f); // tuile
        return ImVec4(0.78f, 0.82f, 0.90f, 1.0f);
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
        m_reg.add<Name>(e, Name{"Plateforme"}); m_selected = e; setStatus("Plateforme creee"); return e;
    }
    Entity spawnCrate(Vec2 pos) {
        Entity e = makeSpriteEntity("crate.png", pos);
        m_reg.add<AABB>(e, AABB{m_reg.get<SpriteAsset>(e).size * 0.5f});
        m_reg.add<RigidBody>(e, RigidBody{});
        m_reg.add<Name>(e, Name{"Caisse"}); m_selected = e; setStatus("Caisse creee"); return e;
    }
    // Tuile de sol : solide statique. À stamper en grille pour bâtir des plateformes.
    Entity spawnTile(Vec2 pos) {
        Entity e = makeSpriteEntity("tile.png", pos);
        m_reg.add<AABB>(e, AABB{m_reg.get<SpriteAsset>(e).size * 0.5f});
        m_reg.add<Name>(e, Name{"Tuile"}); m_selected = e; setStatus("Tuile creee"); return e;
    }
    Entity spawnPlayer(Vec2 pos) {
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{pos});
        AnimSprite a; a.path = assetPath("hero_sheet.png");
        a.frameW = 48; a.frameH = 64; a.idleRow = 0; a.idleFrames = 2; a.walkRow = 1; a.walkFrames = 4;
        m_reg.add<AnimSprite>(e, a);
        m_reg.add<AABB>(e, AABB{{18.0f, 30.0f}});
        m_reg.add<RigidBody>(e, RigidBody{});
        m_reg.add<Controllable>(e, Controllable{});
        m_reg.add<Health>(e, Health{});
        m_reg.add<Name>(e, Name{"Joueur"}); m_selected = e; setStatus("Joueur cree (anime, jouable en Play)"); return e;
    }
    // Ennemi avec un comportement : 0 = patrouille, 1 = poursuite, 2 = tireur.
    Entity makeEnemyEntity(Vec2 pos, int behavior, const char* name) {
        Entity e = makeSpriteEntity("enemy.png", pos);
        m_reg.add<AABB>(e, AABB{m_reg.get<SpriteAsset>(e).size * 0.5f});
        m_reg.add<RigidBody>(e, RigidBody{});
        if (behavior == 1)      m_reg.add<Chase>(e, Chase{});
        else if (behavior == 2) m_reg.add<Shooter>(e, Shooter{});
        else                    m_reg.add<Patrol>(e, Patrol{});
        m_reg.add<Name>(e, Name{name});
        return e;
    }
    Entity spawnEnemy(Vec2 pos)   { Entity e = makeEnemyEntity(pos, 0, "Ennemi");   m_selected = e; setStatus("Ennemi cree (patrouille)"); return e; }
    Entity spawnChaser(Vec2 pos)  { Entity e = makeEnemyEntity(pos, 1, "Chasseur"); m_selected = e; setStatus("Chasseur cree (poursuit)"); return e; }
    Entity spawnShooter(Vec2 pos) { Entity e = makeEnemyEntity(pos, 2, "Tireur");   m_selected = e; setStatus("Tireur cree (tire)"); return e; }
    Entity spawnSpawner(Vec2 pos) {
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{pos});
        m_reg.add<RectShape>(e, RectShape{{44.0f, 44.0f}, mjv::Color{150, 90, 200, 255}});
        m_reg.add<Spawner>(e, Spawner{});
        m_reg.add<Name>(e, Name{"Generateur"}); m_selected = e; setStatus("Generateur cree"); return e;
    }
    Entity spawnHazard(Vec2 pos) {
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{pos});
        m_reg.add<RectShape>(e, RectShape{{42.0f, 18.0f}, mjv::Color{200, 40, 40, 255}});
        m_reg.add<Hazard>(e, Hazard{}); // pas d'AABB : déclencheur, pas un mur solide
        m_reg.add<Name>(e, Name{"Piege"}); m_selected = e; setStatus("Piege cree"); return e;
    }
    // Pièce à ramasser : sprite SANS AABB (pas un mur solide, juste un trigger).
    Entity spawnCoin(Vec2 pos) {
        Entity e = makeSpriteEntity("coin.png", pos);
        m_reg.add<Collectible>(e, Collectible{});
        m_reg.add<Name>(e, Name{"Piece"}); m_selected = e; setStatus("Piece creee"); return e;
    }
    // Objectif de fin de niveau (trigger, sans collision physique).
    Entity spawnGoal(Vec2 pos) {
        Entity e = makeSpriteEntity("flag.png", pos);
        m_reg.add<Goal>(e, Goal{});
        m_reg.add<Name>(e, Name{"Objectif"}); m_selected = e; setStatus("Objectif cree"); return e;
    }
    Entity spawnSprite(const std::string& path, Vec2 pos) {
        ::Texture2D& t = thumbnail(path);
        const Vec2 size{static_cast<float>(t.width), static_cast<float>(t.height)};
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{pos});
        m_reg.add<AABB>(e, AABB{size * 0.5f});
        m_reg.add<SpriteAsset>(e, SpriteAsset{path, size});
        m_reg.add<Name>(e, Name{"Image"}); m_selected = e; setStatus("Image ajoutee a la scene"); return e;
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
        if (m_reg.has<AnimSprite>(e))  { AnimSprite& a = m_reg.get<AnimSprite>(e); return {a.frameW * 0.5f, a.frameH * 0.5f}; }
        return {20.0f, 20.0f};
    }

    // ----------------------------------------------------------- Play/Stop
    void play() {
        m_snapshot = sceneToJson();
        m_playing = true;
        m_score = 0; m_won = false; m_lost = false; m_invuln = 0.0f;
        m_particles.clear(); m_shake = 0.0f;
        m_paused = false; m_menuIndex = 0; m_titleActive = m_titleScreen;
        m_musicStarted = false; m_musicPlaying = false;
        m_timeLeft = m_timeLimit;
        Vec2 sp; if (firstPlayer(sp)) m_playerSpawn = sp;
        setStatus("Lecture - Stop pour revenir a l'edition");
    }
    void stop() {
        m_playing = false;
        m_particles.clear(); m_shake = 0.0f;
        m_music.stop(); m_musicStarted = false; m_musicPlaying = false;
        jsonToScene(m_snapshot);
        setStatus("Edition");
    }
    // Navigation d'un menu au clavier ; renvoie l'option choisie (ou -1).
    int menuNav(int count) {
        if (Input::isPressed(Key::Up)   || Input::isPressed(Key::W) || Input::isPressed(Key::Z)) m_menuIndex = (m_menuIndex + count - 1) % count;
        if (Input::isPressed(Key::Down) || Input::isPressed(Key::S)) m_menuIndex = (m_menuIndex + 1) % count;
        if (Input::isPressed(Key::Enter) || Input::isPressed(Key::Space)) return m_menuIndex;
        return -1;
    }
    void restartLevel() {
        jsonToScene(m_snapshot);
        m_score = 0; m_won = false; m_lost = false; m_paused = false; m_invuln = 0.0f;
        m_particles.clear(); m_shake = 0.0f; m_menuIndex = 0; m_titleActive = false;
        m_music.stop(); m_musicStarted = false; m_musicPlaying = false;
        m_timeLeft = m_timeLimit;
        Vec2 sp; if (firstPlayer(sp)) m_playerSpawn = sp;
    }

    bool firstPlayer(Vec2& out) {
        bool found = false;
        m_reg.view<Controllable, Transform2D>([&](Entity, Controllable&, Transform2D& t) {
            if (!found) { out = t.position; found = true; }
        });
        return found;
    }

    // ----------------------------------------------------------- systèmes (Play)
    void controlSystem(float dt) {
        if (m_won || m_lost) return; // partie terminée : on fige le joueur
        const bool kbd = !ImGui::GetIO().WantCaptureKeyboard;
        const int L = (kbd && (Input::isDown(Key::Left)  || Input::isDown(Key::A) || Input::isDown(Key::Q) || IsKeyDown(m_keyLeft))) ? 1 : 0;
        const int R = (kbd && (Input::isDown(Key::Right) || Input::isDown(Key::D) || IsKeyDown(m_keyRight))) ? 1 : 0;
        const bool jumpPressed = kbd && (Input::isPressed(Key::Space) || Input::isPressed(Key::Up) ||
                                         Input::isPressed(Key::W) || Input::isPressed(Key::Z) || IsKeyPressed(m_keyJump));
        const mjv::Color dust{220, 220, 235, 255};
        m_reg.view<Controllable, RigidBody, Transform2D>([&](Entity e, Controllable& c, RigidBody& rb, Transform2D& tr) {
            rb.velocity.x = static_cast<float>(R - L) * c.speed;
            const float feetY = tr.position.y + halfExtents(e).y;
            // atterrissage : poussière + petite secousse
            if (rb.onGround && !c.wasGround) { emit({tr.position.x, feetY}, 8, dust, 130.0f); m_shake = std::max(m_shake, 0.16f); }
            // coyote time + recharge des sauts quand on est au sol
            if (rb.onGround) { c.coyote = 0.10f; c.jumpsLeft = std::max(1, c.maxJumps); }
            else c.coyote = std::max(0.0f, c.coyote - dt);
            // jump buffering : on mémorise l'appui un court instant
            c.buffer = jumpPressed ? 0.12f : std::max(0.0f, c.buffer - dt);
            if (c.buffer > 0.0f) {
                const bool fromGround = c.coyote > 0.0f;
                if (fromGround || c.jumpsLeft > 0) {
                    rb.velocity.y = -c.jumpForce;
                    c.buffer = 0.0f;
                    if (fromGround) { c.coyote = 0.0f; c.jumpsLeft = std::max(1, c.maxJumps) - 1; }
                    else            { c.jumpsLeft -= 1; } // saut en l'air (double saut)
                    emit({tr.position.x, feetY}, 6, dust, 110.0f);
                }
            }
            c.wasGround = rb.onGround;
        });
    }
    // Fait avancer les sprites animés (marche si l'entité se déplace au sol).
    void animationSystem(float dt) {
        m_reg.view<AnimSprite>([&](Entity e, AnimSprite& a) {
            float vx = 0.0f;
            if (m_reg.has<RigidBody>(e)) vx = m_reg.get<RigidBody>(e).velocity.x;
            a.walking = std::abs(vx) > 8.0f;
            if (vx > 8.0f) a.faceLeft = false; else if (vx < -8.0f) a.faceLeft = true;
            const int frames = std::max(1, a.walking ? a.walkFrames : a.idleFrames);
            a.timer += dt;
            if (a.timer >= a.frameTime) { a.timer = 0.0f; a.frame = (a.frame + 1) % frames; }
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

    bool nearestPlayer(Vec2 from, Vec2& out) {
        bool found = false; float best = 1e18f;
        m_reg.view<Controllable, Transform2D>([&](Entity, Controllable&, Transform2D& t) {
            const float dd = (t.position.x - from.x) * (t.position.x - from.x) + (t.position.y - from.y) * (t.position.y - from.y);
            if (dd < best) { best = dd; out = t.position; found = true; }
        });
        return found;
    }

    // Ennemi qui poursuit le joueur (et saute pour le suivre).
    void chaseSystem(float) {
        m_reg.view<Chase, RigidBody, Transform2D>([&](Entity, Chase& ch, RigidBody& rb, Transform2D& tr) {
            Vec2 pp;
            if (!nearestPlayer(tr.position, pp)) { rb.velocity.x = 0.0f; return; }
            const float dx = pp.x - tr.position.x;
            if (std::abs(dx) < ch.range && std::abs(dx) > 4.0f) {
                rb.velocity.x = (dx < 0.0f ? -1.0f : 1.0f) * ch.speed;
                if (ch.jump && rb.onGround && pp.y < tr.position.y - 40.0f) rb.velocity.y = -720.0f;
            } else {
                rb.velocity.x = 0.0f;
            }
        });
    }

    void spawnProjectile(Vec2 pos, Vec2 vel) {
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{pos});
        m_reg.add<RectShape>(e, RectShape{{12.0f, 12.0f}, mjv::Color{255, 120, 60, 255}});
        m_reg.add<Projectile>(e, Projectile{vel, 3.0f});
    }

    // Ennemi qui tire vers le joueur à intervalles réguliers.
    void shooterSystem(float dt) {
        std::vector<std::pair<Vec2, Vec2>> shots;
        m_reg.view<Shooter, Transform2D>([&](Entity, Shooter& s, Transform2D& tr) {
            s.timer += dt;
            Vec2 pp;
            if (!nearestPlayer(tr.position, pp)) return;
            const float dx = pp.x - tr.position.x, dy = pp.y - tr.position.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < s.range && s.timer >= s.interval) {
                s.timer = 0.0f;
                const Vec2 dir = dist > 0.0f ? Vec2{dx / dist, dy / dist} : Vec2{1.0f, 0.0f};
                shots.push_back({tr.position, dir * s.bulletSpeed});
                m_sfxShoot.play();
            }
        });
        for (const auto& sh : shots) spawnProjectile(sh.first, sh.second);
    }

    // Déplace les tirs ; touche le joueur ou un mur = destruction.
    void projectileSystem(float dt) {
        struct B { Vec2 c, h; };
        std::vector<B> statics;
        m_reg.view<AABB, Transform2D>([&](Entity e, AABB& b, Transform2D& t) { if (!m_reg.has<RigidBody>(e)) statics.push_back({t.position, b.halfSize}); });
        struct P { Entity e; Vec2 c, h; };
        std::vector<P> players;
        m_reg.view<Controllable, Transform2D>([&](Entity e, Controllable&, Transform2D& t) { players.push_back({e, t.position, halfExtents(e)}); });
        std::vector<Entity> dead;
        m_reg.view<Projectile, Transform2D>([&](Entity e, Projectile& pr, Transform2D& t) {
            t.position += pr.vel * dt;
            pr.life -= dt;
            const Vec2 h = halfExtents(e);
            bool kill = pr.life <= 0.0f;
            for (const B& s : statics) if (aabbOverlap(t.position, h, s.c, s.h)) kill = true;
            for (const P& p : players) if (m_invuln <= 0.0f && aabbOverlap(t.position, h, p.c, p.h)) { hitPlayer(p.e); kill = true; }
            if (kill) dead.push_back(e);
        });
        for (Entity e : dead) m_reg.destroy(e);
    }

    // Génère des ennemis à intervalle, sous un plafond.
    void spawnerSystem(float dt) {
        int alive = 0;
        m_reg.view<Patrol>([&](Entity, Patrol&) { ++alive; });
        m_reg.view<Chase>([&](Entity, Chase&) { ++alive; });
        std::vector<std::pair<Vec2, int>> toSpawn;
        m_reg.view<Spawner, Transform2D>([&](Entity, Spawner& s, Transform2D& tr) {
            s.timer += dt;
            if (s.timer >= s.interval && alive < s.maxAlive) { s.timer = 0.0f; toSpawn.push_back({tr.position, s.kind}); ++alive; }
        });
        for (const auto& ts : toSpawn) makeEnemyEntity(ts.first, ts.second == 1 ? 1 : 0, "Ennemi");
    }

    void win()  { if (!m_won && !m_lost) { m_won = true; m_sfxWin.play(); } }
    void lose() { if (!m_won && !m_lost) { m_lost = true; m_sfxLose.play(); } }

    // Le joueur est touché : perd une vie (et respawn) ou perd la partie.
    void hitPlayer(Entity e) {
        m_invuln = 1.4f;
        m_shake = std::max(m_shake, 0.35f);
        if (m_reg.has<Transform2D>(e)) emit(m_reg.get<Transform2D>(e).position, 14, mjv::Color{230, 80, 80, 255}, 240.0f);
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

    static float frand() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }

    // Émet un petit nuage de particules.
    void emit(Vec2 p, int n, mjv::Color col, float speed) {
        for (int i = 0; i < n; ++i) {
            const float a = frand() * 6.2832f;
            const float s = speed * (0.3f + 0.7f * frand());
            Particle pt;
            pt.pos = p;
            pt.vel = {std::cos(a) * s, std::sin(a) * s - speed * 0.3f};
            pt.maxLife = pt.life = 0.4f + 0.4f * frand();
            pt.color = col;
            m_particles.push_back(pt);
        }
    }
    void particleSystem(float dt) {
        for (Particle& pt : m_particles) {
            pt.vel.y += 1400.0f * dt;
            pt.pos += pt.vel * dt;
            pt.life -= dt;
        }
        m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
                                         [](const Particle& p) { return p.life <= 0.0f; }),
                          m_particles.end());
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
                if (aabbOverlap(p.c, p.h, t.position, h)) { m_score += c.points; m_sfxCoin.play(); emit(t.position, 10, mjv::Color{253, 203, 0, 255}, 200.0f); picked.push_back(e); break; }
        });
        for (Entity e : picked) m_reg.destroy(e);

        if (m_invuln <= 0.0f) {
            m_reg.view<Patrol, Transform2D>([&](Entity e, Patrol&, Transform2D& t) {
                const Vec2 h = halfExtents(e);
                // petite marge : les corps se touchent (poussés) sans se chevaucher
                for (const P& p : players)
                    if (aabbOverlap(p.c, {p.h.x + 3.0f, p.h.y + 3.0f}, t.position, h)) { hitPlayer(p.e); return; }
            });
            m_reg.view<Hazard, Transform2D>([&](Entity e, Hazard&, Transform2D& t) {
                const Vec2 h = halfExtents(e);
                for (const P& p : players)
                    if (aabbOverlap(p.c, p.h, t.position, h)) { hitPlayer(p.e); return; }
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
        if (!m_playing) { handleShortcuts(); handleViewportInteraction(); }

        if (m_playing) {
            if (!m_titleActive && !m_won && !m_lost && Input::isPressed(Key::Escape)) { m_paused = !m_paused; m_menuIndex = 0; }

            if (m_titleActive) {                       // --- écran titre ---
                if (Input::isPressed(Key::Enter) || Input::isPressed(Key::Space)) m_titleActive = false;
            } else if (m_paused) {                     // --- menu pause ---
                const int sel = menuNav(3);
                if (sel == 0) m_paused = false;
                else if (sel == 1) restartLevel();
                else if (sel == 2) { stop(); renderSceneToTexture(); return; }
            } else if (m_won || m_lost) {              // --- menu de fin ---
                const bool next = m_won && hasNextLevel();
                const int sel = menuNav(2);
                if (sel == 0) { if (next) advanceLevel(); else restartLevel(); }
                else if (sel == 1) { stop(); renderSceneToTexture(); return; }
            } else {                                   // --- en jeu ---
                if (m_invuln > 0.0f) m_invuln -= dt;
                if (m_timeLimit > 0.0f) { m_timeLeft -= dt; if (m_timeLeft <= 0.0f) { m_timeLeft = 0.0f; lose(); } }
                controlSystem(dt);
                patrolSystem(dt);
                chaseSystem(dt);
                physicsStep(m_reg, std::min(dt, 0.033f), {0.0f, kGravity});
                animationSystem(dt);
                gameplaySystem();
                shooterSystem(dt);
                projectileSystem(dt);
                spawnerSystem(dt);
                particleSystem(dt);
                m_shake = std::max(0.0f, m_shake - dt);
            }
        }
        // Musique de fond : seulement pendant le jeu (pas en édition/titre/pause/fin).
        const bool wantMusic = m_playing && !m_titleActive && !m_paused && !m_won && !m_lost;
        if (wantMusic && !m_musicPlaying) { if (!m_musicStarted) { m_music.play(); m_musicStarted = true; } else m_music.resume(); m_musicPlaying = true; }
        else if (!wantMusic && m_musicPlaying) { m_music.pause(); m_musicPlaying = false; }
        renderSceneToTexture();
    }

    void applyVolumes() {
        const float sfx = m_muted ? 0.0f : m_sfxVol;
        const float mus = m_muted ? 0.0f : m_musicVol;
        m_sfxCoin.setVolume(sfx); m_sfxWin.setVolume(sfx); m_sfxLose.setVolume(sfx);
        m_sfxHit.setVolume(sfx); m_sfxShoot.setVolume(sfx);
        m_music.setVolume(mus);
        Audio::setMasterVolume(m_muted ? 0.0f : 1.0f); // coupe-son maître garanti
    }

    // Petit helper de traduction (selon la langue choisie).
    const char* t(const char* fr, const char* en) const { return m_lang == 1 ? en : fr; }

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
            if (isLocked(e) || isHidden(e)) return; // non sélectionnable
            const Vec2 h = halfExtents(e);
            if (std::abs(p.x - tr.position.x) <= h.x && std::abs(p.y - tr.position.y) <= h.y) hit = e;
        });
        return hit;
    }
    void handleShortcuts() {
        if (ImGui::GetIO().WantCaptureKeyboard) return;
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (ctrl && IsKeyPressed(KEY_Z)) undoAction();
        if (ctrl && IsKeyPressed(KEY_Y)) redoAction();
        if (ctrl && IsKeyPressed(KEY_D)) duplicateSelected();
        if (ctrl && IsKeyPressed(KEY_C)) copySelected();
        if (ctrl && IsKeyPressed(KEY_V)) paste();
        if (IsKeyPressed(KEY_DELETE))    deleteSelected();
    }

    // Pose (ou efface) une tuile sur la cellule de grille sous le point monde.
    void paintAt(Vec2 world) {
        float cell = static_cast<float>(thumbnail(m_paintTile).width);
        if (cell < 1.0f) cell = 64.0f;
        const Vec2 c{std::round(world.x / cell) * cell, std::round(world.y / cell) * cell};
        Entity found = NullEntity;
        m_reg.view<Tile, Transform2D>([&](Entity e, Tile&, Transform2D& t) {
            if (std::abs(t.position.x - c.x) < cell * 0.5f && std::abs(t.position.y - c.y) < cell * 0.5f) found = e;
        });
        if (m_eraseMode) {
            if (found != NullEntity) { if (m_selected == found) m_selected = NullEntity; m_reg.destroy(found); }
            return;
        }
        if (found != NullEntity) return; // déjà une tuile ici
        ::Texture2D& tex = thumbnail(m_paintTile);
        const Vec2 size{static_cast<float>(tex.width), static_cast<float>(tex.height)};
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{c});
        m_reg.add<SpriteAsset>(e, SpriteAsset{m_paintTile, size});
        m_reg.add<AABB>(e, AABB{size * 0.5f});
        m_reg.add<Tile>(e, Tile{});
        m_reg.add<Name>(e, Name{"Tuile"});
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
        // Mode peinture de tuiles : clic gauche = poser/effacer sur la grille.
        if (m_paintMode) {
            if (m_vpHovered) {
                if (Input::mousePressed(Mouse::Left)) pushUndo();
                if (Input::mouseDown(Mouse::Left)) paintAt(screenToWorld(Input::mousePosition()));
            }
            return;
        }
        if (!m_vpHovered) { if (Input::mouseReleased(Mouse::Left)) { m_dragging = false; m_resizeHandle = -1; } return; }

        const Vec2 w = screenToWorld(Input::mousePosition());
        if (Input::mousePressed(Mouse::Left)) {
            const float thr = 12.0f / m_zoom;
            bool onRotate = false;
            if (m_reg.valid(m_selected) && m_reg.has<Transform2D>(m_selected)) {
                const Vec2 rh = rotateHandlePos(m_selected);
                if (std::abs(w.x - rh.x) <= thr && std::abs(w.y - rh.y) <= thr) { pushUndo(); m_rotating = true; onRotate = true; }
            }
            if (!onRotate) {
                const int handle = handleAt(w);
                if (handle >= 0) {
                    pushUndo(); // état avant redimensionnement
                    m_resizeHandle = handle;
                    const Vec2 c = m_reg.get<Transform2D>(m_selected).position;
                    const Vec2 h = halfExtents(m_selected);
                    const Vec2 cs[4] = {{c.x - h.x, c.y - h.y}, {c.x + h.x, c.y - h.y}, {c.x + h.x, c.y + h.y}, {c.x - h.x, c.y + h.y}};
                    m_fixedCorner = cs[(handle + 2) % 4];
                } else {
                    const Entity hit = pickEntity(w);
                    if (hit != NullEntity) { m_selected = hit; m_dragging = true; pushUndo(); m_dragOffset = m_reg.get<Transform2D>(hit).position - w; }
                }
            }
        }

        if (m_rotating && Input::mouseDown(Mouse::Left) && m_reg.valid(m_selected) && m_reg.has<Transform2D>(m_selected)) {
            const Vec2 c = m_reg.get<Transform2D>(m_selected).position;
            m_reg.get<Transform2D>(m_selected).rotation = std::atan2(w.x - c.x, -(w.y - c.y)) * 57.2958f;
        } else if (m_resizeHandle >= 0 && Input::mouseDown(Mouse::Left) && m_reg.valid(m_selected)) {
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
        if (Input::mouseReleased(Mouse::Left)) { m_dragging = false; m_resizeHandle = -1; m_rotating = false; }
    }

    Vec2 rotateHandlePos(Entity e) {
        const Transform2D& t = m_reg.get<Transform2D>(e);
        const float r = t.rotation * 0.0174533f;
        const float dist = halfExtents(e).y + 24.0f / m_zoom;
        return {t.position.x + std::sin(r) * dist, t.position.y - std::cos(r) * dist};
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
            if (m_shake > 0.0f) { cam.target.x += (frand() * 2 - 1) * m_shake * 20.0f; cam.target.y += (frand() * 2 - 1) * m_shake * 20.0f; }
        }
        BeginMode2D(cam);
        if (!m_playing && m_snap) drawGrid();

        m_reg.view<Transform2D, RectShape>([&](Entity e, Transform2D& tr, RectShape& s) {
            if (!m_playing && isHidden(e)) return;
            Graphics::drawRectangleCentered(tr.position, s.size, s.color, tr.rotation);
            Graphics::drawRectangleOutlineCentered(tr.position, s.size, mjv::Color{20, 20, 28, 255}, 1.5f);
        });
        m_reg.view<Transform2D, SpriteAsset>([&](Entity e, Transform2D& tr, SpriteAsset& sp) {
            if (!m_playing && isHidden(e)) return;
            ::Texture2D& t = thumbnail(sp.path);
            const ::Rectangle src{0, 0, static_cast<float>(t.width), static_cast<float>(t.height)};
            const ::Rectangle dst{tr.position.x, tr.position.y, sp.size.x * tr.scale.x, sp.size.y * tr.scale.y};
            DrawTexturePro(t, src, dst, ::Vector2{dst.width * 0.5f, dst.height * 0.5f}, tr.rotation, ::WHITE);
        });
        // Sprites animés (joueur).
        m_reg.view<Transform2D, AnimSprite>([&](Entity e, Transform2D& tr, AnimSprite& a) {
            if (!m_playing && isHidden(e)) return;
            ::Texture2D& t = thumbnail(a.path);
            const bool walk = m_playing && a.walking;
            const int row = walk ? a.walkRow : a.idleRow;
            const int frames = std::max(1, walk ? a.walkFrames : a.idleFrames);
            const int fr = m_playing ? (a.frame % frames) : 0;
            const float flip = a.faceLeft ? -1.0f : 1.0f;
            const ::Rectangle src{static_cast<float>(fr * a.frameW), static_cast<float>(row * a.frameH),
                                  flip * static_cast<float>(a.frameW), static_cast<float>(a.frameH)};
            const ::Rectangle dst{tr.position.x, tr.position.y, a.frameW * tr.scale.x, a.frameH * tr.scale.y};
            DrawTexturePro(t, src, dst, ::Vector2{dst.width * 0.5f, dst.height * 0.5f}, tr.rotation, ::WHITE);
        });
        // Particules.
        for (const Particle& pt : m_particles) {
            const float k = pt.maxLife > 0.0f ? pt.life / pt.maxLife : 0.0f;
            const float sz = 5.0f * k + 1.0f;
            DrawRectangle(static_cast<int>(pt.pos.x - sz * 0.5f), static_cast<int>(pt.pos.y - sz * 0.5f),
                          static_cast<int>(sz), static_cast<int>(sz),
                          ::Color{pt.color.r, pt.color.g, pt.color.b, static_cast<unsigned char>(255 * k)});
        }
        if (!m_playing && m_reg.valid(m_selected) && m_reg.has<Transform2D>(m_selected)) {
            const Vec2 h = halfExtents(m_selected);
            const Vec2 sc = m_reg.get<Transform2D>(m_selected).position;
            Graphics::drawRectangleOutlineCentered(sc, h * 2.0f + Vec2{8, 8}, Colors::Yellow, 2.5f);
            drawHandles(m_selected);
            // Poignée de rotation (cercle vert relié au centre).
            const Vec2 rh = rotateHandlePos(m_selected);
            DrawLine(static_cast<int>(sc.x), static_cast<int>(sc.y), static_cast<int>(rh.x), static_cast<int>(rh.y), ::Color{60, 180, 255, 200});
            DrawCircle(static_cast<int>(rh.x), static_cast<int>(rh.y), 6.0f / m_zoom, ::Color{90, 210, 120, 255});
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

    // Liste d'options de menu centrée, l'option courante surlignée.
    void drawMenuOptions(const char* const opts[], int count, float startY) {
        for (int i = 0; i < count; ++i) {
            const bool seld = (i == m_menuIndex);
            const std::string s = seld ? ("> " + std::string(opts[i]) + " <") : std::string(opts[i]);
            drawCentered(s, startY + i * 48.0f, 34, seld ? mjv::Color{255, 230, 120, 255} : mjv::Color{200, 205, 220, 255});
        }
    }

    void drawHud() {
        const mjv::Color white{245, 245, 245, 255};
        const mjv::Color shadow{0, 0, 0, 140};

        if (!m_titleActive) { // HUD de jeu
            const std::string sc = t("Score : ", "Score: ") + std::to_string(m_score);
            Graphics::drawText(sc, {26.0f, 22.0f}, 38, shadow);
            Graphics::drawText(sc, {24.0f, 20.0f}, 38, white);
            const int lives = playerLives();
            if (lives >= 0) Graphics::drawText(t("Vies : ", "Lives: ") + std::to_string(lives), {24.0f, 66.0f}, 30, white);
            if (m_timeLimit > 0.0f)
                Graphics::drawText(t("Temps : ", "Time: ") + std::to_string(static_cast<int>(std::ceil(m_timeLeft))), {24.0f, 104.0f}, 30, white);
            Graphics::drawText(t("Niveau ", "Level ") + std::to_string(m_levelNum), {kWorldW - 210.0f, 22.0f}, 30, white);
        }

        const float cy = kWorldH * 0.5f;
        if (m_titleActive) {                    // écran-titre
            Graphics::drawRectangle({0.0f, 0.0f}, {kWorldW, kWorldH}, mjv::Color{12, 14, 20, 200});
            drawCentered(m_levelName, cy - 100.0f, 86, mjv::Color{120, 200, 255, 255});
            drawCentered(t("Appuie sur Entree pour jouer", "Press Enter to play"), cy + 24.0f, 32, white);
            drawCentered(t("Fleches / ZQSD : bouger     Espace : sauter", "Arrows / WASD: move     Space: jump"), cy + 72.0f, 22, mjv::Color{180, 185, 200, 255});
        } else if (m_paused) {                  // menu pause
            Graphics::drawRectangle({0.0f, 0.0f}, {kWorldW, kWorldH}, mjv::Color{12, 14, 20, 185});
            drawCentered("PAUSE", cy - 130.0f, 90, mjv::Color{255, 220, 120, 255});
            const char* opts[3] = {t("Reprendre", "Resume"), t("Recommencer", "Restart"), t("Quitter (editer)", "Quit (edit)")};
            drawMenuOptions(opts, 3, cy - 20.0f);
            drawCentered(t("Fleches + Entree pour choisir", "Arrows + Enter to choose"), cy + 160.0f, 22, mjv::Color{150, 155, 170, 255});
        } else if (m_won || m_lost) {           // menu de fin
            const bool next = m_won && hasNextLevel();
            Graphics::drawRectangle({0.0f, 0.0f}, {kWorldW, kWorldH}, mjv::Color{12, 14, 20, 185});
            const std::string title = next ? t("Niveau termine !", "Level complete!") : (m_won ? t("VICTOIRE !", "YOU WIN!") : t("PERDU", "GAME OVER"));
            drawCentered(title, cy - 150.0f, 90, m_won ? mjv::Color{120, 230, 120, 255} : mjv::Color{235, 90, 90, 255});
            drawCentered(t("Score : ", "Score: ") + std::to_string(m_score), cy - 64.0f, 40, white);
            const char* opts[2] = {next ? t("Niveau suivant", "Next level") : t("Rejouer", "Play again"), t("Quitter (editer)", "Quit (edit)")};
            drawMenuOptions(opts, 2, cy + 10.0f);
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
        drawOptions();
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
                if (ImGui::MenuItem("Joueur"))     { pushUndo(); spawnPlayer({250.0f, 200.0f}); }
                if (ImGui::MenuItem("Plateforme")) { pushUndo(); spawnPlatform({400.0f, 350.0f}); }
                if (ImGui::MenuItem("Tuile"))      { pushUndo(); spawnTile(snap(m_camTarget)); }
                if (ImGui::MenuItem("Caisse"))     { pushUndo(); spawnCrate({450.0f, 150.0f}); }
                if (ImGui::MenuItem("Ennemi"))     { pushUndo(); spawnEnemy({500.0f, 200.0f}); }
                if (ImGui::MenuItem("Chasseur"))   { pushUndo(); spawnChaser({500.0f, 200.0f}); }
                if (ImGui::MenuItem("Tireur"))     { pushUndo(); spawnShooter({500.0f, 200.0f}); }
                if (ImGui::MenuItem("Generateur")) { pushUndo(); spawnSpawner({500.0f, 200.0f}); }
                if (ImGui::MenuItem("Piece"))      { pushUndo(); spawnCoin({550.0f, 300.0f}); }
                if (ImGui::MenuItem("Objectif"))   { pushUndo(); spawnGoal({600.0f, 250.0f}); }
                if (ImGui::MenuItem("Piege"))      { pushUndo(); spawnHazard({560.0f, 350.0f}); }
                ImGui::Separator();
                if (ImGui::MenuItem("Entite vide")) { pushUndo(); newEntity(); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edition")) {
                if (ImGui::MenuItem("Annuler", "Ctrl+Z", false, !m_undo.empty())) undoAction();
                if (ImGui::MenuItem("Retablir", "Ctrl+Y", false, !m_redo.empty())) redoAction();
                ImGui::Separator();
                const bool sel = m_reg.valid(m_selected);
                if (ImGui::MenuItem("Dupliquer", "Ctrl+D", false, sel)) duplicateSelected();
                if (ImGui::MenuItem("Copier", "Ctrl+C", false, sel)) copySelected();
                if (ImGui::MenuItem("Coller", "Ctrl+V", false, m_hasClipboard)) paste();
                if (ImGui::MenuItem("Supprimer", "Suppr", false, sel)) deleteSelected();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Vue")) {
                ImGui::Checkbox("Grille magnetique", &m_snap);
                ImGui::SetNextItemWidth(120);
                ImGui::DragFloat("pas de grille", &m_grid, 1.0f, 4.0f, 256.0f, "%.0f");
                ImGui::Separator();
                ImGui::Checkbox("Mode peinture de tuiles", &m_paintMode);
                if (m_paintMode) {
                    ImGui::Checkbox("Gomme (effacer)", &m_eraseMode);
                    ImGui::TextDisabled("Pinceau = clique une image dans Assets");
                    ImGui::TextDisabled("Clic gauche dans la scene = poser/effacer");
                }
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
                char nbuf[64];
                std::snprintf(nbuf, sizeof(nbuf), "%s", m_levelName.c_str());
                ImGui::SetNextItemWidth(200);
                if (ImGui::InputText("Nom du niveau", nbuf, sizeof(nbuf))) m_levelName = nbuf;
                ImGui::Checkbox("Ecran titre au lancement", &m_titleScreen);
                ImGui::SetNextItemWidth(160);
                ImGui::DragFloat("Temps limite (s)", &m_timeLimit, 1.0f, 0.0f, 999.0f, "%.0f");
                ImGui::TextDisabled("0 = pas de limite");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Audio")) {
                ImGui::SetNextItemWidth(160);
                if (ImGui::SliderFloat("Musique", &m_musicVol, 0.0f, 1.0f)) applyVolumes();
                ImGui::SetNextItemWidth(160);
                if (ImGui::SliderFloat("Effets", &m_sfxVol, 0.0f, 1.0f)) applyVolumes();
                if (ImGui::Checkbox("Muet", &m_muted)) applyVolumes();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Options")) {
                if (ImGui::MenuItem("Touches, langue, son...")) m_showOptions = true;
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, m_playing ? ImVec4(0.80f, 0.28f, 0.28f, 1.0f) : ImVec4(0.22f, 0.62f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_playing ? ImVec4(0.90f, 0.38f, 0.38f, 1.0f) : ImVec4(0.30f, 0.72f, 0.40f, 1.0f));
            if (ImGui::Button(m_playing ? "  Stop  " : "  Play  ")) { m_playing ? stop() : play(); }
            ImGui::PopStyleColor(2);
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
                pushUndo(); spawnSprite(path, screenToWorld(Input::mousePosition()));
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
        if (m_reg.has<Hazard>(e))       s += "!";
        if (m_reg.has<OneWay>(e))       s += "~";
        if (m_reg.has<Health>(e))       s += "H";
        if (m_reg.has<RigidBody>(e))    s += "P";
        if (m_reg.has<AnimSprite>(e))   s += "A";
        if (m_reg.has<SpriteAsset>(e))  s += "S";
        if (m_reg.has<RectShape>(e))    s += "R";
        return s + "]";
    }

    static std::string lower(std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
    void drawHierarchy() {
        ImGui::Begin("Hierarchie");
        const std::vector<Entity> ents = m_reg.entities();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##recherche", "Rechercher...", m_hierFilter, sizeof(m_hierFilter));
        const std::string filter = lower(m_hierFilter);
        ImGui::TextDisabled("%d entites", static_cast<int>(ents.size()));
        ImGui::Separator();
        for (Entity e : ents) {
            const std::string name = entityLabel(e);
            if (!filter.empty() && lower(name).find(filter) == std::string::npos) continue;
            const std::string label = name + componentSummary(e) + "##" + std::to_string(e);
            ImGui::PushStyleColor(ImGuiCol_Text, entityColor(e));
            if (ImGui::Selectable(label.c_str(), e == m_selected)) m_selected = e;
            ImGui::PopStyleColor();
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
        // Nom de l'entité.
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s", m_reg.has<Name>(m_selected) ? m_reg.get<Name>(m_selected).value.c_str() : "");
        ImGui::SetNextItemWidth(-72.0f);
        if (ImGui::InputText("##nom", buf, sizeof(buf))) {
            if (!m_reg.has<Name>(m_selected)) m_reg.add<Name>(m_selected, Name{});
            m_reg.get<Name>(m_selected).value = buf;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Suppr.")) { deleteSelected(); ImGui::End(); return; }
        // Verrou / masque.
        bool locked = isLocked(m_selected), hidden = isHidden(m_selected);
        if (ImGui::Checkbox("Verrouille", &locked)) ensureFlags(m_selected).locked = locked;
        ImGui::SameLine();
        if (ImGui::Checkbox("Masque", &hidden)) ensureFlags(m_selected).hidden = hidden;
        ImGui::TextDisabled("Entite #%u", m_selected);
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
        if (m_reg.has<AnimSprite>(m_selected) && ImGui::CollapsingHeader("Sprite anime", ImGuiTreeNodeFlags_DefaultOpen)) {
            AnimSprite& a = m_reg.get<AnimSprite>(m_selected);
            ImGui::TextWrapped("%s", a.path.c_str());
            ImGui::DragInt2("taille frame", &a.frameW, 1.0f, 1, 1024);
            ImGui::DragInt2("idle (ligne, nb)", &a.idleRow, 0.1f, 0, 64);
            ImGui::DragInt2("walk (ligne, nb)", &a.walkRow, 0.1f, 0, 64);
            ImGui::DragFloat("duree/frame (s)", &a.frameTime, 0.005f, 0.02f, 1.0f);
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
            ImGui::DragInt("sauts max (2 = double)", &cc.maxJumps, 0.1f, 1, 5);
        }
        if (m_reg.has<Patrol>(m_selected) && ImGui::CollapsingHeader("Ennemi (patrouille)", ImGuiTreeNodeFlags_DefaultOpen)) {
            Patrol& p = m_reg.get<Patrol>(m_selected);
            ImGui::DragFloat("vitesse", &p.speed, 1.0f, 0.0f, 600.0f);
            ImGui::DragFloat("portee", &p.range, 1.0f, 0.0f, 1200.0f);
        }
        if (m_reg.has<Chase>(m_selected) && ImGui::CollapsingHeader("Chasseur (poursuite)", ImGuiTreeNodeFlags_DefaultOpen)) {
            Chase& ch = m_reg.get<Chase>(m_selected);
            ImGui::DragFloat("vitesse", &ch.speed, 1.0f, 0.0f, 600.0f);
            ImGui::DragFloat("portee", &ch.range, 2.0f, 0.0f, 2000.0f);
            ImGui::Checkbox("peut sauter", &ch.jump);
        }
        if (m_reg.has<Shooter>(m_selected) && ImGui::CollapsingHeader("Tireur", ImGuiTreeNodeFlags_DefaultOpen)) {
            Shooter& s = m_reg.get<Shooter>(m_selected);
            ImGui::DragFloat("cadence (s)", &s.interval, 0.05f, 0.2f, 10.0f);
            ImGui::DragFloat("vitesse tir", &s.bulletSpeed, 2.0f, 50.0f, 1000.0f);
            ImGui::DragFloat("portee", &s.range, 2.0f, 0.0f, 2000.0f);
        }
        if (m_reg.has<Spawner>(m_selected) && ImGui::CollapsingHeader("Generateur d'ennemis", ImGuiTreeNodeFlags_DefaultOpen)) {
            Spawner& sp = m_reg.get<Spawner>(m_selected);
            ImGui::DragFloat("intervalle (s)", &sp.interval, 0.1f, 0.3f, 30.0f);
            ImGui::DragInt("max simultanes", &sp.maxAlive, 0.1f, 1, 50);
            ImGui::Combo("type", &sp.kind, "Patrouilleur\0Chasseur\0");
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

        if (m_reg.has<AABB>(m_selected)) {
            bool ow = m_reg.has<OneWay>(m_selected);
            if (ImGui::Checkbox("Traversable par le bas (one-way)", &ow)) {
                if (ow) m_reg.add<OneWay>(m_selected); else m_reg.remove<OneWay>(m_selected);
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("Ajouter un comportement :");
        if (!m_reg.has<RigidBody>(m_selected)    && ImGui::Button("+ Physique"))  m_reg.add<RigidBody>(m_selected);
        ImGui::SameLine();
        if (!m_reg.has<Controllable>(m_selected) && ImGui::Button("+ Joueur"))    { m_reg.add<Controllable>(m_selected); if (!m_reg.has<RigidBody>(m_selected)) m_reg.add<RigidBody>(m_selected); }
        ImGui::SameLine();
        if (!m_reg.has<Patrol>(m_selected)       && ImGui::Button("+ Ennemi"))    { m_reg.add<Patrol>(m_selected); if (!m_reg.has<RigidBody>(m_selected)) m_reg.add<RigidBody>(m_selected); }
        ImGui::SameLine();
        if (!m_reg.has<Chase>(m_selected)        && ImGui::Button("+ Chasseur"))  { m_reg.add<Chase>(m_selected); if (!m_reg.has<RigidBody>(m_selected)) m_reg.add<RigidBody>(m_selected); }
        ImGui::SameLine();
        if (!m_reg.has<Shooter>(m_selected)      && ImGui::Button("+ Tireur"))    { m_reg.add<Shooter>(m_selected); }
        if (!m_reg.has<Collectible>(m_selected)  && ImGui::Button("+ Piece"))     m_reg.add<Collectible>(m_selected);
        ImGui::SameLine();
        if (!m_reg.has<Goal>(m_selected)         && ImGui::Button("+ Objectif"))  m_reg.add<Goal>(m_selected);
        ImGui::SameLine();
        if (!m_reg.has<Health>(m_selected)       && ImGui::Button("+ Vies"))      m_reg.add<Health>(m_selected);
        ImGui::SameLine();
        if (!m_reg.has<Hazard>(m_selected)       && ImGui::Button("+ Piege"))     m_reg.add<Hazard>(m_selected);
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

    std::string keyName(int k) {
        if (k >= KEY_A && k <= KEY_Z) return std::string(1, static_cast<char>('A' + (k - KEY_A)));
        if (k >= KEY_ZERO && k <= KEY_NINE) return std::string(1, static_cast<char>('0' + (k - KEY_ZERO)));
        switch (k) {
            case KEY_LEFT: return t("Gauche", "Left");   case KEY_RIGHT: return t("Droite", "Right");
            case KEY_UP: return t("Haut", "Up");         case KEY_DOWN: return t("Bas", "Down");
            case KEY_SPACE: return t("Espace", "Space"); case KEY_ENTER: return t("Entree", "Enter");
            case KEY_LEFT_SHIFT: case KEY_RIGHT_SHIFT: return "Shift";
            case KEY_LEFT_CONTROL: case KEY_RIGHT_CONTROL: return "Ctrl";
            default: break;
        }
        return "#" + std::to_string(k);
    }
    void keyRow(const char* label, int& key, int slot) {
        ImGui::Text("%s", label);
        ImGui::SameLine(170.0f);
        ImGui::PushID(slot);
        if (m_rebinding == slot) {
            ImGui::Button(t("... appuie sur une touche", "... press a key"), ImVec2(170, 0));
            const int k = GetKeyPressed();
            if (k == KEY_ESCAPE) m_rebinding = 0;
            else if (k != 0) { key = k; m_rebinding = 0; }
        } else {
            if (ImGui::Button(keyName(key).c_str(), ImVec2(170, 0))) m_rebinding = slot;
        }
        ImGui::PopID();
    }
    void drawOptions() {
        if (!m_showOptions) return;
        ImGui::SetNextWindowSize(ImVec2(460, 430), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(t("Options", "Options"), &m_showOptions)) {
            if (ImGui::CollapsingHeader(t("Son", "Audio"), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SetNextItemWidth(230);
                if (ImGui::SliderFloat(t("Musique", "Music"), &m_musicVol, 0.0f, 1.0f)) applyVolumes();
                ImGui::SetNextItemWidth(230);
                if (ImGui::SliderFloat(t("Effets", "SFX"), &m_sfxVol, 0.0f, 1.0f)) applyVolumes();
                if (ImGui::Checkbox(t("Couper le son (muet)", "Mute all"), &m_muted)) applyVolumes();
            }
            if (ImGui::CollapsingHeader(t("Touches", "Controls"), ImGuiTreeNodeFlags_DefaultOpen)) {
                keyRow(t("Aller a gauche", "Move left"), m_keyLeft, 1);
                keyRow(t("Aller a droite", "Move right"), m_keyRight, 2);
                keyRow(t("Sauter", "Jump"), m_keyJump, 3);
                ImGui::TextDisabled("%s", t("ZQSD / WASD / fleches restent actives", "ZQSD / WASD / arrows always work too"));
            }
            if (ImGui::CollapsingHeader(t("Langue", "Language"), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SetNextItemWidth(200);
                ImGui::Combo(t("Langue", "Language"), &m_lang, "Francais\0English\0");
            }
        }
        ImGui::End();
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
                if (rlImGuiImageButtonSize(name.c_str(), &t, ::Vector2{64, 64})) {
                    if (m_paintMode) { m_paintTile = path; setStatus("Pinceau : " + name); }
                    else { pushUndo(); spawnSprite(path, {kWorldW * 0.5f, kWorldH * 0.5f}); }
                }
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
        m_paused = false; m_menuIndex = 0; m_particles.clear(); m_shake = 0.0f;
        m_titleActive = m_titleScreen;
        m_music.stop(); m_musicStarted = false; m_musicPlaying = false;
        m_timeLeft = m_timeLimit;
        Vec2 sp; if (firstPlayer(sp)) m_playerSpawn = sp;
        m_score = keep;
    }

    // Sérialise UNE entité (réutilisé par save, presse-papier, duplication, undo).
    json entityToJson(Entity e) {
        json je;
        if (m_reg.has<Name>(e))        je["Name"] = m_reg.get<Name>(e).value;
        if (m_reg.has<EditorFlags>(e)) { EditorFlags& f = m_reg.get<EditorFlags>(e); je["EditorFlags"] = {{"locked", f.locked}, {"hidden", f.hidden}}; }
        if (m_reg.has<Transform2D>(e)) { Transform2D& t = m_reg.get<Transform2D>(e); je["Transform2D"] = {{"position", vecToJ(t.position)}, {"rotation", t.rotation}, {"scale", vecToJ(t.scale)}}; }
        if (m_reg.has<RectShape>(e))   { RectShape& r = m_reg.get<RectShape>(e); je["RectShape"] = {{"size", vecToJ(r.size)}, {"color", json::array({r.color.r, r.color.g, r.color.b})}}; }
        if (m_reg.has<AABB>(e))        je["AABB"] = {{"halfSize", vecToJ(m_reg.get<AABB>(e).halfSize)}};
        if (m_reg.has<RigidBody>(e))   je["RigidBody"] = {{"gravityScale", m_reg.get<RigidBody>(e).gravityScale}};
        if (m_reg.has<Controllable>(e)){ Controllable& c = m_reg.get<Controllable>(e); je["Controllable"] = {{"speed", c.speed}, {"jumpForce", c.jumpForce}, {"maxJumps", c.maxJumps}}; }
        if (m_reg.has<Patrol>(e))      { Patrol& p = m_reg.get<Patrol>(e); je["Patrol"] = {{"speed", p.speed}, {"range", p.range}}; }
        if (m_reg.has<Chase>(e))       { Chase& ch = m_reg.get<Chase>(e); je["Chase"] = {{"speed", ch.speed}, {"range", ch.range}, {"jump", ch.jump}}; }
        if (m_reg.has<Shooter>(e))     { Shooter& s = m_reg.get<Shooter>(e); je["Shooter"] = {{"interval", s.interval}, {"bulletSpeed", s.bulletSpeed}, {"range", s.range}}; }
        if (m_reg.has<Spawner>(e))     { Spawner& sp = m_reg.get<Spawner>(e); je["Spawner"] = {{"interval", sp.interval}, {"maxAlive", sp.maxAlive}, {"kind", sp.kind}}; }
        if (m_reg.has<Collectible>(e)) je["Collectible"] = {{"points", m_reg.get<Collectible>(e).points}};
        if (m_reg.has<Goal>(e))        je["Goal"] = true;
        if (m_reg.has<Hazard>(e))      je["Hazard"] = true;
        if (m_reg.has<OneWay>(e))      je["OneWay"] = true;
        if (m_reg.has<Tile>(e))        je["Tile"] = true;
        if (m_reg.has<Health>(e))      je["Health"] = {{"lives", m_reg.get<Health>(e).lives}};
        if (m_reg.has<SpriteAsset>(e)) { SpriteAsset& sp = m_reg.get<SpriteAsset>(e); je["SpriteAsset"] = {{"path", sp.path}, {"size", vecToJ(sp.size)}}; }
        if (m_reg.has<AnimSprite>(e)) {
            AnimSprite& a = m_reg.get<AnimSprite>(e);
            je["AnimSprite"] = {{"path", a.path}, {"fw", a.frameW}, {"fh", a.frameH},
                                {"ir", a.idleRow}, {"if", a.idleFrames}, {"wr", a.walkRow},
                                {"wf", a.walkFrames}, {"ft", a.frameTime}};
        }
        return je;
    }

    Entity createEntityFromJson(const json& je) {
        Entity e = m_reg.create();
        if (je.contains("Name"))        m_reg.add<Name>(e, Name{je["Name"].get<std::string>()});
        if (je.contains("EditorFlags")) { const json& j = je["EditorFlags"]; EditorFlags f; f.locked = j["locked"].get<bool>(); f.hidden = j["hidden"].get<bool>(); m_reg.add<EditorFlags>(e, f); }
        if (je.contains("Transform2D")) { const json& j = je["Transform2D"]; m_reg.add<Transform2D>(e, Transform2D{jToVec(j["position"]), j["rotation"].get<float>(), jToVec(j["scale"])}); }
        if (je.contains("RectShape"))   { const json& j = je["RectShape"]; const json& c = j["color"]; m_reg.add<RectShape>(e, RectShape{jToVec(j["size"]), mjv::Color{c[0].get<std::uint8_t>(), c[1].get<std::uint8_t>(), c[2].get<std::uint8_t>(), 255}}); }
        if (je.contains("AABB"))        m_reg.add<AABB>(e, AABB{jToVec(je["AABB"]["halfSize"])});
        if (je.contains("RigidBody"))   { RigidBody rb; rb.gravityScale = je["RigidBody"]["gravityScale"].get<float>(); m_reg.add<RigidBody>(e, rb); }
        if (je.contains("Controllable")){ const json& j = je["Controllable"]; Controllable c{j["speed"].get<float>(), j["jumpForce"].get<float>()}; c.maxJumps = j.value("maxJumps", 1); m_reg.add<Controllable>(e, c); }
        if (je.contains("Patrol"))      { const json& j = je["Patrol"]; Patrol p; p.speed = j["speed"].get<float>(); p.range = j["range"].get<float>(); m_reg.add<Patrol>(e, p); }
        if (je.contains("Chase"))       { const json& j = je["Chase"]; Chase ch; ch.speed = j["speed"].get<float>(); ch.range = j["range"].get<float>(); ch.jump = j.value("jump", true); m_reg.add<Chase>(e, ch); }
        if (je.contains("Shooter"))     { const json& j = je["Shooter"]; Shooter s; s.interval = j["interval"].get<float>(); s.bulletSpeed = j["bulletSpeed"].get<float>(); s.range = j["range"].get<float>(); m_reg.add<Shooter>(e, s); }
        if (je.contains("Spawner"))     { const json& j = je["Spawner"]; Spawner sp; sp.interval = j["interval"].get<float>(); sp.maxAlive = j["maxAlive"].get<int>(); sp.kind = j["kind"].get<int>(); m_reg.add<Spawner>(e, sp); }
        if (je.contains("Collectible")) m_reg.add<Collectible>(e, Collectible{je["Collectible"]["points"].get<int>()});
        if (je.contains("Goal"))        m_reg.add<Goal>(e, Goal{});
        if (je.contains("Hazard"))      m_reg.add<Hazard>(e, Hazard{});
        if (je.contains("OneWay"))      m_reg.add<OneWay>(e, OneWay{});
        if (je.contains("Tile"))        m_reg.add<Tile>(e, Tile{});
        if (je.contains("Health"))      m_reg.add<Health>(e, Health{je["Health"]["lives"].get<int>()});
        if (je.contains("SpriteAsset")) { const json& j = je["SpriteAsset"]; m_reg.add<SpriteAsset>(e, SpriteAsset{j["path"].get<std::string>(), jToVec(j["size"])}); }
        if (je.contains("AnimSprite")) {
            const json& j = je["AnimSprite"];
            AnimSprite a; a.path = j["path"].get<std::string>();
            a.frameW = j["fw"].get<int>(); a.frameH = j["fh"].get<int>();
            a.idleRow = j["ir"].get<int>(); a.idleFrames = j["if"].get<int>();
            a.walkRow = j["wr"].get<int>(); a.walkFrames = j["wf"].get<int>();
            a.frameTime = j["ft"].get<float>();
            m_reg.add<AnimSprite>(e, a);
        }
        return e;
    }

    json sceneToJson() {
        json doc; doc["entities"] = json::array();
        doc["timeLimit"] = m_timeLimit;
        doc["name"] = m_levelName;
        doc["titleScreen"] = m_titleScreen;
        for (Entity e : m_reg.entities()) doc["entities"].push_back(entityToJson(e));
        return doc;
    }

    void jsonToScene(const json& doc) {
        m_reg.clear(); m_selected = NullEntity;
        m_timeLimit = doc.value("timeLimit", 0.0f);
        m_levelName = doc.value("name", std::string("Mon niveau"));
        m_titleScreen = doc.value("titleScreen", true);
        if (!doc.contains("entities")) return;
        for (const json& je : doc["entities"]) createEntityFromJson(je);
    }

    // --- Annuler / Rétablir, presse-papier ---
    void pushUndo() {
        m_undo.push_back(sceneToJson());
        if (m_undo.size() > 60) m_undo.erase(m_undo.begin());
        m_redo.clear();
    }
    void undoAction() {
        if (m_undo.empty()) { setStatus("Rien a annuler"); return; }
        m_redo.push_back(sceneToJson());
        const json s = m_undo.back(); m_undo.pop_back();
        jsonToScene(s); setStatus("Annule");
    }
    void redoAction() {
        if (m_redo.empty()) { setStatus("Rien a retablir"); return; }
        m_undo.push_back(sceneToJson());
        const json s = m_redo.back(); m_redo.pop_back();
        jsonToScene(s); setStatus("Retabli");
    }
    void duplicateSelected() {
        if (!m_reg.valid(m_selected)) return;
        pushUndo();
        Entity e = createEntityFromJson(entityToJson(m_selected));
        if (m_reg.has<Transform2D>(e)) m_reg.get<Transform2D>(e).position += {24.0f, 24.0f};
        m_selected = e; setStatus("Duplique");
    }
    void copySelected() {
        if (!m_reg.valid(m_selected)) return;
        m_clipboard = entityToJson(m_selected); m_hasClipboard = true; setStatus("Copie");
    }
    void paste() {
        if (!m_hasClipboard) return;
        pushUndo();
        Entity e = createEntityFromJson(m_clipboard);
        if (m_reg.has<Transform2D>(e)) m_reg.get<Transform2D>(e).position += {24.0f, 24.0f};
        m_selected = e; setStatus("Colle");
    }
    void deleteSelected() {
        if (!m_reg.valid(m_selected)) return;
        pushUndo(); m_reg.destroy(m_selected); m_selected = NullEntity; setStatus("Supprime");
    }
    bool isHidden(Entity e) { return m_reg.has<EditorFlags>(e) && m_reg.get<EditorFlags>(e).hidden; }
    bool isLocked(Entity e) { return m_reg.has<EditorFlags>(e) && m_reg.get<EditorFlags>(e).locked; }
    EditorFlags& ensureFlags(Entity e) { if (!m_reg.has<EditorFlags>(e)) m_reg.add<EditorFlags>(e, EditorFlags{}); return m_reg.get<EditorFlags>(e); }
    std::string entityLabel(Entity e) {
        std::string base = m_reg.has<Name>(e) && !m_reg.get<Name>(e).value.empty()
                               ? m_reg.get<Name>(e).value : ("Entite " + std::to_string(e));
        if (isLocked(e)) base = "[V] " + base;
        if (isHidden(e)) base = "[M] " + base;
        return base;
    }

    void setStatus(const std::string& s) { m_status = s; m_statusTimer = 3.0f; }
};

int main() {
    Editor app;
    app.run();
    return 0;
}
