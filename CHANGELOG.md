# Changelog

What changed in each version of Riffhouse, newest first. Versions follow
[semantic versioning](https://semver.org): the last number goes up for fixes,
the middle one for new things, the first one when something you rely on changes.

Each release on GitHub carries the notes of its version from this file.

## 0.1.2

0.1.1 broke for the first people who installed it: it could close at the end of setup, stop answering, and open setup again at every start. This version fixes those, and changes how every build is checked so they do not come back.

### It stays open

- Fixed: the app closed by itself at the very end of setup, the moment "You're all set" went away.
- Fixed: the window stopped answering ("Not Responding") when a sound card was slow to start or would not start at all. Riffhouse kept asking it again and again with no pause. It now waits longer between tries, stops after five, and says what is wrong with a button to try again.
- Fixed: setup opened again at every start when the guitar's input was not there at that moment. Setup runs once. After that it opens only when you ask for it in Settings.
- Fixed: setup stuttered while it listened. It redrew the whole window thirty times a second, and now only the part that moves.
- A change in the audio settings opens the sound card once. It used to open it up to three times, which is where much of the waiting came from.
- When the sound card is changed, Windows is no longer asked for the whole list of devices each time.

### No driver to pick

- The audio settings have three things now: where the guitar comes in, where the sound goes out, and the delay. The driver list is gone.
- Riffhouse tries the fastest way your input has. If that will not start, it moves to the next by itself and tells you. If nothing gives sound, the last working choice comes back.
- ASIO is only offered where it works: an audio interface with its maker's driver shows up in the list of inputs, marked "(ASIO driver)". Stand-in drivers that stay silent on most computers are not listed.
- "Let other apps play sound at the same time" is one switch. It costs a few milliseconds of delay.
- If the fast mode stops working one day, after a Windows update or because another app holds the card, Riffhouse carries on in the shared mode and says so. It no longer stays silent.
- A microphone that Windows swaps in for your guitar's input is still never played through the amp.

### Reports that explain themselves

- Riffhouse keeps a short diary of what it did with the sound card: what it opened, how long that took, what failed. It stays on your computer, in "Riffhouse log.txt" beside the settings.
- If the window stops answering for more than a second, the diary says what it was waiting for. If the app closes by itself, the next start writes down where.
- "Report a problem" and "Save a report" in Settings include the last lines of the diary. Nothing is sent by itself.
- After a run that froze or closed by itself, the next start says so once and points to "Report a problem".

### Small things

- With your guitar on an audio interface, the end of setup reminds you to switch off Direct Monitor. That is the reason for hearing the guitar twice.
- The folder with the bare guitar of each take is now called "Clean guitar (for Change the tone)", not "DI". Takes recorded before still work.

### How builds are checked now

- Every build runs the end of a real setup twice, then starts the app the way you do, checks that the window answers, and closes it. A build that fails any of that is not released.
- The part that keeps the guitar connected is tested against a pretend sound card: one that goes silent, one that will not open, one that is unplugged and plugged back in, and one that sends hundreds of change messages. In an hour of a healthy card it must not touch the card once.

### Tones for the songs you asked for

- Eleven new tones, 188 in all. Ten are made after songs the first players asked for: Metallica "Master of Puppets" (the rhythm and the clean interlude), Def Leppard "Pour Some Sugar on Me", Iron Maiden "Fear of the Dark" (the gallop and the quiet intro), Dream Theater "Wither" (the lead and the verse), Guns N' Roses "Sweet Child O' Mine" (the opening riff and the solo) and Avenged Sevenfold "Nightmare". The eleventh is Big Fuzz Wall, the Big Fuzz pedal into a loud stack.
- Every tone made after someone now says whose sound it is, in front of its title: the band first, then the song in quotation marks or the tone's own name. The line under it names the player.
- Search finds tones by band and by song. Type "metallica" and you get all five, the two song tones first. 58 of the older tones learned their band, 26 their song.
- A new list in the tone browser starts at its top.

### Releases

- A new version is offered only once its installer is on the release page. In the minutes between a release being published and its installer being attached, "Get version" used to open a page with nothing to download.
- A release is complete about a minute after it is published, when the same commit was already built and tested on main. It used to take about twenty.

## 0.1.1

The first public release. 0.1.0 only went out to a few testers; this is what changed since, most of it because of what they ran into. What Riffhouse is and everything in it is in the [README](README.md).

### Your audio interface, your choice

- Fixed: picking another input by hand, an audio interface for example, was undone a moment later and the app went back to the microphone, even after a restart. The same happened to ASIO and DirectSound. A choice you make now stays, and is remembered.
- Setup starts with a list of every input. Pick the one your guitar is on, or leave it on "Find it for me" and strum.
- An audio interface is listened through as well as played into. When your guitar is on one, the sound comes out of it too, and the summary says so. You can send it elsewhere in the audio settings.
- An interface's own ASIO driver is tried first, because that is where the lowest delay is.

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

### How it moves

- Buttons give under the finger, switches slide, lamps warm up and die down, and a knob that undo puts back travels there. Panels arrive from where they belong, and pages and tones dissolve into each other.
- When setup is done the rig powers on, each piece lighting in the order the signal runs.
- Nothing moves while you are not touching anything, and a new press always wins over a move still running.
- "Less motion" in Settings turns sliding off and keeps fades short. It starts from what Windows is set to.

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
- Settings has "Save a report": a text file with your audio settings, the names of your inputs and outputs, and what setup measured, written into your recordings folder. Nothing is sent anywhere.
- "Report a problem" in Settings puts that report on your clipboard and opens the page where problems are reported. "Help and fixes" opens [the common problems and how to fix them](docs/TROUBLESHOOTING.md).
- A test build updates itself to the next test build, and then to the release.
- A saved tone no longer carries the name of your Windows account.

## 0.1.0

The first version. It went out as a test build only.

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
