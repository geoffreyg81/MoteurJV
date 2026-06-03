-- scene.lua — un mini-platformer défini ENTIÈREMENT en Lua.
--
-- Lua crée de vraies entités ECS et leur ajoute des composants ; le moteur C++
-- exécute la physique (mjv.physics) dessus. Modifiable sans recompiler.
--
-- API : reg:create(), reg:add(e, "Comp"), reg:get(e, "Comp"), reg:has(e, "Comp"),
--       reg:view2("A", "B", fn), mjv.physics(dt, gx, gy), Graphics.*, Input.*

local player
local GRAVITY = 1800

-- Crée un solide statique (Transform + RectShape + AABB, sans RigidBody).
local function make_solid(cx, cy, w, h, r, g, b)
    local e = reg:create()
    local t = reg:add(e, "Transform2D"); t.position = Vec2.new(cx, cy)
    local s = reg:add(e, "RectShape");   s.size = Vec2.new(w, h); s.color = Color.new(r, g, b)
    local a = reg:add(e, "AABB");        a.halfSize = Vec2.new(w / 2, h / 2)
    return e
end

-- Crée une caisse dynamique (avec RigidBody → tombe et s'empile).
local function make_box(cx, cy)
    local e = reg:create()
    local t = reg:add(e, "Transform2D"); t.position = Vec2.new(cx, cy)
    local s = reg:add(e, "RectShape");   s.size = Vec2.new(34, 34); s.color = Color.new(200, 150, 90)
    local a = reg:add(e, "AABB");        a.halfSize = Vec2.new(17, 17)
    reg:add(e, "RigidBody")
    return e
end

function start()
    -- Sol + plateformes.
    make_solid(SCREEN_W / 2, SCREEN_H - 24, SCREEN_W, 48, 0, 180, 90)
    make_solid(280, 400, 160, 24, 150, 110, 70)
    make_solid(620, 300, 160, 24, 150, 110, 70)

    -- Le joueur : une entité ECS avec corps physique.
    player = reg:create()
    local t = reg:add(player, "Transform2D"); t.position = Vec2.new(120, 120)
    local s = reg:add(player, "RectShape");   s.size = Vec2.new(36, 56); s.color = Color.new(230, 70, 70)
    local a = reg:add(player, "AABB");        a.halfSize = Vec2.new(18, 28)
    reg:add(player, "RigidBody")

    -- Quelques caisses qui tombent et s'empilent (physique C++).
    for i = 1, 5 do make_box(360 + i * 40, -i * 70) end
end

function update(dt)
    local rb = reg:get(player, "RigidBody")

    -- Déplacement horizontal.
    local vx = 0
    if Input.down("left")  then vx = vx - 240 end
    if Input.down("right") then vx = vx + 240 end
    rb.velocity.x = vx

    -- Saut si au sol.
    if Input.pressed("space") and rb.onGround then
        rb.velocity.y = -720
    end

    -- Le moteur C++ fait avancer la physique sur TOUTES les entités RigidBody.
    mjv.physics(dt, 0, GRAVITY)

    -- Réapparition si on tombe dans le vide.
    local t = reg:get(player, "Transform2D")
    if t.position.y > SCREEN_H + 120 then
        t.position = Vec2.new(120, 120)
        rb.velocity = Vec2.new(0, 0)
    end
end

function draw()
    -- Un seul "système" de rendu, écrit en Lua : dessine toutes les entités qui
    -- ont Transform + RectShape (sol, plateformes, joueur, caisses), avec contour.
    reg:view2("Transform2D", "RectShape", function(e, t, s)
        local x, y = t.position.x - s.size.x / 2, t.position.y - s.size.y / 2
        Graphics.rect(x, y, s.size.x, s.size.y, s.color.r, s.color.g, s.color.b)
        Graphics.outline(x, y, s.size.x, s.size.y, 30, 30, 40)
    end)

    -- Légende.
    Graphics.text("Scene ECS construite ENTIEREMENT en Lua, physique simulee en C++", 16, 14, 18, 20, 20, 30)
    Graphics.text("Carre rouge = joueur (Fleches/ZQSD + Espace)", 16, 38, 16, 40, 40, 50)
    Graphics.text("Caisses marron = 5 entites creees en Lua, tombees avec la gravite", 16, 58, 16, 40, 40, 50)
    Graphics.text("Barres = plateformes statiques", 16, 78, 16, 40, 40, 50)
end
