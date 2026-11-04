#!/bin/bash
# Installiert oder aktualisiert ein INDIKAT-Plugin direkt von GitHub, ohne Gatekeeper-Warnung:
# curl setzt keine Quarantäne-Markierung (anders als der Browser).
#
#   curl -fsSL https://raw.githubusercontent.com/coop030/indikat-deep-space/main/packaging/macos/install.sh | bash
#
# Anderes Plugin: Repository als Argument, z. B.
#   curl -fsSL https://raw.githubusercontent.com/coop030/indikat-deep-space/main/packaging/macos/install.sh | bash -s coop030/indikat-bass
set -euo pipefail

REPO="${1:-coop030/indikat-deep-space}"
VST3="/Library/Audio/Plug-Ins/VST3"
AU="/Library/Audio/Plug-Ins/Components"

echo "Suche den neuesten Build von $REPO ..."
URL=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" \
      | grep -o '"browser_download_url": *"[^"]*macOS\.zip"' | head -1 | sed 's/.*"\(https[^"]*\)"$/\1/')
if [ -z "$URL" ]; then
    echo "Kein macOS-Download in $REPO gefunden."
    exit 1
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
echo "Lade $(basename "$URL") ..."
curl -fL --progress-bar "$URL" -o "$TMP/plugin.zip"
ditto -x -k "$TMP/plugin.zip" "$TMP/x"

echo "Installiere nach $VST3 und $AU (Mac-Passwort erforderlich) ..."
sudo mkdir -p "$VST3" "$AU"

found=0
while IFS= read -r -d '' p; do
    name=$(basename "$p")
    case "$name" in
        *.vst3)      dest="$VST3" ;;
        *.component) dest="$AU" ;;
        *) continue ;;
    esac
    sudo rm -rf "$dest/$name"
    sudo cp -R "$p" "$dest/"
    sudo xattr -cr "$dest/$name"
    sudo codesign --force --deep --sign - "$dest/$name" >/dev/null 2>&1
    echo "  $name"
    found=1
done < <(find "$TMP/x" -maxdepth 3 \( -name '*.vst3' -o -name '*.component' \) -prune -print0)

if [ "$found" -eq 0 ]; then
    echo "Im Download waren keine Plugins."
    exit 1
fi

# AU-Cache auffrischen, damit die DAW die neuen Plugins findet
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo ""
echo "Fertig. DAW neu starten und Plugins neu scannen (FL Studio: Options > Manage plugins > Find installed plugins)."
