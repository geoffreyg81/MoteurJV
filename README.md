# MoteurJV

Un moteur de jeu 2D open source, léger et moderne — en C++.

> État : **v0.7**. Socle 2D complet : boucle de jeu, **ECS**, rendu, input,
> **collisions AABB**, **sprites animés**, **audio**, **mini-physique** (gravité +
> saut). Démo jouable : un **platformer** — personnage animé, plateformes, saut.

## Philosophie

Commencer **petit et focalisé**. On ne réécrit pas Unity en un week-end. La stack
graphique bas niveau (raylib) est **cachée derrière une API propre `mjv::`** : ton
code de jeu ne touche jamais directement à OpenGL/raylib, exactement comme tu
utiliserais Unity sans écrire de DirectX. Le jour où on remplace le backend (Vulkan,
SDL…), le code de jeu ne bouge pas.

## Est-ce vraiment « un moteur » ?

Oui — au sens où **tu déclares ce que tes objets *sont*, et le moteur sait quoi en
faire**. Tu n'écris pas le « comment » à chaque fois.

```cpp
Entity hero = reg.create();
reg.add<Transform2D>(hero, ...);   // il a une position
reg.add<Velocity>(hero, ...);      // il peut se déplacer
reg.add<Sprite>(hero, ...);        // il s'affiche avec cette image
reg.add<Animator>(hero, ...);      // il a des animations
reg.add<AABB>(hero, ...);          // il a une boîte de collision
reg.add<PlayerControl>(hero, ...); // il est piloté au clavier
```

À partir de là, les **systèmes** génériques réagissent tout seuls : `Velocity` +
`Transform` → il bouge ; `Animator` → ses frames défilent ; `AABB` → il ne
traverse plus les murs. **Rien à recoder.** Preuve : les caisses de la démo ne sont
que `Transform + RectShape + AABB` — elles deviennent des obstacles solides sans
une ligne de logique en plus.

**Séparation moteur / jeu :**

| Le **moteur** (`engine/`, réutilisable) | Le **jeu** (`examples/`, spécifique) |
|---|---|
| *Comment* afficher, déplacer, animer, collisionner | *Quoi* : ce perso, ces caisses, à ces positions |
| Marche pour n'importe quel jeu | Change à chaque projet |

**Ce qu'on a, et ce qui manque** (honnêtement) :

- ✅ Le **cœur** d'un moteur (la machinerie ECS) — la partie techniquement la plus dure
- ✅ Rendu, input, collisions AABB, sprites animés
- ❌ Pas encore d'**éditeur visuel** : on déclare en C++, pas à la souris (≠ Unity)
- ❌ Pas encore de **scripting** (Lua) pour piloter le jeu sans recompiler

C'est un **vrai moteur, mais jeune** — au stade « moteur-code », comme l'étaient
Unity et Godot à leurs débuts. L'éditeur et le scripting sont les grosses briques
qui le rapprocheront d'un moteur grand public (voir feuille de route).

## Architecture actuelle

```
MoteurJV/
├── engine/                  # La bibliothèque du moteur (cible CMake "mjv")
│   ├── include/mjv/         # API publique
│   │   ├── Application.hpp  #   boucle de jeu, fenêtre, delta time
│   │   ├── Graphics.hpp     #   façade de rendu 2D
│   │   ├── Input.hpp        #   clavier
│   │   ├── Texture.hpp      #   chargement d'images + dessin de régions (spritesheet)
│   │   ├── Components.hpp   #   composants génériques (Velocity, RectShape, Sprite, AABB)
│   │   ├── Animation.hpp    #   AnimationClip + Animator (spritesheet)
│   │   ├── Audio.hpp        #   mjv::Audio, Sound, Music
│   │   ├── Collision.hpp    #   AABB : overlap + résolution (MTV)
│   │   ├── Physics.hpp      #   mini-physique : gravité + saut + AABB (physicsStep)
│   │   ├── ecs/Registry.hpp #   ECS : Entity, Component, view<...> (systèmes)
│   │   ├── Math.hpp         #   Vec2, Transform2D, Rect
│   │   └── Color.hpp
│   └── src/                 # implémentations (raylib vit ici, et nulle part ailleurs)
├── examples/
│   └── 01_perso2d/          # la démo : perso animé, collisions, caisses
│       └── assets/          #   hero_sheet.png (généré), hero.png
├── tools/                   # make_sprite.py (génère les assets), scripts WSL
├── build.sh                 # build + run sous Linux/WSL
├── build.ps1                # build + run sous Windows natif
└── CMakeLists.txt
```

## Compiler et lancer

raylib est téléchargé et compilé automatiquement par CMake dans les deux cas.

### Linux / WSL2  (méthode utilisée ici ✅)

Sur cette machine, **Smart App Control** (Windows 11) bloque l'exécution de tout
`.exe` compilé localement et non signé. On développe donc sous **WSL2 (Ubuntu)** :
les binaires Linux ne sont pas concernés, et la fenêtre s'affiche via WSLg.

```bash
# dans Ubuntu (WSL) :
./build.sh run
```

Dépendances Ubuntu (déjà installées) : `cmake`, `build-essential`,
`libgl1-mesa-dev`, `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`,
`libxcursor-dev`, `libxi-dev`, `libwayland-dev`, `libxkbcommon-dev`.

**Audio sous WSL** : il faut `libpulse0` (chargé à l'exécution par raylib) et
pointer le serveur PulseAudio de WSLg. `build.sh run` le fait automatiquement
(`PULSE_SERVER=unix:/mnt/wslg/PulseServer`). Sans ça, le moteur tourne mais le
backend audio reste `Null` (aucun son). En natif Windows, rien à configurer.

### Windows natif  (si Smart App Control est désactivé)

Prérequis : **Visual Studio Build Tools 2022** (workload C++, ARM64) + CMake.

```powershell
.\build.ps1 -Run
```

**Commandes :** **Gauche/Droite** (Flèches / ZQSD / WASD) · **Espace** (ou
Haut/Z/W) pour **sauter** · **Tab** affiche les boîtes de collision · **P**
coupe/relance la musique.

## Écrire un jeu avec le moteur (ECS)

Exemple minimal — une entité pilotée au clavier, en quelques lignes :

```cpp
#include "mjv/mjv.hpp"
using namespace mjv;

struct PlayerControl { float speed = 200; };

class MonJeu : public Application {
    Registry reg;
    void onStart() override {
        Entity e = reg.create();                       // une entité = un id
        reg.add<Transform2D>(e, Transform2D{{100, 100}});
        reg.add<Velocity>(e, Velocity{});
        reg.add<PlayerControl>(e);
    }
    void onUpdate(float dt) override {
        // un "system" : parcourt les entités ayant ces composants
        reg.view<Velocity, PlayerControl>([](Entity, Velocity& v, PlayerControl& p) {
            v.value.x = (Input::isDown(Key::Right) - Input::isDown(Key::Left)) * p.speed;
        });
        reg.view<Transform2D, Velocity>([&](Entity, Transform2D& t, Velocity& v) {
            t.position += v.value * dt;
        });
    }
    void onRender() override {
        reg.view<Transform2D>([](Entity, Transform2D& t) {
            Graphics::drawCircle(t.position, 20, Colors::Red);
        });
    }
};

int main() { MonJeu().run(); }
```

Pour un exemple complet (sprite animé, collisions, obstacles, debug), voir
[`examples/01_perso2d/main.cpp`](examples/01_perso2d/main.cpp).

### Briques disponibles (composants & systèmes)

| Composant | Rôle | Système qui l'exploite |
|---|---|---|
| `Transform2D` | position / rotation / échelle | tous |
| `Velocity` | vitesse linéaire | mouvement |
| `Sprite` | texture à afficher | rendu de sprites |
| `Animator` | clips d'animation (spritesheet) | animation |
| `AABB` | boîte de collision | collision / physique |
| `RigidBody` | gravité + vitesse + `onGround` | `physicsStep` |
| `RectShape` | rectangle coloré (sol, plateformes, debug) | rendu |

Une animation se déclare ainsi :

```cpp
Animator anim;
anim.frameWidth = 48; anim.frameHeight = 64;
anim.add("idle", {/*row*/0, /*frames*/2, /*delay*/0.40f, /*loop*/true});
anim.add("walk", {/*row*/1, /*frames*/4, /*delay*/0.12f, /*loop*/true});
anim.play("idle");
// ... puis selon l'état : anim.play(walking ? "walk" : "idle");
```

## Feuille de route

- [x] **Phase 1 — Core** : boucle de jeu, fenêtre, rendu 2D, input, chargement de textures
- [x] **Architecture ECS** : Entity / Component / System (`Registry`)
- [x] Système de collision **AABB** (`Collision.hpp` : overlap + résolution MTV)
- [x] Chargement de **sprites PNG** (composant `Sprite` + `spriteRenderSystem`)
- [x] **Animation de sprites** (`Animator` + spritesheet : clips `idle`/`walk`)
- [x] **Audio** (`mjv::Audio`, `Sound`, `Music`) — Phase 2, étape 1
- [x] **Mini-physique maison** (`RigidBody` + `physicsStep` : gravité, saut, AABB)
- [ ] Physique avancée (Box2D : joints, forces, rebonds) — optionnel
- [ ] **Scripting** (Lua via sol2) — Phase 2, étape 3
- [ ] **Phase 3** : éditeur visuel (Dear ImGui)
- [ ] **Phase 4** : écosystème (docs, assets, communauté)

## Assets

Les sprites de test sont générés par `tools/make_sprite.py` (pur Python stdlib,
aucune dépendance) dans `examples/01_perso2d/assets/` :

- `hero_sheet.png` — **spritesheet animée** (ligne 0 = idle 2 frames, ligne 1 =
  walk 4 frames, frames de 48×64) : c'est celle utilisée par la démo
- `hero.png` — une seule frame (repli statique)

Les sons de test (`step.wav`, `bump.wav`, `music.wav`) sont générés par
`tools/make_audio.py` (module `wave` de la stdlib). Pour tout régénérer :

```bash
python3 tools/make_sprite.py
python3 tools/make_audio.py
```

Remplace-les par de vrais assets (garde les mêmes noms, ou change le chemin dans
`main.cpp`). Sources d'assets 2D **gratuits et libres de droits** :

| Source | Licence | Idéal pour |
|---|---|---|
| [kenney.nl/assets](https://kenney.nl/assets) | CC0 (domaine public) | personnages, tilesets, UI — le must pour débuter |
| [opengameart.org](https://opengameart.org) | CC0 / CC-BY / GPL (vérifier) | très large catalogue |
| [itch.io/game-assets/free](https://itch.io/game-assets/free) | variable (lire la page) | packs de qualité, souvent gratuits |
| [craftpix.net/freebies](https://craftpix.net/freebies/) | licence Craftpix | sprites animés, fonds |

Pour un personnage **animé**, cherche une « character spritesheet » : une image
avec plusieurs frames alignées. Adapte alors `frameWidth`/`frameHeight` et le
nombre de frames par ligne dans la déclaration de l'`Animator`.

## Licence

**MIT** — voir [LICENSE](LICENSE). Licence permissive : usage libre, y compris
commercial, sans piège façon Unity. raylib (la dépendance graphique) est sous
licence zlib, également permissive.
