# Changelog

What changed in each version of Riffhouse, newest first. Versions follow
[semantic versioning](https://semver.org): the last number goes up for fixes,
the middle one for new things, the first one when something you rely on changes.

Each release on GitHub carries the notes of its version from this file.

## Unreleased

## 0.1.0

The first version.

### What is in it

- 11 amps, 9 speaker cabinets with a movable mic, and 29 pedals. Each one is its own piece of gear, drawn by the app.
- 177 tones sorted by genre, with "Inspired by" tones for well-known players and songs.
- Neural captures: any `.nam` file loads as the amp. Your own impulse response loads as the cabinet.
- Setup by itself: it finds the input your guitar is on, picks the lowest delay your computer can hold, sets the level and the noise gate, and removes mains hum.
- Input Fix for a guitar plugged into a mic jack: it measures the treble the jack takes away and puts it back.
- A tuner for any tuning, a metronome, and play-along with a song that stays out of your recording.
- Practise with the song: play it at 50 to 125 percent of its speed without changing its pitch, move it up or down by up to 12 semitones without changing its speed, and mark one part to go round until you have it.
- A recorder that keeps the clean guitar next to every take.
- Takes: listen to what you recorded, and change the tone of a take after you played it. Riffhouse runs the clean guitar through the amp again and saves the result as a new take.
- Drop a file on the window: a `.nam` capture becomes the amp, an impulse response the cabinet, a song the backing track, a tone file the tone.
- Undo and redo for the whole tone: Ctrl+Z and Ctrl+Y, or the two arrows left of the tone's name. One turn of a knob is one step, and so is loading a tone.
- A looper: press O to record a part, O again to let it go round, and again to add a layer. Up to two minutes. It plays to your ears only, so a take you record over it holds just the guitar.
- A VST3 plugin of the same rig for recording software.
- Updates by itself on Windows. The download is checked against the SHA-256 that GitHub publishes for the file, and it goes in when you close the app.
- Reconnects by itself when a cable or a device comes and goes. If Windows swaps in another microphone, it stays silent.

### Known limits

- Windows only as an installer. macOS and Linux build from source.
- The installer is not signed, so Windows shows a warning the first time.
