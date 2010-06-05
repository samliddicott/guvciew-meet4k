#copies backup files if regular data files not found

pixs = \
	guvcview.png\
	camera.png\
	movie.png\
	save.png\
	defaults.png\
	image_controls.png\
	video_controls.png\
	audio_controls.png\
	images_folder.png\
	videos_folder.png\
	controls_folder.png\
	close.png

CONFIG_CLEAN_FILES = guvcview *.desktop* guvcview.1_ guvcview.1.gz guvcview.menu

clean-generic:
	-test -z "$(CONFIG_CLEAN_FILES)" || rm -f $(CONFIG_CLEAN_FILES)

.manpage:
	if test -e guvcview.1; then\
		echo "compressing man page";\
		cp guvcview.1 guvcview.1_;\
		gzip guvcview.1;\
		cp guvcview.1_ guvcview.1;\
	else\
		echo "could not find man page";\
	fi

if DEBIAN_MENU
.menu:
		if test -e guvcview; then\
			echo "guvcview debian menu file exists";\
		else\
			echo "Creating debian menu file";\
			rm -f guvcview.menu;\
			touch guvcview.menu;\
			echo "?package(guvcview):\\" >> guvcview.menu;\
			echo "	needs=\"X11\"\\" >> guvcview.menu;\
			echo "	section=\"Applications/Video\"\\" >> guvcview.menu;\
			echo "	title=\"guvcview\"\\" >> guvcview.menu;\
			echo "	longtitle=\"GTK UVC Viewer - uvc video viewer and capturer\"\\" >> guvcview.menu;\
			echo "	command=\"guvcview\"\\" >> guvcview.menu;\
			echo "	icon=\"$(datadir)/pixmaps/guvcview/guvcview.png\"" >> guvcview.menu;\
			cp guvcview.menu guvcview;\
		fi
endif

if DESKTOP
.desktop:
	if test -e guvcview.desktop; then\
		echo "guvcview.desktop exists";\
	else\
		echo "creating guvcview.desktop";\
		rm -f guvcview.desktop_;\
		touch guvcview.desktop_;\
		echo "[Desktop Entry]" >> guvcview.desktop_;\
		echo "Version=1.0" >> guvcview.desktop_;\
		echo "Name=guvcview" >> guvcview.desktop_;\
		echo "GenericName=GTK UVC video viewer" >> guvcview.desktop_;\
		echo "Comment=A video viewer and capturer for the linux uvc driver" >> guvcview.desktop_;\
		echo "TryExec=guvcview" >> guvcview.desktop_;\
		echo "Exec=guvcview" >> guvcview.desktop_;\
		echo "Icon=$(datadir)/pixmaps/guvcview/guvcview.png" >> guvcview.desktop_;\
		echo "Terminal=false" >> guvcview.desktop_;\
		echo "Type=Application" >> guvcview.desktop_;\
		echo "Categories=Video;AudioVideo;" >> guvcview.desktop_;\
		echo "StartupNotify=true" >> guvcview.desktop_;\
		cp guvcview.desktop_ guvcview.desktop;\
	fi
endif

.png:
	for file in ${pixs}; do \
		if test -e $$file; then \
			echo "$$file exists";\
		else\
			echo "ERROR: $$file not found!";\
		fi;\
	done

.PHONY: .menu .desktop .png .manpage
