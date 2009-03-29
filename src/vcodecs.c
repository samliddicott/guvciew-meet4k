/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
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

#include "vcodecs.h"
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <linux/videodev2.h>

static vcodecs_data listSupVCodecs[] = //list of software supported formats
{
	{
		.avcodec     = FALSE,
		.compressor  = "MJPG",
		.description = N_("MJPG - compressed")
	},
	{
		.avcodec     = FALSE,
		.compressor  = "YUY2",
		.description = N_("YUY2 - uncomp YUV")
	},
	{
		.avcodec     = FALSE,
		.compressor  = "DIB ",
		.description = N_("RGB - uncomp BMP")
	},
	{
		.avcodec     = TRUE,
		.compressor  = "MPEG",
		.description = N_("MPEG video 1")
	},
	{
		.avcodec     = TRUE,
		.compressor  = "FLV1",
		.description = N_("FLV1 - flash video 1")
	},
	{
		.avcodec     = TRUE,
		.compressor  = "WMV1",
		.description = N_("WMV1 - win. med. video 7")
	}
};

const char *get_vid4cc(int codec_ind)
{
	return (listSupVCodecs[codec_ind].compressor);
}

const char *get_desc4cc(int codec_ind)
{
	return (listSupVCodecs[codec_ind].description);
}
