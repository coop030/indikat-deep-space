#!/bin/bash
# Installiert oder aktualisiert INDIKAT-Plugins direkt von GitHub, ohne Gatekeeper-Warnung:
# curl setzt keine Quarantäne-Markierung (anders als der Browser).
#
#   curl -fsSL https://raw.githubusercontent.com/coop030/indikat-deep-space/main/packaging/macos/install.sh | bash -s deepspace pluto
#
# Namen: deepspace, pluto, starpad, bass  (ohne Angabe: deepspace)
set -euo pipefail

VST3="/Library/Audio/Plug-Ins/VST3"
AU="/Library/Audio/Plug-Ins/Components"

# Name -> Repository und Tag-Präfix der Releases
target () {
    case "$1" in
        deepspace|deep-space) echo "coop030/indikat-deep-space build-" ;;
        pluto)                echo "coop030/indikat-bass pluto-build-" ;;
        starpad|star-pad)     echo "coop030/indikat-bass starpad-build-" ;;
        bass)                 echo "coop030/indikat-bass build-" ;;
        *) return 1 ;;
    esac
}

[ $# -eq 0 ] && set -- deepspace

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "Mac-Passwort für die Installation nach /Library/Audio/Plug-Ins:"
sudo -v
sudo mkdir -p "$VST3" "$AU"

for name in "$@"; do
    if ! t=$(target "$name"); then
        echo "Unbekannt: $name (möglich: deepspace, pluto, starpad, bass)"; exit 1
    fi
    repo=${t% *}; prefix=${t#* }

    echo ""
    echo "== $name: suche den neuesten Build in $repo ..."
    json=$(curl -fsSL "https://api.github.com/repos/$repo/releases?per_page=100")
    num=$(printf '%s' "$json" | grep -o "\"tag_name\": *\"$prefix[0-9]*\"" | grep -o '[0-9]*"$' | tr -d '"' | sort -n | tail -1)
    if [ -z "$num" ]; then echo "Kein Release für $name gefunden."; exit 1; fi
    tag="$prefix$num"
    url=$(printf '%s' "$json" | grep -o "\"browser_download_url\": *\"[^\"]*/download/$tag/[^\"]*macOS\.zip\"" | head -1 | sed 's/.*"\(https[^"]*\)"$/\1/')
    if [ -z "$url" ]; then echo "Kein macOS-Download in $tag."; exit 1; fi

    echo "Lade $tag ..."
    rm -rf "$TMP/x" "$TMP/plugin.zip"
    curl -fL --progress-bar "$url" -o "$TMP/plugin.zip"
    ditto -x -k "$TMP/plugin.zip" "$TMP/x"

    found=0
    while IFS= read -r -d '' p; do
        file=$(basename "$p")
        case "$file" in
            *.vst3)      dest="$VST3"; user="$HOME/Library/Audio/Plug-Ins/VST3" ;;
            *.component) dest="$AU";   user="$HOME/Library/Audio/Plug-Ins/Components" ;;
            *) continue ;;
        esac
        sudo rm -rf "$dest/$file"
        sudo cp -R "$p" "$dest/"
        sudo xattr -cr "$dest/$file"
        sudo codesign --force --deep --sign - "$dest/$file" >/dev/null 2>&1
        rm -rf "$user/$file" 2>/dev/null || true   # alte Kopie im Benutzerordner, sonst findet die DAW zwei
        echo "  installiert: $file"
        found=1
    done < <(find "$TMP/x" -maxdepth 3 \( -name '*.vst3' -o -name '*.component' \) -prune -print0)
    [ "$found" -eq 1 ] || { echo "Im Download waren keine Plugins."; exit 1; }
done

# AU-Cache auffrischen, damit die DAW die neuen Plugins findet
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo ""
echo "Fertig. DAW neu starten und Plugins neu scannen (FL Studio: Options > Manage plugins > Find installed plugins)."
