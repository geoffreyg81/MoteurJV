#!/usr/bin/env python3
"""Génère les sons de test (WAV PCM 16 bits), sans dépendance externe.

Produit dans examples/01_perso2d/assets/ :
  - step.wav   : un pas (petit clic court)
  - bump.wav   : un choc (thud grave) joué lors d'une collision
  - music.wav  : une courte boucle musicale

raylib lit le WAV nativement (PlaySound / LoadMusicStream).
"""
import math
import os
import struct
import wave

SR = 22050  # fréquence d'échantillonnage


def write_wav(path, samples):
    # samples : liste de floats dans [-1, 1]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)  # 16 bits
        w.setframerate(SR)
        frames = bytearray()
        for s in samples:
            v = max(-1.0, min(1.0, s))
            frames += struct.pack("<h", int(v * 32767))
        w.writeframes(bytes(frames))
    print("écrit :", path, f"({len(samples)/SR:.2f}s)")


def tone(freq, dur, vol=0.5, decay=8.0, wave_kind="sine"):
    n = int(SR * dur)
    out = []
    for i in range(n):
        t = i / SR
        env = math.exp(-decay * t)  # enveloppe descendante
        if wave_kind == "square":
            v = 1.0 if math.sin(2 * math.pi * freq * t) >= 0 else -1.0
        else:
            v = math.sin(2 * math.pi * freq * t)
        out.append(v * vol * env)
    return out


def noise_click(dur, vol=0.5, decay=40.0):
    import random
    rnd = random.Random(1234)
    n = int(SR * dur)
    return [(rnd.uniform(-1, 1)) * vol * math.exp(-decay * (i / SR)) for i in range(n)]


here = os.path.dirname(os.path.abspath(__file__))
assets = os.path.normpath(os.path.join(here, "..", "examples", "01_perso2d", "assets"))

# --- Pas : clic bref et sec ---------------------------------------------------
step = noise_click(0.06, vol=0.35, decay=60.0)
write_wav(os.path.join(assets, "step.wav"), step)

# --- Choc / atterrissage : thud grave ----------------------------------------
bump = tone(90, 0.18, vol=0.6, decay=18.0)
write_wav(os.path.join(assets, "bump.wav"), bump)


# --- Saut : petit bip ascendant ----------------------------------------------
def sweep(f0, f1, dur, vol=0.4, decay=6.0):
    n = int(SR * dur)
    out = []
    for i in range(n):
        t = i / SR
        f = f0 + (f1 - f0) * (i / n)
        out.append(math.sin(2 * math.pi * f * t) * vol * math.exp(-decay * t))
    return out


write_wav(os.path.join(assets, "jump.wav"), sweep(320, 720, 0.14))

# --- Victoire : petit jingle ascendant ---------------------------------------
C5, E5, G5, C6 = 523.25, 659.25, 783.99, 1046.5
win = (tone(C5, 0.10, 0.40, 6.0, "square") + tone(E5, 0.10, 0.40, 6.0, "square") +
       tone(G5, 0.10, 0.40, 6.0, "square") + tone(C6, 0.24, 0.45, 4.0, "square"))
write_wav(os.path.join(assets, "win.wav"), win)

# --- Défaite : descente "game over" ------------------------------------------
write_wav(os.path.join(assets, "lose.wav"), sweep(420, 110, 0.55, vol=0.5, decay=3.5))

# --- Tir : petit laser descendant --------------------------------------------
write_wav(os.path.join(assets, "shoot.wav"), sweep(900, 300, 0.10, vol=0.35, decay=14.0))

# --- Pièce ramassée : deux notes claires ascendantes -------------------------
coin = tone(988, 0.05, vol=0.4, decay=10.0) + tone(1319, 0.10, vol=0.4, decay=8.0)
write_wav(os.path.join(assets, "coin.wav"), coin)

# --- Musique : courte boucle (basse + mélodie) -------------------------------
# Notes (Hz). Petite mélodie en La mineur pentatonique.
A3, C4, D4, E4, G4, A4 = 220.0, 261.63, 293.66, 329.63, 392.0, 440.0
melody = [A4, C4, D4, E4, G4, E4, D4, C4]   # 8 notes
bass_seq = [A3, A3, D4, E4]                  # 4 notes longues
beat = 0.28  # durée d'une note de mélodie

music = []
for i in range(len(melody)):
    note = tone(melody[i], beat, vol=0.22, decay=3.0, wave_kind="square")
    bass = tone(bass_seq[i % len(bass_seq)], beat, vol=0.18, decay=1.5)
    mixed = [(note[j] if j < len(note) else 0) + (bass[j] if j < len(bass) else 0)
             for j in range(int(SR * beat))]
    music += mixed
write_wav(os.path.join(assets, "music.wav"), music)
