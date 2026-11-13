#!/usr/bin/env python3
# Crea Resources/presets.json: 10 preset per categoria (sample + impostazioni del plugin).
# I parametri non indicati tornano al valore iniziale quando si carica un preset.
import json, random

lib = json.load(open('Samples/library.json'))
by_cat = {}
for s in lib['samples']:
    name = s['file'].split('/')[-1].rsplit('.', 1)[0]
    by_cat.setdefault(s['cat'], []).append(name)

# indici delle scelte: lfoDiv 0=1/8 1=1/4 2=1/2 3=1bar 4=2bar 5=4bar 6=8bar 7=16bar 8=32bar
# dlyDiv 0=1/16 1=1/8T 2=1/8 3=3/16 4=1/4T 5=1/4 6=3/8 7=1/2 ; fltMode 0 LP 1 BP 2 HP ; lfoShape 0 SIN 1 TRI 2 RMP 3 S&H
def R(a, b, d=3): return round(random.uniform(a, b), d)

NAMES = {
 'MD': ['Mango Fog', 'Wackies Haze', 'Distant Skank', 'Tropical Night', 'Hiss Garden', 'Slow Drive', 'Warm Tape',
        'Faded Keys', 'Late Bus', 'Sunset Dub'],
 'BC': ['Quadrant Fog', 'Octagon Pulse', 'Phylyps Air', 'Radiance Loop', 'Mutism', 'Lyot Echo', 'Hardwax Basement',
        'Grey Channel', 'Inner Space', 'Resonance 93'],
 'BW': ['Deep Woompf', 'Sub Chamber', 'Concrete Pad', 'Low Pressure', 'Bunker', 'Thick Felt', 'Midnight Mass',
        'Heavy Air', 'Dark Bloom', 'Tunnel'],
 'PH': ['Metal Flange', 'Comb Rain', 'Phylyps Shine', 'Steel Wash', 'Jet Stream', 'Glass Tube', 'Ring Chamber',
        'Silver Spin', 'Tin Echo', 'Chrome Dub'],
 'MZ': ['M-Series Pump', 'Domina Stab', 'Ploy Chord', 'Club Pressure', 'Clean Punch', 'Tight Groove', 'Peak Stab',
        'Berlin Drive', 'Direct Cut', 'Maurizio Air'],
 'RS': ['Burial Skank', 'Spring Organ', 'Rhodes Dub', 'Roots Off-Beat', 'Melodica Shade', 'Kingston Echo',
        'Paul Riddim', 'Trench Organ', 'Yard Keys', 'Wackies Spring'],
 'CR': ['Chain Grit', 'Echospace Fog', 'Resampled Dust', '12 Bit Rain', 'Porter Haze', 'Tape Decay', 'Static Bloom',
        'Monochrome', 'Detroit Ghost', 'Cold Room'],
 'SW': ['Swayzak Glow', 'Snowboarding', 'Lush Minimal', 'Warm Ensemble', 'Open Sky', 'Soft Focus', 'Silk Chord',
        'Morning Dub', 'Velvet', 'Bright Tape'],
 'MV': ['Moving Tide', 'Slow Sweep', 'Breathing', 'Random Steps', 'Wah Drift', 'Pendulum', 'Long Bloom',
        'Ocean Filter', 'Tide Pool', 'Flux'],
}

def base_dub():
    return {'dlySync': 1, 'dlyDiv': 3, 'dlyFeed': R(.5, .62), 'dlyWow': R(.2, .4), 'dlyTone': R(1800, 3000, 0),
            'dlyMix': R(.3, .42), 'dlySweep': R(.2, .5), 'dlySweepDiv': 1, 'lfoSync': 1, 'drift': R(.25, .4),
            'fltRand': R(.2, .35), 'release': R(250, 450, 0), 'sat': R(.25, .4), 'hiss': R(.08, .2), 'spMix': R(.12, .25)}

def recipe(cat, i):
    p = base_dub()
    v = i % 5   # cinque "facce" diverse per categoria, ognuna in due varianti
    if cat == 'MD':
        p.update(fltMode=1, cutoff=R(620, 950, 0), reso=R(.32, .5), fltEnv=R(.35, .5), fltDecay=R(240, 420, 0),
                 lfoDiv=[5, 6, 5, 7, 4][v], lfoShape=[0, 1, 0, 0, 1][v], lfoDepth=R(.3, .5), hiss=R(.22, .38),
                 spMix=R(.2, .35), dlyTone=R(1800, 2400, 0), release=R(380, 520, 0))
        if v == 2: p.update(dlyDiv=2, dlyPing=1)
        if v == 4: p.update(fltMode=0, cutoff=R(900, 1400, 0), reso=R(.2, .3))
    elif cat == 'BC':
        p.update(fltMode=[1, 1, 0, 1, 0][v], cutoff=R(480, 820, 0), reso=R(.45, .62), fltEnv=R(.48, .62),
                 fltDecay=R(180, 300, 0), lfoDiv=[6, 7, 5, 8, 6][v], lfoShape=[0, 1, 0, 2, 1][v], lfoDepth=R(.45, .7),
                 dlyFeed=R(.64, .8), dlyMix=R(.4, .5), dlyDiv=[3, 3, 4, 3, 6][v], dlySweep=R(.4, .6), dlySweepDiv=[1, 2, 1, 2, 1][v],
                 spMix=R(.05, .12), low=1.0)
        if v == 3: p.update(phMix=R(.25, .4), phDiv=5, phDepth=.6, phFeed=.4)
    elif cat == 'BW':
        p.update(fltMode=0, cutoff=R(280, 520, 0), reso=R(.25, .4), fltEnv=R(.6, .8), fltDecay=R(140, 220, 0),
                 lfoDiv=[7, 6, 8, 5, 7][v], lfoDepth=R(.2, .35), dlyFeed=R(.5, .6), dlyMix=R(.3, .4), sat=R(.4, .55), low=2.0)
        if v in (1, 3): p.update(fltMode=1, cutoff=R(400, 650, 0), reso=R(.4, .55))
    elif cat == 'PH':
        p.update(fltMode=[1, 0, 1, 0, 1][v], cutoff=R(800, 1300, 0), reso=R(.3, .45), fltEnv=R(.35, .5), fltDecay=R(200, 350, 0),
                 lfoDiv=6, lfoDepth=R(.25, .4), flMix=R(.35, .6), flSync=1, flDiv=[4, 5, 3, 6, 4][v], flDepth=R(.5, .75),
                 flFeed=R(-.6, -.3) if v % 2 else R(.3, .6), dlyDiv=[2, 3, 3, 5, 3][v], dlyFeed=R(.45, .6))
        if v == 4: p.update(phMix=R(.3, .5), phStages=2, phDiv=4)
    elif cat == 'MZ':
        p.update(fltMode=0, cutoff=R(950, 1600, 0), reso=R(.2, .32), fltEnv=R(.45, .6), fltDecay=R(120, 200, 0),
                 lfoDiv=[5, 4, 6, 5, 3][v], lfoDepth=R(.18, .3), dlyFeed=R(.4, .5), dlyMix=R(.25, .35), dlySweep=R(0, .25),
                 fltRand=R(.12, .22), drift=R(.15, .25), hiss=R(0, .08), sat=R(.3, .42))
        if v == 2: p.update(fltMode=1, cutoff=R(800, 1100, 0), reso=R(.35, .45))
        if v == 4: p.update(dlyDiv=4)
    elif cat == 'RS':
        p.update(fltMode=[1, 0, 1, 2, 0][v], cutoff=R(950, 1500, 0), reso=R(.2, .32), fltEnv=R(.25, .4), fltDecay=R(150, 300, 0),
                 lfoDiv=[5, 6, 4, 5, 6][v], lfoDepth=R(.15, .3), spMix=R(.45, .7), spDecay=R(.55, .7), dlyFeed=R(.5, .62),
                 dlyMix=R(.3, .4), dlyTone=R(1500, 2200, 0), hiss=R(.2, .35), decay=R(350, 700, 0))
        if v == 3: p.update(cutoff=R(250, 400, 0), dlyPing=1)
        if v == 2: p.update(dlyDiv=2)
    elif cat == 'CR':
        p.update(fltMode=[0, 0, 1, 0, 1][v], cutoff=R(480, 800, 0), reso=R(.35, .5), fltEnv=R(.3, .45), fltDecay=R(250, 450, 0),
                 lfoDiv=[2, 4, 3, 8, 7][v], lfoShape=[3, 2, 3, 0, 1][v], lfoDepth=R(.25, .4), dlyDiv=[6, 3, 6, 5, 3][v],
                 dlyFeed=R(.66, .82), dlyMix=R(.4, .5), dlyWow=R(.45, .65), dlySweep=R(.5, .7), dlySweepDiv=2,
                 hiss=R(.4, .6), sat=R(.38, .5))
    elif cat == 'SW':
        p.update(fltMode=0, cutoff=R(2000, 3600, 0), reso=R(.12, .2), fltEnv=R(.2, .32), fltDecay=R(250, 450, 0),
                 lfoDiv=[6, 5, 7, 6, 5][v], lfoDepth=R(.15, .25), dlyDiv=[5, 3, 5, 6, 3][v], dlyFeed=R(.4, .5),
                 dlyMix=R(.25, .35), dlyTone=R(3500, 5000, 0), flMix=R(.12, .25), flDiv=5, flDepth=.4, flFeed=.2,
                 hiss=R(0, .06), sat=R(.15, .25), high=1.5)
        if v == 4: p.update(fltMode=1, cutoff=R(1200, 1600, 0), reso=.3, flMix=0)
    elif cat == 'MV':
        p.update(fltMode=0, cutoff=R(2500, 6000, 0), reso=R(.1, .2), fltEnv=R(0, .1), lfoDepth=R(.05, .15),
                 lfoDiv=[7, 8, 6, 7, 8][v], dlyFeed=R(.55, .66), dlyMix=R(.35, .45), dlySweep=R(.3, .5), decay=4000,
                 release=R(500, 900, 0))
        if v == 3: p.update(fltMode=1, cutoff=R(900, 1300, 0), reso=.35, lfoShape=3, lfoDiv=2, lfoDepth=.25)
    return p

def pick_sample(cat, i):
    names = by_cat[cat]
    # sceglie sample diversi distribuendoli sulla categoria
    step = max(1, len(names) // 10)
    return names[(i * step + i // 3) % len(names)]

random.seed(2026)
presets = []
for c in lib['categories']:
    cat = c['id']
    for i, nm in enumerate(NAMES[cat]):
        presets.append({'name': nm, 'cat': cat, 'sample': pick_sample(cat, i), 'params': recipe(cat, i)})
# volumi livellati: tools/preset_levels.json è misurato dal banco di prova (RMS con volume -3 dB)
import os
if os.path.exists('tools/preset_levels.json'):
    lv = json.load(open('tools/preset_levels.json'))
    target = -21.0
    for p in presets:
        if p['name'] in lv:
            p['params']['volume'] = round(max(-24.0, min(6.0, -3.0 + (target - lv[p['name']]))), 1)
json.dump({'presets': presets}, open('Resources/presets.json', 'w'), indent=1, ensure_ascii=False)
print(len(presets), 'preset')
