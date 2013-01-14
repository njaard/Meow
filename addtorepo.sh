#!/bin/sh

version=$1
sandbox=$2

here=$PWD

cd $sandbox

packages="$here/meow_${version}_x86_64.deb $here/meow-qt_${version}_x86_64.deb $here/meow_${version}_i686.deb $here/meow-qt_${version}_i686.deb"

dpkg-sig -s origin $packages

for i in $packages;
do
	reprepro includedeb squeeze $i
done

cd ..
rsync -avz meow-deb/ derkarl.org:derkarl.org/meow/debian

