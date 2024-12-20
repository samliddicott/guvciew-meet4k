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
}

extern int debug_level;
extern int is_control_panel;

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

//  int line = 0;

  return 0;
}
