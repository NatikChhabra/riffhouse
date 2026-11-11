# Help and fixes

The problems people run into most, and what fixes each one. If yours is not here, see [Still stuck](#still-stuck) at the end.

Most things below are in **Settings** (the gear at the top right). Two buttons there do most of the work: **Run automatic setup again**, and **Driver, input and buffer...** for picking things by hand.

## Installing

**Windows shows a blue "Windows protected your PC" box.** The installer is not signed with a paid certificate. Press **More info**, then **Run anyway**. The release page lists the SHA-256 of the file if you want to check that it is the one that was published.

**Where does it install?** Into your own user folder, so it never asks for an administrator password. The VST3 plugin goes into your user's VST3 folder. If your recording software does not list it, see [Riffhouse in your recording software](DAW.md).

## Riffhouse cannot hear my guitar

Go through these in order.

1. The cable is pushed all the way in, and the guitar's volume knob is up.
2. In setup, pick your input from the list yourself. "Find it for me" listens on every input in turn, and a quiet guitar on a computer's mic jack can be missed.
3. Windows lets desktop apps use the microphone: Windows Settings, **Privacy & security**, **Microphone**, and switch on **Let desktop apps access your microphone**. A guitar input counts as a microphone to Windows.
4. The input is not muted or turned down in Windows: Windows Settings, **System**, **Sound**, then your input under **Input**.
5. On a laptop with one combined headphone and mic socket, a plain guitar cable often does not reach the mic contact. You need a splitter with a separate mic socket, or a USB guitar cable, or an audio interface.

## I have an audio interface

- In setup, pick the interface from the list. If its maker's driver is installed, the interface shows up twice: once with "ASIO driver" next to it, and once under its Windows name. Take the ASIO one. It has the lowest delay.
- Sound comes out of the interface as well, so plug your headphones or speakers into it. The end of setup says where the sound goes.
- To send the sound somewhere else, open **Driver, input and buffer...** and change **Output**.
- If the interface has several inputs, use input 1 or 2. Those are the ones Riffhouse listens to.
- Turn the interface's own direct monitoring off, or you hear the clean guitar on top of the amp.
- Input Fix is for mic jacks. Setup measures your input and leaves it off when no treble is missing, which is the normal case on an interface's instrument input.

## There is a delay between playing and hearing

Setup measures this and shows it as "about so many ms from string to ear". Under about 12 ms feels immediate. Above about 20 ms most players feel it.

- **Bluetooth headphones always add a delay you can feel**, well over 100 ms. No setting fixes that. Use wired headphones.
- Run setup again with other audio programs closed. It tries the fastest modes one by one and keeps the fastest that runs clean.
- With an audio interface, install its maker's driver and pick the ASIO entry.
- Without an interface, "Windows Audio, exclusive mode" is the fastest there is. Setup picks it when it works.
- You can choose a smaller buffer by hand in **Driver, input and buffer...**. If it crackles, go one size back up.
- DirectSound is slow by design. Use it only if nothing else starts.

## Crackles, pops or dropouts

- Open **Driver, input and buffer...** and pick the next bigger buffer.
- Close browsers, games and other audio programs, and plug a laptop into power.
- Run setup again. It only keeps settings that ran without a single dropout while you played.

## ASIO does not start

- ASIO drivers come with audio interfaces. With no interface there is usually no real ASIO driver on the computer, and the audio settings say so.
- Stand-in drivers such as ASIO4ALL or FlexASIO are not an interface's own driver. They rarely beat exclusive mode and often stay silent. Riffhouse tries them if you pick them, and goes back to what worked when no sound comes through. The screen says which happened.
- Only one program can hold most ASIO drivers at a time. Close your recording software, then try again.

## A driver or input I picked is undone

That was a fault in the first test build (0.1.0) and is fixed. Every choice is tried first. If sound comes through, it stays and is remembered. If not, the last working choice comes back and the screen says why. Update to the newest version: Settings, **Check for updates**.

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

Exclusive mode gives Riffhouse the sound card to itself. That is where the low delay comes from.

- To play along with a song, drop the song on the Riffhouse window, or use **Play along**.
- To hear other programs at the same time, open **Driver, input and buffer...** and pick "Windows Audio, shared mode". The delay goes up a little.

## In recording software

See [Riffhouse in your recording software](DAW.md).

## Updates

Riffhouse checks for a new version when it starts, downloads it in the background, and installs it when you close the app. Settings shows the version you have, and **Check for updates** asks right away. Your tones and settings stay.

## Still stuck

Open Settings and press **Report a problem**. It copies a report to your clipboard (your audio settings, the names of your inputs and outputs, and what setup measured; nothing about you) and opens the page where problems are reported. Paste the report there and say what went wrong. You need a free GitHub account for that page.

**Save a report** writes the same report into your recordings folder as a text file, if you would rather send it to whoever is helping you.
