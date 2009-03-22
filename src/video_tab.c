/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
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
#include <glib.h>
#include <glib/gprintf.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "globals.h"
#include "callbacks.h"
#include "v4l2uvc.h"
#include "string_utils.h"

/*--------------------------- file chooser dialog ----------------------------*/
static void
file_chooser (GtkButton * FileButt, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;

	gwidget->FileDialog = gtk_file_chooser_dialog_new (_("Save File"),
		GTK_WINDOW(gwidget->mainwin),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (gwidget->FileDialog), TRUE);

	int flag_avi = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (FileButt), "file_butt"));
	if(flag_avi) 
	{ /* avi File chooser*/
		const gchar *basename =  gtk_entry_get_text(GTK_ENTRY(gwidget->AVIFNameEntry));
		
		global->aviFPath=splitPath((gchar *) basename, global->aviFPath);
	
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (gwidget->FileDialog), 
			global->aviFPath[1]);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (gwidget->FileDialog),
			global->aviFPath[0]);

		if (gtk_dialog_run (GTK_DIALOG (gwidget->FileDialog)) == GTK_RESPONSE_ACCEPT)
		{
			gchar *fullname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (gwidget->FileDialog));
			global->aviFPath=splitPath(fullname, global->aviFPath);
			gtk_entry_set_text(GTK_ENTRY(gwidget->AVIFNameEntry)," ");
			gtk_entry_set_text(GTK_ENTRY(gwidget->AVIFNameEntry),global->aviFPath[0]);
			g_free(fullname);
		}
	}
	else 
	{	/* Image File chooser*/
		const gchar *basename =  gtk_entry_get_text(GTK_ENTRY(gwidget->ImageFNameEntry));

		global->imgFPath=splitPath((gchar *)basename, global->imgFPath);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (gwidget->FileDialog), 
			global->imgFPath[1]);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (gwidget->FileDialog), 
			global->imgFPath[0]);

		if (gtk_dialog_run (GTK_DIALOG (gwidget->FileDialog)) == GTK_RESPONSE_ACCEPT)
		{
			gchar *fullname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (gwidget->FileDialog));
			global->imgFPath=splitPath(fullname, global->imgFPath);
			g_free(fullname);
			gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry)," ");
			gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry),global->imgFPath[0]);
			/*get the file type*/
			global->imgFormat = check_image_type(global->imgFPath[0]);
			/*set the file type*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->ImageType),global->imgFormat);
		  
			if(global->image_inc>0)
			{ 
				global->image_inc=1; /*if auto naming restart counter*/
				g_snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
				gtk_label_set_text(GTK_LABEL(gwidget->ImageIncLabel), global->imageinc_str);
			}
		
		}
	}
	gtk_widget_destroy (gwidget->FileDialog);
}

void video_tab(struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct GWIDGET *gwidget = all_data->gwidget;
	struct paRecordData *pdata = all_data->pdata;
	
	GtkWidget *table2;
	GtkWidget *scroll2;
	GtkWidget *Tab2;
	GtkWidget *Tab2Label;
	GtkWidget *Tab2Icon;
	GtkWidget *label_Device;
	GtkWidget *FrameRate;
	GtkWidget *label_FPS;
	GtkWidget *ShowFPS;
	GtkWidget *labelResol;
	GtkWidget *ImpType;
	GtkWidget *label_ImpType;
	GtkWidget *label_ImgFile;
	GtkWidget *ImgFolder_img;
	GtkWidget *label_ImageType;
	GtkWidget *label_AVIFile;
	GtkWidget *VidFolder_img;
	GtkWidget *label_AVIComp;
	GtkWidget *label_videoFilters;
	GtkWidget *table_filt;
	GtkWidget *FiltMirrorEnable;
	GtkWidget *FiltUpturnEnable;
	GtkWidget *FiltNegateEnable;
	GtkWidget *FiltMonoEnable;
	GtkWidget *FiltPiecesEnable;
	
	int line = 0;
	int i = 0;
	
	//TABLE
	table2 = gtk_table_new(1,3,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table2), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table2), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table2), 2);
	gtk_widget_show (table2);
	//SCROLL
	scroll2 = gtk_scrolled_window_new(NULL,NULL);
	//ADD TABLE TO SCROLL
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll2),table2);
	gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroll2), GTK_CORNER_TOP_LEFT);
	gtk_widget_show(scroll2);
	
	//new hbox for tab label and icon
	Tab2 = gtk_hbox_new(FALSE,2);
	Tab2Label = gtk_label_new(_("Video & Files"));
	gtk_widget_show (Tab2Label);
	
	gchar* Tab2IconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/video_controls.png",NULL);
	//don't test for file - use default empty image if load fails
	//get icon image
	Tab2Icon = gtk_image_new_from_file(Tab2IconPath);
	g_free(Tab2IconPath);
	gtk_widget_show (Tab2Icon);
	gtk_box_pack_start (GTK_BOX(Tab2), Tab2Icon, FALSE, FALSE,1);
	gtk_box_pack_start (GTK_BOX(Tab2), Tab2Label, FALSE, FALSE,1);
	
	gtk_widget_show (Tab2);
	gtk_notebook_append_page(GTK_NOTEBOOK(gwidget->boxh),scroll2,Tab2);
	
	//Devices
	label_Device = gtk_label_new(_("Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_Device), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_Device, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_Device);
	
	
	gwidget->Devices = gtk_combo_box_new_text ();
	if (videoIn->listDevices->num_devices < 1)
	{
		//use current
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->Devices),
			videoIn->videodevice);
		gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Devices),0);
	}
	else
	{
		for(i=0;i<(videoIn->listDevices->num_devices);i++)
		{
			gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->Devices),
				videoIn->listDevices->listVidDevices[i].name);
			if(videoIn->listDevices->listVidDevices[i].current)
				gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Devices),i);
		}
	}
	gtk_table_attach(GTK_TABLE(table2), gwidget->Devices, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->Devices);
	g_signal_connect (GTK_COMBO_BOX(gwidget->Devices), "changed",
		G_CALLBACK (Devices_changed), all_data);
	
	// Resolution
	gwidget->Resolution = gtk_combo_box_new_text ();
	char temp_str[20];
	int defres=0;
	if (global->debug) 
		g_printf("resolutions of %dº format=%d \n",
			videoIn->listFormats->current_format+1,
			videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].numb_res);
	for(i=0;i<videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].numb_res;i++) 
	{
		if (videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[i].width>0)
		{
			g_snprintf(temp_str,18,"%ix%i",videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[i].width,
							 videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[i].height);
			gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->Resolution),temp_str);
			
			if ((global->width==videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[i].width) && 
				(global->height==videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[i].height))
					defres=i;//set selected resolution index
		}
	}
	
	// Frame Rate
	line++;
	videoIn->fps_num=global->fps_num;
	videoIn->fps=global->fps;
	input_set_framerate (videoIn);
					  
	FrameRate = gtk_combo_box_new_text ();
	int deffps=0;
	if (global->debug) 
		g_printf("frame rates of %dº resolution=%d \n",
			defres+1,
			videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[defres].numb_frates);
	for (i=0;i<videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[defres].numb_frates;i++) 
	{
		g_snprintf(temp_str,18,"%i/%i fps",videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[defres].framerate_num[i],
			videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[defres].framerate_denom[i]);
		gtk_combo_box_append_text(GTK_COMBO_BOX(FrameRate),temp_str);
		
		if ((videoIn->fps_num==videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[defres].framerate_num[i]) && 
			(videoIn->fps==videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[defres].framerate_denom[i]))
				deffps=i;//set selected
	}
	
	gtk_table_attach(GTK_TABLE(table2), FrameRate, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (FrameRate);
	
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(FrameRate),deffps);
	if (deffps==0) 
	{
		global->fps=videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[0].framerate_denom[0];
		global->fps_num=videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[0].framerate_num[0];
		videoIn->fps=global->fps;
		videoIn->fps_num=global->fps_num;
	}
	gtk_widget_set_sensitive (FrameRate, TRUE);
	g_signal_connect (GTK_COMBO_BOX(FrameRate), "changed",
		G_CALLBACK (FrameRate_changed), all_data);
	
	label_FPS = gtk_label_new(_("Frame Rate:"));
	gtk_misc_set_alignment (GTK_MISC (label_FPS), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_FPS, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_FPS);
	
	ShowFPS=gtk_check_button_new_with_label (_(" Show"));
	gtk_table_attach(GTK_TABLE(table2), ShowFPS, 2, 3, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ShowFPS),(global->FpsCount > 0));
	gtk_widget_show (ShowFPS);
	g_signal_connect (GTK_CHECK_BUTTON(ShowFPS), "toggled",
		G_CALLBACK (ShowFPS_changed), all_data);
	
	// add resolution combo box
	line++;
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Resolution),defres);
	
	if(global->debug) 
		g_printf("Def. Res: %i  numb. fps:%i\n",defres,videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[defres].numb_frates);
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->Resolution, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->Resolution);
	
	gtk_widget_set_sensitive (gwidget->Resolution, TRUE);
	g_signal_connect (gwidget->Resolution, "changed",
		G_CALLBACK (resolution_changed), all_data);
	
	labelResol = gtk_label_new(_("Resolution:"));
	gtk_misc_set_alignment (GTK_MISC (labelResol), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table2), labelResol, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (labelResol);
	
	// Input Format
	line++;
	ImpType= gtk_combo_box_new_text ();
	
	int fmtind=0;
	for (fmtind=0; fmtind < videoIn->listFormats->numb_formats; fmtind++)
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(ImpType),videoIn->listFormats->listVidFormats[fmtind].fourcc);
		if(global->format == videoIn->listFormats->listVidFormats[fmtind].format)
			gtk_combo_box_set_active(GTK_COMBO_BOX(ImpType),fmtind); /*set active*/
	}
	
	gtk_table_attach(GTK_TABLE(table2), ImpType, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_widget_set_sensitive (ImpType, TRUE);
	g_signal_connect (GTK_COMBO_BOX(ImpType), "changed",
		G_CALLBACK (ImpType_changed), all_data);
	gtk_widget_show (ImpType);
	
	label_ImpType = gtk_label_new(_("Camera Output:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImpType), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_ImpType, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_ImpType);
	
	// Image Capture
	line++;
	label_ImgFile = gtk_label_new(_("Image File:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImgFile), 1, 0.5);
	    
	gwidget->ImageFNameEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry),global->imgFPath[0]);
	gtk_table_attach(GTK_TABLE(table2), label_ImgFile, 0, 1, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->ImageFNameEntry, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gwidget->ImgFileButt=gtk_button_new_from_stock(GTK_STOCK_OPEN); 
	gchar* OImgIconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/images_folder.png",NULL);
	//don't test for file - use default empty image if load fails
	//get icon image
	ImgFolder_img = gtk_image_new_from_file(OImgIconPath);
	g_free(OImgIconPath);
	gtk_button_set_image (GTK_BUTTON(gwidget->ImgFileButt), ImgFolder_img);
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->ImgFileButt, 2, 3, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->ImgFileButt);
	
	//incremental capture
	line++;
	gwidget->ImageIncLabel=gtk_label_new(global->imageinc_str);
	gtk_misc_set_alignment (GTK_MISC (gwidget->ImageIncLabel), 0, 0.5);
	gtk_table_attach (GTK_TABLE(table2), gwidget->ImageIncLabel, 1, 2, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->ImageIncLabel);
	
	gwidget->ImageInc=gtk_check_button_new_with_label (_("File,Auto"));
	gtk_table_attach(GTK_TABLE(table2), gwidget->ImageInc, 2, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->ImageInc),(global->image_inc > 0));
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->ImageInc), "toggled",
		G_CALLBACK (ImageInc_changed), all_data);
	gtk_widget_show (gwidget->ImageInc);
	
	//image format
	line++;
	label_ImageType=gtk_label_new(_("Image Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImageType), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table2), label_ImageType, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (label_ImageType);
	
	gwidget->ImageType=gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->ImageType),"JPG");
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->ImageType),"BMP");
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->ImageType),"PNG");
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->ImageType),"RAW");
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->ImageType),global->imgFormat);
	gtk_table_attach(GTK_TABLE(table2), gwidget->ImageType, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->ImageType);
	
	gtk_widget_show (label_ImgFile);
	gtk_widget_show (gwidget->ImageFNameEntry);
	gtk_widget_show (gwidget->ImageType);
	
	g_signal_connect (GTK_COMBO_BOX(gwidget->ImageType), "changed",
		G_CALLBACK (ImageType_changed), all_data);
	g_object_set_data (G_OBJECT (gwidget->ImgFileButt), "file_butt", GINT_TO_POINTER(0));
	g_signal_connect (GTK_BUTTON(gwidget->ImgFileButt), "clicked",
		 G_CALLBACK (file_chooser), all_data);

	//AVI Capture
	line++;
	label_AVIFile= gtk_label_new(_("AVI File:"));
	gtk_misc_set_alignment (GTK_MISC (label_AVIFile), 1, 0.5);
	gwidget->AVIFNameEntry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table2), label_AVIFile, 0, 1, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table2), gwidget->AVIFNameEntry, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gwidget->AviFileButt=gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gchar* OVidIconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/videos_folder.png",NULL);
	//don't t1est for file - use default empty image if load fails
	//get icon image
	VidFolder_img = gtk_image_new_from_file(OVidIconPath);
	g_free(OVidIconPath);
	
	gtk_button_set_image (GTK_BUTTON(gwidget->AviFileButt), VidFolder_img);
	gtk_table_attach(GTK_TABLE(table2), gwidget->AviFileButt, 2, 3, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_widget_show (gwidget->AviFileButt);
	gtk_widget_show (label_AVIFile);
	gtk_widget_show (gwidget->AVIFNameEntry);
	g_object_set_data (G_OBJECT (gwidget->AviFileButt), "file_butt", GINT_TO_POINTER(1));
	g_signal_connect (GTK_BUTTON(gwidget->AviFileButt), "clicked",
		G_CALLBACK (file_chooser), all_data);
	
	if (global->avifile) 
	{	
		//avi capture enabled from start
		gtk_entry_set_text(GTK_ENTRY(gwidget->AVIFNameEntry),global->avifile);
	} 
	else 
	{
		videoIn->capAVI = FALSE;
		pdata->capAVI = videoIn->capAVI;
		gtk_entry_set_text(GTK_ENTRY(gwidget->AVIFNameEntry),global->aviFPath[0]);
	}
	
	//incremental capture
	line++;
	gwidget->AVIIncLabel=gtk_label_new(global->aviinc_str);
	gtk_misc_set_alignment (GTK_MISC (gwidget->AVIIncLabel), 0, 0.5);
	gtk_table_attach (GTK_TABLE(table2), gwidget->AVIIncLabel, 1, 2, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->AVIIncLabel);
	
	gwidget->AVIInc=gtk_check_button_new_with_label (_("File,Auto"));
	gtk_table_attach(GTK_TABLE(table2), gwidget->AVIInc, 2, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->AVIInc),(global->avi_inc > 0));
	
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->AVIInc), "toggled",
		G_CALLBACK (AVIInc_changed), all_data);
	gtk_widget_show (gwidget->AVIInc);
	
	// AVI Compressor
	line++;
	gwidget->AVIComp = gtk_combo_box_new_text ();
	
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->AVIComp),_("MJPG - compressed"));
	if(videoIn->formatIn == V4L2_PIX_FMT_UYVY)
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->AVIComp),_("UYVY - uncomp YUV"));
	else
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->AVIComp),_("YUY2 - uncomp YUV"));
	
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->AVIComp),_("RGB - uncomp BMP"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->AVIComp),_("MPEG"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->AVIComp),_("FLV1"));
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->AVIComp, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->AVIComp);
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->AVIComp),global->AVIFormat);
	
	gtk_widget_set_sensitive (gwidget->AVIComp, TRUE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->AVIComp), "changed",
		G_CALLBACK (AVIComp_changed), all_data);
	
	label_AVIComp = gtk_label_new(_("AVI Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_AVIComp), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_AVIComp, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (label_AVIComp);
	
	// Filter controls 
	line++;
	label_videoFilters = gtk_label_new(_("---- Video Filters ----"));
	gtk_misc_set_alignment (GTK_MISC (label_videoFilters), 0.5, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_videoFilters, 0, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL , 0, 0, 0);
	gtk_widget_show (label_videoFilters);
	
	line++;
	table_filt = gtk_table_new(1,4,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table_filt), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table_filt), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table_filt), 4);
	gtk_widget_set_size_request (table_filt, -1, -1);
	
	// Mirror
	FiltMirrorEnable=gtk_check_button_new_with_label (_(" Mirror"));
	g_object_set_data (G_OBJECT (FiltMirrorEnable), "filt_info", GINT_TO_POINTER(YUV_MIRROR));
	gtk_table_attach(GTK_TABLE(table_filt), FiltMirrorEnable, 0, 1, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltMirrorEnable),(global->Frame_Flags & YUV_MIRROR)>0);
	gtk_widget_show (FiltMirrorEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltMirrorEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);
	// Upturn
	FiltUpturnEnable=gtk_check_button_new_with_label (_(" Invert"));
	g_object_set_data (G_OBJECT (FiltUpturnEnable), "filt_info", GINT_TO_POINTER(YUV_UPTURN));
	gtk_table_attach(GTK_TABLE(table_filt), FiltUpturnEnable, 1, 2, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltUpturnEnable),(global->Frame_Flags & YUV_UPTURN)>0);
	gtk_widget_show (FiltUpturnEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltUpturnEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);
	// Negate
	FiltNegateEnable=gtk_check_button_new_with_label (_(" Negative"));
	g_object_set_data (G_OBJECT (FiltNegateEnable), "filt_info", GINT_TO_POINTER(YUV_NEGATE));
	gtk_table_attach(GTK_TABLE(table_filt), FiltNegateEnable, 2, 3, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltNegateEnable),(global->Frame_Flags & YUV_NEGATE)>0);
	gtk_widget_show (FiltNegateEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltNegateEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);
	// Mono
	FiltMonoEnable=gtk_check_button_new_with_label (_(" Mono"));
	g_object_set_data (G_OBJECT (FiltMonoEnable), "filt_info", GINT_TO_POINTER(YUV_MONOCR));
	gtk_table_attach(GTK_TABLE(table_filt), FiltMonoEnable, 3, 4, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltMonoEnable),(global->Frame_Flags & YUV_MONOCR)>0);
	gtk_widget_show (FiltMonoEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltMonoEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);
	
	// Pieces
	FiltPiecesEnable=gtk_check_button_new_with_label (_(" Pieces"));
	g_object_set_data (G_OBJECT (FiltPiecesEnable), "filt_info", GINT_TO_POINTER(YUV_PIECES));
	gtk_table_attach(GTK_TABLE(table_filt), FiltPiecesEnable, 0, 1, 1, 2,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltPiecesEnable),(global->Frame_Flags & YUV_PIECES)>0);
	gtk_widget_show (FiltPiecesEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltPiecesEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);
	
	gtk_table_attach (GTK_TABLE(table2), table_filt, 0, 3, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (table_filt);
	
}
