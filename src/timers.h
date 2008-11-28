/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
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

#ifndef TIMERS_H
#define TIMERS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <SDL/SDL.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <SDL/SDL_syswm.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include "../config.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "v4l2uvc.h"
#include "globals.h"
#include "guvcview.h"
#include "callbacks.h"

gboolean
timer_callback(gpointer data);

gboolean
Image_capture_timer(gpointer data);

gboolean 
FpsCount_callback(gpointer data);

#endif
