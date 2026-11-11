# Contributing

Thanks for wanting to help. Riffhouse is free software and stays that way.

## Tones

The easiest way in. A tone is one JSON file in `presets/`. Make it in the app, save it, copy the file from your tone folder (**Open tone folder** in the tone browser) and open a pull request. Name tones after what they sound like. For a tone modelled on a player or a song, use "Inspired by <Artist>: <Title>".

Every tone is checked for loudness, so one tone is never much louder than the next. Run the checks before you send it.

## Code

- Build and run the checks as shown in the README. A pull request needs them green.
- The sound engine lives in `src/dsp` and `src/engine` and has no interface code in it. The checks in `tests/` run it offline.
- Nothing on the audio thread may allocate memory, lock, or wait.
- The gear is drawn in code in `src/ui/Gear.cpp`. Every design must be original. Do not copy the look, name or logo of a real product.

## Releases

- Every change that a player can notice gets a line under "Unreleased" in `CHANGELOG.md`, in plain words.
- A release is a tag. Move the "Unreleased" lines under a new `## <version>` heading, set the same version in `CMakeLists.txt`, and push the tag `v<version>`. `python3 tools/release/notes.py check v<version>` tells you if the three agree.
- The build server does the rest: it runs the checks, builds the installer, installs it on a clean Windows machine, starts the app, has it draw every screen, removes it again, and only then publishes the release with the notes from the changelog and the SHA-256 of the installer.
- `v<version>-rc1` publishes a test release. The app's updater skips those, so nobody gets one by surprise.
- A bad release is fixed with the next one. Mark the bad one as a pre-release on GitHub and the updater stops handing it out the same minute.

## Bugs

Open an issue. Say what you plugged into what (guitar, cable, laptop jack or interface), your Windows version, and what the setup screen reported.

By contributing you agree that your work is released under the GNU Affero General Public License, version 3 or later.
