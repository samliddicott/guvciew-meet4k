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

#ifndef GUI_QT5_HPP
#define GUI_QT5_HPP

#include <QtWidgets>

extern "C" {
#include "gviewv4l2core.h"
}

class ControlWidgets
{
public:
	ControlWidgets();
	
	int id;                       /*control id*/
	QWidget *label;               /*control label widget*/
	QWidget *widget;              /*control widget 1*/
	QWidget *widget2;             /*control widget 2*/
};

class AudioWidgets
{
public:
	AudioWidgets();
	
	QWidget *api;           /* api control      */
	QWidget *device;        /* device control   */
	QWidget *channels;      /* channels control */
	QWidget *samprate;     /* samprate control */
	QWidget *latency;       /* latency control  */
};



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();
	std::vector<ControlWidgets *> control_widgets_list;

protected:
   void closeEvent(QCloseEvent *event);

private slots:
	void quit_button_clicked();
	void slider_value_changed(int value);
	void spin_value_changed (int value);
	void button_PanTilt1_clicked();
	void button_PanTilt2_clicked();
	void button_clicked();
	void pan_tilt_step_changed(int value);
	void combo_changed (int index);
	void bayer_pix_ord_changed(int index);
	void check_changed (int state);
	void autofocus_changed(int state);
	void setfocus_clicked();
#ifdef V4L2_CTRL_TYPE_INTEGER64
	void int64_button_clicked();
#endif
    void devices_changed (int index);
    void frame_rate_changed (int index);
    void resolution_changed (int index);
    void format_changed(int index);
    void render_fx_filter_changed(int state);
    void render_osd_changed(int state);
    void control_defaults_clicked ();
    void load_save_profile_clicked();
    void menu_camera_button_clicked();
    void photo_file_clicked();
    void photo_sufix_clicked();
    void video_file_clicked();
    void video_sufix_clicked();
    void video_codec_clicked();
    void video_codec_properties();
    void audio_codec_clicked();
    void audio_codec_properties();


private:
   ControlWidgets *gui_qt5_get_widgets_by_id(int id);
   void gui_qt5_update_controls_state();
   int gui_attach_qt5_v4l2ctrls(QWidget *parent);
   int gui_attach_qt5_videoctrls(QWidget *parent);
   int gui_attach_qt5_menu(QWidget *parent);

   QWidget *img_controls_grid;
   QWidget *video_controls_grid;
   
   QComboBox *combobox_video_devices;
   QComboBox *combobox_FrameRate;
   QComboBox *combobox_resolution;
   QComboBox *combobox_InpType;
   
   QMenuBar *menubar;
   
   QAction *webm_vcodec_action = NULL;
   QAction *webm_acodec_action = NULL;
};

#endif
