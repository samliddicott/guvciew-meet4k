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

#include <QApplication>
#include <iostream>
#include <string>

#include "gui_qt5.hpp"

extern "C"{
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "video_capture.h"
#include "gui_qt5.h"
#include "gui.h"
/*add this last to avoid redefining _() and N_()*/
#include "gview.h"
#include "gviewencoder.h"
#include "gviewv4l2core.h"
}

extern int debug_level;

ControlWidgets::ControlWidgets()
{
	id = -1;
	label = NULL;
	widget = NULL;
	widget2 = NULL;
}

AudioWidgets::AudioWidgets()
{
	api = NULL;
	device = NULL;
	channels = NULL;
	samprate = NULL;
	latency = NULL;
}

MainWindow::MainWindow()
{
	QWidget *widget = new QWidget;
	widget->show();
    setCentralWidget(widget);
    
    QVBoxLayout *layout = new QVBoxLayout;
    widget->setLayout(layout);
    
    /*-----menu-----*/
    gui_attach_qt5_menu(this);
    setMenuBar(menubar);
    
    /*-----Tabs-----*/
	QTabWidget *control_tab = new QTabWidget;
	layout->addWidget(control_tab);
	
	QScrollArea *scroll_ctrls = new QScrollArea(control_tab);
	
	gui_attach_qt5_v4l2ctrls(scroll_ctrls);
	scroll_ctrls->setWidget(img_controls_grid);
	scroll_ctrls->setWidgetResizable(true);
	
	control_tab->addTab(scroll_ctrls, "Image Controls");
	
	/*control panel mode exclusions */
	if(!is_control_panel)
	{
		QScrollArea *scroll_video = new QScrollArea(control_tab);
	
		gui_attach_qt5_videoctrls(scroll_video);
		scroll_video->setWidget(video_controls_grid);
		scroll_video->setWidgetResizable(true);
	
		control_tab->addTab(scroll_video, "Video Controls");
	}
}

MainWindow::~MainWindow()
{
	if(debug_level > 1)
		std::cout <<"GUVCVIEW (Qt5): cleaning MainWindow" << std::endl;
	for (std::vector<ControlWidgets *>::iterator it = control_widgets_list.begin() ; it != control_widgets_list.end(); ++it)
		delete(*it);
	control_widgets_list.clear();
}

/******************************* C wrapper functions ********************************/

QApplication *my_app = NULL;
MainWindow *mainWin = NULL;

/*we need global argc and argv for QApplication*/
int argc = 1;
QString argv1 = "guvcview";
char *argv = (char *) argv1.toStdString().c_str();

/*
 * GUI warning/error dialog
 * args:
 *   title - dialog title string
 *   message - error message string
 *   fatal - flag a fatal error (display device list combo box)
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void gui_error_qt5(
	const char *title,
	const char *message,
	int fatal)
{
	if(!QCoreApplication::instance())
	{
		my_app = new QApplication(argc, &argv);
		if(!my_app)
		{
			std::cerr <<"GUVCVIEW (Qt5): app creation failed" << std::endl;
			return;
		}
		else if(debug_level > 1)
			std::cout <<"GUVCVIEW (Qt5): app created" << std::endl;
	
		my_app->setOrganizationName("Guvcview Video Capture");
		my_app->setApplicationName("Guvcview");
	}
	
	/*simple warning message - not fatal and no device selection*/
	if(!fatal)
	{
		QMessageBox::warning(mainWin, gettext(title),
                             gettext(message),
                             QMessageBox::Ok,
                             QMessageBox::Ok);
		return;
	}

	/*fatal error message*/
	
	v4l2_device_list *device_list = v4l2core_get_device_list();
	/*add device list (more than one device)*/
	int show_dev_list = (device_list->num_devices >= 1) ? 1: 0;

	std::cerr << "GUVCVIEW (Qt5): fatal error (" << device_list->num_devices << " devices detected)" << std::endl;
	
	if(show_dev_list)
	{
		std::cerr << "GUVCVIEW (Qt5): creating input dialog" << std::endl;
		
		QStringList items;
		int i = 0;
		for(i = 0; i < (device_list->num_devices); i++)
		{
			items << device_list->list_devices[i].name;
		}
		bool ok;
		
		QString dialog_text = gettext(message);
		dialog_text.append(_("\nYou seem to have video devices installed.\n"
							 "Do you want to try one ?\n"));
		QString item = QInputDialog::getItem(mainWin, gettext(title), dialog_text, 
			items, device_list->num_devices - 1, 
			false, &ok);
			
		if (ok && !item.isEmpty())
		{
			QStringList args;
			QString dev_arg = "--device=";
			int i = 0;
			foreach (const QString &str, items) 
			{
				if(str == item)
					break;
				i++;
			}
			
			dev_arg.append(device_list->list_devices[i].device);
			args << dev_arg;
			QProcess process;
			process.startDetached("guvcview", args);
		}
		
	}	
	else
	{
		std::cerr << "GUVCVIEW (Qt5): creating error dialog" << std::endl;
		QMessageBox::critical(mainWin, gettext(title),
                               gettext(message),
                               QMessageBox::Ok,
                               QMessageBox::Ok);
	}

	quit_callback(NULL); /*terminate the program*/
}


/*
 * GUI initialization
 * args:
 *   width - window width
 *   height - window height
 *
 * asserts:
 *   none
 *
 * returns: error code (0 -OK)
 */
int gui_attach_qt5(int width, int height)
{
	if(debug_level > 1)
		std::cout << "GUVCVIEW (Qt5): attaching GUI\n" << std::endl;
	
	//Q_INIT_RESOURCE(application);
	if(!QCoreApplication::instance())
	{
		my_app = new QApplication(argc, &argv);
		if(!my_app)
		{
			std::cerr <<"GUVCVIEW (Qt5): app creation failed" << std::endl;
			return -1;
		}
		else if(debug_level > 1)
			std::cout <<"GUVCVIEW (Qt5): app created" << std::endl;
	
		my_app->setOrganizationName("Guvcview Video Capture");
		my_app->setApplicationName("Guvcview");
	}

	if(debug_level > 2)
		std::cout <<"GUVCVIEW (Qt5): creating window" << std::endl;
	mainWin = new MainWindow();
	mainWin->resize(width, height);
    mainWin->show();

	return 0;

}

/*
 * run the GUI loop
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int gui_run_qt5()
{
	if(debug_level > 2)
		std::cout <<"GUVCVIEW (Qt5): executing GUI app" << std::endl;
	if(my_app)
		return my_app->exec();
	else
	{
		std::cout <<"GUVCVIEW (Qt5): application not created" << std::endl;
		return -1;
	}
}

/*
 * closes and cleans the GUI
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void gui_close_qt5()
{
	//my_app->closeAllWindows();
	if(debug_level > 1)
		std::cerr << "GUVCVIEW (Qt5): closing Gui" << std::endl;
	
	if(my_app)
		my_app->quit();
	
	delete(mainWin);

	if(debug_level > 2)
		std::cerr << "GUVCVIEW (Qt5): all done" << std::endl;
}

/*
 * set webm codecs in codecs list
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_webm_codecs_qt5()
{
	/*force webm codecs*/
	int video_codec_ind = encoder_get_webm_video_codec_index();
	set_video_codec_ind(video_codec_ind);
	int audio_codec_ind = encoder_get_webm_audio_codec_index();
	set_audio_codec_ind(audio_codec_ind);
}
