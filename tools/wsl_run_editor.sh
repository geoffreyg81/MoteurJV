#!/usr/bin/env bash
# Compile et lance l'éditeur (examples/05_editor).
pkill -x editor 2>/dev/null || true
set -e
/mnt/c/Users/ggine/MoteurJV/build.sh
EXE="$HOME/.cache/mjv-build/examples/05_editor/editor"
[ -S /mnt/wslg/PulseServer ] && export PULSE_SERVER=unix:/mnt/wslg/PulseServer
nohup "$EXE" > /tmp/editor.log 2>&1 &
sleep 4
if pgrep -x editor >/dev/null; then
  echo "OK -> editor tourne (PID $(pgrep -x editor))"
else
  echo "ARRET -- log :"; tail -20 /tmp/editor.log
fi
