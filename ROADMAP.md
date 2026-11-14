# Riffhouse roadmap

Everything coming to Riffhouse, in the order it lands. Every release stays free for everyone.

The nicer version of this page is on the website: https://natikchhabra.github.io/riffhouse/roadmap.html

Updated 14 November 2026.

## Out now: 0.1.5 Hotfix 2

Released 13 November 2026 (Hotfix 1 the same day: the gate no longer cuts held notes, and the Tight Metal amp gets its low end back; Hotfix 2 on 14 November: no crackle or lost sound with a separate guitar input and headphones, the gate stops muting notes that still ring, a crash is fixed, and you can type a knob's value). The polish release: sound you can trust, tested end to end before it ships. Riffhouse updates itself, or download it from the [releases page](https://github.com/NatikChhabra/riffhouse/releases/latest).

- **Pedals that behave like the real circuits.** Every drive and fuzz is now solved as the circuit it comes from, checked against published measurements of the real pedals: how much gain each has, where it peaks and how far the tone knob reaches.
- **Cleaner high gain.** The amps' distortion is worked out in a way that keeps the harsh digital fizz off high notes, without costing your computer more.
- **Quiet when you stop.** The gate closes the moment your hand mutes the strings, and hum is taken out without the ringing the old filters left after every stop.
- **Whines gone.** A steady whine from a charger, a laptop or a cheap pickup is found while you rest and cut out with notches a few Hz wide. A clean input is never touched.
- **Setup that doesn't over-brighten.** The treble fix for laptop mic jacks is gentler, and hiss on that band is turned down while you're not playing.
- **Indian rock and indie tones.** A pack of new tones for the sounds of Indian rock and indie.
- **A weekly player count.** An anonymous count of how many people play each week, so we know what to fix first: a random number, the version and the week, nothing about you. One switch in Settings turns it off.
- **Fixes from your reports.** Something broke? In Settings, press **Save a report**. Every report gets read.

## 0.1.4

Released 12 November 2026, fixes from the first day of 0.1.3.

- **Puts itself back on the fast way in.** If Riffhouse starts on a slow driver, it tries the fastest one your input has a few seconds later and keeps it only if the sound is clean.
- **Clearer sound card messages.** When a driver won't start, Riffhouse says what it went back to and names the usual cause first: another program, often a voice call, holding the device.
- **Setup finds your guitar first.** Inputs named for a guitar or an instrument come first, then audio interfaces, with USB mics and webcams last.
- **Settings is one page.** No side list, every part under its own heading, and the delay figure reads in full.

## 0.1.3

Released 12 November 2026.

- **A sound card that sorts itself out.** Pick Automatic, ASIO, WASAPI or DirectSound, or let Automatic choose the fastest one that plays clean. Every ASIO driver is listed. When an interface is stuck at the wrong sample rate, Riffhouse fixes it, and anything it can't fix is explained in plain words.
- **A new look.** The new logo, a window that fits any screen shape, Settings on one simple page, and pedal names printed on the pedals.
- **A classic wah.** The throaty sixties sweep, as a new pedal, plus a tone built around it. 189 tones in all.
- **Chords and tabs.** Paste a song from any site. Change its key, make the text bigger and let it scroll while you play.
- **Share a tone as a message.** Send a tone to a friend as plain text. They paste it into Riffhouse and it loads.
- **MP3 and FLAC.** Every take you record saves as MP3 or FLAC with one button. And the app starts about twice as fast.

## The big one: 0.2.0

One big update with the things players ask for most. Work starts once 0.1.3 has real players. It comes as soon as it realistically can, with no fixed date, and friends test it on their own PCs first. It ships when nothing crashes and every tone you have sounds exactly the same.

- **A real pedalboard.** Drag any pedal onto the board, in any order, before or after the amp. Stack two of the same pedal, up to twelve in all. Save whole boards and your favourite pedal settings, see a tone's whole chain in one line, and undo any change.
- **Scenes and a Live view.** Four scenes in every tone, like rhythm, lead, clean and solo. Switch with keys 1 to 4 or a MIDI pedal, with no gap, while delay and reverb ring on. Live view puts them full screen for the stage.
- **Mic button.** Your guitar and your voice go into any call or streaming app as one microphone, at a steady level. Riffhouse walks you through the free virtual cable it uses.
- **Share clip.** One button saves the last 30 seconds you played as a small audio file, or a short video card of your rig, ready to send on WhatsApp. Nothing to arm first.
- **Drums to play along with.** 20 to 30 beats across rock, metal, funk, pop, blues and Indian rhythms like dadra and keherwa, in time with the metronome.
- **Two amps at once.** Split your guitar into two amps, each with its own cabinet, and blend them.
- **A daily practice goal.** Five minutes a day if you want it, with a week view of the days you played. Missing a day never wipes anything.
- **New tones every Friday.** Five new free tones each week, waiting on a New this week shelf.
- **Tap tempo and starting sounds.** Tap the beat for delays and drums. New players pick one of six starting sounds, each a full rig that is ready to play.
- **Hindi and Hinglish.** The whole app in Hindi or Hinglish, as well as English.

Everything you have now comes with you. Saved tones and settings carry over and sound exactly the same, and nothing you know moves.

## Toward 1.0

In this order, once 0.2.0 is out. The aim for 1.0: the free guitar app most players reach for, on the computer and on the phone.

1. **0.3: a library of free community tones, inside the app.** Browse, hear it with your own guitar, load it in one click and share yours.
2. **0.4: tone match.** Help getting close to a sound you hear on a record, built from Riffhouse's own amps and pedals.
3. **0.5: Mac.** It needs someone with a Mac to test it. If that's you, [say so in an issue](https://github.com/NatikChhabra/riffhouse/issues/new).
4. **0.6: setlists and A/B compare.** Tones in order for a gig, and two tones side by side.
5. **0.7: a free phone app for practice, Android first.** Amp, pedals, tuner, looper, recording and songs, with a cheap cable.
6. **0.8: an open base to build on.** An open tone format and code that anyone can build their own pedals and tones on.
7. **0.9: more languages and classrooms.** More Indian languages after Hindi, and a way for a teacher to share tones and songs with a class.
8. **1.0.** Computer and phone at the same level, and a month with no new crash in any report before it ships.

## Three promises

1. **Free in every release.** No paid version, no locked tones, no ads in the app and no account. Everyone gets the same Riffhouse.
2. **Releases only grow.** Once something is on this page, it doesn't get pushed to a later version. If time runs short, the date moves instead.
3. **Tested before it ships.** Every release passes its checks first, and the notes say plainly what hasn't been tried on real hardware yet.

Want something that isn't here? [Ask for it in an issue](https://github.com/NatikChhabra/riffhouse/issues/new). What already shipped is in the [changelog](CHANGELOG.md).
