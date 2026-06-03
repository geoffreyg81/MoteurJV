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
│   ├── 01_perso2d/          # démo technique : perso animé, collisions, physique
│   │   └── assets/          #   hero_sheet.png (généré), sons (généré)
│   ├── 02_jeu/              # un VRAI petit jeu : platformer 3 niveaux + ennemis
│   ├── 03_lua/              # un jeu écrit en Lua (game.lua) — sans recompiler
│   ├── 04_lua_ecs/          # l'ECS + la physique pilotés depuis Lua (scene.lua)
│   └── 05_editor/           # un ÉDITEUR visuel (Dear ImGui) : hiérarchie + inspecteur
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
./build.sh run        # la démo technique (examples/01_perso2d)
./build.sh run jeu    # le petit jeu : platformer 3 niveaux (examples/02_jeu)
./build.sh run lua    # un jeu écrit en Lua (examples/03_lua)
./build.sh run lua-ecs # l'ECS + la physique pilotés en Lua (examples/04_lua_ecs)
./build.sh run editor  # l'éditeur visuel ImGui (examples/05_editor)
```

Dépendance supplémentaire pour l'exemple Lua : `liblua5.4-dev`
(`sudo apt install liblua5.4-dev`). sol2 est récupéré par CMake.

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

## Le jeu d'exemple (`examples/02_jeu`)

Un vrai petit **platformer** construit entièrement avec le moteur — la meilleure
preuve que MoteurJV sert à faire des jeux :

- **3 niveaux** enchaînés, avec sol et plateformes
- **Ennemis** qui patrouillent : saute sur la tête pour les éliminer (+200),
  touche-les sur le côté et tu perds une vie
- **Pièces** à ramasser (+100), **score** et **vies** (3)
- Écrans **victoire** / **game over** (Entrée pour rejouer)

Commandes : **Gauche/Droite** (Flèches ou ZQSD) · **Espace** pour sauter ·
**Tab** debug collisions · **P** musique.

## Éditeur visuel (`examples/05_editor`)

Un éditeur **Dear ImGui** (backend [rlImGui](https://github.com/raylib-extras/rlImGui),
le tout récupéré par CMake) par-dessus le rendu du moteur :

- **Hiérarchie** (gauche) : liste toutes les entités du `Registry`, cliquables
- **Inspecteur** (droite) : édite **en temps réel** les composants de l'entité
  sélectionnée (position, taille, **couleur**, vitesse, gravité…), ajoute/retire
  des composants
- **Viewport cliquable** : clique une entité dans la scène pour la sélectionner ;
  **glisse-la à la souris** pour la repositionner
- **Play / Pause** : lance la physique du moteur pendant que tu édites ; en Play,
  l'entité sélectionnée (avec `RigidBody`) se pilote au clavier
- **Sauver / Charger** : sérialise toute la scène en **JSON** (`scene.json`) —
  la brique qui transforme l'éditeur en outil de production
- L'entité sélectionnée est surlignée en jaune dans la scène

**Phase 3 complète** (hiérarchie + inspecteur + viewport souris + sérialisation).

## Scripting Lua (`examples/03_lua`)

Le moteur peut être piloté depuis **Lua** (binding via [sol2](https://github.com/ThePhD/sol2)) :
toute la logique d'un jeu peut vivre dans un fichier `.lua` chargé à l'exécution,
**modifiable et relançable sans recompiler le C++**. C'est le passage de
« moteur pour soi » à « moteur pour les autres ».

Le programme C++ hôte expose une API et appelle automatiquement `start()`,
`update(dt)` et `draw()` définis côté Lua :

```lua
-- game.lua
function update(dt)
    if Input.down("right") then player.x = player.x + 200 * dt end
end
function draw()
    Graphics.circle(player.x, player.y, 22, 80, 170, 240)
    Graphics.text("Score: " .. score, 16, 16, 22, 255, 255, 255)
end
```

### L'ECS piloté depuis Lua (`examples/04_lua_ecs`)

On va plus loin : **l'ECS lui-même est exposé à Lua**. Un jeu peut créer de
vraies entités et leur ajouter des composants, pendant que la **physique C++**
tourne dessus — c'est ce qui sépare le « scripting basique » du « moteur
vraiment scriptable ».

```lua
-- scene.lua : une scène ECS construite entièrement en Lua
local e = reg:create()
local t = reg:add(e, "Transform2D"); t.position = Vec2.new(120, 100)
reg:add(e, "RigidBody")            -- soumis à la gravité
local a = reg:add(e, "AABB");      a.halfSize = Vec2.new(18, 28)

function update(dt)
    local rb = reg:get(player, "RigidBody")
    if Input.down("right") then rb.velocity.x = 240 end
    mjv.physics(dt, 0, 1800)       -- le moteur C++ simule TOUTES les entités
end

function draw()                    -- un "système" de rendu écrit en Lua
    reg:view2("Transform2D", "RectShape", function(e, t, s)
        Graphics.rect(t.position.x - s.size.x/2, t.position.y - s.size.y/2,
                      s.size.x, s.size.y, s.color.r, s.color.g, s.color.b)
    end)
end
```

API ECS exposée : `reg:create/destroy/valid`, `reg:add/get/has/remove(e, "Comp")`,
`reg:view/view2(...)`, `mjv.physics(dt, gx, gy)`. Composants : `Transform2D`,
`Velocity`, `RectShape`, `AABB`, `RigidBody`. Le binding (sol2) vit côté
application — **le cœur `mjv` ne dépend pas de Lua**. À venir : `Sprite`/`Animator`
et l'audio côté Lua.

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
- [x] **Jeu d'exemple complet** : platformer 3 niveaux, ennemis, score (`examples/02_jeu`)
- [ ] Physique avancée (Box2D : joints, forces, rebonds) — optionnel
- [x] **Scripting Lua** (sol2) — jeu en `game.lua`, **ECS + physique pilotés en Lua** (`reg:create`, `reg:add`, `reg:view2`, `mjv.physics`)
- [x] **Phase 3** : éditeur Dear ImGui — hiérarchie, inspecteur live, viewport
  cliquable + glisser souris, sauvegarde/chargement JSON
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
