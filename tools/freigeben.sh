#!/usr/bin/env bash
# Eine Version, die auf dem Prototyp gut läuft, für alle Geschenke (Kanal "stabil") freigeben.
#   tools/freigeben.sh 1.0.1
# Die Sanduhren holen sie sich in der folgenden Nacht zwischen 2 und 5 Uhr.
set -euo pipefail
cd "$(dirname "$0")/.."

V="${1:?Version angeben, z. B. 1.0.1}"
REPO="e33b1711/xmax_led"
[ -f "ota/versionen/$V.json" ] || { echo "Version $V gibt es nicht (erst tools/release.sh)"; exit 1; }
[ -z "$(git status --porcelain)" ] || { echo "Erst alle Änderungen committen."; exit 1; }

cp "ota/versionen/$V.json" ota/manifest-stabil.json
git add ota/manifest-stabil.json
git commit -m "Version $V für alle freigegeben"
git push origin main
gh release edit "v$V" --repo "$REPO" --prerelease=false --latest
echo "Version $V ist freigegeben."
