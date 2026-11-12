# What Riffhouse sends

Riffhouse is free and has no accounts, no ads and no tracking. Three things can leave your PC, and this page lists all of them.

## The weekly player count (on unless you switch it off)

Once a week, about 20 seconds after Riffhouse opens, it sends:

- the week, like `2026-W46`
- a random number made on your PC the first time, like `3f9a0c...` (32 letters and digits). It comes from random numbers only, never from your name, your PC, your sound card or anything else about you
- the app version, like `0.1.5`

That is all. It goes to a small script on Google Apps Script that adds one row to a private sheet only the maker of Riffhouse can open. Google sees the request arrive, as with any website; the script never stores your internet address.

It tells us how many people play each week and how many come back, so we know whether Riffhouse is getting better and what to fix first.

**To switch it off:** Settings, Player count, untick "Count me in the weekly number of players". Nothing is sent after that.

## Checking for updates

Riffhouse asks GitHub for the newest version, the same as opening the download page. Settings, "Install updates by itself when I close Riffhouse" controls installing. GitHub counts how many times each installer is downloaded, with nothing about who.

## Reports (only when you press a button)

"Send a report" sends the app's log and settings privately so a problem can be fixed. Your Windows user name, PC name, names inside device names and email addresses are taken out first. Nothing is sent unless you press the button, at most three a day.

## Questions

Open an issue: https://github.com/NatikChhabra/riffhouse/issues
