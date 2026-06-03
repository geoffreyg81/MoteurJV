#!/usr/bin/env bash
# Script de build de MoteurJV pour Linux / WSL2.
# Usage :  ./build.sh           (configure + compile)
#          ./build.sh run       (compile puis lance la démo)
#          ./build.sh clean      (efface le dossier de build)
set -e

# Répertoire des sources = dossier de ce script.
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# On compile hors de /mnt/c (système de fichiers Windows lent) : build dans ~ .
BUILD_DIR="$HOME/.cache/mjv-build"

case "${1:-build}" in
  clean)
    rm -rf "$BUILD_DIR"
    echo "Build nettoyé."
    exit 0
    ;;
esac

cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" -j"$(nproc)"

if [ "${1:-build}" = "run" ]; then
  # Cible à lancer : "perso2d" (démo, défaut) ou "jeu" (le petit jeu).
  case "${2:-perso2d}" in
    jeu) EXE="$BUILD_DIR/examples/02_jeu/jeu" ;;
    lua) EXE="$BUILD_DIR/examples/03_lua/lua_demo" ;;
    lua-ecs) EXE="$BUILD_DIR/examples/04_lua_ecs/lua_ecs" ;;
    editor)  EXE="$BUILD_DIR/examples/05_editor/editor" ;;
    *)   EXE="$BUILD_DIR/examples/01_perso2d/perso2d" ;;
  esac
  echo "==> Lancement : $EXE"
  # Active l'audio de WSLg (PulseAudio) s'il est présent.
  if [ -S /mnt/wslg/PulseServer ]; then
    export PULSE_SERVER=unix:/mnt/wslg/PulseServer
  fi
  "$EXE"
fi
