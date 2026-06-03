#!/usr/bin/env bash
# Lance la démo avec l'audio WSLg (PulseAudio) activé, et diagnostique.
pkill -x perso2d 2>/dev/null || true
export PULSE_SERVER=unix:/mnt/wslg/PulseServer
EXE="$HOME/.cache/mjv-build/examples/01_perso2d/perso2d"
nohup "$EXE" > /tmp/perso2d.log 2>&1 &
PID=$!
sleep 4
if kill -0 "$PID" 2>/dev/null; then
  echo "OK -> perso2d tourne (PID $PID)"
else
  echo "ARRET premature. Code/log :"
  wait "$PID"; echo "exit=$?"
fi
echo "--- backend audio ---"
grep -i -E 'Backend|AUDIO:' /tmp/perso2d.log | head -5
