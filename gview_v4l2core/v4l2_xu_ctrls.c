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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "gview.h"
#include "v4l2_xu_ctrls.h"

extern int verbosity;

/*
 * XU controls
 */
#define V4L2_CID_BASE_EXTCTR				0x0A046D01
#define V4L2_CID_BASE_LOGITECH				V4L2_CID_BASE_EXTCTR
#define V4L2_CID_PANTILT_RESET_LOGITECH		V4L2_CID_BASE_LOGITECH+2

/*this should realy be replaced by V4L2_CID_FOCUS_ABSOLUTE in libwebcam*/
#define V4L2_CID_LED1_MODE_LOGITECH				V4L2_CID_BASE_LOGITECH+4
#define V4L2_CID_LED1_FREQUENCY_LOGITECH		V4L2_CID_BASE_LOGITECH+5
#define V4L2_CID_DISABLE_PROCESSING_LOGITECH	V4L2_CID_BASE_LOGITECH+0x70
#define V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH	V4L2_CID_BASE_LOGITECH+0x71
#define V4L2_CID_LAST_EXTCTR					V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH

#define UVC_GUID_LOGITECH_VIDEO_PIPE		{0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x50}
#define UVC_GUID_LOGITECH_MOTOR_CONTROL		{0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x56}
#define UVC_GUID_LOGITECH_USER_HW_CONTROL	{0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x1f}

#define XU_HW_CONTROL_LED1					1
#define XU_MOTORCONTROL_PANTILT_RELATIVE	1
#define XU_MOTORCONTROL_PANTILT_RESET		2
#define XU_MOTORCONTROL_FOCUS				3
#define XU_COLOR_PROCESSING_DISABLE			5
#define XU_RAW_DATA_BITS_PER_PIXEL			8

/* some Logitech webcams have pan/tilt/focus controls */
#define LENGTH_OF_XU_MAP (9)

static struct uvc_menu_info led_menu_entry[4] = {{0, N_("Off")},
												 {1, N_("On")},
												 {2, N_("Blinking")},
												 {3, N_("Auto")}};

/* known xu control mappings */
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
		.id        = V4L2_CID_FOCUS_ABSOLUTE,
		.name      = N_("Focus"),
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

/*
 * tries to map available xu controls for supported devices
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *
 * returns: 0 if enumeration succeded or errno otherwise
 */
int init_xu_ctrls(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->fd > 0);

	int i=0;
	int err=0;
	/* after adding the controls, add the mapping now */
	for ( i=0; i<LENGTH_OF_XU_MAP; i++ )
	{
		if(verbosity > 0)
			printf("V4L2_CORE: mapping control for %s\n", xu_mappings[i].name);
		if ((err=xioctl(vd->fd, UVCIOC_CTRL_MAP, &xu_mappings[i])) < 0)
		{
			if ((errno!=EEXIST) || (errno != EACCES))
			{
				fprintf(stderr, "V4L2_CORE: (UVCIOC_CTRL_MAP) Error: %s\n", strerror(errno));
			}
			else if (errno == EACCES)
			{
				fprintf(stderr, "V4L2_CORE: need admin previledges for adding extension controls\n");
				fprintf(stderr, "V4L2_CORE: please run 'guvcview --add_ctrls' as root (or with sudo)\n");
				return  (-1);
			}
			else fprintf(stderr, "V4L2_CORE: Mapping exists: %s\n", strerror(errno));
		}
	}
	return err;
}

/*
 * get lenght of xu control defined by unit id and selector
 * args:
 *   vd - pointer to video device data
 *   unit - unit id of xu control
 *   selector - selector for control
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *
 * returns: length of xu control
 */
uint16_t v4l2core_get_length_xu_control(v4l2_dev_t *vd, uint8_t unit, uint8_t selector)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->fd > 0);

	uint16_t length = 0;

	struct uvc_xu_control_query xu_ctrl_query =
	{
		.unit     = unit,
		.selector = selector,
		.query    = UVC_GET_LEN,
		.size     = sizeof(length),
		.data     = (uint8_t *) &length
	};

	if (xioctl(vd->fd, UVCIOC_CTRL_QUERY, &xu_ctrl_query) < 0)
	{
		fprintf(stderr, "V4L2_CORE: UVCIOC_CTRL_QUERY (GET_LEN) - Error: %s\n", strerror(errno));
		return 0;
	}

	return length;
}

/*
 * get uvc info for xu control defined by unit id and selector
 * args:
 *   vd - pointer to video device data
 *   unit - unit id of xu control
 *   selector - selector for control
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *
 * returns: info of xu control
 */
uint8_t v4l2core_get_info_xu_control(v4l2_dev_t *vd, uint8_t unit, uint8_t selector)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->fd > 0);

	uint8_t info = 0;

	struct uvc_xu_control_query xu_ctrl_query =
	{
		.unit     = unit,
		.selector = selector,
		.query    = UVC_GET_INFO,
		.size     = sizeof(info),
		.data     = &info
	};

	if (xioctl(vd->fd, UVCIOC_CTRL_QUERY, &xu_ctrl_query) < 0)
	{
		fprintf(stderr, "V4L2_CORE: UVCIOC_CTRL_QUERY (GET_INFO) - Error: %s\n", strerror(errno));
		return 0;
	}

	return info;
}

/*
 * runs a query on xu control defined by unit id and selector
 * args:
 *   vd - pointer to video device data
 *   unit - unit id of xu control
 *   selector - selector for control
 *   query - query type
 *   data - pointer to query data
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *
 * returns: 0 if query succeded or errno otherwise
 */
int v4l2core_query_xu_control(v4l2_dev_t *vd, uint8_t unit, uint8_t selector, uint8_t query, void *data)
{
	int err = 0;
	uint16_t len = v4l2core_get_length_xu_control(vd, unit, selector);

	struct uvc_xu_control_query xu_ctrl_query =
	{
		.unit     = unit,
		.selector = selector,
		.query    = query,
		.size     = len,
		.data     = (uint8_t *) data
	};

	/*get query data*/
	if ((err=xioctl(vd->fd, UVCIOC_CTRL_QUERY, &xu_ctrl_query)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: UVCIOC_CTRL_QUERY (%i) - Error: %s\n", query, strerror(errno));
	}

	return err;
}
