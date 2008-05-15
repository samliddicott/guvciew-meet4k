#

EXTRA_DIST = guvcview.png camera.xpm movie.xpm guvcview.desktop

pixmapsdir = $(datadir)/pixmaps/guvcview

pixmaps_DATA = \
		guvcview.png\
		camera.xpm\
		movie.xpm

applicationdir = ${datadir}/applications

application_DATA = guvcview.desktop

guvcview.desktop:
	if test -e guvcview.desktop; then \
		echo "guvcview.desktop exists";\
	else\
		-cp guvcview.desktop_  guvcview.desktop;\
	fi


guvcview.png: 
	if test -e guvcview.png; then \
		echo "guvcview.png exists";\
	else\
		-cp guvcview.png_  guvcview.png;\
	fi

camera.xpm: 
	if test -e camera.xpm; then \
		echo "camera.xpm exists";\
	else\
		-cp camera.xpm_  camera.xpm;\
	fi

movie.xpm: 
	if test -e movie.xpm; then \
		echo "movie.xpm exists";\
	else\
		-cp movie.xpm_  movie.xpm;\
	fi

.PHONY: guvcview.desktop guvcview.png camera.xpm movie.xpm
