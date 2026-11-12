# Help and fixes

The problems people run into most, and what fixes each one. If yours is not here, see [Still stuck](#still-stuck) at the end.

Most things below are in **Settings** (the gear at the top right). Two buttons there do most of the work: **Run automatic setup again**, and **Input, output and delay...** for picking things by hand.

## Installing

**Windows shows a blue "Windows protected your PC" box.** The installer is not signed with a paid certificate. Press **More info**, then **Run anyway**. The release page lists the SHA-256 of the file if you want to check that it is the one that was published.

**Where does it install?** Into your own user folder, so it never asks for an administrator password. The VST3 plugin goes into your user's VST3 folder. If your recording software does not list it, see [Riffhouse in your recording software](DAW.md).

## It froze, or closed by itself

Version 0.1.1 could close at the very end of setup, and could stop answering when the sound card was slow to start. Both are fixed in 0.1.2. Update first: Settings, **Check for updates**, or run the newest installer from the [releases page](https://github.com/NatikChhabra/riffhouse/releases/latest) over the one you have. Your tones and settings stay.

If it still happens on 0.1.2 or later, Riffhouse writes down what it was doing at that moment. Start it again: from 0.1.5 it asks once whether to send a report, and **Send a report** does it in one click. That report is how the cause gets found. See [Still stuck](#still-stuck).

Picking an ASIO driver froze the window in 0.1.4 and earlier when the driver hung (ASIO4ALL often does). From 0.1.5 a driver is tried in a hidden test run first, so the window keeps answering and a driver that hangs is stopped and left alone.

## Setup opens every time I start

That was a fault in 0.1.1 and is fixed in 0.1.2. Setup runs once. After that it only opens when you press **Run automatic setup again** in Settings. If your guitar's input is missing when the app starts, a line at the bottom says so and waits for the input to come back.

## Riffhouse cannot hear my guitar

Go through these in order.

1. The cable is pushed all the way in, and the guitar's volume knob is up.
2. In setup, pick your input from the list yourself. "Find it for me" listens on every input in turn, and a quiet guitar on a computer's mic jack can be missed.
3. Windows lets desktop apps use the microphone: Windows Settings, **Privacy & security**, **Microphone**, and switch on **Let desktop apps access your microphone**. A guitar input counts as a microphone to Windows.
4. The input is not muted or turned down in Windows: Windows Settings, **System**, **Sound**, then your input under **Input**.
5. On a desktop, try the other socket. A guitar or pickup is louder than a mic, so the **line-in** socket (often blue) can work where the mic socket (pink) does not, and the other way round with a very quiet pickup.
6. On a laptop with one combined headphone and mic socket, a plain guitar cable often does not reach the mic contact. You need a splitter with a separate mic socket, or a USB guitar cable, or an audio interface.

## I have an audio interface

- In setup, pick the interface from the list. If its maker's driver is installed, the interface shows up twice: once with "ASIO driver" next to it, and once under its Windows name. Take the ASIO one. It has the lowest delay.
- Sound comes out of the interface as well, so plug your headphones or speakers into it. The end of setup says where the sound goes.
- To send the sound somewhere else, open **Input, output and delay...** and change **Sound out**.
- If the interface has several inputs, use input 1 or 2. Those are the ones Riffhouse listens to.
- Turn the interface's own direct monitoring off, or you hear the clean guitar on top of the amp. See [I hear my guitar twice](#i-hear-my-guitar-twice).
- Input Fix is for mic jacks. Setup measures your input and leaves it off when no treble is missing, which is the normal case on an interface's instrument input.

## I hear my guitar twice

One of the two is the clean guitar, and it comes from the audio interface, not from Riffhouse. Most interfaces can send the input straight to the headphones. It is called **Direct Monitor**, **Monitor** or **Mix**, and it is a button or a knob on the box, or a switch in the interface's own program.

- Switch Direct Monitor off, or turn the Mix knob all the way to the computer side (often marked PC, DAW or Playback).
- Then you hear only the amp. The end of setup reminds you of this when your guitar is on an interface.

## There is a delay between playing and hearing

Setup measures this and shows it as "about so many ms from string to ear". Under about 12 ms feels immediate. Above about 20 ms most players feel it.

- **Bluetooth headphones and speakers always add a delay you can feel**, often well over 100 ms. No setting in any amp program fixes that, which is why headphones sound in time and a Bluetooth speaker does not. Use wired headphones, or a cable from the computer's headphone socket to the speaker's AUX or Audio In socket. A TV or monitor over HDMI adds delay too. From 0.1.5 setup plays through wired headphones by itself when the computer has them plugged in, and says so.
- Two separate USB devices, one for the guitar and one for the headphones, each add their own delay. Headphones in the same device as the guitar (the interface, or the computer's own sockets) are faster.
- Run setup again with other audio programs closed. It tries the fastest modes one by one and keeps the fastest that runs clean.
- With an audio interface, install its maker's driver and pick the ASIO entry.
- Without an interface, the fastest way is for Riffhouse to have the sound card to itself. Setup picks that when it works.
- You can choose a smaller delay by hand: **Driver, input and output...**, then a smaller number under **Buffer**. If it crackles, go one step back up.
- "Let other apps play sound at the same time" costs a few milliseconds. Leave it off for the shortest delay.
- Turn off the interface's Direct Monitor if you hear a second, clean guitar. That is not a delay, it is two signals. See [I hear my guitar twice](#i-hear-my-guitar-twice).
- Still feels late on 0.1.2 or later? Press **Save a report** in Settings and send it. It holds the numbers needed to see where the time goes.

## Crackles, pops or dropouts

- Open **Driver, input and output...** and pick the next bigger number under **Buffer**.
- Close browsers, games and other audio programs, and plug a laptop into power.
- Run setup again. It only keeps settings that ran without a single dropout while you played.

## What about ASIO?

Open **Driver, input and output...** in Settings to pick a driver yourself. Setup and the self-repair at start pick one for you, and if one will not start, Riffhouse goes back to the last one that worked and says why.

- ASIO drivers come with audio interfaces. An interface with its maker's driver installed is the fastest way in. Pick its ASIO entry.
- Many computers with Realtek sound have **Realtek ASIO**. From 0.1.5 setup tries it when the guitar and the headphones both use the computer's own sockets, because then it beats Windows' own sound.
- Every ASIO driver is first tried in a hidden test run. One that fails or hangs there never freezes the window, and setup leaves it out from then on. Picking it by hand tries it again.
- Stand-in drivers such as ASIO4ALL or FlexASIO need every sound device they use all to themselves. They fail when Windows or another program holds one, and with a guitar on one USB device and headphones on another they often fail outright. Windows Audio (exclusive mode) is usually just as fast on such a computer.
- Only one program can hold most ASIO drivers at a time. Close your recording software, then try again.

## An input I picked is undone

Every choice is tried first. If sound comes through, it stays and is remembered. If not, the last working choice comes back and the screen says why. Another program holding the input is the usual reason: close it and pick again.

## Heavy tones sound harsh, thin or buzzy

This is what a computer's mic jack does to a guitar: it takes most of the treble away, and a high-gain amp then only gets bass.

- Run setup. It measures how much treble your input loses and switches **Input Fix** on with the right amount.
- Keep the guitar's volume knob all the way up.
- In the "play as hard as you ever will" step, really do. The input level is set from that.

## Some tones are much louder than others

Switch **Auto level** on, under the output meter. A tone you load then comes up as loud as the one before it.

## Hum, buzz and hiss

- The **mains hum filter** in Settings is on Auto by default. It finds 50 or 60 Hz hum and removes just that.
- The **noise gate** is set by setup just above your background noise, and follows the room when the noise changes.
- Single-coil pickups hum near screens, chargers and dimmers. Turning a little with the guitar often finds a quiet spot.
- A laptop on its charger can buzz through the mic jack. Try it on battery.

## Other programs go silent while Riffhouse is open

Riffhouse takes the sound card for itself. That is where the low delay comes from.

- To play along with a song, drop the song on the Riffhouse window, or use **Play along**.
- To hear other programs at the same time, open **Input, output and delay...** and switch on **Let other apps play sound at the same time**. The delay goes up a little.

## My recordings

Takes are saved in **Riffhouse Recordings** inside your Music folder, unless you chose another folder. Each take is one WAV file with the sound you heard.

The folder inside it, **Clean guitar (for Change the tone)**, holds the same takes as the bare guitar before the amp. Riffhouse needs those for **Change the tone**, which plays a take through another sound after you recorded it. You can ignore that folder. Delete it only if you never want to change the tone of old takes.

## In recording software

See [Riffhouse in your recording software](DAW.md).

## Updates

Riffhouse checks for a new version when it starts, downloads it in the background, and installs it when you close the app. Settings shows the version you have, and **Check for updates** asks right away. Your tones and settings stay.

## Still stuck

Open Settings and press **Send a report** (0.1.5 and later). It goes straight to the people who fix Riffhouse, privately, and needs no account. Your Windows user name, your computer's name, your user folder and any name inside a device name ("Asha's AirPods") are taken out before it leaves. At most three a day go from one computer.

To report in the open, press **Report in public**. It opens a form on GitHub and puts the same report, with the names taken out, on your clipboard. Paste it (Ctrl+V) and say in your own words what went wrong. That page needs a free GitHub account.

**Save a report** writes the same report into your recordings folder as a text file and shows you the file, to keep or send by hand.

What is in the report: the version, your audio settings, the names of your inputs and outputs, what setup measured, your current tone, and a short diary of what Riffhouse did with the sound card and how long each thing took. If the last run froze or closed by itself, the diary says what it was doing. Nothing is ever sent unless you press a button or say yes.
