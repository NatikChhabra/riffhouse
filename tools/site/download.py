#!/usr/bin/env python3
"""Keeps the site's download buttons on the newest installer, with nobody editing a link.

  download.py stamp <site folder>   write the newest installer's address into every download button in that folder.
                                    Run by the publish (pages.yml), which also runs after every release.
  download.py check [stamp]         fetch the pages from riffhouse.in, follow each download button and fail unless the
                                    file it gives is the newest release's installer. With a stamp (the first eight
                                    letters of the commit), it first waits for that publish to be live.

The newest installer is the site's own rule (tools/site/check_release.py): the newest release that is not a draft
or a test release and has a Windows installer.
"""
import os
import pathlib
import re
import sys
import time
import urllib.request

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from check_release import PAGES, REPO, newest_release  # noqa: E402

LIVE = "https://riffhouse.in/"
BUTTON = re.compile(r'(<a\b[^>]*\bdata-download\b[^>]*\bhref=")([^"]*)(")')
LATEST = f"https://github.com/{REPO}/releases/latest"


def fetch(url, headers=None):
    req = urllib.request.Request(url, headers={"User-Agent": "site-check", **(headers or {})})
    return urllib.request.urlopen(req, timeout=30)


def stamp(folder):
    try:
        tag, _, installer = newest_release()
    except SystemExit as err:  # GitHub cannot be asked: publish anyway, the check after the publish will say so
        print(f"download: {err}. The buttons keep the link to the releases page")
        return
    if not installer:
        print(f"download: {tag} has no installer yet, the buttons keep the link to the releases page")
        return
    url, done = installer["browser_download_url"], 0
    for name in PAGES:
        path = pathlib.Path(folder) / name
        page = path.read_text(encoding="utf-8")
        fresh, n = BUTTON.subn(lambda m: m.group(1) + url + m.group(3), page)
        path.write_text(fresh, encoding="utf-8")
        done += n
    if not done:
        sys.exit("download: no download button found to write the installer into")
    print(f"download: {done} buttons now give {installer['name']}")


def check(stamp_id):
    tag, problems, installer = newest_release()
    if not installer:
        sys.exit(f"download: {'; '.join(problems)}")
    want = installer["browser_download_url"]
    wrong = list(problems)
    for name in PAGES:
        page = ""
        for wait in ([0] + [15] * 16 if stamp_id else [0, 15]):  # a publish takes a minute or two to reach every visitor
            time.sleep(wait)
            try:
                page = fetch(LIVE + name, {"Cache-Control": "no-cache"}).read().decode("utf-8")
            except Exception as err:  # a network hiccup: try again
                page = f"<!-- {err} -->"
            if not stamp_id or f"?v={stamp_id}" in page:
                break
        else:
            wrong.append(f"{name}: the publish {stamp_id} is not live after four minutes")
            continue
        links = {m.group(2) for m in BUTTON.finditer(page)}
        if not links:
            wrong.append(f"{name}: no download button found")
        for link in sorted(links - {want}):
            wrong.append(f"{name}: a download button gives {link}, the newest installer is {installer['name']}")
    # The file itself: what a visitor's browser saves when the button is pressed.
    try:
        got = fetch(want, {"Range": "bytes=0-0"}).headers.get("Content-Disposition", "")
    except Exception as err:
        got = f"no answer ({err})"
    if installer["name"] not in got:
        wrong.append(f"the button's link saves '{got}', it should save {installer['name']}")
    if wrong:
        print(f"download: the site does not give the newest installer ({tag})")
        for line in wrong:
            print(f"  - {line}")
            if os.environ.get("GITHUB_ACTIONS"):  # shows the line on the run's summary page
                print(f"::error title=Download button is not on {tag}::{line}")
        sys.exit(1)
    print(f"download: every download button on riffhouse.in gives {installer['name']}")


def main(argv):
    if len(argv) == 3 and argv[1] == "stamp":
        stamp(argv[2])
    elif argv[1:2] == ["check"] and len(argv) <= 3:
        check(argv[2] if len(argv) == 3 else "")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
