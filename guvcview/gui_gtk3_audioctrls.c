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

#include "../config.h"
#include "gui_gtk3.h"
#include "gui_gtk3_callbacks.h"
#include "gui.h"
#include "gviewaudio.h"
#include "video_capture.h"
/*add this last to avoid redefining _() and N_()*/
#include "gview.h"

extern int debug_level;

static audio_widgets_t my_audio_widgets =
{
	.api = NULL,
	.device = NULL,
	.channels = NULL,
	.samprate = NULL,
};


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
int gui_attach_gtk3_audioctrls(GtkWidget *parent)
{
	/*assertions*/
	assert(parent != NULL);

	int line = 0;
	int i = 0;
	/*get the current audio context*/
	audio_context_t *audio_ctx = get_audio_context();

	GtkWidget *audio_controls_grid = gtk_grid_new();
	gtk_widget_show (audio_controls_grid);

	gtk_grid_set_column_homogeneous (GTK_GRID(audio_controls_grid), FALSE);
	gtk_widget_set_hexpand (audio_controls_grid, TRUE);
	gtk_widget_set_halign (audio_controls_grid, GTK_ALIGN_FILL);

	gtk_grid_set_row_spacing (GTK_GRID(audio_controls_grid), 4);
	gtk_grid_set_column_spacing (GTK_GRID (audio_controls_grid), 4);
	gtk_container_set_border_width (GTK_CONTAINER (audio_controls_grid), 2);

	/*API*/
	line++;

	GtkWidget *label_SndAPI = gtk_label_new(_("Audio API:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndAPI), 1, 0.5);

	gtk_grid_attach (GTK_GRID(audio_controls_grid), label_SndAPI, 0, line, 1, 1);
	gtk_widget_show (label_SndAPI);

	my_audio_widgets.api = gtk_combo_box_text_new ();
	gtk_widget_set_halign (my_audio_widgets.api, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (my_audio_widgets.api, TRUE);
	gtk_grid_attach(GTK_GRID(audio_controls_grid), my_audio_widgets.api, 1, line, 1, 1);

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.api),_("NO SOUND"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.api),_("PORTAUDIO"));

#if HAS_PULSEAUDIO
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.api),_("PULSEAUDIO"));
#endif
	gtk_widget_show (my_audio_widgets.api);

	int api = AUDIO_NONE;
	if(audio_ctx != NULL)
		api = audio_ctx->api;

	gtk_combo_box_set_active(GTK_COMBO_BOX(my_audio_widgets.api), api);

	gtk_widget_set_sensitive (my_audio_widgets.api, TRUE);
	g_signal_connect (GTK_COMBO_BOX_TEXT(my_audio_widgets.api), "changed",
		G_CALLBACK (audio_api_changed), &my_audio_widgets);

	/*devices*/
	line++;

	GtkWidget *label_SndDevice = gtk_label_new(_("Input Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndDevice), 1, 0.5);

	gtk_grid_attach (GTK_GRID(audio_controls_grid), label_SndDevice, 0, line, 1, 1);
	gtk_widget_show (label_SndDevice);

	my_audio_widgets.device = gtk_combo_box_text_new ();
	gtk_widget_set_halign (my_audio_widgets.device, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (my_audio_widgets.device, TRUE);
	gtk_grid_attach(GTK_GRID(audio_controls_grid), my_audio_widgets.device, 1, line, 1, 1);
	gtk_widget_show (my_audio_widgets.device);

	if(audio_ctx != NULL)
	{
		for(i = 0; i < audio_ctx->num_input_dev; ++i)
		{
			gtk_combo_box_text_append_text(
				GTK_COMBO_BOX_TEXT(my_audio_widgets.device), 
				audio_ctx->list_devices[i].description);
		}

		gtk_widget_set_sensitive (my_audio_widgets.device, TRUE);
		gtk_combo_box_set_active(GTK_COMBO_BOX(my_audio_widgets.device), audio_ctx->device);
	}
	else
		gtk_widget_set_sensitive (my_audio_widgets.device, FALSE);

	g_signal_connect (GTK_COMBO_BOX_TEXT(my_audio_widgets.device), "changed",
		G_CALLBACK (audio_device_changed), &my_audio_widgets);


	/*sample rate*/
	line++;

	GtkWidget *label_SndSampRate = gtk_label_new(_("Sample Rate:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndSampRate), 1, 0.5);

	gtk_grid_attach (GTK_GRID(audio_controls_grid), label_SndSampRate, 0, line, 1, 1);
	gtk_widget_show (label_SndSampRate);

	my_audio_widgets.samprate = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate),_("Dev. Default"));

	/* add some standard sample rates:
	 *  96000, 88200, 64000, 48000, 44100,
	 *  32000, 24000, 22050, 16000, 12000,
	 *  11025, 8000, 7350
	 */
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "7350");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "8000");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "11025");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "12000");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "16000");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "22050");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "24000");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "32000");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "44100");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "48000");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "64000");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "88200");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "96000");

	gtk_widget_set_halign (my_audio_widgets.samprate, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (my_audio_widgets.samprate, TRUE);
	gtk_grid_attach(GTK_GRID(audio_controls_grid), my_audio_widgets.samprate, 1, line, 1, 1);
	gtk_widget_show (my_audio_widgets.samprate);

	gtk_combo_box_set_active(GTK_COMBO_BOX(my_audio_widgets.samprate), 0); /*device default*/

	if(audio_ctx != NULL)
		gtk_widget_set_sensitive (my_audio_widgets.samprate, TRUE);
	else
		gtk_widget_set_sensitive (my_audio_widgets.samprate, FALSE);

	g_signal_connect (GTK_COMBO_BOX_TEXT(my_audio_widgets.samprate), "changed",
		G_CALLBACK (audio_samplerate_changed), &my_audio_widgets);

	/*channels*/
	line++;

	GtkWidget *label_SndNumChan = gtk_label_new(_("Channels:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndNumChan), 1, 0.5);

	gtk_grid_attach (GTK_GRID(audio_controls_grid), label_SndNumChan, 0, line, 1, 1);
	gtk_widget_show (label_SndNumChan);

	my_audio_widgets.channels = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.channels),_("Dev. Default"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.channels),_("1 - mono"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(my_audio_widgets.channels),_("2 - stereo"));

	gtk_widget_set_halign (my_audio_widgets.channels, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (my_audio_widgets.channels, TRUE);
	gtk_grid_attach(GTK_GRID(audio_controls_grid), my_audio_widgets.channels, 1, line, 1, 1);
	gtk_widget_show (my_audio_widgets.channels);

	gtk_combo_box_set_active(GTK_COMBO_BOX(my_audio_widgets.channels), 0); /*device default*/
	
	if(audio_ctx != NULL)
		gtk_widget_set_sensitive (my_audio_widgets.channels, TRUE);
	else
		gtk_widget_set_sensitive (my_audio_widgets.channels, FALSE);

	g_signal_connect (GTK_COMBO_BOX_TEXT(my_audio_widgets.channels), "changed",
		G_CALLBACK (audio_channels_changed), &my_audio_widgets);

	/*add control grid to parent container*/
	gtk_container_add(GTK_CONTAINER(parent), audio_controls_grid);
	
	return 0;
}
