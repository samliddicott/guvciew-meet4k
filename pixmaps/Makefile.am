#copies backup files if regular data files not found

pixs = \
	guvcview.xpm\
	camera.xpm\
	movie.xpm\
	save.xpm\
	folder.xpm\
	close.xpm

clean:
	test -z "guvcview.desktop" || rm -f guvcview.desktop;\
	test -z "guvcview" || rm -f guvcview

distclean: clean

clean-all: clean

.menu:
	if test -e guvcview.menu; then\
		cp guvcview.menu guvcview;\
	else\
		echo "can't find menu file";\
	fi
.desktop:
	if test -e guvcview.desktop; then\
		echo "guvcview.desktop exists";\
	else\
		echo "creating guvcview.desktop";\
		rm -f guvcview.desktop_;\
		touch guvcview.desktop_;\
		echo "[Desktop Entry]" >> guvcview.desktop_;\
		echo "Name=guvcview" >> guvcview.desktop_;\
		echo "GenericName=GTK UVC video viewer" >> guvcview.desktop_;\
		echo "Comment=A video viewer and capturer for the linux uvc driver" >> guvcview.desktop_;\
		echo "TryExec=guvcview" >> guvcview.desktop_;\
		echo "Exec=guvcview" >> guvcview.desktop_;\
		echo "Icon=$(datadir)/pixmaps/guvcview/guvcview.xpm" >> guvcview.desktop_;\
		echo "Terminal=false" >> guvcview.desktop_;\
		echo "Type=Application" >> guvcview.desktop_;\
		echo "Categories=AudioVideo;" >> guvcview.desktop_;\
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

.PHONY: .menu .desktop .xpm
