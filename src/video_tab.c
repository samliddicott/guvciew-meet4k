/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                                                                               #
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
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "globals.h"
#include "callbacks.h"
#include "v4l2uvc.h"
#include "string_utils.h"
#include "vcodecs.h"
#include "video_format.h"
/*--------------------------- file chooser dialog ----------------------------*/
void video_tab(struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct GWIDGET *gwidget = all_data->gwidget;
	//struct paRecordData *pdata = all_data->pdata;

	GtkWidget *table2;
	GtkWidget *scroll2;
	GtkWidget *Tab2;
	GtkWidget *Tab2Label;
	GtkWidget *Tab2Icon;
	GtkWidget *label_Device;
	//GtkWidget *FrameRate;
	GtkWidget *label_FPS;
	GtkWidget *ShowFPS;
	GtkWidget *labelResol;
	GtkWidget *label_InpType;
	GtkWidget *label_ImgFile;
	GtkWidget *ImgFolder_img;
	GtkWidget *label_ImageType;
	GtkWidget *label_VidFile;
	GtkWidget *label_VidFormat;
	GtkWidget *VidFolder_img;
	GtkWidget *label_VidCodec;
	GtkWidget *label_videoFilters;
	GtkWidget *table_filt;
	GtkWidget *FiltMirrorEnable;
	GtkWidget *FiltUpturnEnable;
	GtkWidget *FiltNegateEnable;
	GtkWidget *FiltMonoEnable;
	GtkWidget *FiltPiecesEnable;
	GtkWidget *FiltParticlesEnable;
	GtkWidget *set_jpeg_comp;
	GtkWidget *label_jpeg_comp;

	int line = 0;
	int i = 0;
	VidFormats *listVidFormats;

	//TABLE
	table2 = gtk_grid_new();
	gtk_grid_set_column_homogeneous (GTK_GRID(table2), FALSE);
	gtk_widget_set_hexpand (table2, TRUE);
	gtk_widget_set_halign (table2, GTK_ALIGN_FILL);

	gtk_grid_set_row_spacing (GTK_GRID(table2), 4);
	gtk_grid_set_column_spacing (GTK_GRID (table2), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table2), 2);
	gtk_widget_show (table2);
	//SCROLL
	scroll2 = gtk_scrolled_window_new(NULL,NULL);
	//ADD TABLE TO SCROLL
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll2),table2);
	gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroll2), GTK_CORNER_TOP_LEFT);
	gtk_widget_show(scroll2);

	//new hbox for tab label and icon
	Tab2 = gtk_grid_new();
	Tab2Label = gtk_label_new(_("Video & Files"));
	gtk_widget_show (Tab2Label);

	gchar* Tab2IconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/video_controls.png",NULL);
	//don't test for file - use default empty image if load fails
	//get icon image
	Tab2Icon = gtk_image_new_from_file(Tab2IconPath);
	g_free(Tab2IconPath);
	gtk_widget_show (Tab2Icon);
	gtk_grid_attach (GTK_GRID(Tab2), Tab2Icon, 0, 0, 1, 1);
	gtk_grid_attach (GTK_GRID(Tab2), Tab2Label, 1, 0, 1, 1);

	gtk_widget_show (Tab2);
	gtk_notebook_append_page(GTK_NOTEBOOK(gwidget->boxh),scroll2,Tab2);

	//Devices
	label_Device = gtk_label_new(_("Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_Device), 1, 0.5);

	gtk_grid_attach (GTK_GRID(table2), label_Device, 0, line, 1, 1);

	gtk_widget_show (label_Device);


	gwidget->Devices = gtk_combo_box_text_new ();
	gtk_widget_set_halign (gwidget->Devices, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->Devices, TRUE);
	if (videoIn->listDevices->num_devices < 1)
	{
		//use current
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->Devices),
			videoIn->videodevice);
		gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Devices),0);
	}
	else
	{
		for(i=0;i<(videoIn->listDevices->num_devices);i++)
		{
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->Devices),
				videoIn->listDevices->listVidDevices[i].name);
			if(videoIn->listDevices->listVidDevices[i].current)
				gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Devices),i);
		}
	}
	gtk_grid_attach (GTK_GRID(table2), gwidget->Devices, 1, line, 1 ,1);
	gtk_widget_show (gwidget->Devices);
	g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->Devices), "changed",
		G_CALLBACK (Devices_changed), all_data);

	// Resolution
	gwidget->Resolution = gtk_combo_box_text_new ();
	gtk_widget_set_halign (gwidget->Resolution, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->Resolution, TRUE);
	char temp_str[20];
	int defres=0;

	{
		int current_format = videoIn->listFormats->current_format;
		listVidFormats = &videoIn->listFormats->listVidFormats[current_format];
	}


	if (global->debug)
		g_print("resolutions of format(%d) = %d \n",
			videoIn->listFormats->current_format+1,
			listVidFormats->numb_res);
	for(i = 0 ; i < listVidFormats->numb_res ; i++)
	{
		if (listVidFormats->listVidCap[i].width>0)
		{
			g_snprintf(temp_str,18,"%ix%i", listVidFormats->listVidCap[i].width,
							 listVidFormats->listVidCap[i].height);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->Resolution),temp_str);

			if ((global->width == listVidFormats->listVidCap[i].width) &&
				(global->height == listVidFormats->listVidCap[i].height))
					defres=i;//set selected resolution index
		}
	}

	// Frame Rate
	line++;

	gwidget->FrameRate = gtk_combo_box_text_new ();
	gtk_widget_set_halign (gwidget->FrameRate, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->FrameRate, TRUE);
	int deffps=0;
	if (global->debug)
		g_print("frame rates of %dº resolution=%d \n",
			defres+1,
			listVidFormats->listVidCap[defres].numb_frates);
	for ( i = 0 ; i < listVidFormats->listVidCap[defres].numb_frates ; i++)
	{
		g_snprintf(temp_str,18,"%i/%i fps", listVidFormats->listVidCap[defres].framerate_denom[i],
			listVidFormats->listVidCap[defres].framerate_num[i]);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->FrameRate),temp_str);

		if (( global->fps_num == listVidFormats->listVidCap[defres].framerate_num[i]) &&
			(global->fps == listVidFormats->listVidCap[defres].framerate_denom[i]))
				deffps=i;//set selected
	}

	gtk_grid_attach (GTK_GRID(table2), gwidget->FrameRate, 1, line, 1 ,1);
	gtk_widget_show (gwidget->FrameRate);


	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->FrameRate),deffps);
	if (deffps==0)
	{
		if (listVidFormats->listVidCap[defres].framerate_denom)
			global->fps = listVidFormats->listVidCap[defres].framerate_denom[0];

		if (listVidFormats->listVidCap[defres].framerate_num)
			global->fps_num = listVidFormats->listVidCap[defres].framerate_num[0];

		g_print("fps is set to %i/%i\n", global->fps_num, global->fps);
	}
	gtk_widget_set_sensitive (gwidget->FrameRate, TRUE);
	g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->FrameRate), "changed",
		G_CALLBACK (FrameRate_changed), all_data);

	label_FPS = gtk_label_new(_("Frame Rate:"));
	gtk_misc_set_alignment (GTK_MISC (label_FPS), 1, 0.5);

	gtk_grid_attach (GTK_GRID(table2), label_FPS, 0, line, 1, 1);

	gtk_widget_show (label_FPS);

	ShowFPS=gtk_check_button_new_with_label (_(" Show"));
	gtk_grid_attach (GTK_GRID(table2), ShowFPS, 2, line, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ShowFPS),(global->FpsCount > 0));
	gtk_widget_show (ShowFPS);
	g_signal_connect (GTK_CHECK_BUTTON(ShowFPS), "toggled",
		G_CALLBACK (ShowFPS_changed), all_data);

	// add resolution combo box
	line++;
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Resolution),defres);

	if(global->debug)
		g_print("Def. Res: %i  numb. fps:%i\n",defres,videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[defres].numb_frates);

	gtk_grid_attach (GTK_GRID(table2), gwidget->Resolution, 1, line, 1 ,1);
	gtk_widget_show (gwidget->Resolution);

	gtk_widget_set_sensitive (gwidget->Resolution, TRUE);
	g_signal_connect (gwidget->Resolution, "changed",
		G_CALLBACK (resolution_changed), all_data);

	labelResol = gtk_label_new(_("Resolution:"));
	gtk_misc_set_alignment (GTK_MISC (labelResol), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table2), labelResol, 0, line, 1, 1);
	gtk_widget_show (labelResol);

	// Input Format
	line++;
	gwidget->InpType= gtk_combo_box_text_new ();
	gtk_widget_set_halign (gwidget->InpType, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->InpType, TRUE);

	int fmtind=0;
	for (fmtind=0; fmtind < videoIn->listFormats->numb_formats; fmtind++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->InpType),videoIn->listFormats->listVidFormats[fmtind].fourcc);
		if(global->format == videoIn->listFormats->listVidFormats[fmtind].format)
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->InpType),fmtind); /*set active*/
	}

	gtk_grid_attach (GTK_GRID(table2), gwidget->InpType, 1, line, 1 ,1);

	gtk_widget_set_sensitive (gwidget->InpType, TRUE);
	g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->InpType), "changed",
		G_CALLBACK (InpType_changed), all_data);
	gtk_widget_show (gwidget->InpType);

	label_InpType = gtk_label_new(_("Camera Output:"));
	gtk_misc_set_alignment (GTK_MISC (label_InpType), 1, 0.5);

	gtk_grid_attach (GTK_GRID(table2), label_InpType, 0, line, 1, 1);

	gtk_widget_show (label_InpType);

	//jpeg compression quality for MJPEG and JPEG input formats
	if((global->format == V4L2_PIX_FMT_MJPEG || global->format == V4L2_PIX_FMT_JPEG) && videoIn->jpgcomp.quality > 0)
	{
		line++;
		gwidget->jpeg_comp = gtk_spin_button_new_with_range(0,100,1);
		gtk_widget_set_halign (gwidget->jpeg_comp, GTK_ALIGN_FILL);
		gtk_widget_set_hexpand (gwidget->jpeg_comp, TRUE);
		/*can't edit the spin value by hand*/
		gtk_editable_set_editable(GTK_EDITABLE(gwidget->jpeg_comp),FALSE);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON(gwidget->jpeg_comp), videoIn->jpgcomp.quality);
		gtk_grid_attach (GTK_GRID(table2), gwidget->jpeg_comp, 1, line, 1 ,1);

		gtk_widget_set_sensitive (gwidget->jpeg_comp, TRUE);
		gtk_widget_show (gwidget->jpeg_comp);

		set_jpeg_comp = gtk_button_new_with_label(_("Apply"));
		gtk_grid_attach (GTK_GRID(table2), set_jpeg_comp, 2, line, 1 ,1);
		g_signal_connect (GTK_BUTTON (set_jpeg_comp), "clicked",
				G_CALLBACK (set_jpeg_comp_clicked), all_data);
		gtk_widget_set_sensitive (set_jpeg_comp, TRUE);
		gtk_widget_show (set_jpeg_comp);

		label_jpeg_comp = gtk_label_new(_("Quality:"));
		gtk_misc_set_alignment (GTK_MISC (label_jpeg_comp), 1, 0.5);

		gtk_grid_attach (GTK_GRID(table2), label_jpeg_comp, 0, line, 1 ,1);

		gtk_widget_show (label_jpeg_comp);
	}
	// Image Capture
	line++;
	label_ImgFile = gtk_label_new(_("Image File:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImgFile), 1, 0.5);

	gwidget->ImageFNameEntry = gtk_entry_new();
	gtk_widget_set_halign (gwidget->ImageFNameEntry, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->ImageFNameEntry, TRUE);
	gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry),global->imgFPath[0]);
	gtk_grid_attach (GTK_GRID(table2), label_ImgFile, 0, line, 1 ,1);

	gtk_grid_attach (GTK_GRID(table2), gwidget->ImageFNameEntry, 1, line, 1 ,1);

	gwidget->ImgFileButt=gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gchar* OImgIconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/images_folder.png",NULL);
	//don't test for file - use default empty image if load fails
	//get icon image
	ImgFolder_img = gtk_image_new_from_file(OImgIconPath);
	g_free(OImgIconPath);
	gtk_button_set_image (GTK_BUTTON(gwidget->ImgFileButt), ImgFolder_img);

	gtk_grid_attach (GTK_GRID(table2), gwidget->ImgFileButt, 2, line, 1, 1);

	gtk_widget_show (ImgFolder_img);
	gtk_widget_show (gwidget->ImgFileButt);

	//incremental capture
	line++;
	gwidget->ImageIncLabel=gtk_label_new(global->imageinc_str);
	gtk_misc_set_alignment (GTK_MISC (gwidget->ImageIncLabel), 0, 0.5);
	gtk_grid_attach (GTK_GRID(table2), gwidget->ImageIncLabel, 1, line, 1 ,1);
	gtk_widget_show (gwidget->ImageIncLabel);

	gwidget->ImageInc=gtk_check_button_new_with_label (_("File,Auto"));
	gtk_grid_attach (GTK_GRID(table2), gwidget->ImageInc, 2, line, 1 ,1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->ImageInc),(global->image_inc > 0));
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->ImageInc), "toggled",
		G_CALLBACK (ImageInc_changed), all_data);
	gtk_widget_show (gwidget->ImageInc);

	//image format
	line++;
	label_ImageType=gtk_label_new(_("Image Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImageType), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table2), label_ImageType, 0, line, 1, 1);
	gtk_widget_show (label_ImageType);

	gwidget->ImageType=gtk_combo_box_text_new ();
	gtk_widget_set_halign (gwidget->ImageType, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->ImageType, TRUE);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->ImageType),"JPG");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->ImageType),"BMP");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->ImageType),"PNG");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->ImageType),"RAW");
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->ImageType),global->imgFormat);
	gtk_grid_attach (GTK_GRID(table2), gwidget->ImageType, 1, line, 1 ,1);
	gtk_widget_show (gwidget->ImageType);

	gtk_widget_show (label_ImgFile);
	gtk_widget_show (gwidget->ImageFNameEntry);
	gtk_widget_show (gwidget->ImageType);

	g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->ImageType), "changed",
		G_CALLBACK (ImageType_changed), all_data);
	g_object_set_data (G_OBJECT (gwidget->ImgFileButt), "file_butt", GINT_TO_POINTER(0));
	g_signal_connect (GTK_BUTTON(gwidget->ImgFileButt), "clicked",
		 G_CALLBACK (file_chooser), all_data);

	gwidget->TakeImageByDefault = gtk_radio_button_new_with_label (NULL, _("Take Picture by Default"));

	gtk_grid_attach (GTK_GRID(table2), gwidget->TakeImageByDefault, 2, line, 1 ,1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->TakeImageByDefault),(global->default_action == 0));
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->TakeImageByDefault), "toggled",
		G_CALLBACK (TakePictureByDefault_clicked), all_data);
	gtk_widget_show (gwidget->TakeImageByDefault);

	//Video Capture
	line++;
	label_VidFile= gtk_label_new(_("Video File:"));
	gtk_misc_set_alignment (GTK_MISC (label_VidFile), 1, 0.5);
	gwidget->VidFNameEntry = gtk_entry_new();
	gtk_widget_set_halign (gwidget->VidFNameEntry, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->VidFNameEntry, TRUE);
	gtk_grid_attach (GTK_GRID(table2), label_VidFile, 0, line, 1 ,1);
	gtk_grid_attach (GTK_GRID(table2), gwidget->VidFNameEntry, 1, line, 1 ,1);

	gwidget->VidFileButt=gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gchar* OVidIconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/videos_folder.png",NULL);
	//don't test for file - use default empty image if load fails
	//get icon image
	VidFolder_img = gtk_image_new_from_file(OVidIconPath);
	g_free(OVidIconPath);

	gtk_button_set_image (GTK_BUTTON(gwidget->VidFileButt), VidFolder_img);
	gtk_grid_attach (GTK_GRID(table2), gwidget->VidFileButt, 2, line, 1, 1);

	gtk_widget_show (VidFolder_img);
	gtk_widget_show (gwidget->VidFileButt);
	gtk_widget_show (label_VidFile);
	gtk_widget_show (gwidget->VidFNameEntry);
	g_object_set_data (G_OBJECT (gwidget->VidFileButt), "file_butt", GINT_TO_POINTER(1));
	g_signal_connect (GTK_BUTTON(gwidget->VidFileButt), "clicked",
		G_CALLBACK (file_chooser), all_data);

	if (global->vidfile)
	{
		//video capture enabled from start
		gtk_entry_set_text(GTK_ENTRY(gwidget->VidFNameEntry),global->vidfile);
	}
	else
	{
		gtk_entry_set_text(GTK_ENTRY(gwidget->VidFNameEntry),global->vidFPath[0]);
	}

	//incremental capture
	line++;
	gwidget->VidIncLabel=gtk_label_new(global->vidinc_str);
	gtk_misc_set_alignment (GTK_MISC (gwidget->VidIncLabel), 0, 0.5);
	gtk_grid_attach (GTK_GRID(table2), gwidget->VidIncLabel, 1, line, 1 ,1);
	gtk_widget_show (gwidget->VidIncLabel);

	gwidget->VidInc=gtk_check_button_new_with_label (_("File,Auto"));
	gtk_grid_attach (GTK_GRID(table2), gwidget->VidInc, 2, line, 1 ,1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->VidInc),(global->vid_inc > 0));

	g_signal_connect (GTK_CHECK_BUTTON(gwidget->VidInc), "toggled",
		G_CALLBACK (VidInc_changed), all_data);
	gtk_widget_show (gwidget->VidInc);

	/** video codec
	 * this is now done in the video menu
	 */
	//line++;
	//gwidget->VidCodec = gtk_combo_box_text_new ();
	//gtk_widget_set_halign (gwidget->VidCodec, GTK_ALIGN_FILL);
	//gtk_widget_set_hexpand (gwidget->VidCodec, TRUE);

	/** sets to valid only existing codecs
	 */ 
	//int num_codecs = setVcodecVal();
	//int vcodec_ind =0;
	//for (vcodec_ind =0; vcodec_ind < num_codecs; vcodec_ind++)
	//	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->VidCodec),gettext(get_desc4cc(vcodec_ind)));

	//gtk_grid_attach (GTK_GRID(table2), gwidget->VidCodec, 1, line, 1 ,1);
	//gtk_widget_show (gwidget->VidCodec);

	//gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->VidCodec), global->VidCodec);

	//gtk_widget_set_sensitive (gwidget->VidCodec, TRUE);
	//g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->VidCodec), "changed",
	//	G_CALLBACK (VidCodec_changed), all_data);

	//label_VidCodec = gtk_label_new(_("Video Codec:"));
	//gtk_misc_set_alignment (GTK_MISC (label_VidCodec), 1, 0.5);

	//gtk_grid_attach (GTK_GRID(table2), label_VidCodec, 0, line, 1, 1);
	//gtk_widget_show (label_VidCodec);

	/**lavc codec properties button
	 * this is now done in the video menu
	 */ 
	
	//gwidget->lavc_button = gtk_button_new_with_label (_("properties"));
	//gtk_grid_attach (GTK_GRID(table2), gwidget->lavc_button, 2, line, 1 ,1);
	//gtk_widget_show (gwidget->lavc_button);
	//g_signal_connect (GTK_BUTTON(gwidget->lavc_button), "clicked",
	//	G_CALLBACK (lavc_properties), all_data);
	//gtk_widget_set_sensitive (gwidget->lavc_button, isLavcCodec(global->VidCodec));


	//video container
	line++;
	gwidget->VidFormat = gtk_combo_box_text_new ();
	gtk_widget_set_halign (gwidget->VidFormat, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->VidFormat, TRUE);

	int vformat_ind =0;
	for (vformat_ind =0; vformat_ind<MAX_VFORMATS; vformat_ind++)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->VidFormat),gettext(get_vformat_desc(vformat_ind)));

	gtk_grid_attach (GTK_GRID(table2), gwidget->VidFormat, 1, line, 1 ,1);
	gtk_widget_show (gwidget->VidFormat);

	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->VidFormat),global->VidFormat);

	gtk_widget_set_sensitive (gwidget->VidFormat, TRUE);
	g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->VidFormat), "changed",
		G_CALLBACK (VidFormat_changed), all_data);

	label_VidFormat = gtk_label_new(_("Video Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_VidFormat), 1, 0.5);

	gtk_grid_attach (GTK_GRID(table2), label_VidFormat, 0, line, 1, 1);
	gtk_widget_show (label_VidFormat);

	gwidget->TakeVidByDefault =gtk_radio_button_new_with_label (gtk_radio_button_get_group (GTK_RADIO_BUTTON (gwidget->TakeImageByDefault)), _("Take Video by Default"));
	gtk_grid_attach (GTK_GRID(table2), gwidget->TakeVidByDefault, 2, line, 1 ,1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->TakeVidByDefault),(global->default_action == 1));
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->TakeVidByDefault), "toggled",
		G_CALLBACK (TakeVidByDefault_clicked), all_data);
	gtk_widget_show (gwidget->TakeVidByDefault);

	// Filter controls
	line++;
	label_videoFilters = gtk_label_new(_("---- Video Filters ----"));
	gtk_misc_set_alignment (GTK_MISC (label_videoFilters), 0.5, 0.5);

	gtk_grid_attach (GTK_GRID(table2), label_videoFilters, 0, line, 3, 1);
	gtk_widget_show (label_videoFilters);

	line++;
	table_filt = gtk_grid_new();
	gtk_grid_set_row_spacing (GTK_GRID (table_filt), 4);
	gtk_grid_set_column_spacing (GTK_GRID (table_filt), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table_filt), 4);
	gtk_widget_set_size_request (table_filt, -1, -1);

	gtk_widget_set_halign (table_filt, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (table_filt, TRUE);
	gtk_grid_attach (GTK_GRID(table2), table_filt, 0, line, 3, 1);
	gtk_widget_show (table_filt);

	// Mirror
	FiltMirrorEnable=gtk_check_button_new_with_label (_(" Mirror"));
	g_object_set_data (G_OBJECT (FiltMirrorEnable), "filt_info", GINT_TO_POINTER(YUV_MIRROR));
	gtk_widget_set_halign (FiltMirrorEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltMirrorEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltMirrorEnable, 0, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltMirrorEnable),(global->Frame_Flags & YUV_MIRROR)>0);
	gtk_widget_show (FiltMirrorEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltMirrorEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);
	// Upturn
	FiltUpturnEnable=gtk_check_button_new_with_label (_(" Invert"));
	g_object_set_data (G_OBJECT (FiltUpturnEnable), "filt_info", GINT_TO_POINTER(YUV_UPTURN));
	gtk_widget_set_halign (FiltUpturnEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltUpturnEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltUpturnEnable, 1, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltUpturnEnable),(global->Frame_Flags & YUV_UPTURN)>0);
	gtk_widget_show (FiltUpturnEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltUpturnEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);
	// Negate
	FiltNegateEnable=gtk_check_button_new_with_label (_(" Negative"));
	g_object_set_data (G_OBJECT (FiltNegateEnable), "filt_info", GINT_TO_POINTER(YUV_NEGATE));
	gtk_widget_set_halign (FiltNegateEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltNegateEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltNegateEnable, 2, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltNegateEnable),(global->Frame_Flags & YUV_NEGATE)>0);
	gtk_widget_show (FiltNegateEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltNegateEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);
	// Mono
	FiltMonoEnable=gtk_check_button_new_with_label (_(" Mono"));
	g_object_set_data (G_OBJECT (FiltMonoEnable), "filt_info", GINT_TO_POINTER(YUV_MONOCR));
	gtk_widget_set_halign (FiltMonoEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltMonoEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltMonoEnable, 3, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltMonoEnable),(global->Frame_Flags & YUV_MONOCR)>0);
	gtk_widget_show (FiltMonoEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltMonoEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);

	// Pieces
	FiltPiecesEnable=gtk_check_button_new_with_label (_(" Pieces"));
	g_object_set_data (G_OBJECT (FiltPiecesEnable), "filt_info", GINT_TO_POINTER(YUV_PIECES));
	gtk_widget_set_halign (FiltPiecesEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltPiecesEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltPiecesEnable, 4, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltPiecesEnable),(global->Frame_Flags & YUV_PIECES)>0);
	gtk_widget_show (FiltPiecesEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltPiecesEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);

	// Particles
	FiltParticlesEnable=gtk_check_button_new_with_label (_(" Particles"));
	g_object_set_data (G_OBJECT (FiltParticlesEnable), "filt_info", GINT_TO_POINTER(YUV_PARTICLES));
	gtk_widget_set_halign (FiltParticlesEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltParticlesEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltParticlesEnable, 5, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltParticlesEnable),(global->Frame_Flags & YUV_PARTICLES)>0);
	gtk_widget_show (FiltParticlesEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltParticlesEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);

}
