#!/usr/bin/env bash
echo '=== process ==='
if pgrep -x jeu >/dev/null; then echo "jeu tourne (PID $(pgrep -x jeu))"; else echo 'jeu arrete'; fi
echo '=== lignes interessantes (init / erreurs) ==='
grep -i -E 'error|warn|fail|fault|texture|audio: device|backend|image:|window' /tmp/jeu.log 2>/dev/null || echo '(rien / log absent)'
echo '=== 6 dernieres lignes ==='
tail -n 6 /tmp/jeu.log 2>/dev/null || echo '(log absent)'
