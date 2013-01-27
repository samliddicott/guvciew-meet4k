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
#include <linux/videodev2.h>

#include "guvcview.h"
#include "ms_time.h"
#include "colorspaces.h"
#include "lavc_common.h"
#include "video_format.h"
#include "vcodecs.h"
#include "acodecs.h"
#include "defs.h"


static vformats_data listSupVFormats[] = //list of software supported formats
{
	{
		.avformat     = FALSE,
		.name         = "AVI",
		.description  = N_("AVI - avi format"),
		.extension    = "avi",
		.format_str   = "avi",
		.pattern      ="*.avi",
		.flags        = 0
	},
	{
		.avformat     = FALSE,
		.name         = "MATROSKA",
		.description  = N_("MKV - Matroska format"),
		.extension    = "mkv",
		.format_str   = "mkv",
		.pattern      = "*.mkv",
		.flags        = 0
	},
	{
		.avformat     = FALSE,
		.name         = "WEBM",
		.description  = N_("WEBM - format"),
		.extension    = "webm",
		.format_str   = "webm",
		.pattern      = "*.webm",
		.flags        = 0
	}
};

const char *get_vformat_extension(int codec_ind)
{
	return (listSupVFormats[codec_ind].extension);
}

const char *get_vformat_pattern(int codec_ind)
{
	return (listSupVFormats[codec_ind].pattern);
}

const char *get_vformat_desc(int codec_ind)
{
	return (listSupVFormats[codec_ind].description);
}

gboolean isLavfFormat(int codec_ind)
{
	return (listSupVFormats[codec_ind].avformat);
}
