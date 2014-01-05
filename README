GTK UVC VIEWER (guvcview)
*************************

Basic Configuration
===================
*Dependencies:
--------------

Guvcview depends on the following:
 - intltool,
 - autotools, 
 - libsdl, 
 - libgtk-3, 
 - portaudio19, 
 - libpng, 
 - libavcodec, 
 - libavutil, 
 - libv4l, 
 - libudev,
 - libusb-1.0,
 - libpulse (optional)

On most distributions you can just install the development 
packages:
 intltool, autotools-dev, libsdl1.2-dev, libgtk-3-dev, 
 portaudio19-dev, libpng12-dev, libavcodec-dev, libavutil-dev,
 libv4l-dev, libudev-dev, libusb-1.0-0-dev, libpulse-dev

*Build configuration:
---------------------
(./bootstrap.sh; ./configure)

The configure script is generated from configure.ac by autoconf,
the helper script ./bootstrap.sh can be used for this, it will also
run the generated configure with the command line options passed.
After configuration a simple 'make && make install' will build and
install guvcview and all the associated data files.

*Data Files:
------------
(language files; image files; gnome menu entry)

guvcview data files are stored by default to /usr/local/share
setting a different prefix (--prefix=BASEDIR) during configuration
will change the installation path to BASEDIR/share.

Built files, src/guvcview and data/gnome.desktop, are dependent 
on this path, so if a new prefix is set a make clean is required 
before issuing the make command. 

After running the configure script the normal, make && make install 
should build and install all the necessary files.    
    
 
*guvcview bin:
--------------
(src/guvcview)

The binarie file installs to the standart location,
/usr/local/bin, to change the install path, configure
must be executed with --prefix=DIR set, this will cause
the bin file to be installed in DIR/bin, make sure 
DIR/bin is set in your PATH variable, or the gnome 
menu entry will fail.


*guvcview.desktop:
------------------

(data/guvcview.desktop)

The desktop file (gnome menu entry) is built from the
data/guvcview.desktop.in definition and is dependent on the 
configure --prefix setting, any changes to this, must 
be done in data/guvcview.desktop.in.

*configuration files:
---------------------
(~/.config/guvcview/video0)

The configuration file is saved into the $HOME dir when 
exiting guvcview. If a video device with index > 0,
e.g: /dev/video1 is used then the file stored will be
named ~/.config/guvcview/video1

Executing guvcview
================== 

For instructions on the command line args 
execute "guvcview --help".
