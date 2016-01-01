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
#include "gui_qt5.hpp"

extern "C" {
#include <errno.h>
#include <assert.h>

/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "gviewv4l2core.h"
#include "video_capture.h"
#include "gviewencoder.h"
#include "gviewrender.h"
#include "gui.h"
#include "core_io.h"
#include "config.h"
/*add this last to avoid redefining _() and N_()*/
#include "gview.h"
}

extern int debug_level;

/*
 * close event (close window)
 * args:
 *   widget - pointer to event widget
 *   event - pointer to event data
 *   data - pointer to user data
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
 void MainWindow::closeEvent(QCloseEvent *event)
{
	/* Terminate program */
	quit_callback(NULL);
}

/*
 * quit button clicked event
 * args:
 *    widget - pointer to widget that caused the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::quit_button_clicked()
{
	/* Terminate program */
	quit_callback(NULL);
}


/*
 * camera_button_menu image clicked
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::menu_camera_button_clicked()
{
	QObject *sender =  QObject::sender();
	int def_action = sender->property("camera_button").toInt();
	
	if(debug_level > 2)
		std::cout << "GUVCVIEW (Qt5): camera button set to action " << def_action << std::endl;
	set_default_camera_button_action(def_action);
}

/*
 * control default clicked event
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::control_defaults_clicked ()
{
	if(debug_level > 2)
		std::cout << "GUVCVIEW (Qt5): setting control defaults" << std::endl;
		
    v4l2core_set_control_defaults();
    gui_qt5_update_controls_state();
}

/*
 * load/save control profile clicked event
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::load_save_profile_clicked()
{
	QObject *sender =  QObject::sender();
	int dialog_type = sender->property("profile_dialog").toInt();
	
	if(dialog_type == 0) //load
	{
		QString filter = _("gpfl  (*.gpfl)");
		filter.append(";;");
		filter.append(_("any (*.*)"));
		QString fileName = QFileDialog::getOpenFileName(this, _("Load Profile"),
			get_profile_path(), filter);
		
		if(!fileName.isEmpty())
		{
			if(debug_level > 1)
				std::cout << "GUVCVIEW (Qt5): load profile " 
					<< fileName.toStdString() << std::endl;
			
			v4l2core_load_control_profile(fileName.toStdString().c_str());
			gui_qt5_update_controls_state();
			
			char *basename = get_file_basename(fileName.toStdString().c_str());
			if(basename)
			{
				set_profile_name(basename);
				free(basename);
			}
			char *pathname = get_file_pathname(fileName.toStdString().c_str());
			if(pathname)
			{
				set_profile_path(pathname);
				free(pathname);
			}
		}	
	}
	else
	{
		QString filter = _("gpfl  (*.gpfl)");
		filter.append(";;");
		filter.append(_("any (*.*)"));
		
		QString profile_name = get_profile_path();
		profile_name.append("/");
		profile_name.append(get_profile_name());
		QString fileName = QFileDialog::getSaveFileName(this, _("Save Profile"),
			profile_name, filter);
		
		if(!fileName.isEmpty())
		{
			if(debug_level > 1)
				std::cout << "GUVCVIEW (Qt5): save profile " 
					<< fileName.toStdString() << std::endl;
				
			v4l2core_save_control_profile(fileName.toStdString().c_str());
			
			char *basename = get_file_basename(fileName.toStdString().c_str());
			if(basename)
			{
				set_profile_name(basename);
				free(basename);
			}
			char *pathname = get_file_pathname(fileName.toStdString().c_str());
			if(pathname)
			{
				set_profile_path(pathname);
				free(pathname);
			}

		}
	}
}

/*
 * photo suffix clicked event
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::photo_sufix_clicked ()
{
	QObject *sender =  QObject::sender();
	QAction *action = (QAction *) sender;
	
	int flag = action->isChecked() ? 1 : 0;
	
	if(debug_level > 1)
		std::cout << "GUVCVIEW (Qt5): photo sufix set to " 
			<< flag << std::endl;
					
	set_photo_sufix_flag(flag);

	/*update config*/
	config_t *my_config = config_get();
	my_config->photo_sufix = flag;
}

/*
 * video suffix clicked event
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::video_sufix_clicked ()
{
	QObject *sender =  QObject::sender();
	QAction *action = (QAction *) sender;
	
	int flag = action->isChecked() ? 1 : 0;
	
	if(debug_level > 1)
		std::cout << "GUVCVIEW (Qt5): video sufix set to " 
			<< flag << std::endl;
			
	set_video_sufix_flag(flag);

	/*update config*/
	config_t *my_config = config_get();
	my_config->video_sufix = flag;
}

/*
 * video codec clicked event
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::video_codec_clicked ()
{
	QObject *sender =  QObject::sender();
	int vcodec_ind = sender->property("video_codec").toInt();

	if(debug_level > 1)
		std::cout << "GUVCVIEW (Qt5): video codec changed to " 
			<< vcodec_ind << std::endl;

	set_video_codec_ind(vcodec_ind);

	if( get_video_muxer() == ENCODER_MUX_WEBM &&
		!encoder_check_webm_video_codec(vcodec_ind))
	{
		/*change from webm to matroska*/
		set_video_muxer(ENCODER_MUX_MKV);
		char *newname = set_file_extension(get_video_name(), "mkv");
		set_video_name(newname);

		free(newname);
	}
}

///*
 //* audio codec changed event
 //* args:
 //*    item - widget that generated the event
 //*    data - pointer to user data
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//void audio_codec_changed (GtkRadioMenuItem *item, void *data)
//{
   //GSList *agroup = (GSList *) data;

	//if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
	//{
		///*
		 //* GSList indexes (g_slist_index) are in reverse order:
		 //* last inserted has index 0
		 //* so count backwards
		 //*/
		//int num_codecs = g_slist_length(agroup);
		//int index = g_slist_index (agroup, item);
		//index = num_codecs - (index + 1); //reverse order and 0 indexed
		//fprintf(stderr,"GUVCVIEW: audio codec changed to %i\n", index);

		//set_audio_codec_ind(index);

		//if( get_video_muxer() == ENCODER_MUX_WEBM &&
			//!encoder_check_webm_audio_codec(index))
		//{
			///*change from webm to matroska*/
			//set_video_muxer(ENCODER_MUX_MKV);
			//char *newname = set_file_extension(get_video_name(), "mkv");
			//set_video_name(newname);
			//free(newname);
		//}
	//}
//}

///*
 //* called from photo format combo in file dialog
 //* args:
 //*    chooser - format combo that caused the event
 //*    file_dialog - chooser parent
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//static void photo_update_extension (GtkComboBox *chooser, GtkWidget *file_dialog)
//{
	//int format = gtk_combo_box_get_active (chooser);

	//set_photo_format(format);

	//char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (file_dialog));
	//char *basename = get_file_basename(filename);

	//GtkFileFilter *filter = gtk_file_filter_new();

	//switch(format)
	//{
		//case IMG_FMT_RAW:
			//gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_dialog),
				//set_file_extension(basename, "raw"));
			//gtk_file_filter_add_pattern(filter, "*.raw");
			//break;
		//case IMG_FMT_PNG:
			//gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_dialog),
				//set_file_extension(basename, "png"));
			//gtk_file_filter_add_pattern(filter, "*.png");
			//break;
		//case IMG_FMT_BMP:
			//gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_dialog),
				//set_file_extension(basename, "bmp"));
			//gtk_file_filter_add_pattern(filter, "*.bmp");
			//break;
		//default:
		//case IMG_FMT_JPG:
			//gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_dialog),
				//set_file_extension(basename, "jpg"));
			//gtk_file_filter_add_pattern(filter, "*.jpg");
			//break;
	//}

	//gtk_file_chooser_set_filter(GTK_FILE_CHOOSER (file_dialog), filter);

	//if(filename)
		//free(filename);
	//if(basename)
		//free(basename);
//}

///*
 //* called from video muxer format combo in file dialog
 //* args:
 //*    chooser - format combo that caused the event
 //*    file_dialog - chooser parent
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//static void video_update_extension (GtkComboBox *chooser, GtkWidget *file_dialog)
//{
	//int format = gtk_combo_box_get_active (chooser);

	//set_video_muxer(format);

	//char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (file_dialog));
	//char *basename = get_file_basename(filename);

	//GtkFileFilter *filter = gtk_file_filter_new();

	//switch(format)
	//{
		//case ENCODER_MUX_WEBM:
			//gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_dialog),
				//set_file_extension(basename, "webm"));
			//gtk_file_filter_add_pattern(filter, "*.webm");
			//break;
		//case ENCODER_MUX_AVI:
			//gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_dialog),
				//set_file_extension(basename, "avi"));
			//gtk_file_filter_add_pattern(filter, "*.avi");
			//break;
		//default:
		//case ENCODER_MUX_MKV:
			//gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_dialog),
				//set_file_extension(basename, "mkv"));
			//gtk_file_filter_add_pattern(filter, "*.mkv");
			//break;
	//}

	//gtk_file_chooser_set_filter(GTK_FILE_CHOOSER (file_dialog), filter);

	//if(filename)
		//free(filename);
	//if(basename)
		//free(basename);
//}

/*
 * photo file clicked event
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::photo_file_clicked ()
{
	QString filter = _("Jpeg  (*.jpg)");
	filter.append(";;");
	filter.append(_("Png (*.png)"));
	filter.append(";;");
	filter.append(_("Bmp  (*.bmp)"));
	filter.append(";;");
	filter.append(_("Raw  (*.raw)"));
	filter.append(";;");
	filter.append(_("Images  (*.jpg *.png *.bmp *.raw)"));
	
	
	QString photo_name = get_photo_path();
	photo_name.append("/");
	photo_name.append(get_photo_name());
	QString fileName = QFileDialog::getSaveFileName(this, _("Photo file name"),
			photo_name, filter);
		
	if(!fileName.isEmpty())
	{
		if(debug_level > 1)
			std::cout << "GUVCVIEW (Qt5): set photo filename to " 
				<< fileName.toStdString() << std::endl;
				
			
			
		char *basename = get_file_basename(fileName.toStdString().c_str());
		if(basename)
		{
			set_photo_name(basename);
			free(basename);
		}
		char *pathname = get_file_pathname(fileName.toStdString().c_str());
		if(pathname)
		{
			set_photo_path(pathname);
			free(pathname);
		}
	}
}

/*
 * video file clicked event
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::video_file_clicked ()
{
	QString filter = _("Matroska  (*.mkv)");
	filter.append(";;");
	filter.append(_("WebM (*.webm)"));
	filter.append(";;");
	filter.append(_("Avi  (*.avi)"));
	filter.append(";;");
	filter.append(_("Videos  (*.mkv *.webm *.avi)"));
	
	QString video_name = get_video_path();
	video_name.append("/");
	video_name.append(get_video_name());
	QString fileName = QFileDialog::getSaveFileName(this, _("Video file name"),
			video_name, filter);
		
	if(!fileName.isEmpty())
	{
		if(debug_level > 1)
			std::cout << "GUVCVIEW (Qt5): set video filename to " 
				<< fileName.toStdString() << std::endl;
					
		char *basename = get_file_basename(fileName.toStdString().c_str());
		if(basename)
		{
			set_video_name(basename);
			free(basename);
		}
		char *pathname = get_file_pathname(fileName.toStdString().c_str());
		if(pathname)
		{
			set_video_path(pathname);
			free(pathname);
		}
		
		/*update codecs for webm special case*/
		if(get_video_codec_ind() == encoder_get_webm_video_codec_index() and
			webm_vcodec_action != NULL)
		{
			webm_vcodec_action->setChecked(true);
		}
		if(get_audio_codec_ind() == encoder_get_webm_audio_codec_index() and
			webm_acodec_action != NULL)
		{
			webm_acodec_action->setChecked(true);
		}
		
	}
}

///*
 //* capture image button clicked event
 //* args:
 //*   button - widget that generated the event
 //*   data - pointer to user data
 //*
 //* asserts:
 //*   none
 //*
 //* returns: none
 //*/
//void capture_image_clicked (GtkButton *button, void *data)
//{
	//int is_photo_timer = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (button), "control_info"));

	//if(is_photo_timer)
	//{
		//stop_photo_timer();
		//gtk_button_set_label(button, _("Cap. Image (I)"));
	//}
	//else
		//video_capture_save_image();
//}

///*
 //* capture video button clicked event
 //* args:
 //*   button - widget that generated the event
 //*   data - pointer to user data
 //*
 //* asserts:
 //*   none
 //*
 //* returns: none
 //*/
//void capture_video_clicked(GtkToggleButton *button, void *data)
//{
	//int active = gtk_toggle_button_get_active (button);

	//if(debug_level > 0)
		//printf("GUVCVIEW: video capture toggled(%i)\n", active);

	//if(active)
	//{
		//start_encoder_thread();
		//gtk_button_set_label(GTK_BUTTON(button), _("Stop Video (V)"));

	//}
	//else
	//{
		//stop_encoder_thread();
		//gtk_button_set_label(GTK_BUTTON(button), _("Cap. Video (V)"));
		///*make sure video timer is reset*/
		//reset_video_timer();
	//}
//}

/*
 * pan/tilt step changed
 * args:
 *    spin - spinbutton that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns:
 *    none
 */
void MainWindow::pan_tilt_step_changed (int value)
{
	QObject *sender =  QObject::sender();
	int id = sender->property("control_info").toInt();

	if(id == V4L2_CID_PAN_RELATIVE)
		v4l2core_set_pan_step(value);
	if(id == V4L2_CID_TILT_RELATIVE)
		v4l2core_set_tilt_step(value);
}

/*
 * Pan Tilt button 1 clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::button_PanTilt1_clicked()
{
	QObject *sender =  QObject::sender();
	int id = sender->property("control_info").toInt();

    v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

	if(id == V4L2_CID_PAN_RELATIVE)
		control->value = v4l2core_get_pan_step();
	else
		control->value = v4l2core_get_tilt_step();

    if(v4l2core_set_control_value_by_id(id))
		std::cerr << "GUVCVIEW: error setting pan/tilt" << std::endl;
}

/*
 * Pan Tilt button 2 clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::button_PanTilt2_clicked()
{
    QObject *sender =  QObject::sender();
	int id = sender->property("control_info").toInt();

    v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

    if(id == V4L2_CID_PAN_RELATIVE)
		control->value =  - v4l2core_get_pan_step();
	else
		control->value =  - v4l2core_get_tilt_step();

    if(v4l2core_set_control_value_by_id(id))
		std::cerr << "GUVCVIEW: error setting pan/tilt" << std::endl;
}

/*
 * generic button clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::button_clicked()
{
    QObject *sender =  QObject::sender();
	int id = sender->property("control_info").toInt();

    v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

	control->value = 1;

    if(v4l2core_set_control_value_by_id(id))
		fprintf(stderr, "GUVCVIEW: error setting button value\n");

	gui_qt5_update_controls_state();
}

//#ifdef V4L2_CTRL_TYPE_STRING
///*
 //* a string control apply button clicked
 //* args:
 //*    button - button that generated the event
 //*    data - pointer to user data
 //*
 //* asserts:
 //*    control->string not null
 //*
 //* returns: none
 //*/
//void string_button_clicked(GtkButton * Button, void *data)
//{
	//int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));
	//GtkWidget *entry = (GtkWidget *) g_object_get_data (G_OBJECT (Button), "control_entry");

	//v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

	//assert(control->string != NULL);

	//strncpy(control->string, gtk_entry_get_text(GTK_ENTRY(entry)), control->control.maximum);

	//if(v4l2core_set_control_value_by_id(id))
		//fprintf(stderr, "GUVCVIEW: error setting string value\n");
//}
//#endif

#ifdef V4L2_CTRL_TYPE_INTEGER64
/*
 * a int64 control apply button clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::int64_button_clicked()
{
	QObject *sender =  QObject::sender();
	int id = sender->property("control_info").toInt();
	QLineEdit *entry = (QLineEdit *) sender->property("control_entry").value();
	
	if(!entry)
	{
		std:cerr << "Guvcview (Qt5): couldn't get QLineEdit pointer for int64 control: " << std::hex << id << std::endl;
		return;
	}
	
	v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

	QString text_input = entry->text();
	text_input = text_input.remove(" ");
	bool ok;
	if(text_input.startsWith("0x")) //hex format
	{
		control->value64 = text_input.toLongLong(&ok, 16);
	}
	else //decimal
	{
		control->value64 = text_input.toLongLong(&ok, 10);
	}
	
	if(v4l2core_set_control_value_by_id(id))
		std:cerr << "GUVCVIEW (Qt5): error setting int64 value" << std::endl;

}
#endif

//#ifdef V4L2_CTRL_TYPE_BITMASK
///*
 //* a bitmask control apply button clicked
 //* args:
 //*    button - button that generated the event
 //*    data - pointer to user data
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//void bitmask_button_clicked(GtkButton * Button, void *data)
//{
	//int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));
	//GtkWidget *entry = (GtkWidget *) g_object_get_data (G_OBJECT (Button), "control_entry");

	//v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

	//char* text_input = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	//text_input = g_strcanon(text_input,"0123456789ABCDEFabcdef", '\0');
	//control->value = (int32_t) g_ascii_strtoll(text_input, NULL, 16);
	//g_free(text_input);

	//if(v4l2core_set_control_value_by_id(id))
		//fprintf(stderr, "GUVCVIEW: error setting string value\n");
//}
//#endif

/*
 * slider changed event
 * args:
 *    range - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::slider_value_changed(int value)
{
    QObject *sender =  QObject::sender();
	int id = sender->property("control_info").toInt();
	
    v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

    control->value = value;

    if(v4l2core_set_control_value_by_id(id))
		std::cerr << "GUVCVIEW (Qt5): error setting slider value" <<std::endl;
}

/*
 * spin changed event
 * args:
 *    spin - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::spin_value_changed (int value)
{
    QObject *sender =  QObject::sender();
	int id = sender->property("control_info").toInt();
	
    v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

    control->value = value;

     if(v4l2core_set_control_value_by_id(id))
		std::cerr << "GUVCVIEW (Qt5): error setting spin value" <<std::endl;

}

/*
 * combo box changed event
 * args:
 *    combo - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::combo_changed (int index)
{
	QObject *sender =  QObject::sender();
	int id = sender->property("control_info").toInt();
    
    v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

    control->value = control->menu[index].index;

	if(v4l2core_set_control_value_by_id(id))
		std::cerr << "GUVCVIEW (Qt5): error setting menu value" <<std::endl;

	gui_qt5_update_controls_state();
}

/*
 * bayer pixel order combo box changed event
 * args:
 *    combo - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::bayer_pix_ord_changed (int index)
{
	v4l2core_set_bayer_pix_order(index);
}

/*
 * check box changed event
 * args:
 *    toggle - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::check_changed (int state)
{
    QObject *sender =  QObject::sender();
	int id = sender->property("control_info").toInt();
	
    v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

    int val = (state != 0) ? 1 : 0;

    control->value = val;

	if(v4l2core_set_control_value_by_id(id))
		std::cerr << "GUVCVIEW: error setting menu value" << std::endl;

    if(id == V4L2_CID_DISABLE_PROCESSING_LOGITECH)
    {
        if (control->value > 0)
			v4l2core_set_isbayer(1);
        else
			v4l2core_set_isbayer(0);

        /*
         * must restart stream and requeue
         * the buffers for changes to take effect
         * (updating fps provides all that is needed)
         */
        v4l2core_request_framerate_update ();
    }

    gui_qt5_update_controls_state();
}

/*
 * device list box changed event
 * args:
 *    index - new device index
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::devices_changed (int index)
{
	if(index == v4l2core_get_this_device_index())
		return;

	v4l2_device_list *device_list = v4l2core_get_device_list();
	
	QMessageBox msgBox;
	msgBox.setIcon(QMessageBox::Question);
	msgBox.setWindowTitle(_("start new"));
	msgBox.setText(_("launch new process or restart?.\n\n"));
	QPushButton *restartButton = msgBox.addButton(_("restart"), QMessageBox::ActionRole);
	QPushButton *newButton = msgBox.addButton(_("new"), QMessageBox::ActionRole);
	msgBox.addButton(QMessageBox::Cancel);

	msgBox.exec();

	if (msgBox.clickedButton() == restartButton) 
	{
		QStringList args;
		QString dev_arg = "--device=";
		
		dev_arg.append(device_list->list_devices[index].device);
		args << dev_arg;
		
		if(debug_level > 1)
			std::cout << "GUVCVIEW (Qt5): spawning new process: guvcview " 
					  << dev_arg.toStdString() << std::endl;
		
		QProcess process;
		if(process.startDetached("guvcview", args))
			quit_callback(NULL); //terminate current process
		else
			std::cerr << "GUVCVIEW (Qt5): spawning new process (guvcview " 
					  << dev_arg.toStdString() << ") failed" << std::endl;
	} 
	else if (msgBox.clickedButton() == newButton) 
	{
		QStringList args;
		QString dev_arg = "--device=";
		
		dev_arg.append(device_list->list_devices[index].device);
		args << dev_arg;
		
		if(debug_level > 1)
			std::cout << "GUVCVIEW (Qt5): spawning new process: guvcview " 
					  << dev_arg.toStdString() << std::endl;
		
		QProcess process;
		if(!process.startDetached("guvcview", args))
			std::cerr << "GUVCVIEW (Qt5): spawning new process (guvcview " 
					  << dev_arg.toStdString() << ") failed" << std::endl;
	}
	
	/*reset to current device*/
	/*disable device combobox signals*/
	combobox_video_devices->blockSignals(true);
	combobox_video_devices->setCurrentIndex(v4l2core_get_this_device_index());
	/*enable device combobox signals*/
	combobox_video_devices->blockSignals(false);
}

/*
 * frame rate list box changed event
 * args:
 *    index - new frame rate index
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::frame_rate_changed (int index)
{
	int format_index = v4l2core_get_frame_format_index(v4l2core_get_requested_frame_format());

	int resolu_index = v4l2core_get_format_resolution_index(
		format_index,
		v4l2core_get_frame_width(),
		v4l2core_get_frame_height());

	v4l2_stream_formats_t *list_stream_formats = v4l2core_get_formats_list();
	
	int fps_denom = list_stream_formats[format_index].list_stream_cap[resolu_index].framerate_denom[index];
	int fps_num = list_stream_formats[format_index].list_stream_cap[resolu_index].framerate_num[index];
	
	v4l2core_define_fps(fps_num, fps_denom);

	int fps[2] = {fps_num, fps_denom};
	gui_set_fps(fps);

	v4l2core_request_framerate_update ();
}

/*
 * resolution list box changed event
 * args:
 *   index - new resolution index
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::resolution_changed (int index)
{
	int format_index = v4l2core_get_frame_format_index(v4l2core_get_requested_frame_format());

	/*disable fps combobox signals*/
	combobox_FrameRate->blockSignals(true);
	/* clear out the old fps list... */
	combobox_FrameRate->clear();

	v4l2_stream_formats_t *list_stream_formats = v4l2core_get_formats_list();
	
	int width = list_stream_formats[format_index].list_stream_cap[index].width;
	int height = list_stream_formats[format_index].list_stream_cap[index].height;

	std::cout << "GUVCVIEW (Qt5): updating frame rates for new resolution of "
		<< width << "x" << height << std::endl;
	/*check if frame rate is available at the new resolution*/
	int i=0;
	int deffps=0;

	for ( i = 0 ; i < list_stream_formats[format_index].list_stream_cap[index].numb_frates ; i++)
	{
		QString fps_str = QString( "%1/%2 fps").arg(list_stream_formats[format_index].list_stream_cap[index].framerate_denom[i]).arg(list_stream_formats[format_index].list_stream_cap[index].framerate_num[i]);

		combobox_FrameRate->addItem(fps_str, i);

		if (( v4l2core_get_fps_num() == list_stream_formats[format_index].list_stream_cap[index].framerate_num[i]) &&
			( v4l2core_get_fps_denom() == list_stream_formats[format_index].list_stream_cap[index].framerate_denom[i]))
				deffps=i;//set selected
	}

	/*set default fps in combo*/
	combobox_FrameRate->setCurrentIndex(deffps);
	
	/*enable fps combobox signals*/
	combobox_FrameRate->blockSignals(false);

	if (list_stream_formats[format_index].list_stream_cap[index].framerate_num)
		v4l2core_define_fps(list_stream_formats[format_index].list_stream_cap[index].framerate_num[deffps], -1);

	if (list_stream_formats[format_index].list_stream_cap[index].framerate_denom)
		v4l2core_define_fps(-1, list_stream_formats[format_index].list_stream_cap[index].framerate_denom[deffps]);

	/*change resolution (try new format and reset render)*/
	v4l2core_prepare_new_resolution(width, height);

	request_format_update();

	/*update the config data*/
	config_t *my_config = config_get();

	my_config->width = width;
	my_config->height= height;
}

/*
 * device pixel format list box changed event
 * args:
 *    index - format index
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::format_changed(int index)
{

	int i=0;
	int defres = 0;

	/*disable resolution combobox signals*/
	combobox_resolution->blockSignals(true);

	/* clear out the old resolution list... */
	combobox_resolution->clear();

	v4l2_stream_formats_t *list_stream_formats = v4l2core_get_formats_list();
		
	int format = list_stream_formats[index].format;

	/*update config*/
	config_t *my_config = config_get();
	strncpy(my_config->format, list_stream_formats[index].fourcc, 4);

	/*redraw resolution combo for new format*/
	for(i = 0 ; i < list_stream_formats[index].numb_res ; i++)
	{
		if (list_stream_formats[index].list_stream_cap[i].width > 0)
		{
			QString res_str = QString( "%1x%2").arg(list_stream_formats[index].list_stream_cap[i].width).arg(list_stream_formats[index].list_stream_cap[i].height);
			combobox_resolution->addItem(res_str, i);

			if ((v4l2core_get_frame_width() == list_stream_formats[index].list_stream_cap[i].width) &&
				(v4l2core_get_frame_height() == list_stream_formats[index].list_stream_cap[i].height))
					defres=i;//set selected resolution index
		}
	}

	/*change resolution*/
	combobox_resolution->setCurrentIndex(defres);
	
	/*enable resolution combobox signals*/
	combobox_resolution->blockSignals(false);

	/*prepare new format*/
	v4l2core_prepare_new_format(format);
	
	resolution_changed (defres);
}

/*
 * render fx filter changed event
 * args:
 *    state - checkbox state
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::render_fx_filter_changed(int state)
{
	QObject *sender =  QObject::sender();
	int filter = sender->property("filt_info").toInt();

	uint32_t mask = (state != 0) ?
			get_render_fx_mask() | filter :
			get_render_fx_mask() & ~filter;

	set_render_fx_mask(mask);
}

///*
 //* audio fx filter changed event
 //* args:
 //*    toggle - widget that generated the event
 //*    data - pointer to user data
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//void audio_fx_filter_changed(GtkToggleButton *toggle, void *data)
//{
	//int filter = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (toggle), "filt_info"));

	//uint32_t mask = gtk_toggle_button_get_active (toggle) ?
			//get_audio_fx_mask() | filter :
			//get_audio_fx_mask() & ~filter;

	//set_audio_fx_mask(mask);
//}

/*
 * render osd changed event
 * args:
 *    state - checkbox state
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::render_osd_changed(int state)
{
	QObject *sender =  QObject::sender();
	int osd = sender->property("osd_info").toInt();

	uint32_t mask = (state != 0) ?
			render_get_osd_mask() | osd :
			render_get_osd_mask() & ~osd;

	render_set_osd_mask(mask);

	/* update config */
	config_t *my_config = config_get();
	my_config->osd_mask = render_get_osd_mask();
	/*make sure to disable VU meter OSD in config - it's set by audio capture*/
	my_config->osd_mask &= ~REND_OSD_VUMETER_MONO;
	my_config->osd_mask &= ~REND_OSD_VUMETER_STEREO;
}

/*
 * software autofocus checkbox changed event
 * args:
 *    toggle - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::autofocus_changed (int state)
{
	int val = (state != 0) ? 1 : 0;

	//GtkWidget *wgtFocus_slider = (GtkWidget *) g_object_get_data (G_OBJECT (toggle), "control_entry");
	//GtkWidget *wgtFocus_spin = (GtkWidget *) g_object_get_data (G_OBJECT (toggle), "control2_entry");
	/*if autofocus disable manual focus control*/
	//gtk_widget_set_sensitive (wgtFocus_slider, !val);
	//gtk_widget_set_sensitive (wgtFocus_spin, !val);

	set_soft_autofocus(val);

}

/*
 * software autofocus button clicked event
 * args:
 *    button - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::setfocus_clicked ()
{
	set_soft_focus(1);
}

///******************* AUDIO CALLBACKS *************************/

///*
 //* audio device list box changed event
 //* args:
 //*    combo - widget that generated the event
 //*    data - pointer to user data
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//void audio_device_changed(GtkComboBox *combo, void *data)
//{
	//audio_widgets_t *my_audio_widgets = (audio_widgets_t *) data;

	//int index = gtk_combo_box_get_active(combo);

	///*update the audio context for the new api*/
	//audio_context_t *audio_ctx = get_audio_context();

	//if(index < 0)
		//index = 0;
	//else if (index >= audio_ctx->num_input_dev)
		//index = audio_ctx->num_input_dev - 1;
	
	///*update config*/
	//config_t *my_config = config_get();
	//my_config->audio_device = index;
	
	///*set the audio device defaults*/
	//audio_set_device(audio_ctx, my_config->audio_device);

	//if(debug_level > 0)
		//printf("GUVCVIEW: audio device changed to %i\n", audio_ctx->device);

	//index = gtk_combo_box_get_active(GTK_COMBO_BOX(my_audio_widgets->channels));

	//if(index == 0)
	//{
		//audio_ctx->channels = audio_ctx->list_devices[audio_ctx->device].channels;
		//if(audio_ctx->channels > 2)
			//audio_ctx->channels = 2;/*limit it to stereo input*/
	//}

	//index = gtk_combo_box_get_active(GTK_COMBO_BOX(my_audio_widgets->samprate));

	//if(index == 0)
		//audio_ctx->samprate = audio_ctx->list_devices[audio_ctx->device].samprate;
		
	//gtk_range_set_value(GTK_RANGE(my_audio_widgets->latency), audio_ctx->latency);
//}

///*
 //* audio samplerate list box changed event
 //* args:
 //*    combo - widget that generated the event
 //*    data - pointer to user data
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//void audio_samplerate_changed(GtkComboBox *combo, void *data)
//{
	//int index = gtk_combo_box_get_active(combo);

	///*update the audio context for the new api*/
	//audio_context_t *audio_ctx = get_audio_context();

	//switch(index)
	//{
		//case 0:
			//audio_ctx->samprate = audio_ctx->list_devices[audio_ctx->device].samprate;
			//break;
		//case 1:
			//audio_ctx->samprate = 7350;
			//break;
		//case 2:
			//audio_ctx->samprate = 8000;
			//break;
		//case 3:
			//audio_ctx->samprate = 11025;
			//break;
		//case 4:
			//audio_ctx->samprate = 12000;
			//break;
		//case 5:
			//audio_ctx->samprate = 16000;
			//break;
		//case 6:
			//audio_ctx->samprate = 22050;
			//break;
		//case 7:
			//audio_ctx->samprate = 24000;
			//break;
		//case 8:
			//audio_ctx->samprate = 32000;
			//break;
		//case 9:
			//audio_ctx->samprate = 44100;
			//break;
		//case 10:
			//audio_ctx->samprate = 48000;
			//break;
		//case 11:
			//audio_ctx->samprate = 64000;
			//break;
		//case 12:
			//audio_ctx->samprate = 88200;
			//break;
		//case 13:
			//audio_ctx->samprate = 96000;
			//break;
		//default:
			//audio_ctx->samprate = 44100;
			//break;
	//}
//}

///*
 //* audio channels list box changed event
 //* args:
 //*    combo - widget that generated the event
 //*    data - pointer to user data
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//void audio_channels_changed(GtkComboBox *combo, void *data)
//{
	//int index = gtk_combo_box_get_active(combo);

	///*update the audio context for the new api*/
	//audio_context_t *audio_ctx = get_audio_context();

	//int channels = 0;

	//switch(index)
	//{
		//case 0:
			//channels = audio_ctx->list_devices[audio_ctx->device].channels;
			//break;
		//case 1:
			//channels =  1;
			//break;
		//default:
		//case 2:
			//channels = 2;
			//break;
	//}

	//if(channels > audio_ctx->list_devices[audio_ctx->device].channels)
		//audio_ctx->channels = audio_ctx->list_devices[audio_ctx->device].channels;

	//if(audio_ctx->channels > 2)
		//audio_ctx->channels = 2; /*limit to stereo*/
//}

///*
 //* audio api list box changed event
 //* args:
 //*    combo - widget that generated the event
 //*    data - pointer to user data
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//void audio_api_changed(GtkComboBox *combo, void *data)
//{
	//audio_widgets_t *my_audio_widgets = (audio_widgets_t *) data;

	//int api = gtk_combo_box_get_active(combo);

	///*update the audio context for the new api*/
	//audio_context_t *audio_ctx = create_audio_context(api, -1);
	//if(!audio_ctx)
		//api = AUDIO_NONE;
		
	///*update the config audio entry*/
	//config_t *my_config = config_get();
	//switch(api)
	//{
		//case AUDIO_NONE:
			//strncpy(my_config->audio, "none", 5);
			//break;
		//case AUDIO_PULSE:
			//strncpy(my_config->audio, "pulse", 5);
			//break;
		//default:
			//strncpy(my_config->audio, "port", 5);
			//break;
	//}


	//if(api == AUDIO_NONE || audio_ctx == NULL)
	//{
		//gtk_combo_box_set_active(combo, api);
		//gtk_widget_set_sensitive(my_audio_widgets->device, FALSE);
		//gtk_widget_set_sensitive(my_audio_widgets->channels, FALSE);
		//gtk_widget_set_sensitive(my_audio_widgets->samprate, FALSE);
		//gtk_widget_set_sensitive(my_audio_widgets->latency, FALSE);
	//}
	//else
	//{
		//g_signal_handlers_block_by_func(
			//GTK_COMBO_BOX_TEXT(my_audio_widgets->device),
			//G_CALLBACK (audio_device_changed),
			//my_audio_widgets);

		///* clear out the old device list... */
		//GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX(my_audio_widgets->device)));
		//gtk_list_store_clear(store);

		//int i = 0;
		//for(i = 0; i < audio_ctx->num_input_dev; ++i)
		//{
			//gtk_combo_box_text_append_text(
				//GTK_COMBO_BOX_TEXT(my_audio_widgets->device),
				//audio_ctx->list_devices[i].description);
		//}

		//gtk_combo_box_set_active(GTK_COMBO_BOX(my_audio_widgets->device), audio_ctx->device);

		//g_signal_handlers_unblock_by_func(
			//GTK_COMBO_BOX_TEXT(my_audio_widgets->device),
			//G_CALLBACK (audio_device_changed),
			//my_audio_widgets);

		//gtk_widget_set_sensitive (my_audio_widgets->device, TRUE);
		//gtk_widget_set_sensitive(my_audio_widgets->channels, TRUE);
		//gtk_widget_set_sensitive(my_audio_widgets->samprate, TRUE);
		//gtk_widget_set_sensitive(my_audio_widgets->latency, TRUE);

		///*update channels*/
		//int index = gtk_combo_box_get_active(GTK_COMBO_BOX(my_audio_widgets->channels));

		//if(index == 0) /*auto*/
			//audio_ctx->channels = audio_ctx->list_devices[audio_ctx->device].channels;

		///*update samprate*/
		//index = gtk_combo_box_get_active(GTK_COMBO_BOX(my_audio_widgets->samprate));

		//if(index == 0) /*auto*/
			//audio_ctx->samprate = audio_ctx->list_devices[audio_ctx->device].samprate;
		
		//gtk_range_set_value(GTK_RANGE(my_audio_widgets->latency), audio_ctx->latency);	
	//}

//}

///*
 //* audio latency changed event
 //* args:
 //*    range - widget that generated the event
 //*    data - pointer to user data
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//void audio_latency_changed(GtkRange *range, void *data)
//{
	///**update the audio context for the new api*/
	//audio_context_t *audio_ctx = get_audio_context();

	//if(audio_ctx != NULL)
		//audio_ctx->latency = (double) gtk_range_get_value (range);
//}

/*
 * video encoder properties clicked event
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::video_codec_properties()
{
	video_codec_t *defaults = encoder_get_video_codec_defaults(get_video_codec_ind());

	QDialog dialog(this);
	dialog.setWindowTitle(_("video codec values"));
	QScrollArea *scroll = new QScrollArea(&dialog);
	
	QWidget *viewport = new QWidget(&dialog);
	scroll->setWidget(viewport);
	scroll->setWidgetResizable(true);
	
	// Use a layout allowing to have a label next to each field
	QFormLayout form(viewport);
	viewport->setLayout(&form);
	
	// Add a layout for QDialog
	QHBoxLayout *dialog_layout = new QHBoxLayout(&dialog);
	dialog.setLayout(dialog_layout);
	dialog.layout()->addWidget(scroll); // add scroll to the QDialog's layout
	
	/*fps*/
	QSpinBox *enc_fps = new QSpinBox(&dialog);
	enc_fps->setRange(0, 60);
	enc_fps->setSingleStep(1);						
	enc_fps->setValue(defaults->fps);
	form.addRow(_("                              encoder fps:   \n (0 - use fps combobox value)"),
		enc_fps);
	/*monotonic pts*/
	QCheckBox *monotonic_pts = new QCheckBox(&dialog);
	monotonic_pts->setChecked(defaults->monotonic_pts != 0);
	form.addRow(_(" monotonic pts"), monotonic_pts);
	/*bit rate*/
	QSpinBox *bit_rate = new QSpinBox(&dialog);
	bit_rate->setRange(160000, 4000000);
	bit_rate->setSingleStep(10000);						
	bit_rate->setValue(defaults->bit_rate);
	form.addRow(_("bit rate:   "), bit_rate);
	/*qmax*/
	QSpinBox *qmax = new QSpinBox(&dialog);
	qmax->setRange(1, 60);
	qmax->setSingleStep(1);						
	qmax->setValue(defaults->qmax);
	form.addRow(_("qmax:   "), qmax);
	/*qmin*/
	QSpinBox *qmin = new QSpinBox(&dialog);
	qmin->setRange(1, 31);
	qmin->setSingleStep(1);						
	qmin->setValue(defaults->qmin);
	form.addRow(_("qmin:   "), qmin);
	/*max qdiff*/
	QSpinBox *max_qdiff = new QSpinBox(&dialog);
	max_qdiff->setRange(1, 4);
	max_qdiff->setSingleStep(1);						
	max_qdiff->setValue(defaults->max_qdiff);
	form.addRow(_("max. qdiff:   "), max_qdiff);
	/*dia size*/
	QSpinBox *dia = new QSpinBox(&dialog);
	dia->setRange(-1, 4);
	dia->setSingleStep(1);						
	dia->setValue(defaults->dia);
	form.addRow(_("dia size:   "), dia);
	/*pre dia size*/
	QSpinBox *pre_dia = new QSpinBox(&dialog);
	pre_dia->setRange(1, 4);
	pre_dia->setSingleStep(1);						
	pre_dia->setValue(defaults->pre_dia);
	form.addRow(_("pre dia size:   "), pre_dia);
	/*pre me*/
	QSpinBox *pre_me = new QSpinBox(&dialog);
	pre_me->setRange(0, 2);
	pre_me->setSingleStep(1);						
	pre_me->setValue(defaults->pre_me);
	form.addRow(_("pre me:   "), pre_me);
	/*me pre cmp*/
	QSpinBox *me_pre_cmp = new QSpinBox(&dialog);
	me_pre_cmp->setRange(0, 6);
	me_pre_cmp->setSingleStep(1);						
	me_pre_cmp->setValue(defaults->me_pre_cmp);
	form.addRow(_("pre cmp:   "), me_pre_cmp);
	/*me cmp*/
	QSpinBox *me_cmp = new QSpinBox(&dialog);
	me_cmp->setRange(0, 6);
	me_cmp->setSingleStep(1);						
	me_cmp->setValue(defaults->me_cmp);
	form.addRow(_("cmp:   "), me_cmp);
	/*me sub cmp*/
	QSpinBox *me_sub_cmp = new QSpinBox(&dialog);
	me_sub_cmp->setRange(0, 6);
	me_sub_cmp->setSingleStep(1);						
	me_sub_cmp->setValue(defaults->me_sub_cmp);
	form.addRow(_("sub cmp:   "), me_sub_cmp);
	/*last pred*/
	QSpinBox *last_pred = new QSpinBox(&dialog);
	last_pred->setRange(1, 3);
	last_pred->setSingleStep(1);						
	last_pred->setValue(defaults->last_pred);
	form.addRow(_("last predictor count:   "), last_pred);
	/*gop size*/
	QSpinBox *gop_size = new QSpinBox(&dialog);
	gop_size->setRange(1, 250);
	gop_size->setSingleStep(1);						
	gop_size->setValue(defaults->gop_size);
	form.addRow(_("gop size:   "), gop_size);
	/*qcompress*/
	QDoubleSpinBox *qcompress = new QDoubleSpinBox(&dialog);
	qcompress->setRange(0, 1);
	qcompress->setSingleStep(0.1);						
	qcompress->setValue(defaults->qcompress);
	form.addRow(_("qcompress:   "), qcompress);
	/*qblur*/
	QDoubleSpinBox *qblur = new QDoubleSpinBox(&dialog);
	qblur->setRange(0, 1);
	qblur->setSingleStep(0.1);						
	qblur->setValue(defaults->qblur);
	form.addRow(_("qblur:   "), qblur);
	/*subq*/
	QSpinBox *subq = new QSpinBox(&dialog);
	subq->setRange(0, 8);
	subq->setSingleStep(1);						
	subq->setValue(defaults->subq);
	form.addRow(_("subq:   "), subq);
	/*framerefs*/
	QSpinBox *framerefs = new QSpinBox(&dialog);
	framerefs->setRange(0, 12);
	framerefs->setSingleStep(1);						
	framerefs->setValue(defaults->framerefs);
	form.addRow(_("framerefs:   "), framerefs);
	/*me method*/
	QSpinBox *me_method = new QSpinBox(&dialog);
	me_method->setRange(1, 10);
	me_method->setSingleStep(1);						
	me_method->setValue(defaults->me_method);
	form.addRow(_("me method:   "), me_method);
	/*mb decision*/
	QSpinBox *mb_decision = new QSpinBox(&dialog);
	mb_decision->setRange(0, 2);
	mb_decision->setSingleStep(1);						
	mb_decision->setValue(defaults->mb_decision);
	form.addRow(_("mb decision:   "), mb_decision);
	/*max b frames*/
	QSpinBox *max_b_frames = new QSpinBox(&dialog);
	max_b_frames->setRange(0, 4);
	max_b_frames->setSingleStep(1);						
	max_b_frames->setValue(defaults->max_b_frames);
	form.addRow(_("max B frames:   "), max_b_frames);
	/*num threads*/
	QSpinBox *num_threads = new QSpinBox(&dialog);
	num_threads->setRange(0, 8);
	num_threads->setSingleStep(1);						
	num_threads->setValue(defaults->num_threads);
	form.addRow(_("num threads:   "), num_threads);

	// Add some standard buttons (Cancel/Ok) at the bottom of the dialog
	QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &dialog);
	form.addRow(&buttonBox);
	QObject::connect(&buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
	QObject::connect(&buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));

	if (dialog.exec() == QDialog::Accepted) 
	{
		if(debug_level > 1)
			std::cout << "GUVCVIEW (Qt5): setting video codec properties"
				<< std::endl;
		
		defaults->fps = enc_fps->value();
		defaults->monotonic_pts = monotonic_pts->isChecked() ? 1: 0;
		defaults->bit_rate = bit_rate->value();
		defaults->qmax = qmax->value();
		defaults->qmin = qmin->value();
		defaults->max_qdiff = max_qdiff->value();
		defaults->dia = dia->value();
		defaults->pre_dia = pre_dia->value();
		defaults->pre_me = pre_me->value();
		defaults->me_pre_cmp = me_pre_cmp->value();
		defaults->me_cmp = me_cmp->value();
		defaults->me_sub_cmp = me_sub_cmp->value();
		defaults->last_pred = last_pred->value();
		defaults->gop_size = gop_size->value();
		defaults->qcompress = qcompress->value();
		defaults->qblur = qblur->value();
		defaults->subq = subq->value();
		defaults->framerefs = framerefs->value();
		defaults->me_method = me_method->value();
		defaults->mb_decision = mb_decision->value();
		defaults->max_b_frames = max_b_frames->value();
		defaults->num_threads = num_threads->value();
    }

}

///*
 //* audio encoder properties clicked event
 //* args:
 //*    item - widget that generated the event
 //*    data - pointer to user data
 //*
 //* asserts:
 //*    none
 //*
 //* returns: none
 //*/
//void encoder_audio_properties(GtkMenuItem *item, void *data)
//{
	//int line = 0;
	//audio_codec_t *defaults = encoder_get_audio_codec_defaults(get_audio_codec_ind());

	//GtkWidget *codec_dialog = gtk_dialog_new_with_buttons (_("audio codec values"),
		//GTK_WINDOW(get_main_window_gtk3()),
		//GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		//_("_Ok"), GTK_RESPONSE_ACCEPT,
		//_("_Cancel"), GTK_RESPONSE_REJECT,
		//NULL);

	//GtkWidget *table = gtk_grid_new();
	//gtk_grid_set_column_homogeneous (GTK_GRID(table), TRUE);

	///*bit rate*/
	//GtkWidget *lbl_bit_rate = gtk_label_new(_("bit rate:   "));
//#if GTK_VER_AT_LEAST(3,15)
	//gtk_label_set_xalign(GTK_LABEL(lbl_bit_rate), 1);
	//gtk_label_set_yalign(GTK_LABEL(lbl_bit_rate), 0.5);
//#else
	//gtk_misc_set_alignment (GTK_MISC (lbl_bit_rate), 1, 0.5);
//#endif
	//gtk_grid_attach (GTK_GRID(table), lbl_bit_rate, 0, line, 1, 1);
	//gtk_widget_show (lbl_bit_rate);

	//GtkWidget *bit_rate = gtk_spin_button_new_with_range(48000,384000,8000);
	//gtk_editable_set_editable(GTK_EDITABLE(bit_rate),TRUE);
	//gtk_spin_button_set_value (GTK_SPIN_BUTTON(bit_rate), defaults->bit_rate);

	//gtk_grid_attach (GTK_GRID(table), bit_rate, 1, line, 1, 1);
	//gtk_widget_show (bit_rate);
	//line++;

	///*sample format*/
	//GtkWidget *lbl_sample_fmt = gtk_label_new(_("sample format:   "));
//#if GTK_VER_AT_LEAST(3,15)
	//gtk_label_set_xalign(GTK_LABEL(lbl_sample_fmt), 1);
	//gtk_label_set_yalign(GTK_LABEL(lbl_sample_fmt), 0.5);
//#else
	//gtk_misc_set_alignment (GTK_MISC (lbl_sample_fmt), 1, 0.5);
//#endif
	//gtk_grid_attach (GTK_GRID(table), lbl_sample_fmt, 0, line, 1, 1);
	//gtk_widget_show (lbl_sample_fmt);

	//GtkWidget *sample_fmt = gtk_spin_button_new_with_range(0, encoder_get_max_audio_sample_fmt(), 1);
	//gtk_editable_set_editable(GTK_EDITABLE(sample_fmt),TRUE);
	//gtk_spin_button_set_value (GTK_SPIN_BUTTON(sample_fmt), defaults->sample_format);

	//gtk_grid_attach (GTK_GRID(table), sample_fmt, 1, line, 1, 1);
	//gtk_widget_show (sample_fmt);
	//line++;

	//GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (codec_dialog));
	//gtk_container_add (GTK_CONTAINER (content_area), table);
	//gtk_widget_show (table);

	//gint result = gtk_dialog_run (GTK_DIALOG (codec_dialog));
	//switch (result)
	//{
		//case GTK_RESPONSE_ACCEPT:
			//defaults->bit_rate = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(bit_rate));
			//defaults->sample_format = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(sample_fmt));
			//break;
		//default:
			//// do nothing since dialog was cancelled
			//break;
	//}
	//gtk_widget_destroy (codec_dialog);
//}

///*
 //* gtk3 window key pressed event
 //* args:
 //*   win - pointer to widget (main window) where event ocurred
 //*   event - pointer to GDK key event structure
 //*   data - pointer to user data
 //*
 //* asserts:
 //*   none
 //*
 //* returns: true if we handled the event or false otherwise
 //*/
//gboolean window_key_pressed (GtkWidget *win, GdkEventKey *event, void *data)
//{
	///* If we have modifiers, and either Ctrl, Mod1 (Alt), or any
	 //* of Mod3 to Mod5 (Mod2 is num-lock...) are pressed, we
	 //* let Gtk+ handle the key */
	////printf("GUVCVIEW: key pressed (key:%i)\n", event->keyval);
	//if (event->state != 0
		//&& ((event->state & GDK_CONTROL_MASK)
		//|| (event->state & GDK_MOD1_MASK)
		//|| (event->state & GDK_MOD3_MASK)
		//|| (event->state & GDK_MOD4_MASK)
		//|| (event->state & GDK_MOD5_MASK)))
		//return FALSE;

    //if(v4l2core_has_pantilt_id())
    //{
		//int id = 0;
		//int value = 0;

        //switch (event->keyval)
        //{
            //case GDK_KEY_Down:
            //case GDK_KEY_KP_Down:
				//id = V4L2_CID_TILT_RELATIVE;
				//value = v4l2core_get_tilt_step();
				//break;

            //case GDK_KEY_Up:
            //case GDK_KEY_KP_Up:
				//id = V4L2_CID_TILT_RELATIVE;
				//value = - v4l2core_get_tilt_step();
				//break;

            //case GDK_KEY_Left:
            //case GDK_KEY_KP_Left:
				//id = V4L2_CID_PAN_RELATIVE;
				//value = v4l2core_get_pan_step();
				//break;

            //case GDK_KEY_Right:
            //case GDK_KEY_KP_Right:
                //id = V4L2_CID_PAN_RELATIVE;
				//value = - v4l2core_get_pan_step();
				//break;

            //default:
                //break;
        //}

        //if(id != 0 && value != 0)
        //{
			//v4l2_ctrl_t *control = v4l2core_get_control_by_id(id);

			//if(control)
			//{
				//control->value =  value;

				//if(v4l2core_set_control_value_by_id(id))
					//fprintf(stderr, "GUVCVIEW: error setting pan/tilt value\n");

				//return TRUE;
			//}
		//}
    //}

    //switch (event->keyval)
    //{
        //case GDK_KEY_WebCam:
			///* camera button pressed */
			//if (get_default_camera_button_action() == DEF_ACTION_IMAGE)
				//gui_click_image_capture_button_gtk3();
			//else
				//gui_click_video_capture_button_gtk3();
			//return TRUE;

		//case GDK_KEY_V:
		//case GDK_KEY_v:
			//gui_click_video_capture_button_gtk3();
			//return TRUE;

		//case GDK_KEY_I:
		//case GDK_KEY_i:
			//gui_click_image_capture_button_gtk3();
			//return TRUE;

	//}

    //return FALSE;
//}

///*
 //* device list events timer callback
 //* args:
 //*   data - pointer to user data
 //*
 //* asserts:
 //*   none
 //*
 //* returns: true if timer is to be reset or false otherwise
 //*/
//gboolean check_device_events(gpointer data)
//{
	//if(v4l2core_check_device_list_events())
	//{
		///*update device list*/
		//g_signal_handlers_block_by_func(GTK_COMBO_BOX_TEXT(get_wgtDevices_gtk3()),
                //G_CALLBACK (devices_changed), NULL);

		//GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX(get_wgtDevices_gtk3())));
		//gtk_list_store_clear(store);

		//v4l2_device_list *device_list = v4l2core_get_device_list();
		//int i = 0;
        //for(i = 0; i < (device_list->num_devices); i++)
		//{
			//gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(get_wgtDevices_gtk3()),
				//device_list->list_devices[i].name);
			//if(device_list->list_devices[i].current)
				//gtk_combo_box_set_active(GTK_COMBO_BOX(get_wgtDevices_gtk3()),i);
		//}

		//g_signal_handlers_unblock_by_func(GTK_COMBO_BOX_TEXT(get_wgtDevices_gtk3()),
                //G_CALLBACK (devices_changed), NULL);
	//}

	//return (TRUE);
//}
