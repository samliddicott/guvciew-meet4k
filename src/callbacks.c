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

#include "callbacks.h"

#include <glib.h>
#include <glib/gprintf.h>
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
	struct avi_t *AviOut = all_data->AviOut;

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

	if (global) closeGlobals (global);
	global = NULL;
	all_data->global = NULL;

	g_free(pdata);
	pdata = NULL;
	all_data->pdata = NULL;    

	if(videoIn->fd > 0) close_v4l2(videoIn);
	else g_free(videoIn);
	videoIn=NULL;
	all_data->videoIn = NULL;

	g_free(AviOut);
	AviOut = NULL;
	all_data->AviOut = NULL;

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

/*--------------------------- controls enable/disable --------------------------*/

/* sound controls*/
void 
set_sensitive_snd_contrls (const int flag, struct GWIDGET *gwidget)
{
	gtk_widget_set_sensitive (gwidget->SndSampleRate, flag);
	gtk_widget_set_sensitive (gwidget->SndDevice, flag);
	gtk_widget_set_sensitive (gwidget->SndNumChan, flag);
	gtk_widget_set_sensitive (gwidget->SndComp, flag);
}

/*avi controls*/
void 
set_sensitive_avi_contrls (const int flag, const int sndEnable, struct GWIDGET *gwidget)
{
	/*sound and avi compression controls*/
	gtk_widget_set_sensitive (gwidget->AVIComp, flag);
	gtk_widget_set_sensitive (gwidget->SndEnable, flag); 
	gtk_widget_set_sensitive (gwidget->AVIInc, flag);
	if(sndEnable > 0) 
	{
		set_sensitive_snd_contrls(flag, gwidget);
	}
	gwidget->avi_widget_state = flag;
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

/*-------------------------------- avi close functions -----------------------*/

/* Called at avi capture stop       */
/* from avi capture button callback */
void
aviClose (struct ALL_DATA *all_data)
{
	DWORD tottime = 0;
	//int tstatus;

	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;

	struct vdIn *videoIn = all_data->videoIn;
	struct avi_t *AviOut = all_data->AviOut;
	
	

	if (!(AviOut->closed))
	{
		tottime = global->AVIstoptime - global->AVIstarttime;
		if (global->debug) g_printf("stop= %d start=%d \n",global->AVIstoptime,global->AVIstarttime);
		if (tottime > 0) 
		{
			/*try to find the real frame rate*/
			AviOut->fps = (double) (global->framecount * 1000) / tottime;
		}
		else 
		{
			/*set the hardware frame rate*/   
			AviOut->fps=videoIn->fps;
		}

		if (global->debug) g_printf("AVI: %d frames in %d ms = %f fps\n",global->framecount,tottime,AviOut->fps);
		/*------------------- close audio stream and clean up -------------------*/
		if (global->Sound_enable > 0) 
		{
			/*wait for audio to finish*/
			int stall = wait_ms( &pdata->streaming, FALSE, 10, 30 );
			if(!(stall)) 
			{
				g_printf("WARNING:sound capture stall (still streaming(%d) \n",
					pdata->streaming);
				pdata->streaming = 0;
			}
			/*write any available audio data*/  
			if(pdata->audio_flag)
			{
				g_printerr("writing %d bytes of audio data\n",pdata->snd_numBytes);
				g_mutex_lock( pdata->mutex);
					if(global->Sound_Format == PA_FOURCC)
					{
						if(pdata->avi_sndBuff) 
						{
							Float2Int16(pdata);
							AVI_write_audio(AviOut,(BYTE *) pdata->avi_sndBuff1,pdata->snd_numSamples*2);
						}
					}
					else if (global->Sound_Format == ISO_FORMAT_MPEG12)
					{
						int size_mp2=0;
						if(pdata->avi_sndBuff && pdata->mp2Buff) 
						{
							size_mp2 = MP2_encode(pdata,0);
							AVI_write_audio(AviOut,pdata->mp2Buff,size_mp2);
							pdata->flush = 1; /*flush mp2 encoder*/
							size_mp2 = MP2_encode(pdata,0);
							AVI_write_audio(AviOut,pdata->mp2Buff,size_mp2);
							pdata->flush = 0;
						}
					}
				g_mutex_unlock( pdata->mutex );
			}
			pdata->audio_flag = 0; /*all audio should have been writen by now*/
			
			if (close_sound (pdata)) g_printerr("Sound Close error\n");
			if(global->Sound_Format == ISO_FORMAT_MPEG12) close_MP2_encoder();
		} 
		AVI_close (AviOut);
		global->framecount = 0;
		global->AVIstarttime = 0;
		if (global->debug) g_printf ("close avi\n");
	}

	pdata = NULL;
	global = NULL;
	videoIn = NULL;
	AviOut = NULL;
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

	if (input_set_control (videoIn, c, val) == 0) 
	{
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ci->spinbutton), val);
	}
	else 
	{
		if (global->debug) g_printerr ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn, c, &val) == 0) 
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

	if (input_set_control (videoIn, c, val) == 0) 
	{
		gtk_range_set_value (GTK_RANGE(ci->widget),val);
	}
	else 
	{
		if (global->debug) g_printerr ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn, c, &val) == 0) 
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
		if (set_focus (videoIn, AFdata->focus) != 0) 
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

	if (input_set_control (videoIn, c, val) != 0)
	{
		g_printerr ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn, c, &val) == 0) 
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
		if (input_get_control (videoIn, c, &val) == 0) 
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
	if (input_set_control (videoIn, c, val) != 0) 
	{
		g_printerr ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn, c, &val) == 0) 
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
		if (input_get_control (videoIn, c, &val) == 0) 
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

	if (c->id == V4L2_CID_EXPOSURE_AUTO_OLD) 
	{
		val=exp_vals[videoIn->available_exp[index]];
	} 
	else 
	{
		val=index;
	}

	if (input_set_control (videoIn, c, val) != 0) 
	{
		g_printerr ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn, c, &val) == 0) 
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
	if (set_focus (videoIn, AFdata->focus) != 0) 
		g_printerr("ERROR: couldn't set focus to %d\n", AFdata->focus);

	AFdata = NULL;
	videoIn = NULL;
}

/* Pan left (for motor cameras ex: Logitech Orbit/Sphere) */
void
PanLeft_clicked (GtkButton * PanLeft, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;

	if(uvcPanTilt(videoIn, -INCPANTILT*(global->PanStep), 0, 0)<0) 
	{
		g_printerr("Pan Left Error");
	}

	global = NULL;
	videoIn = NULL;
}

/* Pan Right (for motor cameras ex: Logitech Orbit/Sphere) */
void
PanRight_clicked (GtkButton * PanRight, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;

	if(uvcPanTilt(videoIn, INCPANTILT*(global->PanStep), 0, 0)<0) 
	{
		g_printerr("Pan Right Error");
	}

	global = NULL;
	videoIn = NULL;
}

/* Tilt Up (for motor cameras ex: Logitech Orbit/Sphere)   */
void
TiltUp_clicked (GtkButton * TiltUp, struct ALL_DATA *all_data)
{	
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	if(uvcPanTilt(videoIn, 0, -INCPANTILT*(global->TiltStep), 0)<0) 
	{
		g_printerr("Tilt UP Error");
	}

	global = NULL;
	videoIn = NULL;
}

/* Tilt Down (for motor cameras ex: Logitech Orbit/Sphere) */
void
TiltDown_clicked (GtkButton * TiltDown, struct ALL_DATA *all_data)
{	
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	if(uvcPanTilt(videoIn, 0, INCPANTILT*(global->TiltStep), 0)<0) 
	{
		g_printerr("Tilt Down Error");
	}

	global = NULL;
	videoIn = NULL;
}

/* Pan Reset (for motor cameras ex: Logitech Orbit/Sphere)*/
void
PReset_clicked (GtkButton * PReset, struct ALL_DATA *all_data)
{	
	struct vdIn *videoIn = all_data->videoIn;
	
	if(uvcPanTilt(videoIn, 0, 0, 1)<0) 
	{
		g_printerr("Pan Reset Error");
	}
	
	videoIn = NULL;
}

/* Tilt Reset (for motor cameras ex: Logitech Orbit/Sphere)*/
void
TReset_clicked (GtkButton * PTReset, struct ALL_DATA *all_data)
{	
	struct vdIn *videoIn = all_data->videoIn;
	
	if(uvcPanTilt(videoIn, 0, 0, 2)<0) 
	{
		g_printerr("Pan Reset Error");
	}

	videoIn = NULL;
}

/* Pan Tilt Reset (for motor cameras ex: Logitech Orbit/Sphere)*/
void
PTReset_clicked (GtkButton * PTReset, struct ALL_DATA *all_data)
{
	struct vdIn *videoIn = all_data->videoIn;
	
	if(uvcPanTilt(videoIn, 0, 0, 3)<0) 
	{
		g_printerr("Pan Tilt Reset Error");
	}

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
	if(index == videoIn->current_device) 
		return;
	g_free(global->videodevice);
	global->videodevice = g_strdup(videoIn->listVidDevices[index].device);
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
	gtk_combo_box_set_active(GTK_COMBO_BOX(Devices), videoIn->current_device);

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

	/* The new resolution is writen to conf file at exit             */
	/* then is read back at start. This means that for changing */
	/* resolution we must restart the application                    */

	int index = gtk_combo_box_get_active(Resolution);
	global->width=videoIn->listVidFormats[global->formind].listVidCap[index].width;
	global->height=videoIn->listVidFormats[global->formind].listVidCap[index].height;

	/*check if frame rate is available at the new resolution*/
	int i=0;
	int deffps=0;
	for(i=0;i<videoIn->listVidFormats[global->formind].listVidCap[index].numb_frates;i++) 
	{
		if ((videoIn->listVidFormats[global->formind].listVidCap[index].framerate_num[i]==global->fps_num) && 
			(videoIn->listVidFormats[global->formind].listVidCap[index].framerate_denom[i]==global->fps)) 
				deffps=i;
	}
	
	global->fps_num=videoIn->listVidFormats[global->formind].listVidCap[index].framerate_num[deffps];
	global->fps=videoIn->listVidFormats[global->formind].listVidCap[index].framerate_denom[deffps];
	
	gwidget->restartdialog = gtk_dialog_new_with_buttons (_("Program Restart"),
		GTK_WINDOW(gwidget->mainwin),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		_("now"),
		GTK_RESPONSE_ACCEPT,
		_("Later"),
		GTK_RESPONSE_REJECT,
		NULL);
	
	GtkWidget *message = gtk_label_new (_("Changes will only take effect after guvcview restart.\n\n\nRestart now?\n"));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(gwidget->restartdialog)->vbox), message);
	gtk_widget_show_all(GTK_WIDGET(GTK_CONTAINER (GTK_DIALOG(gwidget->restartdialog)->vbox)));
	
	gint result = gtk_dialog_run (GTK_DIALOG (gwidget->restartdialog));
	switch (result) 
	{
		case GTK_RESPONSE_ACCEPT:
			/*restart app*/
			shutd(1, all_data);
			break;

		default:
			/* do nothing since Restart rejected*/
			break;
	}

	gtk_widget_destroy (gwidget->restartdialog);

	gwidget = NULL;
	global = NULL;
	videoIn = NULL;
}

/* Input Format control */
void
ImpType_changed(GtkComboBox * ImpType, struct ALL_DATA *all_data) 
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	int index = gtk_combo_box_get_active(ImpType);
	
	
	global->formind = index;
	
	/*check if frame rate and resolution are available   */
	/*if not use minimum values - defres=0 and deffps=0  */
	int i=0;
	int j=0;
	int defres=0;
	int deffps=0;
	
	
	for (i=0;i<videoIn->listVidFormats[global->formind].numb_res;i++) 
	{
		if((videoIn->listVidFormats[global->formind].listVidCap[i].height==global->height) &&
			(videoIn->listVidFormats[global->formind].listVidCap[i].width==global->width) ) 
		{
			/* resolution ok check fps*/
			defres=i;
			for (j=0;j<videoIn->listVidFormats[global->formind].listVidCap[i].numb_frates;j++) 
			{
				if ((videoIn->listVidFormats[global->formind].listVidCap[i].framerate_num[j]==global->fps_num) && 
					(videoIn->listVidFormats[global->formind].listVidCap[i].framerate_denom[j]==global->fps))
						deffps=j;
			}
		}
	}
	global->format = videoIn->listVidFormats[global->formind].format;
	get_PixMode(global->format, global->mode);
	global->height=videoIn->listVidFormats[global->formind].listVidCap[defres].height;
	global->width=videoIn->listVidFormats[global->formind].listVidCap[defres].width;
	global->fps_num=videoIn->listVidFormats[global->formind].listVidCap[defres].framerate_num[deffps];
	global->fps=videoIn->listVidFormats[global->formind].listVidCap[defres].framerate_denom[deffps];
	
	gwidget->restartdialog = gtk_dialog_new_with_buttons (_("Program Restart"),
		GTK_WINDOW(gwidget->mainwin),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		_("now"),
		GTK_RESPONSE_ACCEPT,
		_("Later"),
		GTK_RESPONSE_REJECT,
		NULL);
	
	GtkWidget *message = gtk_label_new (_("Changes will only take effect after guvcview restart.\n\n\nRestart now?\n"));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(gwidget->restartdialog)->vbox), message);
	gtk_widget_show_all(GTK_WIDGET(GTK_CONTAINER (GTK_DIALOG(gwidget->restartdialog)->vbox)));
	
	gint result = gtk_dialog_run (GTK_DIALOG (gwidget->restartdialog));
	switch (result) 
	{
		case GTK_RESPONSE_ACCEPT:
			/*restart app*/
			shutd(1, all_data);
			break;
			
		default:
			/* do nothing since Restart rejected*/		
			break;
	}
  
	gtk_widget_destroy (gwidget->restartdialog);
	
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
		
	videoIn->fps=videoIn->listVidFormats[global->formind].listVidCap[resind].framerate_denom[index];
	videoIn->fps_num=videoIn->listVidFormats[global->formind].listVidCap[resind].framerate_num[index];
 
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

/*avi compression control callback*/
void
AVIComp_changed (GtkComboBox * AVIComp, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	
	int index = gtk_combo_box_get_active (AVIComp);
	global->AVIFormat=index;
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
		set_sensitive_snd_contrls(FALSE, gwidget);
	}
	else 
	{ 
		set_sensitive_snd_contrls(TRUE, gwidget);
	}
	
	gwidget = NULL;
	global = NULL;
}

/* Mirror check box callback */
void
FiltMirrorEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	g_mutex_lock(global->mutex);
		global->Frame_Flags = gtk_toggle_button_get_active (toggle) ? 
			(global->Frame_Flags | YUV_MIRROR) : 
			(global->Frame_Flags & ~YUV_MIRROR);
	g_mutex_unlock(global->mutex);
	
	global = NULL;
}

/* Upturn check box callback */
void
FiltUpturnEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	g_mutex_lock(global->mutex);
		global->Frame_Flags = gtk_toggle_button_get_active (toggle) ? 
			(global->Frame_Flags | YUV_UPTURN) : 
			(global->Frame_Flags & ~YUV_UPTURN);
	g_mutex_unlock(global->mutex);
	global = NULL;
}

/* Pieces check box callback */
void
FiltPiecesEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	g_mutex_lock(global->mutex);
		global->Frame_Flags = gtk_toggle_button_get_active (toggle) ? 
			(global->Frame_Flags | YUV_PIECES) : 
			(global->Frame_Flags & ~YUV_PIECES);
	g_mutex_unlock(global->mutex);
	global = NULL;
}

/* Negate check box callback */
void
FiltNegateEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	g_mutex_lock(global->mutex);
		global->Frame_Flags = gtk_toggle_button_get_active (toggle) ? 
			(global->Frame_Flags | YUV_NEGATE) : 
			(global->Frame_Flags & ~YUV_NEGATE);
	g_mutex_unlock(global->mutex);
	global = NULL;
}

/* Upturn check box callback */
void
FiltMonoEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	g_mutex_lock(global->mutex);
		global->Frame_Flags = gtk_toggle_button_get_active (toggle) ? 
			(global->Frame_Flags | YUV_MONOCR) : 
			(global->Frame_Flags & ~YUV_MONOCR);
	g_mutex_unlock(global->mutex);
	global = NULL;
}

/* Audio Echo effect */
void
EffEchoEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct paRecordData *pdata = all_data->pdata;
	g_mutex_lock(pdata->mutex);
		pdata->snd_Flags = gtk_toggle_button_get_active (toggle) ? 
			(pdata->snd_Flags | SND_ECHO) : 
			(pdata->snd_Flags & ~SND_ECHO);
	g_mutex_unlock(pdata->mutex);
	pdata = NULL;
}
/* Audio Fuzz effect */
void
EffFuzzEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct paRecordData *pdata = all_data->pdata;
	g_mutex_lock(pdata->mutex);
		pdata->snd_Flags = gtk_toggle_button_get_active (toggle) ? 
			(pdata->snd_Flags | SND_FUZZ) : 
			(pdata->snd_Flags & ~SND_FUZZ);
	g_mutex_unlock(pdata->mutex);
	pdata = NULL;
}

/* Audio Reverb effect */
void
EffRevEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct paRecordData *pdata = all_data->pdata;
	g_mutex_lock(pdata->mutex);
		pdata->snd_Flags = gtk_toggle_button_get_active (toggle) ? 
			(pdata->snd_Flags | SND_REVERB) : 
			(pdata->snd_Flags & ~SND_REVERB);
	g_mutex_unlock(pdata->mutex);
	pdata = NULL;
}

/*Audio WahWah effect*/
void
EffWahEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct paRecordData *pdata = all_data->pdata;
	g_mutex_lock(pdata->mutex);
		pdata->snd_Flags = gtk_toggle_button_get_active (toggle) ? 
			(pdata->snd_Flags | SND_WAHWAH) : 
			(pdata->snd_Flags & ~SND_WAHWAH);
	g_mutex_unlock(pdata->mutex);
	pdata = NULL;
}

/*Audio Ducky effect*/
void
EffDuckyEnable_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct paRecordData *pdata = all_data->pdata;
	g_mutex_lock(pdata->mutex);
		pdata->snd_Flags = gtk_toggle_button_get_active (toggle) ? 
			(pdata->snd_Flags | SND_DUCKY) : 
			(pdata->snd_Flags & ~SND_DUCKY);
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
AVIInc_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;

	
	global->avi_inc = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	
	g_snprintf(global->aviinc_str,24,_("File num:%d"),global->avi_inc);
	
	gtk_label_set_text(GTK_LABEL(gwidget->AVIIncLabel), global->aviinc_str);
	
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

/*--------------------------- Capture AVI ------------------------------------*/
/*avi capture button callback*/
void
capture_avi (GtkToggleButton *AVIButt, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct avi_t *AviOut = all_data->AviOut;
	
	const char *fileEntr = gtk_entry_get_text(GTK_ENTRY(gwidget->AVIFNameEntry));
	if(g_strcmp0(fileEntr,global->aviFPath[0])!=0) 
	{
		/*reset if entry change from last capture*/
		if(global->avi_inc) global->avi_inc=1;
		global->aviFPath=splitPath((char *)fileEntr, global->aviFPath);
		gtk_entry_set_text(GTK_ENTRY(gwidget->AVIFNameEntry),"");
		gtk_entry_set_text(GTK_ENTRY(gwidget->AVIFNameEntry),global->aviFPath[0]);
	}
	
	
	char *compression="MJPG";

	switch (global->AVIFormat) 
	{
		case 0:
			compression="MJPG";
			break;
		case 1:
			if(videoIn->formatIn == V4L2_PIX_FMT_UYVY) compression="UYVY";
			else compression="YUY2"; /*for YUYV and YU12(YUV420) */
			break;
		case 2:
			compression="DIB ";
			break;
		default:
			compression="MJPG";
	}
	
	gboolean state = gtk_toggle_button_get_active (AVIButt);
	//if(global->debug) g_printf("Cap AVI toggled: %d\n", state);
	if(videoIn->capAVI && !(state)) 
	{	/****************** Stop AVI ************************/
		videoIn->capAVI = FALSE; /*flag video thread to stop recording frames*/
		pdata->capAVI = videoIn->capAVI;
		/*wait for flag from video thread that recording has stopped    */
		/*wait on videoIn->AVICapStop by sleeping for 200 loops of 10 ms*/
		/*(test AVICapStop == TRUE on every loop)*/
		int stall = wait_ms(&(videoIn->AVICapStop), TRUE, 10, 200);
		if( !(stall > 0) )
		{
			g_printerr("video capture stall on exit(%d) - timeout\n",
				videoIn->AVICapStop);
		}
		global->AVIstoptime = ms_time();
		aviClose(all_data);
		/*enabling sound and avi compression controls*/
		set_sensitive_avi_contrls(TRUE, global->Sound_enable, gwidget);
		gtk_button_set_label(GTK_BUTTON(gwidget->CapAVIButt),_("Cap. AVI"));
		gtk_widget_queue_draw (gwidget->CapAVIButt);
		while (gtk_events_pending ()) gtk_main_iteration (); 
		
	} 
	else if(!(videoIn->capAVI) && state)
	{	/******************** Start AVI *********************/
		global->aviFPath=splitPath((char *)fileEntr, global->aviFPath);
		g_snprintf(global->aviinc_str,24,_("File num:%d"),global->avi_inc);
		gtk_label_set_text(GTK_LABEL(gwidget->AVIIncLabel), global->aviinc_str);
	
		if (global->avi_inc>0) 
		{
			videoIn->AVIFName = incFilename(videoIn->AVIFName,
				global->aviFPath,
				global->avi_inc);
					
			global->avi_inc++;
		} 
		else 
		{
			videoIn->AVIFName = joinPath(videoIn->AVIFName, global->aviFPath);
		}

		if(AVI_open_output_file(AviOut, videoIn->AVIFName)<0) 
		{
			g_printerr("Error: Couldn't create Avi.\n");
			videoIn->capAVI = FALSE;
			pdata->capAVI = videoIn->capAVI;
		} 
		else 
		{
			/*disabling sound and avi compression controls*/
			set_sensitive_avi_contrls(FALSE, global->Sound_enable, gwidget);
			
			//printf("opening avi file: %s\n",videoIn->AVIFName);
	
			/*refresh icon*/
			//gtk_button_set_image(GTK_BUTTON(gwidget->CapAVIButt), gwidget->AVIButton_Img);
			AVI_set_video(AviOut, videoIn->width, videoIn->height, 
				videoIn->fps,compression);
	  
			/* start video capture*/
			global->AVIstarttime = ms_time();
			videoIn->capAVI = TRUE;
			pdata->capAVI = videoIn->capAVI;
			
			/* start sound capture*/
			if(global->Sound_enable > 0) 
			{
				/*get channels and sample rate*/
				set_sound(global,pdata);
				/*set audio header for avi*/
				AVI_set_audio(AviOut, global->Sound_NumChan, 
					global->Sound_SampRate,
					global->Sound_bitRate,
					16, /*only used for PCM*/
					global->Sound_Format);
				/* Initialize sound (open stream)*/
				if(init_sound (pdata)) 
				{
					g_printerr("error opening portaudio\n");
					global->Sound_enable=0;
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->SndEnable),0);
				} 
				else 
				{
					if (global->Sound_Format == ISO_FORMAT_MPEG12) 
					{
						init_MP2_encoder(pdata, global->Sound_bitRate);    
					}
				}
			}
		}
		gtk_button_set_label(GTK_BUTTON(gwidget->CapAVIButt),_("Stop AVI"));
		gtk_widget_queue_draw (gwidget->CapAVIButt);
		while (gtk_events_pending ()) gtk_main_iteration (); 
	}
	
	gwidget = NULL;
	pdata = NULL;
	global = NULL;
	videoIn = NULL;
	AviOut = NULL;
}

/*--------------------- buttons callbacks ------------------*/
void
SProfileButton_clicked (GtkButton * SProfileButton, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	gwidget->FileDialog = gtk_file_chooser_dialog_new (_("Save File"),
		GTK_WINDOW(gwidget->mainwin),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (gwidget->FileDialog), TRUE);
	//printf("profile(default):%s/%s\n",global->profile_FPath[1],global->profile_FPath[0]);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (gwidget->FileDialog),
		global->profile_FPath[1]);
	
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (gwidget->FileDialog),
		global->profile_FPath[0]);
	
	if (gtk_dialog_run (GTK_DIALOG (gwidget->FileDialog)) == GTK_RESPONSE_ACCEPT)
	{
		/*Save Controls Data*/
		char *filename= gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (gwidget->FileDialog));
		global->profile_FPath=splitPath(filename,global->profile_FPath);
		SaveControls(s, global, videoIn);
	}
	gtk_widget_destroy (gwidget->FileDialog);
	
	gwidget = NULL;
	s = NULL;
	global = NULL;
	videoIn = NULL;
}

void
LProfileButton_clicked (GtkButton * LProfileButton, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	
	gwidget->FileDialog = gtk_file_chooser_dialog_new (_("Load File"),
		GTK_WINDOW(gwidget->mainwin),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		NULL);
	
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (gwidget->FileDialog),
		global->profile_FPath[1]);
	
	
	if (gtk_dialog_run (GTK_DIALOG (gwidget->FileDialog)) == GTK_RESPONSE_ACCEPT)
	{
		/*Load Controls Data*/
		char *filename= gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (gwidget->FileDialog));
		global->profile_FPath=splitPath(filename,global->profile_FPath);
		LoadControls(s,global);
	}
	gtk_widget_destroy (gwidget->FileDialog);
	
	gwidget = NULL;
	s = NULL;
	global = NULL;
}

/*called when avi max file size reached*/
/* stops avi capture, increments avi file name, restart avi capture*/
void *
split_avi(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	//struct vdIn *videoIn = all_data->videoIn;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	/*make sure avi is in incremental mode*/
	if(!global->avi_inc) 
	{ 
		AVIInc_changed(GTK_TOGGLE_BUTTON(gwidget->AVIInc), all_data);
		global->avi_inc=1; /*just in case*/
	}
	
	/*stops avi capture*/
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapAVIButt), FALSE);
	/*sleep at least 200 ms*/
	g_usleep( 200000);
	/*wait for 300 cycles of 10 ms for avi capture button to activate*/
	int n = wait_ms(&(gwidget->avi_widget_state), TRUE, 10, 300);
	if(global->debug) g_printf("AVI: sleeped for ~ %d miliseconds before new capture\n", (300 - n) * 10 + 200);
	/*restarts avi capture with new file name*/
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapAVIButt), TRUE);
	/*sleep at least 80 ms*/
	g_usleep( 200000);
	
	/*thread as finished*/
	global->ButtPressThread = FALSE;
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



