#!/usr/bin/env python3
"""
Procedural cozy audio generator for HEARTH 64.
Generates seamless looping fireplace rumble, authentic wood crackle/pop one-shots,
stoke whoosh, log thud, muffled cabin rain loop, and a dreamy N64-style ambient synth loop.
Uses pure Python + ffmpeg for warm EQ/reverb shaping.
"""

import math
import os
import random
import struct
import subprocess
import wave

SR = 22050
OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets_raw", "audio")


def write_wav(path, samples, sr=SR):
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sr)
        buf = bytearray()
        for s in samples:
            v = max(-1.0, min(1.0, s))
            buf.extend(struct.pack("<h", int(v * 32767.0)))
        wf.writeframes(bytes(buf))


def crossfade_loop(samples, loop_len):
    """Equal-power seamless crossfade from tail [loop_len:] onto head [:xfade]."""
    xfade = len(samples) - loop_len
    out = list(samples[:loop_len])
    for i in range(xfade):
        t = i / float(xfade)
        # Equal-power fade
        w_head = math.sin(t * math.pi * 0.5)
        w_tail = math.cos(t * math.pi * 0.5)
        out[i] = out[i] * w_head + samples[loop_len + i] * w_tail
    return out


def normalize(samples, peak=0.82):
    mx = max((abs(s) for s in samples), default=1e-6)
    if mx < 1e-6:
        return samples
    g = peak / mx
    return [s * g for s in samples]


def one_pole_lp(samples, cutoff_hz, sr=SR):
    rc = 1.0 / (2.0 * math.pi * cutoff_hz)
    dt = 1.0 / sr
    a = dt / (rc + dt)
    out = [0.0] * len(samples)
    y = 0.0
    for i, x in enumerate(samples):
        y += a * (x - y)
        out[i] = y
    return out


def one_pole_hp(samples, cutoff_hz, sr=SR):
    rc = 1.0 / (2.0 * math.pi * cutoff_hz)
    dt = 1.0 / sr
    a = rc / (rc + dt)
    out = [0.0] * len(samples)
    y = 0.0
    prev_x = 0.0
    for i, x in enumerate(samples):
        y = a * (y + x - prev_x)
        prev_x = x
        out[i] = y
    return out


def pink_noise(n, rng):
    """Voss-McCartney style 7-pole pink noise."""
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0
    out = [0.0] * n
    for i in range(n):
        w = rng.uniform(-1.0, 1.0)
        b0 = 0.99886 * b0 + w * 0.0555179
        b1 = 0.99332 * b1 + w * 0.0750759
        b2 = 0.96900 * b2 + w * 0.1538520
        b3 = 0.86650 * b3 + w * 0.3104856
        b4 = 0.55000 * b4 + w * 0.5329522
        b5 = -0.7616 * b5 - w * 0.0168980
        out[i] = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362) * 0.11
        b6 = w * 0.115926
    return out


def gen_fire_bed():
    """12s seamless warm hearth rumble + fluttering flame roar + soft ember bed."""
    rng = random.Random(1001)
    loop_sec = 12.0
    xfade_sec = 2.0
    total_n = int((loop_sec + xfade_sec) * SR)
    loop_n = int(loop_sec * SR)

    p1 = pink_noise(total_n, rng)
    p2 = pink_noise(total_n, rng)
    p3 = pink_noise(total_n, rng)

    # Layer 1: Deep hearth rumble (35 - 150 Hz)
    rumble = one_pole_lp(one_pole_lp(p1, 145.0), 170.0)
    rumble = one_pole_hp(rumble, 32.0)

    # Layer 2: Organic licking flame flutter (180 - 650 Hz) modulated by slow turbulent LFOs
    flutter_raw = one_pole_hp(one_pole_lp(p2, 620.0), 170.0)
    lfo_raw = one_pole_lp([rng.uniform(-1.0, 1.0) for _ in range(total_n)], 3.8)
    lfo_slow = one_pole_lp([rng.uniform(-1.0, 1.0) for _ in range(total_n)], 0.9)

    # Layer 3: Gentle ember hiss (1000 - 3200 Hz)
    hiss = one_pole_hp(one_pole_lp(p3, 3000.0), 1100.0)

    # Soft micro-crackles embedded sparingly in the bed
    bed = [0.0] * total_n
    for i in range(total_n):
        t = i / float(SR)
        swell = 0.72 + 0.28 * math.sin(2.0 * math.pi * t / 4.0) + 0.15 * lfo_slow[i] * 8.0
        flut_env = max(0.25, 0.75 + 4.2 * lfo_raw[i])
        bed[i] = rumble[i] * 2.1 * swell + flutter_raw[i] * 0.95 * flut_env + hiss[i] * 0.14

    # Add a few tiny background sizzle micro-pops
    for _ in range(48):
        pos = rng.randint(200, total_n - 800)
        amp = rng.uniform(0.06, 0.18)
        freq = rng.uniform(1800, 4200)
        dec = rng.uniform(0.004, 0.015)
        for j in range(int(dec * SR * 4)):
            if pos + j < total_n:
                env = math.exp(-j / (dec * SR))
                bed[pos + j] += amp * env * (math.sin(2 * math.pi * freq * j / SR) * 0.5 + rng.uniform(-0.5, 0.5))

    loop = normalize(crossfade_loop(bed, loop_n), peak=0.72)
    write_wav(os.path.join(OUT_DIR, "fire_bed.wav"), loop)


def make_crackle_burst(seed, num_pops, duration, pitch_scale=1.0):
    rng = random.Random(seed)
    n = int(duration * SR)
    out = [0.0] * n
    for p in range(num_pops):
        start = int(rng.uniform(0.005, max(0.02, duration - 0.12)) * SR)
        amp = rng.uniform(0.45, 1.0) * (1.0 if p == 0 else rng.uniform(0.4, 0.85))
        f_snap = rng.uniform(1400, 3800) * pitch_scale
        f_wood = rng.uniform(220, 480) * pitch_scale
        decay_snap = rng.uniform(0.003, 0.011)
        decay_wood = rng.uniform(0.012, 0.032)
        length = min(n - start, int(0.09 * SR))
        for j in range(length):
            t = j / float(SR)
            e1 = math.exp(-t / decay_snap)
            e2 = math.exp(-t / decay_wood)
            # Crisp wood fiber crack + resonant wood body
            snap = (rng.uniform(-1.0, 1.0) * 0.65 + math.sin(2 * math.pi * f_snap * t) * 0.35) * e1
            wood = math.sin(2 * math.pi * f_wood * t + 0.5 * math.sin(2 * math.pi * f_wood * 1.7 * t)) * e2 * 0.35
            out[start + j] += amp * (snap + wood)
    # Warm low-pass so it sounds organic on CRT speakers, not harsh digital clicks
    out = one_pole_lp(out, 5200.0)
    out = one_pole_hp(out, 110.0)
    # Smooth tiny fade in/out
    fade = min(64, n // 4)
    for i in range(fade):
        g = i / float(fade)
        out[i] *= g
        out[n - 1 - i] *= g
    return normalize(out, peak=0.80)


def gen_crackles():
    configs = [
        (2001, 1, 0.22, 1.15),  # crisp single pop
        (2002, 2, 0.32, 0.95),  # warm double crack
        (2003, 3, 0.42, 1.05),  # triple resin crackle
        (2004, 4, 0.52, 1.25),  # bright ember sizzle cluster
        (2005, 1, 0.28, 0.78),  # deeper hollow log snap
        (2006, 3, 0.48, 0.88),  # cozy settling crackles
    ]
    for idx, (seed, pops, dur, ps) in enumerate(configs):
        samples = make_crackle_burst(seed, pops, dur, ps)
        write_wav(os.path.join(OUT_DIR, f"crackle_{idx}.wav"), samples)


def gen_stoke():
    """Warm rising flame whoosh + poker nudge + celebratory ember crackles."""
    rng = random.Random(3001)
    dur = 1.05
    n = int(dur * SR)
    p = pink_noise(n, rng)
    whoosh_lp = one_pole_hp(one_pole_lp(p, 520.0), 75.0)
    whoosh_hi = one_pole_hp(one_pole_lp(p, 2400.0), 600.0)
    out = [0.0] * n
    for i in range(n):
        t = i / float(SR)
        # Smooth swell envelope peaking around 0.28s
        env = math.sin(min(1.0, t / 0.28) * math.pi * 0.5) * math.exp(-max(0.0, t - 0.22) * 3.1)
        out[i] = whoosh_lp[i] * 2.2 * env + whoosh_hi[i] * 0.7 * env * env
    # Mix in 5 quick lively sparks
    sparks = make_crackle_burst(3002, 5, dur, 1.1)
    for i in range(n):
        out[i] = out[i] * 0.85 + sparks[i] * 0.45
    write_wav(os.path.join(OUT_DIR, "stoke.wav"), normalize(out, peak=0.82))


def gen_log_thud():
    """Warm hollow wooden log landing on grate + embers bursting."""
    rng = random.Random(4001)
    dur = 0.85
    n = int(dur * SR)
    out = [0.0] * n
    # Two wooden knocks (main land + slight bounce settle at 0.14s)
    hits = [(0.02, 1.0, 128.0, 295.0), (0.16, 0.42, 152.0, 340.0)]
    for t0, amp, f1, f2 in hits:
        s0 = int(t0 * SR)
        for j in range(int(0.24 * SR)):
            if s0 + j < n:
                t = j / float(SR)
                env1 = math.exp(-t * 22.0)
                env2 = math.exp(-t * 48.0)
                body = math.sin(2 * math.pi * (f1 * (1.0 + 0.25 * math.exp(-t * 40.0))) * t) * env1
                knock = math.sin(2 * math.pi * f2 * t) * env2 * 0.45
                noise = rng.uniform(-1.0, 1.0) * math.exp(-t * 90.0) * 0.25
                out[s0 + j] += amp * (body * 0.75 + knock + noise)
    # Followed by ember scatter
    sparks = make_crackle_burst(4009, 6, dur, 1.15)
    for i in range(n):
        t = i / float(SR)
        gate = 1.0 if t > 0.08 else t / 0.08
        out[i] = out[i] * 0.85 + sparks[i] * 0.48 * gate
    out = one_pole_lp(out, 4000.0)
    write_wav(os.path.join(OUT_DIR, "log_thud.wav"), normalize(out, peak=0.84))


def gen_rain_loop():
    """12s seamless warm muffled rain against cabin window."""
    rng = random.Random(5001)
    loop_sec = 12.0
    xfade_sec = 2.0
    total_n = int((loop_sec + xfade_sec) * SR)
    loop_n = int(loop_sec * SR)

    p = pink_noise(total_n, rng)
    # Cozy indoor-muffled rainfall
    rain = one_pole_hp(one_pole_lp(p, 2400.0), 220.0)
    # Gentle low-frequency roof patter warmth
    roof = one_pole_hp(one_pole_lp(p, 480.0), 90.0)

    out = [0.0] * total_n
    for i in range(total_n):
        t = i / float(SR)
        swell = 0.86 + 0.14 * math.sin(2.0 * math.pi * t / 6.0)
        out[i] = (rain[i] * 0.85 + roof[i] * 0.55) * swell

    # Soft muffled glass raindrop taps
    for _ in range(240):
        pos = rng.randint(100, total_n - 400)
        amp = rng.uniform(0.04, 0.16)
        freq = rng.uniform(900, 2600)
        dec = rng.uniform(0.004, 0.012)
        for j in range(int(dec * SR * 4)):
            if pos + j < total_n:
                t = j / float(SR)
                out[pos + j] += amp * math.exp(-t / dec) * math.sin(2 * math.pi * freq * t)

    loop = normalize(crossfade_loop(out, loop_n), peak=0.62)
    write_wav(os.path.join(OUT_DIR, "rain_loop.wav"), loop)


def midi_to_hz(m):
    return 440.0 * (2.0 ** ((m - 69) / 12.0))


def gen_ambient_loop():
    """
    20s seamless nostalgic 90s N64 chill ambient Rhodes/pad progression.
    Chords: Fmaj9 -> Dm9 -> Bbmaj7 -> C6add9 (5s each), warm & dreamy!
    """
    loop_sec = 20.0
    loop_n = int(loop_sec * SR)
    # Render 2 full cycles so reverb/release tails wrap seamlessly into cycle 2!
    total_n = loop_n * 2

    chords = [
        # (start_sec, midi notes, gentle bell melody notes)
        (0.0,  [53, 60, 64, 67, 69], [(0.4, 76), (2.2, 74), (3.6, 72)]),   # Fmaj9
        (5.0,  [50, 57, 60, 64, 69], [(5.5, 72), (7.6, 69), (9.0, 72)]),   # Dm9
        (10.0, [46, 53, 57, 60, 65], [(10.4, 74), (12.4, 72), (14.0, 69)]), # Bbmaj7
        (15.0, [48, 55, 60, 62, 64], [(15.5, 67), (17.2, 69), (18.6, 72)]), # C6add9
    ]

    buf = [0.0] * total_n

    for cycle in range(2):
        base_t = cycle * loop_sec
        for c_start, notes, bells in chords:
            # Warm Rhodes-pad chord (5.6s overlap for lush legato)
            dur = 5.6
            i0 = int((base_t + c_start) * SR)
            length = int(dur * SR)
            for ni, m in enumerate(notes):
                hz = midi_to_hz(m)
                # Subtle analog detune pair
                hz2 = hz * (1.0022 if ni % 2 == 0 else 0.9978)
                amp = 0.12 if ni == 0 else 0.085
                for j in range(length):
                    idx = i0 + j
                    if idx >= total_n:
                        break
                    t = j / float(SR)
                    # Soft slow attack + warm exponential release
                    atk = min(1.0, t / 0.55)
                    rel = 1.0 if t < 4.2 else max(0.0, 1.0 - (t - 4.2) / 1.4)
                    env = (atk ** 1.4) * (rel ** 1.6)
                    # Warm electric piano/pad timbre (fundamental + gentle 2nd & 3rd harmonics)
                    w = (
                        0.68 * math.sin(2 * math.pi * hz * t)
                        + 0.24 * math.sin(2 * math.pi * hz2 * t)
                        + 0.08 * math.sin(2 * math.pi * hz * 2.0 * t) * math.exp(-t * 0.7)
                    )
                    buf[idx] += amp * env * w

            # Gentle Dire-Dire-Docks / music-box sine tine notes
            for b_start, bm in bells:
                bhz = midi_to_hz(bm)
                bi0 = int((base_t + b_start) * SR)
                blen = int(2.6 * SR)
                for j in range(blen):
                    idx = bi0 + j
                    if idx >= total_n:
                        break
                    t = j / float(SR)
                    env = min(1.0, t / 0.025) * math.exp(-t * 1.45)
                    # Soft glassy tine
                    w = (
                        0.80 * math.sin(2 * math.pi * bhz * t)
                        + 0.20 * math.sin(2 * math.pi * bhz * 2.0 * t) * math.exp(-t * 4.0)
                    )
                    buf[idx] += 0.075 * env * w

    # Take the second cycle (which includes the wrapped tails from cycle 1!)
    raw_loop = buf[loop_n:loop_n * 2]
    # Warm gentle low-pass + subtle comb-delay echo wrapped circularly for seamless reverb!
    delays = [(int(0.19 * SR), 0.24), (int(0.31 * SR), 0.18), (int(0.47 * SR), 0.12)]
    wet = list(raw_loop)
    for d_samp, d_gain in delays:
        for i in range(loop_n):
            wet[i] += raw_loop[(i - d_samp) % loop_n] * d_gain
    wet = one_pole_lp(wet, 2200.0)
    wet = normalize(wet, peak=0.58)

    raw_path = os.path.join(OUT_DIR, "ambient_loop.wav")
    write_wav(raw_path, wet)


def gen_gobble():
    """Playful Thanksgiving turkey 'gobble-gobble-gobble!' synthesized vocal warble."""
    dur = 0.88
    n = int(dur * SR)
    out = [0.0] * n
    # 5 quick warbling gobble syllables with falling pitch contour
    syllables = [
        (0.03, 0.13, 640.0, 1.00),
        (0.17, 0.12, 600.0, 0.92),
        (0.30, 0.12, 560.0, 0.95),
        (0.43, 0.13, 520.0, 0.88),
        (0.58, 0.18, 460.0, 0.78),
    ]
    for t0, sdur, f0, amp in syllables:
        i0 = int(t0 * SR)
        slen = int(sdur * SR)
        phase = 0.0
        for j in range(slen):
            idx = i0 + j
            if idx >= n:
                break
            t = j / float(SR)
            u = j / float(slen)
            # Rapid glottal warble (~28 Hz) + pitch glide down
            warble = 1.0 + 0.18 * math.sin(2.0 * math.pi * 28.0 * t)
            pitch = f0 * (1.06 - 0.20 * u) * warble
            phase += 2.0 * math.pi * pitch / SR
            env = math.sin(u * math.pi) ** 0.7
            # Nasal reedy turkey formant (fundamental + 2nd + 3rd harmonic)
            w = (
                0.55 * math.sin(phase)
                + 0.35 * math.sin(2.0 * phase + 0.4)
                + 0.22 * math.sin(3.0 * phase)
                + 0.10 * math.sin(4.0 * phase)
            )
            out[idx] += amp * env * w * (0.72 + 0.28 * math.sin(2.0 * math.pi * 24.0 * t))
    out = one_pole_hp(one_pole_lp(out, 2600.0), 260.0)
    write_wav(os.path.join(OUT_DIR, "gobble.wav"), normalize(out, peak=0.80))


def gen_ribbit():
    """Playful 90s N64 frog 'rib-bit!' croak with resonant upward pitch chirp."""
    dur = 0.56
    n = int(dur * SR)
    out = [0.0] * n
    # Two-part frog croak: quick 'rib-' + resonant rising '-bit!'
    syllables = [
        (0.03, 0.12, 165.0, 340.0, 42.0, 0.85),
        (0.18, 0.24, 195.0, 560.0, 38.0, 1.00),
    ]
    for t0, sdur, f_start, f_end, pulse_hz, amp in syllables:
        i0 = int(t0 * SR)
        slen = int(sdur * SR)
        phase = 0.0
        for j in range(slen):
            idx = i0 + j
            if idx >= n:
                break
            t = j / float(SR)
            u = j / float(slen)
            # Sweeping resonant formant + croaky glottal trill
            freq = f_start + (f_end - f_start) * (u ** 1.25)
            phase += 2.0 * math.pi * freq / SR
            trill = 0.55 + 0.45 * math.sin(2.0 * math.pi * pulse_hz * t)
            env = (math.sin(u * math.pi) ** 0.65) * trill
            w = (
                0.55 * math.sin(phase)
                + 0.35 * math.sin(2.0 * phase + 0.3)
                + 0.25 * math.sin(3.0 * phase)
                + 0.12 * math.sin(4.0 * phase)
            )
            out[idx] += amp * env * w
    out = one_pole_hp(one_pole_lp(out, 2200.0), 140.0)
    write_wav(os.path.join(OUT_DIR, "ribbit.wav"), normalize(out, peak=0.82))


def gen_ding():
    """Crisp cheerful kitchen oven-timer bell 'DING!' for the roast turkey Easter egg."""
    dur = 1.25
    n = int(dur * SR)
    out = [0.0] * n
    # Bright metallic bell partials (fundamental + classic inharmonic bell overtones)
    partials = [
        (1760.0, 1.00, 2.6),
        (3528.0, 0.52, 4.2),
        (4710.0, 0.34, 6.0),
        (6340.0, 0.22, 8.5),
        (882.0,  0.20, 2.2),
    ]
    for i in range(n):
        t = i / float(SR)
        # Tiny bright hammer strike click in first 4ms
        click = math.exp(-t * 600.0) * math.sin(2.0 * math.pi * 4100.0 * t) * 0.45
        val = click
        for hz, amp, dec in partials:
            val += amp * math.exp(-t * dec) * math.sin(2.0 * math.pi * hz * t)
        # Ultra-short 1ms attack to prevent sample discontinuity
        atk = min(1.0, t / 0.0012)
        out[i] = val * atk
    write_wav(os.path.join(OUT_DIR, "ding.wav"), normalize(out, peak=0.84))


def gen_magic():
    """Whimsical 90s N64 wizard wand sparkle chime + magical 'poof!' shimmer."""
    rng = random.Random(7777)
    dur = 1.20
    n = int(dur * SR)
    out = [0.0] * n
    # Ascending fairy-glockenspiel sparkle arpeggio (C6, E6, G6, B6, D7, E7, G7)
    notes = [
        (0.02, 1046.5, 0.55),
        (0.09, 1318.5, 0.60),
        (0.16, 1568.0, 0.65),
        (0.23, 1975.5, 0.72),
        (0.30, 2349.3, 0.78),
        (0.38, 2637.0, 0.85),
        (0.47, 3136.0, 0.95),
        # Final transformation twinkle chord right at ~0.68s!
        (0.66, 2093.0, 0.80),
        (0.68, 2637.0, 0.80),
        (0.70, 3136.0, 0.85),
    ]
    for t0, hz, amp in notes:
        i0 = int(t0 * SR)
        nlen = int(0.45 * SR)
        for j in range(nlen):
            idx = i0 + j
            if idx >= n:
                break
            t = j / float(SR)
            env = min(1.0, t / 0.004) * math.exp(-t * 6.5)
            # Glassy celesta/bell shimmer
            w = 0.72 * math.sin(2 * math.pi * hz * t) + 0.28 * math.sin(2 * math.pi * hz * 2.01 * t) * math.exp(-t * 9.0)
            out[idx] += amp * env * w * 0.35

    # Soft sparkly airy 'poof' right at 0.65s
    poof = one_pole_hp(one_pole_lp([rng.uniform(-1.0, 1.0) for _ in range(int(0.35 * SR))], 4200.0), 1400.0)
    p0 = int(0.64 * SR)
    for j, s in enumerate(poof):
        if p0 + j < n:
            t = j / float(SR)
            env = min(1.0, t / 0.015) * math.exp(-t * 10.5)
            out[p0 + j] += s * env * 0.32

    write_wav(os.path.join(OUT_DIR, "magic.wav"), normalize(out, peak=0.84))


def gen_meow():
    """Sweet cozy 90s N64 cat 'mrr-oww~' meow with rising-then-falling formant contour."""
    dur = 0.68
    n = int(dur * SR)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / float(SR)
        u = i / float(n)
        # Expressive cat pitch curve: soft rise (460 -> 760 Hz) then gentle cozy fall (-> 490 Hz)
        if u < 0.34:
            f0 = 460.0 + 300.0 * math.sin((u / 0.34) * math.pi * 0.5)
        else:
            v = (u - 0.34) / 0.66
            f0 = 760.0 - 270.0 * (v ** 1.35)
        # Subtle warm vibrato
        f0 *= 1.0 + 0.015 * math.sin(2.0 * math.pi * 6.5 * t)
        phase += 2.0 * math.pi * f0 / SR
        # Smooth attack & gentle tapering release
        env = min(1.0, u / 0.14) * ((1.0 - u) ** 0.75)
        # Vowel-like feline harmonic blend (opens from 'm' to 'aow' to 'w')
        openness = math.sin(min(1.0, u * 1.15) * math.pi)
        w = (
            0.55 * math.sin(phase)
            + (0.28 + 0.18 * openness) * math.sin(2.0 * phase + 0.2)
            + (0.10 + 0.20 * openness) * math.sin(3.0 * phase)
            + (0.04 + 0.09 * openness) * math.sin(4.0 * phase)
        )
        out[i] = env * w
    out = one_pole_hp(one_pole_lp(out, 2800.0), 260.0)
    write_wav(os.path.join(OUT_DIR, "meow.wav"), normalize(out, peak=0.78))


def gen_christmas_loop():
    """
    20s seamless nostalgic 90s N64 winter celesta/music-box & warm Rhodes pad loop.
    Chords: Cmaj9 -> Am9 -> Fmaj7 -> G6add9 (5s each), with gentle sleigh-bell shimmer.
    """
    rng = random.Random(1225)
    loop_sec = 20.0
    loop_n = int(loop_sec * SR)
    total_n = loop_n * 2

    chords = [
        (0.0,  [48, 55, 60, 62, 64, 67], [(0.3, 76), (1.7, 79), (3.1, 74), (4.2, 72)], [0.25, 2.75]),
        (5.0,  [45, 52, 57, 59, 60, 64], [(5.3, 72), (6.8, 76), (8.2, 74), (9.3, 71)], [5.25, 7.75]),
        (10.0, [41, 48, 53, 57, 60, 64], [(10.3, 72), (11.8, 74), (13.1, 76), (14.2, 72)], [10.25, 12.75]),
        (15.0, [43, 50, 55, 57, 59, 62], [(15.3, 71), (16.7, 74), (18.1, 79), (19.1, 76)], [15.25, 17.75]),
    ]

    buf = [0.0] * total_n

    for cycle in range(2):
        base_t = cycle * loop_sec
        for c_start, notes, bells, sleigh_times in chords:
            dur = 5.6
            i0 = int((base_t + c_start) * SR)
            length = int(dur * SR)
            for ni, m in enumerate(notes):
                hz = midi_to_hz(m)
                hz2 = hz * (1.0020 if ni % 2 == 0 else 0.9980)
                amp = 0.11 if ni == 0 else 0.072
                for j in range(length):
                    idx = i0 + j
                    if idx >= total_n:
                        break
                    t = j / float(SR)
                    atk = min(1.0, t / 0.50)
                    rel = 1.0 if t < 4.2 else max(0.0, 1.0 - (t - 4.2) / 1.4)
                    env = (atk ** 1.4) * (rel ** 1.6)
                    w = (
                        0.68 * math.sin(2 * math.pi * hz * t)
                        + 0.24 * math.sin(2 * math.pi * hz2 * t)
                        + 0.08 * math.sin(2 * math.pi * hz * 2.0 * t) * math.exp(-t * 0.8)
                    )
                    buf[idx] += amp * env * w

            # Crystalline celesta / music-box winter melody
            for b_start, bm in bells:
                bhz = midi_to_hz(bm)
                bi0 = int((base_t + b_start) * SR)
                blen = int(2.6 * SR)
                for j in range(blen):
                    idx = bi0 + j
                    if idx >= total_n:
                        break
                    t = j / float(SR)
                    env = min(1.0, t / 0.014) * math.exp(-t * 1.65)
                    w = (
                        0.72 * math.sin(2 * math.pi * bhz * t)
                        + 0.22 * math.sin(2 * math.pi * bhz * 2.0 * t) * math.exp(-t * 3.5)
                        + 0.06 * math.sin(2 * math.pi * bhz * 3.01 * t) * math.exp(-t * 6.0)
                    )
                    buf[idx] += 0.082 * env * w

            # Gentle soft distant sleigh-bell shimmer accents
            for st in sleigh_times:
                si0 = int((base_t + st) * SR)
                slen = int(0.35 * SR)
                for j in range(slen):
                    idx = si0 + j
                    if idx >= total_n:
                        break
                    t = j / float(SR)
                    env = min(1.0, t / 0.025) * math.exp(-t * 8.5)
                    shimmer = (
                        0.40 * math.sin(2 * math.pi * 3480.0 * t)
                        + 0.35 * math.sin(2 * math.pi * 4320.0 * t)
                        + 0.25 * math.sin(2 * math.pi * 5180.0 * t)
                        + 0.22 * rng.uniform(-1.0, 1.0)
                    )
                    buf[idx] += 0.016 * env * shimmer

    raw_loop = buf[loop_n:loop_n * 2]
    delays = [(int(0.19 * SR), 0.24), (int(0.31 * SR), 0.18), (int(0.47 * SR), 0.12)]
    wet = list(raw_loop)
    for d_samp, d_gain in delays:
        for i in range(loop_n):
            wet[i] += raw_loop[(i - d_samp) % loop_n] * d_gain
    wet = one_pole_lp(wet, 2800.0)
    wet = normalize(wet, peak=0.58)
    write_wav(os.path.join(OUT_DIR, "christmas_loop.wav"), wet)


def gen_snow_loop():
    """12s seamless cozy muffled winter breeze + soft distant frost-chime whispers."""
    rng = random.Random(1201)
    loop_sec = 12.0
    xfade_sec = 2.0
    total_n = int((loop_sec + xfade_sec) * SR)
    loop_n = int(loop_sec * SR)

    p1 = pink_noise(total_n, rng)
    p2 = pink_noise(total_n, rng)

    breeze = one_pole_hp(one_pole_lp(p1, 780.0), 110.0)
    hush = one_pole_hp(one_pole_lp(p2, 1800.0), 500.0)

    out = [0.0] * total_n
    for i in range(total_n):
        t = i / float(SR)
        gust = 0.65 + 0.35 * math.sin(2.0 * math.pi * t / 4.0) + 0.18 * math.sin(2.0 * math.pi * t / 3.0 + 1.2)
        out[i] = (breeze[i] * 0.95 + hush[i] * 0.42) * gust

    # Very soft, sparse distant frost-chime / sleigh-bell whispers
    chime_freqs = [2093.0, 2637.0, 3136.0, 3520.0]
    for _ in range(18):
        pos = rng.randint(400, total_n - int(0.8 * SR))
        hz = rng.choice(chime_freqs)
        amp = rng.uniform(0.015, 0.036)
        dec = rng.uniform(0.10, 0.24)
        for j in range(int(dec * SR * 4)):
            if pos + j < total_n:
                t = j / float(SR)
                env = min(1.0, t / 0.015) * math.exp(-t / dec)
                out[pos + j] += amp * env * math.sin(2.0 * math.pi * hz * t)

    loop = normalize(crossfade_loop(out, loop_n), peak=0.58)
    write_wav(os.path.join(OUT_DIR, "snow_loop.wav"), loop)


def gen_penguin():
    """Playful SM64-style cheerful penguin double-chirp ('kwah-kwah!')."""
    dur = 0.58
    n = int(dur * SR)
    out = [0.0] * n
    syllables = [
        (0.02, 0.21, 520.0, 820.0, 460.0, 0.95),
        (0.27, 0.25, 550.0, 840.0, 450.0, 1.00),
    ]
    for t0, sdur, f_start, f_peak, f_end, amp in syllables:
        i0 = int(t0 * SR)
        slen = int(sdur * SR)
        phase = 0.0
        for j in range(slen):
            idx = i0 + j
            if idx >= n:
                break
            t = j / float(SR)
            u = j / float(slen)
            if u < 0.35:
                freq = f_start + (f_peak - f_start) * math.sin((u / 0.35) * math.pi * 0.5)
            else:
                v = (u - 0.35) / 0.65
                freq = f_peak - (f_peak - f_end) * (v ** 1.15)
            freq *= 1.0 + 0.025 * math.sin(2.0 * math.pi * 22.0 * t)
            phase += 2.0 * math.pi * freq / SR
            env = min(1.0, u / 0.10) * ((1.0 - u) ** 0.55)
            w = (
                0.50 * math.sin(phase)
                + 0.38 * math.sin(2.0 * phase + 0.3)
                + 0.26 * math.sin(3.0 * phase)
                + 0.14 * math.sin(4.0 * phase)
            )
            out[idx] += amp * env * w
    out = one_pole_hp(one_pole_lp(out, 3200.0), 280.0)
    write_wav(os.path.join(OUT_DIR, "penguin.wav"), normalize(out, peak=0.82))


def gen_snowglobe():
    """Crisp festive sleigh-bell shake + ascending dreamy celesta sparkle cascade."""
    rng = random.Random(1224)
    dur = 1.45
    n = int(dur * SR)
    out = [0.0] * n

    # 1. Crisp sleigh-bell shake clusters (4 shakes across ~0.75s)
    shakes = [0.04, 0.22, 0.42, 0.64]
    bell_partials = [2950.0, 3680.0, 4420.0, 5260.0, 6190.0]
    noise_raw = one_pole_hp(one_pole_lp([rng.uniform(-1.0, 1.0) for _ in range(n)], 6800.0), 2600.0)
    for st in shakes:
        i0 = int(st * SR)
        slen = int(0.24 * SR)
        for j in range(slen):
            idx = i0 + j
            if idx >= n:
                break
            t = j / float(SR)
            env = min(1.0, t / 0.008) * math.exp(-t * 14.0)
            bell = 0.0
            for bi, hz in enumerate(bell_partials):
                bell += (0.28 - bi * 0.04) * math.sin(2.0 * math.pi * hz * t)
            out[idx] += env * (bell * 0.55 + noise_raw[idx] * 0.22)

    # 2. Ascending dreamy celesta sparkle cascade (C6 -> E6 -> G6 -> B6 -> C7 -> E7 -> G7)
    notes = [
        (0.18, 1046.5, 0.48),
        (0.32, 1318.5, 0.55),
        (0.46, 1568.0, 0.62),
        (0.60, 1975.5, 0.68),
        (0.74, 2093.0, 0.78),
        (0.88, 2637.0, 0.84),
        (1.00, 3136.0, 0.90),
    ]
    for t0, hz, amp in notes:
        i0 = int(t0 * SR)
        nlen = int(0.55 * SR)
        for j in range(nlen):
            idx = i0 + j
            if idx >= n:
                break
            t = j / float(SR)
            env = min(1.0, t / 0.005) * math.exp(-t * 5.2)
            w = 0.74 * math.sin(2.0 * math.pi * hz * t) + 0.26 * math.sin(2.0 * math.pi * hz * 2.005 * t) * math.exp(-t * 8.0)
            out[idx] += amp * env * w * 0.34

    write_wav(os.path.join(OUT_DIR, "snowglobe.wav"), normalize(out, peak=0.84))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    print("Generating cozy hearth audio assets...")
    gen_fire_bed()
    gen_crackles()
    gen_stoke()
    gen_log_thud()
    gen_rain_loop()
    gen_ambient_loop()
    gen_gobble()
    gen_ribbit()
    gen_ding()
    gen_magic()
    gen_meow()
    gen_christmas_loop()
    gen_snow_loop()
    gen_penguin()
    gen_snowglobe()
    print(f"Generated 20 audio WAVs in {OUT_DIR}")


if __name__ == "__main__":
    main()
