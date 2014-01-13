#!/bin/bash

platform="$1"
packagefmt="$2"
version="$3"

if [[ $packagefmt == "deb" ]]
then
	meow_package=DEB
elif [[ $packagefmt == "rpm" ]]
then
	meow_package=RPM
fi


if [[ $platform == "debian-unstable-win32-x86_64" ]];
then
	mkdir build-win32
	cd build-win32
	cmake .. -DEXTRALIBS=/home/charles/dlls -DCMAKE_TOOLCHAIN_FILE=../releasetools/Toolchain-mingw32.cmake
	make -j4

	mv meow.exe meow_$version.exe
	i686-w64-mingw32-strip meow_$version.exe
	zip -9 meow_$version.zip meow_$version.exe
	upx --ultra-brute meow_$version.exe
	#upx -1 meow_$version.exe

	mv meow_$version.exe meow_$version.zip ../../packages
else
	mkdir build-$platform-kde
	cd build-$platform-kde
	cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=$meow_package
	make -j6 package
	mv *.$packagefmt ../../packages
	cd ..

	mkdir build-$platform-qt
	cd build-$platform-qt
	cmake .. -DMEOW_QT=1 -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=$meow_package
	make -j6 package
	mv *.$packagefmt ../../packages


fi
