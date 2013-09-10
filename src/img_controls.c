/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
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

/* support for internationalization - i18n */
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "img_controls.h"
#include "v4l2uvc.h"
#include "v4l2_controls.h"
#include "v4l2_dyna_ctrls.h"
#include "globals.h"
#include "string_utils.h"
#include "autofocus.h"
#include "callbacks.h"

/*exposure menu for old type controls */
// static const char *exp_typ[]={
				// "Manual Mode",
				// "Auto Mode",
				// "Shutter Priority Mode",
				// "Aperture Priority Mode"
				// };

/*--------------------------- draw camera controls ---------------------------*/
void
draw_controls (struct ALL_DATA *all_data)
{
	struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct focusData *AFdata = all_data->AFdata;

    if (s->control_list)
    {
        free_control_list(s->control_list);
    }
    s->num_controls = 0;
    //get the control list
    s->control_list = get_control_list(videoIn->fd, &(s->num_controls), global->lctl_method);

    if(!s->control_list)
    {
        printf("Error: empty control list\n");
        return;
    }

    get_ctrl_values (videoIn->fd, s->control_list, s->num_controls, NULL);

    s->table = gtk_grid_new ();
   	gtk_grid_set_column_homogeneous (GTK_GRID(s->table), FALSE);
	gtk_widget_set_hexpand (s->table, TRUE);
	gtk_widget_set_halign (s->table, GTK_ALIGN_FILL);

	gtk_grid_set_row_spacing (GTK_GRID(s->table), 4);
	gtk_grid_set_column_spacing (GTK_GRID (s->table), 4);
	gtk_container_set_border_width (GTK_CONTAINER (s->table), 2);

    //add control widgets to control list
    create_control_widgets(s->control_list, (void *) all_data, global->control_only, global->debug);

    int done = 0;
    int row=0;

    Control *current = s->control_list;
    Control *next = current->next;

    while(!done)
    {
        //add some flags
        if ((current->control.id == V4L2_CID_PAN_RELATIVE) ||
	        (current->control.id == V4L2_CID_TILT_RELATIVE))
	    {
	        videoIn->PanTilt++;
	    }

        //special cases (extra software controls)
	    if (((current->control.id == V4L2_CID_FOCUS_ABSOLUTE) ||
	        (current->control.id == V4L2_CID_FOCUS_LOGITECH)) &&
	         !(global->control_only))
	    {
		    global->AFcontrol=1;

			if(!AFdata)
			{
			    AFdata = initFocusData(current->control.maximum,
			        current->control.minimum,
			        current->control.step,
			        current->control.id);
				all_data->AFdata = AFdata;
			}

			if(!AFdata)
			    global->AFcontrol = 0;
			else
			{
				GtkWidget *Focus_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
				GtkWidget *AutoFocus = gtk_check_button_new_with_label (_("Auto Focus (continuous)"));
				GtkWidget *FocusButton = gtk_button_new_with_label (_("set Focus"));
				gtk_box_pack_start (GTK_BOX (Focus_box), AutoFocus, TRUE, TRUE, 0);
				gtk_box_pack_start (GTK_BOX (Focus_box), FocusButton, TRUE, TRUE, 0);
				gtk_widget_show (Focus_box);
				gtk_widget_show (AutoFocus);
				gtk_widget_show (FocusButton);
				gtk_grid_attach(GTK_GRID(s->table), Focus_box, 1, row, 1, 1);

				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (AutoFocus),
				    global->autofocus ? TRUE: FALSE);

				g_signal_connect (G_OBJECT (AutoFocus), "toggled",
				    G_CALLBACK (autofocus_changed), all_data);
				g_signal_connect (G_OBJECT (FocusButton), "clicked",
				    G_CALLBACK (setfocus_clicked), all_data);

				row++; /*increment control row*/

			}
		}


        if(current->label)
            gtk_grid_attach(GTK_GRID(s->table), current->label, 0, row, 1 , 1);
        if(current->widget)
        {
            gtk_grid_attach(GTK_GRID(s->table), current->widget, 1, row, 1 , 1);
            gtk_widget_set_halign (current->widget, GTK_ALIGN_FILL);
			gtk_widget_set_hexpand (current->widget, TRUE);
        }
        if(current->spinbutton)
            gtk_grid_attach(GTK_GRID(s->table), current->spinbutton, 2, row, 1 , 1);


        if(next == NULL)
            done = 1;
        else
        {
            row++;
            current = next;
            next = current->next;
        }
    }

	/*              try to start the video stream             */
	/* do it here (after all ioctls) since some cameras take  */
	/* a long time to initialize after this                   */
	/* it's OK if it fails since it is retried in uvcGrab     */
	if(!global->control_only) video_enable(videoIn);

	s = NULL;
	global = NULL;
	videoIn = NULL;
}

