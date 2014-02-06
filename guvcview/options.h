/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
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

#ifndef OPTIONS_H
#define OPTIONS_H

typedef struct _options_t
{
	int  verbosity;  /*verbosity level*/
	char device[30]; /*device name*/
	int  width;      /*width*/
	int  height;     /*height*/
	char format[5];  /*pixelformat fourcc*/
	char render[5];  /*render api*/
	char gui[5];     /*gui api*/
} options_t;

/*
 * get the internal options data
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: pointer to internal options_t struct
 */
options_t *options_get();

/*
 * parses the command line options
 * args:
 *   argc - number of comman line args
 *   argv - pointer to list of command line args
 *
 * asserts:
 *   none
 *
 * returns: int (if > 0 app should terminate (help, version)
 */
int options_parse(int argc, char *argv[]);

#endif
