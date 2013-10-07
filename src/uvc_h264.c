/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
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

/* support for internationalization - i18n */
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "uvc_h264.h"
#include "v4l2_dyna_ctrls.c"
#include "globals.h"
#include "string_utils.h"
#include "callbacks.h"

int has_h264_support(int hdevice)
{
	uvcx_version_t uvcx_version;
	uint8_t unit = 12; //FIXME: hardcoded should get unit from GUID

	if(read_xu_control(hdevice, unit, UVCX_VERSION, sizeof(uvcx_version), &uvcx_version) < 0)
	{
		g_printerr("device doesn't seem to support uvc H264\n");
		return 0;
	}
	else
	{
		g_printerr("device seems to support uvc H264 (version: %d)\n", uvcx_version.wVersion);
		return 1;
	}
}


