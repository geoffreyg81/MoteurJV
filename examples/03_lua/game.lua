-- game.lua — toute la logique du jeu, en Lua.
-- Modifie ce fichier et relance : AUCUNE recompilation C++ nécessaire.
--
-- Le moteur MoteurJV appelle automatiquement :
--   start()      une fois au lancement
--   update(dt)   chaque frame (dt = secondes écoulées)
--   draw()       chaque frame (pour dessiner)
--
-- API exposée : Graphics.rect/circle/text, Input.down/pressed, SCREEN_W/H.

local player = { x = 0, y = 0, r = 22, speed = 280 }
local coins = {}
local score = 0

local function spawn_coins()
    coins = {}
    for i = 1, 6 do
        coins[i] = {
            x = math.random(60, SCREEN_W - 60),
            y = math.random(60, SCREEN_H - 60),
            r = 12,
            taken = false,
        }
    end
end

function start()
    player.x = SCREEN_W / 2
    player.y = SCREEN_H / 2
    math.randomseed(1234)
    spawn_coins()
end

function update(dt)
    -- Déplacement
    local dx, dy = 0, 0
    if Input.down("left")  then dx = dx - 1 end
    if Input.down("right") then dx = dx + 1 end
    if Input.down("up")    then dy = dy - 1 end
    if Input.down("down")  then dy = dy + 1 end
    local len = math.sqrt(dx * dx + dy * dy)
    if len > 0 then
        player.x = player.x + (dx / len) * player.speed * dt
        player.y = player.y + (dy / len) * player.speed * dt
    end

    -- Bornes écran
    player.x = math.max(player.r, math.min(SCREEN_W - player.r, player.x))
    player.y = math.max(player.r, math.min(SCREEN_H - player.r, player.y))

    -- Ramassage des pièces
    local remaining = 0
    for _, c in ipairs(coins) do
        if not c.taken then
            local ddx, ddy = c.x - player.x, c.y - player.y
            if math.sqrt(ddx * ddx + ddy * ddy) < player.r + c.r then
                c.taken = true
                score = score + 1
            else
                remaining = remaining + 1
            end
        end
    end
    if remaining == 0 then spawn_coins() end -- toutes ramassées : on recommence
end

function draw()
    Graphics.text("Ce jeu est ecrit en Lua (modifie game.lua, relance, sans recompiler !)", 16, 16, 18, 255, 255, 255)
    Graphics.text("Fleches / ZQSD pour bouger - ramasse les pieces", 16, 40, 16, 200, 200, 210)
    Graphics.text("Score: " .. score, 16, SCREEN_H - 30, 22, 253, 203, 0)

    -- Pièces
    for _, c in ipairs(coins) do
        if not c.taken then
            Graphics.circle(c.x, c.y, c.r, 253, 203, 0)
            Graphics.circle(c.x, c.y, c.r * 0.5, 255, 235, 130)
        end
    end

    -- Joueur
    Graphics.circle(player.x, player.y + 4, player.r, 0, 0, 0)       -- ombre
    Graphics.circle(player.x, player.y, player.r, 80, 170, 240)
end
