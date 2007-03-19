/*************************************************************************************************
#	    guvcview              http://guvcview.berlios.de												#
#     Paulo Assis <pj.assis@gmail.com>																#
#																														#
# This program is free software; you can redistribute it and/or modify         				#
# it under the terms of the GNU General Public License as published by   				#
# the Free Software Foundation; either version 2 of the License, or           				#
# (at your option) any later version.                                          								#
#                                                                              										#
# This program is distributed in the hope that it will be useful,              					#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             		#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 					#
#                                                                              										#
# You should have received a copy of the GNU General Public License           		#
# along with this program; if not, write to the Free Software                  					#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA		#
#                                                                              										#
*************************************************************************************************/

#include <stdlib.h>

#include "v4l2uvc.h"


static int init_v4l2(struct vdIn *vd);

int
init_videoIn(struct vdIn *vd, char *device, int width, int height,
	     int format, int grabmethod, int fps)
{
    //~ int ret = -1;
    //~ int i;
    if (vd == NULL || device == NULL)
	return -1;
    if (width == 0 || height == 0)
	return -1;
    if (grabmethod < 0 || grabmethod > 1)
	grabmethod = 1;		//mmap by default;
    vd->videodevice = NULL;
    vd->status = NULL;
    vd->pictName = NULL;
    vd->videodevice = (char *) calloc(1, 16 * sizeof(char));
    vd->status = (char *) calloc(1, 100 * sizeof(char));
    vd->pictName = (char *) calloc(1, 80 * sizeof(char));
    snprintf(vd->videodevice, 12, "%s", device);
    printf("video %s \n", vd->videodevice);
    vd->capAVI = FALSE;
    vd->AVIFName = DEFAULT_AVI_FNAME;
    vd->fps = fps;
    vd->getPict = 0;
    vd->signalquit = 1;
    vd->width = width;
    vd->height = height;
    vd->formatIn = format;
    vd->grabmethod = grabmethod;
    vd->fileCounter = 0;
    vd->rawFrameCapture = 0;
    vd->rfsBytesWritten = 0;
    vd->rfsFramesWritten = 0;
    vd->captureFile = NULL;
    vd->bytesWritten = 0;
    vd->framesWritten = 0;
	vd->signalquit=1;
	vd->capImage=FALSE;
	vd->ImageFName=DEFAULT_IMAGE_FNAME;
	vd->timecode.type = V4L2_TC_TYPE_25FPS;
	vd->timecode.flags = V4L2_TC_FLAG_DROPFRAME;
	
    if (init_v4l2(vd) < 0) {
	printf(" Init v4L2 failed !! exit fatal \n");
	goto error;;
    }
    /* alloc a temp buffer to reconstruct the pict */
    vd->framesizeIn = (vd->width * vd->height << 1);
    switch (vd->formatIn) {
    case V4L2_PIX_FMT_MJPEG:
	vd->tmpbuffer =
	    (unsigned char *) calloc(1, (size_t) vd->framesizeIn);
	if (!vd->tmpbuffer)
	    goto error;
	vd->framebuffer =
	    (unsigned char *) calloc(1,
				     (size_t) vd->width * (vd->height +
							   8) * 2);
	break;
    case V4L2_PIX_FMT_YUYV:
	vd->framebuffer =
	    (unsigned char *) calloc(1, (size_t) vd->framesizeIn);
	break;
    default:
	printf(" should never arrive exit fatal !!\n");
	goto error;
	break;
    }
    if (!vd->framebuffer)
	goto error;
    return 0;
  error:
    free(vd->videodevice);
    free(vd->status);
    free(vd->pictName);
    close(vd->fd);
    return -1;
}

//~ int change_format(struct vdIn *vd)
//~ {
	//~ if (vd->fd > 0) {
        //~ close(vd->fd);
    //~ }
	
	//~ /* set format in */
    //~ memset(&vd->fmt, 0, sizeof(struct v4l2_format));
    //~ vd->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    //~ vd->fmt.fmt.pix.width = vd->width;
    //~ vd->fmt.fmt.pix.height = vd->height;
    //~ vd->fmt.fmt.pix.pixelformat = vd->formatIn;
    //~ vd->fmt.fmt.pix.field = V4L2_FIELD_ANY;
    //~ int ret = ioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt);
    //~ if (ret < 0) {
	//~ printf("Unable to set format: %d.\n", errno);
	//~ return -1;
    //~ }
    //~ if ((vd->fmt.fmt.pix.width != vd->width) ||
	//~ (vd->fmt.fmt.pix.height != vd->height)) {
	//~ printf(" format asked unavailable get width %d height %d \n",
	       //~ vd->fmt.fmt.pix.width, vd->fmt.fmt.pix.height);
	//~ vd->width = vd->fmt.fmt.pix.width;
	//~ vd->height = vd->fmt.fmt.pix.height;
	//~ /* look the format is not part of the deal ??? */
	//~ //vd->formatIn = vd->fmt.fmt.pix.pixelformat;
    //~ }
	//~ return 0;
//~ }


static int init_v4l2(struct vdIn *vd)
{
    int i;
    int ret = 0;
    
	//~ if (vd->fd > 0) {
        //~ close(vd->fd);
    //~ }
	
	if (vd->fd <=0 ){
	  if ((vd->fd = open(vd->videodevice, O_RDWR )) == -1) {
	    perror("ERROR opening V4L interface \n");
	    exit(1);
      }
    }

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
	/* look the format is not part of the deal ??? */
	//vd->formatIn = vd->fmt.fmt.pix.pixelformat;
    }
	
	vd->streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->streamparm.parm.capture.timeperframe.numerator = 1;
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
    for (i = 0; i < NB_BUFFER; i++) {
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
	    goto fatal;
	}
	if (debug)
	    printf("length: %u offset: %u\n", vd->buf.length,
		   vd->buf.m.offset);
	vd->mem[i] = mmap(0 /* start anywhere */ ,
			  vd->buf.length, PROT_READ, MAP_SHARED, vd->fd,
			  vd->buf.m.offset);
	if (vd->mem[i] == MAP_FAILED) {
	    printf("Unable to map buffer (%d)\n", errno);
	    goto fatal;
	}
	if (debug)
	    printf("Buffer mapped at address %p.\n", vd->mem[i]);
    }
    /* Queue the buffers. */
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
	    goto fatal;;
	}
    }
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

	/* Capture a single raw frame */
	if (vd->rawFrameCapture && vd->buf.bytesused > 0) {
		FILE *frame = NULL;
		char filename[13];
		int ret;

		/* Disable frame capturing unless we're in frame stream mode */
		if(vd->rawFrameCapture == 1)
			vd->rawFrameCapture = 0;

		/* Create a file name and open the file */
		sprintf(filename, "frame%03u.raw", vd->fileCounter++ % 1000);
		frame = fopen(filename, "wb");
		if(frame == NULL) {
			perror("Unable to open file for raw frame capturing");
			goto end_capture;
		}
		
		/* Write the raw data to the file */
		ret = fwrite(vd->mem[vd->buf.index], vd->buf.bytesused, 1, frame);
		if(ret < 1) {
			perror("Unable to write to file");
			goto end_capture;
		}
		printf("Saved raw frame to %s (%u bytes)\n", filename, vd->buf.bytesused);
		if(vd->rawFrameCapture == 2) {
			vd->rfsBytesWritten += vd->buf.bytesused;
			vd->rfsFramesWritten++;
		}


		/* Clean up */
end_capture:
		if(frame)
			fclose(frame);
	}

	/* Capture raw stream data */
	if (vd->captureFile && vd->buf.bytesused > 0) {
		int ret;
		ret = fwrite(vd->mem[vd->buf.index], vd->buf.bytesused, 1, vd->captureFile);
		if (ret < 1) {
			perror("Unable to write raw stream to file");
			fprintf(stderr, "Stream capturing terminated.\n");
			fclose(vd->captureFile);
			vd->captureFile = NULL;
			vd->framesWritten = 0;
			vd->bytesWritten = 0;
		} else {
			vd->framesWritten++;
			vd->bytesWritten += vd->buf.bytesused;
			if (debug)
				printf("Appended raw frame to stream file (%u bytes)\n", vd->buf.bytesused);
		}
	}

    switch (vd->formatIn) {
    case V4L2_PIX_FMT_MJPEG:
        if(vd->buf.bytesused <= HEADERFRAME1) {	/* Prevent crash on empty image */
/*	    if(debug)*/
	        printf("Ignoring empty buffer ...\n");
	    return 0;
        }
	memcpy(vd->tmpbuffer, vd->mem[vd->buf.index],vd->buf.bytesused);
	if (jpeg_decode(&vd->framebuffer, vd->tmpbuffer, &vd->width,
	     &vd->height) < 0) {
	    printf("jpeg decode errors\n");
	    goto err;
	}
	if (debug)
	    printf("bytes in used %d \n", vd->buf.bytesused);
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
    ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
    if (ret < 0) {
	printf("Unable to requeue buffer (%d).\n", errno);
	goto err;
    }

    return 0;
  err:
    vd->signalquit = 0;
    return -1;
}

void close_v4l2(struct vdIn *vd)
{
    if (vd->isstreaming)
	video_disable(vd);
    if (vd->tmpbuffer)
	free(vd->tmpbuffer);
    vd->tmpbuffer = NULL;
    free(vd->framebuffer);
    vd->framebuffer = NULL;
    free(vd->videodevice);
    free(vd->status);
    free(vd->pictName);
    vd->videodevice = NULL;
    vd->status = NULL;
    vd->pictName = NULL;
}


//~ int v4l2ResetControl(struct vdIn *vd, int control)
//~ {
    //~ struct v4l2_control control_s;
    //~ struct v4l2_queryctrl queryctrl;
    //~ int val_def;
    //~ int err;
    //~ if (isv4l2Control(vd, control, &queryctrl) < 0)
	//~ return -1;
    //~ val_def = queryctrl.default_value;
    //~ control_s.id = control;
    //~ control_s.value = val_def;
    //~ if ((err = ioctl(vd->fd, VIDIOC_S_CTRL, &control_s)) < 0) {
	//~ printf("ioctl reset control error\n");
	//~ return -1;
    //~ }

    //~ return 0;
//~ }
//~ int v4l2ResetPanTilt(struct vdIn *vd,int pantilt)
//~ {
    //~ int control = V4L2_CID_PANTILT_RESET;
    //~ struct v4l2_control control_s;
    //~ struct v4l2_queryctrl queryctrl;
    //~ unsigned char val;
    //~ int err;
    //~ if (isv4l2Control(vd, control, &queryctrl) < 0)
	//~ return -1;
    //~ val = (unsigned char) pantilt;
    //~ control_s.id = control;
    //~ control_s.value = val;
    //~ if ((err = ioctl(vd->fd, VIDIOC_S_CTRL, &control_s)) < 0) {
	//~ printf("ioctl reset Pan control error\n");
	//~ return -1;
    //~ }

    //~ return 0;
//~ }
//~ union pantilt {
	//~ struct {
		//~ short pan;
		//~ short tilt;
	//~ } s16;
	//~ int value;
//~ } pantilt;
	
//~ int v4L2UpDownPan(struct vdIn *vd, short inc)
//~ {   int control = V4L2_CID_PANTILT_RELATIVE;
    //~ struct v4l2_control control_s;
    //~ struct v4l2_queryctrl queryctrl;
    //~ int err;
    
   //~ union pantilt pan;
   
       //~ control_s.id = control;
     //~ if (isv4l2Control(vd, control, &queryctrl) < 0)
        //~ return -1;

  //~ pan.s16.pan = inc;
  //~ pan.s16.tilt = 0;
 
	//~ control_s.value = pan.value ;
    //~ if ((err = ioctl(vd->fd, VIDIOC_S_CTRL, &control_s)) < 0) {
	//~ printf("ioctl pan updown control error\n");
	//~ return -1;
	//~ }
	//~ return 0;
//~ }

//~ int v4L2UpDownTilt(struct vdIn *vd, short inc)
//~ {   int control = V4L2_CID_PANTILT_RELATIVE;
    //~ struct v4l2_control control_s;
    //~ struct v4l2_queryctrl queryctrl;
    //~ int err;
     //~ union pantilt pan;  
       //~ control_s.id = control;
     //~ if (isv4l2Control(vd, control, &queryctrl) < 0)
	//~ return -1;  

    //~ pan.s16.pan= 0;
    //~ pan.s16.tilt = inc;
  
	//~ control_s.value = pan.value;
    //~ if ((err = ioctl(vd->fd, VIDIOC_S_CTRL, &control_s)) < 0) {
	//~ printf("ioctl tiltupdown control error\n");
	//~ return -1;
	//~ }
	//~ return 0;
//~ }


int
input_get_control (struct vdIn * device, InputControl * control, int * val)
{
    int fd, ret;
    struct v4l2_control c;

    //~ if (device->fd < 0) {
        //~ fd = open (device->videodevice, O_RDWR | O_NONBLOCK, 0);
        //~ if (fd < 0)
            //~ return -1;
    //~ }
    //~ else {
        fd = device->fd;
    //~ }

    c.id  = control->id;
    ret = ioctl (fd, VIDIOC_G_CTRL, &c);
    if (ret == 0)
        *val = c.value;

    //~ if (device->fd < 0)
        //~ close(fd);

    return ret;
}

int
input_set_control (struct vdIn * device, InputControl * control, int val)
{
   
    int fd, ret;
    struct v4l2_control c;

    //~ if (device->fd < 0) {
        //~ fd = open (device->videodevice, O_RDWR | O_NONBLOCK, 0);
        //~ if (fd < 0)
            //~ return -1;
    //~ }
    //~ else {
        fd = device->fd;
    //~ }

    c.id  = control->id;
    c.value = val;
    ret = ioctl (fd, VIDIOC_S_CTRL, &c);

    //~ if (device->fd < 0)
        //~ close(fd);

    return ret;
}

int
input_set_framerate (struct vdIn * device, int fps)
{
   
	int fd, ret;

    //~ if (device->fd < 0) {
        //~ fd = open (device->videodevice, O_RDWR | O_NONBLOCK, 0);
        //~ if (fd < 0)
            //~ return -1;
    //~ }
    //~ else {
	fd = device->fd;
    //~ }
	device->streamparm.parm.capture.timeperframe.numerator = 1;
	device->streamparm.parm.capture.timeperframe.denominator = fps;
	 
	ret = ioctl(fd,VIDIOC_S_PARM,&device->streamparm);
	if (ret < 0) {
		printf("Unable to set timeperframe: %d.\n", errno);
	} else {
		device->fps = fps; 
	}		

    //~ if (device->fd < 0)
        //~ close(fd);

	return ret;
}

int
input_get_framerate (struct vdIn * device)
{
   
    int fd, ret, fps;

    //~ if (device->fd < 0) {
        //~ fd = open (device->videodevice, O_RDWR | O_NONBLOCK, 0);
        //~ if (fd < 0)
            //~ return -1;
    //~ }
    //~ else {
        fd = device->fd;
    //~ }
    
	ret = ioctl(fd,VIDIOC_G_PARM,&device->streamparm);
	if (ret < 0) {
	printf("Unable to get timeperframe: %d.\n", errno);
	} else {
		/*it seems numerator is allways 1*/
	 fps = device->streamparm.parm.capture.timeperframe.denominator; 
	 device->fps=fps;	
	}		

    //~ if (device->fd < 0)
        //~ close(fd);

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

    //~ if (device->fd < 0) {
        //~ fd = open (device->videodevice, O_RDWR | O_NONBLOCK, 0);
        //~ if (fd < 0)
            //~ return NULL;
    //~ }
    //~ else {
        fd = device->fd;
    //~ }
    
    i = V4L2_CID_BASE; /* as defined by V4L2 */
    while (i <= V4L2_CID_PRIVATE_LAST) {  /* as defined by the UVC driver */
        queryctrl.id = i;
        if (ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl) == 0 &&
                !(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
            //printf ("%x %d %s %d\n", queryctrl.id, queryctrl.type, queryctrl.name, queryctrl.flags);
            control = realloc (control, (n+1)*sizeof (InputControl));
            control[n].i = n;
            control[n].id = queryctrl.id;
            control[n].type = queryctrl.type;
            control[n].name = strdup ((char *)queryctrl.name);
            //printf ("%s\n", control[n].name);
            control[n].min = queryctrl.minimum;
            control[n].max = queryctrl.maximum;
            control[n].step = queryctrl.step;
            control[n].default_val = queryctrl.default_value;
            control[n].enabled = (queryctrl.flags & V4L2_CTRL_FLAG_GRABBED) ? 0 : 1;
            control[n].entries = NULL;
            if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
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
        //else if (i >= V4L2_CID_PRIVATE_LAST) {
          //  break;
        //}
        i++;
       if (i == V4L2_CID_LASTP1)  /* jumps from last V4L2 defined control to first UVC driver defined control */
       		i = V4L2_CID_PRIVATE_BASE;
    }

    *num_controls = n;
		
    //~ if (device->fd < 0)
        //~ close(fd);

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
}
