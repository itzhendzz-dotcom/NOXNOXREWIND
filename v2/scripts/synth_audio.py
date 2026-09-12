#!/usr/bin/env python3
"""Generate original temporal rewind audio for NOXXA REWIND.

The design goal is airy / tape-reversal / crystalline time-warp texture without
copying or sampling any third-party game audio.
"""

import math
import random
import wave
from array import array
from pathlib import Path

SR = 48000
rng = random.Random(0x4E4F5858)


def clamp(x):
    return max(-1.0, min(1.0, x))


def softclip(x):
    return math.tanh(x * 1.15) / math.tanh(1.15)


def highpass(samples, alpha=0.985):
    out = [0.0] * len(samples)
    prev_x = 0.0
    prev_y = 0.0
    for i, x in enumerate(samples):
        y = alpha * (prev_y + x - prev_x)
        out[i] = y
        prev_x = x
        prev_y = y
    return out


def lowpass(samples, alpha=0.035):
    out = [0.0] * len(samples)
    y = 0.0
    for i, x in enumerate(samples):
        y += alpha * (x - y)
        out[i] = y
    return out


def stereo_wav(path, left, right):
    assert len(left) == len(right)
    pcm = array('h')
    for l, r in zip(left, right):
        pcm.append(int(clamp(softclip(l)) * 32767))
        pcm.append(int(clamp(softclip(r)) * 32767))
    with wave.open(str(path), 'wb') as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(SR)
        wf.writeframes(pcm.tobytes())


def make_noise(n):
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def chime(t, base, phase=0.0):
    return (
        0.50 * math.sin(2 * math.pi * base * t + phase) +
        0.27 * math.sin(2 * math.pi * base * 1.63 * t + 0.6 + phase) +
        0.14 * math.sin(2 * math.pi * base * 2.41 * t + 1.2 + phase)
    )


def build_enter():
    dur = 1.05
    n = int(SR * dur)
    noise = highpass(lowpass(make_noise(n), 0.11), 0.94)
    l, r = [], []
    for i in range(n):
        t = i / SR
        x = t / dur
        swell = math.sin(min(1.0, x) * math.pi * 0.5) ** 1.7
        air = noise[i] * (0.04 + 0.26 * swell)
        flutter = 1.0 + 0.007 * math.sin(2 * math.pi * 4.7 * t)
        crystal = chime(t * flutter, 540 + 210 * x) * (swell ** 2) * 0.12
        sub = math.sin(2 * math.pi * (52 + 12 * x) * t) * 0.035 * swell
        reverse_click = math.sin(2 * math.pi * 1700 * t) * math.exp(-28 * max(0.0, 0.93 - x)) * 0.015
        l.append(air + crystal + sub + reverse_click)
        r.append(air * 0.92 + chime(t * flutter, 548 + 205 * x, 0.28) * (swell ** 2) * 0.12 + sub)
    return l, r


def build_loop():
    dur = 2.8
    n = int(SR * dur)
    raw = make_noise(n)
    airy = highpass(lowpass(raw, 0.08), 0.965)
    fade = int(SR * 0.22)
    for i in range(fade):
        a = i / max(1, fade - 1)
        airy[i] = airy[i] * a + airy[n - fade + i] * (1.0 - a)

    l, r = [], []
    for i in range(n):
        t = i / SR
        cyc = t / dur
        breath = 0.68 + 0.32 * math.sin(2 * math.pi * cyc - math.pi / 2) ** 2
        flutter = 1.0 + 0.012 * math.sin(2 * math.pi * 0.73 * t) + 0.004 * math.sin(2 * math.pi * 5.3 * t)
        period = dur / 4.0
        local = (t % period) / period
        rise = local ** 2.2
        shimmer_env = rise * (1.0 - 0.20 * local)
        crystal_l = chime(t * flutter, 612, 0.0) * shimmer_env * 0.055
        crystal_r = chime(t * flutter, 619, 0.42) * shimmer_env * 0.055
        low = (math.sin(2 * math.pi * 46 * t) + 0.45 * math.sin(2 * math.pi * 71 * t + 0.3)) * 0.018
        air_l = airy[i] * 0.17 * breath
        air_r = airy[(i + 137) % n] * 0.17 * breath
        tape = math.sin(2 * math.pi * (118 + 7 * math.sin(2 * math.pi * 0.2 * t)) * t) * 0.012
        l.append(air_l + crystal_l + low + tape)
        r.append(air_r + crystal_r + low - tape)
    return l, r


def build_release():
    dur = 0.85
    n = int(SR * dur)
    noise = highpass(lowpass(make_noise(n), 0.12), 0.95)
    l, r = [], []
    for i in range(n):
        t = i / SR
        x = t / dur
        decay = math.exp(-5.3 * x)
        snap = noise[i] * 0.20 * math.exp(-18 * x)
        glass = chime(t, 780 - 210 * x) * 0.15 * decay
        tail = math.sin(2 * math.pi * 330 * t) * 0.035 * math.exp(-3.5 * x)
        l.append(snap + glass + tail)
        r.append(snap * 0.88 + chime(t, 792 - 205 * x, 0.37) * 0.15 * decay + tail)
    return l, r


def main():
    out = Path(__file__).resolve().parent.parent / 'generated_audio'
    out.mkdir(parents=True, exist_ok=True)
    for name, fn in [
        ('rewind_enter.wav', build_enter),
        ('rewind_loop.wav', build_loop),
        ('rewind_release.wav', build_release),
    ]:
        left, right = fn()
        stereo_wav(out / name, left, right)
        print(out / name)


if __name__ == '__main__':
    main()
