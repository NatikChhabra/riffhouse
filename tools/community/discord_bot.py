#!/usr/bin/env python3
"""The Riff House Discord bot. Runs every few minutes from GitHub Actions, no server needed.

Each run it:
  1. Posts new items from the site feeds (feeds/*.xml) into their channels:
       release -> #releases (pings Release pings, then publishes to following servers)
       beta    -> #beta-lab (pings Beta tester)
       devlog  -> #dev-log  (all new changes in one message)
       tone    -> #tone-of-the-week
  2. Answers common questions in the #help and #bug-reports forums when a new post
     clearly matches one (sound, delay, crackles, ASIO, install warning, ...), with the fix
     and a link to the full help page. Posts it can't match are left for a person.
  3. Closes forum posts tagged Solved, Fixed or Done after a quiet day.
  4. Writes a log to #bot-log: who joined or left, new forum posts, and moderation
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


def post_feeds(log: list[str]):
    builds = read_feed("builds.xml")
    plan = [
        ("releases", [i for i in builds if i["category"] == "release"], 1, "release-pings", True),
        ("beta-lab", [i for i in builds if i["category"] == "beta"], 1, "beta-tester", False),
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
                "description": short(it["description"], 1800),
                "footer": {"text": f"Riff House #{it['guid']}"},
            }
            if it["date"]:
                embed["timestamp"] = it["date"].isoformat()
            msg = send(CH[chan], f"<@&{ping[0]}>" if ping else "", [embed], ping)
            if publish and msg.get("id"):
                try:
                    call("POST", f"/channels/{CH[chan]}/messages/{msg['id']}/crosspost")
                except RuntimeError as e:
                    print(f"warning: crosspost: {e}", file=sys.stderr)
            log.append(f"Posted **{it['title']}** in <#{CH[chan]}>")

    # The dev log can move fast, so every new change since the last run goes in one message.
    new = fresh(read_feed("devlog.xml"), CH["dev-log"], 3)
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
    for step in (lambda: post_feeds(log), lambda: answer_forums(threads, log), lambda: close_finished(threads, log)):
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
