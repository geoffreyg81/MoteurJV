#!/usr/bin/env bash
# Installe les dépendances de build du moteur dans Ubuntu (à lancer en root).
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y \
  cmake \
  libgl1-mesa-dev \
  libx11-dev \
  libxrandr-dev \
  libxinerama-dev \
  libxcursor-dev \
  libxi-dev \
  libwayland-dev \
  libxkbcommon-dev
echo "=== termine ==="
cmake --version | head -1
