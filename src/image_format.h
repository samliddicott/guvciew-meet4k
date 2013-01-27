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

#ifndef IMAGE_FORMAT_H
#define IMAGE_FORMAT_H

#include "../config.h"
#include "defs.h"

#define JPG_FORMAT   0
#define BMP_FORMAT   1
#define PNG_FORMAT   2
#define RAW_FORMAT   3

#define MAX_IFORMATS 4

typedef struct _iformats_data
{
	const char *name;         //format name
	const char *description;  //format description
	const char *extension;    //format extension
	const char *pattern;      //file filter pattern
} iformats_data;

const char *get_iformat_extension(int ind);

const char *get_iformat_desc(int ind);

const char *get_iformat_pattern(int ind);

#endif
