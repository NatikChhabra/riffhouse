![Riffhouse, a free guitar amp and pedalboard](docs/banner.png)

# Riffhouse

A free guitar amp and pedalboard for your computer.

Plug your guitar into whatever you have, even the mic jack on a laptop. Press one button, strum for twenty seconds, and play. No audio interface needed, no account, no ads.

![The amp page](docs/screenshots/amp.png)

## Get it

**Windows:** download the file ending in `windows-setup.exe` from the [latest release](https://github.com/NatikChhabra/riffhouse/releases/latest) and run it.

Windows may show a blue "Windows protected your PC" box the first time. That happens because the installer is not signed with a paid certificate. Press **More info**, then **Run anyway**.

After that Riffhouse keeps itself up to date. A new version downloads in the background, its SHA-256 is checked against the one GitHub publishes for the file, and it goes in when you close the app. It never interrupts you while you play or record, and it leaves your tones and settings alone. You can switch this off in Settings.

**macOS and Linux:** build from source for now (see below).

## What you get

- **11 amps**, from glassy cleans to tight metal, plus a bass amp and a direct channel.
- **9 speaker cabinets** with a mic you can move across the cone, and a slot for your own impulse response.
- **29 pedals:** gate, compressor, wah and envelope filter, octave, seven drives and fuzzes, graphic EQ, six modulations, four delays, six reverbs and a lo-fi box.
- **177 tones** sorted by genre, with "Inspired by" tones for well-known players and songs.
- **Neural captures:** load any `.nam` file as the amp.
- **Tuner** for any tuning, **metronome**, and a **recorder** that also keeps the clean guitar so you can change the tone later.
- **Play along:** load a song, it plays in your headphones and stays out of your recording.
- A **VST3 plugin** of the same rig for recording software.

Every amp, pedal and cabinet is its own piece of gear, drawn by the app itself.

| | |
|---|---|
| ![A drive pedal](docs/screenshots/pedal.png) | ![The wah, which you rock with the mouse](docs/screenshots/wah.png) |
| ![A cabinet with its microphone](docs/screenshots/cab.png) | ![The tone browser](docs/screenshots/browser.png) |

## It sets itself up

- It listens on every input while you strum and picks the one your guitar is on.
- It tries the fastest audio modes your computer has and keeps the lowest delay that runs without crackles.
- It sets the input level and the noise gate from what it hears.
- It finds 50 or 60 Hz mains hum and removes only that.
- **Input Fix** measures how much treble a mic jack takes away from a guitar and puts it back.
- Plug in late, pull the cable out, close the laptop lid: it reconnects by itself. If Windows swaps in another microphone, Riffhouse stays silent, because a laptop mic through an amp howls.

## What we measure

Every change runs 5,400 automatic checks on the sound engine before it ships.

| | |
|---|---|
| Delay added by the rig | 5 samples, about 0.1 ms at 48 kHz |
| Amp oversampling | 8x below 64 kHz, 4x above |
| Aliasing on a hot amp | 42.9 dB below the wanted harmonics |
| Everything switched on | about 9x faster than real time on one core |
| Loudness across all 177 tones | within 5 dB of each other |

## Build it yourself

You need CMake 3.22 or newer and a C++20 compiler. JUCE and the Neural Amp Modeler core are fetched for you.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Riffhouse_Standalone Riffhouse_VST3
```

On Linux install the usual JUCE packages first (ALSA, JACK, FreeType, Fontconfig, X11, Mesa). The list is in `.github/workflows/build.yml`.

On Windows, pass `-DRIFFHOUSE_ASIO_SDK_DIR=<path>` to build with ASIO. Without it you still get WASAPI exclusive mode.

Run the checks:

```sh
cmake --build build --config Release --target riffhouse_tests
build/tests/riffhouse_tests_artefacts/Release/riffhouse_tests
```

## Share tones

A tone is a small JSON file. In the tone browser, **Copy tone code** puts the current tone on your clipboard as text, and **Paste tone code** loads one. **Open tone folder** shows where your own tones live. To add a tone to the factory set, put it in `presets/` and open a pull request.

## Licence and names

Riffhouse is free software under the GNU Affero General Public License, version 3 or later.

It is built with [JUCE](https://juce.com) (AGPLv3) and [Neural Amp Modeler Core](https://github.com/sdatkinson/NeuralAmpModelerCore) (MIT). Fonts: Instrument Sans, Inter, Doto and Big Shoulders, all under the SIL Open Font License.

Every amp, pedal and cabinet in Riffhouse is an original design. Tones named "Inspired by" describe a style. Riffhouse has no affiliation with, and is not endorsed by, any artist, band or equipment maker. All trademarks belong to their owners.
