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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "gviewv4l2core.h"

extern int debug_level;

/*
 * get the filename basename
 * args:
 *    filename - string with filename (full path)
 *
 * asserts:
 *    none
 *
 * returns: new string with basename (must free it)
 */
char *get_file_basename(const char *filename)
{
	char *name = strrchr(filename, '/');

	char *basename = NULL;

	if(name != NULL)
		basename = strdup(name + 1); /*skip '/'*/
	else
		basename = strdup(filename);

	if(debug_level > 0)
		printf("GUVCVIEW: basename for %s is %s\n", filename, basename);

	return basename;
}

/*
 * get the filename path
 * args:
 *    filename - string with filename (full path)
 *
 * asserts:
 *    none
 *
 * returns: new string with path (must free it)
 *      or NULL if no path found
 */
char *get_file_pathname(const char *filename)
{
	char *name = strrchr(filename, '/');

	char *pathname = NULL;

	if(name)
	{
		int strsize = name - filename;
		pathname = strndup(filename, strsize);
	}

	if(debug_level > 0)
		printf("GUVCVIEW: path for %s is %s\n", filename, pathname);

	return pathname;
}

/*
 * get the filename extension
 * args:
 *    filename - string with filename (full path)
 *
 * asserts:
 *    none
 *
 * returns: new string with extension (must free it)
 *      or NULL if no extension found
 */
char *get_file_extension(const char *filename)
{
	char *basename = get_file_basename(filename);

	char *name = strrchr(basename, '.');

	char *extname = NULL;

	if(name)
		extname = strdup(name + 1);

	if(debug_level > 0)
		printf("GUVCVIEW: extension for %s is %s\n", filename, extname);

	free(basename);

	return extname;
}

/*
 * change the filename extension
 * args:
 *    filename - string with filename
 *    ext - string with new extension
 *
 * asserts:
 *    none
 *
 * returns: string with new extension (must free it)
 */
char *set_file_extension(const char *filename, const char *ext)
{
	char *name = strrchr(filename, '.');

	char *new_filename = NULL;

	int strsize = strlen(filename);
	if(name)
		strsize = name - filename;

	new_filename = strndup(filename, strsize);
	new_filename = strcat(new_filename, ext);

	if(debug_level > 0)
		printf("GUVCVIEW: changed file extension to %s\n", new_filename);
	return new_filename;
}
