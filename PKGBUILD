# Build from a clone of https://github.com/iridium/crystal-disk-info-linux with: makepkg -si
pkgname=crystal-disk-info-linux
_upver=9.9.3
_uptag=${_upver//./_}
pkgver=${_upver}
pkgrel=1
pkgdesc="Unofficial Linux port of CrystalDiskInfo with the Shizuku, Kurei Kei and Aoi editions"
arch=(x86_64 aarch64)
url="https://github.com/iridium/crystal-disk-info-linux"
license=(MIT)
depends=(qt6-base qt6-multimedia gcc-libs glibc libcap)
makedepends=(cmake ninja python python-pefile python-pillow unzip)
optdepends=('polkit: scan through pkexec when the helper has no capabilities')
provides=(crystaldiskinfo)
install=crystal-disk-info-linux.install
_sf="https://downloads.sourceforge.net/project/crystaldiskinfo/${_upver}"
source=("${_sf}/CrystalDiskInfo${_uptag}Src.zip"
        "${_sf}/CrystalDiskInfo${_uptag}.zip"
        "${_sf}/CrystalDiskInfo${_uptag}Shizuku.zip"
        "${_sf}/CrystalDiskInfo${_uptag}KureiKei.zip"
        "${_sf}/CrystalDiskInfo${_uptag}Aoi.zip")
noextract=("${source[@]##*/}")
sha256sums=('272b54323335e4fcb3871ccc21146935e9990c1516cfaa79a50bb0b3e553f921'
            '364f40d9161175a4d8b797edc21f9e57c1671dbfb689aadf9d91449ca814015c'
            '4b667676fcf91ccdf69ee5938ef62143155719e3e59960d7874cc1f524a83efb'
            '460bab8a6ce356e1ee04c06e2d814b9016346a1dd6831ff83bf4ac289b4aa8a1'
            'c81b17bd39d888a04d2d49678b64a58114157985e7d25afa3d66e0f444744ff4')

prepare() {
	rm -rf upstream
	mkdir upstream
	bsdtar -xf "CrystalDiskInfo${_uptag}Src.zip" -C upstream
}

build() {
	cmake -S "$startdir/linux" -B build -G Ninja \
		-DCMAKE_BUILD_TYPE=None \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCDI_UPSTREAM="$srcdir/upstream"
	cmake --build build
}

package() {
	DESTDIR="$pkgdir" cmake --install build
	"$startdir/linux/tools/assemble_resources.sh" "$srcdir" "$srcdir/upstream" "$pkgdir/usr/share/crystaldiskinfo"
	install -Dm644 "$startdir/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
	install -Dm644 upstream/License/COPYRIGHT.txt "$pkgdir/usr/share/licenses/$pkgname/CrystalDiskInfo-COPYRIGHT.txt"
}
