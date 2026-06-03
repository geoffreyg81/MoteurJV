#!/usr/bin/env bash
echo '=== toolchain ==='
for c in gcc g++ cmake make git pkg-config; do
  if command -v "$c" >/dev/null 2>&1; then
    echo "$c -> $($c --version 2>/dev/null | head -1)"
  else
    echo "$c -> ABSENT"
  fi
done
echo '=== libs X11/GL dev (raylib) ==='
for p in libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev; do
  if dpkg -s "$p" >/dev/null 2>&1; then echo "$p -> OK"; else echo "$p -> manquant"; fi
done
echo '=== sudo sans mot de passe ? ==='
if sudo -n true 2>/dev/null; then echo 'OUI (passwordless)'; else echo 'NON (mot de passe requis)'; fi
echo "=== user: $(whoami) ==="
echo "=== WAYLAND_DISPLAY=$WAYLAND_DISPLAY  DISPLAY=$DISPLAY ==="
