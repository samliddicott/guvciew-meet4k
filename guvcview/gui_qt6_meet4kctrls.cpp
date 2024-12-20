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
 * meet4K camera angle mode callback
 * args:
 *   index - current combobox index
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::meet4k_camera_angle_changed(int index) {
  uint8_t camera_angle = (uint8_t)CameraAngle->itemData(index).toInt() - 1;
  meet4kcore_set_camera_angle(get_v4l2_device_handler(), camera_angle);
}

/*
 * meet4K camera background callback
 * args:
 *   index - current combobox index
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::meet4k_background_changed(int index) {
  uint8_t background = (uint8_t)Background->itemData(index).toInt() - 1;

  switch (background) {
    case 0: background = 0x01; break;
    case 1: background = 0x11; break;
    case 2: background = 0x12; break;
    default:   background = 0x01;
  }

  meet4kcore_set_bg_mode(get_v4l2_device_handler(), background);

  if (1 != (uint8_t)BackgroundMode->currentIndex())
    BackgroundMode->setCurrentIndex(1);
}

/*
 * meet4k blur level callback
 * args:
 *   index - current slider something
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::meet4k_blur_level_changed(int value) {
  meet4kcore_set_blur_level(get_v4l2_device_handler(), value);

  if (1 != (uint8_t)BackgroundMode->currentIndex())
    BackgroundMode->setCurrentIndex(1);
  if (2 != (uint8_t)Background->currentIndex())
    Background->setCurrentIndex(2);
}

/*
 * meet4K camera background colour callback
 * args:
 *   index - current combobox index
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::meet4k_ColorBG_changed(int index) {
  uint8_t color = (uint8_t)ColorBG->itemData(index).toInt() - 1;
  meet4kcore_set_bg_color(get_v4l2_device_handler(), color);

  if (0 != (uint8_t)Background->currentIndex())
    Background->setCurrentIndex(0);
  if (1 != (uint8_t)BackgroundMode->currentIndex())
    BackgroundMode->setCurrentIndex(1);
}

/*
 * meet4K hdr mode callback
 * args:
 *   value - current value
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::meet4k_hdr_mode_changed(int value)
{
	uint8_t hdr_mode = value;
	meet4kcore_set_hdr_mode(get_v4l2_device_handler(), hdr_mode);
}

/*
 * meet4K face ae mode callback
 * args:
 *   value - current value
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::meet4k_ae_mode_changed(int value)
{
	uint8_t ae_mode = value;
	meet4kcore_set_face_ae_mode(get_v4l2_device_handler(), ae_mode);
}

/*
 * meet4K button rotate mode callback
 * args:
 *   value - current value
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::meet4k_br_mode_changed(int value)
{
	uint8_t br_mode = value;
	meet4kcore_set_button_mode(get_v4l2_device_handler(), br_mode);
}

/*
 * meet4K nr mode callback
 * args:
 *   value - current value
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void MainWindow::meet4k_nr_mode_changed(int value)
{
	uint8_t nr_mode = value;
	meet4kcore_set_hdr_mode(get_v4l2_device_handler(), nr_mode);
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

  /* Background mode (camera effect) */
  line++;

  QLabel *label_BackgroundMode =
      new QLabel(_("Virtual Background Mode:"), meet4k_controls_grid);
  label_BackgroundMode->show();
  grid_layout->addWidget(label_BackgroundMode, line, 0, Qt::AlignRight);

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

  connect(BackgroundMode, SIGNAL(currentIndexChanged(int)), this,
          SLOT(meet4k_background_mode_changed(int)));

  /* Camera angle */
  line++;

  QLabel *label_CameraAngle =
      new QLabel(_("Camera Angle:"), meet4k_controls_grid);
  label_CameraAngle->show();
  grid_layout->addWidget(label_CameraAngle, line, 0, Qt::AlignRight);

  uint8_t min_CameraAngle = 0;
  uint8_t max_CameraAngle = 3;

  CameraAngle = new QComboBox(meet4k_controls_grid);
  CameraAngle->show();
  grid_layout->addWidget(CameraAngle, line, 1);

  if (max_CameraAngle >= 1 && min_CameraAngle < 2)
    CameraAngle->addItem(_("86 degrees"), 1);
  if (max_CameraAngle >= 2 && min_CameraAngle < 3)
    CameraAngle->addItem(_("78 degrees"), 2);
  if (max_CameraAngle >= 3 && min_CameraAngle < 4)
    CameraAngle->addItem(_("65 degrees"), 3);

  uint8_t cur_CameraAngle = meet4kcore_get_camera_angle(get_v4l2_device_handler());
  int CameraAngle_index = cur_CameraAngle;
  if(CameraAngle_index < 0)
    CameraAngle_index = 0;

  CameraAngle->setCurrentIndex(CameraAngle_index);

  connect(CameraAngle, SIGNAL(currentIndexChanged(int)), this,
          SLOT(meet4k_camera_angle_changed(int)));

  /* Background */
  line++;

  QLabel *label_Background =
      new QLabel(_("Background:"), meet4k_controls_grid);
  label_Background->show();
  grid_layout->addWidget(label_Background, line, 0, Qt::AlignRight);

  uint8_t min_Background = 0;
  uint8_t max_Background = 3;

  Background = new QComboBox(meet4k_controls_grid);
  Background->show();
  grid_layout->addWidget(Background, line, 1);

  if (max_Background >= 1 && min_Background < 2)
    Background->addItem(_("Color"), 1);
  if (max_Background >= 2 && min_Background < 3)
    Background->addItem(_("Image"), 2);
  if (max_Background >= 3 && min_Background < 4)
    Background->addItem(_("Blur"), 3);

  char cur_Background = meet4kcore_get_bg_mode(get_v4l2_device_handler());
  int Background_index = 0;
  switch (cur_Background) {
    case 0x01: Background_index = 0; break;
    case 0x11: Background_index = 1; break;
    case 0x12: Background_index = 2; break;
    default: Background_index = 0;
  }
  Background->setCurrentIndex(Background_index);

  connect(Background, SIGNAL(currentIndexChanged(int)), this,
          SLOT(meet4k_background_changed(int)));

  /* Blur level */
  line++;

  QLabel *label_Blur =
      new QLabel(_("Blur:"), meet4k_controls_grid);
  label_Blur->show();
  grid_layout->addWidget(label_Blur, line, 0, Qt::AlignRight);

  uint8_t blur_level = meet4kcore_get_blur_level(get_v4l2_device_handler());

  uint8_t min_Blur = 0;
  uint8_t max_Blur = 64;

  Slider = new QSlider(Qt::Horizontal, img_controls_grid);
  Slider->setRange(min_Blur, max_Blur);
  Slider->setSingleStep(1);
  Slider->setPageStep(10);
  Slider->setValue(blur_level);

  Slider->show();
  grid_layout->addWidget(Slider, line, 1);

  connect(Slider,SIGNAL(valueChanged(int)),this,
          SLOT(meet4k_blur_level_changed(int)));

  /* Background color */
  line++;

  QLabel *label_ColorBG =
      new QLabel(_("BG Color:"), meet4k_controls_grid);
  label_ColorBG->show();
  grid_layout->addWidget(label_ColorBG, line, 0, Qt::AlignRight);

  uint8_t min_ColorBG = 0;
  uint8_t max_ColorBG = 5;

  ColorBG = new QComboBox(meet4k_controls_grid);
  ColorBG->show();
  grid_layout->addWidget(ColorBG, line, 1);

  if (max_ColorBG >= 1 && min_ColorBG < 2)
    ColorBG->addItem(_("Blue"), 1);
  if (max_ColorBG >= 2 && min_ColorBG < 3)
    ColorBG->addItem(_("Green"), 2);
  if (max_ColorBG >= 3 && min_ColorBG < 4)
    ColorBG->addItem(_("Red"), 3);
  if (max_ColorBG >= 4 && min_ColorBG < 5)
    ColorBG->addItem(_("Black"), 4);
  if (max_ColorBG >= 5 && min_ColorBG < 6)
    ColorBG->addItem(_("White"), 5);

  uint8_t cur_ColorBG = meet4kcore_get_bg_color(get_v4l2_device_handler());
  int ColorBG_index = cur_ColorBG;
  if(ColorBG_index < 0)
    ColorBG_index = 0;

  ColorBG->setCurrentIndex(ColorBG_index);

  connect(ColorBG, SIGNAL(currentIndexChanged(int)), this,
          SLOT(meet4k_ColorBG_changed(int)));

  /* simple boolean group */
  line++;

  QWidget *table_extra = new QWidget(meet4k_controls_grid);
  QGridLayout *extra_layout = new QGridLayout();
  table_extra->setLayout(extra_layout);
  table_extra->show();
  grid_layout->addWidget(table_extra, line, 0, 1, 2);

  /* HDR Mode */
  QCheckBox *HDR_enable = new QCheckBox(_("HDR Mode"), table_extra);
  HDR_enable->show();
  extra_layout->addWidget(HDR_enable, 0, 0);
  HDR_enable->setChecked(meet4kcore_get_hdr_mode(get_v4l2_device_handler()));
  /*connect signal*/
  connect(HDR_enable, SIGNAL(stateChanged(int)), this,
          SLOT(meet4k_hdr_mode_changed(int)));

  /* AE Mode */
  QCheckBox *AE_enable = new QCheckBox(_("Face Enhance"), table_extra);
  AE_enable->show();
  extra_layout->addWidget(AE_enable, 0, 1);
  AE_enable->setChecked(meet4kcore_get_face_ae_mode(get_v4l2_device_handler()));
  /*connect signal*/
  connect(AE_enable, SIGNAL(stateChanged(int)), this,
          SLOT(meet4k_ae_mode_changed(int)));

  /* Button Mode */
  QCheckBox *BR_enable = new QCheckBox(_("Button Rotate"), table_extra);
  BR_enable->show();
  extra_layout->addWidget(BR_enable, 0, 2);
  BR_enable->setChecked(meet4kcore_get_button_mode(get_v4l2_device_handler()));
  /*connect signal*/
  connect(BR_enable, SIGNAL(stateChanged(int)), this,
          SLOT(meet4k_br_mode_changed(int)));

  /* NR */
  QCheckBox *NR_enable = new QCheckBox(_("Noise Reduction (audio)"), table_extra);
  NR_enable->show();
  extra_layout->addWidget(NR_enable, 0, 3);
  NR_enable->setChecked(meet4kcore_get_nr_mode(get_v4l2_device_handler()));
  /*connect signal*/
  connect(NR_enable, SIGNAL(stateChanged(int)), this,
          SLOT(meet4k_nr_mode_changed(int)));


  return 0;
}
