#!/usr/bin/env python3
"""Generates the RecklessMuse factory preset library (Presets/*.xml).

Each preset only stores the parameters that differ from the defaults (see Source/Params.cpp);
the plug-in resets everything else to its default when a preset is loaded.
Values are *plain* values: Hz, seconds, 0..1 amounts, or the index of a choice.

Run from the repository root:  python3 tools/generate_presets.py
"""

import os
import random
import re
from xml.sax.saxutils import quoteattr

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Presets")

# --- choice indices --------------------------------------------------------
OCT = {"32": 0, "16": 1, "8": 2, "4": 3, "2": 4}
TRI, SAW, SQR, PULSE, NARROW = 0.0, 1 / 3, 2 / 3, 0.85, 1.0
MO_SINE, MO_TRI, MO_SAW, MO_RAMP, MO_SQR, MO_SH, MO_NOISE = range(7)
OSC1, OSC2, BOTH = 0, 1, 2
KB_OFF, KB_HALF, KB_FULL = 0, 1, 2
LP, HP = 0, 1
DB6, DB12, DB18, DB24 = 0, 1, 2, 3
SERIES, PARALLEL, STEREO = 0, 1, 2
L_SINE, L_TRI, L_SAW, L_RAMP, L_SQR, L_SH, L_SMOOTH = range(7)
PL_SINE, PL_TRI, PL_SQR, PL_SH = range(4)
POLY, MONO, UNISON = range(3)
PRIO_LAST, PRIO_LOW, PRIO_HIGH = range(3)
SINGLE, SPLIT, STACK = range(3)
ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_ORDER, ARP_RANDOM = range(5)
DIV = {d: i for i, d in enumerate(["1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                   "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1", "2/1", "4/1"])}
FM_1TO2, FM_MOD1, FM_MOD2, FM_MODBOTH = range(4)

# modulation matrix
S = dict(off=0, lfo1=1, lfo2=2, modosc=3, fenv=4, aenv=5, vel=6, wheel=7, at=8, key=9, rand=10, m1=11, m2=12)
D = dict(off=0, pitch=1, o1pitch=2, o2pitch=3, o1wave=4, o2wave=5, fm=6, o1lvl=7, o2lvl=8, noise=9, modrate=10,
         cutoff=11, f1cut=12, f2cut=13, f1res=14, f2res=15, ovl=16, vca=17, pan=18, l1rate=19, l2rate=20)


def mm(slot, src, dst, amt, via="off"):
    """One modulation matrix slot (1..8)."""
    return {f"mm{slot}Src": S[src], f"mm{slot}Dst": D[dst], f"mm{slot}Amt": amt, f"mm{slot}Via": S[via]}


def merge(*dicts):
    out = {}
    for d in dicts:
        out.update(d)
    return out


def timbre(prefix, d):
    return {prefix + k: v for k, v in d.items()}


PRESETS = []
NAMES = set()


def add(category, name, a, b=None, glob=None, seq=None, chord=None):
    key = (category, name)
    assert key not in NAMES, f"duplicate preset {key}"
    NAMES.add(key)
    params = merge(timbre("a_", a), timbre("b_", b or {}), glob or {})
    PRESETS.append((category, name, params, seq, chord))


# ---------------------------------------------------------------------------
# Building blocks
# ---------------------------------------------------------------------------
def osc(o1w=SAW, o2w=SAW, o1oct="8", o2oct="8", o1f=0.0, o2f=0.07, m1=0.85, m2=0.7, ring=0.0, mod=0.0, noise=0.0,
        ovl=0.25, sync=False, fm=0.0, fmsrc=FM_1TO2):
    d = dict(o1Wave=o1w, o2Wave=o2w, o1Oct=OCT[o1oct], o2Oct=OCT[o2oct], o1Freq=o1f, o2Freq=o2f,
             mixO1=m1, mixO2=m2, mixRing=ring, mixMod=mod, mixNoise=noise, mixOverload=ovl)
    if sync:
        d["o2Sync"] = 1
    if fm > 0:
        d.update(fmAmt=fm, fmSrc=fmsrc)
    return d


def lp(cut, res=0.15, env=0.35, kb=KB_HALF, order=DB24):
    """Classic single-ladder low-pass: filter 1 is opened as a 20 Hz high-pass (out of the way)."""
    return dict(f1Mode=HP, f1Cutoff=20.0, f2Cutoff=cut, f2Res=res, f2Env=env, f2Kb=kb, fOrder=order)


def hplp(hp, cut, res=0.15, env=0.35, hpres=0.0, kb=KB_HALF, order=DB24):
    return dict(f1Mode=HP, f1Cutoff=hp, f1Res=hpres, f1Kb=KB_HALF, f2Cutoff=cut, f2Res=res, f2Env=env, f2Kb=kb,
                fOrder=order)


def dual(c1, c2, r1=0.2, r2=0.2, e1=0.35, e2=0.35, routing=SERIES, order=DB24, kb=KB_HALF):
    return dict(f1Mode=LP, f1Cutoff=c1, f1Res=r1, f1Env=e1, f1Kb=kb, f2Cutoff=c2, f2Res=r2, f2Env=e2, f2Kb=kb,
                fRouting=routing, fOrder=order)


def fenv(a=0.002, d=0.4, s=0.3, r=0.3, vel=0.3, loop=False):
    out = dict(feA=a, feD=d, feS=s, feR=r, feVel=vel)
    if loop:
        out["feLoop"] = 1
    return out


def aenv(a=0.002, d=0.6, s=0.85, r=0.25, vel=0.3, loop=False):
    out = dict(aeA=a, aeD=d, aeS=s, aeR=r, aeVel=vel)
    if loop:
        out["aeLoop"] = 1
    return out


def mono(glide=0.0, legato=False, prio=PRIO_LAST, bend=2):
    d = dict(polyMode=MONO, glide=glide, notePrio=prio, bendRange=bend)
    if legato:
        d["glideLegato"] = 1
    return d


def unison(voices=4, detune=0.4, glide=0.0, prio=PRIO_LAST):
    return dict(polyMode=UNISON, uniVoices=voices, uniDetune=detune, glide=glide, notePrio=prio)


def poly(detune=0.25, glide=0.0):
    return dict(polyMode=POLY, uniDetune=detune, glide=glide)


def vib(amt=0.25, rate=5.5, wheel=True, wave=PL_TRI):
    return dict(plAmt=amt, plRate=rate, plWheel=1 if wheel else 0, plWave=wave)


def lfo(n, rate=None, wave=None, amp=None, sync=None, div=None, reset=None):
    d = {}
    p = f"l{n}"
    if rate is not None: d[p + "Rate"] = rate
    if wave is not None: d[p + "Wave"] = wave
    if amp is not None: d[p + "Amp"] = amp
    if sync: d[p + "Sync"] = 1
    if div is not None: d[p + "Div"] = DIV[div]
    if reset: d[p + "Reset"] = 1
    return d


def modosc(freq=5.0, wave=MO_TRI, pitch=0.0, pdest=BOTH, filt=0.0, fdest=BOTH, pwm=0.0, vca=0.0, kb=False,
           uni=False, reset=False):
    d = dict(moscFreq=freq, moscWave=wave, moscPitchAmt=pitch, moscPitchDest=pdest, moscFilterAmt=filt, moscFilterDest=fdest,
             moscPwmAmt=pwm, moscVcaAmt=vca)
    if kb: d["moscKbTrack"] = 1
    if uni: d["moscUnipolar"] = 1
    if reset: d["moscKeyReset"] = 1
    return d


def vca(level=0.75, pan=0.0, spread=0.3):
    return dict(vcaLevel=level, vcaPan=pan, vcaSpread=spread)


def delay(mix=0.25, fb=0.35, char=0.4, diff=0.3, divl="1/8D", divr="1/4", sync=True, tl=None, tr=None):
    d = dict(dlyOn=1, dlyMix=mix, dlyFb=fb, dlyChar=char, dlyDiff=diff, dlySync=1 if sync else 0,
             dlyDivL=DIV[divl], dlyDivR=DIV[divr])
    if tl is not None: d["dlyTimeL"] = tl
    if tr is not None: d["dlyTimeR"] = tr
    return d


def arp(div="1/16", mode=ARP_UP, octaves=1, gate=0.5, latch=True):
    return dict(arpOn=1, arpDiv=DIV[div], arpMode=mode, arpOct=octaves, arpGate=gate, arpLatch=1 if latch else 0)


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


rng = random.Random(1964)  # the year Moog showed his first modular


def jitter(v, pct, lo=None, hi=None):
    out = v * (1.0 + rng.uniform(-pct, pct))
    if lo is not None: out = max(lo, out)
    if hi is not None: out = min(hi, out)
    return round(out, 4)


# ---------------------------------------------------------------------------
# BASS
# ---------------------------------------------------------------------------
def bass_presets():
    c = "Bass"
    add(c, "Model D Low End", merge(osc(SAW, SAW, "16", "16", 0, 0.05, 0.9, 0.75, ovl=0.45), lp(240, 0.22, 0.5, KB_HALF),
                                    fenv(0.001, 0.35, 0.12, 0.15, 0.35), aenv(0.001, 0.5, 0.9, 0.12, 0.25),
                                    mono(prio=PRIO_LOW), vib(0.2)))
    add(c, "Taurus Floor", merge(osc(SAW, SAW, "32", "16", 0, 0.12, 0.9, 0.8, ovl=0.4), lp(150, 0.32, 0.45, KB_OFF),
                                 fenv(0.004, 0.8, 0.25, 0.4), aenv(0.002, 1.2, 0.85, 0.35, 0.2), mono(0.03, True, PRIO_LOW)))
    add(c, "Acid Ladder", merge(osc(SAW, SQR, "16", "16", 0, 0, 0.9, 0.0, ovl=0.55), lp(320, 0.8, 0.6, KB_HALF),
                                fenv(0.001, 0.22, 0.0, 0.12, 0.6), aenv(0.001, 0.3, 0.8, 0.08, 0.4),
                                mono(0.06, True)))
    add(c, "Sub Pulse", merge(osc(SQR, TRI, "16", "32", 0, 0, 0.75, 0.9, ovl=0.3), lp(420, 0.1, 0.2, KB_HALF),
                              fenv(0.002, 0.3, 0.3, 0.2), aenv(0.002, 0.4, 1.0, 0.1, 0.2), mono(prio=PRIO_LOW)))
    add(c, "Funk Snap", merge(osc(SQR, SAW, "16", "8", 0, 0.03, 0.85, 0.5, ovl=0.35), lp(500, 0.42, 0.55, KB_FULL),
                              fenv(0.001, 0.16, 0.0, 0.1, 0.5), aenv(0.001, 0.45, 0.55, 0.08, 0.45), mono()))
    add(c, "Sync Bite Bass", merge(osc(SAW, SAW, "16", "16", 0, 5.0, 0.3, 0.9, ovl=0.45, sync=True),
                                   lp(900, 0.25, 0.45), fenv(0.001, 0.28, 0.15, 0.15), aenv(0.001, 0.4, 0.8, 0.1),
                                   mono(), mm(1, "fenv", "o2pitch", 0.35)))
    add(c, "FM Growl", merge(osc(SAW, SQR, "16", "16", 0, 0, 0.8, 0.5, ovl=0.6, fm=0.35), lp(450, 0.35, 0.5),
                             fenv(0.001, 0.3, 0.2, 0.15), aenv(0.001, 0.5, 0.9, 0.12), mono(),
                             mm(1, "fenv", "fm", 0.4)))
    add(c, "Reese Cable", merge(osc(SAW, SAW, "16", "16", 0, 0.18, 0.85, 0.85, ovl=0.4), lp(650, 0.15, 0.2),
                                fenv(0.01, 1.0, 0.6, 0.3), aenv(0.005, 1.0, 1.0, 0.2), unison(4, 0.55),
                                lfo(2, 0.15, L_SMOOTH), mm(1, "lfo2", "cutoff", 0.08)))
    add(c, "Thumb Pluck", merge(osc(TRI, SQR, "16", "16", 0, 0, 0.9, 0.35, ovl=0.2), lp(700, 0.1, 0.4, KB_FULL),
                                fenv(0.001, 0.12, 0.0, 0.1, 0.5), aenv(0.001, 0.35, 0.0, 0.12, 0.5), poly(0.1)))
    add(c, "Rubber Glide", merge(osc(SAW, SAW, "16", "8", 0, 0.04, 0.85, 0.4, ovl=0.5), lp(380, 0.5, 0.45),
                                 fenv(0.002, 0.3, 0.25, 0.2), aenv(0.002, 0.5, 0.9, 0.15), mono(0.12, True)))
    add(c, "Deep Sine Sub", merge(osc(TRI, TRI, "16", "32", 0, 0, 0.8, 0.7, ovl=0.1), lp(180, 0.0, 0.1, KB_FULL),
                                  fenv(), aenv(0.004, 0.5, 1.0, 0.15, 0.1), mono(prio=PRIO_LOW)))
    add(c, "Overdriven Mixer", merge(osc(SAW, SQR, "16", "16", 0, -0.04, 1.0, 1.0, noise=0.08, ovl=0.95),
                                     lp(300, 0.3, 0.45), fenv(0.001, 0.4, 0.2, 0.2), aenv(0.001, 0.5, 0.9, 0.12), mono()))
    add(c, "Ring Mod Bass", merge(osc(SAW, SQR, "16", "16", 0, 7.0, 0.6, 0.0, ring=0.7, ovl=0.35), lp(800, 0.2, 0.35),
                                  fenv(0.001, 0.25, 0.2, 0.1), aenv(0.001, 0.35, 0.7, 0.1), mono()))
    add(c, "Wobble Ladder", merge(osc(SAW, SAW, "16", "16", 0, 0.08, 0.9, 0.8, ovl=0.5), lp(250, 0.55, 0.15),
                                  fenv(), aenv(0.002, 0.5, 1.0, 0.1), mono(),
                                  lfo(1, sync=True, div="1/8T", wave=L_SINE), mm(1, "lfo1", "cutoff", 0.32)))
    add(c, "Hollow Square", merge(osc(SQR, SQR, "16", "8", 0, 0.03, 0.85, 0.3, ovl=0.25), lp(600, 0.2, 0.3),
                                  fenv(0.002, 0.25, 0.3, 0.12), aenv(0.002, 0.4, 0.9, 0.1), mono(prio=PRIO_LOW)))
    add(c, "Stereo Split Bass", merge(osc(SAW, SAW, "16", "16", 0, 0.1, 0.85, 0.8, ovl=0.35),
                                      dual(260, 520, 0.3, 0.2, 0.5, 0.4, STEREO), fenv(0.001, 0.32, 0.15, 0.15),
                                      aenv(0.001, 0.5, 0.9, 0.12), mono(), vca(spread=0.0)))
    add(c, "Moog Pedal Drone", merge(osc(SAW, SAW, "32", "32", 0, 0.08, 0.9, 0.9, ovl=0.5), lp(120, 0.45, 0.3, KB_OFF),
                                     fenv(0.05, 2.0, 0.5, 1.0), aenv(0.02, 1.0, 1.0, 1.2), mono(0.08, True, PRIO_LOW),
                                     lfo(2, 0.08, L_SMOOTH), mm(1, "lfo2", "f2cut", 0.15)))
    add(c, "Pulse Width Bass", merge(osc(NARROW, SAW, "16", "16", 0, 0.02, 0.9, 0.4, ovl=0.3), lp(500, 0.2, 0.4),
                                     modosc(0.4, MO_TRI, pwm=0.6), fenv(0.002, 0.3, 0.3, 0.15),
                                     aenv(0.002, 0.5, 0.9, 0.12), mono()))
    add(c, "Velocity Slap", merge(osc(SAW, SQR, "16", "16", 0, 0.0, 0.9, 0.5, ovl=0.4), lp(260, 0.35, 0.65, KB_FULL),
                                  fenv(0.001, 0.18, 0.05, 0.12, 0.9), aenv(0.001, 0.4, 0.75, 0.1, 0.7), mono()))
    add(c, "Wheel Filter Bass", merge(osc(SAW, SAW, "16", "16", 0, 0.06, 0.9, 0.8, ovl=0.4), lp(200, 0.4, 0.4),
                                      fenv(0.002, 0.4, 0.2, 0.2), aenv(0.002, 0.5, 0.9, 0.12), mono(),
                                      mm(1, "wheel", "cutoff", 0.45), mm(2, "at", "f2res", 0.3)))

    # Systematic variations on the Model D-style bass: waveform x octave x filter character
    waves = [("Saw", SAW, SAW), ("Square", SQR, SQR), ("Saw Square", SAW, SQR), ("Tri Saw", TRI, SAW)]
    chars = [("Round", 220, 0.15, 0.35, 0.45), ("Punch", 350, 0.3, 0.6, 0.22), ("Squelch", 280, 0.68, 0.55, 0.3),
             ("Dark", 140, 0.2, 0.3, 0.6)]
    for wname, w1, w2 in waves:
        for cname, cut, res, env, dec in chars:
            add(c, f"{cname} {wname}",
                merge(osc(w1, w2, "16", "16", 0, jitter(0.05, 0.6), 0.9, jitter(0.7, 0.2, 0, 1), ovl=jitter(0.4, 0.3, 0, 1)),
                      lp(jitter(cut, 0.15), res, env, KB_HALF), fenv(0.001, jitter(dec, 0.2), 0.1, 0.15),
                      aenv(0.001, 0.5, 0.9, 0.12), mono(prio=PRIO_LOW)))


# ---------------------------------------------------------------------------
# LEAD
# ---------------------------------------------------------------------------
def lead_presets():
    c = "Lead"
    add(c, "Lucky Lead", merge(osc(SQR, SAW, "8", "8", 0, 0.06, 0.8, 0.6, ovl=0.35), lp(1800, 0.28, 0.35),
                               fenv(0.02, 0.6, 0.55, 0.4), aenv(0.01, 0.5, 0.95, 0.35), mono(0.12, True),
                               vib(0.3, 5.2)), glob=delay(0.15, 0.3))
    add(c, "Sync Scream", merge(osc(SAW, SAW, "8", "8", 0, 4.0, 0.2, 0.9, ovl=0.45, sync=True), lp(4200, 0.18, 0.25),
                                fenv(0.002, 0.5, 0.2, 0.3), aenv(0.002, 0.5, 0.9, 0.3), mono(0.05),
                                mm(1, "fenv", "o2pitch", 0.45), mm(2, "wheel", "o2pitch", 0.4), vib(0.15)))
    add(c, "Theremin Ghost", merge(osc(TRI, TRI, "8", "8", 0, 0.02, 0.8, 0.3, ovl=0.05), lp(5000, 0.0, 0.0, KB_FULL),
                                   fenv(), aenv(0.08, 1.0, 1.0, 0.6, 0.0), mono(0.35), vib(0.28, 5.8, wheel=False, wave=PL_SINE)),
        glob=delay(0.3, 0.45, 0.6, 0.7))
    add(c, "Fifth Lead", merge(osc(SAW, SAW, "8", "8", 0, 7.0, 0.85, 0.7, ovl=0.35), lp(2200, 0.25, 0.4),
                               fenv(0.005, 0.5, 0.4, 0.3), aenv(0.005, 0.5, 0.9, 0.3), mono(0.04), vib(0.25)))
    add(c, "Unison Wall", merge(osc(SAW, SAW, "8", "8", 0, 0.08, 0.85, 0.8, ovl=0.4), lp(3000, 0.2, 0.35),
                                fenv(0.005, 0.6, 0.5, 0.3), aenv(0.005, 0.5, 0.95, 0.3), unison(6, 0.5, 0.03), vib(0.2)))
    add(c, "Soft Square Solo", merge(osc(SQR, SQR, "8", "4", 0, 0.03, 0.85, 0.25, ovl=0.15), lp(1500, 0.1, 0.3),
                                     fenv(0.03, 0.8, 0.6, 0.4), aenv(0.03, 0.5, 0.9, 0.4), mono(0.1, True), vib(0.3)),
        glob=delay(0.2, 0.35, 0.5, 0.5))
    add(c, "Brass Lead", merge(osc(SAW, SAW, "8", "8", 0, 0.1, 0.85, 0.8, ovl=0.45), lp(900, 0.15, 0.55),
                               fenv(0.06, 0.6, 0.45, 0.3), aenv(0.03, 0.5, 0.9, 0.3), mono(0.03), vib(0.25)))
    add(c, "Whistle Res", merge(osc(TRI, TRI, "8", "8", 0, 0.0, 0.15, 0.0, noise=0.05, ovl=0.05),
                                lp(1100, 0.93, 0.1, KB_FULL), fenv(), aenv(0.03, 0.5, 1.0, 0.3), mono(0.08), vib(0.25, 6.0)))
    add(c, "Chewy Lead", merge(osc(SAW, SQR, "8", "8", 0, 0.05, 0.8, 0.7, ovl=0.5), lp(700, 0.6, 0.5),
                               fenv(0.005, 0.35, 0.3, 0.2), aenv(0.005, 0.5, 0.9, 0.25), mono(0.04),
                               mm(1, "at", "cutoff", 0.3), vib(0.2)))
    add(c, "Hard Sync Sweep", merge(osc(SQR, SAW, "8", "8", 0, 0.0, 0.0, 0.9, ovl=0.4, sync=True), lp(3500, 0.15, 0.2),
                                    fenv(), aenv(0.005, 0.5, 0.9, 0.3), mono(0.04),
                                    lfo(1, 0.3, L_TRI), mm(1, "lfo1", "o2pitch", 0.35), vib(0.15)))
    add(c, "Ring Lead", merge(osc(SAW, SAW, "8", "8", 0, 5.03, 0.5, 0.0, ring=0.8, ovl=0.3), lp(2500, 0.2, 0.3),
                              fenv(0.005, 0.5, 0.4, 0.3), aenv(0.005, 0.5, 0.9, 0.3), mono(0.05), vib(0.2)),
        glob=delay(0.2, 0.4))
    add(c, "Pulse Lead PWM", merge(osc(NARROW, SQR, "8", "8", 0, 0.04, 0.8, 0.5, ovl=0.3), lp(2000, 0.25, 0.35),
                                   modosc(5.5, MO_TRI, pwm=0.5), fenv(0.005, 0.5, 0.4, 0.3), aenv(0.005, 0.5, 0.9, 0.3),
                                   mono(0.06), vib(0.2)))
    add(c, "Tri Flute", merge(osc(TRI, TRI, "8", "4", 0, 0.0, 0.85, 0.15, noise=0.06, ovl=0.1), lp(2600, 0.1, 0.25),
                              fenv(0.06, 0.4, 0.6, 0.3), aenv(0.06, 0.5, 0.9, 0.3), mono(0.05), vib(0.35, 5.0, wheel=False)),
        glob=delay(0.22, 0.4, 0.6, 0.8))
    add(c, "Detuned Duo", merge(osc(SAW, SAW, "8", "8", 0, 0.22, 0.85, 0.85, ovl=0.35), lp(2400, 0.2, 0.3),
                                fenv(0.005, 0.6, 0.5, 0.4), aenv(0.005, 0.5, 0.9, 0.4), mono(0.07), vib(0.2)))
    add(c, "Octave Up Solo", merge(osc(SAW, SQR, "8", "4", 0, 0.02, 0.8, 0.5, ovl=0.35), lp(2800, 0.2, 0.3),
                                   fenv(0.005, 0.5, 0.5, 0.3), aenv(0.005, 0.5, 0.9, 0.3), mono(0.05, True), vib(0.3)))
    add(c, "Screaming Ladder", merge(osc(SAW, SAW, "8", "8", 0, 0.07, 0.9, 0.8, ovl=0.8), lp(1200, 0.82, 0.4),
                                     fenv(0.005, 0.5, 0.4, 0.3), aenv(0.005, 0.5, 0.9, 0.3), mono(0.04), vib(0.25)))
    add(c, "FM Lead Glass", merge(osc(TRI, SQR, "8", "8", 0, 0.0, 0.8, 0.3, ovl=0.2, fm=0.25, fmsrc=FM_MOD1),
                                  modosc(523.0, MO_SINE, kb=True), lp(4000, 0.1, 0.2), fenv(0.005, 0.5, 0.4, 0.3),
                                  aenv(0.005, 0.5, 0.9, 0.3), mono(0.04), vib(0.2), mm(1, "fenv", "fm", 0.3)),
        glob=delay(0.2, 0.4, 0.3))
    add(c, "Portamento King", merge(osc(SAW, SAW, "8", "16", 0, 0.05, 0.85, 0.6, ovl=0.4), lp(1600, 0.3, 0.35),
                                    fenv(0.005, 0.6, 0.5, 0.4), aenv(0.005, 0.5, 0.95, 0.4), mono(0.3), vib(0.3)))

    # Wheel / aftertouch expressive variants
    for i, (name, w1, w2, cut, res) in enumerate([
            ("Expressive Saw", SAW, SAW, 1400, 0.25), ("Expressive Square", SQR, SQR, 1200, 0.3),
            ("Expressive Pulse", PULSE, SAW, 1600, 0.2), ("Expressive Tri", TRI, SAW, 2000, 0.15),
            ("Expressive Sync", SAW, SAW, 2600, 0.2), ("Expressive Ring", SQR, SAW, 1800, 0.25)]):
        a = merge(osc(w1, w2, "8", "8", 0, 0.05 if i != 4 else 3.0, 0.8, 0.7, ring=0.4 if i == 5 else 0.0,
                      ovl=0.35, sync=(i == 4)),
                  lp(cut, res, 0.3), fenv(0.01, 0.6, 0.5, 0.3), aenv(0.01, 0.5, 0.9, 0.35), mono(0.06), vib(0.3),
                  mm(1, "at", "cutoff", 0.35), mm(2, "vel", "f2cut", 0.15))
        add(c, name, a)

    # Glide family
    for glide_t, label in [(0.05, "Quick"), (0.15, "Smooth"), (0.4, "Lazy")]:
        for w1, w2, wl in [(SAW, SAW, "Saw"), (SQR, SAW, "Square")]:
            add(c, f"{label} Glide {wl}", merge(osc(w1, w2, "8", "8", 0, 0.06, 0.85, 0.7, ovl=0.35),
                                                 lp(jitter(1700, 0.2), jitter(0.3, 0.3), 0.35),
                                                 fenv(0.005, 0.6, 0.5, 0.3), aenv(0.005, 0.5, 0.9, 0.3),
                                                 mono(glide_t, True), vib(0.25)))


# ---------------------------------------------------------------------------
# PAD
# ---------------------------------------------------------------------------
def pad_presets():
    c = "Pad"
    pad_delay = delay(0.3, 0.45, 0.55, 0.75, "1/4D", "1/2")
    add(c, "Vox Humana", merge(osc(PULSE, SAW, "8", "8", 0, 0.1, 0.75, 0.6, ovl=0.2), lp(2100, 0.12, 0.2),
                               modosc(0.35, MO_TRI, pwm=0.55), fenv(0.6, 2.0, 0.6, 1.4), aenv(0.45, 1.5, 0.9, 1.4),
                               poly(0.4), vca(0.8, spread=0.7), vib(0.15)), glob=pad_delay)
    add(c, "Polymoog Choir", merge(osc(NARROW, PULSE, "8", "4", 0, 0.08, 0.7, 0.45, ovl=0.15), lp(1700, 0.1, 0.25),
                                   modosc(0.5, MO_SINE, pwm=0.6), fenv(0.5, 2.5, 0.5, 1.5), aenv(0.6, 1.0, 0.95, 1.6),
                                   poly(0.45), vca(0.8, spread=0.8)), glob=pad_delay)
    add(c, "Memory Pad", merge(osc(SAW, SAW, "8", "8", 0, 0.12, 0.8, 0.8, ovl=0.25), lp(1400, 0.2, 0.3),
                               fenv(0.8, 2.0, 0.5, 1.5), aenv(0.7, 1.0, 0.9, 1.6), poly(0.35), vca(spread=0.6),
                               lfo(2, 0.12, L_SMOOTH), mm(1, "lfo2", "cutoff", 0.06)), glob=pad_delay)
    add(c, "Slow Sweep", merge(osc(SAW, SQR, "8", "8", 0, 0.08, 0.8, 0.7, ovl=0.25), lp(500, 0.45, 0.1),
                               fenv(), aenv(1.2, 1.0, 1.0, 2.0), poly(0.3), vca(spread=0.6),
                               lfo(2, 0.06, L_TRI), mm(1, "lfo2", "f2cut", 0.35)), glob=pad_delay)
    add(c, "Stereo Ladder Pad", merge(osc(SAW, SAW, "8", "8", 0, 0.1, 0.8, 0.8, ovl=0.25),
                                      dual(900, 1300, 0.35, 0.3, 0.3, 0.25, STEREO), fenv(0.9, 2.5, 0.5, 1.5),
                                      aenv(0.8, 1.0, 0.95, 1.8), poly(0.3), vca(spread=0.2),
                                      lfo(1, 0.15, L_SINE), lfo(2, 0.11, L_SINE),
                                      mm(1, "lfo1", "f1cut", 0.12), mm(2, "lfo2", "f2cut", 0.12)), glob=pad_delay)
    add(c, "Glass Choir", merge(osc(TRI, SQR, "8", "4", 0, 0.05, 0.8, 0.4, ovl=0.1), hplp(250, 3500, 0.15, 0.1),
                                fenv(0.6, 2.0, 0.6, 1.5), aenv(0.6, 1.2, 0.9, 2.0), poly(0.4), vca(spread=0.8),
                                vib(0.12, 4.8, wheel=False)), glob=delay(0.35, 0.5, 0.3, 0.9, "1/4", "1/4D"))
    add(c, "Warm Blanket", merge(osc(SAW, TRI, "8", "16", 0, 0.05, 0.8, 0.7, ovl=0.3), lp(800, 0.1, 0.15),
                                 fenv(0.5, 2.0, 0.6, 1.5), aenv(0.5, 1.0, 0.9, 1.5), poly(0.3), vca(spread=0.5)))
    add(c, "Ring Shimmer", merge(osc(SAW, TRI, "8", "4", 0, 7.02, 0.6, 0.3, ring=0.4, ovl=0.15), hplp(150, 2800, 0.2, 0.2),
                                 fenv(1.0, 2.0, 0.6, 2.0), aenv(1.0, 1.0, 0.9, 2.2), poly(0.4), vca(spread=0.8)),
        glob=pad_delay)
    add(c, "Dark Matter", merge(osc(SAW, SAW, "16", "8", 0, 0.15, 0.85, 0.7, noise=0.05, ovl=0.35), lp(300, 0.55, 0.2),
                                fenv(1.5, 3.0, 0.5, 2.0), aenv(1.5, 1.0, 1.0, 2.5), poly(0.4), vca(spread=0.6),
                                lfo(2, 0.05, L_SMOOTH), mm(1, "lfo2", "cutoff", 0.25)), glob=pad_delay)
    add(c, "Breathing Pad", merge(osc(PULSE, SAW, "8", "8", 0, 0.09, 0.8, 0.7, ovl=0.2), lp(1100, 0.25, 0.25),
                                  fenv(0.5, 2.0, 0.6, 1.5), aenv(0.6, 1.0, 0.9, 1.5), poly(0.35), vca(spread=0.6),
                                  lfo(1, sync=True, div="1/1", wave=L_SINE), mm(1, "lfo1", "cutoff", 0.15),
                                  mm(2, "lfo1", "vca", 0.12)), glob=pad_delay)
    add(c, "Analog Strings Pad", merge(osc(SAW, SAW, "8", "4", 0, 0.08, 0.8, 0.5, ovl=0.2), hplp(180, 2600, 0.1, 0.2),
                                       fenv(0.4, 1.5, 0.7, 1.2), aenv(0.4, 1.0, 0.95, 1.2), poly(0.5), vca(spread=0.7),
                                       lfo(2, 6.0, L_SINE, 0.3), mm(1, "lfo2", "pitch", 0.003)), glob=pad_delay)
    add(c, "Evolving Mod Osc", merge(osc(SAW, PULSE, "8", "8", 0, 0.05, 0.75, 0.65, mod=0.15, ovl=0.25),
                                     lp(1000, 0.35, 0.2), modosc(0.07, MO_SH, filt=0.12, pwm=0.4),
                                     fenv(0.8, 2.0, 0.6, 1.8), aenv(0.8, 1.0, 1.0, 2.0), poly(0.4), vca(spread=0.7)),
        glob=pad_delay)
    add(c, "Looping Pulse Pad", merge(osc(SAW, SAW, "8", "8", 0, 0.1, 0.8, 0.8, ovl=0.25), lp(700, 0.35, 0.4),
                                      fenv(0.3, 0.6, 0.2, 1.0, loop=True), aenv(0.5, 1.0, 0.9, 1.5), poly(0.35),
                                      vca(spread=0.6)), glob=pad_delay)
    add(c, "Frozen Lake", merge(osc(TRI, TRI, "8", "4", 0, 0.03, 0.8, 0.5, noise=0.12, ovl=0.1), hplp(400, 4000, 0.2, 0.1),
                                fenv(1.5, 3.0, 0.6, 2.5), aenv(1.5, 1.0, 1.0, 3.0), poly(0.45), vca(spread=0.9)),
        glob=delay(0.45, 0.6, 0.35, 1.0, "1/2", "1/4D"))

    # Variation grid: wave pair x motion
    waves = [("Saw", SAW, SAW), ("Pulse", PULSE, SAW), ("Square", SQR, SQR), ("Tri", TRI, SAW)]
    motions = [("Drift", 0.08, L_SMOOTH, "cutoff", 0.1), ("Swell", 0.0, L_SINE, "vca", 0.0),
               ("Tide", 0.18, L_TRI, "f2cut", 0.18), ("Glow", 0.3, L_SINE, "o2wave", 0.2)]
    for wname, w1, w2 in waves:
        for mname, rate, wave, dst, amt in motions:
            a = merge(osc(w1, w2, "8", "8", 0, jitter(0.1, 0.4), 0.8, 0.7, ovl=0.2),
                      lp(jitter(1300, 0.35), jitter(0.2, 0.5, 0, 0.6), 0.25),
                      fenv(jitter(0.7, 0.4), 2.0, 0.6, 1.5),
                      aenv(jitter(0.8 if mname != "Swell" else 1.8, 0.3), 1.0, 0.95, jitter(1.6, 0.3)),
                      poly(jitter(0.35, 0.3, 0, 1)), vca(spread=0.7))
            if amt > 0:
                a = merge(a, lfo(2, rate, wave), mm(1, "lfo2", dst, amt))
            add(c, f"{wname} {mname}", a, glob=pad_delay)


# ---------------------------------------------------------------------------
# KEYS
# ---------------------------------------------------------------------------
def keys_presets():
    c = "Keys"
    add(c, "Poly Moog Keys", merge(osc(SAW, SAW, "8", "8", 0, 0.08, 0.85, 0.7, ovl=0.3), lp(900, 0.15, 0.45, KB_FULL),
                                   fenv(0.002, 0.6, 0.25, 0.4, 0.5), aenv(0.002, 1.2, 0.5, 0.4, 0.5), poly(0.25)))
    add(c, "Organ Drawbars", merge(osc(SQR, SQR, "8", "4", 0, 0.0, 0.8, 0.55, ovl=0.15), lp(3500, 0.0, 0.0, KB_FULL),
                                   fenv(), aenv(0.003, 0.5, 1.0, 0.06, 0.0), poly(0.05),
                                   modosc(6.5, MO_SINE, vca=0.12)))
    add(c, "Clav Funk", merge(osc(NARROW, PULSE, "8", "8", 0, 0.02, 0.85, 0.4, ovl=0.3), hplp(300, 1800, 0.35, 0.5, kb=KB_FULL),
                              fenv(0.001, 0.15, 0.1, 0.1, 0.6), aenv(0.001, 0.5, 0.3, 0.08, 0.6), poly(0.1)))
    add(c, "Bell Piano", merge(osc(TRI, SQR, "8", "8", 0, 0.0, 0.8, 0.3, ovl=0.1, fm=0.2, fmsrc=FM_MOD1),
                               modosc(1046.5, MO_SINE, kb=True), lp(3000, 0.05, 0.3, KB_FULL),
                               fenv(0.001, 1.0, 0.2, 0.6), aenv(0.001, 2.5, 0.0, 0.8, 0.5), poly(0.15),
                               mm(1, "fenv", "fm", 0.4)), glob=delay(0.15, 0.3, 0.5, 0.6))
    add(c, "Mellow EP", merge(osc(TRI, TRI, "8", "4", 0, 0.03, 0.85, 0.25, ovl=0.15), lp(1800, 0.05, 0.3, KB_FULL),
                              modosc(4.5, MO_SINE, vca=0.15), fenv(0.001, 1.0, 0.2, 0.5, 0.6),
                              aenv(0.001, 3.0, 0.0, 0.6, 0.6), poly(0.15)))
    add(c, "Harpsi Pulse", merge(osc(NARROW, SAW, "8", "4", 0, 0.03, 0.8, 0.4, ovl=0.15), hplp(500, 4000, 0.1, 0.3),
                                 fenv(0.001, 0.3, 0.1, 0.4), aenv(0.001, 1.5, 0.0, 0.5, 0.3), poly(0.1)))
    add(c, "Funky Poly Stab", merge(osc(SAW, SQR, "8", "8", 0, 0.05, 0.85, 0.6, ovl=0.35), lp(700, 0.4, 0.55, KB_FULL),
                                    fenv(0.001, 0.2, 0.05, 0.15, 0.6), aenv(0.001, 0.3, 0.2, 0.15, 0.5), poly(0.2)))
    add(c, "Wurly Tremolo", merge(osc(TRI, SQR, "8", "8", 0, 0.0, 0.8, 0.2, ovl=0.3), lp(1500, 0.1, 0.35, KB_FULL),
                                  modosc(5.5, MO_SINE, vca=0.35), fenv(0.001, 0.8, 0.2, 0.4, 0.6),
                                  aenv(0.001, 2.0, 0.1, 0.5, 0.6), poly(0.1)))
    add(c, "Toy Box", merge(osc(SQR, TRI, "4", "2", 0, 0.0, 0.7, 0.5, ovl=0.1), hplp(600, 5000, 0.1, 0.1),
                            fenv(), aenv(0.001, 0.8, 0.0, 0.5, 0.3), poly(0.1)), glob=delay(0.25, 0.4, 0.4))
    add(c, "Combo Organ", merge(osc(SQR, SAW, "8", "4", 0, 0.0, 0.7, 0.5, ovl=0.45), lp(2500, 0.15, 0.0, KB_FULL),
                                fenv(), aenv(0.004, 0.5, 1.0, 0.05, 0.0), poly(0.05), vib(0.15, 6.5, wheel=False)))
    for n, (cut, dec, res) in enumerate([(600, 0.4, 0.2), (1000, 0.7, 0.15), (1500, 0.5, 0.3), (400, 1.0, 0.35)]):
        for w1, w2, wl in [(SAW, SAW, "Saw"), (PULSE, SQR, "Pulse"), (SQR, TRI, "Hollow")]:
            add(c, f"{wl} Keys {n + 1}", merge(osc(w1, w2, "8", "8", 0, jitter(0.06, 0.5), 0.85, 0.6, ovl=0.25),
                                               lp(jitter(cut, 0.1), res, 0.45, KB_FULL),
                                               fenv(0.001, dec, 0.2, 0.3, 0.5), aenv(0.001, dec * 2.5, 0.35, 0.35, 0.5),
                                               poly(0.25)))


# ---------------------------------------------------------------------------
# BRASS
# ---------------------------------------------------------------------------
def brass_presets():
    c = "Brass"
    add(c, "Memory Brass", merge(osc(SAW, SAW, "8", "8", 0, 0.1, 0.85, 0.8, ovl=0.35), lp(700, 0.1, 0.55),
                                 fenv(0.06, 0.5, 0.45, 0.3, 0.4), aenv(0.03, 0.5, 0.9, 0.3, 0.3), poly(0.3), vib(0.2)))
    add(c, "Brass Stab", merge(osc(SAW, SAW, "8", "8", 0, 0.08, 0.85, 0.8, ovl=0.4), lp(600, 0.15, 0.6),
                               fenv(0.02, 0.25, 0.2, 0.15, 0.5), aenv(0.01, 0.3, 0.6, 0.15, 0.4), poly(0.3)))
    add(c, "Soft Horns", merge(osc(SAW, PULSE, "8", "8", 0, 0.06, 0.8, 0.6, ovl=0.2), lp(600, 0.05, 0.4),
                               fenv(0.15, 0.8, 0.5, 0.5), aenv(0.1, 0.8, 0.9, 0.5), poly(0.3), vib(0.2)))
    add(c, "Sync Brass", merge(osc(SAW, SAW, "8", "8", 0, 2.0, 0.6, 0.8, ovl=0.35, sync=True), lp(1000, 0.1, 0.45),
                               fenv(0.05, 0.5, 0.4, 0.3), aenv(0.03, 0.5, 0.9, 0.3), poly(0.25),
                               mm(1, "fenv", "o2pitch", 0.2)))
    add(c, "Fanfare", merge(osc(SAW, SAW, "8", "4", 0, 0.05, 0.85, 0.5, ovl=0.4), lp(900, 0.15, 0.5),
                            fenv(0.04, 0.4, 0.5, 0.3), aenv(0.02, 0.5, 0.9, 0.3), poly(0.3)),
        glob=delay(0.18, 0.3, 0.5, 0.7))
    add(c, "Low Tuba", merge(osc(SAW, SQR, "16", "16", 0, 0.05, 0.85, 0.5, ovl=0.4), lp(350, 0.1, 0.4),
                             fenv(0.08, 0.5, 0.5, 0.2), aenv(0.05, 0.5, 0.9, 0.2), mono(0.03), vib(0.2)))
    add(c, "Unison Brass", merge(osc(SAW, SAW, "8", "8", 0, 0.1, 0.85, 0.8, ovl=0.4), lp(800, 0.1, 0.55),
                                 fenv(0.06, 0.5, 0.45, 0.3), aenv(0.03, 0.5, 0.9, 0.3), unison(5, 0.45), vib(0.2)))
    add(c, "Brass Swell", merge(osc(SAW, SAW, "8", "8", 0, 0.12, 0.85, 0.8, ovl=0.35), lp(500, 0.15, 0.55),
                                fenv(0.8, 1.0, 0.6, 0.6), aenv(0.4, 0.5, 1.0, 0.6), poly(0.3), vib(0.25)))
    for i in range(16):
        atk = [0.02, 0.05, 0.1, 0.2][i % 4]
        cut = [450, 650, 900, 1200][i // 4]
        add(c, f"Section {i + 1:02d}", merge(osc(SAW, rng.choice([SAW, PULSE]), "8", rng.choice(["8", "4"]), 0,
                                                 jitter(0.08, 0.4), 0.85, jitter(0.7, 0.15, 0, 1), ovl=jitter(0.35, 0.3)),
                                             lp(jitter(cut, 0.1), jitter(0.12, 0.5), jitter(0.5, 0.15)),
                                             fenv(atk, jitter(0.5, 0.3), 0.45, 0.3), aenv(atk * 0.6, 0.5, 0.9, 0.3),
                                             poly(jitter(0.3, 0.3)), vib(0.2)))


# ---------------------------------------------------------------------------
# STRINGS
# ---------------------------------------------------------------------------
def string_presets():
    c = "Strings"
    ens = merge(lfo(2, 5.8, L_SINE, 1.0), mm(1, "lfo2", "o1pitch", 0.004), lfo(1, 0.6, L_TRI), mm(2, "lfo1", "o2pitch", 0.003))
    add(c, "String Machine", merge(osc(SAW, SAW, "8", "4", 0, 0.05, 0.8, 0.5, ovl=0.15), hplp(220, 3000, 0.05, 0.1),
                                   fenv(0.3, 1.0, 0.8, 1.0), aenv(0.25, 1.0, 1.0, 1.0), poly(0.55), vca(spread=0.8), ens),
        glob=delay(0.3, 0.4, 0.5, 0.9, "1/4", "1/4D"))
    add(c, "Ensemble Cellos", merge(osc(SAW, SAW, "16", "8", 0, 0.08, 0.8, 0.7, ovl=0.2), lp(1200, 0.1, 0.2),
                                    fenv(0.3, 1.0, 0.7, 0.8), aenv(0.25, 1.0, 1.0, 0.8), poly(0.5), vca(spread=0.6), ens))
    add(c, "Violin Section", merge(osc(SAW, SAW, "8", "8", 0, 0.12, 0.8, 0.8, ovl=0.15), hplp(350, 3800, 0.1, 0.15),
                                   fenv(0.2, 1.0, 0.8, 0.8), aenv(0.15, 1.0, 1.0, 0.7), poly(0.6), vca(spread=0.7), ens,
                                   vib(0.2, 5.5)))
    add(c, "Pizzicato", merge(osc(SAW, SAW, "8", "8", 0, 0.08, 0.8, 0.7, ovl=0.15), hplp(250, 1500, 0.1, 0.5, kb=KB_FULL),
                              fenv(0.001, 0.15, 0.0, 0.15), aenv(0.001, 0.35, 0.0, 0.3, 0.5), poly(0.3), vca(spread=0.7)),
        glob=delay(0.2, 0.3, 0.6, 0.9))
    add(c, "Slow Strings", merge(osc(SAW, SAW, "8", "8", 0, 0.1, 0.8, 0.8, ovl=0.15), lp(2200, 0.1, 0.1),
                                 fenv(1.0, 2.0, 0.8, 1.5), aenv(1.0, 1.0, 1.0, 1.8), poly(0.55), vca(spread=0.8), ens),
        glob=delay(0.35, 0.45, 0.5, 1.0, "1/2", "1/4D"))
    add(c, "Solina Mood", merge(osc(SAW, SQR, "8", "4", 0, 0.03, 0.75, 0.45, ovl=0.1), hplp(280, 2400, 0.05, 0.05),
                                fenv(0.4, 1.0, 0.9, 1.0), aenv(0.4, 1.0, 1.0, 1.2), poly(0.6), vca(spread=0.9), ens),
        glob=delay(0.3, 0.4, 0.6, 1.0, "1/4", "1/2"))
    for i, (cut, atk, rel) in enumerate([(1500, 0.2, 0.8), (2500, 0.4, 1.2), (900, 0.6, 1.5), (3500, 0.1, 0.6),
                                          (1800, 0.8, 2.0), (1200, 0.3, 1.0), (2800, 0.5, 1.4), (700, 1.0, 2.0),
                                          (2000, 0.15, 0.7), (1600, 0.45, 1.1), (3000, 0.7, 1.6), (1100, 0.25, 0.9)]):
        add(c, f"Strings {i + 1:02d}", merge(osc(SAW, SAW, "8", rng.choice(["8", "4", "16"]), 0, jitter(0.08, 0.5),
                                                 0.8, jitter(0.65, 0.2, 0, 1), ovl=0.15),
                                             hplp(jitter(200, 0.3), cut, jitter(0.1, 0.5), 0.15),
                                             fenv(atk, 1.5, 0.8, rel), aenv(atk, 1.0, 1.0, rel),
                                             poly(jitter(0.5, 0.2, 0, 1)), vca(spread=0.8), ens),
            glob=delay(0.25, 0.4, 0.5, 0.9, "1/4", "1/4D"))


# ---------------------------------------------------------------------------
# PLUCK
# ---------------------------------------------------------------------------
def pluck_presets():
    c = "Pluck"
    add(c, "Ladder Pluck", merge(osc(SAW, SAW, "8", "8", 0, 0.07, 0.85, 0.7, ovl=0.3), lp(400, 0.35, 0.6, KB_FULL),
                                 fenv(0.001, 0.18, 0.0, 0.2, 0.5), aenv(0.001, 0.5, 0.0, 0.4, 0.5), poly(0.2)),
        glob=delay(0.25, 0.4, 0.5, 0.4))
    add(c, "Resonant Drop", merge(osc(SAW, SQR, "8", "8", 0, 0.05, 0.85, 0.6, ovl=0.35), lp(300, 0.75, 0.55, KB_FULL),
                                  fenv(0.001, 0.25, 0.0, 0.2), aenv(0.001, 0.6, 0.0, 0.4), poly(0.2)),
        glob=delay(0.3, 0.45))
    add(c, "Harp Glass", merge(osc(TRI, SQR, "8", "4", 0, 0.0, 0.8, 0.3, ovl=0.1), lp(1500, 0.2, 0.4, KB_FULL),
                               fenv(0.001, 0.4, 0.0, 0.5), aenv(0.001, 1.5, 0.0, 1.0), poly(0.15)),
        glob=delay(0.3, 0.45, 0.5, 0.8))
    add(c, "Kalimba", merge(osc(TRI, TRI, "8", "2", 0, 0.0, 0.85, 0.2, ovl=0.1, fm=0.15, fmsrc=FM_1TO2),
                            lp(2500, 0.1, 0.3, KB_FULL), fenv(0.001, 0.2, 0.0, 0.2), aenv(0.001, 0.7, 0.0, 0.5, 0.5),
                            poly(0.1)), glob=delay(0.2, 0.3, 0.5, 0.6))
    add(c, "Muted Guitar", merge(osc(PULSE, SAW, "8", "8", 0, 0.03, 0.85, 0.5, ovl=0.4), lp(800, 0.3, 0.4, KB_FULL),
                                 fenv(0.001, 0.1, 0.0, 0.1, 0.6), aenv(0.001, 0.2, 0.0, 0.15, 0.6), poly(0.15)))
    add(c, "Koto Sync", merge(osc(SAW, SAW, "8", "8", 0, 3.0, 0.4, 0.8, ovl=0.25, sync=True), lp(2000, 0.2, 0.35, KB_FULL),
                              fenv(0.001, 0.3, 0.0, 0.3), aenv(0.001, 0.9, 0.0, 0.5), poly(0.15),
                              mm(1, "fenv", "o2pitch", 0.25)), glob=delay(0.25, 0.35, 0.5, 0.7))
    add(c, "Mallet", merge(osc(TRI, SQR, "8", "8", 0, 0.0, 0.85, 0.2, ring=0.15, ovl=0.1), lp(2200, 0.1, 0.3, KB_FULL),
                           fenv(0.001, 0.2, 0.0, 0.3), aenv(0.001, 0.8, 0.0, 0.6, 0.5), poly(0.1)))
    add(c, "Pinged Filter", merge(osc(TRI, TRI, "8", "8", 0, 0.0, 0.0, 0.0, noise=0.03, ovl=0.05),
                                  lp(900, 0.96, 0.25, KB_FULL), fenv(0.001, 0.3, 0.0, 0.3), aenv(0.001, 1.5, 0.0, 1.0),
                                  poly(0.1)), glob=delay(0.25, 0.4, 0.4, 0.6))
    for i in range(18):
        dec = [0.12, 0.2, 0.35][i % 3]
        res = [0.15, 0.4, 0.65][(i // 3) % 3]
        w = [(SAW, SAW), (SQR, SAW)][i // 9]
        add(c, f"Pluck {i + 1:02d}", merge(osc(w[0], w[1], "8", rng.choice(["8", "4"]), 0, jitter(0.06, 0.5), 0.85,
                                               jitter(0.6, 0.3, 0, 1), ovl=jitter(0.3, 0.3)),
                                           lp(jitter(500, 0.3), res, jitter(0.55, 0.15), KB_FULL),
                                           fenv(0.001, dec, 0.0, 0.2), aenv(0.001, dec * 3.0, 0.0, 0.4, 0.5),
                                           poly(jitter(0.2, 0.4))),
            glob=delay(jitter(0.25, 0.3), 0.4, jitter(0.5, 0.4), 0.5, rng.choice(["1/8D", "1/8", "1/16D"]), "1/4"))


# ---------------------------------------------------------------------------
# SEQUENCE / ARP (latch on: hold a chord once and it keeps running)
# ---------------------------------------------------------------------------
BERLIN = [36, 48, 43, 46, 36, 48, 41, 43, 36, 48, 43, 46, 36, 51, 50, 46]
PULSE_SEQ = [36, 36, 48, 36, 39, 36, 46, 36, 36, 36, 48, 36, 43, 36, 41, 39]
OCTAVE_JUMP = [36, 48, 36, 48, 39, 51, 39, 51, 41, 53, 41, 53, 43, 55, 46, 58]


def seq_data(notes, rests=()):
    full = (notes * (64 // len(notes) + 1))[:64]
    on = [0 if (i % len(notes)) in rests else 1 for i in range(64)]
    return full, on


def sequence_presets():
    c = "Sequence"
    add(c, "Berlin Arp", merge(osc(SAW, SAW, "8", "8", 0, 0.06, 0.85, 0.7, ovl=0.35), lp(500, 0.45, 0.45, KB_HALF),
                               fenv(0.001, 0.2, 0.05, 0.15), aenv(0.001, 0.3, 0.6, 0.15), poly(0.2),
                               lfo(2, 0.05, L_TRI), mm(1, "lfo2", "f2cut", 0.25)),
        glob=merge(arp("1/16", ARP_UP, 2, 0.5), delay(0.3, 0.45, 0.5, 0.4, "1/8D", "1/4")),
        seq=seq_data(BERLIN))
    add(c, "Up Down Bubbles", merge(osc(SQR, TRI, "8", "4", 0, 0.0, 0.8, 0.4, ovl=0.2), lp(900, 0.55, 0.4, KB_FULL),
                                    fenv(0.001, 0.15, 0.0, 0.1), aenv(0.001, 0.2, 0.3, 0.1), poly(0.1)),
        glob=merge(arp("1/16", ARP_UPDOWN, 3, 0.4), delay(0.3, 0.5, 0.4, 0.5, "1/16D", "1/8")))
    add(c, "Random Computer", merge(osc(SQR, SQR, "8", "4", 0, 0.0, 0.8, 0.5, ovl=0.2), lp(1500, 0.6, 0.3, KB_FULL),
                                    fenv(0.001, 0.1, 0.0, 0.1), aenv(0.001, 0.15, 0.0, 0.1), poly(0.1),
                                    lfo(1, sync=True, div="1/16", wave=L_SH), mm(1, "lfo1", "f2cut", 0.3)),
        glob=merge(arp("1/16", ARP_RANDOM, 3, 0.3), delay(0.3, 0.5, 0.5, 0.3)))
    add(c, "Pulse Train", merge(osc(PULSE, SAW, "16", "8", 0, 0.03, 0.85, 0.5, ovl=0.45), lp(350, 0.5, 0.5),
                                fenv(0.001, 0.12, 0.0, 0.1), aenv(0.001, 0.15, 0.3, 0.08), mono()),
        glob=merge(arp("1/16", ARP_ORDER, 1, 0.6), delay(0.2, 0.35)), seq=seq_data(PULSE_SEQ))
    add(c, "Octave Pump", merge(osc(SAW, SAW, "16", "8", 0, 0.05, 0.9, 0.6, ovl=0.5), lp(280, 0.4, 0.55),
                                fenv(0.001, 0.15, 0.0, 0.1), aenv(0.001, 0.2, 0.6, 0.1), mono()),
        glob=merge(arp("1/8", ARP_UP, 2, 0.5), delay(0.15, 0.3)), seq=seq_data(OCTAVE_JUMP))
    add(c, "Triplet Glass", merge(osc(TRI, SQR, "8", "4", 0, 0.0, 0.8, 0.3, ovl=0.1), lp(2500, 0.25, 0.3, KB_FULL),
                                  fenv(0.001, 0.2, 0.0, 0.3), aenv(0.001, 0.6, 0.0, 0.4), poly(0.1)),
        glob=merge(arp("1/8T", ARP_UP, 3, 0.5), delay(0.35, 0.5, 0.4, 0.8, "1/8T", "1/4T")))
    add(c, "Acid Arp", merge(osc(SAW, SAW, "16", "16", 0, 0.0, 0.9, 0.0, ovl=0.55), lp(350, 0.78, 0.55),
                             fenv(0.001, 0.18, 0.0, 0.1, 0.6), aenv(0.001, 0.25, 0.6, 0.08, 0.4), mono(0.04, True),
                             lfo(2, 0.07, L_TRI), mm(1, "lfo2", "f2cut", 0.3)),
        glob=merge(arp("1/16", ARP_ORDER, 2, 0.6), delay(0.2, 0.4)))
    add(c, "Sync Sequence", merge(osc(SAW, SAW, "8", "8", 0, 3.0, 0.3, 0.9, ovl=0.35, sync=True),
                                  lp(1800, 0.3, 0.35), fenv(0.001, 0.15, 0.0, 0.1), aenv(0.001, 0.2, 0.4, 0.1), poly(0.1),
                                  lfo(1, sync=True, div="2/1", wave=L_TRI), mm(1, "lfo1", "o2pitch", 0.3)),
        glob=merge(arp("1/16", ARP_UP, 2, 0.45), delay(0.25, 0.4)), seq=seq_data(BERLIN))
    add(c, "Looping Envelope", merge(osc(SAW, SQR, "8", "8", 0, 0.05, 0.8, 0.6, ovl=0.3), lp(400, 0.5, 0.55),
                                     fenv(0.001, 0.12, 0.0, 0.1, loop=True), aenv(0.005, 0.5, 1.0, 0.3), poly(0.2)),
        glob=delay(0.25, 0.45, 0.4, 0.4, "1/16D", "1/8D"))
    add(c, "Gated Chords", merge(osc(SAW, SAW, "8", "8", 0, 0.1, 0.85, 0.7, ovl=0.3), lp(1100, 0.3, 0.3),
                                 fenv(0.001, 0.3, 0.3, 0.2), aenv(0.001, 0.3, 1.0, 0.05), poly(0.3),
                                 lfo(1, sync=True, div="1/16", wave=L_SQR), mm(1, "lfo1", "vca", 0.5)),
        glob=delay(0.2, 0.35))
    patterns = [("Rising", ARP_UP), ("Falling", ARP_DOWN), ("Bouncing", ARP_UPDOWN), ("Played", ARP_ORDER),
                ("Scattered", ARP_RANDOM)]
    timbres = [("Saw", SAW, SAW, 600, 0.4), ("Square", SQR, SQR, 900, 0.3), ("Pulse", PULSE, TRI, 1200, 0.5),
               ("Bass", SAW, SQR, 300, 0.45)]
    for pname, mode in patterns:
        for tname, w1, w2, cut, res in timbres:
            octs = 1 if tname == "Bass" else rng.choice([1, 2, 3])
            div = rng.choice(["1/16", "1/8", "1/16", "1/8T"])
            add(c, f"{pname} {tname} Arp",
                merge(osc(w1, w2, "16" if tname == "Bass" else "8", "8", 0, jitter(0.05, 0.5), 0.85, 0.6, ovl=0.35),
                      lp(jitter(cut, 0.2), jitter(res, 0.2, 0, 0.9), 0.45, KB_HALF),
                      fenv(0.001, jitter(0.18, 0.3), 0.05, 0.12), aenv(0.001, 0.3, 0.4, 0.12),
                      mono() if tname == "Bass" else poly(0.2)),
                glob=merge(arp(div, mode, octs, jitter(0.5, 0.3, 0.1, 1)),
                           delay(jitter(0.25, 0.3), 0.4, 0.5, 0.4, rng.choice(["1/8D", "1/16D", "1/8"]), "1/4")))


# ---------------------------------------------------------------------------
# FX
# ---------------------------------------------------------------------------
def fx_presets():
    c = "FX"
    add(c, "Wind Tunnel", merge(osc(TRI, TRI, "8", "8", 0, 0.0, 0.0, 0.0, noise=1.0, ovl=0.1), lp(800, 0.85, 0.0, KB_FULL),
                                fenv(), aenv(1.0, 1.0, 1.0, 2.0), poly(0.0), lfo(2, 0.12, L_SMOOTH),
                                mm(1, "lfo2", "cutoff", 0.4)), glob=delay(0.3, 0.5, 0.6, 1.0))
    add(c, "Laser Zap", merge(osc(SAW, SQR, "8", "8", 0, 0.0, 0.8, 0.5, ovl=0.3), lp(3000, 0.4, 0.3),
                              fenv(0.001, 0.25, 0.0, 0.2), aenv(0.001, 0.35, 0.0, 0.2), poly(0.0),
                              mm(1, "fenv", "pitch", 1.0)), glob=delay(0.3, 0.5, 0.3, 0.3, "1/16", "1/8"))
    add(c, "Ring Bells", merge(osc(SQR, TRI, "8", "8", 0, 6.15, 0.0, 0.0, ring=1.0, ovl=0.1), lp(4000, 0.1, 0.0, KB_FULL),
                               fenv(), aenv(0.001, 2.5, 0.0, 2.0, 0.4), poly(0.0)),
        glob=delay(0.35, 0.5, 0.4, 0.8))
    add(c, "Chaos FM", merge(osc(SAW, SQR, "8", "8", 0, 0.0, 0.7, 0.6, ovl=0.5, fm=0.8, fmsrc=FM_MODBOTH),
                             modosc(180.0, MO_SAW, filt=0.2), lp(2000, 0.5, 0.2), fenv(), aenv(0.01, 1.0, 1.0, 1.0),
                             poly(0.0), lfo(2, 0.2, L_SMOOTH), mm(1, "lfo2", "modrate", 0.6)))
    add(c, "Self Osc Whistle", merge(osc(TRI, TRI, "8", "8", 0, 0.0, 0.0, 0.0, ovl=0.0), lp(1000, 1.0, 0.05, KB_FULL),
                                     fenv(), aenv(0.05, 1.0, 1.0, 0.6), mono(0.1), vib(0.25)))
    add(c, "Computer Bleeps", merge(osc(SQR, SQR, "8", "4", 0, 0.0, 0.8, 0.4, ovl=0.2), lp(2500, 0.3, 0.0, KB_FULL),
                                    fenv(), aenv(0.001, 0.5, 1.0, 0.2), poly(0.0),
                                    lfo(1, 8.0, L_SH), mm(1, "lfo1", "pitch", 0.5)), glob=delay(0.3, 0.4))
    add(c, "Space Drone", merge(osc(SAW, SAW, "16", "8", 0, 7.03, 0.8, 0.7, noise=0.1, ovl=0.4), lp(400, 0.6, 0.2),
                                fenv(), aenv(2.0, 1.0, 1.0, 4.0), poly(0.6), vca(spread=0.9),
                                lfo(1, 0.03, L_SMOOTH), lfo(2, 0.07, L_SMOOTH),
                                mm(1, "lfo1", "cutoff", 0.4), mm(2, "lfo2", "pan", 0.6)),
        glob=delay(0.45, 0.65, 0.6, 1.0, "1/2", "1/4D"))
    add(c, "Siren", merge(osc(SQR, SQR, "8", "8", 0, 0.0, 0.8, 0.0, ovl=0.3), lp(3000, 0.2, 0.0), fenv(),
                          aenv(0.01, 1.0, 1.0, 0.5), mono(), lfo(1, 0.4, L_TRI), mm(1, "lfo1", "pitch", 0.5)))
    add(c, "Ocean Surf", merge(osc(TRI, TRI, "8", "8", 0, 0.0, 0.0, 0.0, noise=1.0, ovl=0.1), lp(600, 0.2, 0.0, KB_OFF),
                               fenv(), aenv(1.5, 1.0, 1.0, 3.0), poly(0.0), lfo(2, 0.09, L_SINE),
                               mm(1, "lfo2", "cutoff", 0.5), mm(2, "lfo2", "vca", 0.3)),
        glob=delay(0.3, 0.4, 0.8, 1.0))
    add(c, "Mod Osc Growler", merge(osc(SAW, SAW, "16", "8", 0, 0.05, 0.8, 0.6, ovl=0.6),
                                    modosc(35.0, MO_SQR, filt=0.25, kb=True), lp(500, 0.6, 0.3), fenv(),
                                    aenv(0.01, 0.5, 1.0, 0.3), mono(0.1)))
    add(c, "Noise Snare", merge(osc(TRI, TRI, "8", "8", 0, 0.0, 0.4, 0.0, noise=0.9, ovl=0.3), hplp(400, 5000, 0.2, 0.3),
                                fenv(0.001, 0.12, 0.0, 0.1), aenv(0.001, 0.18, 0.0, 0.15, 0.6), poly(0.0)))
    add(c, "Analog Kick", merge(osc(TRI, TRI, "16", "16", 0, 0.0, 1.0, 0.0, ovl=0.5), lp(300, 0.2, 0.5, KB_OFF),
                                fenv(0.001, 0.08, 0.0, 0.08), aenv(0.001, 0.35, 0.0, 0.3, 0.5), poly(0.0),
                                mm(1, "fenv", "pitch", 0.6)))
    add(c, "Metal Hats", merge(osc(SQR, SQR, "4", "4", 0, 5.43, 0.0, 0.0, ring=1.0, noise=0.4, ovl=0.2),
                               hplp(6000, 15000, 0.1, 0.0), fenv(), aenv(0.001, 0.08, 0.0, 0.06, 0.6), poly(0.0)))
    add(c, "Thunder Roll", merge(osc(TRI, TRI, "16", "16", 0, 0.0, 0.0, 0.0, noise=1.0, ovl=0.6), lp(150, 0.4, 0.3, KB_OFF),
                                 fenv(0.4, 3.0, 0.0, 2.0), aenv(0.3, 3.0, 0.0, 3.0), poly(0.0),
                                 lfo(1, 6.0, L_SH), mm(1, "lfo1", "cutoff", 0.15)), glob=delay(0.3, 0.5, 0.8, 1.0))
    add(c, "Alien Chatter", merge(osc(SAW, SQR, "8", "8", 0, 0.0, 0.6, 0.6, ovl=0.3), lp(1500, 0.75, 0.0),
                                  fenv(), aenv(0.005, 1.0, 1.0, 0.5), poly(0.0),
                                  lfo(1, 11.0, L_SH), lfo(2, 0.3, L_SMOOTH),
                                  mm(1, "lfo1", "f2cut", 0.35), mm(2, "lfo2", "l1rate", 0.6)),
        glob=delay(0.3, 0.5, 0.4, 0.6, "1/16", "1/8T"))
    add(c, "Riser", merge(osc(SAW, SAW, "8", "8", 0, 0.15, 0.8, 0.8, noise=0.3, ovl=0.4), hplp(100, 300, 0.5, 0.9, kb=KB_OFF),
                          fenv(6.0, 1.0, 1.0, 1.0), aenv(4.0, 1.0, 1.0, 1.0), unison(6, 0.6),
                          mm(1, "fenv", "pitch", 0.5)), glob=delay(0.35, 0.5, 0.5, 0.9))
    for i in range(10):
        add(c, f"Texture {i + 1:02d}", merge(
            osc(rng.choice([SAW, SQR, TRI, PULSE]), rng.choice([SAW, SQR, TRI]), rng.choice(["16", "8", "4"]),
                rng.choice(["8", "4", "2"]), 0, round(rng.uniform(-7, 7), 2), 0.7, 0.6, ring=rng.choice([0, 0.4]),
                noise=rng.choice([0, 0.2]), ovl=jitter(0.3, 0.5)),
            lp(jitter(900, 0.5), jitter(0.5, 0.4, 0, 0.9), 0.2),
            modosc(jitter(3.0, 0.8, 0.05), rng.choice([MO_SH, MO_TRI, MO_SINE]), filt=jitter(0.15, 0.5), pwm=0.3),
            fenv(), aenv(jitter(1.0, 0.5), 1.0, 1.0, jitter(2.0, 0.4)), poly(0.5), vca(spread=0.9),
            lfo(2, jitter(0.1, 0.5), L_SMOOTH), mm(1, "lfo2", "cutoff", jitter(0.2, 0.4))),
            glob=delay(0.4, 0.55, jitter(0.5, 0.5, 0, 1), 0.9, "1/4D", "1/2"))


# ---------------------------------------------------------------------------
# SPLIT / STACK (bitimbral)
# ---------------------------------------------------------------------------
BASS_A = merge(osc(SAW, SAW, "16", "16", 0, 0.05, 0.9, 0.75, ovl=0.45), lp(240, 0.22, 0.5),
               fenv(0.001, 0.35, 0.12, 0.15), aenv(0.001, 0.5, 0.9, 0.12), mono(prio=PRIO_LOW))
PAD_A = merge(osc(PULSE, SAW, "8", "8", 0, 0.1, 0.75, 0.6, ovl=0.2), lp(2000, 0.12, 0.2),
              modosc(0.35, MO_TRI, pwm=0.5), fenv(0.5, 2.0, 0.6, 1.2), aenv(0.4, 1.5, 0.9, 1.2),
              poly(0.4), vca(0.7, spread=0.7))
LEAD_A = merge(osc(SQR, SAW, "8", "8", 0, 0.06, 0.8, 0.6, ovl=0.35), lp(1800, 0.28, 0.35),
               fenv(0.02, 0.6, 0.55, 0.4), aenv(0.01, 0.5, 0.95, 0.35), mono(0.1, True), vib(0.3))
BRASS_A = merge(osc(SAW, SAW, "8", "8", 0, 0.1, 0.85, 0.8, ovl=0.35), lp(700, 0.1, 0.55),
                fenv(0.06, 0.5, 0.45, 0.3), aenv(0.03, 0.5, 0.9, 0.3), poly(0.3), vca(0.65))
STRINGS_A = merge(osc(SAW, SAW, "8", "4", 0, 0.05, 0.8, 0.5, ovl=0.15), hplp(220, 3000, 0.05, 0.1),
                  fenv(0.3, 1.0, 0.8, 1.0), aenv(0.3, 1.0, 1.0, 1.0), poly(0.55), vca(0.65, spread=0.8))
KEYS_A = merge(osc(SAW, SAW, "8", "8", 0, 0.08, 0.85, 0.7, ovl=0.3), lp(900, 0.15, 0.45, KB_FULL),
               fenv(0.002, 0.6, 0.25, 0.4, 0.5), aenv(0.002, 1.2, 0.5, 0.4, 0.5), poly(0.25))
PLUCK_A = merge(osc(SAW, SAW, "8", "8", 0, 0.07, 0.85, 0.7, ovl=0.3), lp(400, 0.35, 0.6, KB_FULL),
                fenv(0.001, 0.18, 0.0, 0.2, 0.5), aenv(0.001, 0.5, 0.0, 0.4, 0.5), poly(0.2))
SUB_A = merge(osc(TRI, SQR, "16", "32", 0, 0, 0.85, 0.5, ovl=0.2), lp(300, 0.1, 0.2),
              fenv(), aenv(0.004, 0.5, 1.0, 0.15), mono(prio=PRIO_LOW))


def split_presets():
    c = "Split"
    split = lambda note: dict(timbreMode=SPLIT, splitNote=note)
    stack = dict(timbreMode=STACK)
    upper = [("Pad", PAD_A), ("Lead", LEAD_A), ("Brass", BRASS_A), ("Strings", STRINGS_A), ("Keys", KEYS_A),
             ("Pluck", PLUCK_A)]
    lower = [("Bass", BASS_A), ("Sub", SUB_A)]
    for lname, low in lower:
        for uname, up in upper:
            add(c, f"{lname} + {uname}", up, low, glob=merge(split(48 if lname == "Bass" else 43), delay(0.2, 0.35)))
    add(c, "Brass Over Strings", BRASS_A, STRINGS_A, glob=merge(stack, delay(0.25, 0.4, 0.5, 0.9)))
    add(c, "Lead Over Pad", LEAD_A, merge(PAD_A, vca(0.5, spread=0.8)), glob=merge(stack, delay(0.25, 0.4)))
    add(c, "Keys And Strings", KEYS_A, STRINGS_A, glob=merge(stack, delay(0.2, 0.35, 0.5, 0.8)))
    add(c, "Pluck And Pad", PLUCK_A, merge(PAD_A, vca(0.5, spread=0.8)), glob=merge(stack, delay(0.3, 0.45)))
    add(c, "Fat Stack Bass", BASS_A, merge(SUB_A, vca(0.6)), glob=stack)
    add(c, "Octave Brass Stack", BRASS_A, merge(BRASS_A, dict(o1Oct=OCT["16"], o2Oct=OCT["16"])), glob=stack)
    add(c, "Wide Stereo Stack", merge(PAD_A, vca(0.6, -0.6, 0.3)), merge(STRINGS_A, vca(0.6, 0.6, 0.3)),
        glob=merge(stack, delay(0.3, 0.45, 0.5, 0.9)))
    add(c, "Arp Over Bass", merge(PLUCK_A, vca(0.6)), BASS_A,
        glob=merge(split(48), arp("1/16", ARP_UP, 2, 0.5), delay(0.25, 0.4)))


# ---------------------------------------------------------------------------
# Writer
# ---------------------------------------------------------------------------
def fmt(v):
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, int):
        return str(v)
    return f"{v:.6g}"


def slug(text):
    return re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_")


def write_all():
    os.makedirs(OUT_DIR, exist_ok=True)
    for f in os.listdir(OUT_DIR):
        if f.endswith(".xml"):
            os.remove(os.path.join(OUT_DIR, f))

    for category, name, params, seq, chord in PRESETS:
        attrs = " ".join(f"{k}={quoteattr(fmt(v))}" for k, v in sorted(params.items()))
        lines = ['<?xml version="1.0" encoding="UTF-8"?>',
                 f'<RecklessMusePreset name={quoteattr(name)} category={quoteattr(category)} author="RecklessMuse">',
                 f"  <Params {attrs}/>"]
        if seq is not None:
            notes, on = seq
            lines.append(f'  <Sequence notes="{",".join(map(str, notes))}" on="{",".join(map(str, on))}"/>')
        if chord is not None:
            lines.append(f'  <Chord intervals="{",".join(map(str, chord))}"/>')
        lines.append("</RecklessMusePreset>")
        filename = f"{category}_{slug(name)}.xml"
        with open(os.path.join(OUT_DIR, filename), "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines) + "\n")


def chord_presets():
    """A few presets that ship with a chord memory (CHORD ON: one key plays the whole chord)."""
    chords = [("Minor Seventh Stab", [0, 3, 7, 10], BRASS_A), ("Major Ninth Pad", [0, 4, 7, 11, 14], PAD_A),
              ("Fifths Power", [0, 7, 12], merge(LEAD_A, poly(0.3))), ("Sus Four Keys", [0, 5, 7, 12], KEYS_A),
              ("Dub Chord", [0, 3, 7], merge(PLUCK_A, lp(700, 0.3, 0.5, KB_FULL)))]
    for name, iv, a in chords:
        cat = "Keys" if "Keys" in name or "Dub" in name else ("Pad" if "Pad" in name else "Brass")
        add(cat, name, a, glob=merge(dict(chordOn=1), delay(0.3, 0.45, 0.5, 0.6, "1/8D", "1/4")), chord=iv)


if __name__ == "__main__":
    bass_presets()
    lead_presets()
    pad_presets()
    keys_presets()
    brass_presets()
    string_presets()
    pluck_presets()
    sequence_presets()
    fx_presets()
    split_presets()
    chord_presets()
    write_all()
    counts = {}
    for p in PRESETS:
        counts[p[0]] = counts.get(p[0], 0) + 1
    print(f"Wrote {len(PRESETS)} presets:", ", ".join(f"{k} {v}" for k, v in counts.items()))
