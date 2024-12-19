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

#include "uvc_meet4k.chp"

#ifndef UVC_MEET4K_CH
#define UVC_MEET4K_CH

#define meet4kcore_get_set_1(...) MEET4K_func(meet4kcore_get_set_1, __VA_ARGS__)

/* declare functions */
#define UVC_MEET4K_DEF_meet4kcore_get_set_1(name, read, ...) \
uint8_t meet4kcore_get_##name(v4l2_dev_t *vd); \
int meet4kcore_set_##name(v4l2_dev_t *vd, uint8_t mode);

/* define functions */
#define UVC_MEET4K_B_meet4kcore_get_set_1(name, read, ...) \
uint8_t meet4kcore_get_##name(v4l2_dev_t *vd) \
{ \
	uvcx_obsbot_meet4k_configuration_t configuration; \
	if (E_OK != meet4kcore_get6(vd, &configuration)) { \
		return 0xff; \
	} else { \
		fprintf(stderr, "V4L2_MEET4K: get " #name ": %d\n", configuration.read); \
	} \
	return configuration.read; \
} \
int meet4kcore_set_##name(v4l2_dev_t *vd, uint8_t mode) \
{ \
	uvcx_obsbot_meet4k_configuration_t command = { __VA_ARGS__, mode }; \
	fprintf(stderr, "V4L2_MEET4K: set " #name ": %d\n", mode); \
	return meet4kcore_cmd6(vd, &command); \
}

#endif

meet4kcore_get_set_1(background_mode, effect, 0x0, 1)
meet4kcore_get_set_1(hdr_mode, hdr, 0x1, 1)
meet4kcore_get_set_1(face_ae_mode, face_ae, 0x3, 1)
meet4kcore_get_set_1(nr_mode, noise_reduction, 0xa, 1)

meet4kcore_get_set_1(camera_angle, camera_angle, 0x4, 1)
meet4kcore_get_set_1(bg_mode, bg_mode, 0x5, 1)
meet4kcore_get_set_1(blur_level, blur_level, 0x6, 1)
meet4kcore_get_set_1(button_mode, button_mode, 0x7, 1)
meet4kcore_get_set_1(bg_color, bg_color, 0x10, 1)


