/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#                                                                               #
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of the GNU General Public License as published by          #
# the Free Software Foundation; either version 2 of the License, or             #
# (at your option) any later version.                                           #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License             #
# along with this program; if not, write to the Free Software                   #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     #
#                                                                               #
********************************************************************************/

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include "defs.h"

int
int_num_chars (int n);

int
uint64_num_chars (uint64_t n);

/* check image file extension and return image type*/
int 
check_image_type (char *filename);

/* check video file extension and return video format*/
int 
check_video_type (char *filename);

/* split fullpath in Path (splited[1]) and filename (splited[0])*/
pchar* 
splitPath(char *FullPath, char* splited[2]);

/*join splited path into fullpath*/
char *
joinPath(char *fullPath, pchar * splited);

/*increment file name with inc*/
char *
incFilename(char *fullPath, pchar *splited, uint64_t inc);

char *
setImgExt(char *filename, int format);

char *
setVidExt(char *filename, int format_ind);

char *
add_file_suffix(char *filename, uint64_t suffix);

uint64_t 
get_file_suffix(const char *path, const char* filename);

#endif
