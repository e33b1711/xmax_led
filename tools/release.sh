#!/usr/bin/env bash
# Neue Firmware-Version bauen, signieren und für den Kanal "test" (Prototyp) veröffentlichen.
#   tools/release.sh 1.0.1 "Was ist neu"
# Danach bekommt sie der Prototyp innerhalb von ~15 Minuten. Für die Geschenke: tools/freigeben.sh 1.0.1
set -euo pipefail
cd "$(dirname "$0")/.."

V="${1:?Version angeben, z. B. 1.0.1}"
NOTES="${2:-Version $V}"
KEY="$HOME/.config/xmax_led/signatur-privat.pem"
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"
REPO="e33b1711/xmax_led"

[[ "$V" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Version im Format 1.2.3"; exit 1; }
[ -f "$KEY" ] || { echo "Signaturschlüssel fehlt: $KEY"; exit 1; }
[ -z "$(git status --porcelain)" ] || { echo "Erst alle Änderungen committen."; git status --short; exit 1; }
[ "$(git branch --show-current)" = main ] || { echo "Nur vom Zweig main."; exit 1; }
OLD=$(cat VERSION)
python3 - "$V" "$OLD" <<'EOF' || { echo "Version muss größer als $(cat VERSION) sein"; exit 1; }
import sys
a, b = (tuple(map(int, x.split("."))) for x in sys.argv[1:3])
sys.exit(0 if a > b else 1)
EOF

echo "$V" > VERSION
(cd firmware && "$PIO" run -e sanduhr)
mkdir -p build
BIN="build/sanduhr-$V.bin"
cp firmware/.pio/build/sanduhr/firmware.bin "$BIN"
SHA=$(sha256sum "$BIN" | cut -d' ' -f1)
SIZE=$(stat -c %s "$BIN")

# Signatur über "xmax_led|<version>|<sha256>" - genau so prüft es die Sanduhr
MSG="xmax_led|$V|$SHA"
printf '%s' "$MSG" > build/msg.txt
openssl dgst -sha256 -sign "$KEY" -out build/sig.der build/msg.txt
openssl ec -in "$KEY" -pubout 2>/dev/null > build/pub.pem
openssl dgst -sha256 -verify build/pub.pem -signature build/sig.der build/msg.txt >/dev/null
grep -q "$(sed -n 2p build/pub.pem)" firmware/src/pubkey.h || { echo "Schlüssel passt nicht zu firmware/src/pubkey.h!"; exit 1; }
SIG=$(base64 -w0 build/sig.der)
URL="https://github.com/$REPO/releases/download/v$V/sanduhr-$V.bin"

git add VERSION
git commit -m "Version $V"
git tag "v$V"
git push origin main "v$V"
gh release create "v$V" "$BIN" --repo "$REPO" --prerelease --title "Sanduhr $V" --notes "$NOTES"

mkdir -p ota/versionen
cat > "ota/versionen/$V.json" <<EOF
{
  "version": "$V",
  "url": "$URL",
  "size": $SIZE,
  "sha256": "$SHA",
  "sig": "$SIG"
}
EOF
cp "ota/versionen/$V.json" ota/manifest-test.json
git add ota
git commit -m "Version $V im Kanal test"
git push origin main
rm -f build/msg.txt build/sig.der build/pub.pem
echo
echo "Version $V ist im Kanal test. Der Prototyp holt sie sich (oder Taste B 5 s / 'u' auf der Konsole)."
echo "Für die Geschenke freigeben: tools/freigeben.sh $V"
