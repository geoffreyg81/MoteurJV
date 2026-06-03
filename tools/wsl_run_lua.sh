#!/usr/bin/env bash
# Compile et lance l'exemple Lua (examples/03_lua).
pkill -x lua_demo 2>/dev/null || true
set -e
/mnt/c/Users/ggine/MoteurJV/build.sh
EXE="$HOME/.cache/mjv-build/examples/03_lua/lua_demo"
[ -S /mnt/wslg/PulseServer ] && export PULSE_SERVER=unix:/mnt/wslg/PulseServer
nohup "$EXE" > /tmp/lua_demo.log 2>&1 &
sleep 4
if pgrep -x lua_demo >/dev/null; then
  echo "OK -> lua_demo tourne (PID $(pgrep -x lua_demo))"
else
  echo "ARRET -- log :"; tail -20 /tmp/lua_demo.log
fi
