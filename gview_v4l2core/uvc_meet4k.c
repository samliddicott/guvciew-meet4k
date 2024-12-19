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
#include "v4l2_devices.h"

#include "gview.h"
#include "../config.h"

#include "uvc_meet4k.h"

// a29e7641-de04-47e3-8b2b-f4341aff003b (typically ext unit 6, unit id 6)for most OBSBOT extra controls
#define GUID_UVCX_OBSBOT6_XU   {0x41, 0x76, 0x9E, 0xA2, 0x04, 0xDE, 0xE3, 0x47, 0x8B, 0x2B, 0xF4, 0x34, 0x1A, 0xFF, 0x00, 0x3B}

// 9a1e7291-6843-4683-6d92-39bc7906ee49 (typically ext unit 6, unit id 2)for background bitmap uploading/downloading
#define GUID_UVCX_OBSBOT6_2_XU {0x91, 0x72, 0x1E, 0x9A, 0x43, 0x68, 0x83, 0x46, 0x6D, 0x92, 0x39, 0xBC, 0x79, 0x06, 0xEE, 0x49}

extern int verbosity;

#define UVCX_MEET4K_SETTINGS_6 0x6

typedef struct _uvcx_obsbot_meet4k_configuration_t
{
  union {
    uint8_t bytes[60];
    struct {
      uint8_t effect;            // 0
	  uint8_t hdr;               // 1
      uint8_t dummy3;            // 2
      uint8_t face_ae;           // 3
      uint8_t camera_angle;      // 4
      uint8_t bg_mode;           // 5
      uint8_t blur_level;        // 6
      uint8_t dumm7;             // 7
      uint16_t dummy8;           // 8,9
      uint8_t button_mode;       // set in position 7 // a
      uint16_t dummy11;          // b,c
      uint8_t dummy13;           // d
      uint8_t noise_reduction;   //set in position 0xa frame_model;       // e
      uint8_t dummy15;           // f
      uint8_t dummy16;           // 10
      uint8_t dummy17;           // 11
      uint8_t dummy18;           // 12
      uint8_t dummy19;           // 13
      uint8_t dummy20;           // 14
      uint8_t dummy21;           // 15
      uint8_t bg_color;          // set in position 0x10 // 0x16
    } __attribute__((__packed__));
  };
} __attribute__((__packed__)) uvcx_obsbot_meet4k_configuration_t;

int check_meet4k(v4l2_dev_t *vd)
{
	v4l2_device_list_t *my_device_list = get_device_list();

	if(my_device_list->list_devices[vd->this_device].vendor != 0x6e30)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: OBSBOT Skipping vendor (vendor_id=0x%4x): skiping peripheral V3 unit id check\n",
				my_device_list->list_devices[vd->this_device].vendor);
		return 0;
	}

    if (! strstr(vd->cap.card, "OBSBOT Meet 4K")) {
		if(verbosity > 0)
			printf("V4L2_CORE: OBSBOT Skipping card (card=%s: skiping peripheral V3 unit id check\n",
				vd->cap.card);
		return 0;
    }

	printf("%s: Init. %s (location: %s)\n", __FUNCTION__, vd->cap.card, vd->cap.bus_info);
	return 1;
}

void add_meet4k(v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);
	assert(vd->list_stream_formats != NULL);

	if(verbosity > 0)
		printf("V4L2_CORE: checking Meet4K support\n");

	if (! check_meet4k(vd)) {
		if(verbosity > 0)
			printf("V4L2_CORE: Not Meet4K\n");
	}

	int id;
	if((id = get_uvc_meet4k_unit_id(vd)) <= 0)
	{
		return;
	}
    vd->meet4k_unit_id = id;
	if((id = get_uvc_meet4k_unit_id2(vd)) <= 0)
	{
		return;
	}
    vd->meet4k_unit_id2 = id;
}

uint8_t get_uvc_meet4k_unit_id (v4l2_dev_t *vd)
{
	if(verbosity > 1)
		printf("V4L2_CORE: checking for OBSBOT MEET4K unit id\n");

	/*asserts*/
	assert(vd != NULL);

	uint8_t guid[16] = GUID_UVCX_OBSBOT6_XU;
	vd->meet4k_unit_id = get_guid_unit_id (vd, guid);

	if(verbosity > 0)
		printf("V4L2_CORE: Got Meet4K unit id %d\n", vd->meet4k_unit_id);

    return vd->meet4k_unit_id;
}

uint8_t get_uvc_meet4k_unit_id2 (v4l2_dev_t *vd)
{
	if(verbosity > 1)
		printf("V4L2_CORE: checking for OBSBOT MEET4K unit id\n");

	/*asserts*/
	assert(vd != NULL);

	uint8_t guid[16] = GUID_UVCX_OBSBOT6_2_XU;
	vd->meet4k_unit_id2 = get_guid_unit_id (vd, guid);

	if(verbosity > 0)
		printf("====== V4L2_CORE: Got Meet4K 2 unit id %d\n", vd->meet4k_unit_id2);

    return vd->meet4k_unit_id2;
}


void meet4kcore_dump6(uvcx_obsbot_meet4k_configuration_t *configuration)
{
	int i=0;
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i], configuration->bytes[i+1], configuration->bytes[i+2], configuration->bytes[i+3], configuration->bytes[i+4], configuration->bytes[i+5], configuration->bytes[i+6], configuration->bytes[i+7]); i+=8;
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i], configuration->bytes[i+1], configuration->bytes[i+2], configuration->bytes[i+3], configuration->bytes[i+4], configuration->bytes[i+5], configuration->bytes[i+6], configuration->bytes[i+7]); i+=8;
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i], configuration->bytes[i+1], configuration->bytes[i+2], configuration->bytes[i+3], configuration->bytes[i+4], configuration->bytes[i+5], configuration->bytes[i+6], configuration->bytes[i+7]); i+=8;
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i], configuration->bytes[i+1], configuration->bytes[i+2], configuration->bytes[i+3], configuration->bytes[i+4], configuration->bytes[i+5], configuration->bytes[i+6], configuration->bytes[i+7]); i+=8;
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i], configuration->bytes[i+1], configuration->bytes[i+2], configuration->bytes[i+3], configuration->bytes[i+4], configuration->bytes[i+5], configuration->bytes[i+6], configuration->bytes[i+7]); i+=8;
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i], configuration->bytes[i+1], configuration->bytes[i+2], configuration->bytes[i+3], configuration->bytes[i+4], configuration->bytes[i+5], configuration->bytes[i+6], configuration->bytes[i+7]); i+=8;
	fprintf(stderr, "%04x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, configuration->bytes[i], configuration->bytes[i+1], configuration->bytes[i+2], configuration->bytes[i+3], configuration->bytes[i+4], configuration->bytes[i+5], configuration->bytes[i+6], configuration->bytes[i+7]); i+=8;
	fprintf(stderr, "%04x %02x %02x %02x %02x\n", i, configuration->bytes[i], configuration->bytes[i+1], configuration->bytes[i+2], configuration->bytes[i+3]);
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

