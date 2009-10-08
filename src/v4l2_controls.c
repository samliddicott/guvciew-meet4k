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

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "v4l2uvc.h"
#include "string_utils.h"
#include "v4l2_controls.h"
#include "v4l2_dyna_ctrls.h"

static InputControl *add_control(int fd, struct v4l2_queryctrl *queryctrl, InputControl * control, int * nctrl)
{
	int n = *nctrl;
	control = g_renew(InputControl, control, n+1);
	control[n].i = n;
	control[n].id = queryctrl->id;
	control[n].type = queryctrl->type;
	//allocate control name (must free it on exit)
	control[n].name = strdup ((char *)queryctrl->name);
	control[n].min = queryctrl->minimum;
	control[n].max = queryctrl->maximum;
	control[n].step = queryctrl->step;
	control[n].default_val = queryctrl->default_value;
	control[n].enabled = (queryctrl->flags & V4L2_CTRL_FLAG_GRABBED) ? 0 : 1;
	control[n].entries = NULL;
	if (queryctrl->type == V4L2_CTRL_TYPE_BOOLEAN)
	{
		control[n].min = 0;
		control[n].max = 1;
		control[n].step = 1;
		/*get the first bit*/
		control[n].default_val=(queryctrl->default_value & 0x0001);
	} 
	else if (queryctrl->type == V4L2_CTRL_TYPE_MENU) 
	{
		struct v4l2_querymenu querymenu;
		memset(&querymenu,0,sizeof(struct v4l2_querymenu));
		control[n].min = 0;
		querymenu.id = queryctrl->id;
		querymenu.index = 0;
		while ( xioctl(fd, VIDIOC_QUERYMENU, &querymenu) == 0 ) 
		{
			//allocate entries list
			control[n].entries = g_renew(pchar, control[n].entries, querymenu.index+1);
			//allocate entrie name
			control[n].entries[querymenu.index] = g_strdup ((char *) querymenu.name);
			querymenu.index++;
		}
		control[n].max = querymenu.index - 1;
	}
	n++;
	*nctrl = n;
	return control;
}

/* enumerate device controls (use partial libwebcam implementation)
 * args:
 * fd: device file descriptor (must call open on the device first)
 * numb_controls: pointer to integer containing number of existing supported controls
 *
 * returns: allocated list of device controls or NULL on failure                      */
InputControl *
input_enum_controls (int fd, int *num_controls)
{
	int ret=0;
	int tries = IOCTL_RETRY;
	InputControl * control = NULL;
	int n = 0;
	struct v4l2_queryctrl queryctrl; 
	memset(&queryctrl,0,sizeof(struct v4l2_queryctrl));
	
	queryctrl.id = 0 | V4L2_CTRL_FLAG_NEXT_CTRL;
	
	if ((ret=xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) == 0)
	{
		g_printf("V4L2_CTRL_FLAG_NEXT_CTRL supported\n");
		// The driver supports the V4L2_CTRL_FLAG_NEXT_CTRL flag
		queryctrl.id = 0;
		int currentctrl= queryctrl.id;
		queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
		
		// Loop as long as ioctl does not return EINVAL
		// don't use xioctl here since we must reset queryctrl.id every retry (is this realy true ??)
		while((ret = ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)), ret ? errno != EINVAL : 1) 
		{
			
			if(ret && (errno == EIO || errno == EPIPE || errno == ETIMEDOUT))
			{
				// I/O error RETRY
				queryctrl.id = currentctrl | V4L2_CTRL_FLAG_NEXT_CTRL;
				tries = IOCTL_RETRY;
				while(tries-- &&
				  (ret = ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) &&
				  (errno == EIO || errno == EPIPE || errno == ETIMEDOUT)) 
				{
					queryctrl.id = currentctrl | V4L2_CTRL_FLAG_NEXT_CTRL;
				}
				if ( ret && ( tries <= 0)) g_printerr("Failed to query control id=%d tried %i times - giving up: %s\n", currentctrl, IOCTL_RETRY, strerror(errno));
			}
			// Prevent infinite loop for buggy NEXT_CTRL implementations
			if(ret && queryctrl.id <= currentctrl) 
			{
				currentctrl++;
				goto next_control;
			}
			currentctrl = queryctrl.id;
			
			// skip if control is disabled or failed
			if (ret || (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED))
				goto next_control;
			
			// Add control to control list
			control = add_control(fd, &queryctrl, control, &n);
			
next_control:
			queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
		}
	}
	else //NEXT_CTRL flag not supported, use old method 
	{
		g_printf("V4L2_CTRL_FLAG_NEXT_CTRL not supported\n");
		int currentctrl;
		for(currentctrl = V4L2_CID_BASE; currentctrl < V4L2_CID_LASTP1; currentctrl++) 
		{
			queryctrl.id = currentctrl;
			ret = xioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
				
			if(ret || queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;
			// Add control to control list
			control = add_control(fd, &queryctrl, control, &n);
		}
		
		for(currentctrl = V4L2_CID_CAMERA_CLASS_BASE; currentctrl < V4L2_CID_CAMERA_CLASS_LAST; currentctrl++) 
		{
			queryctrl.id = currentctrl;
			// Try querying the control
			ret = xioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
				
			if(ret || queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;
			// Add control to control list
			control = add_control(fd, &queryctrl, control, &n);
		}
		
		for(currentctrl = V4L2_CID_BASE_LOGITECH; currentctrl < V4L2_CID_LAST_EXTCTR; currentctrl++) 
		{
			queryctrl.id = currentctrl;
			// Try querying the control
			ret = xioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
				  
			if(ret || queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;
			// Add control to control list
			control = add_control(fd, &queryctrl, control, &n);
		}
	}
		
	*num_controls = n;
	return control;
}

/* free device control list 
 * args:
 * s: pointer to VidState struct containing complete device controls info
 *
 * returns: void*/
void
input_free_controls (struct VidState *s)
{
	int i=0;
	for (i = 0; i < s->num_controls; i++) 
	{
		//clean control widgets
		ControlInfo * ci = s->control_info + i;
		if (ci->widget)
			gtk_widget_destroy (ci->widget);
		if (ci->label)
			gtk_widget_destroy (ci->label);
		if (ci->spinbutton)
			gtk_widget_destroy (ci->spinbutton);
		//clean control name
		g_free (s->control[i].name);
		if (s->control[i].type == INPUT_CONTROL_TYPE_MENU) 
		{
			int j;
			for (j = 0; j <= s->control[i].max; j++) 
			{
				//clean entrie name
				g_free (s->control[i].entries[j]);
			}
			//clean entries list 
			g_free (s->control[i].entries);
		}
	}
	//clean control lists
	g_free (s->control_info);
	s->control_info = NULL;
	g_free (s->control);
	s->control = NULL;
}

/* get device control value
 * args:
 * fd: device file descriptor (must call open on the device first)
 * controls: pointer to InputControl struct containing basic control info
 *
 * returns: control value                                                 */
int
input_get_control (int fd, InputControl * control, int * val)
{
	int ret=0;
	struct v4l2_control c;
	memset(&c,0,sizeof(struct v4l2_control));

	c.id  = control->id;
	ret = xioctl (fd, VIDIOC_G_CTRL, &c);
	if (ret == 0) *val = c.value;
	else perror("VIDIOC_G_CTRL - Unable to get control");
	
	return ret;
}

/* set device control value
 * args:
 * fd: device file descriptor (must call open on the device first)
 * controls: pointer to InputControl struct containing basic control info
 * val: control value 
 *
 * returns: VIDIOC_S_CTRL return value ( failure  < 0 )                   */
int
input_set_control (int fd, InputControl * control, int val)
{
	int ret=0;
	struct v4l2_control c;

	c.id  = control->id;
	c.value = val;
	ret = xioctl (fd, VIDIOC_S_CTRL, &c);
	if (ret < 0) perror("VIDIOC_S_CTRL - Unable to set control");

	return ret;
}

/*--------------------------- focus control ----------------------------------*/
/* get device focus value
 * args:
 * fd: device file descriptor (must call open on the device first)
 *
 * returns: focus value                                                 */
int 
get_focus (int fd)
{
	int ret=0;
	struct v4l2_control c;
	int val=0;

	c.id  = V4L2_CID_FOCUS_ABSOLUTE;
	ret = xioctl (fd, VIDIOC_G_CTRL, &c);
	if (ret < 0) 
	{
		perror("VIDIOC_G_CTRL - get focus error");
		val = -1;
	}
	else val = c.value;
	
	return val;

}

/* set device focus value
 * args:
 * fd: device file descriptor (must call open on the device first)
 * val: focus value 
 *
 * returns: VIDIOC_S_CTRL return value ( failure  < 0 )                   */
int 
set_focus (int fd, int val) 
{
	int ret=0;
	struct v4l2_control c;

	c.id  = V4L2_CID_FOCUS_ABSOLUTE;
	c.value = val;
	ret = xioctl (fd, VIDIOC_S_CTRL, &c);
	if (ret < 0) perror("VIDIOC_S_CTRL - set focus error");

	return ret;
}

/* SRC: https://lists.berlios.de/pipermail/linux-uvc-devel/2007-July/001888.html
 * fd: the device file descriptor
 * pan: pan angle in 1/64th of degree
 * tilt: tilt angle in 1/64th of degree
 * reset: set 1 to reset Pan, set 2 to reset tilt, set to 3 to reset pan/tilt to the device origin, set to 0 otherwise 
 *
 * returns: 0 on success or -1 on failure                                                                              */
int uvcPanTilt(int fd, int pan, int tilt, int reset) 
{
	struct v4l2_ext_control xctrls[2];
	struct v4l2_ext_controls ctrls;
	
	if (reset) 
	{
		switch (reset) 
		{
			case 1:
				xctrls[0].id = V4L2_CID_PAN_RESET;
				xctrls[0].value = 1;
				break;
			
			case 2:
				xctrls[0].id = V4L2_CID_TILT_RESET;
				xctrls[0].value = 1;
				break;
			
			case 3:
				xctrls[0].value = 3;
				xctrls[0].id = V4L2_CID_PANTILT_RESET_LOGITECH;
				break;
		}
		ctrls.count = 1;
		ctrls.controls = xctrls;
	} 
	else 
	{
		xctrls[0].id = V4L2_CID_PAN_RELATIVE;
		xctrls[0].value = pan;
		xctrls[1].id = V4L2_CID_TILT_RELATIVE;
		xctrls[1].value = tilt;
	
		ctrls.count = 2;
		ctrls.controls = xctrls;
	}
	
	if ( xioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0 ) 
	{
		perror("VIDIOC_S_EXT_CTRLS - Pan/Tilt error\n");
			return -1;
	}
	
	return 0;
}
