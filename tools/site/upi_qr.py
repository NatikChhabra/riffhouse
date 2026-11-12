"""Draws the donation QR for the site as a plain SVG in the site's colours.

    python3 tools/site/upi_qr.py

The payload is the same one the payee's UPI app shows, so any UPI app reads it.
Needs: pip install qrcode
"""
from pathlib import Path
import qrcode

PAYLOAD = "upi://pay?pa=natik.chhabra@ptaxis&pn=Riff%20House&cu=INR"
INK, PAPER, QUIET = "#15100a", "#ece9e3", 4
OUT = Path(__file__).resolve().parents[2] / "site" / "img" / "upi-qr.svg"


def main():
    qr = qrcode.QRCode(border=0, error_correction=qrcode.constants.ERROR_CORRECT_Q)
    qr.add_data(PAYLOAD)
    qr.make(fit=True)
    rows = qr.get_matrix()
    size = len(rows) + 2 * QUIET
    path = []
    for y, row in enumerate(rows):
        x = 0
        while x < len(row):
            if row[x]:
                run = x
                while run < len(row) and row[run]:
                    run += 1
                path.append(f"M{x + QUIET} {y + QUIET}h{run - x}v1h-{run - x}z")
                x = run
            else:
                x += 1
    OUT.write_text(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}" shape-rendering="crispEdges">'
        f'<rect width="{size}" height="{size}" fill="{PAPER}"/><path fill="{INK}" d="{"".join(path)}"/></svg>\n'
    )
    print(OUT, size)


if __name__ == "__main__":
    main()
