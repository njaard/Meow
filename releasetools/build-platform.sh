#!/bin/bash

platform="$1"
packagefmt="$2"

if [[ $packagefmt == "deb" ]]
then
	meow_package=DEB
elif [[ $packagefmt == "rpm" ]]
then
	meow_package=RPM
fi

mkdir build-$platform-kde
cd build-$platform-kde

cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=$meow_package

make -j6 package
mv *.$packagefmt ../packages

