#!/usr/bin/env bash
# Notbremse: Kanal "stabil" wieder auf eine ältere Version zeigen lassen.
# Wirkt nur auf Geräte, die die neue Version noch NICHT geholt haben (ein Zurückspielen
# verhindert die Sanduhr absichtlich). Für Geräte mit der fehlerhaften Version: neue,
# höhere Version mit der Korrektur bauen und freigeben.
#   tools/zurueckziehen.sh 1.0.0
set -euo pipefail
cd "$(dirname "$0")/.."
V="${1:?Version angeben, auf die der Kanal zeigen soll}"
[ -f "ota/versionen/$V.json" ] || { echo "Version $V gibt es nicht"; exit 1; }
cp "ota/versionen/$V.json" ota/manifest-stabil.json
git add ota/manifest-stabil.json
git commit -m "Kanal stabil zurück auf $V"
git push origin main
echo "Kanal stabil zeigt wieder auf $V."
