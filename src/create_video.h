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

#ifndef CREATE_VIDEO_H
#define CREATE_VIDEO_H

#include "guvcview.h"



/* sound controls*/
void 
set_sensitive_snd_contrls (const int flag, struct GWIDGET *gwidget);

/*video controls*/
void 
set_sensitive_vid_contrls (const int flag, const int sndEnable, struct GWIDGET *gwidget);

int initVideoFile(struct ALL_DATA *data);

void closeVideoFile(struct ALL_DATA *all_data);

int write_video_data(struct ALL_DATA *all_data, BYTE *buff, int size);

int write_video_frame (struct ALL_DATA *all_data, 
	void *jpeg_struct, 
	void *lavc_data,
	void *pvid);

int sync_audio_frame(struct ALL_DATA *all_data);

int write_audio_frame (struct ALL_DATA *all_data);

#endif
