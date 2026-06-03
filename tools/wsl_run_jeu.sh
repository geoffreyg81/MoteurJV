#!/usr/bin/env bash
# Compile tout et lance le petit jeu (examples/02_jeu).
pkill -x jeu 2>/dev/null || true
pkill -x perso2d 2>/dev/null || true
set -e
/mnt/c/Users/ggine/MoteurJV/build.sh
EXE="$HOME/.cache/mjv-build/examples/02_jeu/jeu"
[ -S /mnt/wslg/PulseServer ] && export PULSE_SERVER=unix:/mnt/wslg/PulseServer
nohup "$EXE" > /tmp/jeu.log 2>&1 &
sleep 4
if pgrep -x jeu >/dev/null; then
  echo "OK -> jeu tourne (PID $(pgrep -x jeu))"
else
  echo "ARRET -- log :"; tail -20 /tmp/jeu.log
fi
