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

#ifndef UVC_MEET4K_H
#define UVC_MEET4K_H

#include "gviewv4l2core.h"
#include "v4l2_core.h"
#include "v4l2_xu_ctrls.h"

uint8_t is_probably_obsbot (int vendor, v4l2_dev_t *vd);
void add_meet4k(v4l2_dev_t *vd);
uint8_t get_uvc_meet4k_unit_id (v4l2_dev_t *vd);

#include "uvc_meet4k.ch"

#endif /*UVC_MEET4K_H*/
