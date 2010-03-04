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
#ifndef V4L2_CONTROLS_H
#define V4L2_CONTROLS_H

#include <gtk/gtk.h>

/* 
 * 11-09-2009 dropped support for private controls. Use V4L2 user and extended controls only
 */
 
#ifndef V4L2_CID_CAMERA_CLASS_LAST
#define V4L2_CID_CAMERA_CLASS_LAST		(V4L2_CID_CAMERA_CLASS_BASE +20)
#endif


typedef enum 
{
	INPUT_CONTROL_TYPE_INTEGER = 1,
	INPUT_CONTROL_TYPE_BOOLEAN = 2,
	INPUT_CONTROL_TYPE_MENU = 3,
	INPUT_CONTROL_TYPE_BUTTON = 4,
} InputControlType;

typedef struct _InputControl 
{
	unsigned int i;
	unsigned int id;
	InputControlType type;
	char * name;
	int min, max, step, default_val, enabled;
	char ** entries;
} InputControl;

typedef struct _ControlInfo 
{
	GtkWidget * widget;
	GtkWidget * label;
	GtkWidget *spinbutton; /*used in integer (slider) controls*/
	unsigned int idx;
	int maxchars;
} ControlInfo;

struct VidState 
{
	GtkWidget * table;

	int width_req;
	int height_req;

	InputControl * control;
	int num_controls;
	ControlInfo * control_info;
};

/* enumerate device controls 
 * args:
 * fd: device file descriptor (must call open on the device first)
 * numb_controls: pointer to integer containing number of existing supported controls
 *
 * returns: allocated list of device controls or NULL on failure                      */
InputControl *input_enum_controls (int fd, int *num_controls);

/* get device control value
 * args:
 * fd: device file descriptor (must call open on the device first)
 * controls: pointer to InputControl struct containing basic control info
 *
 * returns: control value                                                 */
int input_get_control (int fd, int control_id, int * val);

/* set device control value
 * args:
 * fd: device file descriptor (must call open on the device first)
 * controls: pointer to InputControl struct containing basic control info
 * val: control value 
 *
 * returns: VIDIOC_S_CTRL return value ( failure  < 0 )                   */
int input_set_control (int fd, int control_id, int val);

/* SRC: https://lists.berlios.de/pipermail/linux-uvc-devel/2007-July/001888.html
 * fd: the device file descriptor
 * pan: pan angle in 1/64th of degree
 * tilt: tilt angle in 1/64th of degree
 * reset: set 1 to reset Pan, set 2 to reset tilt, set to 3 to reset pan/tilt to the device origin, set to 0 otherwise 
 *
 * returns: 0 on success or -1 on failure                                                                              */
int uvcPanTilt(int fd, int pan, int tilt, int reset);

/* free device control list 
 * args:
 * s: pointer to VidState struct containing complete device controls info
 *
 * returns: void*/
void input_free_controls (struct VidState *s);

#endif
