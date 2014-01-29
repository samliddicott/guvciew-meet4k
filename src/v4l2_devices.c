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

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <libv4l2.h>
#include <linux/videodev2.h>

#include "v4l2uvc.h"
#include "v4l2_devices.h"

/* (fall through - DEPRECATED)
 * enumerates system video devices
 * by checking /sys/class/video4linux
 * args:
 * videodevice: current device string (default "/dev/video0")
 *
 * returns: pointer to LDevices struct containing the video devices list */
LDevices *list_devices( gchar *videodevice )
{
	int ret=0;
	int fd=0;
	LDevices *listDevices = NULL;
	listDevices = g_new0( LDevices, 1);
	listDevices->listVidDevices = NULL;
	struct v4l2_capability v4l2_cap;
	GDir *v4l2_dir=NULL;
	GError *error=NULL;

	v4l2_dir = g_dir_open("/sys/class/video4linux",0,&error);
	if(v4l2_dir == NULL)
	{
		g_printerr ("opening '/sys/class/video4linux' failed: %s\n",
			 error->message);
		g_error_free ( error );
		error=NULL;
		g_free(listDevices);
		return NULL;
	}
	const gchar *v4l2_device;
	int num_dev = 0;

	while((v4l2_device = g_dir_read_name(v4l2_dir)) != NULL)
	{
		if(!(g_str_has_prefix(v4l2_device, "video")))
			continue;
		gchar *device = NULL;
		device = g_strjoin("/","/dev",v4l2_device,NULL);

		if ((fd = v4l2_open(device, O_RDWR | O_NONBLOCK, 0)) < 0)
		{
			g_printerr("ERROR opening V4L interface for %s\n",
				device);
			g_free(device);
			continue; /*next dir entry*/
		}
		else
		{
			ret = xioctl(fd, VIDIOC_QUERYCAP, &v4l2_cap);
			if (ret < 0)
			{
				perror("VIDIOC_QUERYCAP error");
				g_printerr("   couldn't query device %s\n",
					device);
				g_free(device);
				v4l2_close(fd);
				continue; /*next dir entry*/
			}
			else
			{
				num_dev++;
				g_print("%s - device %d\n", device, num_dev);
				listDevices->listVidDevices = g_renew(VidDevice,
					listDevices->listVidDevices,
					num_dev);
				listDevices->listVidDevices[num_dev-1].device = g_strdup(device);
				listDevices->listVidDevices[num_dev-1].name = g_strdup((gchar *) v4l2_cap.card);
				listDevices->listVidDevices[num_dev-1].driver = g_strdup((gchar *) v4l2_cap.driver);
				listDevices->listVidDevices[num_dev-1].location = g_strdup((gchar *) v4l2_cap.bus_info);
				listDevices->listVidDevices[num_dev-1].valid = 1;
				if(g_strcmp0(videodevice,listDevices->listVidDevices[num_dev-1].device)==0)
				{
					listDevices->listVidDevices[num_dev-1].current = 1;
					listDevices->current_device = num_dev-1;
				}
				else
					listDevices->listVidDevices[num_dev-1].current = 0;
			}
		}
		g_free(device);
		v4l2_close(fd);

		listDevices->listVidDevices[num_dev-1].vendor = 0;
		listDevices->listVidDevices[num_dev-1].product = 0;

		gchar *vid_dev_lnk = g_strjoin("/","/sys/class/video4linux",v4l2_device,"device",NULL);
		gchar *device_lnk = g_file_read_link (vid_dev_lnk,&error);
		g_free(vid_dev_lnk);

		if(device_lnk == NULL)
		{
			g_printerr ("reading link '/sys/class/video4linux/%s/device' failed: %s\n",
				v4l2_device,
				error->message);
			g_error_free ( error );
			error=NULL;
			//if standard way fails try to get vid, pid from uvc device name
			//we only need this info for Dynamic controls - uvc driver
			listDevices->listVidDevices[num_dev-1].vendor = 0;  /*reset vid */
			listDevices->listVidDevices[num_dev-1].product = 0; /*reset pid */
			if(g_strcmp0(listDevices->listVidDevices[num_dev-1].driver,"uvcvideo") == 0)
			{
				sscanf(listDevices->listVidDevices[num_dev-1].name,"UVC Camera (%04x:%04x)",
					&(listDevices->listVidDevices[num_dev-1].vendor),
					&(listDevices->listVidDevices[num_dev-1].product));
			}
		}
		else
		{
			gchar *d_dir = g_strjoin("/","/sys/class/video4linux", v4l2_device, device_lnk, NULL);
			gchar *id_dir = g_path_get_dirname(d_dir);
			g_free(d_dir);

			gchar *idVendor = g_strjoin("/", id_dir, "idVendor" ,NULL);
			gchar *idProduct = g_strjoin("/", id_dir, "idProduct" ,NULL);

			//g_print("idVendor: %s\n", idVendor);
			//g_print("idProduct: %s\n", idProduct);
			FILE *vid_fp = g_fopen(idVendor,"r");
			if(vid_fp != NULL)
			{
				gchar code[5];
				if(fgets(code, sizeof(code), vid_fp) != NULL)
				{
					listDevices->listVidDevices[num_dev-1].vendor = g_ascii_strtoull(code, NULL, 16);
				}
				fclose (vid_fp);
			}
			else
			{
				g_printerr("couldn't open idVendor: %s\n", idVendor);
			}

			vid_fp = g_fopen(idProduct,"r");
			if(vid_fp != NULL)
			{
				gchar code[5];
				if(fgets(code, sizeof(code), vid_fp) != NULL)
				{
					listDevices->listVidDevices[num_dev-1].product = g_ascii_strtoull(code, NULL, 16);
				}
				fclose (vid_fp);
			}
			else
			{
				g_printerr("couldn't open idProduct: %s\n", idProduct);
			}

			g_free(id_dir);
			g_free(idVendor);
			g_free(idProduct);
		}

		g_free(device_lnk);
	}

	if(v4l2_dir != NULL) g_dir_close(v4l2_dir);

	listDevices->num_devices = num_dev;
	return(listDevices);
}

/* enumerates v4l2 devices
 * by using libudev
 args:
 * videodevice: current device string (default "/dev/video0")
 *
 * returns: pointer to LDevices struct containing the video devices list */

LDevices *enum_devices( gchar *videodevice, struct udev *udev, int debug)
{
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev;

    int num_dev = 0;
    int fd = 0;
    struct v4l2_capability v4l2_cap;

    if (!udev)
    {
        /*use fall through method (sysfs)*/
        g_print("Can't create udev...using sysfs method\n");
        return(list_devices(videodevice));
    }

    LDevices *listDevices = NULL;
    listDevices = g_new0( LDevices, 1);
    listDevices->listVidDevices = NULL;

    /* Create a list of the devices in the 'v4l2' subsystem. */
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "video4linux");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    /* For each item enumerated, print out its information.
        udev_list_entry_foreach is a macro which expands to
        a loop. The loop will be executed for each member in
        devices, setting dev_list_entry to a list entry
        which contains the device's path in /sys. */
    udev_list_entry_foreach(dev_list_entry, devices)
    {
        const char *path;

        /* Get the filename of the /sys entry for the device
            and create a udev_device object (dev) representing it */
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);

        /* usb_device_get_devnode() returns the path to the device node
            itself in /dev. */
        const gchar *v4l2_device = udev_device_get_devnode(dev);
        if (debug)
            g_print("Device Node Path: %s\n", v4l2_device);

        /* open the device and query the capabilities */
        if ((fd = v4l2_open(v4l2_device, O_RDWR | O_NONBLOCK, 0)) < 0)
        {
            g_printerr("ERROR opening V4L2 interface for %s\n", v4l2_device);
            v4l2_close(fd);
            continue; /*next dir entry*/
        }

        if (xioctl(fd, VIDIOC_QUERYCAP, &v4l2_cap) < 0)
        {
            perror("VIDIOC_QUERYCAP error");
            g_printerr("   couldn't query device %s\n", v4l2_device);
            v4l2_close(fd);
            continue; /*next dir entry*/
        }
        v4l2_close(fd);

        num_dev++;
        /* Update the device list*/
        listDevices->listVidDevices = g_renew(VidDevice,
            listDevices->listVidDevices,
            num_dev);
        listDevices->listVidDevices[num_dev-1].device = g_strdup(v4l2_device);
        listDevices->listVidDevices[num_dev-1].name = g_strdup((gchar *) v4l2_cap.card);
        listDevices->listVidDevices[num_dev-1].driver = g_strdup((gchar *) v4l2_cap.driver);
        listDevices->listVidDevices[num_dev-1].location = g_strdup((gchar *) v4l2_cap.bus_info);
        listDevices->listVidDevices[num_dev-1].valid = 1;
        if(g_strcmp0(videodevice,listDevices->listVidDevices[num_dev-1].device)==0)
        {
            listDevices->listVidDevices[num_dev-1].current = 1;
            listDevices->current_device = num_dev-1;
        }
        else
            listDevices->listVidDevices[num_dev-1].current = 0;

        /* The device pointed to by dev contains information about
            the v4l2 device. In order to get information about the
            USB device, get the parent device with the
            subsystem/devtype pair of "usb"/"usb_device". This will
            be several levels up the tree, but the function will find
            it.*/
        dev = udev_device_get_parent_with_subsystem_devtype(
                dev,
                "usb",
                "usb_device");
        if (!dev)
        {
            printf("Unable to find parent usb device.");
            continue;
        }

        /* From here, we can call get_sysattr_value() for each file
            in the device's /sys entry. The strings passed into these
            functions (idProduct, idVendor, serial, etc.) correspond
            directly to the files in the directory which represents
            the USB device. Note that USB strings are Unicode, UCS2
            encoded, but the strings returned from
            udev_device_get_sysattr_value() are UTF-8 encoded. */
        if (debug)
        {
            g_print("  VID/PID: %s %s\n",
                udev_device_get_sysattr_value(dev,"idVendor"),
                udev_device_get_sysattr_value(dev, "idProduct"));
            g_print("  %s\n  %s\n",
                udev_device_get_sysattr_value(dev,"manufacturer"),
                udev_device_get_sysattr_value(dev,"product"));
            g_print("  serial: %s\n",
                udev_device_get_sysattr_value(dev, "serial"));
            g_print("  busnum: %s\n",
                udev_device_get_sysattr_value(dev, "busnum"));
            g_print("  devnum: %s\n",
                udev_device_get_sysattr_value(dev, "devnum"));
        }

        listDevices->listVidDevices[num_dev-1].vendor = g_ascii_strtoull(udev_device_get_sysattr_value(dev,"idVendor"), NULL, 16);
        listDevices->listVidDevices[num_dev-1].product = g_ascii_strtoull(udev_device_get_sysattr_value(dev, "idProduct"), NULL, 16);
        listDevices->listVidDevices[num_dev-1].busnum = g_ascii_strtoull(udev_device_get_sysattr_value(dev, "busnum"), NULL, 16);
		listDevices->listVidDevices[num_dev-1].devnum = g_ascii_strtoull(udev_device_get_sysattr_value(dev, "devnum"), NULL, 16);

        udev_device_unref(dev);
    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);

    listDevices->num_devices = num_dev;
    return(listDevices);

}

/*clean video devices list
 * args: listVidDevices: array of VidDevice (list of video devices)
 * numb_devices: number of existing supported video devices
 *
 * returns: void
 */
void freeDevices(LDevices *listDevices)
{
	int i=0;

	for(i=0;i<(listDevices->num_devices);i++)
	{
		g_free(listDevices->listVidDevices[i].device);
		g_free(listDevices->listVidDevices[i].name);
		g_free(listDevices->listVidDevices[i].driver);
		g_free(listDevices->listVidDevices[i].location);
	}
	g_free(listDevices->listVidDevices);
	g_free(listDevices);
}
