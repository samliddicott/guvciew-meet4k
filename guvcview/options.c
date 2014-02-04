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
#include <getopt.h>

#include "gview.h"
#include "../config.h"

typedef struct _opt_values_t
{
	char opt_short;
	char[20] opt_long;
	int  req_arg;
	char[20] opt_help_arg;
	char[80] opt_help;
} opt_values_t;

static opt_values_t opt_values[] =
{
	{
		.opt_short = 'h',
		.opt_long = "help",
		.req_arg = 0,
		.opt_help_arg = "";
		.opt_help = N_("Print help");
	},
	{
		.opt_short = 'v',
		.opt_long = "version",
		.req_arg = 0,
		.opt_help_arg = "";
		.opt_help = N_("Print version");
	},
	{
		.opt_short = 'w',
		.opt_long = "verbosity",
		.req_arg = 1,
		.opt_help_arg = N_("LEVEL");
		.opt_help = N_("Set Verbosity level (def: 0)");
	},
	{
		.opt_short = 'd',
		.opt_long = "device",
		.req_arg = 1,
		.opt_help_arg = N_("DEVICE");
		.opt_help = N_("Set device name (def: /dev/video0)");
	},
	{
		.opt_short = 'x',
		.opt_long = "resolution",
		.req_arg = 1,
		.opt_help_arg = N_("WIDTHxHEIGHT");
		.opt_help = N_("Request resolution (e.g 640x480)");
	},
	{
		.opt_short = 'f',
		.opt_long = "format",
		.req_arg = 1,
		.opt_help_arg = N_("FOURCC");
		.opt_help = N_("Request format (e.g MJPG)");
	},
	{
		.opt_short = 0,
		.opt_long = "",
		.req_arg = 0,
		.opt_help_arg = "";
		.opt_help = "";
	},
}

static int  opt_verbosity = -1;
static char opt_device[30] = "";
static int  opt_width  = -1;
static int  opt_height = -1;
static char opt_format[5] = "";


/*
 * prints the number of command line options
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: last valid index of opt_values
 */
int opt_get_number()
{
	int i = 0;

	/*long option must always be set*/
	do
	{
		i++;
	}
	while(strlen(opt_values[i].opt_long) > 0)

	return i;
}

/*
 * prints the command line help
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void opt_print_help()
{
	printf(_("Guvcview version %s\n\n"), VERSION);
	printf(_("Usage:\n   guvcview [OPTIONS]\n\n"));
	printf(_("OPTIONS:\n"));

	int i = 0;

	/*long option must always be set*/
	do
	{
		if(opt_values[i].opt_short > 0)
			printf("-%c", opt_values[i].opt_short);

		printf("--%s", opt_values[i].opt_long);

		if(strlen(opt_values[i].opt_help_arg) > 0)
			printf("=%s", _(opt_values[i].opt_help_arg));

		if(strlen(opt_values[i].opt_help) > 0)
			printf("\t%s\n", _(opt_values[i].opt_help));

		i++;
	}
	while(strlen(opt_values[i].opt_long) > 0)
}

/*
 * prints the version info
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void opt_print_version()
{
	printf("Guvcview version %s\n", VERSION);
}

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
int options_parse(int argc, char *argv[])
{
	int ret = 0;
	int long_index =0;
	char* stopstring = NULL;

	int n_options = opt_get_number();

	struct option long_options[n_options + 1];

	char opt_string[128] = "";
	char *opt_str_ptr = opt_string;

	int i =0;
	for(i=0; i < n_options; i++)
	{
		long_options[i] =
		{
			opt_values[i].opt_long,
			(opt_values[i].req_arg > 0 ? required_argument: no_argument),
			0,
			opt_values[i].opt_short}
		};

		/*set opt string (be carefull we don't exceed size)*/
		if(opt_str_ptr - opt_string < 128 - 3)
		{
			*opt_str_ptr++ = opt_values[i].opt_short;
			if(opt_values[i].req_arg > 0)
				*opt_str_ptr++ = ':';
		}
	}

	long_options[n_options] = {0, 0, 0, 0};

	*opt_str_ptr++='\0'; /*null terminated string*/

	for
	while ((opt = getopt_long(argc, argv, opt_string,
		long_options, &long_index )) != -1)
	{
		switch (opt)
		{
            case 'v' :
				opt_print_version();
				ret = 1;
				break;

			case 'w':
				opt_verbosity = atoi(optarg);
				break;

			case 'd':
			{
				int str_size = strlen(optarg);
				if(str_size > 1) /*device needs at least 2 chars*/
					strncpy(opt_device, optarg, 29);
				else
					fprintf(stderr, "V4L2_CORE: (options) Error in device usage: -d[--device] DEVICENAME \n");
				break;
			}
			case 'x':
				opt_width = (int) strtoul(optarg, stopstring, 10);
				if( stopstring[0] != 'x')
				{
					fprintf(stderr, "V4L2_CORE: (options) Error in resolution usage: -x[--resolution] WIDTHxHEIGHT \n");
				}
				else
				{
					++stopstring;
					opt_height = (int) strtoul(optarg, stopstring, 10);
				}
				if(opt_width <= 0)
					opt_width = -1;
				if(opt_height <= 0)
					opt_height = -1;
				break;

			case 'f':
			{
				int str_size = strlen(optarg);
				if(str_size == 4) /*fourcc is 4 chars*/
					strncpy(opt_format, optarg, 4);
				break;
			}
			default:
            case 'h':
				opt_print_help();
				ret = 1;
				break;
        }
    }

    return ret;
}