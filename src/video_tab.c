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

	int flag_vid = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (FileButt), "file_butt"));
	if(flag_vid) 
	{ /* video File chooser*/
		const gchar *basename =  gtk_entry_get_text(GTK_ENTRY(gwidget->VidFNameEntry));
		
		global->vidFPath=splitPath((gchar *) basename, global->vidFPath);
	
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (gwidget->FileDialog), 
			global->vidFPath[1]);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (gwidget->FileDialog),
			global->vidFPath[0]);

		if (gtk_dialog_run (GTK_DIALOG (gwidget->FileDialog)) == GTK_RESPONSE_ACCEPT)
		{
			gchar *fullname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (gwidget->FileDialog));
			global->vidFPath=splitPath(fullname, global->vidFPath);
			g_free(fullname);
			gtk_entry_set_text(GTK_ENTRY(gwidget->VidFNameEntry)," ");
			gtk_entry_set_text(GTK_ENTRY(gwidget->VidFNameEntry),global->vidFPath[0]);
			/*get the file type*/
			global->VidFormat = check_video_type(global->vidFPath[0]);
			/*set the file type*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->VidFormat),global->VidFormat);

			if(global->vid_inc>0)
			{ 
				global->vid_inc=1; /*if auto naming restart counter*/
				g_snprintf(global->vidinc_str,24,_("File num:%d"),global->vid_inc);
				gtk_label_set_text(GTK_LABEL(gwidget->VidIncLabel), global->vidinc_str);
			}
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

static void 
lavc_properties(GtkButton * CodecButt, struct ALL_DATA *all_data)
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
	
	GtkWidget *table = gtk_table_new(1,2,FALSE);

	GtkWidget *lbl_fps = gtk_label_new(_("                              encoder fps:   \n (0 - use fps combobox value)"));
	gtk_misc_set_alignment (GTK_MISC (lbl_fps), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_fps, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_fps);
	
	GtkWidget *enc_fps = gtk_spin_button_new_with_range(0,30,5);
	gtk_editable_set_editable(GTK_EDITABLE(enc_fps),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(enc_fps), codec_defaults->fps);
	
	gtk_table_attach (GTK_TABLE(table), enc_fps, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (enc_fps);
	line++;

	GtkWidget *lbl_bit_rate = gtk_label_new(_("bit rate:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_bit_rate), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_bit_rate, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_bit_rate);
	
	GtkWidget *bit_rate = gtk_spin_button_new_with_range(160000,4000000,10000);
	gtk_editable_set_editable(GTK_EDITABLE(bit_rate),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(bit_rate), codec_defaults->bit_rate);
	
	gtk_table_attach (GTK_TABLE(table), bit_rate, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (bit_rate);
	line++;
	
	GtkWidget *lbl_qmax = gtk_label_new(_("qmax:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_qmax), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_qmax, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_qmax);
	
	GtkWidget *qmax = gtk_spin_button_new_with_range(1,60,1);
	gtk_editable_set_editable(GTK_EDITABLE(qmax),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(qmax), codec_defaults->qmax);
	
	gtk_table_attach (GTK_TABLE(table), qmax, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (qmax);
	line++;
	
	GtkWidget *lbl_qmin = gtk_label_new(_("qmin:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_qmin), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_qmin, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_qmin);
	
	GtkWidget *qmin = gtk_spin_button_new_with_range(1,31,1);
	gtk_editable_set_editable(GTK_EDITABLE(qmin),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(qmin), codec_defaults->qmin);
	
	gtk_table_attach (GTK_TABLE(table), qmin, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (qmin);
	line++;
	
	GtkWidget *lbl_max_qdiff = gtk_label_new(_("max. qdiff:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_max_qdiff), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_max_qdiff, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_max_qdiff);
	
	GtkWidget *max_qdiff = gtk_spin_button_new_with_range(1,4,1);
	gtk_editable_set_editable(GTK_EDITABLE(max_qdiff),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(max_qdiff), codec_defaults->max_qdiff);
	
	gtk_table_attach (GTK_TABLE(table), max_qdiff, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (max_qdiff);
	line++;
	
	GtkWidget *lbl_dia = gtk_label_new(_("dia size:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_dia), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_dia, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_dia);
	
	GtkWidget *dia = gtk_spin_button_new_with_range(-1,4,1);
	gtk_editable_set_editable(GTK_EDITABLE(dia),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(dia), codec_defaults->dia);
	
	gtk_table_attach (GTK_TABLE(table), dia, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (dia);
	line++;
	
	GtkWidget *lbl_pre_dia = gtk_label_new(_("pre dia size:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_pre_dia), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_pre_dia, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_pre_dia);
	
	GtkWidget *pre_dia = gtk_spin_button_new_with_range(1,4,1);
	gtk_editable_set_editable(GTK_EDITABLE(pre_dia),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(pre_dia), codec_defaults->pre_dia);
	
	gtk_table_attach (GTK_TABLE(table), pre_dia, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (pre_dia);
	line++;
	
	GtkWidget *lbl_pre_me = gtk_label_new(_("pre me:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_pre_me), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_pre_me, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_pre_me);
	
	GtkWidget *pre_me = gtk_spin_button_new_with_range(0,2,1);
	gtk_editable_set_editable(GTK_EDITABLE(pre_me),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(pre_me), codec_defaults->pre_me);
	
	gtk_table_attach (GTK_TABLE(table), pre_me, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (pre_me);
	line++;
	
	GtkWidget *lbl_me_pre_cmp = gtk_label_new(_("pre cmp:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_me_pre_cmp), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_me_pre_cmp, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_me_pre_cmp);
	
	GtkWidget *me_pre_cmp = gtk_spin_button_new_with_range(0,6,1);
	gtk_editable_set_editable(GTK_EDITABLE(me_pre_cmp),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(me_pre_cmp), codec_defaults->me_pre_cmp);
	
	gtk_table_attach (GTK_TABLE(table), me_pre_cmp, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (me_pre_cmp);
	line++;
	
	GtkWidget *lbl_me_cmp = gtk_label_new(_("cmp:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_me_cmp), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_me_cmp, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_me_cmp);
	
	GtkWidget *me_cmp = gtk_spin_button_new_with_range(0,6,1);
	gtk_editable_set_editable(GTK_EDITABLE(me_cmp),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(me_cmp), codec_defaults->me_cmp);
	
	gtk_table_attach (GTK_TABLE(table), me_cmp, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (me_cmp);
	line++;
	
	GtkWidget *lbl_me_sub_cmp = gtk_label_new(_("sub cmp:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_me_sub_cmp), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_me_sub_cmp, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_me_sub_cmp);
	
	GtkWidget *me_sub_cmp = gtk_spin_button_new_with_range(0,6,1);
	gtk_editable_set_editable(GTK_EDITABLE(me_sub_cmp),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(me_sub_cmp), codec_defaults->me_sub_cmp);
	
	gtk_table_attach (GTK_TABLE(table), me_sub_cmp, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (me_sub_cmp);
	line++;
	
	GtkWidget *lbl_last_pred = gtk_label_new(_("last predictor count:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_last_pred), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_last_pred, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_last_pred);
	
	GtkWidget *last_pred = gtk_spin_button_new_with_range(1,3,1);
	gtk_editable_set_editable(GTK_EDITABLE(last_pred),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(last_pred), codec_defaults->last_pred);
	
	gtk_table_attach (GTK_TABLE(table), last_pred, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (last_pred);
	line++;
	
	GtkWidget *lbl_gop_size = gtk_label_new(_("gop size:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_gop_size), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_gop_size, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_gop_size);
	
	GtkWidget *gop_size = gtk_spin_button_new_with_range(1,250,1);
	gtk_editable_set_editable(GTK_EDITABLE(gop_size),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(gop_size), codec_defaults->gop_size);
	
	gtk_table_attach (GTK_TABLE(table), gop_size, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gop_size);
	line++;
	
	GtkWidget *lbl_qcompress = gtk_label_new(_("qcompress:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_qcompress), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_qcompress, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_qcompress);
	
	GtkWidget *qcompress = gtk_spin_button_new_with_range(0,1,0.1);
	gtk_editable_set_editable(GTK_EDITABLE(qcompress),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(qcompress), codec_defaults->qcompress);
	
	gtk_table_attach (GTK_TABLE(table), qcompress, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (qcompress);
	line++;
	
	GtkWidget *lbl_qblur = gtk_label_new(_("qblur:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_qblur), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_qblur, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_qblur);
	
	GtkWidget *qblur = gtk_spin_button_new_with_range(0,1,0.1);
	gtk_editable_set_editable(GTK_EDITABLE(qblur),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(qblur), codec_defaults->qblur);
	
	gtk_table_attach (GTK_TABLE(table), qblur, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (qblur);
	line++;

	GtkWidget *lbl_subq = gtk_label_new(_("subq:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_subq), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_subq, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_subq);
	
	GtkWidget *subq = gtk_spin_button_new_with_range(0,8,1);
	gtk_editable_set_editable(GTK_EDITABLE(subq),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(subq), codec_defaults->subq);
	
	gtk_table_attach (GTK_TABLE(table), subq, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (subq);
	line++;

	GtkWidget *lbl_framerefs = gtk_label_new(_("framerefs:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_framerefs), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_framerefs, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_framerefs);
	
	GtkWidget *framerefs = gtk_spin_button_new_with_range(0,12,1);
	gtk_editable_set_editable(GTK_EDITABLE(framerefs),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(framerefs), codec_defaults->framerefs);
	
	gtk_table_attach (GTK_TABLE(table), framerefs, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (framerefs);
	line++;
	
	GtkWidget *lbl_me_method = gtk_label_new(_("me method:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_me_method), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_me_method, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_me_method);
	
	GtkWidget *me_method = gtk_spin_button_new_with_range(1,10,1);
	gtk_editable_set_editable(GTK_EDITABLE(me_method),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(me_method), codec_defaults->me_method);
	
	gtk_table_attach (GTK_TABLE(table), me_method, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (me_method);
	line++;
	
	GtkWidget *lbl_mb_decision = gtk_label_new(_("mb decision:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_mb_decision), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_mb_decision, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_mb_decision);
	
	GtkWidget *mb_decision = gtk_spin_button_new_with_range(0,2,1);
	gtk_editable_set_editable(GTK_EDITABLE(mb_decision),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(mb_decision), codec_defaults->mb_decision);
	
	gtk_table_attach (GTK_TABLE(table), mb_decision, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (mb_decision);
	line++;
	
	GtkWidget *lbl_max_b_frames = gtk_label_new(_("max B frames:   "));
	gtk_misc_set_alignment (GTK_MISC (lbl_max_b_frames), 1, 0.5);
	gtk_table_attach (GTK_TABLE(table), lbl_max_b_frames, 0, 1, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (lbl_max_b_frames);
	
	GtkWidget *max_b_frames = gtk_spin_button_new_with_range(0,4,1);
	gtk_editable_set_editable(GTK_EDITABLE(max_b_frames),TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(max_b_frames), codec_defaults->max_b_frames);
	
	gtk_table_attach (GTK_TABLE(table), max_b_frames, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (max_b_frames);
	line++;
	
	GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (codec_dialog));
	gtk_container_add (GTK_CONTAINER (content_area), table);
	gtk_widget_show (table);
	
	gint result = gtk_dialog_run (GTK_DIALOG (codec_dialog));
	switch (result)
	{
		case GTK_RESPONSE_ACCEPT:
			codec_defaults->fps = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(enc_fps));
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
			codec_defaults->qcompress = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON(qcompress));
			codec_defaults->qblur = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON(qblur));
			codec_defaults->subq = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON(subq));
			codec_defaults->framerefs = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON(framerefs));
			codec_defaults->me_method = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON(me_method));
			codec_defaults->mb_decision = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON(mb_decision));
			codec_defaults->max_b_frames = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON(max_b_frames));
			break;
		default:
			// do nothing since dialog was cancelled
			break;
	}
	gtk_widget_destroy (codec_dialog);

}


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

	{
		int current_format = videoIn->listFormats->current_format;
		listVidFormats = &videoIn->listFormats->listVidFormats[current_format];
	}

	
	if (global->debug) 
		g_printf("resolutions of format(%d) = %d \n",
			videoIn->listFormats->current_format+1,
			listVidFormats->numb_res);
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

	// Frame Rate
	line++;
					  
	gwidget->FrameRate = gtk_combo_box_new_text ();
	int deffps=0;
	if (global->debug) 
		g_printf("frame rates of %dº resolution=%d \n",
			defres+1,
			listVidFormats->listVidCap[defres].numb_frates);
	for ( i = 0 ; i < listVidFormats->listVidCap[defres].numb_frates ; i++) 
	{
		g_snprintf(temp_str,18,"%i/%i fps", listVidFormats->listVidCap[defres].framerate_denom[i],
			listVidFormats->listVidCap[defres].framerate_num[i]);
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->FrameRate),temp_str);
		
		if (( global->fps_num == listVidFormats->listVidCap[defres].framerate_num[i]) && 
			(global->fps == listVidFormats->listVidCap[defres].framerate_denom[i]))
				deffps=i;//set selected
	}
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->FrameRate, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->FrameRate);
	
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->FrameRate),deffps);
	if (deffps==0) 
	{
		if (listVidFormats->listVidCap[defres].framerate_denom)
			global->fps = listVidFormats->listVidCap[defres].framerate_denom[0];

		if (listVidFormats->listVidCap[defres].framerate_num)
			global->fps_num = listVidFormats->listVidCap[defres].framerate_num[0];

		g_printf("fps is set to %i/%i\n", global->fps_num, global->fps);
	}
	gtk_widget_set_sensitive (gwidget->FrameRate, TRUE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->FrameRate), "changed",
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
	gwidget->InpType= gtk_combo_box_new_text ();
	
	int fmtind=0;
	for (fmtind=0; fmtind < videoIn->listFormats->numb_formats; fmtind++)
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->InpType),videoIn->listFormats->listVidFormats[fmtind].fourcc);
		if(global->format == videoIn->listFormats->listVidFormats[fmtind].format)
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->InpType),fmtind); /*set active*/
	}
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->InpType, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_widget_set_sensitive (gwidget->InpType, TRUE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->InpType), "changed",
		G_CALLBACK (InpType_changed), all_data);
	gtk_widget_show (gwidget->InpType);
	
	label_InpType = gtk_label_new(_("Camera Output:"));
	gtk_misc_set_alignment (GTK_MISC (label_InpType), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_InpType, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_InpType);

	//jpeg compression quality for MJPEG and JPEG input formats
	if((global->format == V4L2_PIX_FMT_MJPEG || global->format == V4L2_PIX_FMT_JPEG) && videoIn->jpgcomp.quality > 0)
	{
		line++;
		gwidget->jpeg_comp = gtk_spin_button_new_with_range(0,100,1);
		/*can't edit the spin value by hand*/
		gtk_editable_set_editable(GTK_EDITABLE(gwidget->jpeg_comp),FALSE);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON(gwidget->jpeg_comp), videoIn->jpgcomp.quality);
		gtk_table_attach(GTK_TABLE(table2), gwidget->jpeg_comp, 1, 2, line, line+1,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
		gtk_widget_set_sensitive (gwidget->jpeg_comp, TRUE);
		gtk_widget_show (gwidget->jpeg_comp);

		set_jpeg_comp = gtk_button_new_with_label(_("Apply"));
		gtk_table_attach(GTK_TABLE(table2), set_jpeg_comp, 2, 3, line, line+1,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
		g_signal_connect (GTK_BUTTON (set_jpeg_comp), "clicked",
				G_CALLBACK (set_jpeg_comp_clicked), all_data);
		gtk_widget_set_sensitive (set_jpeg_comp, TRUE);
		gtk_widget_show (set_jpeg_comp);
		
		label_jpeg_comp = gtk_label_new(_("Quality:"));
		gtk_misc_set_alignment (GTK_MISC (label_jpeg_comp), 1, 0.5);

		gtk_table_attach (GTK_TABLE(table2), label_jpeg_comp, 0, 1, line, line+1,
			GTK_FILL, 0, 0, 0);

		gtk_widget_show (label_jpeg_comp);
	}
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

	gtk_widget_show (ImgFolder_img);
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

	//Video Capture
	line++;
	label_VidFile= gtk_label_new(_("Video File:"));
	gtk_misc_set_alignment (GTK_MISC (label_VidFile), 1, 0.5);
	gwidget->VidFNameEntry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table2), label_VidFile, 0, 1, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table2), gwidget->VidFNameEntry, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gwidget->VidFileButt=gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gchar* OVidIconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/videos_folder.png",NULL);
	//don't t1est for file - use default empty image if load fails
	//get icon image
	VidFolder_img = gtk_image_new_from_file(OVidIconPath);
	g_free(OVidIconPath);
	
	gtk_button_set_image (GTK_BUTTON(gwidget->VidFileButt), VidFolder_img);
	gtk_table_attach(GTK_TABLE(table2), gwidget->VidFileButt, 2, 3, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);

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
	gtk_table_attach (GTK_TABLE(table2), gwidget->VidIncLabel, 1, 2, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->VidIncLabel);
	
	gwidget->VidInc=gtk_check_button_new_with_label (_("File,Auto"));
	gtk_table_attach(GTK_TABLE(table2), gwidget->VidInc, 2, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->VidInc),(global->vid_inc > 0));
	
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->VidInc), "toggled",
		G_CALLBACK (VidInc_changed), all_data);
	gtk_widget_show (gwidget->VidInc);
	
	// Video Codec
	line++;
	gwidget->VidCodec = gtk_combo_box_new_text ();
	
	//sets to valid only existing codecs
	setVcodecVal ();
	int vcodec_ind =0;
	for (vcodec_ind =0; vcodec_ind<MAX_VCODECS; vcodec_ind++)
	{
		if (isVcodecValid(vcodec_ind))
			gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->VidCodec),gettext(get_desc4cc(vcodec_ind)));
		
	}
	gtk_table_attach(GTK_TABLE(table2), gwidget->VidCodec, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->VidCodec);
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->VidCodec),global->VidCodec);
	
	gtk_widget_set_sensitive (gwidget->VidCodec, TRUE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->VidCodec), "changed",
		G_CALLBACK (VidCodec_changed), all_data);
	
	label_VidCodec = gtk_label_new(_("Video Codec:"));
	gtk_misc_set_alignment (GTK_MISC (label_VidCodec), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_VidCodec, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (label_VidCodec);
	
	//lavc codec properties button
	gwidget->lavc_button = gtk_button_new_with_label (_("properties"));
	gtk_table_attach (GTK_TABLE(table2), gwidget->lavc_button, 2, 3, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->lavc_button);
	g_signal_connect (GTK_BUTTON(gwidget->lavc_button), "clicked",
		G_CALLBACK (lavc_properties), all_data);
	gtk_widget_set_sensitive (gwidget->lavc_button, isLavcCodec(global->VidCodec));
	
	
	//video container
	line++;
	gwidget->VidFormat = gtk_combo_box_new_text ();
	
	int vformat_ind =0;
	for (vformat_ind =0; vformat_ind<MAX_VFORMATS; vformat_ind++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->VidFormat),gettext(get_vformat_desc(vformat_ind)));
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->VidFormat, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->VidFormat);
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->VidFormat),global->VidFormat);
	
	gtk_widget_set_sensitive (gwidget->VidFormat, TRUE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->VidFormat), "changed",
		G_CALLBACK (VidFormat_changed), all_data);
	
	label_VidFormat = gtk_label_new(_("Video Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_VidFormat), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_VidFormat, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (label_VidFormat);
	
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
	
	// Particles
	FiltParticlesEnable=gtk_check_button_new_with_label (_(" Particles"));
	g_object_set_data (G_OBJECT (FiltParticlesEnable), "filt_info", GINT_TO_POINTER(YUV_PARTICLES));
	gtk_table_attach(GTK_TABLE(table_filt), FiltParticlesEnable, 1, 2, 1, 2,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltParticlesEnable),(global->Frame_Flags & YUV_PARTICLES)>0);
	gtk_widget_show (FiltParticlesEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltParticlesEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), all_data);
	
	
	
	
	//add filters to video tab
	gtk_table_attach (GTK_TABLE(table2), table_filt, 0, 3, line, line+1,
		GTK_FILL, 0, 0, 0);
	gtk_widget_show (table_filt);
	
}
