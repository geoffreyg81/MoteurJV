#!/usr/bin/env bash
# Stoppe l'ancienne démo, recompile, relance.
pkill -x perso2d 2>/dev/null || true
set -e
/mnt/c/Users/ggine/MoteurJV/build.sh
EXE="$HOME/.cache/mjv-build/examples/01_perso2d/perso2d"
[ -S /mnt/wslg/PulseServer ] && export PULSE_SERVER=unix:/mnt/wslg/PulseServer
nohup "$EXE" > /tmp/perso2d.log 2>&1 &
sleep 3
if pgrep -x perso2d >/dev/null; then
  echo "OK -> perso2d (ECS) tourne (PID $(pgrep -x perso2d))"
else
  echo "process arrete -- log:"
  cat /tmp/perso2d.log
fi
