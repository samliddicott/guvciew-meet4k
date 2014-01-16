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

#include <SDL/SDL.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <glib.h>
#include <pthread.h>
#include <gdk/gdkkeysyms.h>

#include <portaudio.h>

#include "v4l2uvc.h"
#include "uvc_h264.h"
#include "v4l2_dyna_ctrls.h"
#include "avilib.h"
#include "globals.h"
#include "sound.h"
#include "snd_devices.h"
#include "ms_time.h"
#include "string_utils.h"
#include "video.h"
#include "acodecs.h"
#include "profile.h"
#include "close.h"
#include "timers.h"
#include "callbacks.h"
#include "vcodecs.h"
#include "lavc_common.h"
#include "create_video.h"
#include "video_format.h"
#include "image_format.h"

#define __AMUTEX &pdata->mutex
#define __VMUTEX &videoIn->mutex
#define __GMUTEX &global->mutex

/*--------------------------- warning message dialog ----------------------------*/
void
WARN_DIALOG(const char *warn_title, const char* warn_msg, struct ALL_DATA *all_data)
{
    struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;

    if(global->no_display)
    {
        g_print("WARNING: %s : %s\n", warn_title, warn_msg);
    }
    else
    {
        GtkWidget *warndialog;
        warndialog = gtk_message_dialog_new (GTK_WINDOW(gwidget->mainwin),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_CLOSE,
            "%s",gettext(warn_title));

        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(warndialog),
            "%s",gettext(warn_msg));

        gtk_widget_show(warndialog);
        gtk_dialog_run (GTK_DIALOG (warndialog));
        gtk_widget_destroy (warndialog);
    }
}

/*---------------------------- error message dialog -----------------------------*/
void
ERR_DIALOG(const char *err_title, const char* err_msg, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	gboolean control_only = (global->control_only || global->add_ctrls);

    if(global->no_display)
    {
        g_printerr("ERROR: %s : %s\n", err_title, err_msg);
    }
    else
    {
        int i=0;

        GtkWidget *errdialog=NULL;
        GtkWidget *Devices=NULL;

        if (videoIn->listDevices->num_devices > 1)
        {
            errdialog = gtk_dialog_new_with_buttons (_("Error"),
                GTK_WINDOW(gwidget->mainwin),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_OK,
                GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL,
                GTK_RESPONSE_REJECT,
                NULL);

            GtkWidget *table = gtk_grid_new();

            GtkWidget *title = gtk_label_new (gettext(err_title));
            gtk_widget_modify_font(title, pango_font_description_from_string ("Sans bold 10"));
            gtk_misc_set_alignment (GTK_MISC (title), 0, 0);
            gtk_grid_attach (GTK_GRID (table), title, 0, 0, 2, 1);
            gtk_widget_show (title);

            GtkWidget *text = gtk_label_new (gettext(err_msg));
            gtk_widget_modify_font(text, pango_font_description_from_string ("Sans italic 8"));
            gtk_misc_set_alignment (GTK_MISC (text), 0, 0);
            gtk_grid_attach (GTK_GRID (table), text, 0, 1, 2, 1);
            gtk_widget_show (text);


            GtkWidget *text2 = gtk_label_new (_("\nYou have more than one video device installed.\n"
                "Do you want to try another one ?\n"));
            gtk_widget_modify_font(text2, pango_font_description_from_string ("Sans 10"));
            gtk_misc_set_alignment (GTK_MISC (text2), 0, 0);
            gtk_grid_attach (GTK_GRID (table), text2, 0, 2, 2, 1);
            gtk_widget_show (text2);

            GtkWidget *lbl_dev = gtk_label_new(_("Device:"));
            gtk_misc_set_alignment (GTK_MISC (lbl_dev), 0.5, 0.5);
            gtk_grid_attach (GTK_GRID(table), lbl_dev, 0, 3, 1, 1);
            gtk_widget_show (lbl_dev);

            Devices = gtk_combo_box_text_new ();

            for(i=0;i<(videoIn->listDevices->num_devices);i++)
            {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Devices),
                    videoIn->listDevices->listVidDevices[i].name);
            }
            gtk_combo_box_set_active(GTK_COMBO_BOX(Devices),videoIn->listDevices->num_devices-1);

            gtk_grid_attach(GTK_GRID(table), Devices, 1, 3, 1, 1);
            gtk_widget_show (Devices);

            GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (errdialog));
            gtk_container_add (GTK_CONTAINER (content_area), table);
            gtk_widget_show (table);
        }
        else
        {

            errdialog = gtk_message_dialog_new (GTK_WINDOW(gwidget->mainwin),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_CLOSE,
                "%s",gettext(err_title));

            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(errdialog),
                "%s",gettext(err_msg));

        }

        //gtk_widget_show(errdialog);

        gint result = gtk_dialog_run (GTK_DIALOG (errdialog));
        switch (result)
        {
            case GTK_RESPONSE_ACCEPT:
            {
                /*launch another guvcview instance for the selected device*/
                int index = gtk_combo_box_get_active(GTK_COMBO_BOX(Devices));
                //if(index == videoIn->listDevices->current_device)
                //    break;
                g_free(global->videodevice);
                global->videodevice = g_strdup(videoIn->listDevices->listVidDevices[index].device);
                gchar *command = g_strjoin("",
                g_get_prgname(),
                    " --device=",
                    global->videodevice,
                    NULL);
                /*spawn new process*/
                GError *error = NULL;
                if(!(g_spawn_command_line_async(command, &error)))
                {
                    g_printerr ("spawn failed: %s\n", error->message);
                    g_error_free ( error );
                }

            }
                break;

            default:
                /* do nothing since dialog was cancelled or closed */
                break;

        }

        gtk_widget_destroy (errdialog);
    }

	clean_struct(all_data);

	/* error dialog is allways called before creating the main loop */
	/* so no need for gtk_main_quit()                               */
	/* but this means we must close portaudio before exiting        */
	if(!control_only)
	{
		g_print("Closing portaudio ...");
		if (Pa_Terminate() != paNoError)
			g_print("Error\n");
		else
			g_print("OK\n");
	}

	g_print("Terminated.\n");;
	exit(1);
};

static void
filename_update_extension (GtkComboBox *chooser, GtkWidget *file_dialog)
{
	int index = gtk_combo_box_get_active (chooser);
	fprintf(stderr, "DEBUG: file filter changed to %i\n", index);

	gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (file_dialog));
	char *basename = g_path_get_basename(filename);

	//GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER (file_dialog));
	//if(G_IS_OBJECT(filter))
	//	g_object_unref(filter);

	GtkFileFilter *filter = gtk_file_filter_new();

	int flag_vid = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (chooser), "format_combo"));
	if(flag_vid)
	{
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_dialog),
			setVidExt(basename, index));
		gtk_file_filter_add_pattern(filter, get_vformat_pattern(index));
	}
	else
	{
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_dialog),
			setImgExt(basename, index));
		gtk_file_filter_add_pattern(filter, get_iformat_pattern(index));
	}

	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER (file_dialog), filter);

	g_free(basename);
	g_free(filename);
}

void
file_chooser (GtkWidget * FileButt, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;

	GtkWidget *FileDialog = gtk_file_chooser_dialog_new (_("Save File"),
		GTK_WINDOW(gwidget->mainwin),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (FileDialog), TRUE);

	/** create a file filter */
	GtkFileFilter *filter = gtk_file_filter_new();

	GtkWidget *FBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	GtkWidget *format_label = gtk_label_new(_("File Format:"));
	gtk_widget_set_halign (FBox, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FBox, TRUE);
	gtk_widget_set_hexpand (format_label, FALSE);
	gtk_widget_show(FBox);
	gtk_widget_show(format_label);
	gtk_box_pack_start(GTK_BOX(FBox), format_label, FALSE, FALSE, 2);

	int flag_vid = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (FileButt), "file_butt"));
	if(flag_vid)
	{ 	/* video File chooser*/
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (FileDialog),
			global->vidFPath[1]);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (FileDialog),
			global->vidFPath[0]);

		/** add format file filters*/
		GtkWidget *VidFormat = gtk_combo_box_text_new ();
		gtk_widget_set_halign (VidFormat, GTK_ALIGN_FILL);
		gtk_widget_set_hexpand (VidFormat, TRUE);

		int vformat_ind =0;
		for (vformat_ind =0; vformat_ind<MAX_VFORMATS; vformat_ind++)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(VidFormat),gettext(get_vformat_desc(vformat_ind)));

		gtk_combo_box_set_active(GTK_COMBO_BOX(VidFormat), global->VidFormat);
		gtk_box_pack_start(GTK_BOX(FBox), VidFormat, FALSE, FALSE, 2);
		gtk_widget_show(VidFormat);

		/**add a pattern to the filter*/
		gtk_file_filter_add_pattern(filter, get_vformat_pattern(global->VidFormat));
		gtk_file_chooser_set_filter(GTK_FILE_CHOOSER (FileDialog), filter);

		gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER (FileDialog), FBox);

		g_object_set_data (G_OBJECT (VidFormat), "format_combo", GINT_TO_POINTER(1));
		g_signal_connect (GTK_COMBO_BOX(VidFormat), "changed",
			G_CALLBACK (filename_update_extension), FileDialog);

		if (gtk_dialog_run (GTK_DIALOG (FileDialog)) == GTK_RESPONSE_ACCEPT)
		{
			gchar *fullname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (FileDialog));
			global->vidFPath=splitPath(fullname, global->vidFPath);
			g_free(fullname);

			/*get the file type*/
			global->VidFormat = check_video_type(global->vidFPath[0]);
			/** check for webm and change codecs acordingly */
			if(global->VidFormat == WEBM_FORMAT)
			{
				int vcodec_ind = get_list_vcodec_index(CODEC_ID_VP8);
				int acodec_ind = get_list_acodec_index(CODEC_ID_VORBIS);
				if(vcodec_ind >= 0 && acodec_ind >= 0)
				{
					if(global->AudCodec != acodec_ind)
					{
						fprintf(stderr, "WARN: changing audio codec ind (%i --> %i)\n", global->AudCodec, acodec_ind);
						global->AudCodec = acodec_ind; //this is also set by the gwidget->SndComp calback
						int index = g_slist_length (gwidget->agroup) - (global->AudCodec + 1);
						GtkWidget* codec_item = g_slist_nth_data (gwidget->agroup, index);
						gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(codec_item), TRUE);
						//gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndComp), global->AudCodec);
					}
					if(global->VidCodec != vcodec_ind)
					{
						fprintf(stderr, "WARN: changing video codec ind (%i --> %i)\n", global->VidCodec, vcodec_ind);
						global->VidCodec = vcodec_ind;//this is also set by the gwidget->VidCodec calback
						int index = g_slist_length (gwidget->vgroup) - (global->VidCodec + 1);
						GtkWidget* codec_item = g_slist_nth_data (gwidget->vgroup, index);
						gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(codec_item), TRUE);
						//gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->VidCodec), global->VidCodec);
					}


				}
				else
				{
					fprintf(stderr, "ERROR: can't find webm codecs (VP8 , VORBIS)\n");
					fprintf(stderr, "       using matroska muxer instead\n");
					global->VidFormat = MKV_FORMAT;

					gtk_combo_box_set_active (GTK_COMBO_BOX(VidFormat), global->VidFormat);
				}
			}

			if(global->vid_inc>0)
			{
				uint64_t suffix = get_file_suffix(global->vidFPath[1], global->vidFPath[0]);
				fprintf(stderr, "Video file suffix detected: %" PRIu64 "\n", suffix);
				if(suffix >= G_MAXUINT64)
				{
					global->vidFPath[0] = add_file_suffix(global->vidFPath[0], suffix);
					suffix = 0;
				}
				if(suffix >= 0)
					global->vid_inc = suffix + 1;
			}
		}
	}
	else
	{	/* Image File chooser*/
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (FileDialog),
			global->imgFPath[1]);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (FileDialog),
			global->imgFPath[0]);

		/** add format file filters*/
		GtkWidget *ImgFormat = gtk_combo_box_text_new ();
		gtk_widget_set_halign (ImgFormat, GTK_ALIGN_FILL);
		gtk_widget_set_hexpand (ImgFormat, TRUE);

		int iformat_ind =0;
		for (iformat_ind =0; iformat_ind<MAX_IFORMATS; iformat_ind++)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ImgFormat),gettext(get_iformat_desc(iformat_ind)));

		gtk_combo_box_set_active(GTK_COMBO_BOX(ImgFormat), global->imgFormat);
		gtk_box_pack_start(GTK_BOX(FBox), ImgFormat, FALSE, FALSE, 2);
		gtk_widget_show(ImgFormat);

		/**add a pattern to the filter*/
		gtk_file_filter_add_pattern(filter, get_iformat_pattern(global->imgFormat));
		gtk_file_chooser_set_filter(GTK_FILE_CHOOSER (FileDialog), filter);

		gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER (FileDialog), FBox);

		g_object_set_data (G_OBJECT (ImgFormat), "format_combo", GINT_TO_POINTER(0));
		g_signal_connect (GTK_COMBO_BOX(ImgFormat), "changed",
			G_CALLBACK (filename_update_extension), FileDialog);

		if (gtk_dialog_run (GTK_DIALOG (FileDialog)) == GTK_RESPONSE_ACCEPT)
		{
			gchar *fullname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (FileDialog));
			global->imgFPath=splitPath(fullname, global->imgFPath);
			g_free(fullname);

			/*get the file type*/
			global->imgFormat = check_image_type(global->imgFPath[0]);

			if(global->image_inc>0)
			{
				uint64_t suffix = get_file_suffix(global->imgFPath[1], global->imgFPath[0]);
				fprintf(stderr, "Image file suffix detected: %" PRIu64 "\n", suffix);
				if(suffix >= G_MAXUINT64)
				{
					global->imgFPath[0] = add_file_suffix(global->imgFPath[0], suffix);
					suffix = 0;
				}
				if(suffix >= 0)
					global->image_inc = suffix + 1;
			}

		}
	}

	//GtkFileFilter* current_filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER (FileDialog));
	//if(G_IS_OBJECT(current_filter))
	//	g_object_unref(current_filter);
	gtk_widget_destroy (FileDialog);
}

void
lavc_properties(GtkMenuItem * codec_prop, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;

	int line = 0;
	vcodecs_data *codec_defaults = get_codec_defaults(global->VidCodec);

	if (!(codec_defaults->avcodec)) return;

	GtkWidget *codec_dialog = gtk_dialog_new_with_buttons (_("codec values"),
		GTK_WINDOW(gwidget->mainwin),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_OK,
		GTK_RESPONSE_ACCEPT,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_REJECT,
		NULL);

	GtkWidget *table = gtk_grid_new();

	GtkWidget *lbl_fps = gtk_label_new(_("                              encoder fps:   \n (0 - use fps combobox value)"));
	gtk_misc_set_alignment (GTK_MISC (lbl_fps), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_fps, 0, line, 1, 1);
	gtk_widget_show (lbl_fps);

	GtkWidget *enc_fps = gtk_spin_button_new_with_range(0,30,5);
	gtk_editable_set_editable(GTK_EDITABLE(enc_fps),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(enc_fps), codec_defaults->fps);

	gtk_grid_attach (GTK_GRID(table), enc_fps, 1, line, 1, 1);
	gtk_widget_show (enc_fps);
	line++;

	GtkWidget *monotonic_pts = gtk_check_button_new_with_label (_(" monotonic pts"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(monotonic_pts),(codec_defaults->monotonic_pts != 0));

	gtk_grid_attach (GTK_GRID(table), monotonic_pts, 1, line, 1, 1);
	gtk_widget_show (monotonic_pts);
	line++;

	GtkWidget *lbl_bit_rate = gtk_label_new(_("bit rate:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_bit_rate), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_bit_rate, 0, line, 1, 1);
	gtk_widget_show (lbl_bit_rate);

	GtkWidget *bit_rate = gtk_spin_button_new_with_range(160000,4000000,10000);
	gtk_editable_set_editable(GTK_EDITABLE(bit_rate),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(bit_rate), codec_defaults->bit_rate);

	gtk_grid_attach (GTK_GRID(table), bit_rate, 1, line, 1, 1);
	gtk_widget_show (bit_rate);
	line++;

	GtkWidget *lbl_qmax = gtk_label_new(_("qmax:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_qmax), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_qmax, 0, line, 1 ,1);
	gtk_widget_show (lbl_qmax);

	GtkWidget *qmax = gtk_spin_button_new_with_range(1,60,1);
	gtk_editable_set_editable(GTK_EDITABLE(qmax),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(qmax), codec_defaults->qmax);

	gtk_grid_attach (GTK_GRID(table), qmax, 1, line, 1, 1);
	gtk_widget_show (qmax);
	line++;

	GtkWidget *lbl_qmin = gtk_label_new(_("qmin:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_qmin), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_qmin, 0, line, 1, 1);
	gtk_widget_show (lbl_qmin);

	GtkWidget *qmin = gtk_spin_button_new_with_range(1,31,1);
	gtk_editable_set_editable(GTK_EDITABLE(qmin),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(qmin), codec_defaults->qmin);

	gtk_grid_attach (GTK_GRID(table), qmin, 1, line, 1, 1);
	gtk_widget_show (qmin);
	line++;

	GtkWidget *lbl_max_qdiff = gtk_label_new(_("max. qdiff:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_max_qdiff), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_max_qdiff, 0, line, 1, 1);
	gtk_widget_show (lbl_max_qdiff);

	GtkWidget *max_qdiff = gtk_spin_button_new_with_range(1,4,1);
	gtk_editable_set_editable(GTK_EDITABLE(max_qdiff),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(max_qdiff), codec_defaults->max_qdiff);

	gtk_grid_attach (GTK_GRID(table), max_qdiff, 1, line, 1, 1);
	gtk_widget_show (max_qdiff);
	line++;

	GtkWidget *lbl_dia = gtk_label_new(_("dia size:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_dia), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_dia, 0, line, 1, 1);
	gtk_widget_show (lbl_dia);

	GtkWidget *dia = gtk_spin_button_new_with_range(-1,4,1);
	gtk_editable_set_editable(GTK_EDITABLE(dia),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(dia), codec_defaults->dia);

	gtk_grid_attach (GTK_GRID(table), dia, 1, line, 1, 1);
	gtk_widget_show (dia);
	line++;

	GtkWidget *lbl_pre_dia = gtk_label_new(_("pre dia size:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_pre_dia), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_pre_dia, 0, line, 1, 1);
	gtk_widget_show (lbl_pre_dia);

	GtkWidget *pre_dia = gtk_spin_button_new_with_range(1,4,1);
	gtk_editable_set_editable(GTK_EDITABLE(pre_dia),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(pre_dia), codec_defaults->pre_dia);

	gtk_grid_attach (GTK_GRID(table), pre_dia, 1, line, 1, 1);
	gtk_widget_show (pre_dia);
	line++;

	GtkWidget *lbl_pre_me = gtk_label_new(_("pre me:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_pre_me), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_pre_me, 0, line, 1, 1);
	gtk_widget_show (lbl_pre_me);

	GtkWidget *pre_me = gtk_spin_button_new_with_range(0,2,1);
	gtk_editable_set_editable(GTK_EDITABLE(pre_me),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(pre_me), codec_defaults->pre_me);

	gtk_grid_attach (GTK_GRID(table), pre_me, 1, line, 1, 1);
	gtk_widget_show (pre_me);
	line++;

	GtkWidget *lbl_me_pre_cmp = gtk_label_new(_("pre cmp:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_me_pre_cmp), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_me_pre_cmp, 0, line, 1, 1);
	gtk_widget_show (lbl_me_pre_cmp);

	GtkWidget *me_pre_cmp = gtk_spin_button_new_with_range(0,6,1);
	gtk_editable_set_editable(GTK_EDITABLE(me_pre_cmp),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(me_pre_cmp), codec_defaults->me_pre_cmp);

	gtk_grid_attach (GTK_GRID(table), me_pre_cmp, 1, line, 1, 1);
	gtk_widget_show (me_pre_cmp);
	line++;

	GtkWidget *lbl_me_cmp = gtk_label_new(_("cmp:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_me_cmp), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_me_cmp, 0, line, 1, 1);
	gtk_widget_show (lbl_me_cmp);

	GtkWidget *me_cmp = gtk_spin_button_new_with_range(0,6,1);
	gtk_editable_set_editable(GTK_EDITABLE(me_cmp),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(me_cmp), codec_defaults->me_cmp);

	gtk_grid_attach (GTK_GRID(table), me_cmp, 1, line, 1, 1);
	gtk_widget_show (me_cmp);
	line++;

	GtkWidget *lbl_me_sub_cmp = gtk_label_new(_("sub cmp:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_me_sub_cmp), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_me_sub_cmp, 0, line, 1, 1);
	gtk_widget_show (lbl_me_sub_cmp);

	GtkWidget *me_sub_cmp = gtk_spin_button_new_with_range(0,6,1);
	gtk_editable_set_editable(GTK_EDITABLE(me_sub_cmp),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(me_sub_cmp), codec_defaults->me_sub_cmp);

	gtk_grid_attach (GTK_GRID(table), me_sub_cmp, 1, line, 1, 1);
	gtk_widget_show (me_sub_cmp);
	line++;

	GtkWidget *lbl_last_pred = gtk_label_new(_("last predictor count:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_last_pred), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_last_pred, 0, line, 1, 1);
	gtk_widget_show (lbl_last_pred);

	GtkWidget *last_pred = gtk_spin_button_new_with_range(1,3,1);
	gtk_editable_set_editable(GTK_EDITABLE(last_pred),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(last_pred), codec_defaults->last_pred);

	gtk_grid_attach (GTK_GRID(table), last_pred, 1, line, 1, 1);
	gtk_widget_show (last_pred);
	line++;

	GtkWidget *lbl_gop_size = gtk_label_new(_("gop size:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_gop_size), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_gop_size, 0, line, 1, 1);
	gtk_widget_show (lbl_gop_size);

	GtkWidget *gop_size = gtk_spin_button_new_with_range(1,250,1);
	gtk_editable_set_editable(GTK_EDITABLE(gop_size),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(gop_size), codec_defaults->gop_size);

	gtk_grid_attach (GTK_GRID(table), gop_size, 1, line, 1, 1);
	gtk_widget_show (gop_size);
	line++;

	GtkWidget *lbl_qcompress = gtk_label_new(_("qcompress:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_qcompress), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_qcompress, 0, line, 1, 1);
	gtk_widget_show (lbl_qcompress);

	GtkWidget *qcompress = gtk_spin_button_new_with_range(0,1,0.1);
	gtk_editable_set_editable(GTK_EDITABLE(qcompress),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(qcompress), codec_defaults->qcompress);

	gtk_grid_attach (GTK_GRID(table), qcompress, 1, line, 1 ,1);
	gtk_widget_show (qcompress);
	line++;

	GtkWidget *lbl_qblur = gtk_label_new(_("qblur:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_qblur), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_qblur, 0, line, 1 ,1);
	gtk_widget_show (lbl_qblur);

	GtkWidget *qblur = gtk_spin_button_new_with_range(0,1,0.1);
	gtk_editable_set_editable(GTK_EDITABLE(qblur),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(qblur), codec_defaults->qblur);

	gtk_grid_attach (GTK_GRID(table), qblur, 1, line, 1 ,1);
	gtk_widget_show (qblur);
	line++;

	GtkWidget *lbl_subq = gtk_label_new(_("subq:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_subq), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_subq, 0, line, 1 ,1);
	gtk_widget_show (lbl_subq);

	GtkWidget *subq = gtk_spin_button_new_with_range(0,8,1);
	gtk_editable_set_editable(GTK_EDITABLE(subq),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(subq), codec_defaults->subq);

	gtk_grid_attach (GTK_GRID(table), subq, 1, line, 1 ,1);
	gtk_widget_show (subq);
	line++;

	GtkWidget *lbl_framerefs = gtk_label_new(_("framerefs:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_framerefs), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_framerefs, 0, line, 1 ,1);
	gtk_widget_show (lbl_framerefs);

	GtkWidget *framerefs = gtk_spin_button_new_with_range(0,12,1);
	gtk_editable_set_editable(GTK_EDITABLE(framerefs),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(framerefs), codec_defaults->framerefs);

	gtk_grid_attach (GTK_GRID(table), framerefs, 1, line, 1 ,1);
	gtk_widget_show (framerefs);
	line++;

	GtkWidget *lbl_me_method = gtk_label_new(_("me method:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_me_method), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_me_method, 0, line, 1 ,1);
	gtk_widget_show (lbl_me_method);

	GtkWidget *me_method = gtk_spin_button_new_with_range(1,10,1);
	gtk_editable_set_editable(GTK_EDITABLE(me_method),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(me_method), codec_defaults->me_method);

	gtk_grid_attach (GTK_GRID(table), me_method, 1, line, 1 ,1);
	gtk_widget_show (me_method);
	line++;

	GtkWidget *lbl_mb_decision = gtk_label_new(_("mb decision:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_mb_decision), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_mb_decision, 0, line, 1 ,1);
	gtk_widget_show (lbl_mb_decision);

	GtkWidget *mb_decision = gtk_spin_button_new_with_range(0,2,1);
	gtk_editable_set_editable(GTK_EDITABLE(mb_decision),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(mb_decision), codec_defaults->mb_decision);

	gtk_grid_attach (GTK_GRID(table), mb_decision, 1, line, 1 ,1);
	gtk_widget_show (mb_decision);
	line++;

	GtkWidget *lbl_max_b_frames = gtk_label_new(_("max B frames:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_max_b_frames), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_max_b_frames, 0, line, 1 ,1);
	gtk_widget_show (lbl_max_b_frames);

	GtkWidget *max_b_frames = gtk_spin_button_new_with_range(0,4,1);
	gtk_editable_set_editable(GTK_EDITABLE(max_b_frames),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(max_b_frames), codec_defaults->max_b_frames);

	gtk_grid_attach (GTK_GRID(table), max_b_frames, 1, line, 1 ,1);
	gtk_widget_show (max_b_frames);
	line++;

	GtkWidget *lbl_num_threads = gtk_label_new(_("num threads:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_num_threads), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_num_threads, 0, line, 1 ,1);
	gtk_widget_show (lbl_num_threads);

	GtkWidget *num_threads = gtk_spin_button_new_with_range(0,8,1);
	gtk_editable_set_editable(GTK_EDITABLE(num_threads),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(num_threads), codec_defaults->num_threads);

	gtk_grid_attach (GTK_GRID(table), num_threads, 1, line, 1 ,1);
	gtk_widget_show (num_threads);
	line++;

	GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (codec_dialog));
	gtk_container_add (GTK_CONTAINER (content_area), table);
	gtk_widget_show (table);

	gint result = gtk_dialog_run (GTK_DIALOG (codec_dialog));
	switch (result)
	{
		case GTK_RESPONSE_ACCEPT:
			codec_defaults->fps = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(enc_fps));
			codec_defaults->monotonic_pts = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(monotonic_pts));
			codec_defaults->bit_rate = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(bit_rate));
			codec_defaults->qmax = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(qmax));
			codec_defaults->qmin = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(qmin));
			codec_defaults->max_qdiff = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(max_qdiff));
			codec_defaults->dia = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(dia));
			codec_defaults->pre_dia = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(pre_dia));
			codec_defaults->pre_me = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(pre_me));
			codec_defaults->me_pre_cmp = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(me_pre_cmp));
			codec_defaults->me_cmp = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(me_cmp));
			codec_defaults->me_sub_cmp = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(me_sub_cmp));
			codec_defaults->last_pred = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(last_pred));
			codec_defaults->gop_size = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(gop_size));
			codec_defaults->qcompress = (float) gtk_spin_button_get_value (GTK_SPIN_BUTTON(qcompress));
			codec_defaults->qblur = (float) gtk_spin_button_get_value (GTK_SPIN_BUTTON(qblur));
			codec_defaults->subq = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(subq));
			codec_defaults->framerefs = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(framerefs));
			codec_defaults->me_method = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(me_method));
			codec_defaults->mb_decision = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(mb_decision));
			codec_defaults->max_b_frames = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(max_b_frames));
			codec_defaults->num_threads = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(num_threads));
			break;
		default:
			// do nothing since dialog was cancelled
			break;
	}
	gtk_widget_destroy (codec_dialog);

}

void
lavc_audio_properties(GtkMenuItem * codec_prop, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;

	int line = 0;
	acodecs_data *codec_defaults = get_aud_codec_defaults(global->AudCodec);

	if (!(codec_defaults->avcodec)) return;

	GtkWidget *codec_dialog = gtk_dialog_new_with_buttons (_("audio codec values"),
		GTK_WINDOW(gwidget->mainwin),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_OK,
		GTK_RESPONSE_ACCEPT,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_REJECT,
		NULL);

	GtkWidget *table = gtk_grid_new();
	gtk_grid_set_column_homogeneous (GTK_GRID(table), TRUE);

	/*bit rate*/
	GtkWidget *lbl_bit_rate = gtk_label_new(_("bit rate:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_bit_rate), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_bit_rate, 0, line, 1, 1);
	gtk_widget_show (lbl_bit_rate);

	GtkWidget *bit_rate = gtk_spin_button_new_with_range(48000,384000,8000);
	gtk_editable_set_editable(GTK_EDITABLE(bit_rate),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(bit_rate), codec_defaults->bit_rate);

	gtk_grid_attach (GTK_GRID(table), bit_rate, 1, line, 1, 1);
	gtk_widget_show (bit_rate);
	line++;

	/*sample format*/
	GtkWidget *lbl_sample_fmt = gtk_label_new(_("sample format:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_sample_fmt), 1, 0.5);
	gtk_grid_attach (GTK_GRID(table), lbl_sample_fmt, 0, line, 1, 1);
	gtk_widget_show (lbl_sample_fmt);

	GtkWidget *sample_fmt = gtk_spin_button_new_with_range(0, AV_SAMPLE_FMT_NB, 1);
	gtk_editable_set_editable(GTK_EDITABLE(sample_fmt),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(sample_fmt), codec_defaults->sample_format);

	gtk_grid_attach (GTK_GRID(table), sample_fmt, 1, line, 1, 1);
	gtk_widget_show (sample_fmt);
	line++;

	GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (codec_dialog));
	gtk_container_add (GTK_CONTAINER (content_area), table);
	gtk_widget_show (table);

	gint result = gtk_dialog_run (GTK_DIALOG (codec_dialog));
	switch (result)
	{
		case GTK_RESPONSE_ACCEPT:
			codec_defaults->bit_rate = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(bit_rate));
			codec_defaults->sample_format = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(sample_fmt));
			break;
		default:
			// do nothing since dialog was cancelled
			break;
	}
	gtk_widget_destroy (codec_dialog);

}

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
/*image controls*/
void
set_sensitive_img_contrls (const int flag, struct GWIDGET *gwidget)
{
	gtk_widget_set_sensitive(gwidget->menu_photo_top, flag);/*image menu entry*/
}

/* sound controls*/
void
set_sensitive_snd_contrls (const int flag, struct GWIDGET *gwidget)
{
	gtk_widget_set_sensitive (gwidget->SndAPI, flag);
	gtk_widget_set_sensitive (gwidget->SndSampleRate, flag);
	gtk_widget_set_sensitive (gwidget->SndDevice, flag);
	gtk_widget_set_sensitive (gwidget->SndNumChan, flag);
}

/*video controls*/
void
set_sensitive_vid_contrls (const int flag, const int sndEnable, struct GWIDGET *gwidget)
{
	/* sound and video compression controls */
	gtk_widget_set_sensitive(gwidget->menu_video_top, flag);/*video menu entry*/
	gtk_widget_set_sensitive (gwidget->SndEnable, flag);
	/* resolution and input format combos   */
	gtk_widget_set_sensitive (gwidget->Resolution, flag);
	gtk_widget_set_sensitive (gwidget->InpType, flag);
	gtk_widget_set_sensitive (gwidget->FrameRate, flag);

	if(sndEnable > 0)
	{
		set_sensitive_snd_contrls(flag, gwidget);
	}

	gwidget->vid_widget_state = flag;
}

gboolean
key_pressed (GtkWidget *win, GdkEventKey *event, struct ALL_DATA *all_data)
{
    struct VidState *s = all_data->s;
    struct GWIDGET *gwidget = all_data->gwidget;
    struct vdIn *videoIn = all_data->videoIn;
    //struct GLOBAL *global = all_data->global;
    /* If we have modifiers, and either Ctrl, Mod1 (Alt), or any
     * of Mod3 to Mod5 (Mod2 is num-lock...) are pressed, we
     * let Gtk+ handle the key */
    //printf("camera key pressed (key:%i)\n", event->keyval);
    if (event->state != 0
            && ((event->state & GDK_CONTROL_MASK)
            || (event->state & GDK_MOD1_MASK)
            || (event->state & GDK_MOD3_MASK)
            || (event->state & GDK_MOD4_MASK)
            || (event->state & GDK_MOD5_MASK)))
        return FALSE;

    if(videoIn->PanTilt)
    {
        switch (event->keyval)
        {
            case GDK_KEY_Down:
            case GDK_KEY_KP_Down:
                /*Tilt Down*/
                uvcPanTilt (videoIn->fd, s->control_list, 0, 1);
                return TRUE;

            case GDK_KEY_Up:
            case GDK_KEY_KP_Up:
                /*Tilt UP*/
                uvcPanTilt (videoIn->fd, s->control_list, 0, -1);
                return TRUE;

            case GDK_KEY_Left:
            case GDK_KEY_KP_Left:
                /*Pan Left*/
                uvcPanTilt (videoIn->fd, s->control_list, 1, 1);
                return TRUE;

            case GDK_KEY_Right:
            case GDK_KEY_KP_Right:
                /*Pan Right*/
                uvcPanTilt (videoIn->fd, s->control_list, 1, -1);
                return TRUE;

            default:
                break;
        }
    }

    switch (event->keyval)
    {
        case GDK_KEY_WebCam:

			/* camera button pressed - trigger image capture*/
			if (all_data->global->default_action == 0) {
				gtk_button_clicked (GTK_BUTTON(gwidget->CapImageButt));
			} else {
				gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON(gwidget->CapVidButt));
			}
			return TRUE;

        case GDK_KEY_V:
        case GDK_KEY_v:
			gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON(gwidget->CapVidButt));
			return TRUE;

		case GDK_KEY_I:
		case GDK_KEY_i:
			gtk_button_clicked (GTK_BUTTON(gwidget->CapImageButt));
			return TRUE;

    }

    return FALSE;
}


/*----------------------------- Callbacks ------------------------------------*/
/*slider controls callback*/
void
slider_changed (GtkRange * range, struct ALL_DATA *all_data)
{
    struct VidState *s = all_data->s;
    //struct GLOBAL *global = all_data->global;
    struct vdIn *videoIn = all_data->videoIn;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (range), "control_info"));
    Control *c = get_ctrl_by_id(s->control_list, id);

    int val = (int) gtk_range_get_value (range);

    c->value = val;

    set_ctrl(videoIn->fd, s->control_list, id);

    //update spin
    if(c->spinbutton)
    {
        //disable widget signals
        g_signal_handlers_block_by_func(GTK_SPIN_BUTTON(c->spinbutton),
            G_CALLBACK (spin_changed), all_data);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON(c->spinbutton), c->value);
        //enable widget signals
        g_signal_handlers_unblock_by_func(GTK_SPIN_BUTTON(c->spinbutton),
            G_CALLBACK (spin_changed), all_data);
    }

	s = NULL;
	//global = NULL;
	videoIn = NULL;
}

/*spin controls callback*/
void
spin_changed (GtkSpinButton * spin, struct ALL_DATA *all_data)
{
	struct VidState *s = all_data->s;
	//struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;

	int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (spin), "control_info"));
	Control *c = get_ctrl_by_id(s->control_list, id);

	int val = gtk_spin_button_get_value_as_int (spin);
    c->value = val;

    set_ctrl(videoIn->fd, s->control_list, id);

    if(c->widget)
    {
        //disable widget signals
        g_signal_handlers_block_by_func(GTK_SCALE (c->widget),
            G_CALLBACK (slider_changed), all_data);
        gtk_range_set_value (GTK_RANGE (c->widget), c->value);
        //enable widget signals
        g_signal_handlers_unblock_by_func(GTK_SCALE (c->widget),
            G_CALLBACK (slider_changed), all_data);
    }

	s = NULL;
	//global = NULL;
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
    struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	struct focusData *AFdata = all_data->AFdata;
	struct vdIn *videoIn = all_data->videoIn;

	//int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (toggle), "control_info"));
	Control *c = get_ctrl_by_id(s->control_list, AFdata->id);

	int val = gtk_toggle_button_get_active (toggle) ? 1 : 0;

	/*if autofocus disable manual focus control*/
	gtk_widget_set_sensitive (c->widget, !val);
	gtk_widget_set_sensitive (c->spinbutton, !val);

	/*reset flag*/
	AFdata->flag = 0;
	AFdata->ind = 0;
	AFdata->focus = -1; /*reset focus*/
	AFdata->right = AFdata->f_max;
	AFdata->left = AFdata->i_step;
	/*set focus to first value if autofocus enabled*/
	if (val>0)
	{
	    c->value = AFdata->focus;
	    set_ctrl(videoIn->fd, s->control_list, AFdata->id);
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
   //struct GLOBAL *global = all_data->global;
    struct vdIn *videoIn = all_data->videoIn;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (toggle), "control_info"));
    Control *c = get_ctrl_by_id(s->control_list, id);

    int val = gtk_toggle_button_get_active (toggle) ? 1 : 0;

    c->value = val;

    set_ctrl(videoIn->fd, s->control_list, id);

    if(id == V4L2_CID_DISABLE_PROCESSING_LOGITECH)
    {
        if (c->value > 0) videoIn->isbayer=1;
        else videoIn->isbayer=0;

        //restart stream by changing fps
        videoIn->setFPS = 1;
    }

    s = NULL;
    //global = NULL;
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

/*combobox controls callback*/
void
combo_changed (GtkComboBox * combo, struct ALL_DATA *all_data)
{
	struct VidState *s = all_data->s;
	struct vdIn *videoIn = all_data->videoIn;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (combo), "control_info"));
    Control *c = get_ctrl_by_id(s->control_list, id);

    int index = gtk_combo_box_get_active (combo);
    c->value = c->menu[index].index;

    set_ctrl(videoIn->fd, s->control_list, id);

	s = NULL;
	videoIn = NULL;
}

/* generic button control */
void
button_clicked (GtkButton * Button, struct ALL_DATA *all_data)
{
    struct VidState *s = all_data->s;
	struct vdIn *videoIn = all_data->videoIn;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));
    Control *c = get_ctrl_by_id(s->control_list, id);

	switch(c->control.type)
	{
#ifdef V4L2_CTRL_TYPE_STRING
		case V4L2_CTRL_TYPE_STRING:
			strncpy(c->string, gtk_entry_get_text(c->widget), c->control.maximum);
			break;
#endif
#ifdef V4L2_CTRL_TYPE_INTEGER64
		case V4L2_CTRL_TYPE_INTEGER64:
			{
				char* text_input = g_strdup(gtk_entry_get_text(c->widget));
				text_input = g_strstrip(text_input);
				if( g_str_has_prefix(text_input,"0x")) //hex format
				{
					text_input = g_strcanon(text_input,"0123456789ABCDEFabcdef", '');
					c->value64 = g_ascii_strtoll(text_input, NULL, 16);
				}
				else //decimal or hex ?
				{
					text_input = g_strcanon(text_input,"0123456789ABCDEFabcdef", '');
					c->value64 = g_ascii_strtoll(text_input, NULL, 0);
				}
				g_free(text_input);
				text_input = g_strdup_printf("0x%" PRIx64 "", c->value64); //print in hex
				gtk_entry_set_text (c->widget, text_input);
				g_free(text_input);
			}
			break;
#endif
#ifdef V4L2_CTRL_TYPE_BITMASK
		case V4L2_CTRL_TYPE_BITMASK:
			{
				char* text_input = g_strdup(gtk_entry_get_text(c->widget));
				text_input = g_strcanon(text_input,"0123456789ABCDEFabcdef", '');
				c->value = (int32_t) g_ascii_strtoll(text_input, NULL, 16);
				g_free(text_input);
				text_input = g_strdup_printf("0x%x", c->value);
				gtk_entry_set_text(c->widget, text_input);
				g_free(text_input);
			}
			break;
#endif
		default: //button
			c->value = 1;
			break;
	}

    set_ctrl(videoIn->fd, s->control_list, id);
}

/* Pan Tilt button 1 control */
void
button_PanTilt1_clicked (GtkButton * Button, struct ALL_DATA *all_data)
{
    struct VidState *s = all_data->s;
	struct vdIn *videoIn = all_data->videoIn;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));
    Control *c = get_ctrl_by_id(s->control_list, id);

    int val = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(c->spinbutton));
    c->value = val;
    set_ctrl(videoIn->fd, s->control_list, id);
}

/* Pan Tilt button 2 control */
void
button_PanTilt2_clicked (GtkButton * Button, struct ALL_DATA *all_data)
{
    struct VidState *s = all_data->s;
	struct vdIn *videoIn = all_data->videoIn;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));

    if(id == V4L2_CID_PAN_RELATIVE)
    {
    	Control *c = get_ctrl_by_id(s->control_list, V4L2_CID_TILT_RELATIVE);
    	c->value = 0;
    	set_ctrl(videoIn->fd, s->control_list, V4L2_CID_TILT_RELATIVE);
    }
    else
    {
    	Control *c = get_ctrl_by_id(s->control_list, V4L2_CID_PAN_RELATIVE);
    	c->value = 0;
    	set_ctrl(videoIn->fd, s->control_list, V4L2_CID_PAN_RELATIVE);
    }

    Control *c = get_ctrl_by_id(s->control_list, id);

    int val = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(c->spinbutton));
    c->value = -val;
    set_ctrl(videoIn->fd, s->control_list, id);
}

/* set focus (for focus motor cameras ex: Logitech Orbit/Sphere and 9000 pro) */
void
setfocus_clicked (GtkButton * FocusButton, struct ALL_DATA *all_data)
{
	struct focusData *AFdata = all_data->AFdata;
	struct vdIn *videoIn = all_data->videoIn;
    struct VidState *s = all_data->s;

	AFdata->setFocus = 1;
	AFdata->ind = 0;
	AFdata->flag = 0;
	AFdata->right = 255;
	AFdata->left = 8;
	AFdata->focus = -1; /*reset focus*/

    Control *c = get_ctrl_by_id(s->control_list, AFdata->id);
	c->value = AFdata->focus;

    set_ctrl(videoIn->fd, s->control_list, AFdata->id);

	AFdata = NULL;
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

	GtkWidget * content_area = gtk_dialog_get_content_area (GTK_DIALOG (gwidget->restartdialog));
	GtkWidget *message = gtk_label_new (_("launch new process or restart?.\n\n"));
	gtk_container_add (GTK_CONTAINER (content_area), message);
	gtk_widget_show_all(gwidget->restartdialog);

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

	__LOCK_MUTEX(__VMUTEX);
		gboolean capVid = videoIn->capVid;
	__UNLOCK_MUTEX(__VMUTEX);
	/*disable fps combobox signals*/
	g_signal_handlers_block_by_func(GTK_COMBO_BOX_TEXT(gwidget->FrameRate), G_CALLBACK (FrameRate_changed), all_data);
	/* clear out the old fps list... */
	GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX(gwidget->FrameRate)));
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
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->FrameRate),temp_str);

		if (( global->fps_num == listVidCap->framerate_num[i]) &&
			(global->fps == listVidCap->framerate_denom[i]))
				deffps=i;//set selected
	}

	/*set default fps in combo*/
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->FrameRate),deffps);

	/*enable fps combobox signals*/
	g_signal_handlers_unblock_by_func(GTK_COMBO_BOX_TEXT(gwidget->FrameRate), G_CALLBACK (FrameRate_changed), all_data);

	if (listVidCap->framerate_num)
		global->fps_num = listVidCap->framerate_num[deffps];

	if (listVidCap->framerate_denom)
		global->fps = listVidCap->framerate_denom[deffps];

	if(capVid)
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
	g_signal_handlers_block_by_func(GTK_COMBO_BOX_TEXT(gwidget->Resolution), G_CALLBACK (resolution_changed), all_data);

	/* clear out the old resolution list... */
	GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX(gwidget->Resolution)));
	gtk_list_store_clear(store);

	videoIn->listFormats->current_format = index;
	listVidFormats = &videoIn->listFormats->listVidFormats[index];

	format = videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].format;
	get_PixMode(format, global->mode);

	printf("redraw resolution combo for format (%x)\n",format);
	/*redraw resolution combo for new format*/
	printf("numb res = %d\n", listVidFormats->numb_res);
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


	global->height = listVidFormats->listVidCap[defres].height;
	global->width = listVidFormats->listVidCap[defres].width;
	global->format = format;

	/*enable resolution combobox signals*/
	g_signal_handlers_unblock_by_func(GTK_COMBO_BOX_TEXT(gwidget->Resolution), G_CALLBACK (resolution_changed), all_data);

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

	global->fps=videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[resind].framerate_denom[index];
	global->fps_num=videoIn->listFormats->listVidFormats[videoIn->listFormats->current_format].listVidCap[resind].framerate_num[index];

	/*fps change is done in two fases:
	 * 1- in video.c we try to change device fps (and set flag to 2)
	 * 2- in v4l2uvc.c (uvcGrab) we query and queue the buffers if using MMAP (and reset flag to 0)*/
	videoIn->setFPS=1;

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

/*Audio API control callback*/
void
SndAPI_changed (GtkComboBox * SoundAPI, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct paRecordData *pdata = all_data->pdata;
	//struct GWIDGET *gwidget = all_data->gwidget;

	global->Sound_API=gtk_combo_box_get_active (SoundAPI);
	pdata->api = global->Sound_API;

	update_snd_devices(all_data);

	g_print("using audio API n:%d\n",global->Sound_API);
	global = NULL;
}

/*sound device control callback*/
void
SndDevice_changed (GtkComboBox * SoundDevice, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;

	global->Sound_UseDev=gtk_combo_box_get_active (SoundDevice);
	g_print("using device id:%d\n",global->Sound_IndexDev[global->Sound_UseDev].id);
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

/*audio compression control callback*/
void
AudCodec_menu_changed (GtkRadioMenuItem *acodec_item, struct ALL_DATA *all_data)
{

	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(acodec_item)))
	{
		/**GSList indexes (g_slist_index) are in reverse order:
		 * last inserted has index 0
		 * so count backwards
		 */
		int num_acodecs = g_slist_length(gwidget->agroup);
		int index = g_slist_index (gwidget->agroup, acodec_item);
		index = num_acodecs - (index + 1); //reverse order and 0 indexed
		fprintf(stderr,"DEBUG: audio codec changed to %i\n", index);

		global->AudCodec = index;
	}

	if( global->VidFormat == WEBM_FORMAT &&
		get_acodec_id(global->AudCodec) != CODEC_ID_VORBIS)
	{
		//change VidFormat to Matroska
		fprintf(stderr, "WARN: webm can only use VORBIS audio codec \n");
		fprintf(stderr, "      using matroska muxer instead\n");
		global->VidFormat = MKV_FORMAT;
		//FIXME: change file extension if needed
	}

	global->Sound_Format = get_aud4cc(global->AudCodec);

	global = NULL;
}

/*video compression control callback*/
void
VidCodec_menu_changed (GtkRadioMenuItem *vcodec_item, struct ALL_DATA *all_data)
{

	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(vcodec_item)))
	{
		/**GSList indexes (g_slist_index) are in reverse order:
		 * last inserted has index 0
		 * so count backwards
		 */
		int num_vcodecs = g_slist_length(gwidget->vgroup);
		int index = g_slist_index (gwidget->vgroup, vcodec_item);
		index = num_vcodecs - (index + 1); //reverse order and 0 indexed
		fprintf(stderr,"DEBUG: video codec changed to %i\n", index);

		global->VidCodec = index;
		global->VidCodec_ID = get_vcodec_id(global->VidCodec);
	}

	if( global->VidFormat == WEBM_FORMAT &&
		global->VidCodec_ID != CODEC_ID_VP8)
	{
		//change VidFormat to Matroska
		fprintf(stderr, "WARN: webm can only use VP8 video codec (0x%x != 0x%x)\n", global->VidCodec, CODEC_ID_VP8);
		fprintf(stderr, "      using matroska muxer instead\n");
		global->VidFormat = MKV_FORMAT;
		//FIXME: change file extension if needed
	}

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
		if(global->debug) g_print("disabling sound.\n");
		set_sensitive_snd_contrls(FALSE, gwidget);
	}
	else
	{
		if(global->debug) g_print("enabling sound.\n");
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
	__LOCK_MUTEX(__GMUTEX);
		global->Frame_Flags = gtk_toggle_button_get_active (toggle) ?
			(global->Frame_Flags | filter) :
			(global->Frame_Flags & ~(filter));
	__UNLOCK_MUTEX(__GMUTEX);

	global = NULL;
}

/* Audio effect checkbox callback*/
void
EffEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct paRecordData *pdata = all_data->pdata;
	int effect = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (toggle), "effect_info"));
	__LOCK_MUTEX(__AMUTEX);
		pdata->snd_Flags = gtk_toggle_button_get_active (toggle) ?
			(pdata->snd_Flags | effect) :
			(pdata->snd_Flags & ~(effect));
	__UNLOCK_MUTEX(__AMUTEX);
	pdata = NULL;
}

void osdChanged(GtkToggleButton * toggle, struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
  	int flag = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (toggle), "flag"));
  	global->osdFlags = gtk_toggle_button_get_active(toggle) ?
		(global->osdFlags | flag) :
    	(global->osdFlags & ~(flag));
}

void
image_prefix_toggled(GtkWidget * toggle, struct ALL_DATA *all_data)
{
	//struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;

	global->image_inc = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(toggle)) ? 1 : 0;

	if(global->image_inc > 0)
	{
		uint64_t suffix = get_file_suffix(global->imgFPath[1], global->imgFPath[0]);
		fprintf(stderr, "Image file suffix detected: %" PRIu64 "\n", suffix);
		if(suffix >= G_MAXUINT64)
		{
			global->imgFPath[0] = add_file_suffix(global->imgFPath[0], suffix);
			suffix = 0;
		}
		if(suffix >= 0)
			global->image_inc = suffix + 1;
	}

	global = NULL;

}

void
video_prefix_toggled(GtkWidget * toggle, struct ALL_DATA *all_data)
{
	//struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;

	global->vid_inc = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(toggle)) ? 1 : 0;

	if(global->vid_inc > 0)
	{
		uint64_t suffix = get_file_suffix(global->vidFPath[1], global->vidFPath[0]);
		fprintf(stderr, "Video file suffix detected: %" PRIu64 "\n", suffix);
		if(suffix >= G_MAXUINT64)
		{
			global->vidFPath[0] = add_file_suffix(global->vidFPath[0], suffix);
			suffix = 0;
		}
		if(suffix >= 0)
			global->vid_inc = suffix + 1;
	}

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

	/** a previous image hasn't been captured yet
	 *  no use flaging it again so return
	 */
	if(videoIn->capImage)
		return;

	if ((global->image_timer == 0) && (global->image_inc>0))
	{
		videoIn->ImageFName = incFilename(videoIn->ImageFName,
			global->imgFPath,
			global->image_inc);

		global->image_inc++;
	}
	else
	{
		videoIn->ImageFName = joinPath(videoIn->ImageFName, global->imgFPath);
	}

	if(global->image_timer > 0)
	{
		/*auto capture on -> stop it*/
		if (global->image_timer_id > 0) g_source_remove(global->image_timer_id);
		global->image_timer=0;

        if(!global->no_display)
        {
            gtk_button_set_label(GTK_BUTTON(gwidget->CapImageButt),_("Cap. Image (I)"));
            set_sensitive_img_contrls(TRUE, gwidget);/*enable image controls*/
        }
	}
	else
	{
		videoIn->capImage = TRUE;
	}

	if(!global->no_display)
    {
		char *message = g_strjoin(" ", _("capturing photo to"), videoIn->ImageFName, NULL);
		printf("status message: %s\n", message);
		gtk_statusbar_pop (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id);
		gtk_statusbar_push (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id, message);
		g_free(message);
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

	__LOCK_MUTEX(__VMUTEX);
		gboolean capVid = videoIn->capVid;
	__UNLOCK_MUTEX(__VMUTEX);

    if(!global->no_display)
    {

        /* disable signals for this callback */
        g_signal_handlers_block_by_func(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), G_CALLBACK (capture_vid), all_data);
        /* widgets are enable/disable in create_video.c */
    }
	if(global->debug) g_print("Cap Video toggled: %d\n", !capVid);

	if(capVid) /* we are capturing */
	{	/****************** Stop Video ************************/
		capVid = FALSE;
		__LOCK_MUTEX(__VMUTEX);
			videoIn->capVid = capVid;
		__UNLOCK_MUTEX(__VMUTEX);
		__LOCK_MUTEX(__AMUTEX);
			pdata->capVid = capVid;
		__UNLOCK_MUTEX(__AMUTEX);
		/*join IO thread*/
		if (global->debug) g_print("Shuting Down IO Thread\n");
		__THREAD_JOIN( all_data->IO_thread );
		if (global->debug) g_print("IO Thread finished\n");

        if(!global->no_display)
        {
            if(global->debug) g_print("enabling controls\n");
            /*enabling sound and video compression controls*/
            set_sensitive_vid_contrls(TRUE, global->Sound_enable, gwidget);

            gtk_button_set_label(GTK_BUTTON(gwidget->CapVidButt),_("Cap. Video (V)"));
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
        }

		if(global->disk_timer_id) g_source_remove(global->disk_timer_id);
		global->disk_timer_id = 0;
	}
	else if(!capVid) /*we are not capturing*/
	{	/******************** Start Video *********************/

		capVid = TRUE;

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

		if(!global->no_display)
		{
			char * message = g_strjoin(" ", _("capturing video to"), videoIn->VidFName, NULL);
			gtk_statusbar_pop (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id);
			gtk_statusbar_push (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id, message);
			g_free(message);
		}

		/* check if enough free space is available on disk*/
		if(!DiskSupervisor(all_data))
		{
			g_print("Cap Video failed\n");
			capVid = FALSE;
		}
		else
		{
			/*start disk check timed callback (every 10 sec)*/
			if (!global->disk_timer_id)
				global->disk_timer_id=g_timeout_add(10*1000, FreeDiskCheck_timer, all_data);

			/*disabling sound and video compression controls*/
            if(!global->no_display)
                set_sensitive_vid_contrls(FALSE, global->Sound_enable, gwidget);

			//request a IDR frame with SPS and PPS data
			uvcx_request_frame_type(videoIn->fd, global->uvc_h264_unit, PICTURE_TYPE_IDR_FULL);
			/*start IO thread*/
			if( __THREAD_CREATE(&all_data->IO_thread, IO_loop, (void *) all_data))
			{
				g_printerr("Thread creation failed\n");
				capVid = FALSE;
			}
		}

        if(!global->no_display)
        {
            if(capVid)
            {
                gtk_button_set_label(GTK_BUTTON(gwidget->CapVidButt),_("Stop Video (V)"));
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), TRUE);
            }
            else
            {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
            }
        }
	}

	/*enable signals for this callback*/
    if(!global->no_display)
        g_signal_handlers_unblock_by_func(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), G_CALLBACK (capture_vid), all_data);

	gwidget = NULL;
	pdata = NULL;
	global = NULL;
	videoIn = NULL;
}

void
camera_button_menu_changed (GtkWidget *item, struct ALL_DATA *all_data)
{
	int flag = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (item), "camera_default"));

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
	{
		all_data->global->default_action = flag; //0-image; 1-video
	}
}

/*--------------------- buttons callbacks ------------------*/
void
Defaults_clicked (GtkWidget *item, struct ALL_DATA *all_data)
{
    struct VidState *s = all_data->s;
    struct vdIn *videoIn = all_data->videoIn;

    set_default_values(videoIn->fd, s->control_list, s->num_controls, all_data);
}

void
Profile_clicked (GtkWidget *item, struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	//struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	//struct vdIn *videoIn = all_data->videoIn;

	GtkWidget *FileDialog;

	int save = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (item), "profile_dialog"));
	if(global->debug) g_print("Profile dialog (%d)\n", save);
	if (save > 0)
	{
		FileDialog = gtk_file_chooser_dialog_new (_("Save Profile"),
			GTK_WINDOW(gwidget->mainwin),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
		gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (FileDialog), TRUE);

		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (FileDialog),
			global->profile_FPath[0]);
	}
	else
	{
		FileDialog = gtk_file_chooser_dialog_new (_("Load Profile"),
			GTK_WINDOW(gwidget->mainwin),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
	}

	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (FileDialog),
		global->profile_FPath[1]);

	if (gtk_dialog_run (GTK_DIALOG (FileDialog)) == GTK_RESPONSE_ACCEPT)
	{
		/*Save Controls Data*/
		char *filename= gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (FileDialog));
		global->profile_FPath=splitPath(filename,global->profile_FPath);

		if(save > 0)
			SaveControls(all_data);
		else
			LoadControls(all_data);
	}
	gtk_widget_destroy (FileDialog);
	gwidget = NULL;
	//s = NULL;
	global = NULL;
	//videoIn = NULL;
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

gboolean
image_capture_callback (gpointer data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GWIDGET *gwidget = all_data->gwidget;

	gtk_button_clicked (GTK_BUTTON(gwidget->CapImageButt));

	return FALSE;
}

gboolean
video_capture_callback (gpointer data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GWIDGET *gwidget = all_data->gwidget;

	gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON(gwidget->CapVidButt));

	return FALSE;
}

void
quitButton_clicked (GtkButton * quitButton, struct ALL_DATA *all_data)
{
	shutd(0, all_data);//shutDown
}



