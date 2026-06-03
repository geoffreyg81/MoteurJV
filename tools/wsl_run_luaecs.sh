#!/usr/bin/env bash
# Compile et lance l'exemple ECS-en-Lua (examples/04_lua_ecs).
pkill -x lua_ecs 2>/dev/null || true
set -e
/mnt/c/Users/ggine/MoteurJV/build.sh
EXE="$HOME/.cache/mjv-build/examples/04_lua_ecs/lua_ecs"
[ -S /mnt/wslg/PulseServer ] && export PULSE_SERVER=unix:/mnt/wslg/PulseServer
nohup "$EXE" > /tmp/lua_ecs.log 2>&1 &
sleep 4
if pgrep -x lua_ecs >/dev/null; then
  echo "OK -> lua_ecs tourne (PID $(pgrep -x lua_ecs))"
else
  echo "ARRET -- log :"; tail -20 /tmp/lua_ecs.log
fi
echo "--- erreurs Lua eventuelles ---"
grep -i "\[Lua\]" /tmp/lua_ecs.log || echo "aucune erreur Lua"
