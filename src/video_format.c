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

#include <glib.h>
#include <glib/gprintf.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <linux/videodev2.h>

#include "guvcview.h"
#include "colorspaces.h"
#include "lavc_common.h"
#include "video_format.h"
#include "defs.h"


static vformats_data listSupVFormats[] = //list of software supported formats
{
	{
		.avformat     = FALSE,
		.name         = "AVI",
		.description  = N_("AVI - avi format"),
		.extension    = "avi",
		.format_str   = "avi",
		.flags        = 0
	}
};

const char *get_vformat_extension(int codec_ind)
{
	return (listSupVFormats[codec_ind].extension);
}

/*
static const char *get_vformat_str(int codec_ind)
{
	return (listSupVFormats[codec_ind].format_str);
}
*/

const char *get_vformat_desc(int codec_ind)
{
	return (listSupVFormats[codec_ind].description);
}

gboolean isLavfFormat(int codec_ind)
{
	return (listSupVFormats[codec_ind].avformat);
}

char *setVidExt(char *filename, int format_ind)
{
	int sname = strlen(filename)+1; /*include '\0' terminator*/
	char basename[sname];
	sscanf(filename,"%[^.]",basename);
	
	g_snprintf(filename, sname, "%s.%s", basename, get_vformat_extension(format_ind));
	
	return (filename);
}

int write_video_packet (BYTE *picture_buf, int size, struct VideoFormatData* data)
{
	return (1);
}

int write_audio_packet (BYTE *audio_buf, int size, struct VideoFormatData* data)
{
	return (1);

}

int clean_FormatContext (void* arg)
{
	return (0);
}

int init_FormatContext(void *data)
{
	return(0);
}
