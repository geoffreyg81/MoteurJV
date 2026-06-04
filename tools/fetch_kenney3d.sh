#!/usr/bin/env bash
# Télécharge un pack 3D Kenney CC0 (Platformer Kit) et l'extrait (arborescence
# préservée pour les .mtl / textures) dans examples/06_3d/assets/kenney3d/.
set -e
ASSETS="/mnt/c/Users/ggine/MoteurJV/examples/06_3d/assets/kenney3d"
PAGE="https://kenney.nl/assets/platformer-kit"

echo "Recherche du lien..."
URL=$(curl -sL --max-time 25 "$PAGE" | grep -oE 'https://kenney\.nl/media/pages/assets/[^"]+\.zip' | head -1)
[ -z "$URL" ] && { echo "ECHEC: lien introuvable"; exit 1; }
echo "URL: $URL"

mkdir -p "$ASSETS"
curl -sL --max-time 180 -o /tmp/kenney3d.zip "$URL"
echo "Taille: $(du -h /tmp/kenney3d.zip | cut -f1)"
rm -rf "$ASSETS"/* 2>/dev/null || true
unzip -o -q /tmp/kenney3d.zip -d "$ASSETS"
echo "=== modeles disponibles (glb/gltf/obj) ==="
find "$ASSETS" -type f \( -iname '*.glb' -o -iname '*.gltf' -o -iname '*.obj' \) | sed "s#$ASSETS/##" | sort | head -40
echo "=== formats ==="
find "$ASSETS" -type f -iname '*.glb' | wc -l | xargs echo "glb :"
find "$ASSETS" -type f -iname '*.gltf' | wc -l | xargs echo "gltf:"
find "$ASSETS" -type f -iname '*.obj' | wc -l | xargs echo "obj :"
