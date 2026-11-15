# What Riffhouse sends

Riffhouse is free and has no accounts, no ads and no tracking. Three things can leave your PC, and this page lists all of them, plus what the website collects.

## The weekly player count (on unless you switch it off)

Once a week, about 20 seconds after Riffhouse opens, it sends:

- the week, like `2026-W46`
- a random number made on your PC the first time, like `3f9a0c...` (32 letters and digits). It comes from random numbers only, never from your name, your PC, your sound card or anything else about you
- the app version, like `0.1.5`

That is all. It goes to a small script on Google Apps Script that adds one row to a private sheet only the maker of Riffhouse can open, with the time it arrived. Google sees the request arrive, as with any website; the script never sees or stores your internet address. Copies built from the source code and copies on test machines send nothing.

It tells us how many people play each week and how many come back, so we know whether Riffhouse is getting better and what to fix first.

**To switch it off:** Settings, Player count, untick "Count me in the weekly number of players". Nothing is sent after that.

## Checking for updates

Riffhouse asks GitHub for the newest version, the same as opening the download page. Settings, "Install updates by itself when I close Riffhouse" controls installing. GitHub counts how many times each installer is downloaded, with nothing about who.

## Reports (only when you press a button)

"Send a report" sends the app's log and settings privately so a problem can be fixed. Your Windows user name, PC name, names inside device names and email addresses are taken out first. Nothing is sent unless you press the button, at most three a day.

## The website

The website counts visits and clicks on its buttons with a small script of our own on Google Apps Script. It sets no cookies and uses no tracking IDs. For a visit it sends only the page and the site you came from (like reddit.com); for a click, which button (like the Download button). Visits and clicks are added up per hour, and only the totals are kept. Your internet address is not stored.

### Update emails (only if you sign up)

If you leave your email address on the [updates page](https://riffhouse.in/updates.html), it goes into a private list only the maker of Riffhouse can open, with the date and a random code for leaving the list. You get one email when an update ships, and nothing else. The address is never shared or sold, and there is no account. Every email has a link that takes you off the list straight away.

### The "Why did you uninstall?" page

If you answer it, it sends the reason you picked, the short note you typed (if any, up to 300 letters) and the app version. Nothing else, and nothing that says who you are. Please don't put personal details in the note.

## Questions

Open an issue: https://github.com/NatikChhabra/riffhouse/issues
