#!/usr/bin/env python3
"""Writes the facts search engines and AI assistants read from the home page.

  schema.py           rewrite the data block in site/index.html, site/llms.txt and the dates in site/sitemap.xml
  schema.py --check   change nothing; fail if any of them is out of date

The data block (schema.org, in the page head) is built from what the page itself says, so the two cannot disagree:
the version and date come from the line under the download button, the counts from the page text, and the questions
and answers from the Questions section. Run it after changing any of those. tools/site/check_release.py runs the check.
"""
import datetime
import html
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SITE = ROOT / "site"
HOME = "https://riffhouse.in/"
REPO = "https://github.com/NatikChhabra/riffhouse"
FIRST_RELEASE = "2026-11-11"  # 0.1.1
BLOCK = re.compile(r'(<script type="application/ld\+json">\n).*?(\n</script>)', re.S)
RELEASE = re.compile(r"data-release[^>]*>Version ([0-9.]+), released (\d{1,2} [A-Z][a-z]+ \d{4})\.<")


def text(fragment):
    return html.unescape(re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", fragment))).strip()


def release(page):
    m = RELEASE.search(page)
    if not m:
        sys.exit("schema: index.html has no 'Version X, released D Month YYYY.' line under the download button")
    day = datetime.datetime.strptime(m.group(2), "%d %B %Y").date()
    return m.group(1), day.isoformat()


def count(page, thing):
    m = re.search(rf"(?<![\w.,])(\d{{1,3}}) {thing}\b", text(page))
    if not m:
        sys.exit(f"schema: index.html does not say how many {thing} there are")
    return int(m.group(1))


def build(page):
    version, day = release(page)
    amps, pedals, cabs, tones = (count(page, t) for t in ("amps", "pedals", "cabinets", "tones"))
    faq = re.findall(r'<details name="faq"><summary>(.*?)</summary><p>(.*?)</p></details>', page, flags=re.S)
    if not faq:
        sys.exit("schema: index.html has no questions")
    person = {"@type": "Person", "name": "Natik Chhabra", "url": "https://github.com/NatikChhabra",
              "sameAs": ["https://www.linkedin.com/in/natikchhabra/", "https://github.com/NatikChhabra"]}
    app = {
        "@type": "SoftwareApplication",
        "@id": HOME + "#app",
        "name": "Riffhouse",
        "alternateName": "Riff House",
        "url": HOME,
        "description": f"Riffhouse is a free, open-source guitar amp simulator and pedalboard for Windows 10 and 11. "
                       f"It runs on its own or as a VST3 plugin, with {amps} amps, {pedals} pedals, {cabs} cabinets and "
                       f"{tones} tones, and it works through a computer's mic jack with no audio interface.",
        "applicationCategory": "MultimediaApplication",
        "applicationSubCategory": "Guitar amp simulator",
        "operatingSystem": "Windows 10, Windows 11 (64-bit)",
        "softwareVersion": version,
        "datePublished": FIRST_RELEASE,
        "dateModified": day,
        "isAccessibleForFree": True,
        "offers": {"@type": "Offer", "price": "0", "priceCurrency": "USD", "availability": "https://schema.org/InStock"},
        "license": "https://www.gnu.org/licenses/agpl-3.0.html",
        "downloadUrl": REPO + "/releases/latest",
        "releaseNotes": HOME + "updates.html",
        "softwareHelp": REPO + "/blob/main/docs/TROUBLESHOOTING.md",
        "image": HOME + "img/og.jpg",
        "screenshot": HOME + "press/screenshots/amp.png",
        "featureList": [
            f"{amps} amps, {pedals} pedals, {cabs} cabinets and {tones} ready tones",
            "Standalone app and VST3 plugin",
            "Works through a computer's mic jack, no audio interface needed",
            "Setup that finds the guitar input and sets levels by itself",
            "Loads .nam amp captures and cabinet impulse responses",
            "Tuner, looper, metronome, play along with slow-down, take recorder",
            "No account, no ads, no paid version",
            "Open source under the AGPL-3.0 licence",
        ],
        "author": person,
        "sameAs": [REPO, "https://www.instagram.com/realriffhouse/", "https://www.youtube.com/@riffhouseapp",
                   "https://www.reddit.com/user/Riff-House/"],
    }
    site = {"@type": "WebSite", "@id": HOME + "#site", "name": "Riffhouse", "url": HOME, "inLanguage": "en",
            "about": {"@id": HOME + "#app"}, "publisher": person}
    questions = {"@type": "FAQPage", "@id": HOME + "#questions", "mainEntity": [
        {"@type": "Question", "name": text(q), "acceptedAnswer": {"@type": "Answer", "text": text(a)}} for q, a in faq]}
    data = {"@context": "https://schema.org", "@graph": [app, site, questions]}
    block = json.dumps(data, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
    return block, day, llms(app, version, day, [(text(q), text(a)) for q, a in faq])


def llms(app, version, day, faq):
    """site/llms.txt: the same facts as plain text, for assistants that read it (llmstxt.org)."""
    lines = [
        "# Riffhouse",
        "",
        "> " + app["description"],
        "",
        f"Current version: {version}, released {day}. Price: free (0 USD), with no account, no ads and no paid version. "
        "Platform: Windows 10 and 11, 64-bit. Licence: AGPL-3.0, open source. Made by Natik Chhabra.",
        "",
        "## What it has",
        "",
        *[f"- {line}" for line in app["featureList"]],
        "",
        "## Questions",
        "",
        *[line for q, a in faq for line in (f"### {q}", "", a, "")],
        "## Links",
        "",
        f"- [Home page]({HOME}): what it is, the tone list, how setup works",
        f"- [Download]({REPO}/releases/latest): the Windows installer",
        f"- [What's new]({HOME}updates.html): every version so far",
        f"- [Roadmap]({HOME}roadmap.html): what comes next",
        f"- [Why it's free]({HOME}why-free.html): who pays for it, and what is never sold",
        f"- [Press kit]({HOME}press.html): facts, logo and screenshots",
        f"- [Source code]({REPO}): the code, the changelog and how to build it",
        f"- [Help and fixes]({REPO}/blob/main/docs/TROUBLESHOOTING.md)",
        f"- [Using it in recording software]({REPO}/blob/main/docs/DAW.md)",
        "",
    ]
    return "\n".join(lines)


def sitemap(day):
    """Every page carries the date of the newest release: the day the facts on the site last changed."""
    old = (SITE / "sitemap.xml").read_text(encoding="utf-8")
    new = re.sub(r"<url><loc>([^<]+)</loc>(?:<lastmod>[^<]*</lastmod>)?</url>", rf"<url><loc>\1</loc><lastmod>{day}</lastmod></url>", old)
    return old, new


def main(argv):
    check = argv[1:] == ["--check"]
    if argv[1:] and not check:
        sys.exit(__doc__)
    path = SITE / "index.html"
    page = path.read_text(encoding="utf-8")
    block, day, plain = build(page)
    if not BLOCK.search(page):
        sys.exit("schema: index.html has no data block to fill")
    fresh = BLOCK.sub(lambda m: m.group(1) + block + m.group(2), page, count=1)
    old_map, new_map = sitemap(day)
    notes = SITE / "llms.txt"
    old_notes = notes.read_text(encoding="utf-8") if notes.is_file() else ""
    stale = [name for name, a, b in (("site/index.html", page, fresh), ("site/sitemap.xml", old_map, new_map),
                                     ("site/llms.txt", old_notes, plain)) if a != b]
    if check:
        if stale:
            sys.exit("schema: out of date: " + ", ".join(stale) + ". Run python3 tools/site/schema.py")
        print("schema: the data block and the sitemap match the page")
        return
    path.write_text(fresh, encoding="utf-8")
    (SITE / "sitemap.xml").write_text(new_map, encoding="utf-8")
    notes.write_text(plain, encoding="utf-8")
    print("schema: wrote " + (", ".join(stale) if stale else "nothing, already up to date"))


if __name__ == "__main__":
    main(sys.argv)
