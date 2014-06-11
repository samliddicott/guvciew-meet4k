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
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>

#include "gviewv4l2core.h"

extern int debug_level;

/*
 * converts string to lowercase
 * args:
 *   str - string pointer
 *
 * asserts:
 *   none
 *
 * returns: pointer to converted string
 */
char *lowercase(char *str)
{
	char *p = str;
	for ( ; *p; ++p) *p = tolower(*p);

	return str;
}

/*
 * trim leading white spaces from source string
 * args:
 *    src - source string
 *
 * asserts:
 *    none
 *
 * returns: error code
 */
int trim_leading_wspaces(char *src)
{
	if(src == NULL || strlen(src) < 1)
		return -1;

	char *srcp = src;

	while(isspace(*srcp))
		srcp++;

	/*move string chars*/
	if(srcp != src)
	{
		char *tmp = strdup(srcp);
		strcpy(src, tmp);
		free(tmp);
	}

	return 0;
}

/*
 * trim trailing white spaces and control chars (\n) from source string
 * args:
 *    src - source string
 *
 * asserts:
 *    none
 *
 * returns: error code
 */
int trim_trailing_wspaces(char *src)
{
	if(src == NULL || strlen(src) < 1)
		return -1;

	/*move to end of string*/
	char *srcp = src + strlen(src);

	while((isspace(*(srcp-1)) || iscntrl(*(srcp-1))) && (srcp - 1) > src)
		srcp--;

	/*end string*/
	*srcp = '\0';

	return 0;
}

/*
 * gets the number of chars to represent n
 * args:
 *    n - uint64_t number to represent
 *
 * asserts:
 *    none
 *
 * returns: number of chars needed to represent n
 */
int get_uint64_num_chars (uint64_t n)
{
	int i = 0;

	while (n != 0)
	{
		n /= 10;
		i++;
	}
	return i;
}

/*
 * smart concatenation
 * args:
 *    dest - destination string
 *    c - connector char
 *    str1 - string to concat
 *
 * asserts:
 *    none
 *
 * returns: concatenated string (must free)
 */
char *smart_cat(const char *dest, const char c, const char *str1)
{
	int size_c = 0;
	if(c != 0)
		size_c = 1;
	int size_dest =  strlen(dest);
	int size_str1 = strlen(str1);

	int size = size_dest + size_c + size_str1 + 1; /*add ending null char*/
	char *my_cat = calloc(size, sizeof(char));
	if(my_cat == NULL)
	{
		fprintf(stderr,"GUVCVIEW: FATAL memory allocation failure (smart_cat): %s\n", strerror(errno));
		exit(-1);
	}
	char *my_p = my_cat;

	if(size_dest)
		memcpy(my_cat, dest, size_dest);

	if(size_c)
		my_cat[size_dest] = c;

	if(size_str1)
	{
		my_p += size_dest + size_c;
		memcpy(my_p, str1, size_str1);
	}
	/*add ending null char*/
	my_cat[size_dest + size_c + size_str1] = '\0';

	if(debug_level > 1)
		printf("GUVCVIEW: (smart_cat) dest=%s(%i) len_c=%c(%i) len_str1=%s(%i) => %s\n",
			dest, size_dest, c, size_c, str1, size_str1, my_cat);
	return my_cat;
}

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

	if(debug_level > 1)
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

	if(debug_level > 1)
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

	if(debug_level > 1)
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

	char *noext_filename = NULL;

	int strsize = strlen(filename);
	if(name)
		strsize = name - filename;

	noext_filename = strndup(filename, strsize);
	char *new_filename = smart_cat(noext_filename, '.', ext);

	free(noext_filename);

	if(debug_level > 1)
		printf("GUVCVIEW: changed file extension to %s\n", new_filename);
	return new_filename;
}

/*
 * get the sufix for filename in path (e.g. for file-3.png sufix is 3)
 * args:
 *   path - string with file path
 *   filename - string with file basename
 *
 * asserts:
 *   none
 *
 * returns: none
 */
unsigned long long get_file_suffix(const char *path, const char* filename)
{
	unsigned long long suffix = 0;

	DIR *dir = opendir(path);;
	struct dirent *dp;

	if(dir == NULL)
	{
		fprintf(stderr, "ERROR: Couldn't open %s directory\n", path);
		return suffix;
	}

	int noextsize = strlen(filename);

	char *name = strrchr(filename, '.');

	if(name)
		noextsize = name - filename;

	char *noextname = strndup(filename, noextsize);
	char *extension = strdup(name + 1);


	int fsize = strlen(filename) + 7;
	char format_str[fsize];
	snprintf(format_str, fsize-1, "%s-%%20s.%s", noextname, extension);

	while ((dp = readdir(dir)) != NULL)
	{
		if(debug_level > 3)
			printf("GUVCVIEW: (get_file_suffix) checking %s\n", dp->d_name);
		if (strncmp(dp->d_name, noextname, noextsize) == 0)
		{
			if(debug_level > 3)
					printf("GUVCVIEW: (get_file_suffix) prefix matched (%s)\n", noextname);
			char *ext = strrchr(dp->d_name, '.');
			if(strcmp(ext + 1, extension) == 0)
			{
				char sfixstr[21];
				unsigned long long sfix = 0;
				sscanf(dp->d_name, format_str, sfixstr);

				if(debug_level > 3)
					printf("GUVCVIEW: (get_file_suffix) matched with suffix %s\n", sfixstr);

				sfix = strtoull(sfixstr, (char **)NULL, 10);

				if(sfix > suffix)
					suffix = sfix;
			}
		}
	}

	closedir(dir);

	free(noextname);
	free(extension);

	if(debug_level > 1)
		printf("GUVCVIEW: (get_file_suffix) %s has sufix %llu\n", filename, suffix);
	return suffix;
}

/*
 * add a number suffix to filename (e.g. name.ext => name-suffix.ext)
 *   the suffix depends on the existing values in the path dir
 * args:
 * 	  path - string with file path (to dir)
 *    filename - string with file basename (name.ext)
 *    suffix - suffix number
 *
 * asserts:
 *    none
 *
 * returns: newly allocated string with suffixed file name (must free)
 */
char *add_file_suffix(const char *path, const char *filename)
{
	unsigned long long suffix = get_file_suffix(path, filename);
	/*increment existing suffix*/
	suffix++;
	int size_suffix = get_uint64_num_chars(suffix);
	int size_name = strlen(filename);

	int noextsize = strlen(filename);

	char *pname = strrchr(filename, '.');

	if(pname)
		noextsize = pname - filename;

	char *noextname = strndup(filename, noextsize);
	char *extension = strdup(pname + 1);

	/*add '-' suffix and '\0' and an extra char just for safety*/
	char *new_name = calloc(size_name + size_suffix + 3, sizeof(char));
	if(new_name == NULL)
	{
		fprintf(stderr,"GUVCVIEW: FATAL memory allocation failure (add_file_suffix): %s\n", strerror(errno));
		exit(-1);
	}
	sprintf(new_name, "%s-%llu.%s", noextname, suffix, extension);

	if(noextname)
		free(noextname);
	if(extension)
		free(extension);

	return new_name;
}
