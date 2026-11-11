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
- A recorder that keeps the clean guitar next to every take.
- Takes: listen to what you recorded, and change the tone of a take after you played it. Riffhouse runs the clean guitar through the amp again and saves the result as a new take.
- Drop a file on the window: a `.nam` capture becomes the amp, an impulse response the cabinet, a song the backing track, a tone file the tone.
- A VST3 plugin of the same rig for recording software.
- Updates by itself on Windows. The download is checked against the SHA-256 that GitHub publishes for the file, and it goes in when you close the app.
- Reconnects by itself when a cable or a device comes and goes. If Windows swaps in another microphone, it stays silent.

### Known limits

- Windows only as an installer. macOS and Linux build from source.
- The installer is not signed, so Windows shows a warning the first time.
