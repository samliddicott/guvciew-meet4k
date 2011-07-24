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
#include "snd_devices.h"
#include "acodecs.h"
#include "../config.h"

static void 
lavc_audio_properties(GtkButton * CodecButt, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	int line = 0;
	acodecs_data *codec_defaults = get_aud_codec_defaults(get_ind_by4cc(global->Sound_Format));
	
	if (!(codec_defaults->avcodec)) return;
	
	GtkWidget *codec_dialog = gtk_dialog_new_with_buttons (_("audio codec values"),
		GTK_WINDOW(gwidget->mainwin),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_OK,
		GTK_RESPONSE_ACCEPT,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_REJECT,
		NULL);
	
	GtkWidget *table = gtk_table_new(1,2,FALSE);
	
	/*bit rate*/
	GtkWidget *lbl_bit_rate = gtk_label_new(_("bit rate:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_bit_rate), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_bit_rate, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_bit_rate);
	
	GtkWidget *bit_rate = gtk_spin_button_new_with_range(48000,384000,8000);
	gtk_editable_set_editable(GTK_EDITABLE(bit_rate),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(bit_rate), codec_defaults->bit_rate);
	
	gtk_table_attach (GTK_TABLE(table), bit_rate, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (bit_rate);
	line++;
	
	GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (codec_dialog));
	gtk_container_add (GTK_CONTAINER (content_area), table);
	gtk_widget_show (table);
	
	gint result = gtk_dialog_run (GTK_DIALOG (codec_dialog));
	switch (result)
	{
		case GTK_RESPONSE_ACCEPT:
			codec_defaults->bit_rate = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(bit_rate));
			break;
		default:
			// do nothing since dialog was cancelled
			break;
	}
	gtk_widget_destroy (codec_dialog);

}

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
	GtkWidget *label_SndComp;
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
	table3 = gtk_table_new(1,3,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table3), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table3), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table3), 2);
	gtk_widget_show (table3);
	//SCROLL
	scroll3=gtk_scrolled_window_new(NULL,NULL);
	//ADD TABLE TO SCROLL
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll3),table3);
	gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroll3),
		GTK_CORNER_TOP_LEFT);
	gtk_widget_show(scroll3);
	
	//new hbox for tab label and icon
	Tab3 = gtk_hbox_new(FALSE,2);
	Tab3Label = gtk_label_new(_("Audio"));
	gtk_widget_show (Tab3Label);
	//check for files
	gchar* Tab3IconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/audio_controls.png",NULL);
	//don't test for file - use default empty image if load fails
	//get icon image
	Tab3Icon = gtk_image_new_from_file(Tab3IconPath);
	g_free(Tab3IconPath);
	gtk_widget_show (Tab3Icon);
	gtk_box_pack_start (GTK_BOX(Tab3), Tab3Icon, FALSE, FALSE,1);
	gtk_box_pack_start (GTK_BOX(Tab3), Tab3Label, FALSE, FALSE,1);
	gtk_widget_show (Tab3);
	
	//ADD SCROLL to NOTEBOOK (TAB)
	gtk_notebook_append_page(GTK_NOTEBOOK(gwidget->boxh),scroll3,Tab3);
	//--------------------- sound controls ------------------------------
	//enable sound
	line++;
	gwidget->SndEnable=gtk_check_button_new_with_label (_(" Sound"));
	gtk_table_attach(GTK_TABLE(table3), gwidget->SndEnable, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->SndEnable),
		(global->Sound_enable > 0));
	gtk_widget_show (gwidget->SndEnable);
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->SndEnable), "toggled",
		G_CALLBACK (SndEnable_changed), all_data);
		
	line++;
	 // VU meter on the image (OSD)
	GtkWidget* vuMeterEnable=gtk_check_button_new_with_label(_(" Show VU meter"));
	g_object_set_data(G_OBJECT(vuMeterEnable), "flag", GINT_TO_POINTER(OSD_METER));
	gtk_table_attach(GTK_TABLE(table3), vuMeterEnable, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vuMeterEnable),(global->osdFlags & OSD_METER)>0);
	gtk_widget_show(vuMeterEnable);
	g_signal_connect(GTK_CHECK_BUTTON(vuMeterEnable), "toggled", G_CALLBACK(osdChanged), all_data);
	
	//sound API
#ifdef PULSEAUDIO
	line++;
	
	gwidget->label_SndAPI = gtk_label_new(_("Audio API:"));
	gtk_misc_set_alignment (GTK_MISC (gwidget->label_SndAPI), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table3), gwidget->label_SndAPI, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->label_SndAPI);
	
	gwidget->SndAPI = gtk_combo_box_new_text ();;
	gtk_table_attach(GTK_TABLE(table3), gwidget->SndAPI, 1, 3, line, line+1,
		GTK_SHRINK | GTK_FILL , 0, 0, 0);
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndAPI),_("PORTAUDIO"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndAPI),_("PULSEAUDIO"));
	gtk_widget_show (gwidget->SndAPI);
	//default API - portaudio
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndAPI),global->Sound_API);
	
	if(global->Sound_API > 0) global->Sound_UseDev=-1; //force default device
	
	gtk_widget_set_sensitive (gwidget->SndAPI, TRUE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->SndAPI), "changed",
		G_CALLBACK (SndAPI_changed), all_data);
	
#endif
	
	//sound device
	line++;
	
	label_SndDevice = gtk_label_new(_("Input Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndDevice), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_SndDevice, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (label_SndDevice);
	
	// get sound device list and info
	gwidget->SndDevice = list_snd_devices (global);
	
	gtk_table_attach(GTK_TABLE(table3), gwidget->SndDevice, 1, 3, line, line+1,
		GTK_SHRINK | GTK_FILL , 0, 0, 0);
	gtk_widget_show (gwidget->SndDevice);
	//using default device
	if(global->Sound_UseDev < 0) global->Sound_UseDev=global->Sound_DefDev;
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndDevice),global->Sound_UseDev);
	
	//disable if using pulse api
	if (global->Sound_enable && !global->Sound_API) gtk_widget_set_sensitive (gwidget->SndDevice, TRUE);
	else  gtk_widget_set_sensitive (gwidget->SndDevice, FALSE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->SndDevice), "changed",
		G_CALLBACK (SndDevice_changed), all_data);
	
	label_SndDevice = gtk_label_new(_("Input Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndDevice), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_SndDevice, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_SndDevice);
	
	//sample rate
	line++;
	gwidget->SndSampleRate= gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndSampleRate),_("Dev. Default"));
	for( i=1; stdSampleRates[i] > 0; i++ )
	{
		char dst[8];
		g_snprintf(dst,7,"%d",stdSampleRates[i]);
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndSampleRate),dst);
	}
	if (global->Sound_SampRateInd>(i-1)) global->Sound_SampRateInd=0; /*out of range*/
	
	gtk_table_attach(GTK_TABLE(table3), gwidget->SndSampleRate, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->SndSampleRate);
	
	global->Sound_SampRate=stdSampleRates[global->Sound_SampRateInd];
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndSampleRate),global->Sound_SampRateInd); /*device default*/
	
	if (global->Sound_enable) gtk_widget_set_sensitive (gwidget->SndSampleRate, TRUE);
	else  gtk_widget_set_sensitive (gwidget->SndSampleRate, FALSE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->SndSampleRate), "changed",
		G_CALLBACK (SndSampleRate_changed), all_data);

	label_SndSampRate = gtk_label_new(_("Sample Rate:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndSampRate), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_SndSampRate, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_SndSampRate);
	
	//channels
	line++;
	gwidget->SndNumChan= gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndNumChan),_("Dev. Default"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndNumChan),_("1 - mono"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndNumChan),_("2 - stereo"));

	gtk_table_attach(GTK_TABLE(table3), gwidget->SndNumChan, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
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
	g_signal_connect (GTK_COMBO_BOX(gwidget->SndNumChan), "changed",
		G_CALLBACK (SndNumChan_changed), all_data);
	
	label_SndNumChan = gtk_label_new(_("Channels:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndNumChan), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_SndNumChan, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (label_SndNumChan);
	if (global->debug) g_printf("SampleRate:%d Channels:%d\n",global->Sound_SampRate,global->Sound_NumChan);
	
	//sound format
	line++;
	gwidget->SndComp = gtk_combo_box_new_text ();
	//sets to valid only existing codecs
	setAcodecVal ();
	int acodec_ind =0;
	for (acodec_ind =0; acodec_ind<MAX_ACODECS; acodec_ind++)
	{
		if (isAcodecValid(acodec_ind))
			gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndComp),gettext(get_aud_desc4cc(acodec_ind)));
		
	}

	int aud_ind = get_ind_by4cc(global->Sound_Format);
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndComp), aud_ind);
	global->Sound_Format = get_aud4cc(aud_ind); /*sync index returned with format*/
	
	if (global->Sound_enable) gtk_widget_set_sensitive (gwidget->SndComp, TRUE);
	
	g_signal_connect (GTK_COMBO_BOX(gwidget->SndComp), "changed",
		G_CALLBACK (SndComp_changed), all_data);
	
	gtk_table_attach(GTK_TABLE(table3), gwidget->SndComp, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_widget_show (gwidget->SndComp);
	label_SndComp = gtk_label_new(_("Audio Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndComp), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_SndComp, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (label_SndComp);
	
	//lavc codec properties button
	gwidget->lavc_aud_button = gtk_button_new_with_label (_("properties"));
	gtk_table_attach (GTK_TABLE(table3), gwidget->lavc_aud_button, 2, 3, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->lavc_aud_button);
	g_signal_connect (GTK_BUTTON(gwidget->lavc_aud_button), "clicked",
		G_CALLBACK (lavc_audio_properties), all_data);
	gtk_widget_set_sensitive (gwidget->lavc_aud_button, isLavcACodec(get_ind_by4cc(global->Sound_Format)));
	
	// Audio effects
	line++;
	label_audioFilters = gtk_label_new(_("---- Audio Effects ----"));
	gtk_misc_set_alignment (GTK_MISC (label_audioFilters), 0.5, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_audioFilters, 0, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL , 0, 0, 0);
	gtk_widget_show (label_audioFilters);
	
	line++;
	table_snd_eff = gtk_table_new(1,4,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table_snd_eff), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table_snd_eff), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table_snd_eff), 4);
	gtk_widget_set_size_request (table_snd_eff, -1, -1);
	
	gtk_table_attach (GTK_TABLE(table3), table_snd_eff, 0, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL , 0, 0, 0);
	gtk_widget_show (table_snd_eff);

	// Echo
	EffEchoEnable=gtk_check_button_new_with_label (_(" Echo"));
	g_object_set_data (G_OBJECT (EffEchoEnable), "effect_info", GINT_TO_POINTER(SND_ECHO));
	gtk_table_attach(GTK_TABLE(table_snd_eff), EffEchoEnable, 0, 1, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffEchoEnable),(pdata->snd_Flags & SND_ECHO)>0);
	gtk_widget_show (EffEchoEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffEchoEnable), "toggled",
		G_CALLBACK (EffEnable_changed), all_data);

	// FUZZ
	EffFuzzEnable=gtk_check_button_new_with_label (_(" Fuzz"));
	g_object_set_data (G_OBJECT (EffFuzzEnable), "effect_info", GINT_TO_POINTER(SND_FUZZ));
	gtk_table_attach(GTK_TABLE(table_snd_eff), EffFuzzEnable, 1, 2, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffFuzzEnable),(pdata->snd_Flags & SND_FUZZ)>0);
	gtk_widget_show (EffFuzzEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffFuzzEnable), "toggled",
		G_CALLBACK (EffEnable_changed), all_data);
	
	// Reverb
	EffRevEnable=gtk_check_button_new_with_label (_(" Reverb"));
	g_object_set_data (G_OBJECT (EffRevEnable), "effect_info", GINT_TO_POINTER(SND_REVERB));
	gtk_table_attach(GTK_TABLE(table_snd_eff), EffRevEnable, 2, 3, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffRevEnable),(pdata->snd_Flags & SND_REVERB)>0);
	gtk_widget_show (EffRevEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffRevEnable), "toggled",
		G_CALLBACK (EffEnable_changed), all_data);
	
	// WahWah
	EffWahEnable=gtk_check_button_new_with_label (_(" WahWah"));
	g_object_set_data (G_OBJECT (EffWahEnable), "effect_info", GINT_TO_POINTER(SND_WAHWAH));
	gtk_table_attach(GTK_TABLE(table_snd_eff), EffWahEnable, 3, 4, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffWahEnable),(pdata->snd_Flags & SND_WAHWAH)>0);
	gtk_widget_show (EffWahEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffWahEnable), "toggled",
		G_CALLBACK (EffEnable_changed), all_data);
	
	// Ducky
	EffDuckyEnable=gtk_check_button_new_with_label (_(" Ducky"));
	g_object_set_data (G_OBJECT (EffDuckyEnable), "effect_info", GINT_TO_POINTER(SND_DUCKY));
	gtk_table_attach(GTK_TABLE(table_snd_eff), EffDuckyEnable, 0, 1, 1, 2,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffDuckyEnable),(pdata->snd_Flags & SND_DUCKY)>0);
	gtk_widget_show (EffDuckyEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffDuckyEnable), "toggled",
		G_CALLBACK (EffEnable_changed), all_data);
		
}
