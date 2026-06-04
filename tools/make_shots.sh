#!/usr/bin/env bash
# Génère des captures d'écran des exemples (via la variable MJV_SHOT du moteur),
# séquentiellement, dans docs/. À lancer après un build.
ROOT=/mnt/c/Users/ggine/MoteurJV
B="$HOME/.cache/mjv-build"
DOCS="$ROOT/docs"
mkdir -p "$DOCS"
[ -S /mnt/wslg/PulseServer ] && export PULSE_SERVER=unix:/mnt/wslg/PulseServer

shot() {
    local exe="$1" out="$2"
    local name; name="$(basename "$exe")"
    pkill -x "$name" 2>/dev/null; sleep 0.5
    MJV_SHOT="$out" nohup "$B/$exe" > /tmp/shot.log 2>&1 &
    local pid=$!
    sleep 4
    kill "$pid" 2>/dev/null; sleep 0.5
    pkill -x "$name" 2>/dev/null
    if [ -f "$out" ]; then echo "OK   $out ($(du -h "$out" | cut -f1))"; else echo "FAIL $out"; tail -4 /tmp/shot.log; fi
}

shot examples/05_editor/editor   "$DOCS/editor.png"
shot examples/06_3d/demo3d       "$DOCS/demo3d.png"
shot examples/02_jeu/jeu         "$DOCS/platformer.png"
shot examples/01_perso2d/perso2d "$DOCS/perso2d.png"
echo "--- fichiers ---"; ls -la "$DOCS"
