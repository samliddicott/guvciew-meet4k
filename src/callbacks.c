/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
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

#include <SDL/SDL.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <glib.h>
#include <glib/gprintf.h>

#include <portaudio.h>

#include "v4l2uvc.h"
#include "avilib.h"
#include "globals.h"
#include "sound.h"
#include "ms_time.h"
#include "string_utils.h"
#include "video.h"
#include "mp2.h"
#include "profile.h"
#include "close.h"
#include "timers.h"
#include "callbacks.h"
#include "vcodecs.h"
#include "lavc_common.h"
#include "create_video.h"
#include "video_format.h"
/*---------------------------- error message dialog-----------------------------*/
void 
ERR_DIALOG(const char *err_title, const char* err_msg, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct VidState *s = all_data->s;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct focusData *AFdata = all_data->AFdata;
	struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	
	gboolean control_only = global->control_only;
	
	GtkWidget *errdialog;
	errdialog = gtk_message_dialog_new (GTK_WINDOW(gwidget->mainwin),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CLOSE,
		"%s",gettext(err_title));

	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(errdialog),
		"%s",gettext(err_msg));

	gtk_widget_show(errdialog);
	gtk_dialog_run (GTK_DIALOG (errdialog));
	gtk_widget_destroy (errdialog);

	g_free(gwidget);
	gwidget = NULL;
	all_data->gwidget = NULL;

	g_free(AFdata);
	AFdata = NULL;
	all_data->AFdata = NULL;

	g_free(s);
	s = NULL;
	all_data->s = NULL;

	if(videoIn->fd > 0) close_v4l2(videoIn, control_only);
	else g_free(videoIn);
	videoIn=NULL;
	all_data->videoIn = NULL;
	
	if (global) closeGlobals (global);
	global = NULL;
	all_data->global = NULL;

	g_free(pdata);
	pdata = NULL;
	all_data->pdata = NULL;    

	g_free(videoF);
	videoF = NULL;
	all_data->videoF = NULL;

	g_printf("Terminated.\n");;
	exit(1);
};


/*------------------------------ Event handlers -------------------------------*/
/* window close */
gint
delete_event (GtkWidget *widget, GdkEventConfigure *event, void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;

	shutd(0, all_data);//shutDown
	return 0;
}

/*image controls*/
void 
set_sensitive_img_contrls (const int flag, struct GWIDGET *gwidget)
{
	gtk_widget_set_sensitive(gwidget->ImgFileButt, flag);/*image butt File chooser*/
	gtk_widget_set_sensitive(gwidget->ImageType, flag);/*file type combo*/
	gtk_widget_set_sensitive(gwidget->ImageFNameEntry, flag);/*Image Entry*/
	gtk_widget_set_sensitive(gwidget->ImageInc, flag);/*image inc checkbox*/
}

/*----------------------------- Callbacks ------------------------------------*/
/*slider controls callback*/
void
slider_changed (GtkRange * range, struct ALL_DATA *all_data)
{
	struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;

	ControlInfo * ci = g_object_get_data (G_OBJECT (range), "control_info");
	InputControl * c = s->control + ci->idx;
	int val = (int) gtk_range_get_value (range);

	if (input_set_control (videoIn->fd, c, val) == 0) 
	{
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ci->spinbutton), val);
	}
	else 
	{
		if (global->debug) g_printerr ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn->fd, c, &val) == 0) 
		{
			if (global->debug) g_printerr ("hardware value is %d\n", val);
			gtk_range_set_value (GTK_RANGE(ci->widget),val);
		}
		else 
		{
			g_printerr ("hardware get failed\n");
		}
	}
	
	s = NULL;
	global = NULL;
	videoIn = NULL;
}

/*spin controls callback*/
void
spin_changed (GtkSpinButton * spin, struct ALL_DATA *all_data)
{
	struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	ControlInfo * ci = g_object_get_data (G_OBJECT (spin), "control_info");
	InputControl * c = s->control + ci->idx;
	int val = gtk_spin_button_get_value_as_int (spin);

	if (input_set_control (videoIn->fd, c, val) == 0) 
	{
		gtk_range_set_value (GTK_RANGE(ci->widget),val);
	}
	else 
	{
		if (global->debug) g_printerr ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn->fd, c, &val) == 0) 
		{
			if (global->debug) g_printerr ("hardware value is %d\n", val);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(ci->spinbutton),val);
		}
		else 
		{
			g_printerr ("hardware get failed\n");
		}
	}
	s = NULL;
	global = NULL;
	videoIn = NULL;
}

/*set video frame jpeg quality/compression*/
void
set_jpeg_comp_clicked (GtkButton * jpeg_comp, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct vdIn *videoIn = all_data->videoIn;

	videoIn->jpgcomp.quality = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(gwidget->jpeg_comp));

	videoIn->setJPEGCOMP = 1; 
	
	videoIn = NULL;
}

/*check box controls callback*/
void
autofocus_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data) 
{
	struct GLOBAL *global = all_data->global;
	struct focusData *AFdata = all_data->AFdata;
	struct vdIn *videoIn = all_data->videoIn;
	
	ControlInfo * ci = g_object_get_data (G_OBJECT (toggle), "control_info");
	int val;
	
	val = gtk_toggle_button_get_active (toggle) ? 1 : 0;

	/*if autofocus disable manual focus control*/
	gtk_widget_set_sensitive (ci->widget, !val);
	gtk_widget_set_sensitive (ci->spinbutton, !val);

	/*reset flag*/
	AFdata->flag = 0;
	AFdata->ind = 0;
	AFdata->focus = -1; /*reset focus*/
	AFdata->right = 255;
	AFdata->left = 8;
	/*set focus to first value if autofocus enabled*/
	if (val>0) 
	{
		if (set_focus (videoIn->fd, AFdata->focus) != 0) 
			g_printerr("ERROR: couldn't set focus to %d\n", AFdata->focus);
	}
	global->autofocus = val;

	global = NULL;
	AFdata = NULL;
	videoIn = NULL;
}


void
check_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;

	ControlInfo * ci = g_object_get_data (G_OBJECT (toggle), "control_info");
	InputControl * c = s->control + ci->idx;
	int val;

	val = gtk_toggle_button_get_active (toggle) ? 1 : 0;

	if (input_set_control (videoIn->fd, c, val) != 0)
	{
		g_printerr ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn->fd, c, &val) == 0) 
		{
			g_printerr ("hardware value is %d\n", val);
		}
		else 
		{
			g_printerr ("hardware get failed\n");
		}
	} 
	else 
	{
		if (global->debug) g_printf("changed %s to %d\n",c->name,val);
		if (input_get_control (videoIn->fd, c, &val) == 0) 
		{
			if (global->debug) g_printf ("hardware value is %d\n", val);
		}
		else
		{
			g_printerr ("hardware get failed\n");
		}
		
	}
	
	s = NULL;
	global = NULL;
	videoIn = NULL;
}

void
bayer_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	ControlInfo * ci = g_object_get_data (G_OBJECT (toggle), "control_info");
	InputControl * c = s->control + ci->idx;
	int val;

	val = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	if (input_set_control (videoIn->fd, c, val) != 0) 
	{
		g_printerr ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn->fd, c, &val) == 0) 
		{
			g_printerr ("hardware value is %d\n", val);
		}
		else 
		{
			g_printerr ("hardware get failed\n");
		}
	}
	else
	{
		if (global->debug) g_printf("changed %s to %d\n",c->name,val);
		/*stop and restart stream*/
		videoIn->setFPS=1;
		/*read value*/
		if (input_get_control (videoIn->fd, c, &val) == 0) 
		{
			if (val>0) videoIn->isbayer=1;
			else videoIn->isbayer=0;
		}
		else 
		{
			g_printerr ("hardware get failed\n");
		}
	}

	s = NULL;
	global = NULL;
	videoIn = NULL;
}

void
pix_ord_changed (GtkComboBox * combo, struct ALL_DATA *all_data)
{
	struct vdIn *videoIn = all_data->videoIn;

	int index = gtk_combo_box_get_active (combo);
	videoIn->pix_order=index;

	videoIn=NULL;
}

void
reversePan_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;

	int val;

	val = gtk_toggle_button_get_active (toggle) ? -1 : 1;

	if(global->PanStep >0) global->PanStep= val * global->PanStep;
	else global->PanStep= -val * global->PanStep;
}

/*combobox controls callback*/
void
combo_changed (GtkComboBox * combo, struct ALL_DATA *all_data)
{
	struct VidState *s = all_data->s;
	struct vdIn *videoIn = all_data->videoIn;

	ControlInfo * ci = g_object_get_data (G_OBJECT (combo), "control_info");
	InputControl * c = s->control + ci->idx;
	int index = gtk_combo_box_get_active (combo);
	int val=0;

	val=index;
	
	if (input_set_control (videoIn->fd, c, val) != 0) 
	{
		g_printerr ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn->fd, c, &val) == 0) 
		{
			g_printerr ("hardware value is %d\n", val);
		}
		else 
		{
			g_printerr ("hardware get failed\n");
		}
	}

	s = NULL;
	videoIn = NULL;
}

/*video format control callback*/
void
VidFormat_changed (GtkComboBox * VidFormat, struct ALL_DATA *all_data) 
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	
	const char *filename;
	global->VidFormat=gtk_combo_box_get_active (VidFormat);	
	filename=gtk_entry_get_text(GTK_ENTRY(gwidget->VidFNameEntry));
	
	if(g_strcmp0(filename,global->vidFPath[0])!=0) 
	{
		global->vidFPath=splitPath((char *)filename, global->vidFPath);
	}
	
	global->vidFPath[0] = setVidExt(global->vidFPath[0], global->VidFormat);
	
	gtk_entry_set_text(GTK_ENTRY(gwidget->VidFNameEntry)," ");
	gtk_entry_set_text(GTK_ENTRY(gwidget->VidFNameEntry),global->vidFPath[0]);
	
	if(global->vid_inc>0) 
	{
		global->vid_inc=1; /*if auto naming restart counter*/
	}
	g_snprintf(global->vidinc_str,24,_("File num:%d"),global->vid_inc);
	gtk_label_set_text(GTK_LABEL(gwidget->VidIncLabel), global->vidinc_str);
	
	gwidget = NULL;
	global = NULL;
}

/* set focus (for focus motor cameras ex: Logitech Orbit/Sphere and 9000 pro) */
void
setfocus_clicked (GtkButton * FocusButton, struct ALL_DATA *all_data)
{
	struct focusData *AFdata = all_data->AFdata;
	struct vdIn *videoIn = all_data->videoIn;

	AFdata->setFocus = 1;
	AFdata->ind = 0;
	AFdata->flag = 0;
	AFdata->right = 255;
	AFdata->left = 8;
	AFdata->focus = -1; /*reset focus*/
	if (set_focus (videoIn->fd, AFdata->focus) != 0)
		g_printerr("ERROR: couldn't set focus to %d\n", AFdata->focus);

	AFdata = NULL;
	videoIn = NULL;
}

// Pan/Tilt (for motor cameras ex: Logitech Orbit/Sphere)
void
PanTilt_clicked (GtkButton * PanTilt, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	int pan = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (PanTilt), "pan_info"));
	int tilt = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (PanTilt), "tilt_info"));
	int reset = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (PanTilt), "reset_info"));
	
	if(uvcPanTilt(videoIn->fd, pan * (global->PanStep), tilt * (global->TiltStep), reset)<0) 
	{
		g_printerr("Pan/Tilt Error: Pan = %d; Tilt = %d; reset = %d\n",
			pan * (global->PanStep),
			tilt * (global->TiltStep),
			reset);
	}

	global = NULL;
	videoIn = NULL;
}

void 
Devices_changed (GtkComboBox * Devices, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	GError *error=NULL;
	
	int index = gtk_combo_box_get_active(Devices);
	if(index == videoIn->listDevices->current_device) 
		return;
	g_free(global->videodevice);
	global->videodevice = g_strdup(videoIn->listDevices->listVidDevices[index].device);
	gchar *command = g_strjoin("",
		g_get_prgname(),
		" --device=",
		global->videodevice,
		NULL);
	
	gwidget->restartdialog = gtk_dialog_new_with_buttons (_("start new"),
		GTK_WINDOW(gwidget->mainwin),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		_("restart"),
		GTK_RESPONSE_ACCEPT,
		_("new"),
		GTK_RESPONSE_REJECT,
		_("cancel"),
		GTK_RESPONSE_CANCEL,
		NULL);
	
	GtkWidget *message = gtk_label_new (_("launch new process or restart?.\n\n"));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(gwidget->restartdialog)->vbox), message);
	gtk_widget_show_all(GTK_WIDGET(GTK_CONTAINER (GTK_DIALOG(gwidget->restartdialog)->vbox)));
	
	gint result = gtk_dialog_run (GTK_DIALOG (gwidget->restartdialog));
	switch (result) 
	{
		case GTK_RESPONSE_ACCEPT:
			/*restart app*/
			shutd(1, all_data);
			break;
		case GTK_RESPONSE_REJECT:
			/*spawn new process*/
			if(!(g_spawn_command_line_async(command, &error)))
			{
				g_printerr ("spawn failed: %s\n", error->message);
				g_error_free ( error );
			}
			break;
		default:
			/* do nothing since dialog was canceled*/
			break;
	}
	/*reset to current device*/
	gtk_combo_box_set_active(GTK_COMBO_BOX(Devices), videoIn->listDevices->current_device);

	gtk_widget_destroy (gwidget->restartdialog);
	g_free(command);
}

/*resolution control callback*/
void
resolution_changed (GtkComboBox * Resolution, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;

	VidCap *listVidCap = NULL;
	int current_format = videoIn->listFormats->current_format;
	int cmb_index = gtk_combo_box_get_active(Resolution);
	char temp_str[20];
	
	/*disable fps combobox signals*/
	g_signal_handlers_block_by_func(GTK_COMBO_BOX(gwidget->FrameRate), G_CALLBACK (FrameRate_changed), all_data);
	/* clear out the old fps list... */
	GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX (gwidget->FrameRate)));
	gtk_list_store_clear(store);


	listVidCap = &videoIn->listFormats->listVidFormats[current_format].listVidCap[cmb_index];
	global->width = listVidCap->width;
	global->height = listVidCap->height;

	/*check if frame rate is available at the new resolution*/
	int i=0;
	int deffps=0;
	
	for ( i = 0 ; i < listVidCap->numb_frates ; i++) 
	{
		g_snprintf(temp_str,18,"%i/%i fps", listVidCap->framerate_denom[i],
			listVidCap->framerate_num[i]);
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->FrameRate),temp_str);
		
		if (( global->fps_num == listVidCap->framerate_num[i]) && 
			(global->fps == listVidCap->framerate_denom[i]))
				deffps=i;//set selected
	}

	/*set default fps in combo*/
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->FrameRate),deffps);
	
	/*enable fps combobox signals*/ 
	g_signal_handlers_unblock_by_func(GTK_COMBO_BOX(gwidget->FrameRate), G_CALLBACK (FrameRate_changed), all_data);
	
	if (listVidCap->framerate_num)	
		global->fps_num = listVidCap->framerate_num[deffps];

	if (listVidCap->framerate_denom)
		global->fps = listVidCap->framerate_denom[deffps];

	if(videoIn->capVid)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
		gdk_flush();
	}

	global->change_res = TRUE;

	gwidget = NULL;
	global = NULL;
	videoIn = NULL;
}

/* Input Format control */
void
InpType_changed(GtkComboBox * InpType, struct ALL_DATA *all_data) 
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	int format = 0;
	char temp_str[20];
	int index = gtk_combo_box_get_active(InpType);
	int i=0;
	//int j=0;
	int defres = 0;
	VidFormats *listVidFormats;
	
	/*disable resolution combobox signals*/
	g_signal_handlers_block_by_func(GTK_COMBO_BOX(gwidget->Resolution), G_CALLBACK (resolution_changed), all_data);
	
	/* clear out the old resolution list... */
	GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX (gwidget->Resolution)));
	gtk_list_store_clear(store);
	
	videoIn->listFormats->current_format = index;
	listVidFormats = &videoIn->listFormats->listVidFormats[index];
	
	format = videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].format;
	get_PixMode(format, global->mode);
	
	/*redraw resolution combo for new format*/
	for(i = 0 ; i < listVidFormats->numb_res ; i++)  
	{
		if (listVidFormats->listVidCap[i].width>0)
		{
			g_snprintf(temp_str,18,"%ix%i", listVidFormats->listVidCap[i].width,
							 listVidFormats->listVidCap[i].height);
			gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->Resolution),temp_str);
			
			if ((global->width == listVidFormats->listVidCap[i].width) && 
				(global->height == listVidFormats->listVidCap[i].height))
					defres=i;//set selected resolution index
		}
	}
	
	
	global->height = listVidFormats->listVidCap[defres].height;
	global->width = listVidFormats->listVidCap[defres].width;
	global->format = format;
	
	/*enable resolution combobox signals*/
	g_signal_handlers_unblock_by_func(GTK_COMBO_BOX(gwidget->Resolution), G_CALLBACK (resolution_changed), all_data);
	
	/*reset resolution/format*/
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Resolution),defres);
	
	gwidget = NULL;
	global = NULL;
	videoIn = NULL;
}

/*frame rate control callback*/
void
FrameRate_changed (GtkComboBox * FrameRate, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	int resind = gtk_combo_box_get_active(GTK_COMBO_BOX(gwidget->Resolution));
	
	int index = gtk_combo_box_get_active (FrameRate);
		
	videoIn->fps=videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[resind].framerate_denom[index];
	videoIn->fps_num=videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[resind].framerate_num[index];
 
	videoIn->setFPS=1;

	global->fps=videoIn->fps;
	global->fps_num=videoIn->fps_num;
	
	gwidget = NULL;
	global = NULL;
	videoIn = NULL;
}

/*sound sample rate control callback*/
void
SndSampleRate_changed (GtkComboBox * SampleRate, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	
	global->Sound_SampRateInd = gtk_combo_box_get_active (SampleRate);
	global->Sound_SampRate=stdSampleRates[global->Sound_SampRateInd];
	
	global = NULL;
}

/*image type control callback*/
void
ImageType_changed (GtkComboBox * ImageType, struct ALL_DATA *all_data) 
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	
	const char *filename;
	global->imgFormat=gtk_combo_box_get_active (ImageType);	
	filename=gtk_entry_get_text(GTK_ENTRY(gwidget->ImageFNameEntry));
	
	if(g_strcmp0(filename,global->imgFPath[0])!=0) 
	{
		global->imgFPath=splitPath((char *)filename, global->imgFPath);
	}
	
	global->imgFPath[0] = setImgExt(global->imgFPath[0], global->imgFormat);
	
	gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry)," ");
	gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry),global->imgFPath[0]);
	
	if(global->image_inc>0) 
	{
		global->image_inc=1; /*if auto naming restart counter*/
	}
	g_snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
	gtk_label_set_text(GTK_LABEL(gwidget->ImageIncLabel), global->imageinc_str);
	
	gwidget = NULL;
	global = NULL;
}

/*Audio API control callback*/
void
SndAPI_changed (GtkComboBox * SoundAPI, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct paRecordData *pdata = all_data->pdata;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	global->Sound_API=gtk_combo_box_get_active (SoundAPI);
	pdata->api = global->Sound_API;
	if(!global->Sound_API) gtk_widget_set_sensitive (gwidget->SndDevice, TRUE);//enable sound device combobox
	else
	{
		global->Sound_UseDev = global->Sound_DefDev;; //force default device
		gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndDevice),global->Sound_UseDev);
		gtk_widget_set_sensitive (gwidget->SndDevice, FALSE); //disable sound device combobox
	}
	g_printf("using audio API n:%d\n",global->Sound_API);
	global = NULL;
}

/*sound device control callback*/
void
SndDevice_changed (GtkComboBox * SoundDevice, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	
	global->Sound_UseDev=gtk_combo_box_get_active (SoundDevice);
	g_printf("using device id:%d\n",global->Sound_IndexDev[global->Sound_UseDev].id);
	global = NULL;
}

/*sound channels control callback*/
void
SndNumChan_changed (GtkComboBox * SoundChan, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	
	/*0-device default 1-mono 2-stereo*/
	global->Sound_NumChanInd = gtk_combo_box_get_active (SoundChan);
	global->Sound_NumChan=global->Sound_NumChanInd;
	
	global = NULL;
}

/*sound Format callback*/
void
SndComp_changed (GtkComboBox * SoundComp, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	
	/* 0-PCM (default) 1-MP2 */
	switch (gtk_combo_box_get_active (SoundComp)) 
	{
		case 0:
			global->Sound_Format  = PA_FOURCC;
			break;
		case 1:
			global->Sound_Format = ISO_FORMAT_MPEG12;
			break;
		default:
			global->Sound_Format  = PA_FOURCC;
	}
	
	global = NULL;
}

/*video compression control callback*/
void
VidCodec_changed (GtkComboBox * VidCodec, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	int index = gtk_combo_box_get_active (VidCodec);
	global->VidCodec=index;
	gtk_widget_set_sensitive (gwidget->lavc_button, isLavcCodec(global->VidCodec));
	
	global = NULL;
}

/* sound enable check box callback */
void
SndEnable_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	
	global->Sound_enable = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	if (!global->Sound_enable) 
	{
		if(global->debug) g_printf("disabling sound.\n");
		set_sensitive_snd_contrls(FALSE, gwidget);
	}
	else 
	{ 
		if(global->debug) g_printf("enabling sound.\n");
		set_sensitive_snd_contrls(TRUE, gwidget);
	}
	
	gwidget = NULL;
	global = NULL;
}

/* Video Filters check box callback */
void
FiltEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	int filter = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (toggle), "filt_info"));
	g_mutex_lock(global->mutex);
		global->Frame_Flags = gtk_toggle_button_get_active (toggle) ? 
			(global->Frame_Flags | filter) : 
			(global->Frame_Flags & ~(filter));
	g_mutex_unlock(global->mutex);
	
	global = NULL;
}

/* Audio effect checkbox callback*/
void
EffEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct paRecordData *pdata = all_data->pdata;
	int effect = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (toggle), "effect_info"));
	g_mutex_lock(pdata->mutex);
		pdata->snd_Flags = gtk_toggle_button_get_active (toggle) ? 
			(pdata->snd_Flags | effect) : 
			(pdata->snd_Flags & ~(effect));
	g_mutex_unlock(pdata->mutex);
	pdata = NULL;
}

void 
ImageInc_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	
	global->image_inc = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	
	g_snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
	
	gtk_label_set_text(GTK_LABEL(gwidget->ImageIncLabel), global->imageinc_str);
	
	gwidget = NULL;
	global = NULL;
}

void
VidInc_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;

	
	global->vid_inc = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	
	g_snprintf(global->vidinc_str,24,_("File num:%d"),global->vid_inc);
	
	gtk_label_set_text(GTK_LABEL(gwidget->VidIncLabel), global->vidinc_str);
	
	gwidget = NULL;
	global = NULL;

}

/*----------------------------- Capture Image --------------------------------*/
/*image capture button callback*/
void
capture_image (GtkButton *ImageButt, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	const char *fileEntr=gtk_entry_get_text(GTK_ENTRY(gwidget->ImageFNameEntry));
	if(g_strcmp0(fileEntr,global->imgFPath[0])!=0) 
	{
		/*reset if entry change from last capture*/
		if(global->image_inc) global->image_inc=1;
		global->imgFPath=splitPath((char *)fileEntr, global->imgFPath);
		gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry),"");
		gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry),global->imgFPath[0]);
		/*get the file type*/
		global->imgFormat = check_image_type(global->imgFPath[0]);
		/*set the file type*/
		gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->ImageType),global->imgFormat);
	}

	g_snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
	gtk_label_set_text(GTK_LABEL(gwidget->ImageIncLabel), global->imageinc_str);
	
	if ((global->image_timer == 0) && (global->image_inc>0)) 
	{
		videoIn->ImageFName = incFilename(videoIn->ImageFName, 
			global->imgFPath,
			global->image_inc);
		
		global->image_inc++;
	} 
	else 
	{
		//printf("fsize=%d bytes fname=%d bytes\n",fsize,sfname);
		videoIn->ImageFName = joinPath(videoIn->ImageFName, global->imgFPath);
	}
	
	if(global->image_timer > 0) 
	{ 
		/*auto capture on -> stop it*/
		if (global->image_timer_id > 0) g_source_remove(global->image_timer_id);
		gtk_button_set_label(GTK_BUTTON(gwidget->CapImageButt),_("Cap. Image"));
		global->image_timer=0;
		set_sensitive_img_contrls(TRUE, gwidget);/*enable image controls*/
	} 
	else 
	{
		if(global->imgFormat == 3) 
		{ /*raw frame*/
			videoIn->cap_raw = 1;
		} 
		else 
		{
			videoIn->capImage = TRUE;
		}
	}
	
	gwidget = NULL;
	global = NULL;
	videoIn = NULL;
}

/*--------------------------- Capture Video ------------------------------------*/
/*video capture button callback*/
void
capture_vid (GtkToggleButton *VidButt, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	const char *fileEntr = gtk_entry_get_text(GTK_ENTRY(gwidget->VidFNameEntry));
	if(g_strcmp0(fileEntr,global->vidFPath[0])!=0) 
	{
		/*reset if entry change from last capture*/
		if(global->vid_inc) global->vid_inc=1;
		global->vidFPath=splitPath((char *)fileEntr, global->vidFPath);
		gtk_entry_set_text(GTK_ENTRY(gwidget->VidFNameEntry),"");
		gtk_entry_set_text(GTK_ENTRY(gwidget->VidFNameEntry),global->vidFPath[0]);
	}
	
	gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt));
	if(global->debug) g_printf("Cap Video toggled: %d\n", state);
	
	if(videoIn->capVid || !state) 
	{	/****************** Stop Video ************************/
		//enable resolution and input format combos
		gtk_widget_set_sensitive (gwidget->Resolution, TRUE);
		gtk_widget_set_sensitive (gwidget->InpType, TRUE);
		
		closeVideoFile(all_data);
		if(!(state))
		{
			gtk_button_set_label(GTK_BUTTON(gwidget->CapVidButt),_("Cap. Video"));
			//gtk_widget_show (gwidget->VidButton_Img);
		}
		if(global->disk_timer_id) g_source_remove(global->disk_timer_id);
		global->disk_timer_id = 0;
	} 
	else if(!(videoIn->capVid) /*&& state*/)
	{	/******************** Start Video *********************/
		//disable resolution and input format combos
		gtk_widget_set_sensitive (gwidget->Resolution, FALSE);
		gtk_widget_set_sensitive (gwidget->InpType, FALSE);
		
		global->vidFPath=splitPath((char *)fileEntr, global->vidFPath);
		g_snprintf(global->vidinc_str,24,_("File num:%d"),global->vid_inc);
		gtk_label_set_text(GTK_LABEL(gwidget->VidIncLabel), global->vidinc_str);
	
		/*start disk check timed callback (every 10 sec)*/
		if (!global->disk_timer_id)
			global->disk_timer_id=g_timeout_add(10*1000, FreeDiskCheck_timer, all_data);
				
		if (global->vid_inc>0) 
		{
			videoIn->VidFName = incFilename(videoIn->VidFName,
				global->vidFPath,
				global->vid_inc);
					
			global->vid_inc++;
		} 
		else 
		{
			videoIn->VidFName = joinPath(videoIn->VidFName, global->vidFPath);
		}
		
		if(initVideoFile(all_data)<0)
		{
			g_printf("Cap Video failed\n");
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
			state = FALSE;
		}
		if(state)
		{
			gtk_button_set_label(GTK_BUTTON(gwidget->CapVidButt),_("Stop Video"));
			//gtk_widget_show (gwidget->VidButton_Img);
		}
	}
	
	gwidget = NULL;
	pdata = NULL;
	global = NULL;
	videoIn = NULL;
}

/*--------------------- buttons callbacks ------------------*/

void
ProfileButton_clicked (GtkButton * ProfileButton, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	gboolean *save = g_object_get_data (G_OBJECT (ProfileButton), "profile_save");
	if(global->debug) g_printf("Profile dialog (%d)\n",*save);
	if (*save)
	{
		gwidget->FileDialog = gtk_file_chooser_dialog_new (_("Save File"),
			GTK_WINDOW(gwidget->mainwin),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
		gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (gwidget->FileDialog), TRUE);
	
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (gwidget->FileDialog),
			global->profile_FPath[0]);
	}
	else
	{
		gwidget->FileDialog = gtk_file_chooser_dialog_new (_("Load File"),
			GTK_WINDOW(gwidget->mainwin),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
	}
	
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (gwidget->FileDialog),
		global->profile_FPath[1]);
		
	if (gtk_dialog_run (GTK_DIALOG (gwidget->FileDialog)) == GTK_RESPONSE_ACCEPT)
	{
		/*Save Controls Data*/
		char *filename= gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (gwidget->FileDialog));
		global->profile_FPath=splitPath(filename,global->profile_FPath);
		
		if(*save)
			SaveControls(s, global, videoIn);
		else
			LoadControls(s,global);
	}
	gtk_widget_destroy (gwidget->FileDialog);
	gwidget = NULL;
	s = NULL;
	global = NULL;
	videoIn = NULL;
}

/*called when avi max file size reached*/
/* stops avi capture, increments avi file name, restart avi capture*/
void *
split_avi(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	gdk_threads_enter();
	/*make sure avi is in incremental mode*/
	if(!global->vid_inc) 
	{ 
		VidInc_changed(GTK_TOGGLE_BUTTON(gwidget->VidInc), all_data);
		global->vid_inc=1; /*just in case*/
	}
	
	/*stops avi capture*/
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
	//gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON(gwidget->CapAVIButt));
	gdk_flush ();
	gdk_threads_leave();
	int stall = wait_ms(&(videoIn->VidCapStop), TRUE, 10, 200);
	if( !(stall > 0) )
	{
		g_printerr("video capture stall on exit(%d) - timeout\n",
			videoIn->VidCapStop);
	}
	/*starts avi capture*/
	gdk_threads_enter();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), TRUE);
	
	global->VidButtPress = FALSE;
	gdk_flush ();
	gdk_threads_leave();
	/*thread as finished*/
	global=NULL;
	gwidget = NULL;
	return NULL;
}

void 
ShowFPS_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	
	global->FpsCount = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	
	if(global->FpsCount > 0) 
	{
		/*sets the Fps counter timer function every 2 sec*/
		global->timer_id = g_timeout_add(2*1000,FpsCount_callback, all_data);
	} 
	else 
	{
		if (global->timer_id > 0) g_source_remove(global->timer_id);
		g_snprintf(global->WVcaption,10,"GUVCVideo");
		SDL_WM_SetCaption(global->WVcaption, NULL);
	}
	global = NULL;
}

void
quitButton_clicked (GtkButton * quitButton, struct ALL_DATA *all_data)
{
	shutd(0, all_data);//shutDown
}



