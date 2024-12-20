/*******************************************************************************#
#           uvc Meet4K support for OBSBOT Meet 4K                               #
#       for guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Sam Liddicott <sam@liddicott.com>                                   #
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

#include "gui_qt6.hpp"

extern "C" {
#include <assert.h>
/* support for internationalization - i18n */
#include <libintl.h>
#include <locale.h>

#include "video_capture.h"
/*add this last to avoid redefining _() and N_()*/
#include "gview.h"

#include "uvc_meet4k.h"
}

extern int debug_level;
extern int is_control_panel;

/*
 * meet4k background mode callback
 * args:
 *   index - current combobox index
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::meet4k_background_mode_changed(int index) {
  uint8_t camera_effect = (uint8_t)BackgroundMode->itemData(index).toInt() - 1;
  meet4kcore_set_camera_effect(get_v4l2_device_handler(), camera_effect);
}

/*
 * update controls from commit probe data
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void MainWindow::update_meet4k_controls() {

}


/*
 * attach meet4k controls tab widget
 * args:
 *   parent - tab parent widget
 *
 * asserts:
 *   parent is not null
 *
 * returns: error code (0 -OK)
 */
int MainWindow::gui_attach_qt6_meet4kctrls(QWidget *parent) {
  /*assertions*/
  assert(parent != NULL);

  if (debug_level > 1)
    std::cout << "GUVCVIEW (Qt5): attaching meet4k controls" << std::endl;

  QGridLayout *grid_layout = new QGridLayout();

  meet4k_controls_grid = new QWidget(parent);
  meet4k_controls_grid->setLayout(grid_layout);
  meet4k_controls_grid->show();

  int line = 0;

  QLabel *label_BackgroundMode =
      new QLabel(_("Virtual Background Mode:"), meet4k_controls_grid);
  label_BackgroundMode->show();

  BackgroundMode = new QComboBox(meet4k_controls_grid);
  BackgroundMode->show();
  grid_layout->addWidget(BackgroundMode, line, 1);

  uint8_t min_backgroundmode = 0;
  uint8_t max_backgroundmode = 3;

  if (max_backgroundmode >= 1 && min_backgroundmode < 2)
    BackgroundMode->addItem(_("OFF"), 1);
  if (max_backgroundmode >= 2 && min_backgroundmode < 3)
    BackgroundMode->addItem(_("Virtual"), 2);
  if (max_backgroundmode >= 3 && min_backgroundmode < 4)
    BackgroundMode->addItem(_("Track"), 3);

  uint8_t cur_BackgroundMode = meet4kcore_get_camera_effect(get_v4l2_device_handler());
  int BackgroundMode_index = cur_BackgroundMode;
  if(BackgroundMode_index < 0)
    BackgroundMode_index = 0;

  BackgroundMode->setCurrentIndex(BackgroundMode_index);

  // connect signal
  connect(BackgroundMode, SIGNAL(currentIndexChanged(int)), this,
          SLOT(meet4k_background_mode_changed(int)));

  return 0;
}
