#!/usr/bin/env python3
"""Release helper. Three jobs, all from files in the repo:

  notes.py check <tag>        the tag, CMakeLists.txt and CHANGELOG.md agree on the version
  notes.py notes <tag>        print the release notes for that version (from CHANGELOG.md)
  notes.py prerelease <tag>   print true for v1.2.3-rc1 and the like, else false

A tag is v<major>.<minor>.<patch>, with an optional -rc<n> or -beta<n> for a test release.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
TAG = re.compile(r"^v(\d+\.\d+\.\d+)(-(rc|beta)\d+)?$")


def fail(msg):
    sys.exit(f"release: {msg}")


def parse(tag):
    m = TAG.match(tag)
    if not m:
        fail(f"'{tag}' is not a release tag. Use v1.2.3, v1.2.3-rc1 or v1.2.3-beta1.")
    return m.group(1), bool(m.group(2))


def cmake_version():
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    m = re.search(r"project\(Riffhouse VERSION ([0-9.]+)", text)
    if not m:
        fail("no version found in CMakeLists.txt")
    return m.group(1)


def section(version):
    """The lines under '## <version>' up to the next '## ' heading."""
    lines = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8").splitlines()
    out, inside = [], False
    for line in lines:
        if line.startswith("## "):
            if inside:
                break
            inside = line[3:].split(" ")[0].strip() == version
            continue
        if inside:
            out.append(line)
    return "\n".join(out).strip()


def main(argv):
    if len(argv) != 3 or argv[1] not in ("check", "notes", "prerelease"):
        fail("usage: notes.py check|notes|prerelease <tag>")
    version, pre = parse(argv[2])
    if argv[1] == "prerelease":
        print("true" if pre else "false")
        return
    body = section(version)
    if argv[1] == "check":
        if cmake_version() != version:
            fail(f"the tag says {version}, CMakeLists.txt says {cmake_version()}. Change one of them.")
        if not body:
            fail(f"CHANGELOG.md has no notes under '## {version}'. Write them first.")
        print(f"version {version}: tag, CMakeLists.txt and CHANGELOG.md agree")
        return
    if not body:
        fail(f"CHANGELOG.md has no notes under '## {version}'.")
    # A test release also carries what has changed since the one before it.
    fresh = section("Unreleased") if pre else ""
    if fresh:
        print("## New in this test release\n\n" + fresh + f"\n\n## {version}\n")
    print(body)


if __name__ == "__main__":
    main(sys.argv)
