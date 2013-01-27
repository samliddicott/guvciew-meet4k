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

#include <glib.h>
#include <glib/gprintf.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>

#include "defs.h"
#include "image_format.h"


static iformats_data listSupIFormats[] = //list of supported image formats
{
	{
		.name         = "Jpeg",
		.description  = N_("Jpeg (jpg)"),
		.extension    = "jpg",
		.pattern      ="*.jpg",
	},
	{
		.name         = "Bmp",
		.description  = N_("Bitmap (Bmp)"),
		.extension    = "bmp",
		.pattern      = "*.bmp",
	},
	{
		.name         = "Png",
		.description  = N_("Portable Network Graphics (Png)"),
		.extension    = "png",
		.pattern      = "*.png",
	},
	{
		.name         = "Raw",
		.description  = N_("Raw Image (raw)"),
		.extension    = "raw",
		.pattern      = "*.raw",
	}
};

const char *get_iformat_extension(int ind)
{
	return (listSupIFormats[ind].extension);
}

const char *get_iformat_pattern(int ind)
{
	return (listSupIFormats[ind].pattern);
}

const char *get_iformat_desc(int ind)
{
	return (listSupIFormats[ind].description);
}
