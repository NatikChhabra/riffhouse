"""Checks the #general bug-report pointer only fires on real Riff House bug reports.
Run: python3 tools/community/test_chat_pointers.py"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import discord_bot as b

def is_bug(t):
    t = t.lower()
    return bool(b.BUG_WHAT.search(t) and b.BUG_WHERE.search(t) and not b.CASUAL.search(t))

REAL = [
 "riff house crashes every time i load the nightmare preset",
 "the app froze when i changed the cab",
 "found a bug, tuner shows wrong note on low E",
 "after the update it keeps closing on startup",
 "riffhouse 0.1.5 crashed mid song",
 "the delay pedal in riff house is glitchy, it pops when i turn it on",
 "app stopped working after the update",
 "metronome is bugged, it drifts",
]
CASUAL_CHAT = [
 "lol my pc crashed playing valorant",
 "crash cymbal on this track is so loud",
 "that riff was a bug of a riff haha",
 "i took a crash course in music theory",
 "my game keeps freezing today",
 "this tone is insane",
 "who else plays metal here",
 "my amp at home is a marshall",
 "bug spray time, mosquitoes everywhere",
 "windows update broke my wifi again",
 "that solo was crazy, it froze me in place",
 "is the app free?",
 "my old amp crashed and burned years ago lol",
 "discord crashed lmao",
 "my amp crashed and burned years ago",
 "my real amp froze up in the cold gig",
 "the delay pedal on my board is glitchy",
 "the app store crashed on my phone",
 "car crashed into a pole near my house",
 "spotify app keeps crashing today",
 "there's a crash at the end of the song where the drums come in",
]

bad = [m for m in REAL if not is_bug(m)] + [m for m in CASUAL_CHAT if is_bug(m)]
print("ok" if not bad else "misfires: %r" % bad)
sys.exit(1 if bad else 0)
