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
#include "utils.h"

int check_videoIn(struct vdIn *vd)
{
int ret;
 if (vd == NULL)
	return -1;

    if ((vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
	printf("Error opening device %s: video capture not supported.\n",
	       vd->videodevice);
    }
    if (!(vd->cap.capabilities & V4L2_CAP_STREAMING)) {
	    printf("%s does not support streaming i/o\n", vd->videodevice);
    }
    if (!(vd->cap.capabilities & V4L2_CAP_READWRITE)) {
	    printf("%s does not support read i/o\n", vd->videodevice);
    }
    enum_frame_formats(vd);
    return 0;
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
	vd->Imgtype=IMGTYPE;
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
    vd->AVIFName = DEFAULT_AVI_FNAME;
    vd->fps = fps;
	vd->fps_num = fps_num;
    vd->signalquit = 1;
    vd->width = width;
    vd->height = height;
    vd->formatIn = format;
    vd->grabmethod = grabmethod;
	vd->capImage=FALSE;
	vd->ImageFName=DEFAULT_IMAGE_FNAME;
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
	vd->framebuffer =
	    (unsigned char *) calloc(1,
				     (size_t) vd->width * (vd->height +
							   8) * 2);
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

	//~ /* Capture a single raw frame */
	//~ if (vd->rawFrameCapture && vd->buf.bytesused > 0) {
		//~ FILE *frame = NULL;
		//~ char filename[13];
		//~ int ret;

		//~ /* Disable frame capturing unless we're in frame stream mode */
		//~ if(vd->rawFrameCapture == 1)
			//~ vd->rawFrameCapture = 0;

		//~ /* Create a file name and open the file */
		//~ sprintf(filename, "frame%03u.raw", vd->fileCounter++ % 1000);
		//~ frame = fopen(filename, "wb");
		//~ if(frame == NULL) {
			//~ perror("Unable to open file for raw frame capturing");
			//~ goto end_capture;
		//~ }
		
		//~ /* Write the raw data to the file */
		//~ ret = fwrite(vd->mem[vd->buf.index], vd->buf.bytesused, 1, frame);
		//~ if(ret < 1) {
			//~ perror("Unable to write to file");
			//~ goto end_capture;
		//~ }
		//~ printf("Saved raw frame to %s (%u bytes)\n", filename, vd->buf.bytesused);
		//~ if(vd->rawFrameCapture == 2) {
			//~ vd->rfsBytesWritten += vd->buf.bytesused;
			//~ vd->rfsFramesWritten++;
		//~ }


		//~ /* Clean up */
//~ end_capture:
		//~ if(frame)
			//~ fclose(frame);
	//~ }

	/* Capture raw stream data */
	//~ if (vd->captureFile && vd->buf.bytesused > 0) {
		//~ int ret;
		//~ ret = fwrite(vd->mem[vd->buf.index], vd->buf.bytesused, 1, vd->captureFile);
		//~ if (ret < 1) {
			//~ perror("Unable to write raw stream to file");
			//~ fprintf(stderr, "Stream capturing terminated.\n");
			//~ fclose(vd->captureFile);
			//~ vd->captureFile = NULL;
			//~ vd->framesWritten = 0;
			//~ vd->bytesWritten = 0;
		//~ } else {
			//~ vd->framesWritten++;
			//~ vd->bytesWritten += vd->buf.bytesused;
			//~ if (debug)
				//~ printf("Appended raw frame to stream file (%u bytes)\n", vd->buf.bytesused);
		//~ }
	//~ }

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
    //free(vd->pictName);
    vd->videodevice = NULL;
    vd->status = NULL;
    //vd->pictName = NULL;
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
input_set_framerate (struct vdIn * device, int fps, int fps_num)
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
	device->streamparm.parm.capture.timeperframe.numerator = fps_num;
	device->streamparm.parm.capture.timeperframe.denominator = fps;
	 
	ret = ioctl(fd,VIDIOC_S_PARM,&device->streamparm);
	if (ret < 0) {
		printf("Unable to set timeperframe: %d.\n", errno);
	} else {
		device->fps = fps;
		device->fps_num = fps_num;
	}		

    //~ if (device->fd < 0)
        //~ close(fd);

	return ret;
}

int
input_get_framerate (struct vdIn * device)
{
   
    int fd, ret, fps, fps_num;

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
	 fps_num = device->streamparm.parm.capture.timeperframe.numerator;
	 device->fps=fps;
	 device->fps_num=fps_num;
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
	printf("cleaned allocations - 80%%\n");
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
				printf("%u/%u, ",
						fival.discrete.numerator, fival.discrete.denominator);
				listVidCap[list_form][list_ind].framerate_num[list_fps]=fival.discrete.numerator;
				listVidCap[list_form][list_ind].framerate_denom[list_fps]=fival.discrete.denominator;
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
	listVidCap[list_form][list_ind].numb_frates=list_fps;
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
					list_form=0;
					listVidCap[list_form][list_ind].width=fsize.discrete.width;
					listVidCap[list_form][list_ind].height=fsize.discrete.height;
		/*if this is the selected format set number of resolutions for combobox*/
					if(vd->formatIn == pixfmt) vd->numb_resol=list_ind+1;
					break;
				case V4L2_PIX_FMT_YUYV:
					list_form=1;
					listVidCap[list_form][list_ind].width=fsize.discrete.width;
					listVidCap[list_form][list_ind].height=fsize.discrete.height;
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
