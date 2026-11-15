#!/bin/bash
# INDIKAT Updater (macOS + Linux)
#
# Sucht in allen GitHub-Repos von coop030, die mit "indikat-" anfangen, den neuesten Build jedes
# Plugins und installiert nur das, was neuer ist als die installierte Version.
# Neue Plugins (neue Repos oder neue Release-Reihen wie "xyz-build-1") findet er von selbst.
#
#   curl -fsSL https://raw.githubusercontent.com/coop030/indikat-deep-space/main/packaging/updater/indikat-update.sh | bash
#
# Optionen (nach "bash -s"):
#   check          nur anzeigen, was es Neues gibt, nichts installieren
#   force          alles neu installieren, auch wenn es schon aktuell ist
#   <name> ...     nur diese Plugins, z. B. "deep-space pluto" (Teil des Namens reicht)
#
# Auf dem Mac legt der erste Lauf "INDIKAT Updater.command" auf den Schreibtisch:
# danach reicht ein Doppelklick. curl setzt keine Quarantäne, daher keine Gatekeeper-Warnung.
set -euo pipefail

OWNER="coop030"
SELF_URL="https://raw.githubusercontent.com/coop030/indikat-deep-space/main/packaging/updater/indikat-update.sh"
API="https://api.github.com"

case "$(uname -s)" in
    Darwin) OS=macOS ;;
    Linux)  OS=Linux ;;
    *) echo "Dieses Skript ist für macOS und Linux. Unter Windows: indikat-update.ps1"; exit 1 ;;
esac

# Zielordner (per Umgebungsvariable änderbar, z. B. zum Testen)
if [ "$OS" = macOS ]; then
    VST3="${INDIKAT_VST3_DIR:-/Library/Audio/Plug-Ins/VST3}"
    AU="${INDIKAT_AU_DIR:-/Library/Audio/Plug-Ins/Components}"
    STATE_DIR="${INDIKAT_STATE_DIR:-$HOME/Library/Application Support/INDIKAT}"
else
    VST3="${INDIKAT_VST3_DIR:-$HOME/.vst3}"
    AU=""
    STATE_DIR="${INDIKAT_STATE_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/indikat}"
fi
STATE="$STATE_DIR/installed.txt"   # Zeilen: "<repo>:<reihe> <tag>"
SUDO=""
if [ "$OS" = macOS ] && [ -z "${INDIKAT_NO_SUDO:-}" ]; then SUDO="sudo"; fi

CHECK=0; FORCE=0; FILTER=""
for a in "$@"; do
    case "$a" in
        check|--check) CHECK=1 ;;
        force|--force) FORCE=1 ;;
        -h|--help) sed -n '2,16p' "$0" 2>/dev/null || true; exit 0 ;;
        *) FILTER="$FILTER $(printf '%s' "$a" | tr 'A-Z' 'a-z' | tr -d ' _-')" ;;
    esac
done

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$STATE_DIR"; touch "$STATE"

fetch () { curl -fsSL -H "Accept: application/vnd.github+json" "$1"; }

# Doppelklick-Verknüpfung auf dem Mac-Schreibtisch (lädt immer die aktuelle Updater-Version)
if [ "$OS" = macOS ] && [ -z "${INDIKAT_NO_SHORTCUT:-}" ]; then
    LINK="$HOME/Desktop/INDIKAT Updater.command"
    if [ ! -e "$LINK" ]; then
        printf '#!/bin/bash\ncurl -fsSL "%s" | bash -s "$@"\necho ""\nread -n 1 -s -r -p "Taste drücken zum Schließen ..."\n' "$SELF_URL" > "$LINK"
        chmod +x "$LINK"
        echo "Auf dem Schreibtisch liegt jetzt \"INDIKAT Updater.command\" – ab jetzt einfach doppelklicken."
        echo ""
    fi
fi

echo "INDIKAT Updater – suche neue Builds ($OS) ..."

# 1) Alle indikat-Repos finden
#    (falls GitHub die Liste verweigert: bekannte Repos als Rückfall)
KNOWN="$OWNER/indikat-bass $OWNER/indikat-deep-space"
repos=${INDIKAT_REPOS:-$( (fetch "$API/users/$OWNER/repos?per_page=100" 2>/dev/null || true) \
    | grep -o "\"full_name\": *\"$OWNER/indikat-[^\"]*\"" | sed 's/.*"\([^"]*\)"$/\1/' | sort -u || true)}
[ -n "$repos" ] || repos=$KNOWN

# 2) Pro Repo alle Downloads für dieses System sammeln: "<repo>:<reihe> <nummer> <tag> <url> <name>"
: > "$TMP/all"
for repo in $repos; do
    json=$(fetch "$API/repos/$repo/releases?per_page=100") || { echo "  ($repo nicht erreichbar, übersprungen)"; continue; }
    { printf '%s' "$json" | grep -o "\"browser_download_url\": *\"[^\"]*-$OS\.zip\"" || true; } \
        | sed 's/.*"\(https[^"]*\)"$/\1/' | while read -r url; do
        tag=$(printf '%s' "$url" | sed 's#.*/download/\([^/]*\)/.*#\1#')
        num=$(printf '%s' "$tag" | grep -o '[0-9]*$' || true)
        [ -n "$num" ] || continue
        series=${tag%"$num"}
        file=${url##*/}
        name=$(printf '%s' "$file" | sed "s/-$OS\.zip\$//; s/^INDIKAT-//")
        echo "$repo:$series $num $tag $url $name"
    done >> "$TMP/all"
done

# 3) Pro Reihe den höchsten Build nehmen
sort -k1,1 -k2,2nr "$TMP/all" | awk '!seen[$1]++' > "$TMP/latest"
[ -s "$TMP/latest" ] || { echo "Keine Downloads für $OS gefunden."; exit 1; }

installed_tag () { awk -v k="$1" '$1==k {print $2}' "$STATE" | tail -1; }

# 4) Was ist neu?
: > "$TMP/todo"
printf '\n  %-16s %-20s %s\n' "Plugin" "installiert" "neueste"
while read -r key num tag url name; do
    if [ -n "$FILTER" ]; then
        n=$(printf '%s' "$name" | tr 'A-Z' 'a-z' | tr -d ' _-'); hit=0
        for f in $FILTER; do case "$n" in *"$f"*) hit=1 ;; esac; done
        [ "$hit" -eq 1 ] || continue
    fi
    have=$(installed_tag "$key")
    if [ "$FORCE" -eq 1 ] || [ "$have" != "$tag" ]; then
        mark="  <- neu"; echo "$key $tag $url $name" >> "$TMP/todo"
    else
        mark="  ok"
    fi
    printf '  %-16s %-20s %s%s\n' "$name" "${have:--}" "$tag" "$mark"
done < "$TMP/latest"
echo ""

if [ ! -s "$TMP/todo" ]; then echo "Alles aktuell."; exit 0; fi
if [ "$CHECK" -eq 1 ]; then echo "(nur geprüft – zum Installieren ohne \"check\" starten)"; exit 0; fi

# 5) Installieren
if [ -n "$SUDO" ]; then
    echo "Mac-Passwort für die Installation nach /Library/Audio/Plug-Ins:"
    sudo -v
fi
$SUDO mkdir -p "$VST3"
[ -n "$AU" ] && $SUDO mkdir -p "$AU"

unpack () {
    if [ "$OS" = macOS ] && command -v ditto >/dev/null; then ditto -x -k "$1" "$2"; else unzip -q "$1" -d "$2"; fi
}

failed=0
while read -r key tag url name; do
    echo "== $name ($tag)"
    rm -rf "$TMP/x" "$TMP/p.zip"
    if ! curl -fL --progress-bar "$url" -o "$TMP/p.zip" || ! unpack "$TMP/p.zip" "$TMP/x"; then
        echo "  Download fehlgeschlagen, übersprungen."; failed=1; continue
    fi
    found=0
    while IFS= read -r -d '' p; do
        file=$(basename "$p")
        case "$file" in
            *.vst3)      dest="$VST3"; user="$HOME/Library/Audio/Plug-Ins/VST3" ;;
            *.component) [ -n "$AU" ] || continue; dest="$AU"; user="$HOME/Library/Audio/Plug-Ins/Components" ;;
            *) continue ;;
        esac
        $SUDO rm -rf "$dest/$file"
        $SUDO cp -R "$p" "$dest/"
        if [ "$OS" = macOS ]; then
            $SUDO xattr -cr "$dest/$file" 2>/dev/null || true
            $SUDO codesign --force --deep --sign - "$dest/$file" >/dev/null 2>&1 || true
            # alte Kopie im Benutzerordner, sonst findet die DAW das Plugin doppelt
            [ "$dest" != "$user" ] && rm -rf "$user/$file" 2>/dev/null || true
        fi
        echo "  installiert: $file"
        found=1
    done < <(find "$TMP/x" -maxdepth 3 \( -name '*.vst3' -o -name '*.component' \) -prune -print0)
    if [ "$found" -eq 1 ]; then
        { grep -v "^$key " "$STATE" || true; echo "$key $tag"; } > "$TMP/state" && cat "$TMP/state" > "$STATE"
    else
        echo "  Im Download waren keine Plugins."; failed=1
    fi
done < "$TMP/todo"

# AU-Cache auffrischen, damit die DAW die neuen Plugins findet
[ "$OS" = macOS ] && { killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true; }

echo ""
if [ "$failed" -eq 0 ]; then echo "Fertig."; else echo "Fertig, aber nicht alles hat geklappt (siehe oben)."; fi
echo "DAW neu starten und Plugins neu scannen (FL Studio: Options > Manage plugins > Find installed plugins)."
exit "$failed"
