#!/usr/bin/env bash
# Merge CdiResource from every downloaded CrystalDiskInfo edition zip into one
# share directory, so a single Linux build can switch between all editions'
# themes (Standard, Shizuku, Kurei Kei, Aoi).
#
#   assemble_resources.sh <downloads dir> <upstream src dir> <output share dir>
set -euo pipefail

DL=$1
SRC=$2
OUT=$3
HERE=$(cd "$(dirname "$0")" && pwd)

rm -rf "$OUT"
mkdir -p "$OUT/CdiResource/themes" "$OUT/CdiResource/language" "$OUT/CdiResource/voice" "$OUT/icons" "$OUT/License"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

shopt -s nullglob
# Standard first so edition-specific files win when names collide.
zips=()
for z in "$DL"/CrystalDiskInfo*.zip; do
	case "$z" in *Src.zip|*Installer*) continue ;; esac
	zips+=("$z")
done
IFS=$'\n' zips=($(printf '%s\n' "${zips[@]}" | awk '{ print (/[0-9]\.zip$/ ? 0 : 1) "\t" $0 }' | sort | cut -f2-))
unset IFS

for z in "${zips[@]}"; do
	name=$(basename "$z" .zip)
	if ! unzip -tqq "$z" >/dev/null 2>&1; then
		echo "skip $name (incomplete or corrupt zip)" >&2
		continue
	fi
	echo "merging $name" >&2
	mkdir -p "$stage/$name"
	unzip -qq -o "$z" -d "$stage/$name"
	res=$(find "$stage/$name" -maxdepth 3 -type d -name CdiResource | head -n1)
	[ -n "$res" ] || continue
	cp -a "$res/themes/." "$OUT/CdiResource/themes/" 2>/dev/null || true
	cp -a "$res/language/." "$OUT/CdiResource/language/" 2>/dev/null || true
	if [ -d "$res/License" ]; then cp -a "$res/License/." "$OUT/License/"; fi
	lic=$(find "$stage/$name" -maxdepth 2 -type d -name License | head -n1)
	if [ -n "$lic" ]; then cp -a "$lic/." "$OUT/License/"; fi

	# Character voices ship as resource-only DLLs holding Opus clips.
	for dll in "$res"/voice/*.dll; do
		python3 -I "$HERE/extract_voice.py" "$dll" "$OUT/CdiResource/voice" || echo "voice extraction failed for $dll" >&2
	done
done

# Small status icons for the S.M.A.R.T. list and per-edition window icons.
for i in good goodGreen caution bad unknown; do cp "$SRC/res/$i.ico" "$OUT/icons/"; done
for d in resN resS resK resA; do
	mkdir -p "$OUT/icons/$d"
	cp "$SRC/$d/DiskInfo.ico" "$OUT/icons/$d/" 2>/dev/null || true
done
cp "$SRC/License/"*.txt "$OUT/License/" 2>/dev/null || true

echo "themes: $(ls "$OUT/CdiResource/themes" | wc -l), languages: $(ls "$OUT/CdiResource/language" | wc -l), voices: $(find "$OUT/CdiResource/voice" -type f | wc -l)" >&2

# Desktop icon (hicolor) from the Standard edition icon.
mkdir -p "$OUT/../icons/hicolor/256x256/apps"
python3 -I -c '
import sys
from PIL import Image
im = Image.open(sys.argv[1])
im.size = max(im.ico.sizes())
im.load()
im.save(sys.argv[2])
' "$SRC/resN/DiskInfo.ico" "$OUT/../icons/hicolor/256x256/apps/crystaldiskinfo.png" || echo "icon conversion failed" >&2
