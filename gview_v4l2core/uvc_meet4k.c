/*******************************************************************************#
#           uvc Meet4K support for OBSBOT Meet 4K                               #
#       for guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Sam Liddicott <sam@liddicott.com>                                   #
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
#include <inttypes.h>
#include <libavcodec/avcodec.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "gview.h"
#include "../config.h"

#include "uvc_meet4k.h"

extern int verbosity;

#define UVCX_MEET4K_SETTINGS_6 0x6

typedef struct _uvcx_obsbot_meet4k_configuration_t
{
  union {
    uint8_t bytes[60];
    struct {
      uint8_t effect;            // 0
	  uint8_t hdr;               // 1
      uint8_t dummy0;            // 2
      uint8_t face_ae;           // 3
      uint8_t camera_angle;      // 4
      uint8_t bg_mode;           // 5
      uint8_t blur_level;        // 6
      uint8_t button_mode;       // 7
      uint16_t dummy1;           // 8,9
      uint8_t  dummy_x;   // a
      uint16_t dummy2;           // b,c
      uint8_t noise_reduction; //set in position 0xa frame_model;       // d
      uint16_t dummy4;           // e,f
      uint8_t bg_color;          // 10
    };
  };
} __attribute__((__packed__)) uvcx_obsbot_meet4k_configuration_t;


void add_meet4k(v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);

    if (strstr(vd->cap.card, "OBSBOT Meet 4K")) {
        printf("%s: Init. %s (location: %s)\n", __FUNCTION__, vd->cap.card, vd->cap.bus_info);
        vd->meet4k_unit_id = 6;
    }
}

uint8_t get_uvc_meet4k_unit_id (v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);

    return vd->meet4k_unit_id;
}

void meet4kcore_dump6(uvcx_obsbot_meet4k_configuration_t *configuration)
{
	int i=0;
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++]);
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++]);
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++]);
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++]);
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++]);
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++]);
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++]);
	fprintf(stderr, "%04x %02x %02x %02x %02x\n", i, configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++], configuration->bytes[i++]);
}

/*
 * send 0x6 command
 * args:
 *   vd - pointer to video device data
 *   mode - camera mode
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( 0 -OK)
 */
int meet4kcore_cmd6(v4l2_dev_t *vd, uvcx_obsbot_meet4k_configuration_t *command)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->meet4k_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc meet4k (%i)\n", vd->meet4k_unit_id);
		return E_NO_STREAM_ERR;
	}

	int err = E_OK;

	if((err = v4l2core_query_xu_control(
		vd,
		vd->meet4k_unit_id,
		UVCX_MEET4K_SETTINGS_6,
		UVC_SET_CUR,
		command)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (Meet4k) SET_CUR error: %s\n", strerror(errno));
    } else {
		fprintf(stderr, "V4L2_CORE: (Meet4k) SET_CUR %02x %02x %02x %02x %02x %02x\n", command->bytes[0], command->bytes[1], command->bytes[2], command->bytes[3], command->bytes[4], command->bytes[5]);
	}

	return err;
}


/*
 * get the camera mode
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: returns meet4k camera mode
 */
int meet4kcore_get6(v4l2_dev_t *vd, uvcx_obsbot_meet4k_configuration_t *configuration)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->meet4k_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc meet4k (%i)\n", vd->meet4k_unit_id);
		return 0xff;
	}

	int err = E_OK;

	if(err = (v4l2core_query_xu_control(
		vd,
		vd->meet4k_unit_id,
		UVCX_MEET4K_SETTINGS_6,
		UVC_GET_CUR,
		configuration)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (Meet4k) query (%u) error: %s\n", UVC_GET_CUR, strerror(errno));
		return 0xff;
    } else {
		meet4kcore_dump6(configuration);
	}

	return err;
}

#define UVC_MEET4K_DEF UVC_MEET4K_B
#include "uvc_meet4k.ch"

