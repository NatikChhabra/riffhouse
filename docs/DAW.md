# Riffhouse in your recording software

The installer puts a VST3 plugin of the whole rig on your computer next to the app. This page gets it running in a DAW in a few minutes.

## Before you open the DAW

Open the Riffhouse app once and let setup run. It measures your input: how loud you play, how much treble a mic jack takes away, how much noise there is. Every new copy of the plugin starts from those settings, so it sounds in the DAW the way it sounds in the app.

## Find the plugin

1. Open your DAW and let it scan for plugins. Most do this when they start.
2. Look for **Riffhouse** under effects. The maker is also listed as Riffhouse.

If it is not in the list, the DAW is not looking in the folder the installer used:

```
%LOCALAPPDATA%\Programs\Common\VST3
```

Paste that line into the address bar of a File Explorer window to see the folder. Then add it to the VST3 folders in your DAW's plugin settings and scan again.

- **Ableton Live**: Settings, Plug-Ins, switch on "Use VST3 Plug-In Custom Folder" and pick the folder.
- **FL Studio**: Options, Manage plugins, add the folder under plugin search paths, then "Find installed plugins".
- **REAPER**: Options, Preferences, Plug-ins, VST. Add the folder to the path list and press Re-scan.
- **Any other DAW**: look for the list of VST3 folders in its plugin settings, add the folder, scan again.

## Play through it

1. Make an audio track and pick the input your guitar is on. A mono track is fine.
2. Put Riffhouse on the track as an effect.
3. Switch on input monitoring for the track, so you hear yourself through the plugin.
4. Set the DAW's audio buffer to 128 samples or less while you play. Riffhouse adds no delay of its own. What you feel is the buffer.

## What is saved with the project

- The whole tone, every knob and switch, and the level auto level settled on. A project sounds the same when you open it again.
- A neural capture or an impulse response is saved as the path to its file. Leave the file where it was, or load it again.
- An export sounds like what you heard: auto level settles each tone the same way offline as it does live.

## If something is off

- **No sound**: check input monitoring on the track, and that the track's input is the one your guitar is on.
- **Crackles**: raise the DAW's buffer one step. Other heavy plugins on the same project use the same processor.
- **You hear yourself late**: lower the buffer. If your audio interface has direct monitoring, switch it off, or you hear the dry guitar as well.
- **Loud, harsh high-gain tones**: your input level is too hot or your input swallows treble. Run setup in the app again, then load a new copy of the plugin.
- **Keyboard shortcuts**: inside a DAW the keys belong to the DAW. Space starts its transport and Ctrl+Z is its undo, also while the Riffhouse window is open.

Still stuck? In the app, open Settings and press "Save a report for help". It writes a text file with your audio settings into your recordings folder. Nothing is sent anywhere. Attach it to an issue on the project page.
