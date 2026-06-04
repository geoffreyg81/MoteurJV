#!/usr/bin/env bash
pkill -x demo3d 2>/dev/null || true
EXE="$HOME/.cache/mjv-build/examples/06_3d/demo3d"
[ -S /mnt/wslg/PulseServer ] && export PULSE_SERVER=unix:/mnt/wslg/PulseServer
nohup "$EXE" > /tmp/demo3d.log 2>&1 &
sleep 4
if pgrep -x demo3d >/dev/null; then echo "OK -> demo3d tourne (PID $(pgrep -x demo3d))"; else echo "ARRET"; tail -15 /tmp/demo3d.log; fi
grep -i -E 'segfault|terminate|assert' /tmp/demo3d.log || echo "aucune erreur"
