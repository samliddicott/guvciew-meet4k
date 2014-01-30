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
#include <stdio.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "v4l2_formats.h"

extern int verbosity;

/*
 * enumerate frame intervals (fps)
 * args:
 *   vd - pointer to video device data
 *   pixfmt - v4l2 pixel format that we want to list frame intervals for
 *   width - video width that we want to list frame intervals for
 *   height - video height that we want to list frame intervals for
 *   fmtind - current index of format list
 *   fsizeind - current index of frame size list
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *   vd->list_stream_formats is not null
 *   fmtind is valid
 *   vd->list_stream_formats->list_stream_cap is not null
 *   fsizeind is valid
 *
 * returns 0 if enumeration succeded or errno otherwise
 */
static int enum_frame_intervals(v4l2_dev* vd,
		uint32_t pixfmt, uint32_t width, uint32_t height,
		int fmtind, int fsizeind)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->fd > 0);
	assert(vd->list_stream_formats != NULL);
	assert(vd->numb_formats >= fmtind);
	assert(vd->list_stream_formats->list_stream_cap != NULL);
	assert(vd->list_stream_formats[fmtind-1].numb_res >= fsizeind);

	int ret=0;
	struct v4l2_frmivalenum fival;
	int list_fps=0;
	memset(&fival, 0, sizeof(fival));
	fival.index = 0;
	fival.pixel_format = pixfmt;
	fival.width = width;
	fival.height = height;

	vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_num = NULL;
	vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_denom = NULL;

	if(verbosity > 0)
		printf("\tTime interval between frame: ");
	while ((ret = xioctl(vd->fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0)
	{
		fival.index++;
		if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
		{
			if(verbosity > 0)
				printf("%u/%u, ", fival.discrete.numerator, fival.discrete.denominator);

			list_fps++;
			vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_num = realloc(
				vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_num,
				sizeof(int) * list_fps);
			vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_denom = realloc(
				vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_denom,
				sizeof(int) * list_fps);

			vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_num[list_fps-1] = fival.discrete.numerator;
			vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_denom[list_fps-1] = fival.discrete.denominator;
		}
		else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS)
		{
			if(verbosity > 0)
				printf("{min { %u/%u } .. max { %u/%u } }, ",
					fival.stepwise.min.numerator, fival.stepwise.min.numerator,
					fival.stepwise.max.denominator, fival.stepwise.max.denominator);
			break;
		}
		else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE)
		{
			if(verbosity > 0)
				printf("{min { %u/%u } .. max { %u/%u } / "
					"stepsize { %u/%u } }, ",
					fival.stepwise.min.numerator, fival.stepwise.min.denominator,
					fival.stepwise.max.numerator, fival.stepwise.max.denominator,
					fival.stepwise.step.numerator, fival.stepwise.step.denominator);
			break;
		}
	}

	if (list_fps==0)
	{
		vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].numb_frates = 1;
		vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_num = realloc(
				vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_num,
				sizeof(int));
		vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_denom = realloc(
				vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_denom,
				sizeof(int));

		vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_num[0] = 1;
		vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].framerate_denom[0] = 1;
	}
	else
		vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].numb_frates = list_fps;

	if(verbosity > 0)
		printf("\n");
	if (ret != 0 && errno != EINVAL)
	{
		fprintf(stderr, "V4L2_CORE: (VIDIOC_ENUM_FRAMEINTERVALS) Error enumerating frame intervals\n");
		return errno;
	}
	return 0;
}

/*
 * enumerate frame sizes (width and height)
 * args:
 *   vd - pointer to video device data
 *   pixfmt - v4l2 pixel format that we want to list frame sizes for
 *   fmtind - current index of format list
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *   vd->list_stream_formats is not null
 *   fmtind is valid
 *
 * returns 0 if enumeration succeded or errno otherwise
 */
static int enum_frame_sizes(v4l2_dev* vd, uint32_t pixfmt, int fmtind)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->fd > 0);
	assert(vd->list_stream_formats != NULL);
	assert(vd->numb_formats >= fmtind);

	int ret=0;
	int fsizeind=0; /*index for supported sizes*/
	vd->list_stream_formats[fmtind-1].list_stream_cap = NULL;
	struct v4l2_frmsizeenum fsize;

	memset(&fsize, 0, sizeof(fsize));
	fsize.index = 0;
	fsize.pixel_format = pixfmt;

	while ((ret = xioctl(vd->fd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0)
	{
		fsize.index++;
		if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
		{
			if(verbosity > 0)
				printf("{ discrete: width = %u, height = %u }\n",
					fsize.discrete.width, fsize.discrete.height);

			fsizeind++;
			vd->list_stream_formats[fmtind-1].list_stream_cap = realloc(
				vd->list_stream_formats[fmtind-1].list_stream_cap,
				fsizeind * sizeof(v4l2_stream_cap));

			assert(vd->list_stream_formats[fmtind-1].list_stream_cap != NULL);

			vd->list_stream_formats[fmtind-1].numb_res = fsizeind;

			vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].width = fsize.discrete.width;
			vd->list_stream_formats[fmtind-1].list_stream_cap[fsizeind-1].height = fsize.discrete.height;

			ret = enum_frame_intervals(vd,
				pixfmt,
				fsize.discrete.width,
				fsize.discrete.height,
				fmtind,
				fsizeind);

			if (ret != 0)
				fprintf(stderr, "V4L2_CORE:  Unable to enumerate frame sizes %s\n", strerror(ret));
		}
		else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
		{
			if(verbosity > 0)
			{
				printf("{ continuous: min { width = %u, height = %u } .. "
					"max { width = %u, height = %u } }\n",
					fsize.stepwise.min_width, fsize.stepwise.min_height,
					fsize.stepwise.max_width, fsize.stepwise.max_height);
				printf("  will not enumerate frame intervals.\n");
			}
		}
		else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
		{
			if(verbosity > 0)
			{
				printf("{ stepwise: min { width = %u, height = %u } .. "
					"max { width = %u, height = %u } / "
					"stepsize { width = %u, height = %u } }\n",
					fsize.stepwise.min_width, fsize.stepwise.min_height,
					fsize.stepwise.max_width, fsize.stepwise.max_height,
					fsize.stepwise.step_width, fsize.stepwise.step_height);
				printf("  will not enumerate frame intervals.\n");
			}
		}
		else
		{
			fprintf(stderr, "V4L2_CORE: fsize.type not supported: %d\n", fsize.type);
			fprintf(stderr, "    (Discrete: %d   Continuous: %d  Stepwise: %d)\n",
				V4L2_FRMSIZE_TYPE_DISCRETE,
				V4L2_FRMSIZE_TYPE_CONTINUOUS,
				V4L2_FRMSIZE_TYPE_STEPWISE);
		}
	}

	if (ret != 0 && errno != EINVAL)
	{
		fprintf(stderr, "V4L2_CORE: (VIDIOC_ENUM_FRAMESIZES) - Error enumerating frame sizes\n");
		return errno;
	}
	else if ((ret != 0) && (fsizeind == 0))
	{
		/* ------ some drivers don't enumerate frame sizes ------ */
		/*         negotiate with VIDIOC_TRY_FMT instead          */

		/*if fsizeind = 0 list_stream_cap shouldn't have been allocated*/
		assert(vd->list_stream_formats[fmtind-1].list_stream_cap == NULL);

		fsizeind++;
		struct v4l2_format fmt;
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.width = vd->format.fmt.pix.width; /* check defined size*/
		fmt.fmt.pix.height = vd->format.fmt.pix.height;
		fmt.fmt.pix.pixelformat = pixfmt;
		fmt.fmt.pix.field = V4L2_FIELD_ANY;

		ret = xioctl(vd->fd, VIDIOC_TRY_FMT, &fmt);

		/*use the returned values*/
		int width = fmt.fmt.pix.width;
		int height = fmt.fmt.pix.height;

		if(verbosity > 0)
		{
			printf("{ VIDIOC_TRY_FMT : width = %u, height = %u }\n",
				vd->format.fmt.pix.width,
				vd->format.fmt.pix.height);
			printf("fmtind:%i fsizeind: %i\n", fmtind, fsizeind);
		}

		vd->list_stream_formats[fmtind-1].list_stream_cap = realloc(
			vd->list_stream_formats[fmtind-1].list_stream_cap,
			sizeof(v4l2_stream_cap) * fsizeind);

		assert(vd->list_stream_formats[fmtind-1].list_stream_cap != NULL);

		vd->list_stream_formats[fmtind-1].numb_res=fsizeind;

		/*don't enumerateintervals, use a default of 1/25 fps instead*/
		vd->list_stream_formats[fmtind-1].list_stream_cap[0].framerate_num = NULL;
		vd->list_stream_formats[fmtind-1].list_stream_cap[0].framerate_num = realloc(
			vd->list_stream_formats[fmtind-1].list_stream_cap[0].framerate_num,
			sizeof(int));

		vd->list_stream_formats[fmtind-1].list_stream_cap[0].framerate_denom = NULL;
		vd->list_stream_formats[fmtind-1].list_stream_cap[0].framerate_denom = realloc(
			vd->list_stream_formats[fmtind-1].list_stream_cap[0].framerate_denom,
			sizeof(int));

		vd->list_stream_formats[fmtind-1].list_stream_cap[0].width = width;
		vd->list_stream_formats[fmtind-1].list_stream_cap[0].height = height;
		vd->list_stream_formats[fmtind-1].list_stream_cap[0].framerate_num[0] = 1;
		vd->list_stream_formats[fmtind-1].list_stream_cap[0].framerate_denom[0] = 25;
		vd->list_stream_formats[fmtind-1].list_stream_cap[0].numb_frates = 1;
	}

	return 0;
}

/*
 * enumerate frame formats (pixelformats, resolutions and fps)
 * and creates list in vd->list_stream_formats
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *   vd->list_stream_formats is null
 *
 * returns: 0 if enumeration succeded or errno otherwise
 */
int enum_frame_formats(v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->fd > 0);
	assert(vd->list_stream_formats == NULL);

	int ret=E_OK;

	int fmtind=0;
	struct v4l2_fmtdesc fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	vd->list_stream_formats = calloc ( 1, sizeof(v4l2_stream_formats));
	vd->list_stream_formats[0].list_stream_cap = NULL;

	while ((ret = xioctl(vd->fd, VIDIOC_ENUM_FMT, &fmt)) == 0)
	{
		fmt.index++;
		if(verbosity > 0)
			printf("{ pixelformat = '%c%c%c%c', description = '%s' }\n",
				fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,
				(fmt.pixelformat >> 16) & 0xFF, (fmt.pixelformat >> 24) & 0xFF,
				fmt.description);

		fmtind++;

		vd->list_stream_formats = realloc(
			vd->list_stream_formats,
			fmtind * sizeof(v4l2_stream_formats));

		assert(vd->list_stream_formats != NULL);

		vd->numb_formats = fmtind;

		vd->list_stream_formats[fmtind-1].format = fmt.pixelformat;
		snprintf(vd->list_stream_formats[fmtind-1].fourcc, 5, "%c%c%c%c",
				fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,
				(fmt.pixelformat >> 16) & 0xFF, (fmt.pixelformat >> 24) & 0xFF);
		//enumerate frame sizes
		ret = enum_frame_sizes(vd, fmt.pixelformat, fmtind);
		if (ret != 0)
			fprintf( stderr, "v4L2_CORE: Unable to enumerate frame sizes :%s\n", strerror(ret));
	}

	if (errno != EINVAL)
		fprintf( stderr, "v4L2_CORE: (VIDIOC_ENUM_FMT) - Error enumerating frame formats: %s\n", strerror(errno));


	return (ret);
}

/* get frame format index from format list
 * args:
 *   vd - pointer to video device data
 *   format - v4l2 pixel format
 *
 * asserts:
 *   vd is not null
 *   vd->list_stream_formats is not null
 *
 * returns: format list index or -1 if not available
 */
int get_frame_format_index(v4l2_dev* vd, int format)
{
	/*asserts*/
	assert(vd != NULL);
	assert(vd->list_stream_formats != NULL);

	int i=0;
	for(i=0; i<vd->numb_formats; i++)
	{
		if(format == vd->list_stream_formats[i].format)
			return (i);
	}
	return (-1);
}

/*
 * free frame formats list
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->list_stream_formats is not null
 *
 * returns: void
 */
void free_frame_formats(v4l2_dev* vd)
{
	/*asserts*/
	assert(vd != NULL);
	assert(vd->list_stream_formats != NULL);

	int i=0;
	int j=0;
	for(i=0; i < vd->numb_formats; i++)
	{
		if(vd->list_stream_formats[i].list_stream_cap != NULL)
		{
			for(j=0; j < vd->list_stream_formats[i].numb_res;j++)
			{
				if(vd->list_stream_formats[i].list_stream_cap[j].framerate_num != NULL)
					free(vd->list_stream_formats[i].list_stream_cap[j].framerate_num);

				if(vd->list_stream_formats[i].list_stream_cap[j].framerate_denom != NULL)
					free(vd->list_stream_formats[i].list_stream_cap[j].framerate_denom);
			}
			free(vd->list_stream_formats[i].list_stream_cap);
		}
	}
	free(vd->list_stream_formats);
	vd->list_stream_formats = NULL;
}