/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

/*******************************************************************************#
#                                                                               #
#  MJpeg decoding and frame capture taken from luvcview                         #
#                                                                               # 
#                                                                               #
********************************************************************************/
#include <stdlib.h>
    
#include "v4l2uvc.h"
#include "utils.h"
/* support for internationalization - i18n */
#include <glib/gi18n.h>

/* needed only for language files (not used)*/
/*UVC driver control strings*/
#define	BRIGHT 		N_("Brightness")
#define	CONTRAST 	N_("Contrast")
#define	HUE 		N_("Hue")
#define	SATURAT		N_("Saturation")
#define	SHARP		N_("Sharpness")
#define	GAMMA		N_("Gamma")
#define	BLCOMP		N_("Backlight Compensation")
#define	GAIN		N_("Gain")
#define	PLFREQ		N_("Power Line Frequency")
#define HUEAUTO		N_("Hue, Auto")
#define	EXPAUTO		N_("Exposure, Auto")
#define	EXPAUTOPRI	N_("Exposure, Auto Priority")
#define	EXPABS		N_("Exposure (Absolute)")
#define	WBTAUTO		N_("White Balance Temperature, Auto")
#define	WBT		N_("White Balance Temperature")
#define WBCAUTO		N_("White Balance Component, Auto")
#define WBCB		N_("White Balance Blue Component")
#define	WBCR		N_("White Balance Red Component")
#define	FOCUSAUTO	N_("Focus, Auto")


/* some Logitech webcams have pan/tilt/focus controls */
#define LENGTH_OF_XU_CTR (6)
#define LENGTH_OF_XU_MAP (8)

static struct uvc_xu_control_info xu_ctrls[] = {
	{
		.entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector = XU_MOTORCONTROL_PANTILT_RELATIVE,
		.index    = 0,
		.size     = 4,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_DEF
	},
	{
		.entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector = XU_MOTORCONTROL_PANTILT_RESET,
		.index    = 1,
		.size     = 1,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
	},
	{
		.entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector = XU_MOTORCONTROL_FOCUS,
		.index    = 2,
		.size     = 6,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX |UVC_CONTROL_GET_DEF
	},
	{
		.entity   = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector = XU_COLOR_PROCESSING_DISABLE,
		.index    = 4,
		.size     = 1,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR |UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
	},
	{
		.entity   = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector = XU_RAW_DATA_BITS_PER_PIXEL,
		.index    = 7,
		.size     = 1,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR |UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
	},
	{
		.entity   = UVC_GUID_LOGITECH_USER_HW_CONTROL,
		.selector = XU_HW_CONTROL_LED1,
		.index    = 0,
		.size     = 3,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR |UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
	},
	
};

/* mapping for Pan/Tilt/Focus */
static struct uvc_xu_control_mapping xu_mappings[] = {
	{
		.id        = V4L2_CID_PAN_RELATIVE_LOGITECH,
		.name      = N_("Pan (relative)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
		.size      = 16,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_SIGNED
	},
	{
		.id        = V4L2_CID_TILT_RELATIVE_LOGITECH,
		.name      = N_("Tilt (relative)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
		.size      = 16,
		.offset    = 16,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_SIGNED
	},
	{
		.id        = V4L2_CID_PANTILT_RESET_LOGITECH,
		.name      = N_("Pan/Tilt (reset)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RESET,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	{
		.id        = V4L2_CID_FOCUS_LOGITECH,
		.name      = N_("Focus (absolute)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_FOCUS,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	{
		.id        = V4L2_CID_LED1_MODE_LOGITECH,
		.name      = N_("LED1 Mode"),
		.entity    = UVC_GUID_LOGITECH_USER_HW_CONTROL,
		.selector  = XU_HW_CONTROL_LED1,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	{
		.id        = V4L2_CID_LED1_FREQUENCY_LOGITECH,
		.name      = N_("LED1 Frequency"),
		.entity    = UVC_GUID_LOGITECH_USER_HW_CONTROL,
		.selector  = XU_HW_CONTROL_LED1,
		.size      = 8,
		.offset    = 16,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	{
		.id        = V4L2_CID_DISABLE_PROCESSING_LOGITECH,
		.name      = N_("Disable video processing"),
		.entity    = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector  = XU_COLOR_PROCESSING_DISABLE,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_BOOLEAN,
		.data_type = UVC_CTRL_DATA_TYPE_BOOLEAN
	},
	{
		.id        = V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH,
		.name      = N_("Raw bits per pixel"),
		.entity    = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector  = XU_RAW_DATA_BITS_PER_PIXEL,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	
};


int check_videoIn(struct vdIn *vd)
{
int ret;
 if (vd == NULL)
	return -1;
	
	
	memset(&vd->cap, 0, sizeof(struct v4l2_capability));
    ret = ioctl(vd->fd, VIDIOC_QUERYCAP, &vd->cap);
    if (ret < 0) {
		printf("Error opening device %s: unable to query device fd=%d.\n",
	    vd->videodevice,vd->fd);
		goto fatal;
    }

    if ((vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
		printf("Error opening device %s: video capture not supported.\n",
	    vd->videodevice);
		goto fatal;;
    }
    if (vd->grabmethod) {
		if (!(vd->cap.capabilities & V4L2_CAP_STREAMING)) {
	    	printf("%s does not support streaming i/o\n", vd->videodevice);
	    	goto fatal;
		}
    } else {
		if (!(vd->cap.capabilities & V4L2_CAP_READWRITE)) {
	    	printf("%s does not support read i/o\n", vd->videodevice);
	    	goto fatal;
		}
    }
    enum_frame_formats(vd);
    return 0;
fatal:
	return -1;
}



int
init_videoIn(struct vdIn *vd, char *device, int width, int height,
	     int format, int grabmethod, int fps, int fps_num)
{
    if (vd == NULL || device == NULL)
	return -1;
    if (width == 0 || height == 0)
	return -1;
    if (grabmethod < 0 || grabmethod > 1)
	grabmethod = 1;		//mmap by default;
    vd->videodevice = NULL;
    vd->status = NULL;
    if((vd->videodevice = (char *) calloc(1, 16 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:vd->videodevice\n");
		goto error1;
	}
    if((vd->status = (char *) calloc(1, 100 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:vd->status\n");
		goto error1;
	}
    snprintf(vd->videodevice, 15, "%s", device);
    printf("video %s \n", vd->videodevice);
    vd->capAVI = FALSE;
	
    if((vd->AVIFName = (char *) calloc(1, 120 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:vd->AVIFName\n");
		goto error1;
	}
    snprintf(vd->AVIFName, 14, DEFAULT_AVI_FNAME);
    vd->SupMjpg=0;
    vd->SupYuv=0;
    vd->fps = fps;
    vd->fps_num = fps_num;
    vd->signalquit = 1;
    vd->PanTilt=0;
    vd->setFPS=0;
    vd->width = width;
    vd->height = height;
    vd->formatIn = format;
    vd->grabmethod = grabmethod;
    vd->capImage=FALSE;
	
    if((vd->ImageFName = (char *) calloc(1, 120 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:vd->ImgFName\n");
		goto error1;
	}
    snprintf(vd->ImageFName, 14, DEFAULT_IMAGE_FNAME);
	
    vd->timecode.type = V4L2_TC_TYPE_25FPS;
    vd->timecode.flags = V4L2_TC_FLAG_DROPFRAME;
	
    vd->available_exp[0]=-1;
    vd->available_exp[1]=-1;
    vd->available_exp[2]=-1;
    vd->available_exp[3]=-1;
	
    if (init_v4l2(vd) < 0) {
	printf(" Init v4L2 failed !! exit fatal \n");
	goto error2;
    }
    /* alloc a temp buffer to reconstruct the pict (MJPEG)*/
    vd->framesizeIn = (vd->width * vd->height << 1);
    switch (vd->formatIn) {
    	case V4L2_PIX_FMT_MJPEG:
		vd->tmpbuffer =
	    	(unsigned char *) calloc(1, (size_t) vd->framesizeIn);
		if (!vd->tmpbuffer) {
	   		printf("couldn't calloc memory for:vd->tmpbuffer\n");
			goto error3;
		}
		vd->framebuffer = (unsigned char *) calloc(1,
			     (size_t) vd->width * (vd->height + 8) * 2);
		break;
    	case V4L2_PIX_FMT_YUYV:/*YUYV doesn't need a temp buffer*/
		vd->framebuffer =
	    	(unsigned char *) calloc(1, (size_t) vd->framesizeIn);
		break;
    	default:
		printf(" should never arrive exit fatal !!\n");
		goto error4;
		break;
    }
    if (!vd->framebuffer) {
		printf("couldn't calloc memory for:vd->framebuffer\n");	
		goto error5;
	}
	
    return 0;
	/*error: clean up allocs*/
  error5:
	free(vd->framebuffer);
  error4:
    free(vd->tmpbuffer);
  error3:
	close(vd->fd);
  error2:
    free(vd->status);
  error1:
    free(vd->videodevice);
    return -1;
}


/* map the buffers */
static int query_buff(struct vdIn *vd, const int setUNMAP) 
{
	int i, ret;
    for (i = 0; i < NB_BUFFER; i++) {
		
		/* unmap old buffer */
		if (setUNMAP)
			if(munmap(vd->mem[i],vd->buf.length)<0) printf("couldn't unmap buff\n");
		
		memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
		vd->buf.index = i;
		vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vd->buf.flags = V4L2_BUF_FLAG_TIMECODE;
		vd->buf.timecode = vd->timecode;
		vd->buf.timestamp.tv_sec = 0;//get frame as soon as possible
		vd->buf.timestamp.tv_usec = 0;
		vd->buf.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(vd->fd, VIDIOC_QUERYBUF, &vd->buf);
		if (ret < 0) {
	    		printf("Unable to query buffer (%d).\n", errno);
	    	return 1;
		}
		/* map new buffer */
		vd->mem[i] = mmap(0 /* start anywhere */ ,
			  vd->buf.length, PROT_READ, MAP_SHARED, vd->fd,
			  vd->buf.m.offset);
		if (vd->mem[i] == MAP_FAILED) {
	    	printf("Unable to map buffer (%d)\n", errno);
	    	return 2;
		}
	}

	return 0;
}

static int queue_buff(struct vdIn *vd)
{
	int i, ret;
	for (i = 0; i < NB_BUFFER; ++i) {
		memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
		vd->buf.index = i;
		vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vd->buf.flags = V4L2_BUF_FLAG_TIMECODE;
		vd->buf.timecode = vd->timecode;
		vd->buf.timestamp.tv_sec = 0;//get frame as soon as possible
		vd->buf.timestamp.tv_usec = 0;
		vd->buf.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
		if (ret < 0) {
	    	printf("Unable to queue buffer (%d).\n", errno);
	    	return 1;
		}
    }
	return 0;
}


int init_v4l2(struct vdIn *vd)
{
    int ret = 0;
	
	if (vd->fd <=0 ){
	  if ((vd->fd = open(vd->videodevice, O_RDWR )) == -1) {
	    perror("ERROR opening V4L interface \n");
	    exit(1);
      }
    }
	
	/* populate video capabilities structure array           */
	/* should only be called after all vdIn struct elements  */
	/* have been initialized                                 */
	check_videoIn(vd);

    /* set format in */
    memset(&vd->fmt, 0, sizeof(struct v4l2_format));
    vd->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->fmt.fmt.pix.width = vd->width;
    vd->fmt.fmt.pix.height = vd->height;
    vd->fmt.fmt.pix.pixelformat = vd->formatIn;
    vd->fmt.fmt.pix.field = V4L2_FIELD_ANY;
    ret = ioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt);
    if (ret < 0) {
		printf("Unable to set format: %d.\n", errno);
		goto fatal;
    }
    if ((vd->fmt.fmt.pix.width != vd->width) ||
		(vd->fmt.fmt.pix.height != vd->height)) {
		printf(" format asked unavailable get width %d height %d \n",
	       	vd->fmt.fmt.pix.width, vd->fmt.fmt.pix.height);
			vd->width = vd->fmt.fmt.pix.width;
			vd->height = vd->fmt.fmt.pix.height;
    }
	
	vd->streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->streamparm.parm.capture.timeperframe.numerator = vd->fps_num;
	vd->streamparm.parm.capture.timeperframe.denominator = vd->fps;
	ret = ioctl(vd->fd,VIDIOC_S_PARM,&vd->streamparm);
	if (ret < 0) {
		printf("Unable to set timeperframe: %d.\n", errno);
	}	
    /* request buffers */
    memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
    vd->rb.count = NB_BUFFER;
    vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->rb.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb);
    if (ret < 0) {
		printf("Unable to allocate buffers: %d.\n", errno);
		goto fatal;
    }
    /* map the buffers */
	if (query_buff(vd,0)) goto fatal;

    /* Queue the buffers. */
    if (queue_buff(vd)) goto fatal;
	
    return 0;
  fatal:
    return -1;

}



static int video_enable(struct vdIn *vd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    ret = ioctl(vd->fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
		printf("Unable to %s capture: %d.\n", "start", errno);
	return ret;
    }
    vd->isstreaming = 1;
    return 0;
}

static int video_disable(struct vdIn *vd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    ret = ioctl(vd->fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
	printf("Unable to %s capture: %d.\n", "stop", errno);
	if(errno == 9) vd->isstreaming = 0;/*capture as allready stoped*/
	return ret;
    }
    vd->isstreaming = 0;
    return 0;
}


int uvcGrab(struct vdIn *vd)
{
#define HEADERFRAME1 0xaf
    int ret;

    if (!vd->isstreaming)
		if (video_enable(vd))
	    	goto err;
    memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
    vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->buf.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(vd->fd, VIDIOC_DQBUF, &vd->buf);
    if (ret < 0) {
		printf("Unable to dequeue buffer (%d) fd is %d.\n", errno, vd->fd);
		goto err;
    }

	

    switch (vd->formatIn) {
    	case V4L2_PIX_FMT_MJPEG:
        	if(vd->buf.bytesused <= HEADERFRAME1) {
				/* Prevent crash on empty image */
	        	printf("Ignoring empty buffer ...\n");
	    		return 0;
        	}
			memcpy(vd->tmpbuffer, vd->mem[vd->buf.index],vd->buf.bytesused);
			if (jpeg_decode(&vd->framebuffer, vd->tmpbuffer, &vd->width,
	     	                                             &vd->height) < 0) {
	    		printf("jpeg decode errors\n");
	    		goto err;
			}
			break;
    	case V4L2_PIX_FMT_YUYV:
			if (vd->buf.bytesused > vd->framesizeIn)
	    		memcpy(vd->framebuffer, vd->mem[vd->buf.index],
					   (size_t) vd->framesizeIn);
			else
	    		memcpy(vd->framebuffer, vd->mem[vd->buf.index],
		   		       (size_t) vd->buf.bytesused);
			break;
    	default:
			goto err;
			break;
    }
	if(vd->setFPS) {
		video_disable(vd);
		input_set_framerate (vd);
		video_enable(vd);
		query_buff(vd,1);
		queue_buff(vd);
		vd->setFPS=0;	
	} else {	
    	ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
    	if (ret < 0) {
			printf("Unable to requeue buffer (%d).\n", errno);
			goto err;
    	}
	}

    return 0;
  err:
    vd->signalquit = 0;
    return -1;
}

void close_v4l2(struct vdIn *vd)
{
    int i;
	
	if (vd->isstreaming)
		video_disable(vd);
    if (vd->tmpbuffer)
		free(vd->tmpbuffer);
    if (vd->framebuffer)
		free(vd->framebuffer);
    if (vd->videodevice)
		free(vd->videodevice);
	if (vd->status)
    	free(vd->status);
	if (vd->ImageFName)
    	free(vd->ImageFName);
	if (vd->AVIFName)
		free(vd->AVIFName);
	
	for (i = 0; i < NB_BUFFER; i++) {
		if(munmap(vd->mem[i],vd->buf.length)<0) printf("couldn't unmap buff\n");
	}
	
    vd->videodevice = NULL;
	vd->tmpbuffer = NULL;
	vd->framebuffer = NULL;
    vd->status = NULL;
	vd->ImageFName = NULL;
	vd->AVIFName = NULL;
    
}

int
input_get_control (struct vdIn * device, InputControl * control, int * val)
{
    int fd, ret;
    struct v4l2_control c;

    fd = device->fd;
 

    c.id  = control->id;
    ret = ioctl (fd, VIDIOC_G_CTRL, &c);
    if (ret == 0)
        *val = c.value;

    return ret;
}

int
input_set_control (struct vdIn * device, InputControl * control, int val)
{
   
    int fd, ret;
    struct v4l2_control c;

    fd = device->fd;


    c.id  = control->id;
    c.value = val;
    ret = ioctl (fd, VIDIOC_S_CTRL, &c);

    return ret;
}

int
input_set_framerate (struct vdIn * device)
{  
	int fd, ret;

	fd = device->fd;
    
	device->streamparm.parm.capture.timeperframe.numerator = device->fps_num;
	device->streamparm.parm.capture.timeperframe.denominator = device->fps;
	 
	ret = ioctl(fd,VIDIOC_S_PARM,&device->streamparm);
	if (ret < 0) {
		printf("Unable to set timeperframe: %d.\n", errno);
	} 

	return ret;
}

int
input_get_framerate (struct vdIn * device)
{
   
    int fd, ret, fps, fps_num;

    fd = device->fd;
    
	ret = ioctl(fd,VIDIOC_G_PARM,&device->streamparm);
	if (ret < 0) {
		printf("Unable to get timeperframe: %d.\n", errno);
	} else {
		/*it seems numerator is allways 1*/
		fps = device->streamparm.parm.capture.timeperframe.denominator;
	 	fps_num = device->streamparm.parm.capture.timeperframe.numerator;
	 	device->fps=fps;
	 	device->fps_num=fps_num;
	}		
    return ret;
}

InputControl *
input_enum_controls (struct vdIn * device, int * num_controls)
{
    int fd;
    InputControl * control = NULL;
    int n = 0;
    struct v4l2_queryctrl queryctrl;
    int i;
    fd = device->fd;
	
	initDynCtrls(device);
    
    i = V4L2_CID_BASE; /* as defined by V4L2 */
    while (i <= V4L2_CID_LAST_EXTCTR) { 
        queryctrl.id = i;
        if (ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl) == 0 &&
                !(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
            control = realloc (control, (n+1)*sizeof (InputControl));
            control[n].i = n;
            control[n].id = queryctrl.id;
            control[n].type = queryctrl.type;
            control[n].name = strdup ((char *)queryctrl.name);
            control[n].min = queryctrl.minimum;
            control[n].max = queryctrl.maximum;
            control[n].step = queryctrl.step;
            control[n].default_val = queryctrl.default_value;
            control[n].enabled = (queryctrl.flags & V4L2_CTRL_FLAG_GRABBED) ? 0 : 1;
            control[n].entries = NULL;
			if (queryctrl.type == V4L2_CTRL_TYPE_BOOLEAN){
				control[n].min = 0;
				control[n].max = 1;
				control[n].step = 1;
				/*get the first bit*/
				control[n].default_val=(queryctrl.default_value & 0x0001);
			} else if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
                struct v4l2_querymenu querymenu;
                control[n].min = 0;

                querymenu.id = queryctrl.id;
                querymenu.index = 0;
                while (ioctl (fd, VIDIOC_QUERYMENU, &querymenu) == 0) {
                    control[n].entries = realloc (control[n].entries,
                            (querymenu.index+1)*sizeof (char *));
                    control[n].entries[querymenu.index] = strdup ((char *)querymenu.name);
                    querymenu.index++;
                }
                control[n].max = querymenu.index - 1;
            }
            n++;
        }
        i++;
       	if (i == V4L2_CID_LAST_NEW)  /* jump between CIDs*/
       		i = V4L2_CID_CAMERA_CLASS_BASE;
	   	if (i == V4L2_CID_CAMERA_CLASS_LAST)
			i = V4L2_CID_PRIVATE_BASE;
		if (i == V4L2_CID_PRIVATE_LAST)
			i = V4L2_CID_BASE_EXTCTR;
	   
    }

    *num_controls = n;
		

   return control;
}

void
input_free_controls (InputControl * control, int num_controls)
{
    int i;

    for (i = 0; i < num_controls; i++) {
        free (control[i].name);
        if (control[i].type == INPUT_CONTROL_TYPE_MENU) {
            int j;
            for (j = 0; j <= control[i].max; j++) {
                free (control[i].entries[j]);
            }
            free (control[i].entries);
        }
    }
    free (control);
	printf("cleaned controls\n");
}
/******************************* enumerations *********************************/
int enum_frame_intervals(struct vdIn *vd, __u32 pixfmt, __u32 width, __u32 height, 
						         int list_form, int list_ind)
{
	int ret;
	struct v4l2_frmivalenum fival;
	int list_fps=0;
	memset(&fival, 0, sizeof(fival));
	fival.index = 0;
	fival.pixel_format = pixfmt;
	fival.width = width;
	fival.height = height;
	printf("\tTime interval between frame: ");
	while ((ret = ioctl(vd->fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) {
		if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
				printf("%u/%u, ", fival.discrete.numerator, fival.discrete.denominator);
				vd->listVidCap[list_form][list_ind].framerate_num[list_fps]=fival.discrete.numerator;
				vd->listVidCap[list_form][list_ind].framerate_denom[list_fps]=fival.discrete.denominator;
				if(list_fps<(MAX_LIST_FPS-1)) list_fps++;
		} else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
				printf("{min { %u/%u } .. max { %u/%u } }, ",
						fival.stepwise.min.numerator, fival.stepwise.min.numerator,
						fival.stepwise.max.denominator, fival.stepwise.max.denominator);
				break;
		} else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
				printf("{min { %u/%u } .. max { %u/%u } / "
						"stepsize { %u/%u } }, ",
						fival.stepwise.min.numerator, fival.stepwise.min.denominator,
						fival.stepwise.max.numerator, fival.stepwise.max.denominator,
						fival.stepwise.step.numerator, fival.stepwise.step.denominator);
				break;
		}
		fival.index++;
	}
    	/* WORKAROUND*/
    	if (list_fps == 0) {
	/*logitech 2M pixel cameras don't return fps for YUV 1600x1200 (set to min - 5 fps)*/
		vd->listVidCap[list_form][list_ind].numb_frates=1;
		vd->listVidCap[list_form][list_ind].framerate_num[list_fps]=1;
		vd->listVidCap[list_form][list_ind].framerate_denom[list_fps]=5;
	} else {
		vd->listVidCap[list_form][list_ind].numb_frates=list_fps;
	}
	printf("\n");
	if (ret != 0 && errno != EINVAL) {
		printf("ERROR enumerating frame intervals: %d\n", errno);
		return errno;
	}

	return 0;
}
int enum_frame_sizes(struct vdIn *vd, __u32 pixfmt)
{
	int ret;
	int list_ind=0;
	int list_form=0;
	struct v4l2_frmsizeenum fsize;

	memset(&fsize, 0, sizeof(fsize));
	fsize.index = 0;
	fsize.pixel_format = pixfmt;
	while ((ret = ioctl(vd->fd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0) {
		if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
			printf("{ discrete: width = %u, height = %u }\n",
					fsize.discrete.width, fsize.discrete.height);
			switch(pixfmt) {
				case V4L2_PIX_FMT_MJPEG:
					vd->SupMjpg++;
					list_form=0;
					vd->listVidCap[list_form][list_ind].width=fsize.discrete.width;
					vd->listVidCap[list_form][list_ind].height=fsize.discrete.height;
		/*if this is the selected format set number of resolutions for combobox*/
					if(vd->formatIn == pixfmt) vd->numb_resol=list_ind+1;
					break;
				case V4L2_PIX_FMT_YUYV:
					vd->SupYuv++;
					list_form=1;
					vd->listVidCap[list_form][list_ind].width=fsize.discrete.width;
					vd->listVidCap[list_form][list_ind].height=fsize.discrete.height;
		/*if this is the selected format set number of resolutions for combobox*/
					if(vd->formatIn == pixfmt) vd->numb_resol=list_ind+1;
					break;
					
			}
			ret = enum_frame_intervals(vd, pixfmt,
					fsize.discrete.width, fsize.discrete.height,
									                  list_form,list_ind);
			if(list_ind<(MAX_LIST_VIDCAP-1)) list_ind++;
			if (ret != 0)
				printf("  Unable to enumerate frame sizes.\n");
		} else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
			printf("{ continuous: min { width = %u, height = %u } .. "
					"max { width = %u, height = %u } }\n",
					fsize.stepwise.min_width, fsize.stepwise.min_height,
					fsize.stepwise.max_width, fsize.stepwise.max_height);
			printf("  Refusing to enumerate frame intervals.\n");
			break;
		} else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
			printf("{ stepwise: min { width = %u, height = %u } .. "
					"max { width = %u, height = %u } / "
					"stepsize { width = %u, height = %u } }\n",
					fsize.stepwise.min_width, fsize.stepwise.min_height,
					fsize.stepwise.max_width, fsize.stepwise.max_height,
					fsize.stepwise.step_width, fsize.stepwise.step_height);
			printf("  Refusing to enumerate frame intervals.\n");
			break;
		}
		fsize.index++;
	}
	if (ret != 0 && errno != EINVAL) {
		printf("ERROR enumerating frame sizes: %d\n", errno);
		return errno;
	}

	return 0;
}

int enum_frame_formats(struct vdIn *vd)
{
	int ret;
	struct v4l2_fmtdesc fmt;
	

	memset(&fmt, 0, sizeof(fmt));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while ((ret = ioctl(vd->fd, VIDIOC_ENUM_FMT, &fmt)) == 0) {
		
		fmt.index++;
		printf("{ pixelformat = '%c%c%c%c', description = '%s' }\n",
				fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,
				(fmt.pixelformat >> 16) & 0xFF, (fmt.pixelformat >> 24) & 0xFF,
				fmt.description);
		ret = enum_frame_sizes(vd, fmt.pixelformat);
		if (ret != 0)
			printf("  Unable to enumerate frame sizes.\n");
	}
	if (errno != EINVAL) {
		printf("ERROR enumerating frame formats: %d\n", errno);
		return errno;
	}

	return 0;
}

int initDynCtrls(struct vdIn *vd) {

	int i=0;
	int err;
	errno=0;
	/* try to add all controls listed above */
	for ( i=0; i<LENGTH_OF_XU_CTR; i++ ) {
		printf("Adding control for %d\n",i);
		if ((err=ioctl(vd->fd, UVCIOC_CTRL_ADD, &xu_ctrls[i])) < 0 ) {
			if (errno!=EEXIST) {
				printf("uvcioc ctrl add error: errno=%d (retval=%d)\n",errno,err);
			} else {
				//printf("control %d already exists\n",i);
			}
		}
	}

	errno=0;
	/* after adding the controls, add the mapping now */
	for ( i=0; i<LENGTH_OF_XU_MAP; i++ ) {
		printf("mapping controls for %s\n", xu_mappings[i].name);
		if ((err=ioctl(vd->fd, UVCIOC_CTRL_MAP, &xu_mappings[i])) < 0) {
			if (errno!=EEXIST) {
				printf("uvcioc ctrl map error: errno=%d (retval=%d)\n",errno,err);
			} else {
				//printf("mapping %d already exists\n", i);
			}
		}
	} 
	return 0;
}

/*
SRC: https://lists.berlios.de/pipermail/linux-uvc-devel/2007-July/001888.html
- dev: the device file descriptor
- pan: pan angle in 1/64th of degree
- tilt: tilt angle in 1/64th of degree
- reset: set to 1 to reset pan/tilt to the device origin, set to 0 otherwise
*/
int uvcPanTilt(struct vdIn *vd, int pan, int tilt, int reset) {
	struct v4l2_ext_control xctrls[2];
	struct v4l2_ext_controls ctrls;
	
	if (reset) {
		xctrls[0].id = V4L2_CID_PANTILT_RESET_LOGITECH;
		xctrls[0].value = 3;
	
		ctrls.count = 1;
		ctrls.controls = xctrls;
	} else {
		xctrls[0].id = V4L2_CID_PAN_RELATIVE_LOGITECH;
		xctrls[0].value = pan;
		xctrls[1].id = V4L2_CID_TILT_RELATIVE_LOGITECH;
		xctrls[1].value = tilt;
	
		ctrls.count = 2;
		ctrls.controls = xctrls;
	}
	
	if ( ioctl(vd->fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0 ) {
		return -1;
	}
	
	return 0;
}
