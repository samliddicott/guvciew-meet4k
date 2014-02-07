/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#                             Add VU meter OSD                                  #
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
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "gui.h"
#include "gui_gtk3.h"
#include "video_capture.h"

extern int debug_level;

static int gui_api = GUI_GTK3;

/*default camera button action: DEF_ACTION_IMAGE - save image; DEF_ACTION_VIDEO - save video*/
static int default_camera_button_action = 0;

/*control profile file name*/
static char *profile_name = NULL;

/*control profile path to dir*/
static char *profile_path = NULL;

/*
 * gets the default camera button action
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: default camera button action
 */
int get_default_camera_button_action()
{
	return default_camera_button_action;
}

/*
 * sets the default camera button action
 * args:
 *   action: camera button default action
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_default_camera_button_action(int action)
{
	default_camera_button_action = action;
}

/*
 * gets the control profile file name
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: control profile file name
 */
const char *get_profile_name()
{
	return profile_name;
}

/*
 * sets the control profile file name
 * args:
 *   name: control profile file name
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_profile_name(const char *name)
{
	if(profile_name != NULL)
		free(profile_name);

	profile_name = strdup(name);
}

/*
 * gets the control profile path (to dir)
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: control profile file name
 */
const char *get_profile_path()
{
	return profile_path;
}

/*
 * sets the control profile path (to dir)
 * args:
 *   path: control profile path
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_profile_path(const char *path)
{
	if(profile_path != NULL)
		free(profile_path);

	profile_path = strdup(path);
}

/*
 * GUI initialization
 * args:
 *   device - pointer to device data we want to attach the gui for
 *   gui - gui API to use (GUI_NONE, GUI_GTK3, ...)
 *   width - window width
 *   height - window height
 *
 * asserts:
 *   device is not null
 *
 * returns: error code
 */
int gui_attach(v4l2_dev_t *device, int gui, int width, int height)
{
	/*asserts*/
	assert(device != NULL);

	int ret = 0;

	gui_api = gui;

	switch(gui_api)
	{
		case GUI_NONE:
			break;

		case GUI_GTK3:
		default:
			ret = gui_attach_gtk3(device, width, height);
			if(ret)
				gui_api = GUI_NONE;
			break;
	}

	return ret;
}

/*
 * run the GUI loop
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int gui_run()
{

	int ret = 0;

	switch(gui_api)
	{
		case GUI_NONE:
			break;

		case GUI_GTK3:
		default:
			ret = gui_run_gtk3();
			break;
	}

	return ret;
}

/*
 * closes and cleans the GUI
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void gui_close()
{
	if(profile_name != NULL)
		free(profile_name);
	profile_name = NULL;
	if(profile_path != NULL)
		free(profile_path);
	profile_path = NULL;

	switch(gui_api)
	{
		case GUI_NONE:
			break;

		case GUI_GTK3:
		default:
			gui_close_gtk3();
			break;
	}
	quit_callback(NULL);
}