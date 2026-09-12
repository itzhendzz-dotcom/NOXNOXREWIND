#!/usr/bin/env python3
"""Generate NOXXA REWIND v2.1 temporal audio.

Soft reverse-air / tape suction / distant glass shimmer. No heavy impact,
no bass blast, no third-party samples. The bed is long so runtime playback
needs no looping callback thread.
"""

import math
import random
import wave
from array import array
from pathlib import Path

SR = 48000
rng = random.Random(0x2110A)


def clamp(x): return max(-1.0, min(1.0, x))


def stereo_wav(path, left, right):
    pcm = array('h')
    for l, r in zip(left, right):
        pcm.append(int(clamp(math.tanh(l * 1.08)) * 32767))
        pcm.append(int(clamp(math.tanh(r * 1.08)) * 32767))
    with wave.open(str(path), 'wb') as wf:
        wf.setnchannels(2); wf.setsampwidth(2); wf.setframerate(SR)
        wf.writeframes(pcm.tobytes())


def noise(n): return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def lp(xs, a):
    y = 0.0; out = []
    for x in xs:
        y += a * (x - y); out.append(y)
    return out


def hp(xs, a=0.985):
    py = 0.0; px = 0.0; out = []
    for x in xs:
        y = a * (py + x - px); out.append(y); py, px = y, x
    return out


def bell(t, f, phase=0.0):
    return (math.sin(2*math.pi*f*t+phase)*0.62 +
            math.sin(2*math.pi*f*1.51*t+0.7+phase)*0.25 +
            math.sin(2*math.pi*f*2.02*t+1.3+phase)*0.13)


def make_enter():
    dur = 0.42; n = int(SR * dur); air = hp(lp(noise(n), 0.055), 0.97)
    l, r = [], []
    for i in range(n):
        t = i / SR; x = t / dur; rise = math.sin(x*math.pi*0.5)**2.2
        suck = air[i] * 0.28 * rise
        glass = bell(t, 460 + 390*x) * 0.055 * rise
        flutter = math.sin(2*math.pi*(82+44*x)*t) * 0.010 * rise
        l.append(suck + glass + flutter)
        r.append(suck*0.91 + bell(t, 472+376*x, 0.25)*0.052*rise - flutter)
    return l, r


def make_bed():
    dur = 9.3; n = int(SR * dur); raw = noise(n)
    air_a = hp(lp(raw, 0.028), 0.992); air_b = hp(lp(raw[::-1], 0.024), 0.990)
    l, r = [], []
    for i in range(n):
        t = i / SR; x = t / dur
        breath = 0.74 + 0.26 * math.sin(2*math.pi*0.17*t + 0.8)**2
        flutter = 1.0 + 0.0035 * math.sin(2*math.pi*3.8*t)
        local = (t % 1.55) / 1.55; rise = local**2.6; fade = (1.0-local)**0.25
        wisp = bell(t*flutter, 265+210*local) * 0.030 * rise * fade
        wisp_r = bell(t*flutter, 278+205*local, 0.31) * 0.028 * rise * fade
        tape = math.sin(2*math.pi*(96+3.0*math.sin(2*math.pi*0.09*t))*t) * 0.005
        grain = air_b[i] * 0.030 if (i % 1900) < 38 else 0.0
        edge = min(1.0, x*8.0) * min(1.0, (1.0-x)*8.0)
        l.append((air_a[i]*0.16*breath + air_b[i]*0.06 + wisp + tape + grain) * edge)
        r.append((air_a[(i+257)%n]*0.14*breath + air_b[(i+811)%n]*0.065 + wisp_r - tape - grain*0.55) * edge)
    return l, r


def make_release():
    dur = 0.58; n = int(SR * dur); air = hp(lp(noise(n), 0.05), 0.98)
    l, r = [], []
    for i in range(n):
        t = i / SR; x = t / dur; decay = math.exp(-5.8*x)
        breath = air[i] * 0.12 * decay
        glass = bell(t, 690-130*x) * 0.060 * decay
        tick = math.sin(2*math.pi*1450*t) * 0.018 * math.exp(-34*x)
        l.append(breath + glass + tick)
        r.append(breath*0.9 + bell(t, 706-125*x, 0.28)*0.056*decay - tick*0.65)
    return l, r


def main():
    out = Path(__file__).resolve().parent.parent / 'generated_audio'; out.mkdir(parents=True, exist_ok=True)
    for name, fn in [('rewind_enter.wav', make_enter), ('rewind_loop.wav', make_bed), ('rewind_release.wav', make_release)]:
        L, R = fn(); stereo_wav(out / name, L, R); print(out / name)


if __name__ == '__main__': main()
