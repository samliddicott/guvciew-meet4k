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
#include "v4l2_core.h"


int main(int argc, char *argv[])
{
	set_v4l2_verbosity(1);

	v4l2_dev* device = init_v4l2_dev("/dev/video0");

	/*set format*/
	device->current_format = 0;

	device->format.fmt.pix.pixelformat = device->list_stream_formats[0].format;
	device->format.fmt.pix.width  = device->list_stream_formats[0].list_stream_cap[0].width;
	device->format.fmt.pix.height = device->list_stream_formats[0].list_stream_cap[0].height;

	/*try to set the video stream format on the device*/
	if(try_video_stream(device) != 0)
		printf("could not set the defined stream format\n");

	if(device)
		close_v4l2_dev(device);

	return 0;
}
