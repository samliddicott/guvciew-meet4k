#!/bin/sh

mkdir ./debian/guvcview/usr/share/pixmaps
mkdir ./debian/guvcview/usr/share/applications

cp ../pixmaps/guvcview.png ./debian/guvcview/usr/share/pixmaps/.
cp ../pixmaps/camera.xpm   ./debian/guvcview/usr/share/pixmaps/.
cp ../pixmaps/movie.xpm    ./debian/guvcview/usr/share/pixmaps/.
cp ./debian/guvcview.desktop ./debian/guvcview/usr/share/applications/.

chmod -R 755 ./debian/guvcview

#fakeroot dpkg-deb --build guvcview_0.5.1-0_i386
