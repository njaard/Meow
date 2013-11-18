#!/bin/sh
user="$1"

here="$PWD" 


mount --bind /var/stable-ia32/home /var/stable-ia64/home/

i386 chroot /var/stable-ia32 su - $user -c 'cd ~/meow; mkdir -p build-chroot32; cd build-chroot32; export PATH=/home/charles/cmake-2.8.5-Linux-i386/bin/:$PATH; cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=DEB; make -j6 package; mv *.deb ..' &

chroot /var/stable-ia64 su - $user -c 'cd ~/meow; mkdir -p build-chroot64; cd build-chroot64; export PATH=/home/charles/cmake-2.8.5-Linux-i386/bin/:$PATH; cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=DEB; make -j6 package; mv *.deb ..' &

i386 chroot /var/stable-ia32 su - $user -c 'cd ~/meow; mkdir -p buildqt-chroot32; cd buildqt-chroot32; export PATH=/home/charles/cmake-2.8.5-Linux-i386/bin/:$PATH; cmake .. -DMEOW_QT=1 -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=DEB; make -j6 package; mv *.deb ..' &

chroot /var/stable-ia64 su - $user -c 'cd ~/meow; mkdir -p buildqt-chroot64; cd buildqt-chroot64; export PATH=/home/charles/cmake-2.8.5-Linux-i386/bin/:$PATH; cmake .. -DMEOW_QT=1 -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=DEB; make -j6 package; mv *.deb ..' &

chroot /var/fedora-x86_64 su - $user -c 'cd ~/meow; mkdir -p build-chroot-fedora64; cd build-chroot-fedora64; cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=RPM; make -j6 package; mv *.rpm ..' &
chroot /var/fedora-x86_64 su - $user -c 'cd ~/meow; mkdir -p build-qt-chroot-fedora64; cd build-qt-chroot-fedora64; cmake .. -DMEOW_QT=1 -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=RPM; make -j6 package; mv *.rpm ..' &

chroot /var/fedora-i386 su - $user -c 'cd ~/meow; mkdir -p build-chroot-fedora32; cd build-chroot-fedora32; cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=RPM; make -j6 package; mv *.rpm ..' &
chroot /var/fedora-i386 su - $user -c 'cd ~/meow; mkdir -p build-qt-chroot-fedora32; cd build-qt-chroot-fedora32; cmake .. -DMEOW_QT=1 -DCMAKE_INSTALL_PREFIX=/usr -DMEOW_PACKAGE=RPM; make -j6 package; mv *.rpm ..' &



wait
wait
wait
wait
wait
wait

