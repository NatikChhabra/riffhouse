#!/usr/bin/env python3
"""Does the website still match the newest release? Run after every release.

  check_release.py            ask GitHub for the newest public release and check the site against it
  check_release.py <tag>      check against that tag, without asking GitHub (v0.1.5 or v0.1.5.3)

It reads the files in site/ and fails with a plain list of what is stale:

  - the What's new page has a card for the version, it is the first card and it is marked "Out now"
  - the roadmap's "Out now" stop names the version
  - a hotfix or update (a fourth number, v0.1.5.3) is named on both pages
  - the card has its picture and a link to that version's notes
  - every page states the same number of tones, amps, pedals and cabinets, and the tone
    count is the number of tones in site/tones.json
  - the version line written into the home and press pages names the version (it is what search engines and
    AI assistants read: they do not run the script that fills in the live one)
  - the data block in the home page head, site/llms.txt and the sitemap dates are up to date (tools/site/schema.py)
  - the release has an installer the download button can find (only when asking GitHub)

For a visitor, the download buttons are written with the newest installer on every publish (tools/site/download.py),
and the home page and the press page read the newest version from GitHub again when they load.
"""
import json
import os
import pathlib
import re
import subprocess
import sys
import time
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[2]
SITE = ROOT / "site"
REPO = "NatikChhabra/riffhouse"
TAG = re.compile(r"^v(\d+\.\d+\.\d+)(\.\d+)?$")
PAGES = ["index.html", "no-audio-interface.html", "free-amp-sim-windows.html", "why-free.html", "updates.html", "roadmap.html", "press.html", "left.html"]
WORDS = {"nine": 9, "ten": 10, "eleven": 11, "twelve": 12, "thirteen": 13, "thirty": 30}


def newest_release():
    """The newest release that is not a draft or a test release and has a Windows installer: the site's own rule."""
    req = urllib.request.Request(f"https://api.github.com/repos/{REPO}/releases?per_page=20",
                                 headers={"Accept": "application/vnd.github+json", "User-Agent": "site-check"})
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    last = None
    for wait in (0, 5, 20):
        time.sleep(wait)
        try:
            with urllib.request.urlopen(req, timeout=30) as res:
                releases = json.load(res)
            break
        except Exception as err:  # a network hiccup: try again
            last = err
    else:
        sys.exit(f"site check: could not read the releases from GitHub ({last})")
    public = [r for r in releases if not r["draft"] and not r["prerelease"]]
    if not public:
        sys.exit("site check: there is no public release yet")
    for r in public:
        files = [a for a in r["assets"] if a["name"].lower().endswith("windows-setup.exe")]
        if files:
            problems = []
            if r is not public[0]:
                problems.append(f"{public[0]['tag_name']} is published but has no windows-setup.exe, "
                                f"so the download button still gives {r['tag_name']}")
            return r["tag_name"], problems, files[0]
    return public[0]["tag_name"], [f"{public[0]['tag_name']} has no windows-setup.exe: the download button has nothing to give"], None


def text_of(html):
    """The words a visitor reads: tags, scripts and styles taken out."""
    html = re.sub(r"<(script|style)\b.*?</\1>", " ", html, flags=re.S)
    return re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", html))


def counts(html, thing):
    """Every number the page gives for 'N tones', 'Nine cabinets' and the like."""
    found = set()
    for m in re.finditer(rf"(?<![\w.,])(\d{{1,3}}|{'|'.join(WORDS)})\s+{thing}\b", text_of(html), flags=re.I):
        word = m.group(1).lower()
        found.add(int(word) if word.isdigit() else WORDS[word])
    return found


def check(tag):
    m = TAG.match(tag)
    if not m:
        sys.exit(f"site check: '{tag}' is not a public release tag (v1.2.3 or v1.2.3.4)")
    base, full = m.group(1), tag[1:]
    patched = bool(m.group(2))
    pages = {name: (SITE / name).read_text(encoding="utf-8") for name in PAGES}
    stale = []

    # What's new: the newest card is this version and says so.
    upd = pages["updates.html"]
    cards = re.findall(r'<li class="upd([^"]*)" id="v([0-9.]+)"', upd)
    ids = [v for _, v in cards]
    card = full if full in ids else base
    if card not in ids:
        stale.append(f"updates.html has no card for {full}: add one (id=\"v{base}\") with its picture")
    else:
        if ids[0] != card:
            stale.append(f"updates.html: the card for {card} is not the first one (the first is {ids[0]})")
        now = [v for cls, v in cards if "is-now" in cls]
        if now != [card]:
            stale.append(f"updates.html: 'Out now' is on {', '.join(now) or 'no card'}, it belongs on {card}")
        if patched and full not in text_of(upd):
            stale.append(f"updates.html does not name {full}: say which hotfix or update is the current one")
        if f"/releases/tag/v{full}" not in upd:
            stale.append(f"updates.html has no link to the notes for v{full}")
        if not (SITE / "img" / "updates" / f"{card}.webp").is_file():
            stale.append(f"site/img/updates/{card}.webp is missing: the card for {card} has no picture")
    stop = re.search(r'<li class="is-now"><a href="#v([0-9.]+)">', upd)
    if not stop or stop.group(1) != card:
        stale.append(f"updates.html: the row of stops under the headline does not mark {card} as out now")

    # Roadmap: the 'Out now' stop is this version.
    road = pages["roadmap.html"]
    stop = re.search(r'<li class="is-now"><a href="#now"><span class="stop-ver">([0-9.]+)</span>', road)
    if not stop or stop.group(1) not in (base, full):
        stale.append(f"roadmap.html: 'Out now' says {stop.group(1) if stop else 'nothing'}, the release is {full}")
    head = re.search(r'<h2 id="h-now">.*?</h2>', road, flags=re.S)
    if not head or not re.search(rf"(?<![0-9.]){re.escape(base)}(?![0-9])", text_of(head.group(0))):
        stale.append(f"roadmap.html: the 'Out now' heading does not say {base}")
    if patched and full not in text_of(road):
        stale.append(f"roadmap.html does not name {full}: say which hotfix or update is the current one")

    # The version line a crawler reads, and the facts built from it.
    for name in ("index.html", "press.html"):
        line = re.search(r"data-release[^>]*>([^<]*)<", pages[name])
        if not line or not re.search(rf"Version {re.escape(full)}, released \d{{1,2}} [A-Z][a-z]+ \d{{4}}\.", line.group(1)):
            stale.append(f"{name}: the version line in the page says '{line.group(1).strip() if line else 'nothing'}', "
                         f"write 'Version {full}, released <day month year>.'")
    built = subprocess.run([sys.executable, str(ROOT / "tools" / "site" / "schema.py"), "--check"], capture_output=True, text=True)
    if built.returncode:
        stale.append((built.stderr or built.stdout).strip().replace("schema: ", ""))

    # The numbers agree on every page, and the tone count is the real one.
    tones = len(json.loads((SITE / "tones.json").read_text(encoding="utf-8")))
    for thing in ("tones", "amps", "pedals", "cabinets"):
        said = {}
        for name, html in pages.items():
            if name == "updates.html":  # older cards say what was true then: read the newest card only
                html = re.split(r'<li class="upd[ "]', html)[1] if '<li class="upd' in html else html
            for n in counts(html, thing):
                said.setdefault(n, []).append(name)
        if thing == "tones":
            said.pop(8, None)  # "eight new tones" in a version's notes is not the total
            wrong = {n: where for n, where in said.items() if n != tones}
            if wrong:
                stale.append(f"site/tones.json has {tones} tones, but " +
                             "; ".join(f"{', '.join(sorted(set(w)))} says {n}" for n, w in sorted(wrong.items())))
        elif len(said) > 1:
            stale.append(f"the pages disagree on {thing}: " +
                         "; ".join(f"{n} in {', '.join(sorted(set(w)))}" for n, w in sorted(said.items())))
    return full, stale


def main(argv):
    if len(argv) > 2 or (len(argv) == 2 and argv[1] in ("-h", "--help")):
        sys.exit(__doc__)
    tag, problems = (argv[1], []) if len(argv) == 2 else newest_release()[:2]
    full, stale = check(tag)
    stale = problems + stale
    if stale:
        print(f"site check: the website is behind release {full}")
        for line in stale:
            print(f"  - {line}")
            if os.environ.get("GITHUB_ACTIONS"):  # shows the line on the run's summary page
                print(f"::error title=Website is behind {full}::{line}")
        sys.exit(1)
    print(f"site check: the website matches release {full}")


if __name__ == "__main__":
    main(sys.argv)
