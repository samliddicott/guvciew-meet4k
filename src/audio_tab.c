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
#include <glib.h>
#include <glib/gprintf.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "globals.h"
#include "callbacks.h"
#include "v4l2uvc.h"
#include "snd_devices.h"
#include "acodecs.h"
#include "../config.h"

void audio_tab(struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;
	struct paRecordData *pdata = all_data->pdata;

	GtkWidget *table3;
	GtkWidget *scroll3;
	GtkWidget *Tab3;
	GtkWidget *Tab3Label;
	GtkWidget *Tab3Icon;
	GtkWidget *label_SndSampRate;
	GtkWidget *label_SndDevice;
	GtkWidget *label_SndNumChan;
	GtkWidget *label_audioFilters;
	GtkWidget *table_snd_eff;
	GtkWidget *EffEchoEnable;
	GtkWidget *EffFuzzEnable;
	GtkWidget* EffRevEnable;
	GtkWidget* EffWahEnable;
	GtkWidget* EffDuckyEnable;

	int line = 0;
	int i = 0;
	//TABLE
	table3 = gtk_grid_new();
	gtk_grid_set_column_homogeneous (GTK_GRID(table3), FALSE);
	gtk_widget_set_hexpand (table3, TRUE);
	gtk_widget_set_halign (table3, GTK_ALIGN_FILL);

	gtk_grid_set_row_spacing (GTK_GRID(table3), 4);
	gtk_grid_set_column_spacing (GTK_GRID (table3), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table3), 2);
	gtk_widget_show (table3);

	//SCROLL
	scroll3=gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroll3),
		GTK_CORNER_TOP_LEFT);
	//ADD TABLE TO SCROLL
	
    //viewport is only needed for gtk < 3.8
    //for 3.8 and above s->table can be directly added to scroll1
    GtkWidget* viewport = gtk_viewport_new(NULL,NULL);
    gtk_container_add(GTK_CONTAINER(viewport), table3);
    gtk_widget_show(viewport);

    gtk_container_add(GTK_CONTAINER(scroll3), viewport);	
	gtk_widget_show(scroll3);

	//new grid for tab label and icon
	Tab3 = gtk_grid_new();
	Tab3Label = gtk_label_new(_("Audio"));
	gtk_widget_show (Tab3Label);
	//check for files
	gchar* Tab3IconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/audio_controls.png",NULL);
	//don't test for file - use default empty image if load fails
	//get icon image
	Tab3Icon = gtk_image_new_from_file(Tab3IconPath);
	g_free(Tab3IconPath);
	gtk_widget_show (Tab3Icon);
	gtk_grid_attach (GTK_GRID(Tab3), Tab3Icon, 0, 0, 1, 1);
	gtk_grid_attach (GTK_GRID(Tab3), Tab3Label, 1, 0, 1, 1);
	gtk_widget_show (Tab3);

	//ADD SCROLL to NOTEBOOK (TAB)
	gtk_notebook_append_page(GTK_NOTEBOOK(gwidget->boxh),scroll3,Tab3);
	//--------------------- sound controls ------------------------------
	//enable sound
	line++;
	gwidget->SndEnable=gtk_check_button_new_with_label (_(" Sound"));
	gtk_grid_attach(GTK_GRID(table3), gwidget->SndEnable, 1, line, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->SndEnable),
		(global->Sound_enable > 0));
	gtk_widget_show (gwidget->SndEnable);
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->SndEnable), "toggled",
		G_CALLBACK (SndEnable_changed), all_data);

	line++;
	 // VU meter on the image (OSD)
	GtkWidget* vuMeterEnable=gtk_check_button_new_with_label(_(" Show VU meter"));
	g_object_set_data(G_OBJECT(vuMeterEnable), "flag", GINT_TO_POINTER(OSD_METER));
	gtk_grid_attach(GTK_GRID(table3), vuMeterEnable, 1, line, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vuMeterEnable),(global->osdFlags & OSD_METER)>0);
	gtk_widget_show(vuMeterEnable);
	g_signal_connect(GTK_CHECK_BUTTON(vuMeterEnable), "toggled", G_CALLBACK(osdChanged), all_data);

	//sound API
#ifdef PULSEAUDIO
	line++;

	gwidget->label_SndAPI = gtk_label_new(_("Audio API:"));
	gtk_misc_set_alignment (GTK_MISC (gwidget->label_SndAPI), 1, 0.5);

	gtk_grid_attach (GTK_GRID(table3), gwidget->label_SndAPI, 0, line, 1, 1);
	gtk_widget_show (gwidget->label_SndAPI);

	gwidget->SndAPI = gtk_combo_box_text_new ();
	gtk_widget_set_halign (gwidget->SndAPI, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->SndAPI, TRUE);
	gtk_grid_attach(GTK_GRID(table3), gwidget->SndAPI, 1, line, 1, 1);

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->SndAPI),_("PORTAUDIO"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->SndAPI),_("PULSEAUDIO"));
	gtk_widget_show (gwidget->SndAPI);
	//default API - portaudio
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndAPI),global->Sound_API);

	//if(global->Sound_API > 0) global->Sound_UseDev=-1; //force default device

	gtk_widget_set_sensitive (gwidget->SndAPI, TRUE);
	g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->SndAPI), "changed",
		G_CALLBACK (SndAPI_changed), all_data);

#endif

	//sound device
	line++;

	label_SndDevice = gtk_label_new(_("Input Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndDevice), 1, 0.5);

	gtk_grid_attach (GTK_GRID(table3), label_SndDevice, 0, line, 1, 1);
	gtk_widget_show (label_SndDevice);

	// get sound device list and info
	gwidget->SndDevice = list_snd_devices (global);

	gtk_widget_set_halign (gwidget->SndDevice, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->SndDevice, TRUE);
	gtk_grid_attach(GTK_GRID(table3), gwidget->SndDevice, 1, line, 1, 1);
	gtk_widget_show (gwidget->SndDevice);
	//using default device
	if(global->Sound_UseDev < 0) global->Sound_UseDev=global->Sound_DefDev;
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndDevice),global->Sound_UseDev);

	//disable if using pulse api
	//if (global->Sound_enable && !global->Sound_API) gtk_widget_set_sensitive (gwidget->SndDevice, TRUE);
	//else  gtk_widget_set_sensitive (gwidget->SndDevice, FALSE);
	g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->SndDevice), "changed",
		G_CALLBACK (SndDevice_changed), all_data);

	label_SndDevice = gtk_label_new(_("Input Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndDevice), 1, 0.5);

	gtk_grid_attach (GTK_GRID(table3), label_SndDevice, 0, line, 1, 1);

	gtk_widget_show (label_SndDevice);

	//sample rate
	line++;
	gwidget->SndSampleRate= gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->SndSampleRate),_("Dev. Default"));
	for( i=1; stdSampleRates[i] > 0; i++ )
	{
		char dst[8];
		g_snprintf(dst,7,"%d",stdSampleRates[i]);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->SndSampleRate),dst);
	}
	if (global->Sound_SampRateInd>(i-1)) global->Sound_SampRateInd=0; /*out of range*/

	gtk_widget_set_halign (gwidget->SndSampleRate, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->SndSampleRate, TRUE);
	gtk_grid_attach(GTK_GRID(table3), gwidget->SndSampleRate, 1, line, 1, 1);
	gtk_widget_show (gwidget->SndSampleRate);

	global->Sound_SampRate=stdSampleRates[global->Sound_SampRateInd];
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndSampleRate),global->Sound_SampRateInd); /*device default*/

	if (global->Sound_enable) gtk_widget_set_sensitive (gwidget->SndSampleRate, TRUE);
	else  gtk_widget_set_sensitive (gwidget->SndSampleRate, FALSE);
	g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->SndSampleRate), "changed",
		G_CALLBACK (SndSampleRate_changed), all_data);

	label_SndSampRate = gtk_label_new(_("Sample Rate:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndSampRate), 1, 0.5);

	gtk_grid_attach (GTK_GRID(table3), label_SndSampRate, 0, line, 1, 1);

	gtk_widget_show (label_SndSampRate);

	//channels
	line++;
	gwidget->SndNumChan= gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->SndNumChan),_("Dev. Default"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->SndNumChan),_("1 - mono"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->SndNumChan),_("2 - stereo"));

	gtk_widget_set_halign (gwidget->SndNumChan, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (gwidget->SndNumChan, TRUE);
	gtk_grid_attach(GTK_GRID(table3), gwidget->SndNumChan, 1, line, 1, 1);
	gtk_widget_show (gwidget->SndNumChan);
	switch (global->Sound_NumChanInd)
	{
		case 0:
			//device default
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndNumChan),0);
			break;

		case 1:
			//mono
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndNumChan),1);
			global->Sound_NumChan=1;
			break;

		case 2:
			//stereo
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndNumChan),2);
			global->Sound_NumChan=2;
			break;

		default:
			//set Default to NUM_CHANNELS
			global->Sound_NumChan=NUM_CHANNELS;
			break;
	}
	if (global->Sound_enable) gtk_widget_set_sensitive (gwidget->SndNumChan, TRUE);
	else gtk_widget_set_sensitive (gwidget->SndNumChan, FALSE);
	g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->SndNumChan), "changed",
		G_CALLBACK (SndNumChan_changed), all_data);

	label_SndNumChan = gtk_label_new(_("Channels:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndNumChan), 1, 0.5);

	gtk_grid_attach (GTK_GRID(table3), label_SndNumChan, 0, line, 1, 1);
	gtk_widget_show (label_SndNumChan);
	if (global->debug) g_print("SampleRate:%d Channels:%d\n",global->Sound_SampRate,global->Sound_NumChan);

	/** sound format
	 * this is now done in the video menu
	 */
	//line++;
	//gwidget->SndComp = gtk_combo_box_text_new ();
	/**sets to valid only existing codecs*/
	//int num_codecs = setAcodecVal();
	//int acodec_ind =0;
	//for (acodec_ind =0; acodec_ind < num_codecs; acodec_ind++)
	//{
	//	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->SndComp),gettext(get_aud_desc4cc(acodec_ind)));
	//}

	//gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndComp), global->AudCodec);
	//global->Sound_Format = get_aud4cc(global->AudCodec); /*sync index returned with format*/

	//if (global->Sound_enable) gtk_widget_set_sensitive (gwidget->SndComp, TRUE);

	//g_signal_connect (GTK_COMBO_BOX_TEXT(gwidget->SndComp), "changed",
	//	G_CALLBACK (SndComp_changed), all_data);

	//gtk_widget_set_halign (gwidget->SndComp, GTK_ALIGN_FILL);
	//gtk_widget_set_hexpand (gwidget->SndComp, TRUE);
	//gtk_grid_attach(GTK_GRID(table3), gwidget->SndComp, 1, line, 1, 1);

	//gtk_widget_show (gwidget->SndComp);
	//label_SndComp = gtk_label_new(_("Audio Format:"));
	//gtk_misc_set_alignment (GTK_MISC (label_SndComp), 1, 0.5);

	//gtk_grid_attach (GTK_GRID(table3), label_SndComp, 0, line, 1, 1);
	//gtk_widget_show (label_SndComp);

	/** lavc codec properties button
	 *  this is now done in the video menu
	 */
	//gwidget->lavc_aud_button = gtk_button_new_with_label (_("properties"));
	//gtk_grid_attach (GTK_GRID(table3), gwidget->lavc_aud_button, 2, line, 1, 1);
	//gtk_widget_show (gwidget->lavc_aud_button);
	//g_signal_connect (GTK_BUTTON(gwidget->lavc_aud_button), "clicked",
	//	G_CALLBACK (lavc_audio_properties), all_data);
	//gtk_widget_set_sensitive (gwidget->lavc_aud_button, isLavcACodec(get_ind_by4cc(global->Sound_Format)));

	// Audio effects
	line++;
	label_audioFilters = gtk_label_new(_("---- Audio Effects ----"));
	gtk_misc_set_alignment (GTK_MISC (label_audioFilters), 0.5, 0.5);

	gtk_grid_attach (GTK_GRID(table3), label_audioFilters, 0, line, 3, 1);
	gtk_widget_show (label_audioFilters);

	line++;
	table_snd_eff = gtk_grid_new();
	gtk_grid_set_row_spacing (GTK_GRID (table_snd_eff), 4);
	gtk_grid_set_column_spacing (GTK_GRID (table_snd_eff), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table_snd_eff), 4);
	gtk_widget_set_size_request (table_snd_eff, -1, -1);

	gtk_widget_set_halign (table_snd_eff, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (table_snd_eff, TRUE);
	gtk_grid_attach (GTK_GRID(table3), table_snd_eff, 0, line, 3, 1);
	gtk_widget_show (table_snd_eff);

	// Echo
	EffEchoEnable=gtk_check_button_new_with_label (_(" Echo"));
	g_object_set_data (G_OBJECT (EffEchoEnable), "effect_info", GINT_TO_POINTER(SND_ECHO));
	gtk_widget_set_halign (EffEchoEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (EffEchoEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_snd_eff), EffEchoEnable, 0, 0, 1, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffEchoEnable),(pdata->snd_Flags & SND_ECHO)>0);
	gtk_widget_show (EffEchoEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffEchoEnable), "toggled",
		G_CALLBACK (EffEnable_changed), all_data);

	// FUZZ
	EffFuzzEnable=gtk_check_button_new_with_label (_(" Fuzz"));
	g_object_set_data (G_OBJECT (EffFuzzEnable), "effect_info", GINT_TO_POINTER(SND_FUZZ));
	gtk_widget_set_halign (EffFuzzEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (EffFuzzEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_snd_eff), EffFuzzEnable, 1, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffFuzzEnable),(pdata->snd_Flags & SND_FUZZ)>0);
	gtk_widget_show (EffFuzzEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffFuzzEnable), "toggled",
		G_CALLBACK (EffEnable_changed), all_data);

	// Reverb
	EffRevEnable=gtk_check_button_new_with_label (_(" Reverb"));
	g_object_set_data (G_OBJECT (EffRevEnable), "effect_info", GINT_TO_POINTER(SND_REVERB));
	gtk_widget_set_halign (EffRevEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (EffRevEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_snd_eff), EffRevEnable, 2, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffRevEnable),(pdata->snd_Flags & SND_REVERB)>0);
	gtk_widget_show (EffRevEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffRevEnable), "toggled",
		G_CALLBACK (EffEnable_changed), all_data);

	// WahWah
	EffWahEnable=gtk_check_button_new_with_label (_(" WahWah"));
	g_object_set_data (G_OBJECT (EffWahEnable), "effect_info", GINT_TO_POINTER(SND_WAHWAH));
	gtk_widget_set_halign (EffWahEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (EffWahEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_snd_eff), EffWahEnable, 3, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffWahEnable),(pdata->snd_Flags & SND_WAHWAH)>0);
	gtk_widget_show (EffWahEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffWahEnable), "toggled",
		G_CALLBACK (EffEnable_changed), all_data);

	// Ducky
	EffDuckyEnable=gtk_check_button_new_with_label (_(" Ducky"));
	g_object_set_data (G_OBJECT (EffDuckyEnable), "effect_info", GINT_TO_POINTER(SND_DUCKY));
	gtk_widget_set_halign (EffDuckyEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (EffDuckyEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_snd_eff), EffDuckyEnable, 4, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffDuckyEnable),(pdata->snd_Flags & SND_DUCKY)>0);
	gtk_widget_show (EffDuckyEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffDuckyEnable), "toggled",
		G_CALLBACK (EffEnable_changed), all_data);

}
