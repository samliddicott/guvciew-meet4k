/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#           Dr. Alexander K. Seewald <alex@seewald.at>                          #
#                             Autofocus algorithm                               #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#           George Sedov <radist.morse@gmail.com>                               #
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
/* locale.h is needed if -O0 used (no optimiztions)  */
/* otherwise included from libintl.h on glib/gi18n.h */
#include <locale.h>
#include <gtk/gtk.h>

#include "../config.h"
#include "string_utils.h"
#include "options.h"
#include "vcodecs.h"
#include "acodecs.h"
#include "callbacks.h"

GtkWidget *create_menu(struct ALL_DATA *all_data, int control_only)
{
	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	GtkWidget *menubar;

	GtkWidget *controls_menu;
	GtkWidget *controls_top;
	GtkWidget *controls_load;
	GtkWidget *controls_save;
	GtkWidget *controls_default;

	menubar = gtk_menu_bar_new();
	gtk_menu_bar_set_pack_direction(GTK_MENU_BAR(menubar), GTK_PACK_DIRECTION_LTR);
	
	controls_menu = gtk_menu_new();

	//controls menu
	controls_top = gtk_menu_item_new_with_label(_("Controls"));
	controls_load = gtk_menu_item_new_with_label(_("Load"));
	controls_save = gtk_menu_item_new_with_label(_("Save"));
	controls_default = gtk_menu_item_new_with_label(_("Default"));

	gtk_widget_show (controls_top);
	gtk_widget_show (controls_load);
	gtk_widget_show (controls_save);
	gtk_widget_show (controls_default);
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(controls_top), controls_menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(controls_menu), controls_load);
	gtk_menu_shell_append(GTK_MENU_SHELL(controls_menu), controls_save);
	gtk_menu_shell_append(GTK_MENU_SHELL(controls_menu), controls_default);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), controls_top);

	if(!control_only) /*control_only exclusion */
	{
		GtkWidget *item = NULL;

		GtkWidget *video_menu;
		GtkWidget *video_top;
		GtkWidget *video_file;
		GtkWidget *video_timestamp;
		GtkWidget *video_codec_menu;
		GtkWidget *video_codec_top;
		GtkWidget *video_codec_prop;
		GtkWidget *audio_codec_menu;
		GtkWidget *audio_codec_top;
		GtkWidget *audio_codec_prop;

		GtkWidget *photo_menu;
		GtkWidget *photo_top;
		GtkWidget *photo_file;
		GtkWidget *photo_timestamp;

		video_menu = gtk_menu_new();
		photo_menu = gtk_menu_new();

		//video menu
		video_top = gtk_menu_item_new_with_label(_("Video"));
		video_file = gtk_menu_item_new_with_label(_("File"));
		video_timestamp = gtk_check_menu_item_new_with_label(_("Append timestamp"));

		gtk_widget_show (video_top);
		gtk_widget_show (video_file);
		gtk_widget_show (video_timestamp);
		
		/** flag the file dialog as video file*/
		g_object_set_data (G_OBJECT (video_file), "file_butt", GINT_TO_POINTER(1));
		g_signal_connect (GTK_MENU_ITEM(video_file), "activate",
			G_CALLBACK (file_chooser), all_data);
		
		//video codec submenu
		video_codec_menu = gtk_menu_new();
		video_codec_top = gtk_menu_item_new_with_label(_("Video Codec"));
		gtk_widget_show (video_codec_top);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(video_codec_top), video_codec_menu);
		//Add codecs to submenu
		gwidget->vgroup = NULL;
		int num_vcodecs = setVcodecVal();
		int vcodec_ind =0;
		for (vcodec_ind =0; vcodec_ind < num_vcodecs; vcodec_ind++)
		{
			item = gtk_radio_menu_item_new_with_label(gwidget->vgroup, gettext(get_desc4cc(vcodec_ind)));
			if (vcodec_ind == global->VidCodec)
				gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
		
			//NOTE: GSList indexes (g_slist_index) are in reverse order: last inserted has index 0
			gwidget->vgroup = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
			
			gtk_widget_show (item);
			gtk_menu_shell_append(GTK_MENU_SHELL(video_codec_menu), item);
			
			g_signal_connect (GTK_RADIO_MENU_ITEM(item), "toggled",
                G_CALLBACK (VidCodec_menu_changed), all_data);
		}
		
		video_codec_prop =  gtk_menu_item_new_with_label(_("Video Codec Properties"));
		gtk_widget_show (video_codec_prop);
		g_signal_connect (GTK_MENU_ITEM(video_codec_prop), "activate",
			G_CALLBACK (lavc_properties), all_data);
		
		//Audio codec submenu
		audio_codec_menu = gtk_menu_new();
		audio_codec_top = gtk_menu_item_new_with_label(_("Audio Codec"));
		gtk_widget_show (audio_codec_top);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(audio_codec_top), audio_codec_menu);
		//Add codecs to submenu
		gwidget->agroup = NULL;
		int num_acodecs = setAcodecVal();
		int acodec_ind =0;
		for (acodec_ind =0; acodec_ind < num_acodecs; acodec_ind++)
		{
			item = gtk_radio_menu_item_new_with_label(gwidget->agroup, gettext(get_aud_desc4cc(acodec_ind)));
			gwidget->agroup = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
			if (acodec_ind == global->AudCodec)
				gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
			
			gtk_widget_show (item);
			gtk_menu_shell_append(GTK_MENU_SHELL(audio_codec_menu), item);
			
			g_signal_connect (GTK_RADIO_MENU_ITEM(item), "toggled",
                G_CALLBACK (AudCodec_menu_changed), all_data);
		}
		
		audio_codec_prop = gtk_menu_item_new_with_label(_("Audio Codec Properties"));
		gtk_widget_show (audio_codec_prop);
		g_signal_connect (GTK_MENU_ITEM(audio_codec_prop), "activate",
			G_CALLBACK (lavc_audio_properties), all_data);
		
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(video_top), video_menu);
		gtk_menu_shell_append(GTK_MENU_SHELL(video_menu), video_file);
		gtk_menu_shell_append(GTK_MENU_SHELL(video_menu), video_timestamp);
		gtk_menu_shell_append(GTK_MENU_SHELL(video_menu), video_codec_top);
		gtk_menu_shell_append(GTK_MENU_SHELL(video_menu), video_codec_prop);
		gtk_menu_shell_append(GTK_MENU_SHELL(video_menu), audio_codec_top);
		gtk_menu_shell_append(GTK_MENU_SHELL(video_menu), audio_codec_prop);
		gtk_menu_shell_append(GTK_MENU_SHELL(menubar), video_top);

		//photo menu
		photo_top = gtk_menu_item_new_with_label(_("Photo"));
		photo_file = gtk_menu_item_new_with_label(_("File"));
		photo_timestamp = gtk_check_menu_item_new_with_label(_("Append timestamp"));

		gtk_widget_show (photo_top);
		gtk_widget_show (photo_file);
		gtk_widget_show (photo_timestamp);
		
		/** flag the file dialog as image file*/
		g_object_set_data (G_OBJECT (photo_file), "file_butt", GINT_TO_POINTER(0));
		g_signal_connect (GTK_MENU_ITEM(photo_file), "activate",
			G_CALLBACK (file_chooser), all_data);
		
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(photo_top), photo_menu);
		gtk_menu_shell_append(GTK_MENU_SHELL(photo_menu), photo_file);
		gtk_menu_shell_append(GTK_MENU_SHELL(photo_menu), photo_timestamp);
		gtk_menu_shell_append(GTK_MENU_SHELL(menubar), photo_top);
	}
	
	gtk_widget_show (menubar);
	gtk_container_set_resize_mode (GTK_CONTAINER(menubar), GTK_RESIZE_PARENT);
	return menubar;
}
