#!/usr/bin/env python3
"""Build the RSS feeds the Riff House Discord reads.

The website publishes these files next to the site. A feed bot in the Discord
server watches each one and posts every new item into its channel:

  releases.xml          new stable versions          -> #releases
  beta.xml              pre-release test builds      -> #beta-lab (Beta testers only)
  devlog.xml            recent changes on main       -> #dev-log
  tone-of-the-week.xml  one preset picked each week  -> #tone-of-the-week
  riff-challenge.xml    one riff prompt each Monday  -> #riff-challenge

The free feed bot follows at most three feeds, so it reads three combined ones and
routes items by their <category> (one connection per channel, filtered on category):

  builds.xml   releases.xml + beta.xml      categories: release, beta
  devlog.xml                                category: devlog
  weekly.xml   tone-of-the-week + challenge categories: tone, challenge

Nothing here needs a secret. Release and commit data come from the public
GitHub API (GITHUB_TOKEN is used when present, only to avoid rate limits).
The weekly picks are worked out from the week number, so every run gives the
same item for the same week and the bot never posts a week twice.

Usage: python3 tools/community/build_feeds.py <output folder>
"""

from __future__ import annotations

import datetime as dt
import email.utils
import json
import os
import random
import re
import sys
import urllib.request
from pathlib import Path
from xml.sax.saxutils import escape

REPO = os.environ.get("GITHUB_REPOSITORY", "NatikChhabra/riffhouse")
SITE = "https://natikchhabra.github.io/riffhouse/"
ROOT = Path(__file__).resolve().parents[2]
PRESETS = ROOT / "presets"
CHALLENGES = Path(__file__).with_name("challenges.json")
WEEKS_KEPT = 6
# Week 0 of the weekly posts. The Monday the Discord server opened.
FIRST_MONDAY = dt.date(2026, 11, 9)


def api(path: str):
    req = urllib.request.Request(f"https://api.github.com/repos/{REPO}/{path}")
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("User-Agent", "riffhouse-feeds")
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)


def rfc822(when: dt.datetime) -> str:
    return email.utils.format_datetime(when.astimezone(dt.timezone.utc))


def parse_time(s: str) -> dt.datetime:
    return dt.datetime.fromisoformat(s.replace("Z", "+00:00"))


def plain(md: str, limit: int = 900) -> str:
    """Release notes as short plain text that reads well in a Discord post."""
    lines = []
    for line in (md or "").splitlines():
        line = line.rstrip()
        line = re.sub(r"^#+\s*", "", line)          # headings
        line = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", line)  # images
        line = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", line)  # links -> text
        line = line.replace("**", "").replace("__", "")
        lines.append(line)
    text = re.sub(r"\n{3,}", "\n\n", "\n".join(lines)).strip()
    if len(text) > limit:
        cut = text[:limit].rsplit("\n", 1)[0].rstrip()
        text = cut + "\n..."
    return text


def write_feed(path: Path, title: str, link: str, about: str, items: list[dict]) -> None:
    out = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<rss version="2.0">',
        "<channel>",
        f"<title>{escape(title)}</title>",
        f"<link>{escape(link)}</link>",
        f"<description>{escape(about)}</description>",
        "<language>en</language>",
    ]
    for it in items:
        out += [
            "<item>",
            f"<title>{escape(it['title'])}</title>",
            f"<link>{escape(it['link'])}</link>",
            f'<guid isPermaLink="false">{escape(it["guid"])}</guid>',
            f"<pubDate>{rfc822(it['when'])}</pubDate>",
            f"<category>{escape(it['category'])}</category>",
            f"<description>{escape(it['text'])}</description>",
            "</item>",
        ]
    out += ["</channel>", "</rss>", ""]
    path.write_text("\n".join(out), encoding="utf-8")


def release_items(prerelease: bool) -> list[dict]:
    items = []
    for r in api("releases?per_page=30"):
        if r.get("draft") or bool(r.get("prerelease")) != prerelease:
            continue
        tag = r["tag_name"]
        notes = plain(r.get("body") or "")
        if prerelease:
            title = f"Beta build {tag} is ready to test"
            text = (
                f"{notes}\n\nGet the installer from the release page. It is a test build, so keep "
                "your stable version around. Post what broke and what felt better in #beta-feedback."
            )
        else:
            title = f"Riff House {tag} is out"
            text = f"{notes}\n\nThe app updates itself. New here? Download: {SITE}"
        items.append({
            "category": "beta" if prerelease else "release",
            "title": title,
            "link": r["html_url"],
            "guid": f"release-{tag}",
            "when": parse_time(r.get("published_at") or r["created_at"]),
            "text": text.strip(),
        })
    return items[:10]


def devlog_items() -> list[dict]:
    items = []
    for c in api("commits?sha=main&per_page=30"):
        msg = c["commit"]["message"].strip()
        first, _, rest = msg.partition("\n")
        if first.startswith("Merge ") or "[skip devlog]" in msg:
            continue
        body = "\n".join(
            l for l in rest.strip().splitlines()
            if not re.match(r"^(Co-Authored-By|Signed-off-by|Claude-Session):", l, re.I)
        ).strip()
        items.append({
            "category": "devlog",
            "title": first[:200],
            "link": c["html_url"],
            "guid": f"commit-{c['sha']}",
            "when": parse_time(c["commit"]["committer"]["date"]),
            "text": plain(body, 600) or first,
        })
    return items[:15]


def weeks_so_far(today: dt.date) -> list[tuple[int, dt.datetime]]:
    """(week number, Monday 09:00 UTC) for the last few weeks, newest first."""
    n = max(0, (today - FIRST_MONDAY).days // 7)
    out = []
    for w in range(n, max(-1, n - WEEKS_KEPT), -1):
        monday = FIRST_MONDAY + dt.timedelta(weeks=w)
        out.append((w, dt.datetime.combine(monday, dt.time(9, 0), dt.timezone.utc)))
    return out


def eligible_presets() -> list[dict]:
    """Our own presets. No 'Inspired by' ones: those carry artist names, which we keep out of posts."""
    out = []
    for f in sorted(PRESETS.glob("*.json")):
        p = json.loads(f.read_text(encoding="utf-8"))
        if p.get("category") == "Inspired By" or p.get("inspiredBy"):
            continue
        out.append({"file": f.name, **p})
    return out


def describe_chain(params: dict) -> str:
    """The signal chain in playing order, e.g. 'Tube Screamer drive > Glass Clean amp (gain 3.5) > ...'."""
    def fx(key: str, label: str) -> str | None:
        if not params.get(f"{key}_on"):
            return None
        kind = params.get(f"{key}_type")
        return f"{kind} {label}" if kind else label

    parts = [
        "compressor" if params.get("comp_on") else None,
        "wah" if params.get("wah_on") else None,
        fx("drv", "drive"),
        f"{params.get('amp_model')} amp (gain {params.get('amp_gain')})" if params.get("amp_on") else None,
        f"{params.get('cab_model')} cab" if params.get("cab_on") else None,
        fx("mod", "modulation"),
        fx("dly", "delay"),
        fx("rev", "reverb"),
    ]
    return " > ".join(p for p in parts if p)


def tone_items(today: dt.date) -> list[dict]:
    presets = eligible_presets()
    items = []
    for w, when in weeks_so_far(today):
        # Shuffle once per cycle so every preset gets a turn before any repeats.
        cycle, slot = divmod(w, len(presets))
        order = presets[:]
        random.Random(f"riffhouse-tone-{cycle}").shuffle(order)
        p = order[slot]
        genres = ", ".join(p.get("genres") or [])
        chain = describe_chain(p.get("params") or {})
        lines = [p.get("description") or ""]
        if genres:
            lines.append(f"Good for: {genres}")
        if chain:
            lines.append(f"Chain: {chain}")
        lines.append(
            f"Find it in Riff House under {p.get('category')} > {p['name']}. "
            "Tweak it, then share your version in #tone-lab. Reply here with a clip if you play something with it."
        )
        items.append({
            "category": "tone",
            "title": f"Tone of the week: {p['name']}",
            "link": f"https://github.com/{REPO}/blob/main/presets/{p['file']}",
            "guid": f"tone-week-{w}",
            "when": when,
            "text": "\n".join(l for l in lines if l),
        })
    return items


def challenge_items(today: dt.date) -> list[dict]:
    prompts = json.loads(CHALLENGES.read_text(encoding="utf-8"))
    items = []
    for w, when in weeks_so_far(today):
        c = prompts[w % len(prompts)]
        text = (
            f"{c['prompt']}\n\n"
            "Reply in this thread with a clip (phone video is fine), and say which Riff House preset or settings you used. "
            "React to the takes you like. New prompt next Monday."
        )
        items.append({
            "category": "challenge",
            "title": f"Riff challenge, week {w + 1}: {c['title']}",
            "link": SITE,
            "guid": f"riff-week-{w}",
            "when": when,
            "text": text,
        })
    return items


def newest_first(*groups: list[dict]) -> list[dict]:
    return sorted((it for g in groups for it in g), key=lambda it: it["when"], reverse=True)


def main() -> int:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "site/feeds")
    out.mkdir(parents=True, exist_ok=True)
    today = dt.datetime.now(dt.timezone.utc).date()
    gh = f"https://github.com/{REPO}"

    tones, challenges = tone_items(today), challenge_items(today)
    write_feed(out / "tone-of-the-week.xml", "Riff House tone of the week", SITE,
               "One Riff House preset picked every Monday.", tones)
    write_feed(out / "riff-challenge.xml", "Riff House riff challenge", SITE,
               "A new riff prompt every Monday.", challenges)
    write_feed(out / "weekly.xml", "Riff House weekly", SITE,
               "Tone of the week and the riff challenge, every Monday.", newest_first(tones, challenges))

    # GitHub data. If the API is down, keep the old files out of the way and carry on:
    # the weekly feeds above still publish.
    try:
        stable, beta = release_items(False), release_items(True)
        write_feed(out / "releases.xml", "Riff House releases", gh, "New Riff House versions.", stable)
        write_feed(out / "beta.xml", "Riff House beta builds", gh, "Test builds before they ship.", beta)
        write_feed(out / "builds.xml", "Riff House builds", gh, "New versions and beta builds.", newest_first(stable, beta))
    except Exception as e:  # noqa: BLE001 - report and carry on
        print(f"warning: releases: {e}", file=sys.stderr)
    try:
        write_feed(out / "devlog.xml", "Riff House dev log", gh, "What changed in Riff House.", devlog_items())
    except Exception as e:  # noqa: BLE001
        print(f"warning: devlog: {e}", file=sys.stderr)

    for f in sorted(out.glob("*.xml")):
        print(f"{f.name}: {f.read_text(encoding='utf-8').count('<item>')} items")
    return 0


if __name__ == "__main__":
    sys.exit(main())
