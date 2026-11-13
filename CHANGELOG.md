# Changelog

What changed in each version of Riffhouse, newest first. Versions follow
[semantic versioning](https://semver.org): the last number goes up for fixes,
the middle one for new things, the first one when something you rely on changes.

Each release on GitHub carries the notes of its version from this file.

## 0.1.5

Pedals that behave like the real circuits, cleaner high gain, quiet mutes, no more frozen window when you pick ASIO, a setup that goes for the lowest delay by itself, and a way to send us a report in one click.

- Every drive and fuzz pedal is now worked out as the circuit it comes from, transistor by transistor and diode by diode, and checked against published measurements of the real pedals: how much gain it has, where it peaks and how far the tone knob reaches. They clean up when you pick softly, the way real pedals do. The big fuzz is smoother on top, as the real one is.
- High-gain amps no longer add a harsh digital fizz to high notes: the amps' distortion is worked out in a way that keeps it away (61 dB below the notes, was 43), with no extra load on your computer.
- The gate closes the moment your hand mutes the strings, and after the amp too, so a stop is silent instead of crunchy. It follows the room: steady hiss or hum raises it by itself, judged by the noise's loudest moments, so a buzzing hum can't hold it open.
- Hum is learnt and taken out instead of filtered, so G, B and D notes keep their body and nothing rings after you stop. It now takes out the buzz up to 2 kHz that a ground loop or a light dimmer adds, not only the low hum, and learns it in about half a second of quiet.
- Input Fix is gentler: setup never asks for more than 12 dB of it, and the hiss it would bring up is turned down while you are not playing. Setup also hears the whole of what you play when it measures your guitar, not only the loudest parts.
- Eight new tones for the sounds of Indian rock and indie: Bangalore Thrash, Shillong Blues Rock, Bangla Band Jangle, Carnatic Prog Lead, Desi Djent, Post-Rock Swells, Bedroom Indie Clean and Nineties Indipop Crunch. 197 tones in all, every one level-matched again.
- A classic rock solo tone that was meant to use the classic wah now does.
- The speaker cabinets are built from the real speakers' published measurements: each one follows its speaker's response within 2 dB from 250 Hz to 5 kHz (they were up to 17 dB off). The box shapes the lows, and the mic does what a close mic does: brighter on the dust cap, much darker at the cone edge, more bass as it moves in.
- No clicks: switching any pedal, changing tones and swinging any knob fast are click-free, checked on every pedal, tone and knob. Knobs glide instead of stepping, and a tone change dips for a moment so it never pops.
- Delay and reverb tails ring on over a tone change instead of cutting off.
- Three amps have new names of our own: Brit Plexi is now Brit 68, Brit 800 is Brit 81 and Hot Rod Lead is Redline Lead. Tones saved or shared before load as they did.
- The tuner is accurate to half a cent from a five-string bass's low B to the top of the guitar neck, and settles in a quarter of a second. The metronome keeps time to the sample, at every sample rate and tempo.

- Windows exclusive mode can run much smaller buffers. A guitar input that Windows sets to 192 kHz used to hold every computer at 12 ms buffers; now it goes down to about 3 ms.
- When Windows runs the guitar input and the headphones at different rates, setup says how to set both to 48 kHz so the fastest mode works.
- Picking an ASIO driver no longer freezes the window. A driver is first tried in a second, hidden copy of Riffhouse; one that hangs or fails there (ASIO4ALL often does when the sound devices under it are busy) is stopped from outside and never opened in the app. Automatic setup skips a driver that failed on this computer before. Picking it by hand checks it again.
- When the computer's own Realtek chip plays the sound, setup also tries Realtek's ASIO driver, which goes lower than Windows Audio on the same jacks.
- Setup no longer opens the sound card a second time at the end when the buffer it found is already running, so each step lags less.
- Bluetooth speakers and headphones, TVs and monitors over HDMI are named as slow outputs. Setup plays through wired headphones instead when the computer has them, and says so. Bluetooth always adds a delay you can hear; a cable to the speaker's AUX in fixes it.
- The delay on the Settings plate says how it feels: instant, fine or noticeable. When the buffer is already the smallest the driver allows, it says that too.
- Send a report (Settings, Help) sends what Riffhouse wrote down to the people who fix it, privately, with your user name and computer name taken out. After a crash, a freeze or a sound card change that did not work, Riffhouse asks once whether to send one. Nothing leaves the computer unless you say yes, and at most three a day.
- Report in public opens a form on GitHub and puts the report on your clipboard, with your names taken out too.
- Riffhouse counts how many people play each week. Once a week it sends a random number made on your PC, the app version and the week, nothing about you or your PC. It is on by default, says so once, and Settings, Player count switches it off. Details in docs/PRIVACY.md.
- The taskbar shows the new icon after an update, not the old house.
- Takes plays any audio file you drop in its folder: MP3, FLAC, OGG, M4A and files with odd names like ".mp3.mpeg".
- The tone list shows each tone's amp beside its name.
- Rest the mouse on any knob and it says, in plain words, what it does and which way. Double-click still resets it.
- Settings has a Support Riffhouse button under About. Everyone gets the same app; supporting is a thank-you.
- The first time you run Riffhouse, setup ends with six sounds to start from (clean, blues, classic rock, hard rock, metal and indie), each a whole rig. Play while you click through them, or use the arrow keys, and start with the one you like.
- Setup's first page asks for wired headphones and says where the guitar can plug in. After an update, Riffhouse says in one line what changed.
- A driver that does not count dropouts says "not counted by this driver" instead of "-1".
- No crackle or thump when Riffhouse opens. High-gain amps made a short noise of their own while their circuits settled; the rig now settles in silence before it plays, and the sound fades in over a moment.

## 0.1.4

Fixes from the first day of 0.1.3, and a Riffhouse that puts itself back on the fast way in.

- The delay on the Settings plate reads "48 ms". It showed "48.1..." with stray dots, because the "ms" did not fit.
- When a driver you pick will not start, the Driver, Guitar in and Sound out boxes show what Riffhouse went back to, instead of the driver that failed. The message names that driver too: "went back to Guitar (Realtek USB Audio) on Windows Audio, exclusive mode, which works".
- A driver that will not start names the usual cause first: another program using the device, often a voice call (Discord, Zoom, Meet). ASIO4ALL and FlexASIO also say they need the device all to themselves.
- The Delay box is now called Buffer and lists "576 samples (12.0 ms)". The big figure on the Settings plate is the whole trip from string to ear, and its Runs line names the buffer, so the two numbers no longer look like they disagree.
- Settings drops the side list. It is one page under the title, every part under its own heading.
- After a run that crashed or did not close, Save a report keeps what happened in that run, not only the calm start after it.
- A start on a slow way in (DirectSound, or a buffer over 20 ms) tries the fastest way the same input has, a few seconds after the window opens: an audio interface's own ASIO driver first, then Windows' fastest modes. It keeps it only if sound comes through clean, and says what it changed. Nothing changes if no faster way works.
- Setup listens first to an input named for a guitar or an instrument, then audio interfaces, and leaves USB microphones and webcams for last. Every input is still in the list to pick by hand.

## 0.1.3

A new logo, a sound card that sorts itself out, a cleaner window that fits any screen, a classic wah, and two new ways to share: tones as a plain message, and songs as chords and tabs you play from.

### Your sound card, your way

- Settings > Driver, input and output has a Driver box again: Automatic, ASIO, WASAPI exclusive, WASAPI shared and DirectSound. Each lists its own inputs and outputs.
- Every ASIO driver on the computer is listed when you pick ASIO, ASIO4ALL and FlexASIO included.
- Automatic uses your interface's ASIO driver when there is one, and otherwise Windows exclusive mode, the fastest. It no longer settles for shared mode when exclusive works.
- ASIO sorts itself out. An interface stuck at another sample rate (by its own settings, or because Windows is using it) is tried at its own rate and every usual one, and Riffhouse says which rate it ended on.
- When an ASIO driver will not start, it says why in plain words: most often another program is using it, or its clock is set to an outside source.
- When only the interface's own settings can change the delay, Riffhouse says how many milliseconds that costs and opens those settings with one button. It picks up the change by itself.
- When it has to use shared mode, it says why in plain words. The usual reason is a guitar on one USB device and headphones on another, and it says what to plug in instead.
- Picking an input by hand now ends on the lowest delay that plays without crackles, the same way setup does.
- Nothing locks you out: a driver that will not start puts back the last setup that worked and tells you what happened.
- With an ASIO driver, Sound out lists your interface's outputs by name (headphones, speakers), and any other output on the computer below them.
- Interfaces with more than two inputs get a Socket box, so a guitar in socket 3 or 4 is heard.
- A loop in the looper stays when the delay or the sound card changes at the same sample rate.

### Cleaner, and fits any window

- The new Riffhouse logo: the amp-face badge and the script name, in the top bar, on the app icon and in the installer.
- The rig grows to fill the window, wide or tall, instead of sitting in black bars.
- Settings is one simple page again, with a short list on the side that jumps to each part, and a plate that shows the delay.
- The bar at the bottom only shows what is doing something (hum removed, Input Fix, the gate), so a quiet bar means nothing needs a look.
- The take strip at the bottom no longer covers Settings or any other page.
- Pedal knob labels are printed on the pedal like real silk-screen.

### A classic wah

- The wah has a third type, Classic: the throaty 60s sweep, darker and more vocal than the others, rocked with your foot or an expression pedal. It has its own green pedal, THROAT.
- New tone: Rocked Wah Soul. The solo tone inspired by Sweet Child O' Mine now uses the Classic wah.

### Share and play

- Copy tone code now copies a short message: the tone's name, the free download link and the code. A friend copies the whole message and presses Ctrl+V in Riffhouse. The code is found even when a chat app wraps it over lines.
- Chords and tabs: copy a song's chords or tab from any website and press Ctrl+V. Chords sit over their words, tab is drawn on six strings, and each chord's shape is shown along the top. Change the key, make it bigger to read from your chair, let it scroll by itself while you play, and copy it out in the key you see. Songs are kept as text files in Documents/Riffhouse/Songs.
- Every take has an MP3 / FLAC button. Convert a sound file does the same for any WAV, AIFF, FLAC, OGG or MP3. Copies never replace the original.

### Also

- Settings > Updates: "Get new versions early, as a tester" offers test versions a few days before everyone gets them.
- Riffhouse starts in about half the time.
- Tones named after an artist say plainly that they are a style, in the tone list and in a shared tone's message. About says how an artist can ask for a change.
- The installer carries every part's licence in Licences.txt.

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
- The diary notes how long the app took to start, how long the window took to draw the first time, and what it was opening if a start takes more than fifteen seconds.
- "Report a problem" and "Save a report" in Settings include the last lines of the diary. Nothing is sent by itself.
- After a run that froze or closed by itself, the next start says so once and points to "Report a problem".

### Small things

- With your guitar on an audio interface, the end of setup reminds you to switch off Direct Monitor. That is the reason for hearing the guitar twice.
- The speaker cabinet starts silent every time. Its two filters used to carry the last samples of whatever played before into the next start. It was far too quiet to hear, but it made two measurements of the same tone differ.
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
