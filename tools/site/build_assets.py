#!/usr/bin/env python3
"""Builds the website's pictures and its tone list from the app itself.

  Riffhouse --snapshots <folder> 2          every screen and each piece of gear, drawn by the app
  python3 tools/site/build_assets.py <folder>

Writes site/img/ (gear cutouts, screen crops, icons), site/fonts/ and site/tones.json.
Needs Pillow and fonttools with brotli. Run it again whenever the gear or the tones change.
"""
import json
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "site"
SHOTS = Path(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1] != "--tones" else None

# Cutout file, name on the box, what it does, in the order of the signal chain.
PEDALS = [
    ("gate-0", "Silencer", "Noise gate"), ("comp-0", "Squeeze", "Compressor"),
    ("wah-0", "Funk Filter", "Wah and envelope filter"), ("pitch-0", "Deep Octave", "Octave and pitch"),
    ("drive-0", "Clean Boost", "Boost"), ("drive-1", "Treble Booster", "Boost"),
    ("drive-2", "Green Drive", "Overdrive"), ("drive-3", "Blues Drive", "Overdrive"),
    ("drive-4", "Rodent", "Distortion"), ("drive-5", "Big Fuzz", "Fuzz"), ("drive-6", "Metal Box", "Distortion"),
    ("eq-0", "Graphic EQ", "Equaliser"),
    ("mod-0", "Liquid Chorus", "Chorus"), ("mod-1", "Jet Flanger", "Flanger"), ("mod-2", "Phase Four", "Phaser"),
    ("mod-3", "Tremble", "Tremolo"), ("mod-4", "Warble", "Vibrato"), ("mod-5", "Rotary Vibe", "Vibe"),
    ("delay-0", "Digital Echo", "Delay"), ("delay-1", "Bucket Delay", "Delay"),
    ("delay-2", "Tape Echo", "Delay"), ("delay-3", "Ping Pong", "Delay"),
    ("reverb-0", "Small Room", "Reverb"), ("reverb-1", "Concert Hall", "Reverb"), ("reverb-2", "Plate", "Reverb"),
    ("reverb-3", "Spring Tank", "Reverb"), ("reverb-4", "Shimmer", "Reverb"), ("reverb-5", "Cloud", "Reverb"),
    ("lofi-0", "Dusty Tape", "Lo-fi"),
]
AMPS = ["Glass Clean", "Tweed Edge", "Chime Top", "Brit Plexi", "Brit 800", "Hot Rod Lead",
        "Modern High Gain", "Tight Metal", "Bass Station", "Studio Direct", "Neural Capture"]
CABS = ["1x12 Open Back", "2x12 Blue", "4x12 Vintage", "4x12 Modern", "2x12 American",
        "1x8 Tiny Combo", "4x10 Bass", "1x15 Bass", "Your IR"]
# The choices behind each pedal slot, the same lists and defaults as src/engine/Params.cpp.
SLOTS = [  # switch, default on, type key, choices, default choice, cutout prefix
    ("gate_on", True, None, None, 0, "gate"), ("comp_on", False, None, None, 0, "comp"),
    ("wah_on", False, None, None, 0, "wah"), ("pitch_on", False, None, None, 0, "pitch"),
    ("drv_on", False, "drv_type", ["Clean Boost", "Treble Boost", "Green Drive", "Blues Drive", "Rodent Dist", "Big Fuzz", "Metal Box"], 2, "drive"),
    ("amp_on", True, "amp_model", AMPS, 3, "amp"), ("cab_on", True, "cab_model", CABS, 2, "cab"),
    ("eq_on", False, None, None, 0, "eq"),
    ("mod_on", False, "mod_type", ["Chorus", "Flanger", "Phaser", "Tremolo", "Vibrato", "Vibe"], 0, "mod"),
    ("dly_on", False, "dly_type", ["Digital", "Analog", "Tape", "Ping Pong"], 0, "delay"),
    ("rev_on", True, "rev_type", ["Room", "Hall", "Plate", "Spring", "Shimmer", "Cloud"], 0, "reverb"),
    ("lofi_on", False, None, None, 0, "lofi"),
]
# Crops of whole screens (left, top, right, bottom on the 2400 x 1560 snapshot) and the width they are saved at.
CROPS = {
    "setup-done": ("setup-done.png", (562, 222, 1838, 1338), None),
    "play-along": ("record-song.png", None, None),
    "tuner": ("tuner.png", None, None),
    "looper": ("looper.png", None, None),
}


def save(im, path, quality=86, **kw):
    path.parent.mkdir(parents=True, exist_ok=True)
    im.save(path, quality=quality, method=4, **kw)


def trim(im):
    """Cut the clear margin off a cutout and keep only the part of its shadow that sits close to the gear.
    The app draws a wide, faint room shadow as well; on a light page that reads as a grey box."""
    floor = 26
    im.putalpha(im.getchannel("A").point(lambda a: 0 if a <= floor else round((a - floor) * 255 / (255 - floor))))
    box = im.getchannel("A").point(lambda a: 255 if a > 2 else 0).getbbox()
    return im.crop(box) if box else im


def gear():
    out = {}
    for key, name, kind in PEDALS:
        im = trim(Image.open(SHOTS / "cutouts" / f"{key}.png").convert("RGBA"))
        save(im, SITE / "img" / "gear" / f"{key}.webp", 88)
        small = im.copy()
        small.thumbnail((360, 360), Image.LANCZOS)
        save(small, SITE / "img" / "gear" / f"{key}-s.webp", 84)
        out[key] = {"name": name, "kind": kind, "w": im.width, "h": im.height}
    for i in range(len(AMPS)):
        for prefix, count in (("amp", f"{i:02d}"), ("cab", str(i))):
            src = SHOTS / "cutouts" / f"{prefix}-{count}.png"
            if src.exists():
                im = trim(Image.open(src).convert("RGBA"))
                im.thumbnail((520, 360), Image.LANCZOS)
                save(im, SITE / "img" / "gear" / f"{prefix}-{int(count)}-s.webp", 84)
    return out


LQ, RQ = "\u201c", "\u201d"


def shown(d):
    """How the app lists a tone made after someone (Preset::artist, title, part and credit in
    src/engine/Presets.cpp): the band, or the player, in front; the song in quotation marks;
    and a credit line for whatever the title does not already say."""
    who, band, song, name = d.get("inspiredBy", ""), d.get("band") or "", d.get("song") or "", d["name"]
    if not who:
        return {}
    artist = band or who
    title = name.split(": ", 1)[1] if name.startswith("Inspired by ") and ": " in name else name
    starts = bool(song) and title.lower().startswith(song.lower())
    if starts:
        rest = title[len(song):].strip()
        title = LQ + song + RQ + (" " + rest if rest else "")
    other = who if who != artist else ""
    names_song = bool(song) and not starts
    quoted = LQ + song + RQ
    if other:
        credit = "Inspired by " + other + (" on " + quoted + ". " if names_song else ". ")
    else:
        credit = "After " + quoted + ". " if names_song else ""
    return {"w": artist, "t": title, "x": credit, "b": band, "s": song}


def tones(pedals):
    rows = []
    for f in sorted((ROOT / "presets").glob("*.json")):
        d = json.loads(f.read_text(encoding="utf-8"))
        p = d["params"]
        chain = []
        for switch, on, type_key, choices, default, prefix in SLOTS:
            if not p.get(switch, on):
                continue
            i = choices.index(p[type_key]) if type_key and type_key in p else default
            if prefix == "amp":
                chain.append(["amp", i, AMPS[i]])
            elif prefix == "cab":
                chain.append(["cab", i, CABS[i]])
            else:
                chain.append([prefix, i, pedals[f"{prefix}-{i}"]["name"]])
        row = {"n": d["name"], "c": d["category"], "g": d["genres"], "a": d.get("inspiredBy", ""),
               "d": d["description"], "r": chain}
        row.update(shown(d))
        rows.append(row)
    rows.sort(key=lambda r: ((r.get("w") or r["n"]) + " " + (r.get("t") or "")).lower())
    (SITE / "tones.json").write_text(json.dumps(rows, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    return len(rows)


def screens():
    for name, (src, box, _) in CROPS.items():
        im = Image.open(SHOTS / src).convert("RGB")
        if box:
            im = im.crop(box)
        for width in (2400, 1200):
            if im.width >= width or width == 1200:
                small = im if im.width <= width else im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
                save(small, SITE / "img" / "app" / f"{name}-{small.width}.webp", 82)
    for name in ("take-tone",):  # this one needs a recorded take, so it comes from docs/
        im = Image.open(ROOT / "docs" / "screenshots" / f"{name}.png").convert("RGB")
        save(im, SITE / "img" / "app" / f"{name}-{im.width}.webp", 82)
        small = im.resize((1200, round(im.height * 1200 / im.width)), Image.LANCZOS)
        save(small, SITE / "img" / "app" / f"{name}-1200.webp", 82)


def icons():
    icon = Image.open(ROOT / "resources" / "icon.png").convert("RGBA")
    for size, name in ((180, "apple-touch-icon.png"), (192, "icon-192.png"), (512, "icon-512.png")):
        icon.resize((size, size), Image.LANCZOS).save(SITE / "img" / name)
    icon.resize((48, 48), Image.LANCZOS).save(SITE / "favicon.ico", sizes=[(16, 16), (32, 32), (48, 48)])


def fonts():
    from fontTools import subset
    keep = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100)) + [0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x20B9, 0x00D7, 0x2212]
    for name in ("BigShoulders-ExtraBold", "InstrumentSans-Regular", "InstrumentSans-Medium", "InstrumentSans-SemiBold"):
        opts = subset.Options()
        opts.flavor = "woff2"
        opts.layout_features = ["*"]
        font = subset.load_font(str(ROOT / "resources" / "fonts" / f"{name}.ttf"), opts)
        s = subset.Subsetter(opts)
        s.populate(unicodes=keep)
        s.subset(font)
        (SITE / "fonts").mkdir(parents=True, exist_ok=True)
        subset.save_font(font, str(SITE / "fonts" / f"{name}.woff2"), opts)


if __name__ == "__main__":
    if sys.argv[1:] == ["--tones"]:  # only the tone list, from presets/, no pictures needed
        print(tones({k: {"name": n} for k, n, _ in PEDALS}), "tones")
        sys.exit()
    if not SHOTS or not (SHOTS / "cutouts").is_dir():
        sys.exit(__doc__)
    made = gear()
    print(len(made), "pedals,", tones(made), "tones")
    screens()
    icons()
    fonts()
