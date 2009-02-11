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
#ifndef V4L2_DEVICES_H
#define V4L2_DEVICES_H

typedef struct _VidDevice
{
	char *device;
	char *name;
	char *driver;
	char *location;
	char *vendor;
	char *product;
	char *version;
	int valid;
	int current;
} VidDevice;

/* enumerates system video devices
 * by checking /sys/class/video4linux
 * args: 
 * videodevice: current device string (default "/dev/video0")
 * num_dev: pointer to int with number of system devices
 * current_dev: pointer to int with index from device list with current device 
 * 
 * returns: pointer to VidDevice an allocated device list or NULL on failure   */
VidDevice *enum_devices( gchar *videodevice, int *num_dev, int *current_dev);

/*clean video devices list
 * args: listVidDevices: array of VidDevice (list of video devices)
 * numb_devices: number of existing supported video devices
 *
 * returns: void                                                       */
void freeDevices(VidDevice *listVidDevices, int num_devices);

#endif
