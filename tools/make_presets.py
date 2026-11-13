#!/usr/bin/env python3
"""Generates the factory preset library (presets/*.json).

Presets are written as short recipes here, so the whole library can be
reviewed in one file and regenerated after DSP changes. After generating,
run `riffhouse_tests levels levels.json` and this script's
`--calibrate levels.json` step to level-match every preset. A plain run keeps
each preset's output level from the file already in presets/, so regenerating
never loses the calibration; the levels are measured as if each output level
were 0 dB, so calibrating gives the same answer however often it is run.

"Inspired by" presets are fan-made approximations of a player's general
style using Riffhouse's own models. Riffhouse is not affiliated with or
endorsed by any artist named here.
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "presets")

AMPS = ["Glass Clean", "Tweed Edge", "Chime Top", "Brit 68", "Brit 81", "Redline Lead",
        "Modern High Gain", "Tight Metal", "Bass Station", "Studio Direct", "Neural Capture"]
CABS = ["1x12 Open Back", "2x12 Blue", "4x12 Vintage", "4x12 Modern", "2x12 American",
        "1x8 Tiny Combo", "4x10 Bass", "1x15 Bass", "Your IR"]
DRIVES = ["Clean Boost", "Treble Boost", "Green Drive", "Blues Drive", "Rodent Dist", "Big Fuzz", "Metal Box"]

def A(model, gain=5, bass=5, mid=5, treble=5, presence=5, depth=5, master=5, level=0.0):
    assert model in AMPS, model
    return dict(amp_on=True, amp_model=model, amp_gain=gain, amp_bass=bass, amp_mid=mid, amp_treble=treble,
                amp_presence=presence, amp_depth=depth, amp_master=master, amp_level=level)

def C(model, mic=3, dist=2, low=60, high=12000):
    assert model in CABS, model
    return dict(cab_on=True, cab_model=model, cab_mic=mic, cab_distance=dist, cab_lowcut=low, cab_highcut=high)

def D(kind, gain=4, tone=5, level=5):
    assert kind in DRIVES, kind
    return dict(drv_on=True, drv_type=kind, drv_gain=gain, drv_tone=tone, drv_level=level)

def G(thresh=-62, release=80):
    return dict(gate_on=True, gate_thresh=thresh, gate_release=release)

def NOGATE():
    return dict(gate_on=False)

def COMP(sustain=5, attack=4, level=5, blend=10):
    return dict(comp_on=True, comp_sustain=sustain, comp_attack=attack, comp_level=level, comp_blend=blend)

def WAH(mode="Auto", pos=5, sens=6, q=5):
    return dict(wah_on=True, wah_mode=mode, wah_pos=pos, wah_sens=sens, wah_q=q)

def PITCH(mode="Sub Octave", semis=-12, mix=5):
    return dict(pitch_on=True, pitch_mode=mode, pitch_semis=semis, pitch_mix=mix)

def MOD(kind, rate=4, depth=5, mix=5):
    return dict(mod_on=True, mod_type=kind, mod_rate=rate, mod_depth=depth, mod_mix=mix)

def DLY(kind="Digital", time=380, sync="Off", fb=3.5, tone=6, mix=2.5):
    return dict(dly_on=True, dly_type=kind, dly_time=time, dly_sync=sync, dly_feedback=fb, dly_tone=tone, dly_mix=mix)

def REV(kind="Room", decay=4, pre=12, tone=5, mix=1.8):
    return dict(rev_on=True, rev_type=kind, rev_decay=decay, rev_predelay=pre, rev_tone=tone, rev_mix=mix)

def NOREV():
    return dict(rev_on=False)

def LOFI(crush=0, rate=0, wobble=4, noise=3, radio=3, mix=10):
    return dict(lofi_on=True, lofi_bits=crush, lofi_rate=rate, lofi_wobble=wobble, lofi_noise=noise, lofi_filter=radio, lofi_mix=mix)

def EQ(b100=0, b200=0, b400=0, b800=0, b1k6=0, b3k2=0, b6k4=0, level=0):
    return dict(eq_on=True, eq_100=b100, eq_200=b200, eq_400=b400, eq_800=b800, eq_1k6=b1k6, eq_3k2=b3k2, eq_6k4=b6k4, eq_level=level)

def FIX(**kw):
    return dict(kw)

# The band a player is known for, so a tone made after the player is also found under the band's name.
BANDS = {
    "Adam Jones": "Tool", "Alex Turner": "Arctic Monkeys", "Angus Young": "AC/DC", "Billie Joe Armstrong": "Green Day",
    "Billy Corgan": "The Smashing Pumpkins", "Brian May": "Queen", "Chuck Schuldiner": "Death", "Cliff Burton": "Metallica",
    "Dan Auerbach": "The Black Keys", "David Gilmour": "Pink Floyd", "Dimebag Darrell": "Pantera",
    "Eddie Van Halen": "Van Halen", "Flea": "Red Hot Chili Peppers", "Geddy Lee": "Rush", "Jack White": "The White Stripes",
    "James Hetfield": "Metallica", "Jerry Cantrell": "Alice in Chains", "Jimmy Page": "Led Zeppelin",
    "Joe Duplantier": "Gojira", "John Frusciante": "Red Hot Chili Peppers", "Johnny Marr": "The Smiths",
    "Johnny Ramone": "Ramones", "Jonny Greenwood": "Radiohead", "Josh Homme": "Queens of the Stone Age",
    "Keith Richards": "The Rolling Stones", "Kerry King": "Slayer", "Kevin Parker": "Tame Impala",
    "Kevin Shields": "My Bloody Valentine", "Kim Thayil": "Soundgarden", "Kirk Hammett": "Metallica",
    "Kurt Cobain": "Nirvana", "Mark Knopfler": "Dire Straits", "Mark Speer": "Khruangbin", "Mark Tremonti": "Alter Bridge",
    "Matt Bellamy": "Muse", "Max Cavalera": "Sepultura", "Mikael Akerfeldt": "Opeth", "Misha Mansoor": "Periphery",
    "Nile Rodgers": "Chic", "Pete Townshend": "The Who", "Rahul Ram": "Indian Ocean", "Randy Rhoads": "Ozzy Osbourne",
    "Rivers Cuomo": "Weezer", "Robert Smith": "The Cure", "Slash": "Guns N' Roses", "Steve Jones": "Sex Pistols",
    "Synyster Gates": "Avenged Sevenfold", "The Edge": "U2", "Tim Henson": "Polyphia", "Tom DeLonge": "blink-182",
    "Tom Morello": "Rage Against the Machine", "Tony Iommi": "Black Sabbath", "Tosin Abasi": "Animals as Leaders",
    "Zakk Wylde": "Black Label Society",
}

# Tones made after one song say which song.
SONGS = {
    ("B.B. King", "Thrill Lead"): "The Thrill Is Gone",
    ("Billie Joe Armstrong", "Basket Crunch"): "Basket Case",
    ("David Gilmour", "Comfortably Lead"): "Comfortably Numb",
    ("David Gilmour", "Shine On Clean"): "Shine On You Crazy Diamond",
    ("Dimebag Darrell", "Cowboys Groove"): "Cowboys from Hell",
    ("Jack White", "Seven Nation Fuzz"): "Seven Nation Army",
    ("Jerry Cantrell", "Rooster Grind"): "Rooster",
    ("Jimi Hendrix", "Little Wing Clean"): "Little Wing",
    ("Jimi Hendrix", "Purple Fuzz Lead"): "Purple Haze",
    ("John Mayer", "Gravity Clean"): "Gravity",
    ("John Mayer", "Slow Dancing Clean"): "Slow Dancing in a Burning Room",
    ("Johnny Ramone", "Blitzkrieg Buzz"): "Blitzkrieg Bop",
    ("Kim Thayil", "Black Hole Drone"): "Black Hole Sun",
    ("Kurt Cobain", "Come As You Are Clean"): "Come As You Are",
    ("Kurt Cobain", "Teen Spirit Distortion"): "Smells Like Teen Spirit",
    ("Mac DeMarco", "Salad Days Wobble"): "Salad Days",
    ("Mark Knopfler", "Sultans Clean"): "Sultans of Swing",
    ("Max Cavalera", "Roots Groove"): "Roots Bloody Roots",
    ("Prince", "Purple Lead"): "Purple Rain",
    ("Randy Rhoads", "Crazy Train Lead"): "Crazy Train",
    ("Rivers Cuomo", "Buddy Holly Fuzz"): "Buddy Holly",
    ("Slash", "Paradise Lead"): "Paradise City",
    ("Stevie Ray Vaughan", "Texas Flood"): "Texas Flood",
    ("The Edge", "Streets Delay"): "Where the Streets Have No Name",
    ("The Local Train", "Aaoge Tum Kabhi Rock"): "Aaoge Tum Kabhi",
    ("Tom Morello", "Killing Riff"): "Killing in the Name",
}

PRESETS = []

def P(name, category, genres, desc, *parts, inspired="", band="", song="", out=0.0, double=0):
    params = {}
    for part in parts:
        params.update(part)
    if double:
        params["doubler"] = double
    params["out_level"] = out
    preset = dict(name=name, category=category, genres=genres, inspiredBy=inspired)
    if band: preset["band"] = band
    if song: preset["song"] = song
    preset.update(description=desc, params=params)
    PRESETS.append(preset)

def I(artist, title, category, genres, desc, *parts, band="", song="", **kw):
    P(f"Inspired by {artist}: {title}", category, genres, desc, *parts, inspired=artist,
      band=band or BANDS.get(artist, ""), song=song or SONGS.get((artist, title), ""), **kw)

# A tone made after one song of a band. The song is the tone's title, with the part it is for when there are several.
def S(band, song, part, genres, desc, *parts, **kw):
    P(f"Inspired by {band}: {song}" + (f" {part}" if part else ""), "Inspired By", genres, desc, *parts,
      inspired=band, band=band, song=song, **kw)

# ---------------------------------------------------------------- Essentials
P("First Light Clean", "Essentials", ["Pop", "Indie", "Worship"], "A clean, polished starting point. Strum and go.",
  A("Glass Clean", 4, 5, 5, 6, 5), C("2x12 American", 3, 2), COMP(4, 5), REV("Plate", 4, 15, 5, 2.2), G(-70))
P("Warm Crunch", "Essentials", ["Rock", "Blues", "Indie"], "Breaks up when you dig in, cleans up when you play soft.",
  A("Brit 68", 5.5, 5, 6, 5.5, 5), C("4x12 Vintage", 3, 2), REV("Room", 3.5, 8, 5, 1.6), G(-66))
P("Big Rock Rhythm", "Essentials", ["Rock", "Hard Rock"], "Thick, punchy rock rhythm.",
  A("Brit 81", 6, 5.5, 6.5, 5.5, 5.5), C("4x12 Vintage", 3, 2), REV("Room", 3, 8, 5, 1.2), G(-62))
P("Heavy Starter", "Essentials", ["Metal", "Metalcore"], "Tight modern high gain with a gate.",
  A("Tight Metal", 6.5, 5.5, 5, 6, 6, 6), C("4x12 Modern", 3, 1.5), G(-56, 60), NOREV())
P("Smooth Lead", "Essentials", ["Rock", "Fusion"], "Singing sustain with a little delay behind it.",
  A("Redline Lead", 6.5, 5, 6.5, 5.5, 5), C("4x12 Vintage", 3.5, 2), DLY("Analog", 420, "Off", 3, 5, 2.5), REV("Hall", 4, 20, 5, 1.5), G(-62))
P("Bedroom Lo-Fi", "Essentials", ["Lo-Fi", "Bedroom Pop"], "Wobbly, warm and dusty. Instant lo-fi.",
  A("Glass Clean", 3.5, 5, 5, 4), C("1x12 Open Back", 5, 3), MOD("Chorus", 2, 4, 5), LOFI(1, 0, 4, 3, 3, 10), REV("Room", 4, 10, 4, 2.5), G(-72))
P("Ambient Wash", "Essentials", ["Ambient", "Post-Rock"], "Clean notes become clouds.",
  A("Glass Clean", 3.5, 5, 5, 5.5), C("2x12 American", 4, 3), DLY("Ping Pong", 450, "1/4 dotted", 5, 5, 3.5), REV("Shimmer", 6, 40, 6, 4.5), G(-72))
P("Bass Basics", "Essentials", ["Bass", "Rock", "Pop"], "A solid, round bass tone that sits in a mix.",
  A("Bass Station", 4, 6, 5, 5), C("4x10 Bass", 3, 2, 30, 9000), COMP(5, 4), NOREV(), G(-70))
P("Plain DI (No Effects)", "Essentials", ["Utility"], "Your guitar with only the noise gate and hum filter. Useful for recording a clean track to re-amp later.",
  dict(amp_on=False, cab_on=False), NOREV(), G(-70))

# ---------------------------------------------------------------- Clean
P("Glass Chime", "Clean", ["Pop", "Worship", "Indie"], "Bright, glassy clean with compression and plate.",
  A("Glass Clean", 4, 4.5, 4.5, 6.5, 6), C("2x12 American", 2.5, 2), COMP(5, 5), REV("Plate", 4.5, 18, 6, 2.5), G(-70))
P("Jazz Box", "Clean", ["Jazz"], "Dark, round and warm for chords and lines.",
  A("Glass Clean", 3, 6, 5.5, 3, 4), C("1x12 Open Back", 6, 3, 60, 7000), REV("Room", 3.5, 10, 3.5, 1.4), G(-74))
P("Funk Clean Machine", "Clean", ["Funk", "Disco"], "Tight, snappy, compressed. Made for 16th-note strumming.",
  A("Glass Clean", 4, 4, 5.5, 6, 6), C("2x12 American", 2, 1), COMP(7, 2, 5), REV("Room", 2, 5, 5, 0.8), G(-70))
P("Neo-Soul Silk", "Clean", ["Neo-Soul", "R&B"], "Soft, warm cleans with gentle chorus.",
  A("Glass Clean", 3.5, 5.5, 5, 4.5, 4.5), C("1x12 Open Back", 4, 2.5), COMP(4, 6), MOD("Chorus", 2.5, 3, 4), REV("Plate", 4, 20, 4.5, 2.2), G(-72))
P("Spring Surf", "Clean", ["Surf", "Rockabilly"], "Drenched in drippy spring reverb.",
  A("Glass Clean", 4.5, 5, 4.5, 6.5, 6), C("2x12 American", 3, 2), REV("Spring", 6, 0, 6, 5), G(-68))
P("Country Pickin'", "Clean", ["Country"], "Twangy and compressed with a slapback echo.",
  A("Glass Clean", 4.5, 4.5, 5, 6.5, 6), C("2x12 American", 2.5, 2), COMP(6, 3, 5), DLY("Tape", 120, "Off", 1, 5, 2.5), REV("Spring", 3, 0, 5, 1.2), G(-68))
P("Chorus Dreams", "Clean", ["80s", "New Wave", "Pop"], "Lush 80s chorus clean.",
  A("Glass Clean", 4, 5, 4.5, 6, 5.5), C("2x12 American", 3, 2), COMP(4, 5), MOD("Chorus", 3.5, 6, 7), DLY("Digital", 380, "Off", 3, 6, 2), REV("Hall", 5, 25, 5, 2.4), G(-70), double=3)
P("Worship Swells", "Clean", ["Worship", "Ambient"], "Dotted-eighth delay and big hall for building songs.",
  A("Chime Top", 3.5, 5, 5, 5.5, 5), C("2x12 Blue", 3, 2), COMP(3, 6), DLY("Digital", 400, "1/8 dotted", 4.5, 6, 3.5), REV("Hall", 6, 30, 5.5, 3.2), G(-70))
P("Chime Jangle", "Clean", ["Jangle Pop", "Indie", "Britpop"], "Shimmery, jangly chords.",
  A("Chime Top", 4.5, 4.5, 5, 6.5, 6), C("2x12 Blue", 2.5, 2), COMP(4, 5), REV("Room", 4, 12, 6, 2), G(-68))
P("Clean Arpeggio Space", "Clean", ["Post-Rock", "Math Rock"], "Clear and detailed with stereo delay for arpeggios.",
  A("Glass Clean", 3.5, 5, 5, 6, 5.5), C("2x12 American", 3, 2), COMP(3, 6), DLY("Ping Pong", 350, "1/8", 3.5, 6, 2.5), REV("Plate", 4, 15, 5.5, 2.3), G(-72))
P("Tremolo Noir", "Clean", ["Blues", "Cinematic", "Indie"], "Moody pulsing tremolo through a spring.",
  A("Glass Clean", 4.5, 5.5, 5, 5, 5), C("1x12 Open Back", 4, 2.5), MOD("Tremolo", 5, 6, 3), REV("Spring", 4.5, 0, 5, 2.5), G(-70))

# ---------------------------------------------------------------- Crunch / Rock
P("Tweed Breakup", "Crunch", ["Blues", "Classic Rock", "Americana"], "Loose, hairy, touch-sensitive breakup.",
  A("Tweed Edge", 6, 5, 5.5, 5.5), C("1x12 Open Back", 3.5, 2.5), REV("Spring", 3, 0, 5, 1.6), G(-64))
P("Classic Rock Crunch", "Crunch", ["Classic Rock", "Hard Rock"], "Open chords and big riffs, 70s style.",
  A("Brit 68", 7, 5, 6.5, 6, 5.5), C("4x12 Vintage", 3, 2), REV("Room", 3, 8, 5, 1.4), G(-62))
P("British Invasion", "Crunch", ["60s", "Britpop", "Rock"], "Chimey, raspy amp-pushed crunch.",
  A("Chime Top", 7, 4.5, 5, 6.5, 6), C("2x12 Blue", 3, 2), REV("Room", 3, 8, 5, 1.5), G(-64))
P("Indie Garage", "Crunch", ["Indie", "Garage Rock"], "Raw, loud, a little messy, in a good way.",
  A("Tweed Edge", 7.5, 5, 6, 6), C("2x12 Blue", 3.5, 2.5), D("Blues Drive", 5, 5.5, 5), REV("Room", 3, 5, 5, 1.2), G(-62))
P("Pop Punk Power", "Rock", ["Pop Punk", "Punk"], "Bright, tight and loud power chords.",
  A("Brit 81", 6.5, 5.5, 6, 6.5, 6), C("4x12 Vintage", 2.5, 1.5), G(-60), NOREV(), double=6)
P("Punk Downstrokes", "Rock", ["Punk"], "Buzzsaw crunch for fast downstrokes.",
  A("Brit 81", 7, 5, 6.5, 6.5, 6), C("4x12 Vintage", 2.5, 1.5), G(-60), NOREV())
P("Alt Rock Wall", "Rock", ["Alternative", "Grunge", "Post-Grunge"], "Big doubled rhythm wall.",
  A("Modern High Gain", 5, 6, 5, 5.5, 5.5, 6), C("4x12 Modern", 3, 2), G(-60), REV("Room", 2.5, 5, 5, 0.9), double=7)
P("Hard Rock Anthem", "Rock", ["Hard Rock", "Arena Rock"], "Hot-rodded British rhythm with bite.",
  A("Redline Lead", 5.5, 5.5, 6.5, 6, 5.5), C("4x12 Vintage", 3, 2), G(-62), REV("Room", 3, 8, 5, 1.2))
P("Stoner Doom", "Rock", ["Stoner Rock", "Doom", "Sludge"], "Huge, fuzzy and slow. Tune low.",
  A("Brit 68", 7, 7, 5, 4.5, 4, 7), C("4x12 Vintage", 4, 2.5), D("Big Fuzz", 7, 4, 5), G(-58), REV("Room", 3, 5, 4, 1.2))
P("Shoegaze Haze", "Rock", ["Shoegaze", "Dream Pop"], "A wall of fuzz, vibrato and reverb.",
  A("Tweed Edge", 6, 5, 5, 5), C("2x12 Blue", 4.5, 3), D("Big Fuzz", 6, 5, 4.5), MOD("Vibrato", 2.5, 4, 4), REV("Cloud", 5, 40, 4.5, 4.5), G(-60), double=5)
P("Post-Rock Crescendo", "Rock", ["Post-Rock"], "From quiet to huge: drive, delay and a giant hall.",
  A("Chime Top", 6, 5, 5, 6), C("2x12 Blue", 3, 2), D("Blues Drive", 5, 5, 5), DLY("Digital", 450, "1/4 dotted", 5, 5.5, 3.5), REV("Hall", 7.5, 40, 5, 3.5), G(-62))
P("Garage Fuzz", "Rock", ["Garage Rock", "Blues Rock"], "Gnarly fuzz straight into a cranked combo.",
  A("Tweed Edge", 5, 5, 5, 5.5), C("1x12 Open Back", 3.5, 2), D("Big Fuzz", 8, 6, 4.5), G(-60), REV("Room", 2.5, 5, 5, 1))

# ---------------------------------------------------------------- Blues
P("Texas Blues", "Blues", ["Blues", "Texas Blues"], "Pushed clean amp hit with a green overdrive.",
  A("Glass Clean", 6.5, 5.5, 5.5, 6, 5.5, 5, 7), C("4x12 Vintage", 3, 2), D("Green Drive", 3, 5.5, 6.5), REV("Spring", 3.5, 0, 5, 1.6), G(-64))
P("Chicago Grit", "Blues", ["Blues", "Chicago Blues"], "Raw and gritty small-amp blues.",
  A("Tweed Edge", 6.5, 5, 6, 5), C("1x12 Open Back", 3.5, 2.5), REV("Room", 3, 5, 4.5, 1.5), G(-64))
P("Slow Blues Lead", "Blues", ["Blues"], "Creamy, singing lead for slow blues.",
  A("Brit 68", 6.5, 5, 7, 5, 5), C("4x12 Vintage", 4, 2), D("Green Drive", 3, 5, 6), DLY("Analog", 380, "Off", 2.5, 4.5, 1.8), REV("Hall", 4, 20, 4.5, 1.6), G(-64))
P("Blues Rock Burn", "Blues", ["Blues Rock"], "Hot blues rock with a fuzz edge.",
  A("Brit 68", 6, 5, 6, 5.5), C("4x12 Vintage", 3, 2), D("Blues Drive", 6, 5, 6), REV("Room", 3, 8, 5, 1.4), G(-62))

# ---------------------------------------------------------------- Metal
P("Thrash Machine", "Metal", ["Thrash", "Metal"], "Scooped, aggressive and fast.",
  A("Modern High Gain", 6.5, 6.5, 3.5, 6.5, 6.5, 6), C("4x12 Modern", 2.5, 1.5), D("Green Drive", 1.5, 6, 7), G(-54, 50), NOREV(), double=6)
P("Djent Precision", "Metal", ["Djent", "Progressive Metal"], "Ultra tight with a boosted front end. Great for low tunings.",
  A("Tight Metal", 6.5, 5, 6, 6.5, 6.5, 5.5), C("4x12 Modern", 2.5, 1, 80, 10000), D("Green Drive", 0.5, 6, 7), G(-50, 40), NOREV(), double=7)
P("Metalcore Breakdown", "Metal", ["Metalcore", "Deathcore"], "Heavy, punchy and gated for breakdowns.",
  A("Tight Metal", 7, 6, 5, 6, 6.5, 7), C("4x12 Modern", 3, 1.5), D("Green Drive", 0.5, 6, 7), G(-48, 35), NOREV(), double=7)
P("Death Metal Buzz", "Metal", ["Death Metal"], "Saw-toothed and brutal.",
  A("Modern High Gain", 7, 6, 4, 6.5, 6), C("4x12 Modern", 2.5, 1.5), D("Metal Box", 6, 6, 5), G(-50, 40), NOREV())
P("Black Metal Frost", "Metal", ["Black Metal"], "Thin, cold and trebly with a dark hall.",
  A("Brit 81", 8.5, 3.5, 4.5, 7.5, 7, 3), C("4x12 Vintage", 2, 2), G(-54), REV("Hall", 6, 30, 3.5, 2.5))
P("Nu-Metal Bounce", "Metal", ["Nu-Metal"], "Fat, scooped and bouncy. Tune way down.",
  A("Modern High Gain", 6, 7, 3.5, 5.5, 5.5, 7), C("4x12 Modern", 3, 1.5), G(-52, 45), NOREV(), double=7)
P("Sludge Swamp", "Metal", ["Sludge", "Doom"], "Thick, murky and loud.",
  A("Brit 81", 8, 7, 5.5, 4.5, 4, 7), C("4x12 Vintage", 4.5, 2.5), D("Big Fuzz", 5, 4, 5), G(-54), REV("Room", 2.5, 5, 4, 0.8))
P("Prog Metal Lead", "Metal", ["Progressive Metal", "Shred"], "Fluid, articulate lead with delay.",
  A("Redline Lead", 7, 5, 6.5, 6, 6), C("4x12 Modern", 3, 2), D("Green Drive", 1, 6, 7), DLY("Digital", 400, "1/4", 3, 5.5, 2.5), REV("Hall", 4.5, 20, 5, 1.6), G(-56))
P("Shred Lead", "Metal", ["Shred", "Neo-Classical"], "Saturated and smooth, every note sings.",
  A("Redline Lead", 8, 5, 7, 6, 6), C("4x12 Vintage", 3, 2), D("Green Drive", 1.5, 6, 7), DLY("Analog", 380, "Off", 3, 5, 2.2), REV("Hall", 4.5, 20, 5, 1.6), G(-58))
P("Folk Metal Fire", "Metal", ["Folk Metal", "Metal"], "Big, mid-forward heavy rhythm that leaves room for folk instruments.",
  A("Tight Metal", 6, 5.5, 6.5, 6, 6, 6), C("4x12 Modern", 3, 1.5), G(-52, 45), REV("Room", 2.5, 5, 5, 0.8), double=6)

# ---------------------------------------------------------------- Lead / Solo
P("80s Arena Lead", "Lead", ["80s", "Hair Metal"], "Big, chorused, delayed 80s solo tone.",
  A("Redline Lead", 7, 5, 6, 6, 6), C("4x12 Vintage", 3, 2), MOD("Chorus", 3, 4, 4), DLY("Digital", 420, "Off", 3.5, 6, 3), REV("Hall", 5, 25, 5.5, 2.2), G(-60))
P("Fusion Lead", "Lead", ["Fusion", "Jazz Fusion"], "Smooth, compressed, vocal lead.",
  A("Redline Lead", 6, 5, 6.5, 5, 5), C("4x12 Vintage", 4, 2), COMP(4, 6), DLY("Digital", 360, "Off", 3, 5, 2.5), REV("Plate", 4, 20, 5, 1.8), G(-62))
P("Wah Solo", "Lead", ["Rock", "Funk Rock"], "Expressive auto-wah lead.",
  A("Brit 81", 7, 5, 6, 5.5), C("4x12 Vintage", 3, 2), WAH("Auto", 5, 7, 6), DLY("Analog", 380, "Off", 2, 4.5, 1.6), G(-60))
P("Octave Fuzz Lead", "Lead", ["Psychedelic", "Rock"], "Ringing octave-up fuzz lead.",
  A("Brit 68", 6, 5, 6, 5.5), C("4x12 Vintage", 3.5, 2), PITCH("Octave Up", 12, 5), D("Big Fuzz", 7, 6, 4.5), DLY("Analog", 400, "Off", 3, 4.5, 2), G(-60))
P("Floating Ballad Lead", "Lead", ["Rock", "Ballad"], "Long sustain, lush delay, big hall.",
  A("Redline Lead", 6, 5, 6.5, 5, 5), C("4x12 Vintage", 4, 2), DLY("Analog", 520, "Off", 4, 4.5, 3), REV("Hall", 6.5, 35, 5, 2.6), G(-62))

# ---------------------------------------------------------------- Funk / R&B / Pop
P("Disco Strut", "Funk", ["Disco", "Funk"], "Bright, choppy, compressed rhythm.",
  A("Glass Clean", 4.5, 4, 5.5, 6.5, 6.5), C("2x12 American", 2, 1), COMP(7, 2, 5.5), REV("Room", 2, 5, 5, 0.6), G(-68))
P("Envelope Funk", "Funk", ["Funk"], "Quacky auto-wah funk.",
  A("Glass Clean", 4.5, 5, 5, 5.5), C("2x12 American", 3, 2), COMP(6, 3), WAH("Auto", 5, 6, 7), REV("Room", 2, 5, 5, 0.8), G(-68))
P("Rocked Wah Soul", "Funk", ["Funk", "Soul"], "The classic 60s wah, rocked by foot: half-down for a throaty rhythm, heel to toe for the 'wacka' strum.",
  A("Glass Clean", gain=4.5, treble=5.5), C("2x12 American"), COMP(sustain=6, attack=3), WAH("Classic", pos=4.5),
  REV("Room", decay=2, pre=5, mix=0.8), G(-68))
P("R&B Ballad Clean", "Funk", ["R&B", "Soul"], "Warm, smooth, with a gentle vibe.",
  A("Glass Clean", 3.5, 5.5, 5, 5), C("1x12 Open Back", 4, 2.5), COMP(4, 6), MOD("Vibe", 3, 4, 4), REV("Plate", 4.5, 20, 4.5, 2.4), G(-72))
P("Pop Radio Clean", "Funk", ["Pop"], "Modern, compressed, wide and bright.",
  A("Glass Clean", 4, 4.5, 5, 6, 6), C("2x12 American", 2.5, 2), COMP(5, 5), REV("Plate", 3.5, 15, 6, 1.8), G(-70), double=5)
P("Afrobeat Highlife", "Funk", ["Afrobeat", "Highlife"], "Bright, dry, clean and percussive for interlocking lines.",
  A("Glass Clean", 4, 4, 5.5, 6.5, 6), C("1x12 Open Back", 2.5, 1.5), COMP(5, 3), REV("Room", 2, 5, 5, 0.8), G(-70))
P("Reggae Skank", "Funk", ["Reggae", "Ska"], "Short, bright upstrokes with a touch of spring.",
  A("Glass Clean", 4, 3.5, 5.5, 6.5, 6), C("1x12 Open Back", 2.5, 1.5), REV("Spring", 3.5, 0, 6, 1.6), G(-70))
P("Gospel Chops", "Funk", ["Gospel", "Soul"], "Round, warm and articulate for church chops.",
  A("Glass Clean", 4, 5.5, 5.5, 5, 5), C("2x12 American", 3.5, 2), COMP(5, 5), REV("Hall", 4, 20, 5, 2), G(-70))

# ---------------------------------------------------------------- Ambient / Experimental
P("Shimmer Cathedral", "Ambient", ["Ambient", "Worship"], "Octave shimmer tails that go on forever.",
  A("Glass Clean", 3.5, 5, 5, 5.5), C("2x12 American", 4, 3), COMP(3, 6), REV("Shimmer", 7.5, 50, 6, 5), DLY("Digital", 500, "Off", 4, 5, 2.5), G(-74))
P("Cloud Nine", "Ambient", ["Ambient", "Drone"], "Infinite, slowly moving reverb pad.",
  A("Glass Clean", 3, 5, 5, 5), C("2x12 American", 4.5, 3), MOD("Chorus", 1, 4, 4), REV("Cloud", 7, 60, 5, 6), G(-74))
P("Underwater Vibe", "Ambient", ["Psychedelic", "Dream Pop"], "Swirling vibe and tape delay.",
  A("Tweed Edge", 4.5, 5, 5, 4.5), C("1x12 Open Back", 4.5, 3), MOD("Vibe", 3.5, 7, 6), DLY("Tape", 420, "Off", 4, 4, 3), REV("Hall", 5, 30, 4.5, 2.5), G(-68))
P("Jet Flanger", "Ambient", ["Psychedelic", "Rock"], "Classic swooshing flanger.",
  A("Brit 68", 5, 5, 5.5, 6), C("4x12 Vintage", 3, 2), MOD("Flanger", 2, 7, 6), REV("Room", 3, 8, 5, 1.4), G(-64))
P("Phase Funk", "Ambient", ["Funk", "Psychedelic"], "Slow, liquid phaser.",
  A("Glass Clean", 4.5, 5, 5, 5.5), C("2x12 American", 3, 2), MOD("Phaser", 2.5, 6, 5), REV("Room", 2.5, 8, 5, 1.2), G(-70))
P("Sub Octave Synth", "Ambient", ["Experimental", "Electronic"], "Octave-down fuzz that sounds like a synth bass.",
  A("Studio Direct", 5, 5, 5, 5), C("1x12 Open Back", 4, 2), PITCH("Sub Octave", -12, 8), D("Big Fuzz", 6, 4, 4), G(-60))
P("Radio From Another Room", "Ambient", ["Lo-Fi", "Experimental"], "Thin, crackly, AM radio.",
  A("Tweed Edge", 5, 5, 5, 5), C("1x8 Tiny Combo", 4, 2), LOFI(3, 3, 3, 6, 8, 10), REV("Room", 3, 5, 4, 1.5), G(-66))
P("Bit Crusher Chaos", "Ambient", ["Experimental", "Electronic"], "8-bit video game crunch.",
  A("Brit 68", 5, 5, 5, 6), C("1x8 Tiny Combo", 3, 2), LOFI(7, 7, 0, 0, 2, 10), G(-62), NOREV())

# ---------------------------------------------------------------- Lo-Fi
P("Cassette Memories", "Lo-Fi", ["Lo-Fi", "Chillhop"], "Warbly tape and soft hiss.",
  A("Glass Clean", 3.5, 5.5, 5, 4), C("1x12 Open Back", 5, 3), COMP(4, 6), LOFI(0, 0, 6, 4, 4, 10), REV("Room", 4, 10, 4, 2.2), G(-74))
P("Vinyl Chillhop", "Lo-Fi", ["Chillhop", "Lo-Fi Hip-Hop"], "Mellow jazzy chords with vinyl crackle.",
  A("Glass Clean", 3, 6, 5, 3.5, 4), C("1x12 Open Back", 6, 3, 80, 6000), COMP(4, 6), MOD("Chorus", 1.5, 3, 3), LOFI(0.5, 1, 3, 6, 5, 10), REV("Plate", 4, 15, 4, 2.2), G(-74))
P("Bedroom Indie Wobble", "Lo-Fi", ["Bedroom Pop", "Indie"], "Detuned, wobbly chorus clean.",
  A("Glass Clean", 4, 5, 5, 5), C("1x12 Open Back", 4, 2.5), MOD("Vibrato", 2.5, 3, 3), LOFI(0, 0, 3, 2, 2, 10), REV("Room", 4, 10, 4.5, 2.4), G(-72))
P("VHS Sunset", "Lo-Fi", ["Synthwave", "Lo-Fi"], "Retro, chorused, tape-worn.",
  A("Glass Clean", 4, 5, 5, 6), C("2x12 American", 3, 2), MOD("Chorus", 3, 6, 7), LOFI(1, 1, 5, 3, 3, 10), DLY("Tape", 380, "Off", 3.5, 4, 2.5), REV("Hall", 5, 25, 5, 2.5), G(-70))

# ---------------------------------------------------------------- Bass
P("Rock Bass Grind", "Bass", ["Bass", "Rock"], "Gritty, punchy rock bass.",
  A("Bass Station", 6.5, 6, 6, 5.5), C("4x10 Bass", 3, 2, 30, 9000), COMP(5, 4), NOREV(), G(-66))
P("Funk Slap Bass", "Bass", ["Bass", "Funk"], "Scooped and snappy for slap.",
  A("Bass Station", 4, 7, 3.5, 7), C("4x10 Bass", 2.5, 1.5, 30, 12000), COMP(7, 2), NOREV(), G(-70))
P("Motown Bass", "Bass", ["Bass", "Soul", "Motown"], "Deep, thumpy, old-school.",
  A("Bass Station", 3.5, 6.5, 5, 3), C("1x15 Bass", 4, 2.5, 30, 5000), COMP(4, 5), NOREV(), G(-72))
P("Metal Bass", "Bass", ["Bass", "Metal"], "Clanky, driven bass that cuts through guitars.",
  A("Bass Station", 7, 6, 6.5, 6.5), C("4x10 Bass", 2.5, 1.5, 35, 10000), D("Metal Box", 3, 6, 5), COMP(5, 4), G(-60), NOREV())
P("Fuzz Bass", "Bass", ["Bass", "Stoner Rock"], "Fuzzed-out bass with clean lows underneath.",
  A("Bass Station", 5, 6, 5, 5), C("4x10 Bass", 3, 2, 30, 9000), D("Big Fuzz", 6, 5, 4), G(-62), NOREV())
P("Synth Bass Octave", "Bass", ["Bass", "Electronic"], "Sub-octave synth-like bass.",
  A("Bass Station", 4, 6, 5, 5), C("1x15 Bass", 3.5, 2, 30, 6000), PITCH("Sub Octave", -12, 6), COMP(6, 4), G(-64), NOREV())

# ---------------------------------------------------------------- Desi / Indian
P("Filmi Rock Lead", "Rock", ["Bollywood", "Filmi Rock"], "Big, melodic lead for Bollywood rock songs.",
  A("Redline Lead", 6.5, 5, 6.5, 5.5), C("4x12 Vintage", 3.5, 2), DLY("Digital", 420, "Off", 3.5, 5.5, 2.8), REV("Hall", 5, 25, 5, 2.2), G(-60))
P("Desi Indie Strum", "Clean", ["Desi Indie", "Indie"], "Warm, open strumming tone for indie songs.",
  A("Chime Top", 3.5, 5.5, 5, 5), C("1x12 Open Back", 4, 2.5), COMP(4, 5), REV("Room", 4, 12, 4.5, 2.2), G(-70))
P("Sufi Rock Drive", "Rock", ["Sufi Rock", "Fusion"], "Soaring overdriven tone for melodic fusion lines.",
  A("Brit 68", 6.5, 5, 6.5, 5.5), C("4x12 Vintage", 3.5, 2), D("Green Drive", 2.5, 5.5, 6.5), DLY("Analog", 450, "Off", 3, 4.5, 2.2), REV("Hall", 5, 25, 5, 2), G(-62))
P("Raga Drone Pad", "Ambient", ["Indian Classical Fusion", "Ambient"], "Sustained, shimmering pad for drones and slides.",
  A("Glass Clean", 3.5, 5, 5, 5), C("2x12 American", 4, 3), COMP(5, 6), REV("Shimmer", 7, 40, 5, 4.5), DLY("Digital", 600, "Off", 4, 5, 2.5), G(-74))

# Indian rock and indie pack (0.1.5): the sounds of the scenes, named for styles and cities only
P("Bangalore Thrash", "Metal", ["Indian Metal", "Thrash"], "Fast, dry thrash rhythm from the old-school metal scene. Tight lows, bright pick.",
  A("Brit 81", 7.5, 5.5, 6, 6.5, 6), C("4x12 Vintage", 3, 1.5), D("Green Drive", 1.5, 6, 7), G(-56, 50), NOREV())
P("Shillong Blues Rock", "Blues", ["Indian Blues", "Blues Rock"], "Warm, singing blues-rock crunch that cleans up when you play softly.",
  A("Tweed Edge", 6.5, 5, 6, 5.5), C("1x12 Open Back", 3.5, 2), D("Green Drive", 3, 5, 6), REV("Spring", 4, 8, 5, 1.8), G(-64))
P("Bangla Band Jangle", "Crunch", ["Bangla Rock", "Indian Rock"], "Bright, jangly edge-of-breakup strum for band songs with big choruses.",
  A("Chime Top", 5.5, 4.5, 5.5, 6), C("2x12 Blue", 3, 2), COMP(4, 5), MOD("Chorus", 2, 3, 2.5), REV("Room", 4, 12, 5, 2), G(-66))
P("Carnatic Prog Lead", "Lead", ["Indian Prog", "Carnatic Fusion"], "Smooth, articulate lead for fast gamakas and slides, with a short echo.",
  A("Redline Lead", 6.5, 5, 6.5, 5.5, 5.5), C("4x12 Vintage", 3.5, 2), D("Clean Boost", 3, 5, 6), DLY("Digital", 320, "Off", 2.5, 5, 1.8), REV("Plate", 4, 15, 5, 1.6), G(-60))
P("Desi Djent", "Metal", ["Indian Prog Metal", "Djent"], "Tight, percussive low-tuned chug with a boosted front end and clear chords on top.",
  A("Tight Metal", 7, 6, 5, 6.5, 6, 6.5), C("4x12 Modern", 2.5, 1.5), D("Metal Box", 1, 5.5, 5), G(-54, 45), NOREV(), double=6)
P("Post-Rock Swells", "Ambient", ["Indian Post-Rock", "Post-Rock"], "Clean, wide and long for tremolo-picked builds and swelling chords.",
  A("Glass Clean", 4, 5, 5, 5.5), C("2x12 American", 3.5, 2.5), COMP(4, 6), DLY("Tape", 480, "Off", 4.5, 4.5, 3), REV("Cloud", 7, 30, 5, 4), G(-72))
P("Bedroom Indie Clean", "Clean", ["Desi Indie", "Bedroom Pop"], "Soft, rounded clean with a slow chorus, for quiet songs recorded at home.",
  A("Glass Clean", 3.5, 5.5, 5, 4.5), C("1x12 Open Back", 4.5, 2.5), COMP(4, 5), MOD("Chorus", 1.5, 4, 3), REV("Plate", 3.5, 12, 4, 2), G(-72))
P("Nineties Indipop Crunch", "Crunch", ["Indipop", "Pop Rock"], "Glossy mid crunch with a chorus shimmer, made for the riff before the chorus.",
  A("Brit 68", 5.5, 5, 6, 6), C("2x12 Blue", 3, 2), MOD("Chorus", 2.5, 3.5, 2.5), DLY("Digital", 360, "Off", 2.5, 6, 1.5), REV("Room", 4, 12, 5, 1.8), G(-64))

# ================================================================ Inspired by
# Rock legends
I("Jimi Hendrix", "Purple Fuzz Lead", "Inspired By", ["Psychedelic", "Classic Rock"], "Fuzz into a cranked British stack with a swirling vibe.",
  A("Brit 68", 6.5, 5, 6, 6, 5.5, 5, 7), C("4x12 Vintage", 3, 2), D("Big Fuzz", 7, 6, 4.5), MOD("Vibe", 3.5, 6, 5), REV("Room", 3, 8, 5, 1.4), G(-60))
I("Jimi Hendrix", "Little Wing Clean", "Inspired By", ["Psychedelic", "Blues"], "Edge-of-breakup clean with a vibe.",
  A("Brit 68", 4, 5, 5.5, 6), C("4x12 Vintage", 3, 2), MOD("Vibe", 3, 4, 4), REV("Room", 3.5, 10, 5, 1.8), G(-66))
I("Jimmy Page", "Zeppelin Crunch", "Inspired By", ["Classic Rock", "Hard Rock"], "Treble-boosted British crunch.",
  A("Brit 68", 6.5, 4.5, 6, 6.5, 6), C("4x12 Vintage", 3, 2.5), D("Treble Boost", 4, 5, 5.5), REV("Room", 3.5, 8, 5, 1.8), G(-62))
I("Eric Clapton", "Woman Tone", "Inspired By", ["Blues Rock", "Classic Rock"], "Thick, dark, singing lead with the treble rolled off.",
  A("Brit 68", 8, 6, 7, 2.5, 3.5), C("4x12 Vintage", 5, 2), D("Clean Boost", 4, 3, 5.5), REV("Room", 3, 8, 3.5, 1.4), G(-62))
I("Eric Clapton", "Slowhand Clean", "Inspired By", ["Blues", "Pop Rock"], "Warm, compressed, gently chorused clean.",
  A("Glass Clean", 4.5, 5, 5.5, 5.5), C("2x12 American", 3, 2), COMP(4, 5), MOD("Chorus", 2.5, 3, 4), REV("Plate", 3.5, 15, 5, 1.8), G(-70))
I("Eddie Van Halen", "Brown Sound", "Inspired By", ["Hard Rock", "80s"], "Hot, saggy British stack with a phaser.",
  A("Redline Lead", 6.5, 5, 6, 6, 6, 5, 7), C("4x12 Vintage", 3, 2.5), MOD("Phaser", 3, 5, 3), REV("Plate", 3, 15, 5, 1.6), G(-58))
I("Slash", "Paradise Lead", "Inspired By", ["Hard Rock"], "Warm, mid-heavy hot-rod lead.",
  A("Redline Lead", 6.5, 5.5, 7, 5, 5), C("4x12 Vintage", 3.5, 2), DLY("Analog", 420, "Off", 2.5, 4.5, 1.8), REV("Room", 3, 8, 5, 1.4), G(-60))
I("Slash", "Wah Rock", "Inspired By", ["Hard Rock"], "Hot-rod crunch with a cocked wah.",
  A("Redline Lead", 6, 5.5, 6.5, 5), C("4x12 Vintage", 3.5, 2), WAH("Manual", 6, 5, 6), G(-60))
I("Angus Young", "Schoolboy Crunch", "Inspired By", ["Hard Rock"], "Less gain than you think, all attitude.",
  A("Brit 68", 5.5, 5, 6, 6.5, 6), C("4x12 Vintage", 3, 2), REV("Room", 2.5, 5, 5, 1), G(-62))
I("David Gilmour", "Comfortably Lead", "Inspired By", ["Progressive Rock", "Classic Rock"], "Sustaining fuzz lead, long delay and big hall.",
  A("Glass Clean", 6, 5, 5, 5.5, 5, 5, 7), C("2x12 American", 3.5, 2), D("Big Fuzz", 6, 4.5, 4.5), DLY("Analog", 480, "Off", 4, 4.5, 3), REV("Hall", 6, 30, 5, 2.4), G(-62))
I("David Gilmour", "Shine On Clean", "Inspired By", ["Progressive Rock"], "Compressed, chorused clean with delay.",
  A("Glass Clean", 4, 5, 5, 6), C("2x12 American", 3, 2), COMP(5, 5), MOD("Chorus", 2, 4, 4), DLY("Analog", 440, "Off", 3.5, 5, 2.5), REV("Hall", 5, 25, 5, 2.2), G(-70))
I("Brian May", "Queen Lead", "Inspired By", ["Classic Rock", "Arena Rock"], "Treble-boosted chime amp at full tilt.",
  A("Chime Top", 7.5, 5, 6, 6), C("2x12 Blue", 3, 2), D("Treble Boost", 6, 5, 5.5), DLY("Digital", 400, "Off", 3, 5, 2.4), REV("Hall", 4, 20, 5, 1.8), G(-62))
I("Keith Richards", "Open-G Swagger", "Inspired By", ["Rock and Roll", "Classic Rock"], "Loose, raw tweed breakup.",
  A("Tweed Edge", 6, 5, 5.5, 5.5), C("1x12 Open Back", 3.5, 2.5), REV("Room", 3, 5, 5, 1.4), G(-64))
I("Pete Townshend", "Windmill Power Chords", "Inspired By", ["Classic Rock", "Mod"], "Loud, open British power chords.",
  A("Brit 68", 7, 5, 5.5, 6.5, 6), C("4x12 Vintage", 3, 2), REV("Room", 3, 5, 5, 1.4), G(-62))
I("Mark Knopfler", "Sultans Clean", "Inspired By", ["Classic Rock", "Roots"], "Out-of-phase sparkle, compressed, a touch of delay.",
  A("Glass Clean", 5, 4, 5.5, 6, 6), C("2x12 American", 2.5, 2), COMP(5, 4), DLY("Digital", 300, "Off", 2, 6, 1.5), REV("Room", 3.5, 10, 5, 1.8), EQ(0, -2, -2, 2, 3, 2, 0), G(-68))
I("Stevie Ray Vaughan", "Texas Flood", "Inspired By", ["Texas Blues", "Blues"], "Big clean amp slammed by a green overdrive.",
  A("Glass Clean", 7, 5.5, 6, 6, 5.5, 5, 7.5), C("4x12 Vintage", 3, 2), D("Green Drive", 2, 5.5, 7), REV("Spring", 3, 0, 5, 1.5), G(-64))
I("B.B. King", "Thrill Lead", "Inspired By", ["Blues"], "Clean, singing, sweet single notes.",
  A("Glass Clean", 5.5, 4.5, 6.5, 5), C("1x12 Open Back", 4, 2), COMP(4, 5), REV("Room", 3.5, 10, 4.5, 1.8), G(-68))
I("Chuck Berry", "Rock and Roll Twang", "Inspired By", ["Rock and Roll", "50s"], "Small, slightly dirty combo with slapback.",
  A("Tweed Edge", 5.5, 5, 5.5, 6), C("1x12 Open Back", 3, 2), DLY("Tape", 110, "Off", 0.5, 5, 2.5), G(-66))
I("Carlos Santana", "Sustain Forever", "Inspired By", ["Latin Rock", "Classic Rock"], "Smooth, mid-heavy, endless sustain.",
  A("Redline Lead", 7, 5, 7.5, 4.5, 4.5), C("4x12 Vintage", 4.5, 2), REV("Hall", 4, 20, 4.5, 1.8), G(-62))
I("Tony Iommi", "Sabbath Riff", "Inspired By", ["Heavy Metal", "Doom"], "Treble booster into a cranked British head. Tune down.",
  A("Brit 81", 6.5, 5, 6.5, 5.5, 5), C("4x12 Vintage", 3.5, 2), D("Treble Boost", 6, 5, 5.5), REV("Room", 3, 5, 4.5, 1.2), G(-60))
I("Randy Rhoads", "Crazy Train Lead", "Inspired By", ["Heavy Metal", "Neo-Classical"], "Boosted British lead with a hint of chorus.",
  A("Brit 81", 7.5, 5, 6.5, 6), C("4x12 Vintage", 3, 2), D("Green Drive", 2, 5.5, 7), MOD("Chorus", 3, 2.5, 2.5), DLY("Analog", 380, "Off", 2.5, 4.5, 1.8), G(-58))
I("Zakk Wylde", "Pinch Harmonic Beast", "Inspired By", ["Heavy Metal"], "Big, mid-boosted squealing rhythm.",
  A("Brit 81", 7.5, 6, 7, 6, 6), C("4x12 Vintage", 3, 2), D("Metal Box", 2, 5, 7), G(-56), REV("Room", 2.5, 5, 5, 0.8))
I("Kirk Hammett", "Wah Solo", "Inspired By", ["Thrash", "Heavy Metal"], "Scooped high gain with an auto-wah for solos.",
  A("Modern High Gain", 7, 6, 4, 6.5, 6.5), C("4x12 Modern", 3, 2), WAH("Auto", 5, 7, 6), DLY("Digital", 380, "Off", 2.5, 5, 2), G(-56))
I("James Hetfield", "Downpicked Thrash", "Inspired By", ["Thrash", "Heavy Metal"], "Scooped, tight, crushing rhythm.",
  A("Modern High Gain", 7, 6.5, 3, 6.5, 6.5, 6.5), C("4x12 Modern", 2.5, 1.5), D("Green Drive", 1, 6, 7), G(-54, 50), NOREV(), double=7)
I("Dimebag Darrell", "Cowboys Groove", "Inspired By", ["Groove Metal"], "Razor-sharp, gated, very scooped.",
  A("Tight Metal", 7.5, 7, 2.5, 7, 7, 6), C("4x12 Modern", 2.5, 1.5), D("Clean Boost", 3, 6, 6), G(-48, 30), NOREV())
I("Tom Morello", "Killing Riff", "Inspired By", ["Rap Metal", "Alternative"], "Tight British crunch plus an octave-up pitch effect.",
  A("Brit 81", 6.5, 5, 6, 5.5, 5.5), C("4x12 Vintage", 3, 1.5), PITCH("Shift", 12, 5), G(-58), NOREV())
I("Kerry King", "Reign of Thrash", "Inspired By", ["Thrash"], "Fast, buzzy, aggressive thrash.",
  A("Modern High Gain", 8, 6, 4.5, 7, 7), C("4x12 Modern", 2.5, 1.5), G(-52, 40), NOREV())
I("Max Cavalera", "Roots Groove", "Inspired By", ["Groove Metal", "Thrash"], "Downtuned, fat, grinding rhythm.",
  A("Modern High Gain", 6.5, 7, 4, 5.5, 5.5, 7), C("4x12 Modern", 3, 1.5), D("Metal Box", 3, 5, 6), G(-52, 40), NOREV())
I("Chuck Schuldiner", "Death Melody", "Inspired By", ["Death Metal"], "Clear, aggressive tone for melodic death metal.",
  A("Modern High Gain", 7, 5.5, 5.5, 6.5, 6.5), C("4x12 Modern", 2.5, 1.5), D("Green Drive", 1, 6, 7), G(-52, 40), REV("Room", 2, 5, 5, 0.6))
I("Adam Jones", "Spiral Riff", "Inspired By", ["Progressive Metal", "Alternative Metal"], "Thick, mid-heavy high gain with a phaser option.",
  A("Modern High Gain", 6, 6, 6.5, 5, 5, 6.5), C("4x12 Vintage", 3.5, 2), G(-54, 50), REV("Room", 2.5, 5, 4.5, 0.8))
I("Misha Mansoor", "Djent Machine", "Inspired By", ["Djent", "Progressive Metal"], "Boosted, tight and articulate down to drop G.",
  A("Tight Metal", 6.5, 5, 6, 6.5, 6.5, 5), C("4x12 Modern", 2, 1, 90, 10000), D("Green Drive", 0, 6.5, 7.5), G(-48, 35), NOREV(), double=7)
I("Tosin Abasi", "Eight-String Clean", "Inspired By", ["Progressive Metal", "Fusion"], "Pristine, compressed clean with delay for tapping.",
  A("Glass Clean", 3.5, 5, 5, 6, 6), C("2x12 American", 2.5, 2), COMP(6, 4), DLY("Ping Pong", 380, "1/8", 3, 6, 2), REV("Plate", 4, 15, 5.5, 2), G(-70))
I("Plini", "Floating Lead", "Inspired By", ["Progressive", "Instrumental"], "Smooth boosted lead with lush ambient delay.",
  A("Redline Lead", 6, 5, 6, 5.5, 5.5), C("4x12 Modern", 3.5, 2), D("Green Drive", 1.5, 5.5, 7), DLY("Ping Pong", 450, "1/4", 4.5, 5, 3.5), REV("Hall", 6, 30, 5, 3), G(-60))
I("Tim Henson", "Glossy Clean", "Inspired By", ["Math Rock", "Instrumental"], "Bright, squashed, chorused clean.",
  A("Glass Clean", 4, 4.5, 5, 6.5, 6.5), C("2x12 American", 2.5, 1.5), COMP(7, 3), MOD("Chorus", 3, 4, 5), REV("Plate", 3.5, 15, 6, 2), G(-70), double=4)
I("Mikael Akerfeldt", "Opeth Dynamics", "Inspired By", ["Progressive Metal"], "Dark, warm high gain that still has detail.",
  A("Redline Lead", 6.5, 6, 6, 4.5, 4.5, 6), C("4x12 Vintage", 4, 2), G(-56), REV("Room", 3, 8, 4.5, 1.2))
I("Joe Duplantier", "Gojira Heavy", "Inspired By", ["Progressive Metal", "Death Metal"], "Huge, tight, mid-forward heavy rhythm.",
  A("Tight Metal", 7, 6.5, 6, 5.5, 6, 7), C("4x12 Modern", 3, 1.5), G(-50, 40), NOREV(), double=7)
I("Synyster Gates", "Metalcore Solo", "Inspired By", ["Metalcore", "Heavy Metal"], "Saturated, polished lead with delay.",
  A("Redline Lead", 7.5, 5, 6.5, 6, 6), C("4x12 Modern", 3, 2), D("Green Drive", 1, 6, 7), DLY("Digital", 400, "1/4", 3, 6, 2.5), REV("Hall", 4.5, 20, 5, 1.8), G(-58))
I("Mark Tremonti", "Rock Anthem Rhythm", "Inspired By", ["Hard Rock", "Post-Grunge"], "Thick, saturated modern rock rhythm.",
  A("Modern High Gain", 6, 6, 5.5, 5.5, 5.5, 6), C("4x12 Vintage", 3, 2), G(-56), NOREV(), double=6)
I("Bloodywood", "Desi Folk Metal", "Inspired By", ["Folk Metal", "Nu-Metal"], "Heavy, bouncy and mid-forward to sit with dhol and flute.",
  A("Tight Metal", 6.5, 6, 6, 6, 6, 6.5), C("4x12 Modern", 3, 1.5), D("Green Drive", 0.5, 6, 7), G(-50, 40), NOREV(), double=7)
I("Architects", "Modern Metalcore", "Inspired By", ["Metalcore"], "Crushing, polished modern metalcore rhythm.",
  A("Tight Metal", 7, 6, 5.5, 6, 6.5, 6.5), C("4x12 Modern", 2.5, 1.5), D("Green Drive", 0.5, 6, 7), G(-48, 35), NOREV(), double=7)
I("Bring Me The Horizon", "Arena Metalcore", "Inspired By", ["Metalcore", "Alternative"], "Big modern heavy rhythm with space.",
  A("Modern High Gain", 6.5, 6, 5, 6, 6, 6.5), C("4x12 Modern", 3, 1.5), G(-52, 40), REV("Room", 2, 5, 5, 0.8), double=7)

# Grunge / alternative / indie
I("Kurt Cobain", "Teen Spirit Distortion", "Inspired By", ["Grunge"], "Raw distortion pedal into a clean-ish amp.",
  A("Glass Clean", 5, 5.5, 5, 5.5), C("4x12 Vintage", 3.5, 2), D("Rodent Dist", 6.5, 4.5, 5), G(-60), REV("Room", 2, 5, 5, 0.8))
I("Kurt Cobain", "Come As You Are Clean", "Inspired By", ["Grunge"], "Watery chorus clean.",
  A("Glass Clean", 4, 5, 5, 5.5), C("4x12 Vintage", 3.5, 2), MOD("Chorus", 3, 7, 7), REV("Room", 2.5, 5, 5, 1), G(-66))
I("Jerry Cantrell", "Rooster Grind", "Inspired By", ["Grunge", "Alternative Metal"], "Thick, sludgy, mid-heavy grunge.",
  A("Brit 81", 7, 6, 6.5, 5, 5, 6), C("4x12 Vintage", 3.5, 2), MOD("Chorus", 1.5, 2, 2), G(-58), REV("Room", 2.5, 5, 4.5, 1))
I("Kim Thayil", "Black Hole Drone", "Inspired By", ["Grunge"], "Fuzzy, droney, heavy grunge.",
  A("Brit 68", 6.5, 6, 5.5, 5, 5, 6), C("4x12 Vintage", 3.5, 2), D("Big Fuzz", 5, 5, 5), G(-58), REV("Room", 2.5, 5, 4.5, 1))
I("Billy Corgan", "Fuzz Wall", "Inspired By", ["Alternative", "Grunge"], "Massive, saturated fuzz wall.",
  A("Brit 81", 5, 6, 5, 5.5), C("4x12 Vintage", 3.5, 2), D("Big Fuzz", 8, 5.5, 5), G(-56), NOREV(), double=8)
I("Jack White", "Seven Nation Fuzz", "Inspired By", ["Garage Rock", "Blues Rock"], "Gnarly fuzz plus an octave down for the riff.",
  A("Tweed Edge", 6, 5, 5.5, 5.5), C("4x12 Vintage", 3.5, 2), PITCH("Sub Octave", -12, 6), D("Big Fuzz", 7, 5, 5), G(-58), REV("Room", 2, 5, 5, 0.8))
I("The Edge", "Streets Delay", "Inspired By", ["Rock", "Post-Punk"], "Chiming amp with dotted-eighth delay.",
  A("Chime Top", 5.5, 4.5, 5, 6.5, 6), C("2x12 Blue", 3, 2), COMP(3, 6), DLY("Digital", 400, "1/8 dotted", 4.5, 7, 4), REV("Hall", 4.5, 20, 6, 2), G(-66))
I("Johnny Marr", "Jangle Chorus", "Inspired By", ["Indie", "Post-Punk"], "Sparkling, chorused arpeggios.",
  A("Glass Clean", 4.5, 4.5, 5, 6.5, 6), C("2x12 American", 2.5, 2), COMP(4, 5), MOD("Chorus", 3, 5, 6), REV("Room", 3.5, 10, 5.5, 2), G(-68), double=4)
I("Robert Smith", "Cold Flanger", "Inspired By", ["Post-Punk", "Goth", "New Wave"], "Icy chorus-flange clean with a hall.",
  A("Glass Clean", 4.5, 4.5, 5, 6, 5.5), C("2x12 American", 3, 2), MOD("Flanger", 2, 5, 3), REV("Hall", 5.5, 25, 5, 2.8), G(-68))
I("Kevin Shields", "Glide Guitar", "Inspired By", ["Shoegaze"], "Fuzz, vibrato and a giant reverb wash.",
  A("Tweed Edge", 6, 5, 5, 5), C("2x12 Blue", 4.5, 3), D("Big Fuzz", 7, 5, 4.5), MOD("Vibrato", 3, 6, 5), REV("Cloud", 5.5, 30, 4, 5), G(-58), double=6)
I("Jonny Greenwood", "Radio Glitch", "Inspired By", ["Alternative", "Art Rock"], "Spiky, glitchy, lo-fi attack.",
  A("Brit 68", 6, 5, 6, 6.5), C("2x12 Blue", 3, 2), LOFI(4, 4, 1, 1, 4, 7), G(-60), REV("Room", 2, 5, 5, 1))
I("Josh Homme", "Desert Riff", "Inspired By", ["Stoner Rock", "Desert Rock"], "Midrangey, fuzzy, dry desert rock.",
  A("Tweed Edge", 6.5, 5, 7, 5), C("2x12 Blue", 3.5, 2), D("Big Fuzz", 5, 6.5, 5), PITCH("Octave Up", 12, 2), G(-58), NOREV())
I("John Frusciante", "Funky Clean", "Inspired By", ["Funk Rock", "Alternative"], "Thin, bright, dry funk clean.",
  A("Brit 68", 3, 4.5, 5, 6.5, 6), C("4x12 Vintage", 2.5, 2), REV("Room", 2, 5, 5, 0.8), G(-68))
I("John Frusciante", "Fuzz Solo", "Inspired By", ["Funk Rock", "Alternative"], "Big fuzz lead with tape echo.",
  A("Brit 68", 5, 5, 5.5, 6), C("4x12 Vintage", 3, 2), D("Big Fuzz", 6.5, 6, 4.5), DLY("Tape", 380, "Off", 3, 4, 2.2), G(-60))
I("Tom DeLonge", "Pop Punk Bright", "Inspired By", ["Pop Punk"], "Bright, tight, doubled pop punk.",
  A("Brit 81", 6.5, 5, 6, 7, 6.5), C("4x12 Vintage", 2.5, 1.5), G(-58), NOREV(), double=7)
I("Billie Joe Armstrong", "Basket Crunch", "Inspired By", ["Pop Punk", "Punk"], "Hot, crunchy British punk rhythm.",
  A("Brit 81", 7, 5.5, 6.5, 6, 6), C("4x12 Vintage", 3, 1.5), G(-58), NOREV(), double=6)
I("Alex Turner", "Arctic Fuzz", "Inspired By", ["Indie Rock"], "Gritty fuzz riffs, a little lo-fi.",
  A("Tweed Edge", 6, 5, 6, 5.5), C("2x12 Blue", 3.5, 2), D("Big Fuzz", 5.5, 5.5, 5), G(-60), REV("Room", 2.5, 5, 5, 1))
I("Matt Bellamy", "Muse Octave Fuzz", "Inspired By", ["Alternative", "Arena Rock"], "Synthy octave fuzz riff tone.",
  A("Brit 81", 6, 5.5, 5.5, 6), C("4x12 Vintage", 3, 2), PITCH("Sub Octave", -12, 5), D("Big Fuzz", 7, 6, 5), G(-56), NOREV())
I("Dan Auerbach", "Black Keys Grit", "Inspired By", ["Blues Rock", "Garage Rock"], "Fuzzy, dirty blues rock.",
  A("Tweed Edge", 6.5, 5.5, 6, 5), C("1x12 Open Back", 3.5, 2), D("Big Fuzz", 5, 5, 5), REV("Room", 3, 5, 4.5, 1.2), G(-60))
I("Rivers Cuomo", "Buddy Holly Fuzz", "Inspired By", ["Alternative", "Power Pop"], "Thick, saturated power-pop fuzz.",
  A("Brit 81", 5.5, 5.5, 6, 5.5), C("4x12 Vintage", 3.5, 2), D("Big Fuzz", 6, 5, 5), G(-58), NOREV(), double=6)
I("Mac DeMarco", "Salad Days Wobble", "Inspired By", ["Indie", "Lo-Fi"], "Wobbly, chorused, slightly off clean.",
  A("Glass Clean", 4, 5, 5, 5.5), C("1x12 Open Back", 3.5, 2), MOD("Vibrato", 3, 4, 3), LOFI(0, 0, 3, 1, 1, 10), REV("Room", 3, 8, 5, 1.8), G(-70))
I("Kevin Parker", "Tame Impala Phase", "Inspired By", ["Psychedelic", "Indie"], "Fuzzy, phased, psychedelic.",
  A("Tweed Edge", 5.5, 5, 5, 5.5), C("2x12 Blue", 3.5, 2), D("Big Fuzz", 5.5, 5, 4.5), MOD("Phaser", 3, 6, 5), DLY("Tape", 420, "Off", 3, 4, 2), REV("Room", 3, 8, 5, 1.6), G(-62))
I("Mark Speer", "Khruangbin Groove", "Inspired By", ["Psychedelic Soul", "Funk"], "Warm clean with vibe and spring.",
  A("Glass Clean", 4, 5.5, 5.5, 5), C("1x12 Open Back", 4, 2.5), MOD("Vibe", 3, 4, 4), REV("Spring", 4, 0, 4.5, 2.2), G(-70))
I("Steve Lacy", "Bedroom Funk", "Inspired By", ["Lo-Fi", "R&B"], "Compressed, slightly lo-fi funk clean.",
  A("Glass Clean", 4, 5, 5, 5), C("1x8 Tiny Combo", 3.5, 2), COMP(6, 3), LOFI(0.5, 0, 2, 1, 2, 10), REV("Room", 2.5, 5, 5, 1), G(-70))
I("Mk.gee", "Hazy Chorus", "Inspired By", ["Alternative R&B", "Lo-Fi"], "Washed-out, chorus-heavy, tape-worn clean.",
  A("Glass Clean", 4.5, 5, 5, 5.5), C("1x12 Open Back", 4, 2.5), COMP(6, 3), MOD("Chorus", 2.5, 7, 8), LOFI(1, 0, 4, 2, 3, 10), REV("Plate", 4, 15, 4.5, 2.2), G(-70))

# Funk, R&B, pop, soul
I("Nile Rodgers", "Chic Chop", "Inspired By", ["Disco", "Funk"], "Bright, compressed, ultra-clean rhythm.",
  A("Glass Clean", 4, 4, 5.5, 6.5, 6.5), C("2x12 American", 2, 1), COMP(7, 2, 5.5), REV("Room", 1.5, 5, 5, 0.5), G(-70))
I("Cory Wong", "Clean Funk Comp", "Inspired By", ["Funk"], "Squeezed, sparkling single-coil funk.",
  A("Glass Clean", 4, 4, 5, 6.5, 6.5), C("2x12 American", 2, 1), COMP(8, 2, 5.5), REV("Room", 1.5, 5, 5, 0.5), G(-70))
I("Prince", "Purple Lead", "Inspired By", ["Funk Rock", "Pop"], "Screaming, chorused lead with big reverb.",
  A("Redline Lead", 7, 5, 6, 6), C("4x12 Vintage", 3, 2), D("Green Drive", 2, 6, 6.5), MOD("Chorus", 3, 3, 3), DLY("Digital", 400, "Off", 3, 6, 2.2), REV("Hall", 5, 25, 5.5, 2.2), G(-60))
I("John Mayer", "Gravity Clean", "Inspired By", ["Blues", "Pop"], "Big clean with a touch of green drive.",
  A("Glass Clean", 5.5, 5.5, 5, 6, 5.5), C("2x12 American", 3, 2), D("Green Drive", 2, 5, 6), REV("Spring", 3.5, 0, 5, 1.8), G(-66))
I("John Mayer", "Slow Dancing Clean", "Inspired By", ["Pop", "Soul"], "Glassy, compressed clean with plate.",
  A("Glass Clean", 4, 5, 5, 6), C("2x12 American", 3, 2), COMP(4, 5), REV("Plate", 4, 15, 5.5, 2.2), G(-70))
I("Mateus Asato", "Neo-Soul Sparkle", "Inspired By", ["Neo-Soul", "R&B"], "Silky, compressed clean with lush reverb.",
  A("Glass Clean", 4, 5, 5, 6, 5.5), C("2x12 American", 3, 2), COMP(5, 4), MOD("Chorus", 2, 2.5, 3), REV("Plate", 5, 20, 5, 2.6), G(-70))
I("Tom Misch", "Jazzy Groove", "Inspired By", ["Neo-Soul", "Jazz"], "Warm, round, slightly compressed.",
  A("Glass Clean", 3.5, 5.5, 5, 4.5, 4.5), C("1x12 Open Back", 4.5, 2.5), COMP(4, 5), REV("Room", 3.5, 10, 4.5, 1.8), G(-72))
I("Ichika Nito", "Ethereal Harmonics", "Inspired By", ["Instrumental", "Ambient"], "Pristine clean and a deep shimmering reverb.",
  A("Glass Clean", 3.5, 5, 5, 6), C("2x12 American", 3, 2.5), COMP(4, 6), REV("Shimmer", 6, 30, 5.5, 3.5), G(-74))
I("Jimmy Nolen", "Chicken Scratch", "Inspired By", ["Funk", "Soul"], "Thin, bright, percussive funk rhythm.",
  A("Glass Clean", 4, 3.5, 5.5, 7, 6.5), C("1x12 Open Back", 2, 1), COMP(6, 2), REV("Room", 1.5, 5, 5, 0.5), G(-70))

# Country / roots / jazz / surf
I("Brad Paisley", "Chicken Pickin'", "Inspired By", ["Country"], "Snappy, compressed, a little hot.",
  A("Chime Top", 5, 4.5, 5, 6.5, 6), C("2x12 Blue", 2.5, 2), COMP(6, 3), DLY("Analog", 300, "Off", 2, 5, 1.8), G(-66))
I("Brent Mason", "Nashville Session", "Inspired By", ["Country"], "Glassy, compressed, studio clean.",
  A("Glass Clean", 4.5, 4.5, 5, 6.5, 6), C("2x12 American", 2.5, 2), COMP(6, 3), REV("Room", 3, 8, 5.5, 1.4), G(-68))
I("Keith Urban", "Country Rock Drive", "Inspired By", ["Country Rock"], "Bright overdrive for modern country.",
  A("Chime Top", 6, 4.5, 5, 6.5, 6), C("2x12 Blue", 3, 2), D("Blues Drive", 3, 5.5, 6), DLY("Digital", 380, "Off", 2.5, 6, 2), G(-64))
I("Wes Montgomery", "Octave Jazz", "Inspired By", ["Jazz"], "Dark, warm, thumb-picked jazz.",
  A("Glass Clean", 3, 6.5, 5.5, 2.5, 3.5), C("1x12 Open Back", 6, 3, 60, 5500), REV("Room", 3, 10, 3.5, 1.2), G(-74))
I("Pat Metheny", "Lyrical Chorus", "Inspired By", ["Jazz Fusion"], "Warm, chorused, delayed jazz lead.",
  A("Glass Clean", 3.5, 5.5, 5, 4.5), C("1x12 Open Back", 4.5, 2.5), MOD("Chorus", 2, 5, 6), DLY("Ping Pong", 380, "Off", 3, 5, 2.5), REV("Hall", 5, 25, 4.5, 2.4), G(-72))
I("Dick Dale", "Surf Tremolo Picking", "Inspired By", ["Surf"], "Loud, bright, soaked in spring.",
  A("Glass Clean", 6, 5, 4.5, 7, 6.5), C("2x12 American", 2.5, 2), REV("Spring", 6.5, 0, 6, 4.5), G(-66))

# Ambient / post-rock
I("Explosions in the Sky", "Crescendo Tremolo", "Inspired By", ["Post-Rock"], "Shimmering tremolo picking with a huge hall.",
  A("Chime Top", 5, 5, 5, 6), C("2x12 Blue", 3, 2), DLY("Digital", 400, "1/4 dotted", 4, 6, 3), REV("Hall", 7.5, 40, 5.5, 3.5), G(-66))
I("Sigur Ros", "Glacial Bowed Wash", "Inspired By", ["Post-Rock", "Ambient"], "Endless, glacial reverb wash.",
  A("Tweed Edge", 5, 5, 5, 5), C("2x12 Blue", 4, 3), D("Big Fuzz", 4, 4, 4), REV("Cloud", 8, 60, 4.5, 6), G(-62))

# Punk / classic
I("Johnny Ramone", "Blitzkrieg Buzz", "Inspired By", ["Punk"], "Pure downstroke buzzsaw.",
  A("Brit 81", 7.5, 5, 6.5, 6.5, 6), C("4x12 Vintage", 2.5, 1.5), G(-58), NOREV())
I("Steve Jones", "Never Mind Crunch", "Inspired By", ["Punk"], "Thick, cranked British punk rhythm.",
  A("Brit 68", 8, 5.5, 6.5, 6), C("4x12 Vintage", 3, 2), G(-58), NOREV(), double=6)

# India and South Asia
I("The Local Train", "Aaoge Tum Kabhi Rock", "Inspired By", ["Desi Indie", "Alternative"], "Warm Hindi alt-rock crunch with delay.",
  A("Brit 68", 5.5, 5, 6, 5.5), C("4x12 Vintage", 3, 2), DLY("Digital", 420, "Off", 3, 5.5, 2.2), REV("Hall", 4.5, 20, 5, 2), G(-62))
I("Prateek Kuhad", "Soft Indie Clean", "Inspired By", ["Desi Indie", "Folk Pop"], "Gentle, warm clean for soft songs.",
  A("Glass Clean", 3, 5.5, 5, 4.5, 4.5), C("1x12 Open Back", 4.5, 2.5), COMP(4, 6), REV("Room", 4, 12, 4.5, 2.4), G(-74))
I("Indian Ocean", "Fusion Groove", "Inspired By", ["Indian Fusion", "Folk Rock"], "Earthy, slightly driven tone for fusion grooves.",
  A("Tweed Edge", 5, 5.5, 6, 5), C("1x12 Open Back", 3.5, 2.5), REV("Room", 3.5, 10, 4.5, 1.8), G(-66))
I("Parikrama", "Indian Classic Rock", "Inspired By", ["Indian Rock", "Classic Rock"], "Big, melodic classic-rock crunch.",
  A("Brit 81", 6, 5, 6.5, 5.5), C("4x12 Vintage", 3, 2), DLY("Analog", 420, "Off", 2.5, 5, 2), REV("Hall", 4, 20, 5, 1.8), G(-60))
I("Raghu Dixit", "Folk Strum Energy", "Inspired By", ["Indian Folk", "Folk Rock"], "Bright, bouncy strumming tone.",
  A("Chime Top", 4.5, 5, 5, 6), C("2x12 Blue", 3, 2), COMP(4, 5), REV("Room", 3.5, 10, 5, 2), G(-68))
I("When Chai Met Toast", "Sunny Indie Pop", "Inspired By", ["Desi Indie", "Indie Pop"], "Bright, happy, chorused clean.",
  A("Glass Clean", 4, 5, 5, 6, 5.5), C("2x12 American", 3, 2), COMP(4, 5), MOD("Chorus", 2.5, 3, 4), REV("Plate", 4, 15, 5.5, 2.2), G(-70))
I("A.R. Rahman", "Cinematic Swell", "Inspired By", ["Bollywood", "Cinematic"], "Lush, wide, cinematic clean-to-lead tone.",
  A("Chime Top", 5, 5, 5, 5.5), C("2x12 Blue", 3.5, 2.5), COMP(4, 5), DLY("Ping Pong", 450, "Off", 4, 5, 3), REV("Hall", 6.5, 30, 5, 3.2), G(-66), double=4)

# Bass players
I("Flea", "Slap Funk Bass", "Inspired By", ["Bass", "Funk Rock"], "Punchy, bright slap bass.",
  A("Bass Station", 4.5, 6.5, 4, 7), C("4x10 Bass", 2.5, 1.5, 30, 12000), COMP(7, 2), NOREV(), G(-70))
I("Cliff Burton", "Fuzz Wah Bass", "Inspired By", ["Bass", "Thrash"], "Fuzzed bass with a wah, for lead bass lines.",
  A("Bass Station", 6, 6, 5, 5.5), C("4x10 Bass", 3, 2, 30, 9000), D("Big Fuzz", 6, 5, 5), WAH("Auto", 5, 6, 5), G(-60), NOREV())
I("Geddy Lee", "Prog Bass Growl", "Inspired By", ["Bass", "Progressive Rock"], "Bright, growly, midrange-y bass.",
  A("Bass Station", 6, 5.5, 6.5, 6.5), C("4x10 Bass", 2.5, 2, 30, 11000), COMP(5, 4), NOREV(), G(-66))
I("Rahul Ram", "Earthy Fusion Bass", "Inspired By", ["Bass", "Indian Fusion"], "Warm, woody, round bass.",
  A("Bass Station", 4, 6.5, 5.5, 4), C("1x15 Bass", 4, 2.5, 30, 6000), COMP(4, 5), NOREV(), G(-70))
I("James Jamerson", "Motown Thump", "Inspired By", ["Bass", "Motown", "Soul"], "Deep, dark, thumpy old-school bass.",
  A("Bass Station", 3.5, 7, 5, 2.5), C("1x15 Bass", 4.5, 2.5, 30, 4500), COMP(4, 5), NOREV(), G(-72))


# ---------------------------------------------------------------- Songs players asked for
S("Metallica", "Master of Puppets", "", ["Thrash", "Heavy Metal"], "Fast, dry, scooped rhythm for downpicked eighths.",
  A("Tight Metal", 7, 4.5, 4, 6.5, 6, 5.5), C("4x12 Modern", 2.5, 1.5), EQ(3, 0, -2, -5, -2, 2, 3), G(-54, 50), NOREV(), double=7)
S("Metallica", "Master of Puppets", "Interlude", ["Thrash", "Heavy Metal"], "Clean arpeggios with chorus for the slow middle part.",
  A("Glass Clean", 3.5, 5, 4.5, 6, 5.5), C("2x12 American", 3, 2), COMP(4, 5), MOD("Chorus", 3, 5, 5), REV("Hall", 5, 20, 5, 2.5), G(-70))
S("Def Leppard", "Pour Some Sugar on Me", "", ["Hard Rock", "80s", "Arena Rock"], "Compressed, chorused, layered arena crunch.",
  A("Brit 81", 6, 4.5, 6.5, 6.5, 6), C("4x12 Vintage", 2.5, 2, 90, 9000), COMP(6, 4), MOD("Chorus", 2.5, 4, 4), REV("Plate", 4, 20, 6, 2), G(-60), double=8)
S("Iron Maiden", "Fear of the Dark", "", ["Heavy Metal", "NWOBHM"], "Mid-forward British gallop that also carries the twin leads.",
  A("Brit 81", 7, 5, 7, 6, 5.5), C("4x12 Vintage", 3, 2), REV("Room", 3.5, 10, 5, 1.4), G(-60))
S("Iron Maiden", "Fear of the Dark", "Intro", ["Heavy Metal", "NWOBHM"], "The quiet opening: clean, chorus and a long hall.",
  A("Glass Clean", 3, 5, 5, 5.5, 5), C("2x12 American", 3.5, 2.5), MOD("Chorus", 2.5, 5, 5), DLY("Digital", 400, "Off", 2, 5, 1.5), REV("Hall", 5.5, 25, 5, 2.8), G(-70))
S("Dream Theater", "Wither", "Lead", ["Progressive Metal", "Progressive Rock"], "Smooth, sustaining solo tone with delay and a hall.",
  A("Redline Lead", 7, 5, 6.5, 5.5, 5), C("4x12 Modern", 3.5, 2), D("Green Drive", 1, 5.5, 7), DLY("Digital", 440, "Off", 3, 5.5, 2.5), REV("Hall", 5, 25, 5, 2.2), G(-60))
S("Dream Theater", "Wither", "Verse", ["Progressive Metal", "Progressive Rock"], "Glassy, chorused clean with a soft delay.",
  A("Glass Clean", 3.5, 5, 5, 6, 5), C("2x12 American", 3, 2), COMP(4, 5), MOD("Chorus", 2, 4, 4.5), DLY("Digital", 440, "Off", 2.5, 5.5, 2), REV("Hall", 5, 25, 5.5, 2.6), G(-70))
S("Guns N' Roses", "Sweet Child O' Mine", "", ["Hard Rock", "Classic Rock"], "Warm, round crunch for the opening riff. Use the neck pickup.",
  A("Brit 81", 5.5, 5.5, 7, 5, 4.5), C("4x12 Vintage", 3.5, 2), DLY("Analog", 380, "Off", 2, 4.5, 1.2), REV("Room", 3.5, 10, 5, 1.6), G(-62))
S("Guns N' Roses", "Sweet Child O' Mine", "Solo", ["Hard Rock", "Classic Rock"], "Singing neck-pickup lead. Switch the wah on for the second half of the solo.",
  A("Redline Lead", 7, 5.5, 7, 5.5, 5), C("4x12 Vintage", 3.5, 2), WAH("Classic", 5, 6.5, 5.5), FIX(wah_on=False), DLY("Analog", 400, "Off", 2.5, 4.5, 1.8), REV("Room", 3.5, 10, 5, 1.5), G(-60))
S("Avenged Sevenfold", "Nightmare", "", ["Heavy Metal", "Metalcore"], "Thick, tight rhythm. The song is in drop D.",
  A("Modern High Gain", 7, 6, 4.5, 6.5, 6.5, 6.5), C("4x12 Modern", 2.5, 1.5), D("Green Drive", 1, 6, 7), G(-54, 50), NOREV(), double=7)
P("Big Fuzz Wall", "Rock", ["Alternative", "Shoegaze", "Stoner Rock"], "The Big Fuzz pedal into a loud, nearly clean stack. Thick and endless.",
  A("Brit 68", 3.5, 5.5, 5.5, 5, 4.5), C("4x12 Vintage", 3.5, 2), D("Big Fuzz", 7.5, 5, 5), REV("Room", 3, 8, 5, 1.2), G(-58))


def slug(name):
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def existing_levels():
    """Output levels the presets on disk already carry, by preset name."""
    kept = {}
    if not os.path.isdir(OUT):
        return kept
    for f in os.listdir(OUT):
        if f.endswith(".json"):
            try:
                with open(os.path.join(OUT, f)) as fh:
                    p = json.load(fh)
                kept[p["name"]] = p["params"]["out_level"]
            except (ValueError, KeyError, OSError):
                pass
    return kept


def write_all(levels=None):
    os.makedirs(OUT, exist_ok=True)
    kept = existing_levels()
    for f in os.listdir(OUT):
        if f.endswith(".json"):
            os.remove(os.path.join(OUT, f))
    names = set()
    for p in PRESETS:
        assert p["name"] not in names, "duplicate " + p["name"]
        names.add(p["name"])
        if p["name"] in kept:
            p["params"]["out_level"] = kept[p["name"]]
        if levels and p["name"] in levels:
            # level-match every preset to -18 dBFS RMS, but keep peaks under
            # -2 dBFS so the safety limiter never has to work on the test DI
            m = levels[p["name"]]
            adjust = min(-18.0 - m["rms"], -2.0 - m["peak"])
            p["params"]["out_level"] = round(max(-36.0, min(12.0, adjust)), 1)
        with open(os.path.join(OUT, slug(p["name"]) + ".json"), "w") as fh:
            json.dump(p, fh, indent=2)
            fh.write("\n")
    print(f"wrote {len(PRESETS)} presets to {os.path.normpath(OUT)}")


if __name__ == "__main__":
    levels = None
    if len(sys.argv) > 2 and sys.argv[1] == "--calibrate":
        levels = json.load(open(sys.argv[2]))
    write_all(levels)
