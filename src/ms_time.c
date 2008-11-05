/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/


#include <time.h>
#include <sys/time.h>
#include "ms_time.h"


/*------------------------------ get time ------------------------------------*/
DWORD ms_time (void)
{
   static struct timeval tod;
   gettimeofday (&tod, NULL);
   return ((DWORD) tod.tv_sec * 1000.0 + (DWORD) tod.tv_usec / 1000.0);
};

ULLONG ns_time (void)
{
   static struct timespec ts;
   clock_gettime(CLOCK_REALTIME, &ts);
   return ((ULLONG) ts.tv_sec * 1000000000.0 + (ULLONG) ts.tv_nsec);
};
