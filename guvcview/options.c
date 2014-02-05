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
#include "options.h"
#include "../config.h"

typedef struct _opt_values_t
{
	char opt_short;
	char opt_long[20];
	int  req_arg;
	char opt_help_arg[20];
	char opt_help[80];
} opt_values_t;

static opt_values_t opt_values[] =
{
	{
		.opt_short = 'h',
		.opt_long = "help",
		.req_arg = 0,
		.opt_help_arg = "",
		.opt_help = N_("Print help")
	},
	{
		.opt_short = 'v',
		.opt_long = "version",
		.req_arg = 0,
		.opt_help_arg = "",
		.opt_help = N_("Print version"),
	},
	{
		.opt_short = 'w',
		.opt_long = "verbosity",
		.req_arg = 1,
		.opt_help_arg = N_("LEVEL"),
		.opt_help = N_("Set Verbosity level (def: 0)")
	},
	{
		.opt_short = 'd',
		.opt_long = "device",
		.req_arg = 1,
		.opt_help_arg = N_("DEVICE"),
		.opt_help = N_("Set device name (def: /dev/video0)"),
	},
	{
		.opt_short = 'x',
		.opt_long = "resolution",
		.req_arg = 1,
		.opt_help_arg = N_("WIDTHxHEIGHT"),
		.opt_help = N_("Request resolution (e.g 640x480)")
	},
	{
		.opt_short = 'f',
		.opt_long = "format",
		.req_arg = 1,
		.opt_help_arg = N_("FOURCC"),
		.opt_help = N_("Request format (e.g MJPG)")
	},
	{
		.opt_short = 0,
		.opt_long = "",
		.req_arg = 0,
		.opt_help_arg = "",
		.opt_help = ""
	},
};

static options_t my_options =
{
	.verbosity = 0,
	.device = "/dev/video0",
	.width = 640,
	.height = 480,
	.format = "MJPG"
};

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
options_t *options_get()
{
	return &my_options;
}

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
	while(strlen(opt_values[i].opt_long) > 0);

	return i;
}

/*
 * gets the max length of help string (up to end of opt_help_arg)
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: max lenght
 */
static int opt_get_help_max_len()
{
	int i = 0;

	int max_len = 0;
	int len = 0;

	/*long option must always be set*/
	do
	{
		len = 5 + /*-c, and --*/
			  strlen(opt_values[i].opt_long);
		if(strlen(opt_values[i].opt_help_arg) > 0)
			len += strlen(opt_values[i].opt_help_arg) + 1; /*add =*/
		if(len > max_len)
				max_len = len;
		i++;
	}
	while(strlen(opt_values[i].opt_long) > 0);


	return max_len;
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

	int max_len = opt_get_help_max_len();
	int len = 0;

	int i = 0;

	/*long option must always be set*/
	do
	{
		if(opt_values[i].opt_short > 0)
		{
			len = 3;
			printf("-%c,", opt_values[i].opt_short);
		}

		printf("--%s", opt_values[i].opt_long);
		len += strlen(opt_values[i].opt_long) + 2;

		if(strlen(opt_values[i].opt_help_arg) > 0)
		{
			len += strlen(opt_values[i].opt_help_arg) + 1;
			printf("=%s", _(opt_values[i].opt_help_arg));
		}

		int spaces = max_len - len;
		int j = 0;
		for(j=0; j < spaces; j++)
			printf(" ");

		if(strlen(opt_values[i].opt_help) > 0)
			printf("\t:%s\n", _(opt_values[i].opt_help));

		i++;
	}
	while(strlen(opt_values[i].opt_long) > 0);
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
	char *stopstring;

	int n_options = opt_get_number();

	struct option long_options[n_options + 1];

	char opt_string[128] = "";
	char *opt_str_ptr = opt_string;

	int i =0;
	for(i=0; i < n_options; i++)
	{
		long_options[i].name = opt_values[i].opt_long;
		long_options[i].has_arg = opt_values[i].req_arg > 0 ? required_argument: no_argument;
		long_options[i].flag = NULL;
		long_options[i].val = opt_values[i].opt_short;

		/*set opt string (be carefull we don't exceed size)*/
		if(opt_str_ptr - opt_string < 128 - 3)
		{
			*opt_str_ptr++ = opt_values[i].opt_short;
			if(opt_values[i].req_arg > 0)
				*opt_str_ptr++ = ':';
		}
	}

	long_options[n_options].name = 0;
	long_options[n_options].has_arg = 0;
	long_options[n_options].flag = NULL;
	long_options[n_options].val= 0;

	*opt_str_ptr++='\0'; /*null terminated string*/

	char opt = 0;

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
				my_options.verbosity = atoi(optarg);
				break;

			case 'd':
			{
				int str_size = strlen(optarg);
				if(str_size > 1) /*device needs at least 2 chars*/
					strncpy(my_options.device, optarg, 29);
				else
					fprintf(stderr, "V4L2_CORE: (options) Error in device usage: -d[--device] DEVICENAME \n");
				break;
			}
			case 'x':
				my_options.width = (int) strtoul(optarg, &stopstring, 10);
				if( *stopstring != 'x')
				{
					fprintf(stderr, "V4L2_CORE: (options) Error in resolution usage: -x[--resolution] WIDTHxHEIGHT \n");
				}
				else
				{
					++stopstring;
					my_options.height = (int) strtoul(optarg, &stopstring, 10);
				}
				if(my_options.width <= 0)
					my_options.width = 640;
				if(my_options.height <= 0)
					my_options.height = 480;
				break;

			case 'f':
			{
				int str_size = strlen(optarg);
				if(str_size == 4) /*fourcc is 4 chars*/
					strncpy(my_options.format, optarg, 4);
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
