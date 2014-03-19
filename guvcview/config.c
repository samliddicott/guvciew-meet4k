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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

#include "gviewv4l2core.h"
#include "gview.h"
#include "core_io.h"
#include "gui.h"
#include "options.h"
#include "config.h"
#include "../config.h"

#define MAXLINE 100 /*100 char lines max*/

static config_t my_config =
{
	.width = 640,
	.height = 480,
	.format = "MJPG",
	.render = "sdl1",
	.gui = "gtk3",
	.audio = "port",
	.capture = "mmap",
};

/*
 * get the internal config data
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: pointer to internal config_t struct
 */
config_t *config_get()
{
	return &my_config;
}

/*
 * save options to config file
 * args:
 *    filename - config file
 *
 * asserts:
 *    none
 *
 * returns: error code
 */
int config_save(const char *filename)
{
	FILE *fp;

	/*open file for write*/
	if((fp = fopen(filename,"w")) == NULL)
	{
		fprintf(stderr, "GUVCVIEW: couldn't open %s for write: %s\n", filename, strerror(errno));
		return -1;
	}

	/* use c locale - make sure floats are writen with a "." and not a "," */
    setlocale(LC_NUMERIC, "C");

	/*write config data*/
	fprintf(fp, "#Guvcview %s config file\n", VERSION);
	fprintf(fp, "\n");
	fprintf(fp, "#video input width\n");
	fprintf(fp, "width=%i\n", my_config.width);
	fprintf(fp, "#video input height\n");
	fprintf(fp, "height=%i\n", my_config.height);
	fprintf(fp, "#video input format\n");
	fprintf(fp, "format=%s\n", my_config.format);
	fprintf(fp, "#video input capture method\n");
	fprintf(fp, "capture=%s\n", my_config.capture);
	fprintf(fp, "#audio api\n");
	fprintf(fp, "audio=%s\n", my_config.audio);
	fprintf(fp, "#gui api\n");
	fprintf(fp, "gui=%s\n", my_config.gui);
	fprintf(fp, "#render api\n");
	fprintf(fp, "render=%s\n", my_config.render);

	/* return to system locale */
    setlocale(LC_NUMERIC, "");

	/* flush stream buffers to filesystem */
	fflush(fp);

	/* close file after fsync (sync file data to disk) */
	if (fsync(fileno(fp)) || fclose(fp))
	{
		fprintf(stderr, "GUVCVIEW: error writing configuration data to file: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

/*
 * load options from config file
 * args:
 *    filename - config file
 *
 * asserts:
 *    none
 *
 * returns: error code
 */
int config_load(const char *filename)
{
	FILE *fp;
	char bufr[MAXLINE];
	int line = 0;

	/*open file for read*/
	if((fp = fopen(filename,"r")) == NULL)
	{
		fprintf(stderr, "GUVCVIEW: couldn't open %s for read: %s\n", filename, strerror(errno));
		return -1;
	}

	while(fgets(bufr, MAXLINE, fp) != NULL)
	{
		line++;
		char *bufp = bufr;
		/*parse config line*/

		/*trim leading spaces*/
		trim_leading_wspaces(bufp);

		/*skip empty or commented lines */
		int size = strlen(bufp);
		if(size < 1 || *bufp == '#')
			continue;

		char *token = NULL;
		char *value = NULL;

		char *sp = strrchr(bufp, '=');

		if(sp)
		{
			int size = sp - bufp;
			token = strndup(bufp, size);
			trim_leading_wspaces(token);
			trim_trailing_wspaces(token);

			value = strdup(sp + 1);
			trim_leading_wspaces(value);
			trim_trailing_wspaces(value);
		}

		/*skip invalid lines */
		if(!token || !value || strlen(token) < 1 || strlen(value) < 1)
		{
			fprintf(stderr, "GUVCVIEW: (config) skiping invalid config entry at line %i\n", line);
			if(token)
				free(token);
			if(value)
				free(value);
			continue;
		}

		/*check tokens*/
		if(strcmp(token, "width") == 0)
			my_config.width = (int) strtoul(value, NULL, 10);
		else if(strcmp(token, "height") == 0)
			my_config.height = (int) strtoul(value, NULL, 10);
		else if(strcmp(token, "format") == 0)
			strncpy(my_config.format, value, 4);
		else if(strcmp(token, "capture") == 0)
			strncpy(my_config.capture, value, 4);
		else if(strcmp(token, "audio") == 0)
			strncpy(my_config.audio, value, 5);
		else if(strcmp(token, "gui") == 0)
			strncpy(my_config.gui, value, 4);
		else if(strcmp(token, "render") == 0)
			strncpy(my_config.render, value, 4);


		if(token)
			free(token);
		if(value)
			free(value);

	}

	if(errno)
	{
		fprintf(stderr, "GUVCVIEW: couldn't read line %i of config file: %s", line, strerror(errno));
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}