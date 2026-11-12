# The Riffhouse logo

The combo badge (an amp-face "R" on grille cloth in a tolex frame) and the script name, built in
code so every size comes from one source.

- `mk.py`: the 3D badge and the 3D script, rendered with Blender's Python module (`bpy`).
- `name.py`, `outline.py`: the script name from Yellowtail (OFL, in resources/fonts), letters pulled
  together until the strokes weld, a baseline rising 4 degrees, one swash.
- `icons.py`, `flat2.py`: the drawn mark for 16 to 48 px and the flat versions.
- `assemble.py <out folder>`: every final file (icons, .ico, wordmarks, social card) from the masters
  that `master2.sh` renders.

Colours: cream #efe2bf, frame #171615, cloth #86693a, lamp #f08a1c.

The app uses resources/brand (the badge and the script for the top bar), resources/icon.png and
resources/icon-small.png, and packaging/riffhouse.ico.
