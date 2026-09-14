  
  # sudo apt install mingw-w64 mingw-w64-tools cmake make
  
  cd /home/pawel/Desktop/ChromaZ
  rm -rf build
  mkdir build
  cd build
  cmake .. && make && ctest --output-on-failure

#   mkdir -p chromaz_1.0-1_amd64/DEBIAN
#   mkdir -p chromaz_1.0-1_amd64/usr/bin
#   mkdir -p chromaz_1.0-1_amd64/usr/share/applications
#   mkdir -p chromaz_1.0-1_amd64/usr/share/pixmaps
#   
#   >chromaz_1.0-1_amd64/DEBIAN/control
#   Package: chromaz
# Version: 1.0-1
# Section: science
# Priority: optional
# Architecture: amd64
# Depends: qt6-base-dev (>= 6.0.0), libc6 (>= 2.27)
# Maintainer: Pawel Zayakin <pz@magisters.net>
# Description: Sanger Chromatogram Alignment and Contig Viewer
#  ChromaZ allows viewing Sanger AB1 chromatograms, sequence alignment, 
#  and contig visualization with interactive editing.
#   
#   >chromaz_1.0-1_amd64/usr/share/applications/chromaz.desktop
# [Desktop Entry]
# Name=ChromaZ
# Comment=Sanger Chromatogram Alignment Viewer
# Exec=/usr/bin/ChromaZ
# Icon=chromaz
# Terminal=false
# Type=Application
# Categories=Science;Biology;
# 
# cp build/ChromaZ chromaz_1.0-1_amd64/usr/bin/
# chmod +x chromaz_1.0-1_amd64/usr/bin/ChromaZ
# cp chromaz.png chromaz_1.0-1_amd64/usr/share/pixmaps/
# chmod -R 755 chromaz_1.0-1_amd64
# dpkg-deb --build chromaz_1.0-1_amd64
  
  
  #### Windows  ####
  # mkdir -p ~/mingw-qt6 && cd ~/mingw-qt6
  # wget https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-qt6-base-6.11.2-2-any.pkg.tar.zst
  # tar -I zstd -xvf mingw-w64-x86_64-qt6-base-*.pkg.tar.zst
  # rm mingw-w64-x86_64-qt6-base-*.pkg.tar.zst

  cd /home/pawel/Desktop/ChromaZ
  rm -rf build-win
  mkdir -p build-win && cd build-win
  cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake ..
  make
  cp ChromaZ.exe ../win/
  cd ..
  makensis installer.nsi
  
#   cd .. && 7z a -t7z -m0=lzma2 -mx=9 build-win/archive.7z ChromaZ && cd build-win
#   cat << 'EOF' > config.txt
# ;!@Install@!UTF-8!
# Title="Установка ChromaZ"
# BeginPrompt="Распаковать и запустить ChromaZ?"
# RunProgram="ChromaZ\ChromaZ.exe"
# ;!@InstallEnd@!
# EOF
#   wget https://github.com/ip7z/7zip/releases/download/26.03/lzma2603.7z
#   7z e lzma2603.7z bin/7zS2.sfx bin/7zSD.sfx
#   cat 7zS2.sfx config.txt archive.7z > ../ChromaZ.exe
  
  