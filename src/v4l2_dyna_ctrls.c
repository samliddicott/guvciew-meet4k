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
#include <glib/gstdio.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <errno.h>

#include "v4l2uvc.h"
#include "v4l2_controls.h"
#include "v4l2_dyna_ctrls.h"

/* some Logitech webcams have pan/tilt/focus controls */
#define LENGTH_OF_XU_CTR (6)
#define LENGTH_OF_XU_MAP (9)

/*
static struct uvc_xu_control_info xu_ctrls[] =
{
	{
		.entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector = XU_MOTORCONTROL_PANTILT_RELATIVE,
		.index    = 0,
		.size     = 4,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_DEF | UVC_CONTROL_AUTO_UPDATE
	},
	{
		.entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector = XU_MOTORCONTROL_PANTILT_RESET,
		.index    = 1,
		.size     = 1,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF | UVC_CONTROL_AUTO_UPDATE
	},
	{
		.entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector = XU_MOTORCONTROL_FOCUS,
		.index    = 2,
		.size     = 6,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX |UVC_CONTROL_GET_DEF | UVC_CONTROL_AUTO_UPDATE
	},
	{
		.entity   = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector = XU_COLOR_PROCESSING_DISABLE,
		.index    = 4,
		.size     = 1,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR |UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF | UVC_CONTROL_AUTO_UPDATE
	},
	{
		.entity   = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector = XU_RAW_DATA_BITS_PER_PIXEL,
		.index    = 7,
		.size     = 1,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR |UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF | UVC_CONTROL_AUTO_UPDATE
	},
	{
		.entity   = UVC_GUID_LOGITECH_USER_HW_CONTROL,
		.selector = XU_HW_CONTROL_LED1,
		.index    = 0,
		.size     = 3,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR |UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF | UVC_CONTROL_AUTO_UPDATE
	},

};
*/

static struct uvc_menu_info led_menu_entry[4] = {{0, "Off"},
												 {1, "On"},
												 {2, "Blinking"},
												 {3, "Auto"}};

/* mapping for Pan/Tilt/Focus */
static struct uvc_xu_control_mapping xu_mappings[] =
{
	{
		.id        = V4L2_CID_PAN_RELATIVE,
		.name      = N_("Pan (relative)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
		.size      = 16,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_SIGNED,
		.menu_info = NULL,
		.menu_count = 0,
		.reserved = {0,0,0,0}
	},
	{
		.id        = V4L2_CID_TILT_RELATIVE,
		.name      = N_("Tilt (relative)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
		.size      = 16,
		.offset    = 16,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_SIGNED,
		.menu_info = NULL,
		.menu_count = 0,
		.reserved = {0,0,0,0}
	},
	{
		.id        = V4L2_CID_PAN_RESET,
		.name      = N_("Pan Reset"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RESET,
		.size      = 1,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_BUTTON,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED,
		.menu_info = NULL,
		.menu_count = 0,
		.reserved = {0,0,0,0}
	},
	{
		.id        = V4L2_CID_TILT_RESET,
		.name      = N_("Tilt Reset"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RESET,
		.size      = 1,
		.offset    = 1,
		.v4l2_type = V4L2_CTRL_TYPE_BUTTON,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED,
		.menu_info = NULL,
		.menu_count = 0,
		.reserved = {0,0,0,0}
	},
	{
		.id        = V4L2_CID_FOCUS_LOGITECH,
		.name      = N_("Focus (absolute)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_FOCUS,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED,
		.menu_info = NULL,
		.menu_count = 0,
		.reserved = {0,0,0,0}
	},
	{
		.id        = V4L2_CID_LED1_MODE_LOGITECH,
		.name      = N_("LED1 Mode"),
		.entity    = UVC_GUID_LOGITECH_USER_HW_CONTROL,
		.selector  = XU_HW_CONTROL_LED1,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_MENU,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED,
		.menu_info = led_menu_entry,
		.menu_count = 4,
		.reserved = {0,0,0,0}
	},
	{
		.id        = V4L2_CID_LED1_FREQUENCY_LOGITECH,
		.name      = N_("LED1 Frequency"),
		.entity    = UVC_GUID_LOGITECH_USER_HW_CONTROL,
		.selector  = XU_HW_CONTROL_LED1,
		.size      = 8,
		.offset    = 16,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED,
		.menu_info = NULL,
		.menu_count = 0,
		.reserved = {0,0,0,0}
	},
	{
		.id        = V4L2_CID_DISABLE_PROCESSING_LOGITECH,
		.name      = N_("Disable video processing"),
		.entity    = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector  = XU_COLOR_PROCESSING_DISABLE,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_BOOLEAN,
		.data_type = UVC_CTRL_DATA_TYPE_BOOLEAN,
		.menu_info = NULL,
		.menu_count = 0,
		.reserved = {0,0,0,0}
	},
	{
		.id        = V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH,
		.name      = N_("Raw bits per pixel"),
		.entity    = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector  = XU_RAW_DATA_BITS_PER_PIXEL,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED,
		.menu_info = NULL,
		.menu_count = 0,
		.reserved = {0,0,0,0}
	},

};

int initDynCtrls(int hdevice)
{
	int i=0;
	int err=0;
	/* after adding the controls, add the mapping now */
	for ( i=0; i<LENGTH_OF_XU_MAP; i++ )
	{
		g_print("mapping control for %s\n", xu_mappings[i].name);
		if ((err=xioctl(hdevice, UVCIOC_CTRL_MAP, &xu_mappings[i])) < 0)
		{
			if ((errno!=EEXIST) || (errno != EACCES))
			{
				perror("UVCIOC_CTRL_MAP - Error");
				//return (-2);
			}
			else if (errno == EACCES)
			{
				g_printerr("need admin previledges for adding extension controls\n");
				g_printerr("please run 'guvcview --add_ctrls' as root (or with sudo)\n");
				return  (-1);
			}
			else perror("Mapping exists");
		}
	}
	return err;
}

uint16_t get_length_xu_control(int hdevice, uint8_t unit, uint8_t selector)
{
	uint16_t length = 0;

	struct uvc_xu_control_query xu_ctrl_query =
	{
		.unit     = unit,
		.selector = selector,
		.query    = UVC_GET_LEN,
		.size     = sizeof(length),
		.data     = (uint8_t *) &length
	};

	if (xioctl(hdevice, UVCIOC_CTRL_QUERY, &xu_ctrl_query) < 0)
	{
		perror("UVCIOC_CTRL_QUERY (GET_LEN) - Error");
		return 0;
	}

	return length;
}

uint8_t get_info_xu_control(int hdevice, uint8_t unit, uint8_t selector)
{
	uint8_t info = 0;

	struct uvc_xu_control_query xu_ctrl_query =
	{
		.unit     = unit,
		.selector = selector,
		.query    = UVC_GET_INFO,
		.size     = sizeof(info),
		.data     = &info
	};

	if (xioctl(hdevice, UVCIOC_CTRL_QUERY, &xu_ctrl_query) < 0)
	{
		perror("UVCIOC_CTRL_QUERY (GET_INFO) - Error");
		return 0;
	}

	return info;
}

int query_xu_control(int hdevice, uint8_t unit, uint8_t selector, uint8_t query, void *data)
{
	int err = 0;
	uint16_t len = get_length_xu_control(hdevice, unit, selector);

	struct uvc_xu_control_query xu_ctrl_query =
	{
		.unit     = unit,
		.selector = selector,
		.query    = query,
		.size     = len,
		.data     = (uint8_t *) data
	};

	//get query data
	if ((err=xioctl(hdevice, UVCIOC_CTRL_QUERY, &xu_ctrl_query)) < 0)
	{
		perror("UVCIOC_CTRL_QUERY - Error");
	}

	return err;
}
