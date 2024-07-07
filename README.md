GTK UVC VIEWER (guvcview)
*************************

Basic Configuration
===================
Dependencies:
-------------

Guvcview depends on the following:
 - intltool,
 - cmake, 
 - libsdl2 and/or sfml, 
 - libgtk-3 and/or libqt6, 
 - portaudio19, 
 - libpng, 
 - libavcodec, 
 - libavutil, 
 - libv4l, 
 - libudev,
 - libusb-1.0,
 - libpulse (optional)
 - libgsl0 (optional)

On most distributions you can just install the development 
packages:
 intltool, autotools-dev, libsdl2-dev, libsfml-dev, 
 libgtk-3-dev or qtbase5-dev, portaudio19-dev, libpng12-dev,
 libavcodec-dev, libavutil-dev, libv4l-dev, libudev-dev, 
 libusb-1.0-0-dev, libpulse-dev, libgsl0-dev

Build configuration:
--------------------
guvcview uses cmake since version 2.2.1, basic usage:

mkdir build 
cd build

(for Gtk3 ui) cmake --install-prefix=/usr -DUSE_SFML=ON ..
(for Qt6 ui)  cmake --install-prefix=/usr -DUSE_GTK3=OFF -DUSE_QT6=ON ..

After configuration binaries can be build with 'cmake --build .'

To install guvcview and all the associated data files:  
 'cmake --build . --target install'  (this may require root or sudo)

guvcview will build with Gtk3 support by default, if you want to use the 
Qt6 interface instead, disable Gtk3 with -DUSE_GTK3=OFF and enable Qt6 
with -DUSE_QT6=ON
Guvcview can be build with both Gtk3 and Qt6 support enabled, you can then
change the ui interface from the command line: 'guvcview --gui=gtk3' or 
'guvcview --gui=qt6'
For the rendering engine you can use SDL2 (enabled by default) and/or 
SFML (disabled by default), both engines can be enabled during configuration,
you can then choose between the two with a command line option '--render=sdl'
or '--render=sfml'.
 

Data Files:
------------
(language files; image files; gnome menu entry)

guvcview data files are stored by default to /usr/local/share
setting a different prefix (--install-prefix=BASEDIR) during configuration
will change the installation path to BASEDIR/share.

Built files, src/guvcview and data/gnome.desktop, are dependent 
on this path, so if a new prefix is set a make clean is required 
before issuing the make command. 

After running the configure script the normal, make && make install 
should build and install all the necessary files.    
    
 
guvcview bin:
-------------
(guvcview)

The binarie file installs to the standart location,
/usr/local/bin, to change the install path, configure
must be executed with --install-prefix=DIR set, this will cause
the bin file to be installed in DIR/bin, make sure 
DIR/bin is set in your PATH variable, or the gnome 
menu entry will fail.

guvcview libraries:
-------------------
(libgviewv4l2core, libgviewrender, libgviewaudio, libgviewencoder)

The core functionality of guvcview is now split into 4 libraries
these will install to ${prefix}/lib and development headers to
${prefix}/include/guvcview-2/libname if -DINSTALL_DEVKIT=ON is used. 
pkg-config should be use to determine the compile flags.


guvcview.desktop:
-----------------

(data/guvcview.desktop)

The desktop file (gnome menu entry) is built from the
data/guvcview.desktop.in definition and is dependent on the 
--install-prefix setting, any changes to this, must 
be done in data/guvcview.desktop.in.

configuration files:
--------------------
(~/.config/guvcview2/video0)

The configuration file is saved into the $HOME dir when 
exiting guvcview. If a video device with index > 0,
e.g: /dev/video1 is used then the file stored will be
named ~/.config/guvcview2/video1

Executing guvcview
================== 

For instructions on the command line args 
execute "guvcview --help" or "man guvcview".
