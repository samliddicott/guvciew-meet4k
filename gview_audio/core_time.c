/******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net              #
#                                                                              #
#           Paulo Assis <pj.assis@gmail.com>                                   #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; either version 2 of the License, or            #
# (at your option) any later version.                                          #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "core_time.h"
#include "gview.h"

/*
 * monotonic time in nanoseconds
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: monotonic time in nanoseconds
 */
uint64_t ns_time_monotonic() {
  struct timespec now;

  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    fprintf(stderr, "V4L2_CORE: ns_time_monotonic (clock_gettime) error: %s\n",
            strerror(errno));
    return 0;
  }

  return ((uint64_t)now.tv_sec * NSEC_PER_SEC + (uint64_t)now.tv_nsec);
}
