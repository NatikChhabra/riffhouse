#!/usr/bin/env python3
"""Puts the sound demos on the website.

  python3 tools/site/build_demos.py <folder>

<folder> holds one clip per tone, the same take through each: <slug>.wav (to measure) and
<slug>.mp3 (to play). Copies the MP3s to site/audio/ and writes site/demos.json with each tone's
name and description from site/tones.json and the shape of its sound, drawn as bars on the page.
The Hear it section stays hidden while demos.json is missing or empty.
"""
import json
import shutil
import sys
import wave
from array import array
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "site"
BARS = 80
# The order they sit in on the page: clean to heavy.
ORDER = ["first-light-clean", "clean-arpeggio-space", "warm-crunch", "texas-blues", "smooth-lead", "heavy-starter"]


def samples(path):
    """Mono samples of a 16 or 24 bit WAV, as floats from -1 to 1."""
    with wave.open(str(path)) as w:
        ch, width, n = w.getnchannels(), w.getsampwidth(), w.getnframes()
        raw = w.readframes(n)
        rate = w.getframerate()
    if width == 2:
        ints = array("h", raw)
        scale = 32768.0
    elif width == 3:
        ints = [int.from_bytes(raw[i:i + 3], "little", signed=True) for i in range(0, len(raw), 3)]
        scale = 8388608.0
    else:
        sys.exit(f"{path.name}: {width * 8} bit WAV is not handled")
    mono = [sum(ints[i:i + ch]) / ch / scale for i in range(0, len(ints), ch)]
    return mono, rate


def bars(mono):
    """Loudest moment in each of BARS equal slices."""
    step = len(mono) / BARS
    return [max((abs(v) for v in mono[int(i * step):int((i + 1) * step)]), default=0.0) for i in range(BARS)]


def main(folder):
    tones = {t["n"]: t for t in json.loads((SITE / "tones.json").read_text())}
    by_slug = {n.lower().replace(" ", "-"): n for n in tones}
    found = [s for s in ORDER if (folder / f"{s}.wav").is_file() and (folder / f"{s}.mp3").is_file()]
    found += sorted(p.stem for p in folder.glob("*.wav") if p.stem not in ORDER and (folder / f"{p.stem}.mp3").is_file())
    if not found:
        sys.exit(__doc__)
    clips, top = [], 0.0
    for slug in found:
        if slug not in by_slug:
            sys.exit(f"{slug}: no tone with that name in site/tones.json")
        mono, rate = samples(folder / f"{slug}.wav")
        shape = bars(mono)
        top = max(top, max(shape))
        tone = tones[by_slug[slug]]
        clips.append({"name": tone["n"], "note": tone["d"], "file": f"audio/{slug}.mp3", "seconds": round(len(mono) / rate, 2), "peaks": shape})
        (SITE / "audio").mkdir(exist_ok=True)
        shutil.copyfile(folder / f"{slug}.mp3", SITE / "audio" / f"{slug}.mp3")
    for c in clips:  # one scale for all, so a squashed tone looks squashed next to a clean one
        c["peaks"] = [max(3, round(v / top * 96)) for v in c["peaks"]]
    (SITE / "demos.json").write_text(json.dumps(clips, separators=(",", ":")) + "\n")
    print(len(clips), "clips:", ", ".join(c["name"] for c in clips))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(Path(sys.argv[1]))
