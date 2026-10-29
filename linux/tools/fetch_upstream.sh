#!/usr/bin/env bash
# Download the pinned upstream CrystalDiskInfo release files and unpack the
# source. Nothing from upstream is committed to this repository: the source is
# MIT-licensed, but the edition artwork and voices belong to their artists and
# are fetched from the official mirrors at build time.
#
#   fetch_upstream.sh <downloads dir> <src dir>
#
# Files already present with the right checksum are reused, so a manual
# download into <downloads dir> works too.
set -euo pipefail

DL=$1
SRC=$2
VERSION=9.9.3
BASE="https://downloads.sourceforge.net/project/crystaldiskinfo/$VERSION"

# file                                sha256
FILES=(
	"CrystalDiskInfo9_9_3Src.zip      272b54323335e4fcb3871ccc21146935e9990c1516cfaa79a50bb0b3e553f921"
	"CrystalDiskInfo9_9_3.zip         364f40d9161175a4d8b797edc21f9e57c1671dbfb689aadf9d91449ca814015c"
	"CrystalDiskInfo9_9_3Shizuku.zip  4b667676fcf91ccdf69ee5938ef62143155719e3e59960d7874cc1f524a83efb"
	"CrystalDiskInfo9_9_3KureiKei.zip 460bab8a6ce356e1ee04c06e2d814b9016346a1dd6831ff83bf4ac289b4aa8a1"
	"CrystalDiskInfo9_9_3Aoi.zip      c81b17bd39d888a04d2d49678b64a58114157985e7d25afa3d66e0f444744ff4"
)

mkdir -p "$DL"
pids=()
for entry in "${FILES[@]}"; do
	read -r file sum <<<"$entry"
	if [ -f "$DL/$file" ] && echo "$sum  $DL/$file" | sha256sum -c --quiet - 2>/dev/null; then
		continue
	fi
	echo "downloading $file" >&2
	(
		curl -fL --retry 5 --retry-delay 3 -s -o "$DL/$file.part" "$BASE/$file"
		echo "$sum  $DL/$file.part" | sha256sum -c --quiet -
		mv "$DL/$file.part" "$DL/$file"
	) &
	pids+=($!)
done
status=0
for p in "${pids[@]}"; do wait "$p" || status=1; done
if [ $status -ne 0 ]; then
	echo "download or checksum failed" >&2
	exit 1
fi

if [ ! -f "$SRC/AtaSmart.cpp" ]; then
	mkdir -p "$SRC"
	unzip -qq -o "$DL/CrystalDiskInfo9_9_3Src.zip" -d "$SRC"
fi
