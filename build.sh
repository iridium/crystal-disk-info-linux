#!/usr/bin/env bash
# Build CrystalDiskInfo for Linux.
#
#   ./build.sh            build on this machine into ./dist
#   ./build.sh --podman   build inside an Arch podman container instead
#   ./build.sh clean      remove build outputs (keeps downloads/)
#
# Upstream files are downloaded into downloads/ and the source is unpacked
# into upstream/; both are pinned by SHA-256 in linux/tools/fetch_upstream.sh.
set -euo pipefail
cd "$(dirname "$0")"

STEPS='
	linux/tools/fetch_upstream.sh downloads upstream
	cmake -S linux -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/dist"
	ninja -C build
	ninja -C build install >/dev/null
	linux/tools/assemble_resources.sh downloads upstream dist/share/crystaldiskinfo
'

case "${1:-}" in
clean)
	rm -rf build dist upstream
	exit 0
	;;
--podman)
	IMAGE=cdi-linux-build
	podman build -q -t "$IMAGE" -f Containerfile . >/dev/null
	podman run --rm --userns=keep-id -v "$PWD":/work:Z -w /work "$IMAGE" bash -euo pipefail -c "$STEPS"
	;;
"")
	missing=()
	for c in cmake ninja python3 curl unzip sha256sum; do command -v "$c" >/dev/null || missing+=("$c"); done
	python3 -c 'import pefile' 2>/dev/null || missing+=("python pefile")
	python3 -c 'import PIL' 2>/dev/null || missing+=("python pillow")
	if [ ${#missing[@]} -gt 0 ]; then
		echo "missing: ${missing[*]}" >&2
		echo "Install the build dependencies (see README) or run ./build.sh --podman" >&2
		exit 1
	fi
	bash -euo pipefail -c "$STEPS"
	;;
*)
	sed -n '2,9p' "$0" >&2
	exit 1
	;;
esac

cat <<MSG

Built into ./dist. Run:  ./dist/bin/crystaldiskinfo

Reading S.M.A.R.T. needs raw disk access. Either grant it once to the small helper:
  sudo setcap cap_sys_rawio,cap_sys_admin,cap_dac_read_search+ep dist/libexec/crystaldiskinfo/cdi-probe
or use "Function > Advanced Feature > Rescan as Administrator (pkexec)" in the app.
(Rebuilding replaces the helper, so run setcap again after ./build.sh.)
MSG
