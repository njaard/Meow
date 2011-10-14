#!/bin/sh

version=$1
sandbox=$2

here=$PWD

cd $sandbox
reprepro includedeb squeeze $here/meow_$version\_x86_64.deb
reprepro includedeb squeeze $here/meow-qt_$version\_x86_64.deb
reprepro includedeb squeeze $here/meow_$version\_i686.deb
reprepro includedeb squeeze $here/meow-qt_$version\_i686.deb

cd ..

rsync -avz meow-deb/ lethe:derkarl.org/meow/debian

