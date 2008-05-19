#copies backup files if regular data files not found

pixs = \
	guvcview.xpm\
	camera.xpm\
	movie.xpm\
	save.xpm\
	folder.xpm\
	close.xpm

clean:
	test -z "guvcview.desktop" || rm -f guvcview.desktop

distclean: clean

clean-all: clean

.desktop:
	if test -e guvcview.desktop; then\
		echo "guvcview.desktop exists";\
	else\
		echo "creating guvcview.desktop";\
		rm -f guvcview.desktop_;\
		touch guvcview.desktop_;\
		echo "[Desktop Entry]" >> guvcview.desktop_;\
		echo "Version=1.0" >> guvcview.desktop_;\
		echo "Encoding=UTF-8" >> guvcview.desktop_;\
		echo "Name=guvcview" >> guvcview.desktop_;\
		echo "GenericName=GTK UVC video viewer" >> guvcview.desktop_;\
		echo "Comment=A video viewer and capturer for the linux uvc driver" >> guvcview.desktop_;\
		echo "TryExec=guvcview" >> guvcview.desktop_;\
		echo "Exec=guvcview" >> guvcview.desktop_;\
		echo "Icon=${DATA_DIR}/pixmaps/guvcview/guvcview.xpm" >> guvcview.desktop_;\
		echo "Terminal=false" >> guvcview.desktop_;\
		echo "Type=Application" >> guvcview.desktop_;\
		echo "Categories=Application;AudioVideo;" >> guvcview.desktop_;\
		cp guvcview.desktop_ guvcview.desktop;\
	fi
		 
.xpm:
	for file in ${pixs}; do \
		if test -e $$file; then \
			echo "$$file exists";\
		else\
			echo "ERROR: $$file not found!";\
		fi;\
	done

.PHONY: .desktop .xpm
