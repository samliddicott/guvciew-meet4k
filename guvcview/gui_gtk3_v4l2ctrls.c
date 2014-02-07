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

#include "gui_gtk3.h"
#include "gui_gtk3_callbacks.h"
#include "gui.h"
/*add this last to avoid redefining _() and N_()*/
#include "gview.h"


extern int debug_level;

/*
 * attach v4l2 controls tab widget
 * args:
 *   device - pointer to device data we want to attach the gui for
 *   parent - tab parent widget
 *
 * asserts:
 *   device is not null
 *   parent is not null
 *
 * returns: error code (0 -OK)
 */
int gui_attach_gtk3_v4l2ctrls(v4l2_dev_t *device, GtkWidget *parent)
{
	/*assertions*/
	assert(device != NULL);
	assert(parent != NULL);

	GtkWidget *img_controls_grid = gtk_grid_new();

	gtk_grid_set_column_homogeneous (GTK_GRID(img_controls_grid), FALSE);
	gtk_widget_set_hexpand (img_controls_grid, TRUE);
	gtk_widget_set_halign (img_controls_grid, GTK_ALIGN_FILL);

	gtk_grid_set_row_spacing (GTK_GRID(img_controls_grid), 4);
	gtk_grid_set_column_spacing (GTK_GRID (img_controls_grid), 4);
	gtk_container_set_border_width (GTK_CONTAINER (img_controls_grid), 2);

	int i = 0;
	v4l2_ctrl_t *current = device->list_device_controls;

    for(; current != NULL; current = current->next)
    {
		/*label*/
		char *tmp;
        tmp = g_strdup_printf ("%s:", gettext((char *) current->control.name));
        GtkWidget *label = gtk_label_new (tmp);
        free(tmp);
        gtk_widget_show (label);
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

        GtkWidget *widget = NULL;
        GtkWidget *widget2 = NULL; /*usually a spin button*/

		switch (current->control.type)
		{
			case V4L2_CTRL_TYPE_INTEGER:

				switch (current->control.id)
				{
					//special cases
					case V4L2_CID_PAN_RELATIVE:
					case V4L2_CID_TILT_RELATIVE:
					{
						widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);

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
						gtk_box_pack_start(GTK_BOX(widget),PanTilt1,TRUE,TRUE,2);
						gtk_box_pack_start(GTK_BOX(widget),PanTilt2,TRUE,TRUE,2);

						g_object_set_data (G_OBJECT (PanTilt1), "control_info",
							GINT_TO_POINTER(current->control.id));
						g_object_set_data (G_OBJECT (PanTilt2), "control_info",
							GINT_TO_POINTER(current->control.id));

						/*connect signals*/
						g_signal_connect (GTK_BUTTON(PanTilt1), "clicked",
							G_CALLBACK (button_PanTilt1_clicked), device);
						g_signal_connect (GTK_BUTTON(PanTilt2), "clicked",
							G_CALLBACK (button_PanTilt2_clicked), device);

						gtk_widget_show (widget);

						widget2 = gtk_spin_button_new_with_range(-256, 256, 64);

						gtk_editable_set_editable(GTK_EDITABLE(widget2), TRUE);

						gtk_spin_button_set_value (GTK_SPIN_BUTTON(widget2), 128);

						/*connect signal*/
						g_object_set_data (G_OBJECT (widget2), "control_info",
							GINT_TO_POINTER(current->control.id));
						g_signal_connect(GTK_SPIN_BUTTON(widget2),"value-changed",
							G_CALLBACK (pan_tilt_step_changed), device);

						gtk_widget_show (widget2);

						break;
					}

					case V4L2_CID_PAN_RESET:
					case V4L2_CID_TILT_RESET:
					{
						widget = gtk_button_new_with_label(" ");
						gtk_widget_show (widget);

						g_object_set_data (G_OBJECT (widget), "control_info",
							GINT_TO_POINTER(current->control.id));

						/*connect signal*/
						g_signal_connect (GTK_BUTTON(widget), "clicked",
							G_CALLBACK (button_clicked), device);

						break;
					};

					case V4L2_CID_LED1_MODE_LOGITECH:
					{
						char* LEDMenu[4] = {_("Off"),_("On"),_("Blinking"),_("Auto")};
						/*turn it into a menu control*/
						if(!current->menu)
                    		current->menu = calloc(4+1, sizeof(struct v4l2_querymenu));
                    	else
                    		current->menu = realloc(current->menu,  (4+1) * sizeof(struct v4l2_querymenu));

						current->menu[0].id = current->control.id;
						current->menu[0].index = 0;
						current->menu[0].name[0] = 'N'; /*just set something here*/
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

						widget = gtk_combo_box_text_new ();
						for (j = 0; current->menu[j].index <= current->control.maximum; j++)
						{
							gtk_combo_box_text_append_text (
								GTK_COMBO_BOX_TEXT (widget),
								(char *) LEDMenu[j]);
							if(current->value == current->menu[j].index)
								def = j;
						}

						gtk_combo_box_set_active (GTK_COMBO_BOX(widget), def);
						gtk_widget_show (widget);

						g_object_set_data (G_OBJECT (widget), "control_info",
                           	GINT_TO_POINTER(current->control.id));

						/*connect signal*/
						g_signal_connect (GTK_COMBO_BOX_TEXT(widget), "changed",
							G_CALLBACK (combo_changed), device);

						break;
					}

					case V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH:
					{
						/*turn it into a menu control*/
						char* BITSMenu[2] = {_("8 bit"),_("12 bit")};
						/*turn it into a menu control*/
						if(!current->menu)
							current->menu = calloc(2+1, sizeof(struct v4l2_querymenu));
						else
							current->menu = realloc(current->menu, (2+1) * sizeof(struct v4l2_querymenu));

						current->menu[0].id = current->control.id;
						current->menu[0].index = 0;
						current->menu[0].name[0] = 'o'; /*just set something here*/
						current->menu[1].id = current->control.id;
						current->menu[1].index = 1;
						current->menu[1].name[0] = 'd';
						current->menu[2].id = current->control.id;
						current->menu[2].index = 2;
						current->menu[2].name[0] = '\0';

						int j = 0;
						int def = 0;
						widget = gtk_combo_box_text_new ();
						for (j = 0; current->menu[j].index <= current->control.maximum; j++)
						{
							//if (debug_level > 0)
							//	printf("GUVCVIEW: adding menu entry %d: %d, %s\n",j, current->menu[j].index, current->menu[j].name);
							gtk_combo_box_text_append_text (
								GTK_COMBO_BOX_TEXT (widget),
								(char *) BITSMenu[j]);
							if(current->value == current->menu[j].index)
								def = j;
						}

						gtk_combo_box_set_active (GTK_COMBO_BOX(widget), def);
						gtk_widget_show (widget);

						g_object_set_data (G_OBJECT (widget), "control_info",
							GINT_TO_POINTER(current->control.id));
						/*connect signal*/
						g_signal_connect (GTK_COMBO_BOX_TEXT(widget), "changed",
							G_CALLBACK (combo_changed), device);
						break;
					}

					default: /*standard case - hscale + spin*/
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

							widget = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);

							gtk_scale_set_draw_value (GTK_SCALE (widget), FALSE);
							gtk_range_set_round_digits(GTK_RANGE (widget), 0);

							gtk_widget_show (widget);

							widget2= gtk_spin_button_new(adjustment, current->control.step, 0);

							gtk_editable_set_editable(GTK_EDITABLE(widget2),TRUE);

							gtk_widget_show (widget2);

							g_object_set_data (G_OBJECT (widget), "control_info",
								GINT_TO_POINTER(current->control.id));
							g_object_set_data (G_OBJECT (widget2), "control_info",
								GINT_TO_POINTER(current->control.id));

							/*connect signals*/
							g_signal_connect (GTK_SCALE(widget), "value-changed",
								G_CALLBACK (slider_changed), device);
							g_signal_connect(GTK_SPIN_BUTTON(widget2),"value-changed",
								G_CALLBACK (spin_changed), device);
						}
						else
                          fprintf(stderr, "GUVCVIEW: (Invalid range) [MAX <= MIN] for control id: 0x%08x \n", current->control.id);

						break;
					}
				}
				break;

#ifdef V4L2_CTRL_TYPE_INTEGER64
			case V4L2_CTRL_TYPE_INTEGER64:

				widget = gtk_entry_new();
				gtk_entry_set_max_length(widget, current->control.maximum);

				widget2= gtk_button_new_from_stock(GTK_STOCK_APPLY);

				gtk_widget_show (widget);
				gtk_widget_show (widget2);

				g_object_set_data (G_OBJECT (widget2), "control_info",
					GINT_TO_POINTER(current->control.id));
				g_object_set_data (G_OBJECT (widget2), "control_entry",
					widget);

				/*connect signal*/
				g_signal_connect (GTK_BUTTON(widget2), "clicked",
					G_CALLBACK (int64_button_clicked), device);

				break;

				break;
#endif
#ifdef V4L2_CTRL_TYPE_STRING
			case V4L2_CTRL_TYPE_STRING:

				widget = gtk_entry_new();
				gtk_entry_set_max_length(widget, current->control.maximum);

				widget2= gtk_button_new_from_stock(GTK_STOCK_APPLY);

				gtk_widget_show (widget);
				gtk_widget_show (widget2);

				g_object_set_data (G_OBJECT (widget2), "control_info",
					GINT_TO_POINTER(current->control.id));
				g_object_set_data (G_OBJECT (widget2), "control_entry",
					widget);

				/*connect signal*/
				g_signal_connect (GTK_BUTTON(widget2), "clicked",
					G_CALLBACK (string_button_clicked), device);

				break;
#endif
#ifdef V4L2_CTRL_TYPE_BITMASK
			case V4L2_CTRL_TYPE_BITMASK:

					widget = gtk_entry_new();

					widget2 = gtk_button_new_from_stock(GTK_STOCK_APPLY);

					gtk_widget_show (widget);
					gtk_widget_show (widget2);

					g_object_set_data (G_OBJECT (widget2), "control_info",
                        GINT_TO_POINTER(current->control.id));
					g_object_set_data (G_OBJECT (widget2), "control_entry",
						widget);

                    g_signal_connect (GTK_BUTTON(widget2), "clicked",
                        G_CALLBACK (bitmask_button_clicked), device);

				break;
#endif
#ifdef V4L2_CTRL_TYPE_INTEGER_MENU
			case V4L2_CTRL_TYPE_INTEGER_MENU:
#endif
            case V4L2_CTRL_TYPE_MENU:

				if(current->menu)
				{
					int j = 0;
					int def = 0;
					widget = gtk_combo_box_text_new ();

					for (j = 0; current->menu[j].index <= current->control.maximum; j++)
					{
						if(current->control.type == V4L2_CTRL_TYPE_MENU)
						{
							gtk_combo_box_text_append_text (
								GTK_COMBO_BOX_TEXT (widget),
								(char *) current->menu[j].name);
						}
#ifdef V4L2_CTRL_TYPE_INTEGER_MENU
						else
						{
							char buffer[30]="0";
							snprintf(buffer, "%" PRIu64 "", 29, current->menu[j].value);
							gtk_combo_box_text_append_text (
								GTK_COMBO_BOX_TEXT (widget), buffer);
						}
#endif
						if(current->value == current->menu[j].index)
							def = j;
					}

					gtk_combo_box_set_active (GTK_COMBO_BOX(widget), def);
					gtk_widget_show (widget);

					g_object_set_data (G_OBJECT (widget), "control_info",
						GINT_TO_POINTER(current->control.id));

					/*connect signal*/
					g_signal_connect (GTK_COMBO_BOX_TEXT(widget), "changed",
						G_CALLBACK (combo_changed), device);
				}
                break;

			case V4L2_CTRL_TYPE_BUTTON:

				widget = gtk_button_new_with_label(" ");
				gtk_widget_show (widget);

				g_object_set_data (G_OBJECT (widget), "control_info",
					GINT_TO_POINTER(current->control.id));

				g_signal_connect (GTK_BUTTON(widget), "clicked",
					G_CALLBACK (button_clicked), device);
                break;

            case V4L2_CTRL_TYPE_BOOLEAN:

				if(current->control.id ==V4L2_CID_DISABLE_PROCESSING_LOGITECH)
				{
					widget2 = gtk_combo_box_text_new ();

					gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget2),
						"GBGB... | RGRG...");
					gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget2),
						"GRGR... | BGBG...");
					gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget2),
						"BGBG... | GRGR...");
					gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget2),
						"RGRG... | GBGB...");

					device->bayer_pix_order = 0
					gtk_combo_box_set_active(GTK_COMBO_BOX(widget2), device->bayer_pix_order);

					gtk_widget_show (widget2);

					/*connect signal*/
					g_signal_connect (GTK_COMBO_BOX_TEXT (widget2), "changed",
						G_CALLBACK (bayer_pix_ord_changed), device);

					device->isbayer = (current->value ? TRUE : FALSE);
				}

				widget = gtk_check_button_new();
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					current->value ? TRUE : FALSE);
				gtk_widget_show (widget);

				g_object_set_data (G_OBJECT (widget), "control_info",
					GINT_TO_POINTER(current->control.id));

				/*connect signal*/
				g_signal_connect (GTK_TOGGLE_BUTTON(widget), "toggled",
					G_CALLBACK (check_changed), device);

                break;

			default:
				printf("control[%d]:(unknown - 0x%x) 0x%x '%s'\n",i ,control->control.type,
					control->control.id, control->control.name);
				break;
		}

		/*atach widgets to grid*/
		gtk_grid_attach(GTK_GRID(img_controls_grid), label, 0, i, 1 , 1);

		if(widget)
		{
			gtk_grid_attach(GTK_GRID(img_controls_grid), widget, 1, i, 1 , 1);
            gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
			gtk_widget_set_hexpand (widget, TRUE);
		}

		if(widget2)
		{
			gtk_grid_attach(GTK_GRID(img_controls_grid), widget2, 2, i, 1 , 1);
		}

        i++;
    }

	/*add control grid to parent container*/
	gtk_container_add(GTK_CONTAINER(parent), img_controls_grid);

	return 0;
}