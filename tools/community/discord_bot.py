#!/usr/bin/env python3
"""The Riff House Discord bot. Runs every few minutes from GitHub Actions, no server needed.

Each run it:
  1. Posts new items from the site feeds (feeds/*.xml) into their channels:
       release -> #releases (pings Release pings, then publishes to following servers)
       beta    -> #beta-lab (pings Beta tester)
       devlog  -> #dev-log  (all new changes in one message)
       tone    -> #tone-of-the-week
  1b. Posts each new reel or trailer listed in community/reels.json to #content
     (see community/reels/README.md for how to add one).
  1c. Runs the release-day event in community/launch.json: a #general thread where the first 50
      people get the Day One role. Starts itself when that release appears.
  1d. (welcome() is kept but off: Welcomer, an always-on bot, greets new members instantly in #welcome)
  2. Answers common questions in the #help and #bug-reports forums when a new post
     clearly matches one (sound, delay, crackles, ASIO, install warning, ...), with the fix
     and a link to the full help page. Posts it can't match are left for a person.
  3. Closes forum posts tagged Solved, Fixed or Done after a quiet day.
  4. Keeps chat channels on purpose: a setup question in #general gets the fix and a pointer to
     #help, an idea gets pointed to #ideas (once a day per person, only if nobody answered in 10 min).
  5. Flags help and bug posts nobody answered within an hour at the top of the log (URGENT for crashes).
  6. Writes a log to #bot-log: who joined or left, new forum posts, and moderation
     (bans, kicks, timeouts, AutoMod) from the audit log.

All state lives in Discord itself: what was already posted is read back from the
channels, and the log keeps its place with a cursor in its last message. So the bot
can be re-run, skipped or moved without a database.

Needs DISCORD_BOT_TOKEN (the bare token). DRY_RUN=1 prints instead of posting.
Usage: python3 tools/community/discord_bot.py
"""

from __future__ import annotations

import datetime as dt
import email.utils
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET

API = "https://discord.com/api/v10"
SITE = "https://natikchhabra.github.io/riffhouse/"
HELP_PAGE = "https://github.com/NatikChhabra/riffhouse/blob/main/docs/TROUBLESHOOTING.md"
GUILD = "1570500603606736896"

CH = {
    "releases": "1570506502635266048",
    "dev-log": "1570506517160005664",
    "tone-of-the-week": "1570506840012230696",
    "beta-lab": "1570507022372438018",
    "help": "1570506640921460777",
    "bug-reports": "1570506657262346440",
    "ideas": "1570506671346688040",
    "beta-feedback": "1570507034372219052",
    "tone-lab": "1570506784672710766",
    "bot-log": "1570506931154591755",
    "announcements": "1570506487632232538",
    "general": "1570500604445458526",
    "gear-talk": "1570506861869015161",
    "off-topic": "1570506873050890350",
    "showcase": "1570506850963816468",
    "welcome": "1570506374117597265",
    "content": "1570923361222271110",
}
ROLE = {"release-pings": "1570506134857711637", "beta-tester": "1570505782259089549"}
FORUMS = {CH[k]: k for k in ("help", "bug-reports", "ideas", "beta-feedback", "tone-lab")}
FAQ_FORUMS = {CH["help"], CH["bug-reports"]}
AMBER = 0xF5A524
DRY = os.environ.get("DRY_RUN") == "1"
UA = "DiscordBot (https://github.com/NatikChhabra/riffhouse, 1)"


# ---------------------------------------------------------------- Discord REST

def call(method: str, path: str, body=None, reason: str | None = None):
    if DRY and method != "GET":
        print(f"[dry] {method} {path} {json.dumps(body)[:600] if body else ''}")
        return {}
    token = os.environ.get("DISCORD_BOT_TOKEN", "")
    for attempt in range(5):
        req = urllib.request.Request(API + path, method=method)
        req.add_header("Authorization", f"Bot {token}")
        req.add_header("User-Agent", UA)
        if reason:
            req.add_header("X-Audit-Log-Reason", reason)
        data = None
        if body is not None:
            data = json.dumps(body).encode()
            req.add_header("Content-Type", "application/json")
        try:
            with urllib.request.urlopen(req, data, timeout=30) as r:
                raw = r.read()
                return json.loads(raw) if raw else {}
        except urllib.error.HTTPError as e:
            if e.code == 429:
                wait = float(json.loads(e.read() or b"{}").get("retry_after", 2))
                time.sleep(min(wait, 30) + 0.25)
                continue
            if e.code >= 500 and attempt < 4:
                time.sleep(2 ** attempt)
                continue
            raise RuntimeError(f"{method} {path}: {e.code} {e.read()[:300]!r}") from None
    raise RuntimeError(f"{method} {path}: rate limited")


def send(channel: str, content: str = "", embeds=None, ping_roles=()):
    body = {"content": content, "allowed_mentions": {"parse": [], "roles": list(ping_roles)}}
    if embeds:
        body["embeds"] = embeds
    return call("POST", f"/channels/{channel}/messages", body)


def snowflake_time(sf: str) -> dt.datetime:
    return dt.datetime.fromtimestamp(((int(sf) >> 22) + 1420070400000) / 1000, dt.timezone.utc)


def time_snowflake(t: dt.datetime) -> str:
    return str((int(t.timestamp() * 1000) - 1420070400000) << 22)


ME = None


def me() -> str:
    global ME
    if ME is None:
        ME = call("GET", "/users/@me")["id"]
    return ME


def my_messages(channel: str, limit: int = 100) -> list[dict]:
    return [m for m in call("GET", f"/channels/{channel}/messages?limit={limit}") if m["author"]["id"] == me()]


# ---------------------------------------------------------------- 1. feeds

def read_feed(name: str) -> list[dict]:
    req = urllib.request.Request(f"{SITE}feeds/{name}?t={int(time.time())}", headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=30) as r:
        root = ET.fromstring(r.read())
    items = []
    for it in root.iter("item"):
        g = lambda tag: (it.findtext(tag) or "").strip()
        items.append({
            "title": g("title"), "link": g("link"), "guid": g("guid"), "category": g("category"),
            "description": g("description"),
            "date": email.utils.parsedate_to_datetime(g("pubDate")) if g("pubDate") else None,
        })
    items.sort(key=lambda i: i["date"] or dt.datetime.min.replace(tzinfo=dt.timezone.utc))
    return items


def posted_guids(channel: str) -> tuple[set[str], dt.datetime | None]:
    """Feed ids the bot already posted in a channel (kept in the embed footer), and the newest date."""
    seen, newest = set(), None
    for m in my_messages(channel):
        for e in m.get("embeds", []):
            seen.update(re.findall(r"#([\w.\-]+)", (e.get("footer") or {}).get("text", "")))
            if e.get("timestamp"):
                t = dt.datetime.fromisoformat(e["timestamp"].replace("Z", "+00:00"))
                newest = t if newest is None or t > newest else newest
    return seen, newest


def fresh(items: list[dict], channel: str, first_run_keep: int) -> list[dict]:
    seen, newest = posted_guids(channel)
    if not seen:  # first run in this channel: only the newest few, never the whole history
        return items[-first_run_keep:]
    return [i for i in items if i["guid"] not in seen and (newest is None or i["date"] is None or i["date"] >= newest)]


def short(text: str, n: int) -> str:
    text = text.strip()
    return text if len(text) <= n else text[: text.rfind("\n", 0, n) if "\n" in text[:n] else n].rstrip() + "\n..."


def link_channels(text: str) -> str:
    """#tone-lab in feed text becomes a clickable channel."""
    return re.sub(r"#([a-z\-]+)", lambda m: f"<#{CH[m.group(1)]}>" if m.group(1) in CH else m.group(0), text)


def post_feeds(log: list[str]):
    builds = read_feed("builds.xml")
    last_release = max((i["date"] for i in builds if i["category"] == "release" and i["date"]),
                       default=dt.datetime.min.replace(tzinfo=dt.timezone.utc))
    plan = [
        ("releases", [i for i in builds if i["category"] == "release"], 1, "release-pings", True),
        # a beta older than the newest release is already out for everyone, so it is not news
        ("beta-lab", [i for i in builds if i["category"] == "beta" and i["date"] and i["date"] > last_release], 1, "beta-tester", False),
        ("tone-of-the-week", read_feed("weekly.xml"), 1, None, False),
    ]
    for chan, items, keep, role, publish in plan:
        try:
            new = fresh(items, CH[chan], keep)
        except RuntimeError as e:
            print(f"error: #{chan}: {e}", file=sys.stderr)
            continue
        for it in new:
            # no ping for something that came out hours ago (first run, or a missed run)
            old = it["date"] and dt.datetime.now(dt.timezone.utc) - it["date"] > dt.timedelta(hours=6)
            ping = [ROLE[role]] if role and not old else []
            embed = {
                "title": it["title"][:256], "url": it["link"], "color": AMBER,
                "description": link_channels(short(it["description"], 1800)),
                "footer": {"text": f"Riff House #{it['guid']}"},
            }
            if it["date"]:
                embed["timestamp"] = it["date"].isoformat()
            preset = preset_file(it["link"]) if chan == "tone-of-the-week" else None
            if preset:
                msg = send_file(CH[chan], "", None, preset[0], preset[1], "application/json", [embed], ping)
            else:
                msg = send(CH[chan], f"<@&{ping[0]}>" if ping else "", [embed], ping)
            if publish and msg.get("id"):
                try:
                    call("POST", f"/channels/{CH[chan]}/messages/{msg['id']}/crosspost")
                except RuntimeError as e:
                    print(f"warning: crosspost: {e}", file=sys.stderr)
            log.append(f"Posted **{it['title']}** in <#{CH[chan]}>")

    # The dev log can move fast, so every new change since the last run goes in one message.
    # players only: never site, Discord, workflow or work-in-progress commits, even if an old feed still has them
    skip = re.compile(r"discord|feed|website|\bsite\b|readme|workflow|\bci\b|relay|roadmap|marketing|\bdocs?\b|bot\b|"
                      r"\b\d+\.\d+\.\d+\b|review|\btests?\b|refactor|\bwip\b", re.I)
    new = [i for i in fresh(read_feed("devlog.xml"), CH["dev-log"], 3) if not skip.search(i["title"])]
    if new:
        lines = [f"- [{i['title']}]({i['link']})" for i in reversed(new)][:15]
        embed = {
            "title": "What changed",
            "description": "\n".join(lines)[:3900], "color": AMBER,
            "footer": {"text": "Riff House " + " ".join("#" + i["guid"] for i in new)[:2000]},
            "timestamp": new[-1]["date"].isoformat() if new[-1]["date"] else None,
        }
        send(CH["dev-log"], "", [embed])
        log.append(f"Posted {len(new)} change(s) in <#{CH['dev-log']}>")


# ---------------------------------------------------------------- 1b. reels and the trailer

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REELS = os.path.join(ROOT, "community", "reels.json")
UPLOAD_LIMIT = 10 * 1024 * 1024


def send_file(channel: str, content: str, path: str | None, name: str, data: bytes | None = None,
              mime: str = "video/mp4", embeds=None, ping_roles=()):
    if DRY:
        print(f"[dry] upload {name} to {channel}: {content[:200]}")
        return {}
    boundary = "riffhouse" + str(int(time.time() * 1000))
    body_json = {"content": content, "allowed_mentions": {"parse": [], "roles": list(ping_roles)},
                 "attachments": [{"id": 0, "filename": name}]}
    if embeds:
        body_json["embeds"] = embeds
    if data is None:
        with open(path, "rb") as f:
            data = f.read()
    body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"payload_json\"\r\nContent-Type: application/json\r\n\r\n"
            f"{json.dumps(body_json)}\r\n--{boundary}\r\nContent-Disposition: form-data; name=\"files[0]\"; filename=\"{name}\"\r\n"
            f"Content-Type: {mime}\r\n\r\n").encode() + data + f"\r\n--{boundary}--\r\n".encode()
    req = urllib.request.Request(f"{API}/channels/{channel}/messages", data=body, method="POST")
    req.add_header("Authorization", f"Bot {os.environ.get('DISCORD_BOT_TOKEN', '')}")
    req.add_header("User-Agent", UA)
    req.add_header("Content-Type", f"multipart/form-data; boundary={boundary}")
    with urllib.request.urlopen(req, timeout=120) as r:
        return json.loads(r.read())


def preset_file(link: str) -> tuple[str, bytes] | None:
    """The preset .json behind a tone link, so people can grab the file straight from Discord."""
    got = re.match(r"https://github\.com/([^/]+/[^/]+)/blob/([^/]+)/(presets/[\w.\-]+\.json)$", link or "")
    if not got:
        return None
    raw = f"https://raw.githubusercontent.com/{got.group(1)}/{got.group(2)}/{got.group(3)}"
    try:
        with urllib.request.urlopen(urllib.request.Request(raw, headers={"User-Agent": UA}), timeout=30) as r:
            return os.path.basename(got.group(3)), r.read()
    except urllib.error.URLError:
        return None


def post_reels(log: list[str]):
    if not os.path.exists(REELS):
        return
    with open(REELS, encoding="utf-8") as f:
        reels = json.load(f)
    if not reels:
        return
    chan = CH["content"]
    done = set()
    for m in my_messages(chan):
        done.update(a["filename"] for a in m.get("attachments", []))
        done.add(m.get("content", ""))
    now = dt.datetime.now(dt.timezone.utc)
    for r in reels:
        rid, link, caption = r["id"], r.get("link", ""), r.get("caption", "").strip()
        if r.get("post_at"):  # queued: goes out on its own at this time (ISO, with a time zone)
            when = dt.datetime.fromisoformat(r["post_at"].replace("Z", "+00:00"))
            if when.tzinfo is None:
                when = when.replace(tzinfo=dt.timezone(dt.timedelta(hours=5, minutes=30)))  # India time
            if now < when:
                continue
        name = f"{rid}.mp4"
        content = (caption + ("\n" + link if link else "")).strip()
        if name in done or any(link and link in d for d in done) or content in done:
            continue
        video = os.path.join(ROOT, r["video"]) if r.get("video") else None
        if video and os.path.exists(video) and os.path.getsize(video) <= UPLOAD_LIMIT:
            msg = send_file(chan, content, video, name)
        elif link:
            msg = send(chan, content)
        else:
            print(f"warning: reel {rid}: video missing or over 10 MB and no link, skipped", file=sys.stderr)
            continue
        if msg.get("id"):
            try:
                call("POST", f"/channels/{chan}/messages/{msg['id']}/crosspost")
            except RuntimeError as e:
                print(f"warning: crosspost: {e}", file=sys.stderr)
        log.append(f"Posted reel **{rid}** in <#{chan}>")
        # a big one (the trailer) also gets a short @everyone pointer in #announcements
        if r.get("announce") and msg.get("id"):
            text = f"@everyone {r['announce'].strip()}\nhttps://discord.com/channels/{GUILD}/{chan}/{msg['id']}"
            a = call("POST", f"/channels/{CH['announcements']}/messages",
                     {"content": text, "allowed_mentions": {"parse": ["everyone"]}})
            if a.get("id"):
                try:
                    call("POST", f"/channels/{CH['announcements']}/messages/{a['id']}/crosspost")
                except RuntimeError as e:
                    print(f"warning: crosspost: {e}", file=sys.stderr)
            log.append(f"Pointed #announcements to **{rid}** with @everyone")


# ---------------------------------------------------------------- 1c. launch event

LAUNCH = os.path.join(ROOT, "community", "launch.json")
FOUNDER = "1570505112076427435"


def launch_event(log: list[str]):
    """On release day: a thread in #general, and the first people to post in it get a role for good.
    Starts by itself when the release named in community/launch.json shows up in the feed."""
    if not os.path.exists(LAUNCH):
        return
    with open(LAUNCH, encoding="utf-8") as f:
        cfg = json.load(f)
    tag, limit, role, name = cfg["version"], int(cfg.get("limit", 50)), cfg["role"], cfg["thread_name"]
    if not any(i["guid"] == f"release-{tag}" for i in read_feed("builds.xml")):
        return
    gen = CH["general"]
    th = next((t for t in call("GET", f"/guilds/{GUILD}/threads/active").get("threads", [])
               if t.get("parent_id") == gen and t["name"] == name), None)
    if th is None:
        archived = call("GET", f"/channels/{gen}/threads/archived/public?limit=50").get("threads", [])
        if any(t["name"] == name for t in archived):
            return  # event already ran
        th = call("POST", f"/channels/{gen}/threads", {"name": name, "type": 11, "auto_archive_duration": 10080},
                  reason="Launch event")
        if not th.get("id"):
            return
        call("POST", f"/channels/{th['id']}/messages", {"content": (
            f"{tag.lstrip('v')} is out. first {limit} people to drop a message in here get the **Day One** role, and it stays forever.\n"
            "say what you're playing through it, or just say hi. update from Settings > Check for updates, "
            "or grab it at https://natikchhabra.github.io/riffhouse/"), "allowed_mentions": {"parse": []}})
        log.append(f"Launch event started: <#{th['id']}>")
        return
    if th.get("thread_metadata", {}).get("locked"):
        return
    members = []
    after = "0"
    while True:
        page = call("GET", f"/guilds/{GUILD}/members?limit=1000&after={after}")
        members += page
        if len(page) < 1000:
            break
        after = page[-1]["user"]["id"]
    have = {m["user"]["id"] for m in members if role in m["roles"]}
    staff = {m["user"]["id"] for m in members if FOUNDER in m["roles"] or m["user"].get("bot")}
    msgs = sorted(call("GET", f"/channels/{th['id']}/messages?limit=100"), key=lambda m: int(m["id"]))
    for m in msgs:
        if len(have) >= limit:
            break
        uid = m["author"]["id"]
        if m["author"].get("bot") or uid in staff or uid in have or m.get("webhook_id"):
            continue
        call("PUT", f"/guilds/{GUILD}/members/{uid}/roles/{role}", reason="Launch event: first 50")
        have.add(uid)
        log.append(f"Day One: <@{uid}> ({len(have)}/{limit})")
    if len(have) >= limit:
        call("POST", f"/channels/{th['id']}/messages", {"content": f"that's {limit}. thanks for showing up on day one, means a lot",
                                                         "allowed_mentions": {"parse": []}})
        call("PATCH", f"/channels/{th['id']}", {"locked": True, "archived": True}, reason="Launch event full")
        log.append(f"Launch event full, thread closed")


# ---------------------------------------------------------------- 1d. welcome

def welcome(log: list[str]):
    """Say hi by name to everyone who joined in the last 3 days and isn't welcomed yet."""
    since = dt.datetime.now(dt.timezone.utc) - dt.timedelta(days=3)
    done = set()
    for m in my_messages(CH["welcome"]):
        done |= {u["id"] for u in m.get("mentions", [])}
    new, after = [], "0"
    while True:
        page = call("GET", f"/guilds/{GUILD}/members?limit=1000&after={after}")
        for m in page:
            u = m["user"]
            if (u.get("bot") or m.get("pending") or FOUNDER in m["roles"] or u["id"] in done
                    or dt.datetime.fromisoformat(m["joined_at"]) < since):
                continue
            new.append((m["joined_at"], u["id"]))
        if len(page) < 1000:
            break
        after = page[-1]["user"]["id"]
    if not new:
        return
    ids = [i for _, i in sorted(new)][:25]
    names = " ".join(f"<@{i}>" for i in ids)
    text = (f"hey {names}, welcome to Riff House\n\n"
            "it's a free guitar amp app for Windows. amps, pedals, cabs, no paywall on tone\n\n"
            "get it here: https://natikchhabra.github.io/riffhouse/\n"
            f"stuck on anything? make a post in <#{CH['help']}>\n"
            "want builds before everyone else? grab the Beta tester role in <id:customize>\n\n"
            f"then say hi in <#{CH['general']}> and tell us what you play")
    call("POST", f"/channels/{CH['welcome']}/messages",
         {"content": text, "allowed_mentions": {"parse": [], "users": ids}})
    log.append(f"Welcomed {names} in <#{CH['welcome']}>")


# ---------------------------------------------------------------- 2. FAQ answers

# Each answer: words that must appear (any group matching counts), the reply, and the help page section.
# Written the way Natik talks: short, plain, no greeting fluff.
FAQ = [
    {
        "key": "other-apps",
        "match": [r"(youtube|spotify|other apps?|browser|discord).*(no sound|silent|stops?|mute|can'?t hear)",
                  r"play along", r"backing track"],
        "text": (
            "Riff House takes the sound card for itself, that's where the low delay comes from. To play along, drop the song on the window or use **Play along**. "
            "To hear other apps at the same time, open **Input, output and delay...** and turn on **Let other apps play sound at the same time** (costs a few ms)."
        ),
        "anchor": "other-programs-go-silent-while-riffhouse-is-open",
    },
    {
        "key": "no-sound",
        "match": [r"\bno (sound|audio|signal)\b", r"can'?t hear", r"cannot hear", r"\bnot hearing\b",
                  r"\bsilent\b", r"\bnothing (comes out|plays|happens when i play)", r"doesn'?t (pick up|detect|hear)",
                  r"guitar (not|isn'?t) (detected|showing|working)"],
        "text": (
            "Most of the time it's one of these:\n"
            "1. In setup, pick your input from the list yourself instead of Find it for me. A quiet guitar on a mic jack gets missed.\n"
            "2. Windows Settings > Privacy & security > Microphone > turn on **Let desktop apps access your microphone**. Your guitar input counts as a mic.\n"
            "3. Check the input isn't muted in Windows Settings > System > Sound.\n"
            "4. Laptop with one combo headphone/mic jack? A normal guitar cable usually doesn't reach the mic contact. You need a splitter, a USB guitar cable or an interface."
        ),
        "anchor": "riffhouse-cannot-hear-my-guitar",
    },
    {
        "key": "delay",
        "match": [r"\blatency\b", r"\bdelay(ed)?\b", r"\blag(gy|ging)?\b", r"\bout of sync\b", r"\blate\b.*\bplay"],
        "text": (
            "Usual causes of delay:\n"
            "- **Bluetooth headphones or speakers** always add 100 ms+, nothing fixes that. Use wired ones.\n"
            "- Guitar on one USB device and headphones on another adds delay. Plug headphones into the same thing the guitar goes into.\n"
            "- Close other audio apps and hit **Run automatic setup again** in Settings, it picks the fastest mode that runs clean.\n"
            "- Got an interface? Install its driver and pick the ASIO entry.\n"
            "Setup shows the number. Under ~12 ms feels instant."
        ),
        "anchor": "there-is-a-delay-between-playing-and-hearing",
    },
    {
        "key": "crackle",
        "match": [r"crackl", r"\bpops?\b", r"popping", r"dropouts?", r"stutter", r"glitch(y|ing)"],
        "text": (
            "Crackles are almost always the buffer being too small for the PC right now.\n"
            "- Settings > **Driver, input and output...** > pick the next bigger number under Buffer.\n"
            "- Close browsers and games, and plug the laptop into power.\n"
            "- Run setup again, it only keeps settings that ran without a single dropout."
        ),
        "anchor": "crackles-pops-or-dropouts",
    },
    {
        "key": "asio",
        "match": [r"\basio\b", r"asio4all", r"flexasio"],
        "text": (
            "On ASIO:\n"
            "- With an interface, install its maker's driver and pick its ASIO entry. That's the fastest.\n"
            "- ASIO4ALL and FlexASIO need every device to themselves, and fail if Windows or another app holds one. On those PCs Windows Audio (exclusive) is usually just as fast.\n"
            "- Only one app can hold most ASIO drivers. Close your DAW and try again.\n"
            "- From 0.1.5 a driver that hangs gets tested in the background first, so it can't freeze the window."
        ),
        "anchor": "what-about-asio",
    },
    {
        "key": "twice",
        "match": [r"hear (it|my guitar|the guitar|myself) twice", r"\bdouble(d)? (sound|signal|guitar)\b",
                  r"clean (guitar|signal|sound) (on top|under|behind|too|as well)", r"direct monitor"],
        "text": (
            "The second clean one comes from your interface, not Riff House. Turn off **Direct Monitor** on the interface "
            "(or turn the Mix knob all the way to PC/DAW/Playback) and you'll only hear the amp."
        ),
        "anchor": "i-hear-my-guitar-twice",
    },
    {
        "key": "smartscreen",
        "match": [r"windows protected", r"smartscreen", r"\bvirus\b", r"defender", r"unknown publisher", r"\bsafe to (install|run|download)"],
        "text": (
            "That blue box is just because the installer isn't signed with a paid certificate. Hit **More info** then **Run anyway**. "
            "The code is all public on GitHub, and the release page lists the SHA-256 if you want to check the file."
        ),
        "anchor": "installing",
    },
    {
        "key": "harsh",
        "match": [r"\b(harsh|thin|fizzy|buzzy|muddy)\b.*\b(tone|sound|gain|distortion|metal)",
                  r"\b(tone|sound|gain|distortion|metal)\b.*\b(harsh|thin|fizzy|buzzy|muddy)\b",
                  r"\bsounds? (bad|terrible|awful|weird)\b"],
        "text": (
            "If you're on a computer mic jack, it eats most of the treble and high gain then turns to mush.\n"
            "- Run setup. It measures what your input loses and sets **Input Fix** for it.\n"
            "- Keep the guitar's volume knob all the way up.\n"
            "- In the play as hard as you ever will step, really dig in. Input level is set from that."
        ),
        "anchor": "heavy-tones-sound-harsh-thin-or-buzzy",
    },
    {
        "key": "hum",
        "match": [r"\bhum(ming)?\b", r"\bhiss(ing)?\b", r"\bbuzz(ing)?\b", r"\bnoise\b"],
        "text": (
            "For hum and hiss:\n"
            "- The mains hum filter in Settings is on Auto, it removes 50/60 Hz hum by itself.\n"
            "- Setup sets the noise gate just above your background noise.\n"
            "- Single coils hum near screens and chargers, turning a bit often finds a quiet spot.\n"
            "- Laptop on charger buzzing through the mic jack? Try it on battery."
        ),
        "anchor": "hum-buzz-and-hiss",
    },
    {
        "key": "daw",
        "match": [r"\bvst3?\b", r"\bdaw\b", r"reaper", r"fl studio", r"ableton", r"cubase", r"\bplugin\b"],
        "text": (
            "Riff House also installs as a VST3 into your user's VST3 folder. If your DAW doesn't list it, rescan plugins and add that folder. "
            "Full steps for each DAW: https://github.com/NatikChhabra/riffhouse/blob/main/docs/DAW.md"
        ),
        "anchor": None,
    },
    {
        "key": "crash",
        "match": [r"\bcrash", r"\bfroze\b", r"\bfreez", r"closes? (by itself|on its own)", r"not responding", r"won'?t (open|start|launch)"],
        "text": (
            "First make sure you're on the newest version (Settings > **Check for updates**, or run the newest installer over it, your tones stay).\n"
            "If it still happens, open it again and hit **Send a report** when it asks. That report says exactly what it was doing when it went down, it's how these get fixed."
        ),
        "anchor": "it-froze-or-closed-by-itself",
    },
]

SIGN_OFF = "\n\nIf that doesn't sort it, say what you tried and someone will dig in."


def faq_for(text: str) -> dict | None:
    t = text.lower()
    scored = []
    for i, f in enumerate(FAQ):
        hits = sum(1 for p in f["match"] if re.search(p, t))
        if hits:
            scored.append((hits, -i, f))
    return max(scored)[2] if scored else None


def answer_forums(threads: list[dict], log: list[str]):
    now = dt.datetime.now(dt.timezone.utc)
    for th in threads:
        if th.get("parent_id") not in FAQ_FORUMS:
            continue
        if now - snowflake_time(th["id"]) > dt.timedelta(days=2):
            continue
        msgs = call("GET", f"/channels/{th['id']}/messages?limit=50")
        if any(m["author"]["id"] == me() for m in msgs):
            continue
        humans = [m for m in msgs if not m["author"].get("bot")]
        if not humans:
            continue
        # someone already helped: don't jump in late
        starter_author = th.get("owner_id")
        if any(m["author"]["id"] != starter_author for m in humans):
            continue
        text = th["name"] + "\n" + "\n".join(m.get("content", "") for m in humans)
        f = faq_for(text)
        if not f:
            continue
        link = f"{HELP_PAGE}#{f['anchor']}" if f["anchor"] else ""
        body = f["text"] + (f"\n\nMore detail: <{link}>" if link else "") + SIGN_OFF
        send(th["id"], body[:2000])
        log.append(f"Answered <#{th['id']}> with the **{f['key']}** fix")


# ---------------------------------------------------------------- 2b. tidy up

DONE_TAGS = {"solved", "fixed", "done"}


def close_finished(threads: list[dict], log: list[str]):
    """Posts tagged Solved, Fixed or Done close themselves after a quiet day. Anyone can reopen by replying."""
    done = {}
    for forum in {t.get("parent_id") for t in threads} & set(FORUMS):
        for tag in call("GET", f"/channels/{forum}").get("available_tags", []):
            if tag["name"].lower() in DONE_TAGS:
                done[tag["id"]] = tag["name"]
    now = dt.datetime.now(dt.timezone.utc)
    for th in threads:
        if not set(th.get("applied_tags", [])) & set(done):
            continue
        last = th.get("last_message_id") or th["id"]
        if now - snowflake_time(last) < dt.timedelta(days=1):
            continue
        call("PATCH", f"/channels/{th['id']}", {"archived": True}, reason="Marked done and quiet for a day")
        log.append(f"Closed <#{th['id']}> (marked done, quiet for a day)")


# ---------------------------------------------------------------- 2c. keep channels on purpose, triage

CHAT = ("general", "gear-talk", "off-topic", "showcase")
TROUBLE = re.compile(r"\b(help|problem|issue|not working|doesn'?t work|won'?t|can'?t|cannot|error|fix|stuck|bug|broken)\b")
APP = re.compile(r"\b(riff ?house|the app|app|setup|asio|latency|driver|input|interface|installer|update)\b")
IDEA = re.compile(r"\b(please add|can you add|could you add|would be (cool|nice|great|sick)|feature request|suggestion|wish it had|you should add|add (a|an|the) \w+ (amp|pedal|cab))\b")
# a bug report in chat: a failure word AND a Riff House word, and nothing that says it's about something else
BUG_WHAT = re.compile(r"\b(crash(es|ed|ing)|froze|freez(es|ing)|keeps? (closing|quitting)|(closes|quits|shuts down) (by itself|on its own|randomly)|found a bug|(is|it'?s|got) (a )?bug(gy|ged)?|glitch(es|ed|ing|y)?|stopped working|stops working|broke after|broken after)\b")
BUG_WHERE = re.compile(r"\b(riff ?house|the app|this app|app|preset|tuner|metronome|installer|after (the|an?) update|0\.\d+\.\d+|v0\.\d|settings|play along|nam capture)\b")
CASUAL = re.compile(r"\b(game|valorant|minecraft|pc build|windows update|my phone|discord|crash (course|cymbal|pad)|bug spray|insect|lol|lmao|haha|crashed and burned|car|bike|server|website|stock|market|spotify|youtube|chrome|browser|instagram|whatsapp|app store|play store|steam)\b")
NUDGED_FOOTER = "Riff House nudge"


def recent_nudges(channel: str) -> set[str]:
    """Users the bot already pointed somewhere in this channel in the last day (one nudge a day, no nagging)."""
    out, now = set(), dt.datetime.now(dt.timezone.utc)
    for m in my_messages(channel, 100):
        if now - snowflake_time(m["id"]) < dt.timedelta(days=1):
            out.update(u["id"] for u in m.get("mentions", []))
    return out


def keep_on_purpose(cursor: str | None, log: list[str]):
    """A help question in chat gets the fix (when known) and a pointer to #help; an idea gets pointed to #ideas,
    a bug report gets pointed to #bug-reports and the in-app Report button. Only after 10 quiet minutes with no reply from anyone else, so it never talks over people."""
    now = dt.datetime.now(dt.timezone.utc)
    since = snowflake_time(cursor) - dt.timedelta(minutes=15) if cursor else now - dt.timedelta(hours=1)
    for name in CHAT:
        chan = CH[name]
        msgs = call("GET", f"/channels/{chan}/messages?limit=100")
        nudged = recent_nudges(chan)
        for m in reversed(msgs):  # oldest first, so a person's first question is the one answered
            t = snowflake_time(m["id"])
            if m["author"].get("bot") or t < since or now - t < dt.timedelta(minutes=10):
                continue
            uid, text = m["author"]["id"], (m.get("content") or "").lower()
            if uid in nudged or len(text) < 12:
                continue
            answered = any(o["author"]["id"] != uid and not o["author"].get("bot") and snowflake_time(o["id"]) > t
                           and (o.get("message_reference") or {}).get("message_id") == m["id"] for o in msgs)
            if answered:
                continue
            ref = {"message_id": m["id"], "fail_if_not_exists": False}
            if IDEA.search(text):
                body = f"<@{uid}> nice one, drop it in <#{CH['ideas']}> as its own post so people can vote on it. The most voted ones get built first."
            elif "?" in text and TROUBLE.search(text) and APP.search(text):
                f = faq_for(text)
                fix = (f["text"] + "\n\n") if f else ""
                body = (f"<@{uid}> {fix}If that doesn't do it, make a post in <#{CH['help']}> with your Riff House version and what your guitar plugs into. "
                        "Stuff there doesn't get buried and it helps the next person too.")
            elif BUG_WHAT.search(text) and BUG_WHERE.search(text) and not CASUAL.search(text):
                body = (f"<@{uid}> sorry about that. Quickest way to get it fixed is **Report a problem** inside Riff House, it sends us your logs. "
                        f"Or make a post in <#{CH['bug-reports']}> with your version and what you were doing when it happened.")
            else:
                continue
            call("POST", f"/channels/{chan}/messages", {"content": body[:2000], "message_reference": ref,
                                                         "allowed_mentions": {"users": [uid], "replied_user": False}})
            nudged.add(uid)
            log.append(f"Pointed <@{uid}> from <#{chan}> to {'#ideas' if IDEA.search(text) else '#help' if '?' in text else '#bug-reports'}")


def triage(threads: list[dict], cursor: str | None, log: list[str]):
    """Puts what needs a person at the top of the log: help and bug posts with no human answer after an hour."""
    if not cursor:
        return
    now = dt.datetime.now(dt.timezone.utc)
    last = snowflake_time(cursor)
    for th in threads:
        if th.get("parent_id") not in (CH["help"], CH["bug-reports"], CH["beta-feedback"]):
            continue
        due = snowflake_time(th["id"]) + dt.timedelta(hours=1)
        if not (last - dt.timedelta(minutes=1) < due <= now):  # flag each post once, when it crosses the hour
            continue
        msgs = call("GET", f"/channels/{th['id']}/messages?limit=50")
        if any(not m["author"].get("bot") and m["author"]["id"] != th.get("owner_id") for m in msgs):
            continue
        urgent = re.search(r"crash|froze|freez|won'?t (open|start)|lost|deleted|install", (th["name"] + " ".join(m.get("content", "") for m in msgs)).lower())
        log.insert(0, f"{'URGENT ' if urgent else ''}Needs a reply: <#{th['id']}> in <#{th['parent_id']}>, waiting over an hour")


# ---------------------------------------------------------------- 3. log

AUDIT = {20: "kicked", 22: "banned", 23: "unbanned", 72: "message deleted by a mod", 143: "AutoMod blocked a message",
         144: "AutoMod flagged a message", 145: "AutoMod timed out", 24: "member updated"}


def log_cursor() -> str:
    for m in my_messages(CH["bot-log"], 50):
        for e in m.get("embeds", []):
            got = re.search(r"cursor (\d+)", (e.get("footer") or {}).get("text", ""))
            if got:
                return got.group(1)
    return time_snowflake(dt.datetime.now(dt.timezone.utc) - dt.timedelta(hours=1))


def collect_log(threads: list[dict], cursor: str, log: list[str]) -> str:
    since = snowflake_time(cursor)
    top = cursor

    # joins (the member list is small; page through it)
    after = "0"
    while True:
        page = call("GET", f"/guilds/{GUILD}/members?limit=1000&after={after}")
        for m in page:
            joined = dt.datetime.fromisoformat(m["joined_at"])
            if joined > since and not m["user"].get("bot"):
                age = (dt.datetime.now(dt.timezone.utc) - snowflake_time(m["user"]["id"])).days
                flag = " (new account, watch it)" if age < 7 else ""
                state = "still on the rules screen" if m.get("pending") else "in"
                log.append(f"Joined: <@{m['user']['id']}> `{m['user']['username']}`, account {age} days old, {state}{flag}")
        if len(page) < 1000:
            break
        after = page[-1]["user"]["id"]

    # new forum posts, so nothing gets missed
    for th in threads:
        if th.get("parent_id") in FORUMS and int(th["id"]) > int(cursor):
            log.append(f"New post in <#{th['parent_id']}>: <#{th['id']}>")
            top = max(top, th["id"], key=int)

    # moderation, from the audit log
    entries = call("GET", f"/guilds/{GUILD}/audit-logs?limit=100&after={cursor}").get("audit_log_entries", [])
    for e in sorted(entries, key=lambda e: int(e["id"])):
        top = max(top, e["id"], key=int)
        what = AUDIT.get(e["action_type"])
        if not what:
            continue
        if e["action_type"] == 24:
            ch = {c["key"]: c.get("new_value") for c in e.get("changes", [])}
            if "communication_disabled_until" not in ch:
                continue
            what = "timed out" if ch["communication_disabled_until"] else "timeout removed"
        who = f"<@{e['target_id']}>" if e.get("target_id") else "someone"
        by = f" by <@{e['user_id']}>" if e.get("user_id") else ""
        why = f": {e['reason']}" if e.get("reason") else ""
        log.append(f"Mod: {who} {what}{by}{why}")
    # members who left show up as missing; Discord gives no event for that over REST, so we only log what we can see.
    return max(top, time_snowflake(dt.datetime.now(dt.timezone.utc) - dt.timedelta(seconds=5)), key=int)


def write_log(log: list[str], cursor: str):
    if not log:
        return
    chunks, cur = [], ""
    for line in log:
        if len(cur) + len(line) + 1 > 3800:
            chunks.append(cur)
            cur = ""
        cur += line + "\n"
    chunks.append(cur)
    for i, c in enumerate(chunks):
        footer = f"cursor {cursor}" if i == len(chunks) - 1 else "continued"
        send(CH["bot-log"], "", [{"description": c, "color": 0x2B2D31, "footer": {"text": footer},
                                  "timestamp": dt.datetime.now(dt.timezone.utc).isoformat()}])


# ---------------------------------------------------------------- main

def main() -> int:
    if not os.environ.get("DISCORD_BOT_TOKEN"):
        print("::warning::DISCORD_BOT_TOKEN is not set, nothing to do")
        return 0
    log: list[str] = []
    failed = False
    try:
        cursor = log_cursor()
    except Exception as e:
        print(f"error: can't read #bot-log: {e}", file=sys.stderr)
        cursor, failed = None, True
    threads = call("GET", f"/guilds/{GUILD}/threads/active").get("threads", [])
    # Natik, 13 Nov 2026: the bot only posts releases, dev log, tone of the week, reels and the trailer.
    # No replies to people (help answers, chat pointers): those go through Natik's weekly review.
    for step in (lambda: post_feeds(log), lambda: post_reels(log), lambda: close_finished(threads, log)):
        try:
            step()
        except Exception as e:  # one broken part must not stop the others
            print(f"error: {e}", file=sys.stderr)
            failed = True
    if cursor:
        try:
            write_log(log, collect_log(threads, cursor, log))
        except Exception as e:
            print(f"error: log: {e}", file=sys.stderr)
            failed = True
    print("\n".join(log) or "nothing new")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
