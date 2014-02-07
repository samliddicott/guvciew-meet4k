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
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <glib/gstdio.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <libv4l2.h>
#include <errno.h>

#include "v4l2uvc.h"
#include "string_utils.h"
#include "v4l2_controls.h"
#include "v4l2_dyna_ctrls.h"
#include "callbacks.h"

#ifndef V4L2_CTRL_ID2CLASS
#define V4L2_CTRL_ID2CLASS(id)    ((id) & 0x0fff0000UL)
#endif

/*
 * don't use xioctl for control query when using V4L2_CTRL_FLAG_NEXT_CTRL
 */
static int query_ioctl(int hdevice, int current_ctrl, struct v4l2_queryctrl *ctrl)
{
    int ret = 0;
    int tries = 4;
    do
    {
        if(ret)
            ctrl->id = current_ctrl | V4L2_CTRL_FLAG_NEXT_CTRL;
        ret = v4l2_ioctl(hdevice, VIDIOC_QUERYCTRL, ctrl);
    }
    while (ret && tries-- &&
        ((errno == EIO || errno == EPIPE || errno == ETIMEDOUT)));

    return(ret);
}

gboolean is_special_case_control(int control_id)
{
    switch(control_id)
    {
        case V4L2_CID_PAN_RELATIVE:
        case V4L2_CID_TILT_RELATIVE:
        case V4L2_CID_PAN_RESET:
        case V4L2_CID_TILT_RESET:
        case V4L2_CID_LED1_MODE_LOGITECH:
        case V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH:
            return TRUE;
            break;
        default:
            return FALSE;
            break;
    }
}

static void print_control(Control *control, int i)
{
	int j=0;

	switch (control->control.type)
	{
		case V4L2_CTRL_TYPE_INTEGER:
			g_print("control[%d]:(int) 0x%x '%s'\n",i ,control->control.id, control->control.name);
			g_print ("\tmin:%d max:%d step:%d def:%d curr:%d\n",
				control->control.minimum, control->control.maximum, control->control.step,
				control->control.default_value, control->value);
			break;

#ifdef V4L2_CTRL_TYPE_INTEGER64
		case V4L2_CTRL_TYPE_INTEGER64:
			g_print("control[%d]:(int64) 0x%x '%s'\n",i ,control->control.id, control->control.name);
			g_print ("\tcurr:%" PRIu64 "\n", control->value64);
			break;
#endif
#ifdef V4L2_CTRL_TYPE_STRING
		case V4L2_CTRL_TYPE_STRING:
			g_print("control[%d]:(str) 0x%x '%s'\n",i ,control->control.id, control->control.name);
			g_print ("\tmin:%d max:%d step:%d\n",
				control->control.minimum, control->control.maximum, control->control.step);
			break;
#endif
		case V4L2_CTRL_TYPE_BOOLEAN:
			g_print("control[%d]:(bool) 0x%x '%s'\n",i ,control->control.id, control->control.name);
			g_print ("\tdef:%d curr:%d\n",
				control->control.default_value, control->value);
			break;

		case V4L2_CTRL_TYPE_MENU:
			g_print("control[%d]:(menu) 0x%x '%s'\n",i ,control->control.id, control->control.name);
			g_print("\tmin:%d max:%d def:%d curr:%d\n",
				control->control.minimum, control->control.maximum,
				control->control.default_value, control->value);
			for (j = 0; control->menu[j].index <= control->control.maximum; j++)
				g_print("\tmenu[%d]: [%d] -> '%s'\n", j, control->menu[j].index, control->menu[j].name);
			break;

#ifdef V4L2_CTRL_TYPE_INTEGER_MENU
		case V4L2_CTRL_TYPE_INTEGER_MENU:
			g_print("control[%d]:(intmenu) 0x%x '%s'\n",i ,control->control.id, control->control.name);
			g_print("\tmin:%d max:%d def:%d curr:%d\n",
				control->control.minimum, control->control.maximum,
				control->control.default_value, control->value);
			for (j = 0; control->menu[j].index <= control->control.maximum; j++)
				g_print("\tmenu[%d]: [%d] -> %" PRId64 " (0x%" PRIx64 ")", j, control->menu[j].index,
					(int64_t) control->menu[j].value,
					(int64_t) control->menu[j].value);
			break;
#endif
		case V4L2_CTRL_TYPE_BUTTON:
			g_print("control[%d]:(button) 0x%x '%s'\n",i ,control->control.id, control->control.name);
			break;

#ifdef V4L2_CTRL_TYPE_BITMASK
		case V4L2_CTRL_TYPE_BITMASK:
			g_print("control[%d]:(bitmask) 0x%x '%s'\n",i ,control->control.id, control->control.name);
			g_print("\tmax:%d def:%d curr:%d\n",
				control->control.maximum, control->control.default_value, control->value);
#endif
		default:
			g_print("control[%d]:(unknown - 0x%x) 0x%x '%s'\n",i ,control->control.type,
				control->control.id, control->control.name);
			break;
	}
}

static Control *add_control(int hdevice, struct v4l2_queryctrl *queryctrl, Control **current, Control **first)
{
	Control *control = NULL;
	struct v4l2_querymenu *menu = NULL; //menu list

	if (queryctrl->flags & V4L2_CTRL_FLAG_DISABLED)
	{
		printf("Control 0x%08x is disabled: remove it from control list\n", queryctrl->id);
		return NULL;
	}

	//check menu items if needed
    if(queryctrl->type == V4L2_CTRL_TYPE_MENU
#ifdef V4L2_CTRL_TYPE_INTEGER_MENU
		|| queryctrl->type == V4L2_CTRL_TYPE_INTEGER_MENU
#endif
		)
    {
        int i = 0;
        int ret = 0;
        struct v4l2_querymenu querymenu={0};

        for (querymenu.index = queryctrl->minimum;
            querymenu.index <= queryctrl->maximum;
            querymenu.index++)
        {
            querymenu.id = queryctrl->id;
            ret = xioctl (hdevice, VIDIOC_QUERYMENU, &querymenu);
            if (ret < 0)
                continue;

            if(!menu)
                menu = g_new0(struct v4l2_querymenu, i+1);
            else
                menu = g_renew(struct v4l2_querymenu, menu, i+1);

            memcpy(&(menu[i]), &querymenu, sizeof(struct v4l2_querymenu));
            i++;
        }

        if(!menu)
            menu = g_new0(struct v4l2_querymenu, i+1);
        else
           	menu = g_renew(struct v4l2_querymenu, menu, i+1);

        menu[i].id = querymenu.id;
        menu[i].index = queryctrl->maximum+1;
        if(queryctrl->type == V4L2_CTRL_TYPE_MENU)
			menu[i].name[0] = 0;
    }

    // Add the control to the linked list
    control = calloc (1, sizeof(Control));
    memcpy(&(control->control), queryctrl, sizeof(struct v4l2_queryctrl));
    control->class = V4L2_CTRL_ID2CLASS(control->control.id);
    //add the menu adress (NULL if not a menu)
    control->menu = menu;
#ifdef V4L2_CTRL_TYPE_STRING
    //allocate a string with max size if needed
    if(control->control.type == V4L2_CTRL_TYPE_STRING)
        control->string = (char *) g_strnfill(control->control.maximum + 1, 0);
    else
#endif
        control->string = NULL;

    if(*first != NULL)
    {
        (*current)->next = control;
        *current = control;
    }
    else
    {
		*first = control;
        *current = *first;
    }

    return control;
}

/*
 * returns a Control structure NULL terminated linked list
 * with all of the device controls with Read/Write permissions.
 * These are the only ones that we can store/restore.
 * Also sets num_ctrls with the controls count.
 */
Control *get_control_list(int hdevice, int *num_ctrls, int list_method)
{
    int ret=0;
    Control *first   = NULL;
    Control *current = NULL;

    int n = 0;
    struct v4l2_queryctrl queryctrl={0};

    int currentctrl = 0;
    queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;

	if(list_method == LIST_CTL_METHOD_NEXT_FLAG) //try the next_flag method first
	{
		while ((ret=query_ioctl(hdevice, currentctrl, &queryctrl)) == 0)
		{
			if(add_control(hdevice, &queryctrl, &current, &first) != NULL)
                n++;

            currentctrl = queryctrl.id;

			queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
		}
		if (queryctrl.id != V4L2_CTRL_FLAG_NEXT_CTRL)
		{
			*num_ctrls = n;
			return first; //done
		}

		if(ret)
		{
			printf("Control 0x%08x failed to query with error %i\n", queryctrl.id, ret);
		}

		printf("buggy V4L2_CTRL_FLAG_NEXT_CTRL flag implementation (workaround enabled)\n");

		//next_flag method failed loop through the ids:
		// USER CLASS Controls
		for (currentctrl = V4L2_CID_USER_BASE; currentctrl < V4L2_CID_LASTP1; currentctrl++)
		{
			queryctrl.id = currentctrl;
			if (xioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl) == 0)
			{
				if(add_control(hdevice, &queryctrl, &current, &first) != NULL)
					n++;
			}
		}
		//CAMERA CLASS Controls
		for (currentctrl = V4L2_CID_CAMERA_CLASS_BASE; currentctrl < V4L2_CID_CAMERA_CLASS_BASE+32; currentctrl++)
		{
			queryctrl.id = currentctrl;
			if (xioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl) == 0)
			{
				if(add_control(hdevice, &queryctrl, &current, &first) != NULL)
					n++;
			}
		}
		//PRIVATE controls (deprecated)
		for (queryctrl.id = V4L2_CID_PRIVATE_BASE;
				xioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl) == 0; queryctrl.id++)
		{
			if(add_control(hdevice, &queryctrl, &current, &first) != NULL)
                n++;
		}
	}
	else
	{
		printf("using control id loop method for enumeration \n");
		//next_flag method failed loop through the ids:
		// USER CLASS Controls
		for (currentctrl = V4L2_CID_USER_BASE; currentctrl < V4L2_CID_LASTP1; currentctrl++)
		{
			queryctrl.id = currentctrl;
			if (xioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl) == 0)
			{
				if(add_control(hdevice, &queryctrl, &current, &first) != NULL)
					n++;
			}
		}
		//CAMERA CLASS Controls
		for (currentctrl = V4L2_CID_CAMERA_CLASS_BASE; currentctrl < V4L2_CID_CAMERA_CLASS_BASE+32; currentctrl++)
		{
			queryctrl.id = currentctrl;
			if (xioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl) == 0)
			{
				if(add_control(hdevice, &queryctrl, &current, &first) != NULL)
					n++;
			}
		}
		//PRIVATE controls (deprecated)
		for (queryctrl.id = V4L2_CID_PRIVATE_BASE;
				xioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl) == 0; queryctrl.id++)
		{
			if(add_control(hdevice, &queryctrl, &current, &first) != NULL)
                n++;
		}
	}

    *num_ctrls = n;
    return first;
}

/*
 * called when setting controls
 */
static void update_ctrl_flags(Control *control_list, int id)
{
    switch (id)
    {
        case V4L2_CID_EXPOSURE_AUTO:
            {
                Control *ctrl_this = get_ctrl_by_id(control_list, id );
                if(ctrl_this == NULL)
                    break;

                switch (ctrl_this->value)
                {
                    case V4L2_EXPOSURE_AUTO:
                        {
                            //printf("auto\n");
                            Control *ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_IRIS_ABSOLUTE );
                            if (ctrl_that)
                                ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;

                            ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_IRIS_RELATIVE );
                            if (ctrl_that)
                                ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;
                            ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_EXPOSURE_ABSOLUTE );
                            if (ctrl_that)
                                ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;
                        }
                        break;

                    case V4L2_EXPOSURE_APERTURE_PRIORITY:
                        {
                            //printf("AP\n");
                            Control *ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_EXPOSURE_ABSOLUTE );
                            if (ctrl_that)
                                ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;
                            ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_IRIS_ABSOLUTE );
                            if (ctrl_that)
                                ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                            ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_IRIS_RELATIVE );
                            if (ctrl_that)
                                ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                        }
                        break;

                    case V4L2_EXPOSURE_SHUTTER_PRIORITY:
                        {
                            //printf("SP\n");
                            Control *ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_IRIS_ABSOLUTE );
                            if (ctrl_that)
                                ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;

                            ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_IRIS_RELATIVE );
                            if (ctrl_that)
                                ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;
                            ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_EXPOSURE_ABSOLUTE );
                            if (ctrl_that)
                                ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                        }
                        break;

                    default:
                        {
                            //printf("manual\n");
                            Control *ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_EXPOSURE_ABSOLUTE );
                            if (ctrl_that)
                                ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                            ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_IRIS_ABSOLUTE );
                            if (ctrl_that)
                                ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                            ctrl_that = get_ctrl_by_id(control_list,
                                V4L2_CID_IRIS_RELATIVE );
                            if (ctrl_that)
                                ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                        }
                        break;
                }
            }
            break;

        case V4L2_CID_FOCUS_AUTO:
            {
                Control *ctrl_this = get_ctrl_by_id(control_list, id );
                if(ctrl_this == NULL)
                    break;
                if(ctrl_this->value > 0)
                {
                    Control *ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_FOCUS_ABSOLUTE);
                    if (ctrl_that)
                        ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;

                    ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_FOCUS_RELATIVE);
                    if (ctrl_that)
                        ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;
                }
                else
                {
                    Control *ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_FOCUS_ABSOLUTE);
                    if (ctrl_that)
                        ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);

                    ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_FOCUS_RELATIVE);
                    if (ctrl_that)
                        ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                }
            }
            break;

        case V4L2_CID_HUE_AUTO:
            {
                Control *ctrl_this = get_ctrl_by_id(control_list, id );
                if(ctrl_this == NULL)
                    break;
                if(ctrl_this->value > 0)
                {
                    Control *ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_HUE);
                    if (ctrl_that)
                        ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;
                }
                else
                {
                    Control *ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_HUE);
                    if (ctrl_that)
                        ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                }
            }
            break;

        case V4L2_CID_AUTO_WHITE_BALANCE:
            {
                Control *ctrl_this = get_ctrl_by_id(control_list, id );
                if(ctrl_this == NULL)
                    break;

                if(ctrl_this->value > 0)
                {
                    Control *ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_WHITE_BALANCE_TEMPERATURE);
                    if (ctrl_that)
                        ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;
                    ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_BLUE_BALANCE);
                    if (ctrl_that)
                        ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;
                    ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_RED_BALANCE);
                    if (ctrl_that)
                        ctrl_that->control.flags |= V4L2_CTRL_FLAG_GRABBED;
                }
                else
                {
                    Control *ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_WHITE_BALANCE_TEMPERATURE);
                    if (ctrl_that)
                        ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                    ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_BLUE_BALANCE);
                    if (ctrl_that)
                        ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                    ctrl_that = get_ctrl_by_id(control_list,
                        V4L2_CID_RED_BALANCE);
                    if (ctrl_that)
                        ctrl_that->control.flags &= !(V4L2_CTRL_FLAG_GRABBED);
                }
            }
            break;
    }
}

/*
 * update flags of entire control list
 */
static void update_ctrl_list_flags(Control *control_list)
{
    Control *current = control_list;
 
    for(; current != NULL; current = current->next)
        update_ctrl_flags(control_list, current->control.id);
}

/*
 * Disables special auto-controls with higher IDs than
 * their absolute/relative counterparts
 * this is needed before restoring controls state
 */
void disable_special_auto (int hdevice, Control *control_list, int id)
{
    Control *current = get_ctrl_by_id(control_list, id);
    if(current && ((id == V4L2_CID_FOCUS_AUTO) || (id == V4L2_CID_HUE_AUTO)))
    {
        current->value = 0;
        set_ctrl(hdevice, control_list, id);
    }
}

static void update_widget_state(Control *control_list, void *all_data)
{
    Control *current = control_list;
    
    for(; current != NULL; current = current->next)
    {
        if(all_data && current->widget)
        {
            switch(current->control.type)
            {
#ifdef V4L2_CTRL_TYPE_STRING
				case V4L2_CTRL_TYPE_STRING:
				{
					char *text_input = g_strescape(current->string, "");
					gtk_entry_set_text (current->widget, text_input);
					g_free(text_input);
					break;
				}
#endif
                case V4L2_CTRL_TYPE_BOOLEAN:
                    //disable widget signals
                    g_signal_handlers_block_by_func(GTK_TOGGLE_BUTTON(current->widget),
                        G_CALLBACK (check_changed), all_data);
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (current->widget),
                        current->value ? TRUE : FALSE);
                    //enable widget signals
                    g_signal_handlers_unblock_by_func(GTK_TOGGLE_BUTTON(current->widget),
                        G_CALLBACK (check_changed), all_data);
                    break;

#ifdef V4L2_CTRL_TYPE_BITMASK
				case V4L2_CTRL_TYPE_BITMASK:
				{
					char *text_input = g_strdup_printf("0x%x", current->value);
					gtk_entry_set_text (current->widget, text_input);
					g_free(text_input);
					break;
				}
#endif
#ifdef V4L2_CTRL_TYPE_INTEGER64
				case V4L2_CTRL_TYPE_INTEGER64:
				{
					char *text_input = g_strdup_printf("0x%" PRIx64 "", c->value64);
					gtk_entry_set_text (current->widget, text_input);
					g_free(text_input);
					break;
				}
#endif
                case V4L2_CTRL_TYPE_INTEGER:
                    if(!(is_special_case_control(current->control.id)))
                    {
                        //disable widget signals
                        g_signal_handlers_block_by_func(GTK_SCALE (current->widget),
                            G_CALLBACK (slider_changed), all_data);
                        gtk_range_set_value (GTK_RANGE (current->widget), current->value);
                        //enable widget signals
                        g_signal_handlers_unblock_by_func(GTK_SCALE (current->widget),
                            G_CALLBACK (slider_changed), all_data);
                        if(current->spinbutton)
                        {
                            //disable widget signals
                            g_signal_handlers_block_by_func(GTK_SPIN_BUTTON(current->spinbutton),
                                G_CALLBACK (spin_changed), all_data);
                            gtk_spin_button_set_value (GTK_SPIN_BUTTON(current->spinbutton), current->value);
                            //enable widget signals
                            g_signal_handlers_unblock_by_func(GTK_SPIN_BUTTON(current->spinbutton),
                                G_CALLBACK (spin_changed), all_data);
                        }
                    }
                    break;

#ifdef V4L2_CTRL_TYPE_INTEGER_MENU
				case V4L2_CTRL_TYPE_INTEGER_MENU:
#endif
                case V4L2_CTRL_TYPE_MENU:
                {
                    //disable widget signals
                    g_signal_handlers_block_by_func(GTK_COMBO_BOX_TEXT(current->widget),
                        G_CALLBACK (combo_changed), all_data);
                    //get new index
                    int j = 0;
                    int def = 0;
                    for (j = 0; current->menu[j].index <= current->control.maximum; j++)
                    {
                    	if(current->value == current->menu[j].index)
                    	   	def = j;
                    }

                    gtk_combo_box_set_active(GTK_COMBO_BOX(current->widget), def);
                    //enable widget signals
                    g_signal_handlers_unblock_by_func(GTK_COMBO_BOX_TEXT(current->widget),
                        G_CALLBACK (combo_changed), all_data);
                    break;
                }

                default:
                    break;
            }
        }
        if((current->control.flags & V4L2_CTRL_FLAG_GRABBED) ||
            (current->control.flags & V4L2_CTRL_FLAG_DISABLED))
        {
            if(current->label)
                gtk_widget_set_sensitive (current->label, FALSE);
            if(current->widget)
                gtk_widget_set_sensitive (current->widget, FALSE);
            if(current->spinbutton)
                gtk_widget_set_sensitive (current->spinbutton, FALSE);
        }
        else
        {
            if(current->label)
                gtk_widget_set_sensitive (current->label, TRUE);
            if(current->widget)
                gtk_widget_set_sensitive (current->widget, TRUE);
            if(current->spinbutton)
                gtk_widget_set_sensitive (current->spinbutton, TRUE);
        }
    }
}



/*
 * creates the control associated widgets for all controls in the list
 */

void create_control_widgets(Control *control_list, void *all_data, int control_only, int verbose)
{
    struct ALL_DATA *data = (struct ALL_DATA *) all_data;
    struct vdIn *videoIn = data->videoIn;
    Control *current;
    int i = 0;

    for(current = control_list; current != NULL; current = current->next)
    {
    	get_ctrl(videoIn->fd, control_list, current->control.id, all_data);
        if (verbose)
        {
            print_control(current, i);
        }

        if(!current->control.step) current->control.step = 1;
        gchar *tmp;
        tmp = g_strdup_printf ("%s:", gettext((char *) current->control.name));
        current->label = gtk_label_new (tmp);
        g_free(tmp);
        gtk_widget_show (current->label);
        gtk_misc_set_alignment (GTK_MISC (current->label), 1, 0.5);

        switch(current->control.type)
        {
#ifdef V4L2_CTRL_TYPE_STRING
            case V4L2_CTRL_TYPE_STRING:
                //text box and set button with maximum charaters
                {
					current->widget = gtk_entry_new();
					gtk_entry_set_max_length(current->widget, current->control.maximum);

					current->spinbutton = gtk_button_new_from_stock(GTK_STOCK_APPLY);

					gtk_widget_show (current->widget);
					gtk_widget_show (current->spinbutton);

					g_object_set_data (G_OBJECT (current->spinbutton), "control_info",
                        GINT_TO_POINTER(current->control.id));

                    g_signal_connect (GTK_BUTTON(current->spinbutton), "clicked",
                        G_CALLBACK (button_clicked), all_data);

				}
                break;
#endif
#ifdef V4L2_CTRL_TYPE_INTEGER64
            case V4L2_CTRL_TYPE_INTEGER64:
                //text box and set button
                {
					current->widget = gtk_entry_new();

					current->spinbutton = gtk_button_new_from_stock(GTK_STOCK_APPLY);

					gtk_widget_show (current->widget);
					gtk_widget_show (current->spinbutton);

					g_object_set_data (G_OBJECT (current->spinbutton), "control_info",
                        GINT_TO_POINTER(current->control.id));

                    g_signal_connect (GTK_BUTTON(current->spinbutton), "clicked",
                        G_CALLBACK (button_clicked), all_data);

				}
                break;
#endif
#ifdef V4L2_CTRL_TYPE_BITMASK
			case V4L2_CTRL_TYPE_BITMASK:
				//text box and set button
                {
					current->widget = gtk_entry_new();

					current->spinbutton = gtk_button_new_from_stock(GTK_STOCK_APPLY);

					gtk_widget_show (current->widget);
					gtk_widget_show (current->spinbutton);

					g_object_set_data (G_OBJECT (current->spinbutton), "control_info",
                        GINT_TO_POINTER(current->control.id));

                    g_signal_connect (GTK_BUTTON(current->spinbutton), "clicked",
                        G_CALLBACK (button_clicked), all_data);

				}
				break;
#endif
            case V4L2_CTRL_TYPE_BUTTON:
                {
                    current->widget = gtk_button_new_with_label(" ");
                    gtk_widget_show (current->widget);

                    g_object_set_data (G_OBJECT (current->widget), "control_info",
                        GINT_TO_POINTER(current->control.id));

                    g_signal_connect (GTK_BUTTON(current->widget), "clicked",
                        G_CALLBACK (button_clicked), all_data);
                }
                break;

            case V4L2_CTRL_TYPE_INTEGER:
                {
                    switch (current->control.id)
                    {   //special cases
                        case V4L2_CID_PAN_RELATIVE:
                        case V4L2_CID_TILT_RELATIVE:
                        {
                            //videoIn->PanTilt++;
                            current->widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);

                            GtkWidget *PanTilt1 = NULL;
                            GtkWidget *PanTilt2 = NULL;
                            if(current->control.id == V4L2_CID_PAN_RELATIVE)
                            {
                                PanTilt1 = gtk_button_new_with_label(_("Left"));
                                PanTilt2 = gtk_button_new_with_label(_("Right"));
                            }
                            else
                            {
                                PanTilt1 = gtk_button_new_with_label(_("Down"));
                                PanTilt2 = gtk_button_new_with_label(_("Up"));
                            }

                            gtk_widget_show (PanTilt1);
                            gtk_widget_show (PanTilt2);
                            gtk_box_pack_start(GTK_BOX(current->widget),PanTilt1,TRUE,TRUE,2);
                            gtk_box_pack_start(GTK_BOX(current->widget),PanTilt2,TRUE,TRUE,2);

                            g_object_set_data (G_OBJECT (PanTilt1), "control_info",
                                GINT_TO_POINTER(current->control.id));
                            g_object_set_data (G_OBJECT (PanTilt2), "control_info",
                                GINT_TO_POINTER(current->control.id));

                            g_signal_connect (GTK_BUTTON(PanTilt1), "clicked",
                                G_CALLBACK (button_PanTilt1_clicked), all_data);
                            g_signal_connect (GTK_BUTTON(PanTilt2), "clicked",
                                G_CALLBACK (button_PanTilt2_clicked), all_data);

                            gtk_widget_show (current->widget);

                            current->spinbutton = gtk_spin_button_new_with_range(-256, 256, 64);
                            /*can't edit the spin value by hand*/
                            gtk_editable_set_editable(GTK_EDITABLE(current->spinbutton),TRUE);

                            gtk_spin_button_set_value (GTK_SPIN_BUTTON(current->spinbutton), 128);
                            gtk_widget_show (current->spinbutton);
                        };
                        break;

                        case V4L2_CID_PAN_RESET:
                        case V4L2_CID_TILT_RESET:
                        {
                            current->widget = gtk_button_new_with_label(" ");
                            gtk_widget_show (current->widget);

                            g_object_set_data (G_OBJECT (current->widget), "control_info",
                                GINT_TO_POINTER(current->control.id));

                            g_signal_connect (GTK_BUTTON(current->widget), "clicked",
                                G_CALLBACK (button_clicked), all_data);
                        };
                        break;

                        case V4L2_CID_LED1_MODE_LOGITECH:
                        {
                        	char* LEDMenu[4] = {_("Off"),_("On"),_("Blinking"),_("Auto")};
                            /*turn it into a menu control*/
                            if(!current->menu)
                    			current->menu = g_new0(struct v4l2_querymenu, 4+1);
                    		else
                    			current->menu = g_renew(struct v4l2_querymenu, current->menu, 4+1);

                   			current->menu[0].id = current->control.id;
                   			current->menu[0].index = 0;
                   			current->menu[0].name[0] = 'N'; //just set something here
                   			current->menu[1].id = current->control.id;
                   			current->menu[1].index = 1;
                   			current->menu[1].name[0] = 'O';
                   			current->menu[2].id = current->control.id;
                   			current->menu[2].index = 2;
                   			current->menu[2].name[0] = 'B';
                   			current->menu[3].id = current->control.id;
                   			current->menu[3].index = 3;
                   			current->menu[3].name[0] = 'A';
                   			current->menu[4].id = current->control.id;
                   			current->menu[4].index = current->control.maximum+1;
                   			current->menu[4].name[0] = '\0';

                            int j = 0;
                        	int def = 0;
                        	current->widget = gtk_combo_box_text_new ();
                        	for (j = 0; current->menu[j].index <= current->control.maximum; j++)
                        	{
                            	gtk_combo_box_text_append_text (
                                	GTK_COMBO_BOX_TEXT (current->widget),
                                	(char *) LEDMenu[j]);
                            	if(current->value == current->menu[j].index)
                            		def = j;
                        	}

                        	gtk_combo_box_set_active (GTK_COMBO_BOX(current->widget), def);
                        	gtk_widget_show (current->widget);

                        	g_object_set_data (G_OBJECT (current->widget), "control_info",
                            	GINT_TO_POINTER(current->control.id));
                        	//connect signal
                       	 	g_signal_connect (GTK_COMBO_BOX_TEXT(current->widget), "changed",
                            	G_CALLBACK (combo_changed), all_data);
                        };
                        break;

                        case V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH:
                        {
                            /*turn it into a menu control*/
                            char* BITSMenu[2] = {_("8 bit"),_("12 bit")};
                            /*turn it into a menu control*/
                            if(!current->menu)
                    			current->menu = g_new0(struct v4l2_querymenu, 2+1);
                    		else
                    			current->menu = g_renew(struct v4l2_querymenu, current->menu, 2+1);

                   			current->menu[0].id = current->control.id;
                   			current->menu[0].index = 0;
                   			current->menu[0].name[0] = 'o'; //just set something here
                   			current->menu[1].id = current->control.id;
                   			current->menu[1].index = 1;
                   			current->menu[1].name[0] = 'd';
                   			current->menu[2].id = current->control.id;
                   			current->menu[2].index = 2;
                   			current->menu[2].name[0] = '\0';
                            int j = 0;
                        	int def = 0;
                        	current->widget = gtk_combo_box_text_new ();
                        	for (j = 0; current->menu[j].index <= current->control.maximum; j++)
                        	{
                        		//if (verbose)
        	                   	//	printf("adding menu entry %d: %d, %s\n",j, current->menu[j].index, current->menu[j].name);
                            	gtk_combo_box_text_append_text (
                                	GTK_COMBO_BOX_TEXT (current->widget),
                                	(char *) BITSMenu[j]);
                            	if(current->value == current->menu[j].index)
                            		def = j;
                        	}

                        	gtk_combo_box_set_active (GTK_COMBO_BOX(current->widget), def);
                        	gtk_widget_show (current->widget);

                        	g_object_set_data (G_OBJECT (current->widget), "control_info",
                            	GINT_TO_POINTER(current->control.id));
                        	//connect signal
                       	 	g_signal_connect (GTK_COMBO_BOX_TEXT(current->widget), "changed",
                            	G_CALLBACK (combo_changed), all_data);
                        };
                        break;

                        default: //standard case - hscale
                        {
                            /* check for valid range */
                            if((current->control.maximum > current->control.minimum) && (current->control.step != 0))
                            {
                            	GtkAdjustment *adjustment =  gtk_adjustment_new (
                                	current->value,
                                	current->control.minimum,
                                    current->control.maximum,
                                    current->control.step,
                                    current->control.step*10,
                                    0);

                                current->widget = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);

                                gtk_scale_set_draw_value (GTK_SCALE (current->widget), FALSE);
                                gtk_range_set_round_digits(GTK_RANGE (current->widget), 0);

                                gtk_widget_show (current->widget);

                                current->spinbutton = gtk_spin_button_new(adjustment,current->control.step, 0);

                                /*can't edit the spin value by hand*/
                                gtk_editable_set_editable(GTK_EDITABLE(current->spinbutton),TRUE);

                                gtk_widget_show (current->spinbutton);

                                g_object_set_data (G_OBJECT (current->widget), "control_info",
                                    GINT_TO_POINTER(current->control.id));
                                g_object_set_data (G_OBJECT (current->spinbutton), "control_info",
                                    GINT_TO_POINTER(current->control.id));
                                //connect signal
                                g_signal_connect (GTK_SCALE(current->widget), "value-changed",
                                    G_CALLBACK (slider_changed), all_data);
                                g_signal_connect(GTK_SPIN_BUTTON(current->spinbutton),"value-changed",
                                    G_CALLBACK (spin_changed), all_data);
                            }
                            else
                            {
                                printf("INVALID RANGE (MAX <= MIN) for control id: 0x%08x \n", current->control.id);
                            }
                        };
                        break;
                    };
                };
                break;

#ifdef V4L2_CTRL_TYPE_INTEGER_MENU
			case V4L2_CTRL_TYPE_INTEGER_MENU:
#endif
            case V4L2_CTRL_TYPE_MENU:
                {
                    if(current->menu)
                    {
                        int j = 0;
                        int def = 0;
                        current->widget = gtk_combo_box_text_new ();
                        for (j = 0; current->menu[j].index <= current->control.maximum; j++)
                        {
							if(current->control.type == V4L2_CTRL_TYPE_MENU)
							{
								gtk_combo_box_text_append_text (
									GTK_COMBO_BOX_TEXT (current->widget),
									(char *) current->menu[j].name);
                            }
#ifdef V4L2_CTRL_TYPE_INTEGER_MENU
                            else
                            {
								char buffer[30]="0";
								snprintf(buffer, "%" PRIu64 "", 29, current->menu[j].value);
								gtk_combo_box_text_append_text (
									GTK_COMBO_BOX_TEXT (current->widget), buffer);
                            }
#endif
                            if(current->value == current->menu[j].index)
                            	def = j;
                        }

                        gtk_combo_box_set_active (GTK_COMBO_BOX(current->widget), def);
                        gtk_widget_show (current->widget);

                        g_object_set_data (G_OBJECT (current->widget), "control_info",
                            GINT_TO_POINTER(current->control.id));
                        //connect signal
                        g_signal_connect (GTK_COMBO_BOX_TEXT(current->widget), "changed",
                            G_CALLBACK (combo_changed), all_data);
                    }
                }
                break;

            case V4L2_CTRL_TYPE_BOOLEAN:
                {
                    if(current->control.id ==V4L2_CID_DISABLE_PROCESSING_LOGITECH)
                    {
                        //a little hack :D we use the spin widget as a combo
                        current->spinbutton = gtk_combo_box_text_new ();
			            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(current->spinbutton),
			                "GBGB... | RGRG...");
			            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(current->spinbutton),
			                "GRGR... | BGBG...");
			            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(current->spinbutton),
			                "BGBG... | GRGR...");
			            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(current->spinbutton),
			                "RGRG... | GBGB...");

			            gtk_combo_box_set_active(GTK_COMBO_BOX(current->spinbutton), 0);

			            gtk_widget_show (current->spinbutton);

			            g_signal_connect (GTK_COMBO_BOX_TEXT (current->spinbutton), "changed",
				            G_CALLBACK (pix_ord_changed), all_data);

				        videoIn->isbayer = (current->value ? TRUE : FALSE);
                    }

                    current->widget = gtk_check_button_new();
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (current->widget),
                        current->value ? TRUE : FALSE);
                    gtk_widget_show (current->widget);

                    g_object_set_data (G_OBJECT (current->widget), "control_info",
                        GINT_TO_POINTER(current->control.id));
                    //connect signal
                    g_signal_connect (GTK_TOGGLE_BUTTON(current->widget), "toggled",
                        G_CALLBACK (check_changed), all_data);
                }
                break;

            default:
                printf("control type: 0x%08x not supported\n", current->control.type);
                break;
        }

        i++;//increment control index
    }
    update_widget_state(control_list, all_data);
 }

/*
 * Returns the Control structure corresponding to control id,
 * from the control list.
 */
Control *get_ctrl_by_id(Control *control_list, int id)
{
    Control *current = control_list;
    
    for(; current != NULL; current = current->next)
    {
        if(current->control.id == id)
            return (current);
    }
    if(current->control.id == id)
        return (current);
    else//no id match
        return(NULL);
}

/*
 * Goes through the control list and gets the controls current values
 * also updates flags and widget states
 */
void get_ctrl_values (int hdevice, Control *control_list, int num_controls, void *all_data)
{
    int ret = 0;
    struct v4l2_ext_control clist[num_controls];
    Control *current = control_list;
    
    int count = 0;
    int i = 0;

    for(; current != NULL; current = current->next)
    {
        if(current->control.flags & V4L2_CTRL_FLAG_WRITE_ONLY)
             continue;

        clist[count].id = current->control.id;
#ifdef V4L2_CTRL_TYPE_STRING
        clist[count].size = 0;
        if(current->control.type == V4L2_CTRL_TYPE_STRING)
        {
            clist[count].size = current->control.maximum;
            clist[count].string = (char *) g_strnfill(clist[count].size + 1, 0);
        }
#endif
        count++;

        if((current->next == NULL) || (current->next->class != current->class))
        {
            struct v4l2_ext_controls ctrls = {0};
            ctrls.ctrl_class = current->class;
            ctrls.count = count;
            ctrls.controls = clist;
            ret = xioctl(hdevice, VIDIOC_G_EXT_CTRLS, &ctrls);
            if(ret)
            {
                printf("VIDIOC_G_EXT_CTRLS failed\n");
                struct v4l2_control ctrl;
                //get the controls one by one
                if( current->class == V4L2_CTRL_CLASS_USER
#ifdef V4L2_CTRL_TYPE_STRING
					&& current->control.type != V4L2_CTRL_TYPE_STRING
#endif
#ifdef V4L2_CTRL_TYPE_INTEGER64
					&& current->control.type != V4L2_CTRL_TYPE_INTEGER64
#endif
				  )
                {
                    printf("   using VIDIOC_G_CTRL for user class controls\n");
                    for(i=0; i < count; i++)
                    {
                        ctrl.id = clist[i].id;
                        ctrl.value = 0;
                        ret = xioctl(hdevice, VIDIOC_G_CTRL, &ctrl);
                        if(ret)
                            continue;
                        clist[i].value = ctrl.value;
                    }
                }
                else
                {
                    printf("   using VIDIOC_G_EXT_CTRLS on single controls for class: 0x%08x\n",
                        current->class);
                    for(i=0;i < count; i++)
                    {
                        ctrls.count = 1;
                        ctrls.controls = &clist[i];
                        ret = xioctl(hdevice, VIDIOC_G_EXT_CTRLS, &ctrls);
                        if(ret)
                            printf("control id: 0x%08x failed to get (error %i)\n",
                                clist[i].id, ret);
                    }
                }
            }

            //fill in the values on the control list
            for(i=0; i<count; i++)
            {
                Control *ctrl = get_ctrl_by_id(control_list, clist[i].id);
                if(!ctrl)
                {
                    printf("couldn't get control for id: %i\n", clist[i].id);
                    continue;
                }
                switch(ctrl->control.type)
                {
#ifdef V4L2_CTRL_TYPE_STRING
                    case V4L2_CTRL_TYPE_STRING:
                    {
                        //string gets set on VIDIOC_G_EXT_CTRLS
                        //add the maximum size to value
                        unsigned len = clist[i].size;
						unsigned max_len = ctrl->control.maximum;

						strncpy(ctrl->string, clist[i].string, max_len);
						if(len > max_len)
						{
							ctrl->string[max_len] = 0; //Null terminated
							printf("control id: 0x%08x returned string size of %d when max is %d\n",
								ctrl->control.id, len, max_len);
						}

						//clean up
						g_free(clist[i].string);
						clist[i].string = NULL;
                        break;
                    }
#endif
                    case V4L2_CTRL_TYPE_INTEGER64:
                        ctrl->value64 = clist[i].value64;
                        break;
                    default:
                        ctrl->value = clist[i].value;
                        //printf("control %i [0x%08x] = %i\n",
                        //    i, clist[i].id, clist[i].value);
                        break;
                }
            }

            count = 0;
        }
    }

    update_ctrl_list_flags(control_list);
    update_widget_state(control_list, all_data);

}

/*
 * Gets the value for control id
 * and updates control flags and widgets
 */
int get_ctrl(int hdevice, Control *control_list, int id, void *all_data)
{
    Control *control = get_ctrl_by_id(control_list, id );
    int ret = 0;

    if(!control)
        return (-1);
    if(control->control.flags & V4L2_CTRL_FLAG_WRITE_ONLY)
        return (-1);

    if( control->class == V4L2_CTRL_CLASS_USER
#ifdef V4L2_CTRL_TYPE_STRING
		&& control->control.type != V4L2_CTRL_TYPE_STRING
#endif
#ifdef V4L2_CTRL_TYPE_INTEGER64
		&& control->control.type != V4L2_CTRL_TYPE_INTEGER64
#endif
        )
    {
        struct v4l2_control ctrl;
        //printf("   using VIDIOC_G_CTRL for user class controls\n");
        ctrl.id = control->control.id;
        ctrl.value = 0;
        ret = xioctl(hdevice, VIDIOC_G_CTRL, &ctrl);
        if(ret)
            printf("control id: 0x%08x failed to get value (error %i)\n",
                ctrl.id, ret);
        else
            control->value = ctrl.value;
    }
    else
    {
        //printf("   using VIDIOC_G_EXT_CTRLS on single controls for class: 0x%08x\n",
        //    current->class);
        struct v4l2_ext_controls ctrls = {0};
        struct v4l2_ext_control ctrl = {0};
        ctrl.id = control->control.id;
#ifdef V4L2_CTRL_TYPE_STRING
        ctrl.size = 0;
        if(control->control.type == V4L2_CTRL_TYPE_STRING)
        {
            ctrl.size = control->control.maximum;
            ctrl.string = (char *) g_strnfill(ctrl.size + 1, 0);
        }
#endif
        ctrls.ctrl_class = control->class;
        ctrls.count = 1;
        ctrls.controls = &ctrl;
        ret = xioctl(hdevice, VIDIOC_G_EXT_CTRLS, &ctrls);
        if(ret)
            printf("control id: 0x%08x failed to get value (error %i)\n",
                ctrl.id, ret);
        else
        {
            switch(control->control.type)
            {
#ifdef V4L2_CTRL_TYPE_STRING
                case V4L2_CTRL_TYPE_STRING:
				{
					unsigned len = ctrl.size;
					unsigned max_len = control->control.maximum;

					strncpy(control->string, ctrl.string, max_len);
					if(len > max_len)
					{
						control->value = max_len;
						control->string[max_len] = 0; //Null terminated
						printf("control id: 0x%08x returned string size of %d when max is %d\n",
							control->control.id, len, max_len);
					}


					//clean up
					g_free(ctrl.string);
					ctrl.string = NULL;

					break;
				}
#endif
#ifdef V4L2_CTRL_TYPE_INTEGER64
                case V4L2_CTRL_TYPE_INTEGER64:
                    control->value64 = ctrl.value64;
                    break;
#endif

                default:
                    control->value = ctrl.value;
                    //printf("control %i [0x%08x] = %i\n",
                    //    i, clist[i].id, clist[i].value);
                    break;
            }
        }
    }

    update_ctrl_flags(control_list, id);
    update_widget_state(control_list, all_data);

    return (ret);
}

/*
 * Goes through the control list and tries to set the controls values
 */
void set_ctrl_values (int hdevice, Control *control_list, int num_controls)
{
    int ret = 0;
    struct v4l2_ext_control clist[num_controls];
    Control *current = control_list;

    int count = 0;
    int i = 0;

    for(; current != NULL; current = current->next)
    {
        if(current->control.flags & V4L2_CTRL_FLAG_READ_ONLY)
            continue;

        clist[count].id = current->control.id;
        switch (current->control.type)
        {
#ifdef V4L2_CTRL_TYPE_STRING
            case V4L2_CTRL_TYPE_STRING:
            {
				unsigned len = strlen(current->string);
				unsigned max_len = current->control.maximum;

				if(len > max_len)
				{
					clist[count].size = max_len;
					clist[count].string = (char *) g_strndup(current->string, max_len);
					printf("control id: 0x%08x trying to set string size of %d when max is %d (clip)\n",
						current->control.id, len, max_len);
				}
				else
				{
					clist[count].size = len;
					clist[count].string = (char *) g_strdup(current->string);
				}
                break;
            }
#endif
            case V4L2_CTRL_TYPE_INTEGER64:
                clist[count].value64 = current->value64;
                break;
            default:
                clist[count].value = current->value;
                break;
        }
        count++;

        if((current->next == NULL) || (current->next->class != current->class))
        {
            struct v4l2_ext_controls ctrls = {0};
            ctrls.ctrl_class = current->class;
            ctrls.count = count;
            ctrls.controls = clist;
            ret = xioctl(hdevice, VIDIOC_S_EXT_CTRLS, &ctrls);
            if(ret)
            {
                printf("VIDIOC_S_EXT_CTRLS for multiple controls failed (error %i)\n", ret);
                struct v4l2_control ctrl;
                //set the controls one by one
                if( current->class == V4L2_CTRL_CLASS_USER
#ifdef V4L2_CTRL_TYPE_STRING
					&& current->control.type != V4L2_CTRL_TYPE_STRING
#endif
#ifdef V4L2_CTRL_TYPE_INTEGER64
					&& current->control.type != V4L2_CTRL_TYPE_INTEGER64
#endif
                )
                {
                    printf("   using VIDIOC_S_CTRL for user class controls\n");
                    for(i=0;i < count; i++)
                    {
                        ctrl.id = clist[i].id;
                        ctrl.value = clist[i].value;
                        ret = xioctl(hdevice, VIDIOC_S_CTRL, &ctrl);
                        if(ret)
                        {
                            Control *ctrl = get_ctrl_by_id(control_list, clist[i].id);
                            if(ctrl)
                                printf("control(0x%08x) \"%s\" failed to set (error %i)\n",
                                    clist[i].id, ctrl->control.name, ret);
                            else
                              printf("control(0x%08x) failed to set (error %i)\n",
                                    clist[i].id, ret);
                        }
                    }
                }
                else
                {
                    printf("   using VIDIOC_S_EXT_CTRLS on single controls for class: 0x%08x\n",
                        current->class);
                    for(i=0;i < count; i++)
                    {
                        ctrls.count = 1;
                        ctrls.controls = &clist[i];
                        ret = xioctl(hdevice, VIDIOC_S_EXT_CTRLS, &ctrls);

                        Control *ctrl = get_ctrl_by_id(control_list, clist[i].id);

                        if(ret)
                        {
                            if(ctrl)
                                printf("control(0x%08x) \"%s\" failed to set (error %i)\n",
                                    clist[i].id, ctrl->control.name, ret);
                            else
                              printf("control(0x%08x) failed to set (error %i)\n",
                                    clist[i].id, ret);
                        }
#ifdef V4L2_CTRL_TYPE_STRING
                        if(ctrl && ctrl->control.type == V4L2_CTRL_TYPE_STRING)
                        {
							g_free(clist[i].string); //free allocated string
							clist[i].string = NULL;
						}
#endif
                    }
                }
            }
            count = 0;
        }
    }

    //update list with real values
    //get_ctrl_values (hdevice, control_list, num_controls);
}

/*
 * sets all controls to default values
 */
void set_default_values(int hdevice, Control *control_list, int num_controls, void *all_data)
{
    Control *current = control_list;

    for(; current != NULL; current = current->next)
    {
        if(current->control.flags & V4L2_CTRL_FLAG_READ_ONLY)
            continue;
        
        //printf("setting 0x%08X to %i\n",current->control.id, current->control.default_value);
        switch (current->control.type)
        {
#ifdef V4L2_CTRL_TYPE_STRING
            case V4L2_CTRL_TYPE_STRING: // do string controls have a default value?
                break;
#endif
#ifdef V4L2_CTRL_TYPE_INTEGER64
            case V4L2_CTRL_TYPE_INTEGER64: // do int64 controls have a default value?
                break;
#endif
            default:
                //if its one of the special auto controls disable it first
                disable_special_auto (hdevice, control_list, current->control.id);
                current->value = current->control.default_value;
                break;
        }
    }

    set_ctrl_values (hdevice, control_list, num_controls);
    get_ctrl_values (hdevice, control_list, num_controls, all_data);
}

/*
 * sets the value for control id
 */
int set_ctrl(int hdevice, Control *control_list, int id)
{
    Control *control = get_ctrl_by_id(control_list, id );
    int ret = 0;

    if(!control)
        return (-1);
    if(control->control.flags & V4L2_CTRL_FLAG_READ_ONLY)
        return (-1);

    if( control->class == V4L2_CTRL_CLASS_USER
#ifdef V4L2_CTRL_TYPE_STRING
    && control->control.type != V4L2_CTRL_TYPE_STRING
#endif
    && control->control.type != V4L2_CTRL_TYPE_INTEGER64)
    {
        //using VIDIOC_G_CTRL for user class controls
        struct v4l2_control ctrl;
        ctrl.id = control->control.id;
        ctrl.value = control->value;
        ret = xioctl(hdevice, VIDIOC_S_CTRL, &ctrl);
    }
    else
    {
        //using VIDIOC_G_EXT_CTRLS on single controls
        struct v4l2_ext_controls ctrls = {0};
        struct v4l2_ext_control ctrl = {0};
        ctrl.id = control->control.id;
        switch (control->control.type)
        {
#ifdef V4L2_CTRL_TYPE_STRING
            case V4L2_CTRL_TYPE_STRING:
            {
				unsigned len = strlen(control->string);
				unsigned max_len = control->control.maximum;

				if(len > max_len)
				{
					ctrl.size = max_len;
					ctrl.string = (char *) g_strndup(control->string, max_len);
					printf("control id: 0x%08x trying to set string size of %d when max is %d (clip)\n",
						control->control.id, len, max_len);
				}
				else
				{
					ctrl.size = len;
					ctrl.string = (char *) g_strndup(control->string, max_len);
				}
                break;
            }
#endif
#ifdef V4L2_CTRL_TYPE_INTEGER64
            case V4L2_CTRL_TYPE_INTEGER64:
                ctrl.value64 = control->value64;
                break;
#endif
            default:
                ctrl.value = control->value;
                break;
        }
        ctrls.ctrl_class = control->class;
        ctrls.count = 1;
        ctrls.controls = &ctrl;
        ret = xioctl(hdevice, VIDIOC_S_EXT_CTRLS, &ctrls);
        if(ret)
            printf("control id: 0x%08x failed to set (error %i)\n",
                ctrl.id, ret);
#ifdef V4L2_CTRL_TYPE_STRING
        if(control->control.type == V4L2_CTRL_TYPE_STRING)
        {
			g_free(ctrl.string); //clean up string allocation
			ctrl.string = NULL;
		}
#endif
    }

    //update real value
    get_ctrl(hdevice, control_list, id, NULL);

    return (ret);
}

/*
 * frees the control list allocations
 */
void free_control_list (Control *control_list)
{
    Control *first = control_list;
    Control *next = first->next;
    while (next != NULL)
    {
        if(first->string) g_free(first->string);
        if(first->menu) g_free(first->menu);
        free(first);
        first = next;
        next = first->next;
    }
    //clean the last one
    if(first->string) g_free(first->string);
    if(first) free(first);
}

/*
 * sets pan tilt (direction = 1 or -1)
 */
void uvcPanTilt (int hdevice, Control *control_list, int is_pan, int direction)
{
    Control *ctrl = NULL;
    int id = V4L2_CID_TILT_RELATIVE;
    if (is_pan)
        id = V4L2_CID_PAN_RELATIVE;

    ctrl = get_ctrl_by_id(control_list, id );

    if (ctrl && ctrl->spinbutton)
    {

        ctrl->value = direction * gtk_spin_button_get_value_as_int (
            GTK_SPIN_BUTTON(ctrl->spinbutton));
        set_ctrl(hdevice, control_list, id);
    }

}


