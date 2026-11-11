# Changelog

What changed in each version of Riffhouse, newest first. Versions follow
[semantic versioning](https://semver.org): the last number goes up for fixes,
the middle one for new things, the first one when something you rely on changes.

Each release on GitHub carries the notes of its version from this file.

## Unreleased

### Sounds right on a mic jack

- Input Fix is rebuilt. A guitar plugged into a computer's mic jack loses most of its treble, over 20 dB at 3 kHz on the PC this was measured on, and heavy tones turned loud and harsh because the amp only got bass. The fix is now the exact opposite of that loss and goes up to 24 dB.
- Setup has a new step: play as hard as you ever will for a few seconds. The input level is set from that and not from the first careful strums.
- If you later play harder than you did in setup, the input level turns itself down and tells you.
- The noise gate follows the room. When the hiss or hum gets louder, the gate moves up with it. A ringing note is never taken for noise.

### Every tone equally loud

- Auto level: a tone you load comes up as loud as the one before it, clean or heavy. Another amp or cabinet keeps the volume. A pedal you kick on may lift it a little, never a lot. A knob you turn still does what a knob does.
- It measures with your own playing once it has heard you, and until then with a guitar phrase of its own. Switch it off under the output meter if you want the raw levels.

### Setup that takes its time

- Four steps instead of three, each with one clear instruction. After each step it stops for a moment and shows what it found.
- The noise is measured only once the strings have really stopped.
- Installs from an earlier version are offered one new run of setup.

### Audio drivers

- Fixed: switching the driver in the audio settings (to ASIO, for example) was undone a moment later, and the app went silent.
- The audio settings are our own now. A driver you pick is tried first. If no sound comes through it, the last working choice comes back and the screen says why.
- With no ASIO driver installed the screen says so, and says what to use.

### In a DAW

- New copies of the plugin start from the input settings the app's setup found.
- A project that had a capture loaded but another amp selected came back with the capture selected. Fixed.
- The keyboard stays with the DAW while the plugin window is open.
- A host that sends bigger blocks than it announced no longer gets silence.
- The delay reported to the host stays right when a capture is loaded or the amp changes.
- Several copies in one project share their level measurements and do not all measure at once.
- A short guide: [Riffhouse in your recording software](docs/DAW.md).

### Also

- The 29 pedals are modelled in 3D now, with real metal, paint and shadows. Knobs and lights still move live.
- The exact version is shown in Settings and in the window title of test builds.
- Settings has "Save a report for help": a text file with your audio settings and what setup measured, written into your recordings folder. Nothing is sent anywhere.
- A saved tone no longer carries the name of your Windows account.

## 0.1.0

The first version.

### What is in it

- 11 amps, 9 speaker cabinets with a movable mic, and 29 pedals. Each one is its own piece of gear.
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
- Foot control: a MIDI foot controller, expression pedal or keyboard turns any knob, switches pedals on and off, steps through your starred tones, presses the looper, taps the tempo and starts a take. You teach it by touching the control in Riffhouse and then moving the pedal. An expression pedal rocks the wah without being taught.
- A VST3 plugin of the same rig for recording software.
- Updates by itself on Windows. The download is checked against the SHA-256 that GitHub publishes for the file, and it goes in when you close the app.
- Reconnects by itself when a cable or a device comes and goes. If Windows swaps in another microphone, it stays silent.

### Known limits

- Windows only as an installer. macOS and Linux build from source.
- The installer is not signed, so Windows shows a warning the first time.
