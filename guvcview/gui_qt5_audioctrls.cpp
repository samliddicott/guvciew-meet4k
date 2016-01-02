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

#include <iostream>
#include <vector>
#include <string>

#include "gui_qt5.hpp"

extern "C" {
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "../config.h"
#include "gui.h"
#include "gviewaudio.h"
#include "video_capture.h"
/*add this last to avoid redefining _() and N_()*/
#include "gview.h"
}

extern int debug_level;

/*
 * attach audio controls tab widget
 * args:
 *   parent - tab parent widget
 *
 * asserts:
 *   parent is not null
 *
 * returns: error code (0 -OK)
 */
int MainWindow::gui_attach_qt5_audioctrls(QWidget *parent)
{
	/*assertions*/
	assert(parent != NULL);

	if(debug_level > 1)
		std::cout << "GUVCVIEW (Qt5): attaching audio controls" 
			<< std::endl;

	int line = 0;

	/*get the current audio context*/
	audio_context_t *audio_ctx = get_audio_context();

	QGridLayout *grid_layout = new QGridLayout();

	audio_controls_grid = new QWidget(parent);
	audio_controls_grid->setLayout(grid_layout);
	audio_controls_grid->show();


	/*API*/

	QLabel *label_SndAPI = new QLabel(_("Audio API:"), 
		audio_controls_grid);
	label_SndAPI->show();
	grid_layout->addWidget(label_SndAPI, line, 0, Qt::AlignRight);
	
	combobox_audio_api = new QComboBox(audio_controls_grid);
	combobox_audio_api->show();
	grid_layout->addWidget(combobox_audio_api, line, 1);

	combobox_audio_api->addItem(_("NO SOUND"), 0);
	combobox_audio_api->addItem(_("PORTAUDIO"), 1);
#if HAS_PULSEAUDIO
	combobox_audio_api->addItem(_("PULSEAUDIO"), 2);
#endif

	int api = AUDIO_NONE;
	if(audio_ctx != NULL)
		api = audio_ctx->api;

	combobox_audio_api->setCurrentIndex(api);

	/*signals*/
	connect(combobox_audio_api, SIGNAL(currentIndexChanged(int)), 
		this, SLOT(audio_api_changed(int)));

	/*devices*/
	line++;

	QLabel *label_SndDevice = new QLabel(_("Input Device:"), 
		audio_controls_grid);
	label_SndDevice->show();
	grid_layout->addWidget(label_SndDevice, line, 0, Qt::AlignRight);

	combobox_audio_devices = new QComboBox(audio_controls_grid);
	combobox_audio_devices->show();
	grid_layout->addWidget(combobox_audio_devices, line, 1);

	if(audio_ctx != NULL)
	{
		int i = 0;
		for(i = 0; i < audio_ctx->num_input_dev; ++i)
			combobox_audio_devices->addItem(audio_ctx->list_devices[i].description, i);

		combobox_audio_devices->setCurrentIndex(audio_ctx->device);

	}
	else
		combobox_audio_devices->setDisabled(true);

	/*signals*/
	connect(combobox_audio_devices, SIGNAL(currentIndexChanged(int)), 
		this, SLOT(audio_devices_changed(int)));
	

	/*sample rate*/
	line++;

	QLabel *label_SndSampRate = new QLabel(_("Sample Rate:"), 
		audio_controls_grid);
	label_SndSampRate->show();
	grid_layout->addWidget(label_SndSampRate, line, 0, Qt::AlignRight);
	
	combobox_audio_samprate = new QComboBox(audio_controls_grid);
	combobox_audio_samprate->show();
	grid_layout->addWidget(combobox_audio_samprate, line, 1);
	
	combobox_audio_samprate->addItem(_("Dev. Default"), 0);
	

	/* add some standard sample rates:
	 *  96000, 88200, 64000, 48000, 44100,
	 *  32000, 24000, 22050, 16000, 12000,
	 *  11025, 8000, 7350
	 */
	combobox_audio_samprate->addItem("7350", 7350);
	combobox_audio_samprate->addItem("8000", 8000);
	combobox_audio_samprate->addItem("11025", 11025);
	combobox_audio_samprate->addItem("12000", 12000);
	combobox_audio_samprate->addItem("16000", 16000);
	combobox_audio_samprate->addItem("22050", 22050);
	combobox_audio_samprate->addItem("24000", 24000);
	combobox_audio_samprate->addItem("32000", 32000);
	combobox_audio_samprate->addItem("44100", 44100);
	combobox_audio_samprate->addItem("48000", 48000);
	combobox_audio_samprate->addItem("64000", 64000);
	combobox_audio_samprate->addItem("88200", 88200);
	combobox_audio_samprate->addItem("96000", 96000);


	combobox_audio_samprate->setCurrentIndex(0); /*device default*/

	if(audio_ctx != NULL)
		combobox_audio_samprate->setDisabled(false);
	else
		combobox_audio_samprate->setDisabled(true);

	/*signals*/
	connect(combobox_audio_samprate, SIGNAL(currentIndexChanged(int)), 
		this, SLOT(audio_samplerate_changed(int)));
	
	/*channels*/
	line++;

	QLabel *label_SndNumChan = new QLabel(_("Channels:"), 
		audio_controls_grid);
	label_SndNumChan->show();
	grid_layout->addWidget(label_SndNumChan, line, 0, Qt::AlignRight);
	
	combobox_audio_channels = new QComboBox(audio_controls_grid);
	combobox_audio_channels->show();
	grid_layout->addWidget(combobox_audio_channels, line, 1);

	combobox_audio_channels->addItem(_("Dev. Default"), 0);
	combobox_audio_channels->addItem(_("1 - mono"), 1);
	combobox_audio_channels->addItem(_("2 - stereo"), 2);
	
	combobox_audio_channels->setCurrentIndex(0); /*device default*/

	if(audio_ctx != NULL)
		combobox_audio_channels->setDisabled(false);
	else
		combobox_audio_channels->setDisabled(true);

	/*signals*/
	connect(combobox_audio_samprate, SIGNAL(currentIndexChanged(int)), 
		this, SLOT(audio_channels_changed(int)));
	
	/*latency*/
	line++;

	QLabel *label_Latency = new QLabel(_("Latency:"), 
		audio_controls_grid);
	label_Latency->show();
	grid_layout->addWidget(label_Latency, line, 0, Qt::AlignRight);
	
	double latency = 0.0;
	if(audio_ctx != NULL)
		latency = audio_ctx->latency;
	
	if(debug_level > 2)
		std::cout << "GUVCVIEW (Qt5): audio latency is set to " 
			<< latency << std::endl;
			
	spinbox_audio_latency = new QDoubleSpinBox(audio_controls_grid);
	spinbox_audio_latency->show();
	grid_layout->addWidget(spinbox_audio_latency, line, 1);
	
	spinbox_audio_latency->setRange(0.001, 0.1);
	spinbox_audio_latency->setSingleStep(0.001);
	spinbox_audio_latency->setValue(latency);

	if(audio_ctx != NULL)
		spinbox_audio_latency->setDisabled(false);
	else
		spinbox_audio_latency->setDisabled(true);

	/*signals*/
	connect(spinbox_audio_latency,SIGNAL(valueChanged(double)),this, SLOT(audio_latency_changed(double)));
	
	///* ----- Filter controls -----*/
	//line++;
	//GtkWidget *label_audioFilters = gtk_label_new(_("---- Audio Filters ----"));
//#if GTK_VER_AT_LEAST(3,15)
	//gtk_label_set_xalign(GTK_LABEL(label_audioFilters), 0.5);
	//gtk_label_set_yalign(GTK_LABEL(label_audioFilters), 0.5);
//#else
	//gtk_misc_set_alignment (GTK_MISC (label_audioFilters), 0.5, 0.5);
//#endif

	//gtk_grid_attach (GTK_GRID(audio_controls_grid), label_audioFilters, 0, line, 3, 1);
	//gtk_widget_show (label_audioFilters);

	///*filters grid*/
	//line++;
	//GtkWidget *table_filt = gtk_grid_new();
	//gtk_grid_set_row_spacing (GTK_GRID (table_filt), 4);
	//gtk_grid_set_column_spacing (GTK_GRID (table_filt), 4);
	//gtk_container_set_border_width (GTK_CONTAINER (table_filt), 4);
	//gtk_widget_set_size_request (table_filt, -1, -1);

	//gtk_widget_set_halign (table_filt, GTK_ALIGN_FILL);
	//gtk_widget_set_hexpand (table_filt, TRUE);
	//gtk_grid_attach (GTK_GRID(audio_controls_grid), table_filt, 0, line, 3, 1);
	//gtk_widget_show (table_filt);

	///* Echo FX */
	//GtkWidget *FiltEchoEnable = gtk_check_button_new_with_label (_(" Echo"));
	//g_object_set_data (G_OBJECT (FiltEchoEnable), "filt_info", GINT_TO_POINTER(AUDIO_FX_ECHO));
	//gtk_widget_set_halign (FiltEchoEnable, GTK_ALIGN_FILL);
	//gtk_widget_set_hexpand (FiltEchoEnable, TRUE);
	//gtk_grid_attach(GTK_GRID(table_filt), FiltEchoEnable, 0, 0, 1, 1);

	//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltEchoEnable),
		//(get_audio_fx_mask() & AUDIO_FX_ECHO) > 0);
	//gtk_widget_show (FiltEchoEnable);
	//g_signal_connect (GTK_CHECK_BUTTON(FiltEchoEnable), "toggled",
		//G_CALLBACK (audio_fx_filter_changed), NULL);

	///* Reverb FX */
	//GtkWidget *FiltReverbEnable = gtk_check_button_new_with_label (_(" Reverb"));
	//g_object_set_data (G_OBJECT (FiltReverbEnable), "filt_info", GINT_TO_POINTER(AUDIO_FX_REVERB));
	//gtk_widget_set_halign (FiltReverbEnable, GTK_ALIGN_FILL);
	//gtk_widget_set_hexpand (FiltReverbEnable, TRUE);
	//gtk_grid_attach(GTK_GRID(table_filt), FiltReverbEnable, 1, 0, 1, 1);

	//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltReverbEnable),
		//(get_audio_fx_mask() & AUDIO_FX_REVERB) > 0);
	//gtk_widget_show (FiltReverbEnable);
	//g_signal_connect (GTK_CHECK_BUTTON(FiltReverbEnable), "toggled",
		//G_CALLBACK (audio_fx_filter_changed), NULL);

	///* Fuzz FX */
	//GtkWidget *FiltFuzzEnable = gtk_check_button_new_with_label (_(" Fuzz"));
	//g_object_set_data (G_OBJECT (FiltFuzzEnable), "filt_info", GINT_TO_POINTER(AUDIO_FX_FUZZ));
	//gtk_widget_set_halign (FiltFuzzEnable, GTK_ALIGN_FILL);
	//gtk_widget_set_hexpand (FiltFuzzEnable, TRUE);
	//gtk_grid_attach(GTK_GRID(table_filt), FiltFuzzEnable, 2, 0, 1, 1);

	//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltFuzzEnable),
		//(get_audio_fx_mask() & AUDIO_FX_FUZZ) > 0);
	//gtk_widget_show (FiltFuzzEnable);
	//g_signal_connect (GTK_CHECK_BUTTON(FiltFuzzEnable), "toggled",
		//G_CALLBACK (audio_fx_filter_changed), NULL);

	///* WahWah FX */
	//GtkWidget *FiltWahEnable = gtk_check_button_new_with_label (_(" WahWah"));
	//g_object_set_data (G_OBJECT (FiltWahEnable), "filt_info", GINT_TO_POINTER(AUDIO_FX_WAHWAH));
	//gtk_widget_set_halign (FiltWahEnable, GTK_ALIGN_FILL);
	//gtk_widget_set_hexpand (FiltWahEnable, TRUE);
	//gtk_grid_attach(GTK_GRID(table_filt), FiltWahEnable, 3, 0, 1, 1);

	//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltWahEnable),
		//(get_audio_fx_mask() & AUDIO_FX_WAHWAH) > 0);
	//gtk_widget_show (FiltWahEnable);
	//g_signal_connect (GTK_CHECK_BUTTON(FiltWahEnable), "toggled",
		//G_CALLBACK (audio_fx_filter_changed), NULL);

	///* Ducky FX */
	//GtkWidget *FiltDuckyEnable = gtk_check_button_new_with_label (_(" Ducky"));
	//g_object_set_data (G_OBJECT (FiltDuckyEnable), "filt_info", GINT_TO_POINTER(AUDIO_FX_DUCKY));
	//gtk_widget_set_halign (FiltDuckyEnable, GTK_ALIGN_FILL);
	//gtk_widget_set_hexpand (FiltDuckyEnable, TRUE);
	//gtk_grid_attach(GTK_GRID(table_filt), FiltDuckyEnable, 4, 0, 1, 1);

	//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltDuckyEnable),
		//(get_audio_fx_mask() & AUDIO_FX_DUCKY) > 0);
	//gtk_widget_show (FiltDuckyEnable);
	//g_signal_connect (GTK_CHECK_BUTTON(FiltDuckyEnable), "toggled",
		//G_CALLBACK (audio_fx_filter_changed), NULL);

	line++;
	QSpacerItem *spacer = new QSpacerItem(40, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);
	grid_layout->addItem(spacer, line, 0);
	QSpacerItem *hspacer = new QSpacerItem(40, 20, QSizePolicy::Expanding);
	grid_layout->addItem(hspacer, line, 1);

	return 0;
}
