#!/bin/sh

platforms="fedora-i386 fedora-x86_64 stable-i386 stable-x86_64"

for i in $platforms;
do
	umount /var/meow-builds/$i/home
	umount /var/meow-builds/proc
	umount /var/meow-builds/dev
	mount --bind /var/meow-builds/home /var/meow-builds/$i/home/
	mount proc -t proc /var/meow-builds/proc/
	mount udev -t devtmpfs /var/meow-builds/dev


	arch=`echo $i | sed 's/^.*-//'`
	package_fmt=deb
	if [[ $i =~ fedora ]]; then package_fmt=rpm; fi
	$arch chroot /var/meow-builds/$i su - charles -c \
		"cd ~/meow; bash releasetools/build-platform.sh $i $package_fmt"
done


