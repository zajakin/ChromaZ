  VER="1.0.0"
  # git tag v${VER} && git push origin v${VER}  
  sudo apt install mingw-w64 mingw-w64-tools cmake make  imagemagick librsvg2-bin
  
  cd /home/pawel/Desktop/ChromaZ/
  rm -rf build
  mkdir build && cd build
  rsvg-convert -f png -w 256 -h 256 ../src/app.svg -o chromaz.png
  convert chromaz.png ../src/app.ico
  cmake ../src && make && ctest --output-on-failure
  
  set -e
  PKG_DIR="chromaz_${VER}_amd64"
  rm -rf "$PKG_DIR"
  mkdir -p "$PKG_DIR/DEBIAN"
  mkdir -p "$PKG_DIR/usr/bin"
  mkdir -p "$PKG_DIR/usr/share/applications"
  mkdir -p "$PKG_DIR/usr/share/pixmaps"
  cat << EOF > $PKG_DIR/DEBIAN/control
Package: chromaz
Version: ${VER}
Section: science
Priority: optional
Architecture: amd64
Depends: libqt6widgets6 (>= 6.0.0), libqt6gui6 (>= 6.0.0), libqt6core6 (>= 6.0.0), libc6 (>= 2.27)
Maintainer: Pawel Zayakin <pz@magisters.net>
Description: Sanger Chromatogram Alignment and Contig Viewer
 ChromaZ allows viewing Sanger AB1 chromatograms, sequence alignment,
 and contig visualization with interactive editing.
EOF
  cat << 'EOF' > $PKG_DIR/usr/share/applications/chromaz.desktop
[Desktop Entry]
Name=ChromaZ
Comment=Sanger Chromatogram Alignment Viewer
Exec=/usr/bin/ChromaZ
Icon=chromaz
Terminal=false
Type=Application
Categories=Science;Biology;
EOF
  cp ChromaZ "$PKG_DIR/usr/bin/"
  cp chromaz.png "$PKG_DIR/usr/share/pixmaps/"
  find "$PKG_DIR" -type d -exec chmod 755 {} +
  find "$PKG_DIR" -type f -exec chmod 644 {} +
  chmod +x "$PKG_DIR/usr/bin/ChromaZ"
  dpkg-deb --root-owner-group --build "$PKG_DIR"
  mv "$PKG_DIR.deb" ../

  #### Windows  ####
  mkdir -p ~/mingw-qt6 && cd ~/mingw-qt6
  wget https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-qt6-base-6.11.2-2-any.pkg.tar.zst
  tar -I zstd -xvf mingw-w64-x86_64-qt6-base-*.pkg.tar.zst
  rm mingw-w64-x86_64-qt6-base-*.pkg.tar.zst

  cd /home/pawel/Desktop/ChromaZ
  rm -rf build-win
  mkdir -p build-win && cd build-win
  cmake -DCMAKE_TOOLCHAIN_FILE=../src/toolchain-mingw64.cmake ../src
  make
  cp ChromaZ.exe ../win/
  cd ..
  makensis src/installer.nsi
  
  git tag v${VER} && git push origin v${VER}
  
