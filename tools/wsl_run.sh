#!/usr/bin/env bash
# Lance la démo en arrière-plan et vérifie qu'elle tourne.
EXE="$HOME/.cache/mjv-build/examples/01_perso2d/perso2d"
nohup "$EXE" > /tmp/perso2d.log 2>&1 &
sleep 3
if pgrep -x perso2d >/dev/null; then
  echo "FENETRE OUVERTE -> perso2d tourne (PID $(pgrep -x perso2d))"
else
  echo "process arrete -- log:"
  cat /tmp/perso2d.log
fi
