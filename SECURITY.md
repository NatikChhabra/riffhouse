# Security

## Found a problem?

Please tell us privately, not in a public issue:
use **Report a vulnerability** on the [Security tab](https://github.com/NatikChhabra/riffhouse/security/advisories/new).
We read every report and reply within a few days.

## How you can check a download

- Only download Riffhouse from the [Releases page](https://github.com/NatikChhabra/riffhouse/releases) or the
  [website](https://natikchhabra.github.io/riffhouse/), which links there.
- Every release lists the SHA-256 of its installer. In PowerShell:
  `Get-FileHash .\Riffhouse-*-windows-setup.exe` must show the same value.
- Installers built from now on carry a signed build record from GitHub. With the GitHub CLI:
  `gh attestation verify .\Riffhouse-*-windows-setup.exe -R NatikChhabra/riffhouse`
  proves the file was built by this repository's own workflow, from public code, and not uploaded by hand.

## How the app stays safe

- Updates come only from this repository's releases over HTTPS. The app checks the installer's SHA-256
  against the one GitHub publishes before it runs anything, and throws the file away if they differ.
- Bug reports leave the PC only when the player presses Send, with the Windows user name and PC name removed.
- The weekly player count sends the week, a random install id and the app version. Nothing else.
  It can be switched off in Settings. See [docs/PRIVACY.md](docs/PRIVACY.md).
- No passwords, keys or tokens are inside the app.
