#!/usr/bin/env bash
# Télécharge un pack Kenney CC0 (Pixel Platformer) et extrait les PNG dans
# examples/01_perso2d/assets/kenney/ (le panneau Assets de l'éditeur les liste
# automatiquement, en récursif).
set -e
ASSETS="/mnt/c/Users/ggine/MoteurJV/examples/01_perso2d/assets/kenney"
PAGE="https://kenney.nl/assets/pixel-platformer"

echo "Recherche du lien de téléchargement..."
URL=$(curl -sL --max-time 25 "$PAGE" | grep -oE 'https://kenney\.nl/media/pages/assets/[^"]+\.zip' | head -1)
if [ -z "$URL" ]; then
  echo "ECHEC: lien .zip introuvable sur la page."
  exit 1
fi
echo "URL: $URL"

mkdir -p "$ASSETS"
echo "Téléchargement..."
curl -sL --max-time 120 -o /tmp/kenney.zip "$URL"
echo "Taille: $(du -h /tmp/kenney.zip | cut -f1)"

echo "Extraction des PNG..."
rm -rf /tmp/kenney_extract && mkdir -p /tmp/kenney_extract
unzip -o -q /tmp/kenney.zip -d /tmp/kenney_extract
# Copie tous les PNG (aplatis) dans le dossier kenney des assets.
find /tmp/kenney_extract -type f -name '*.png' -exec cp -f {} "$ASSETS/" \;
echo "PNG copiés: $(ls -1 "$ASSETS"/*.png 2>/dev/null | wc -l)"
ls -1 "$ASSETS" | head -10
