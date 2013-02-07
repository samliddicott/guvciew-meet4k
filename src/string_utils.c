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

#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include "defs.h"
#include "string_utils.h"
#include "video_format.h"


/* counts chars needed for n*/
int
int_num_chars (int n)
{
	int i = 0;

	if (n <= 0)
	{
		i++;
		n = -n;
	}

	while (n != 0)
	{
		n /= 10;
		i++;
	}
	return i;
}

int
uint64_num_chars (uint64_t n)
{
	int i = 0;

	while (n != 0)
	{
		n /= 10;
		i++;
	}
	return i;
}

/* check image file extension and return image type*/
int
check_image_type (char *filename)
{
	int format=0;
	
	/*get the file extension*/
	GString * file_str = g_string_new(filename);
	file_str = g_string_ascii_down(file_str);

	if (g_str_has_suffix (file_str->str, ".jpg"))
	{
		format = 0;
	}
	else if (g_str_has_suffix (file_str->str, ".bmp"))
	{
		format = 1;
	}
	else if (g_str_has_suffix (file_str->str, ".png"))
	{
		format = 2;
	}
	else if (g_str_has_suffix (file_str->str, ".raw"))
	{
		format = 3;
	}
	else
		format = 0;

	fprintf(stderr, "file %s has extension type %i\n", filename, format);
	g_string_free(file_str, TRUE);
	return (format);
}


/* check video file extension and return video format*/
int
check_video_type (char *filename)
{
	int format=0;
	/*get the file extension*/
	GString * file_str = g_string_new(filename);
	file_str = g_string_ascii_down(file_str);

	if (g_str_has_suffix (file_str->str, ".avi"))
	{
		format = AVI_FORMAT;
	}
	else if (g_str_has_suffix (file_str->str, ".mkv"))
	{
		format = MKV_FORMAT;
	}
	else if (g_str_has_suffix (file_str->str, ".webm"))
	{
		format = WEBM_FORMAT;
	}
	else
		format = MKV_FORMAT;

	fprintf(stderr, "file %s has extension type %i\n", filename, format);
	g_string_free(file_str, TRUE);
	return (format);
}

/* split fullpath in Path (splited[1]) and filename (splited[0])*/
pchar* splitPath(char *FullPath, char* splited[2])
{
	char *basename = g_path_get_basename(FullPath);
	char *dirname  = g_path_get_dirname(FullPath);

	//fprintf(stderr, "base: '%s' dir: '%s'\n",basename, dirname);
	
	int cpysize = 0;
	int size = strlen(basename)+1;

	if (size > (strlen(splited[0])+1))
	{
		/* strlen doesn't count '/0' so add 1 char*/
		splited[0]=g_renew(char, splited[0], size);
	}

	cpysize = g_strlcpy(splited[0], basename, size*sizeof(char));
	if ( (cpysize+1) < (size*sizeof(char)) )
		g_printerr("filename copy size error:(%i != %lu)\n",
			cpysize+1,
			(unsigned long) size*sizeof(char));

	
	size = strlen(dirname)+1;

	if (size > (strlen(splited[1])+1))
	{
		/* strlen doesn't count '/0' so add 1 char*/
		splited[1]=g_renew(char, splited[1], size);
	}

	cpysize = g_strlcpy(splited[1], dirname, size*sizeof(char));
	if ( (cpysize + 1) < (size*sizeof(char)) )
		g_printerr("dirname copy size error:(%i != %lu)\n",
			cpysize+1,
			(unsigned long) size*sizeof(char));
	

	g_free(basename);
	g_free(dirname);

	return (splited);
}

char *joinPath(char *fullPath, pchar *splited)
{
	/*clean existing string allocation*/
	if(fullPath != NULL)
		g_free(fullPath);

	/*allocate newly formed string*/
	fullPath = g_strjoin ("/", splited[1], splited[0], NULL);

	return (fullPath);
}

char *incFilename(char *fullPath, pchar *splited, uint64_t inc)
{
	/** we don't want to change the base filename (splited[0])
	 * so copy it
	 */
	char* filename = g_strdup(splited[0]);
	filename = add_file_suffix(filename, inc);
	
	/*clean existing string allocation*/
	g_free(fullPath);
	
	fullPath = g_strjoin ("/", splited[1], filename, NULL);
	
	if(filename)
		g_free(filename);
	
	return(fullPath);
}

char *setImgExt(char *filename, int format)
{
	int sname = strlen(filename)+1; /*include '\0' terminator*/
	char basename[sname];
	sscanf(filename,"%[^.]",basename);
	switch(format)
	{
		case 0:
			g_snprintf(filename, sname, "%s.jpg", basename);
			break;
		case 1:
			g_snprintf(filename, sname, "%s.bmp", basename);
			break;
		case 2:
			g_snprintf(filename, sname, "%s.png", basename);
			break;
		case 3:
			g_snprintf(filename, sname, "%s.raw", basename);
			break;
		default:
			g_printerr("Image format not supported\n");
	}
	return (filename);
}

char *setVidExt(char *filename, int format_ind)
{
	//include '\0' terminator
	int size = strlen(filename) + 1;
	char basename[size];
	sscanf(filename,"%[^.]",basename);
	const char* extension = get_vformat_extension(format_ind);
	//add '.' and '\0'
	int total_size = strlen(basename) + strlen(extension) + 2;
	if(total_size > size)
		filename = g_renew(char, filename, total_size);

	g_snprintf(filename, total_size, "%s.%s", basename, extension);

	return (filename);
}

char *add_file_suffix(char *filename, uint64_t suffix)
{
	int fsize=strlen(filename);
	char basename[fsize+1];
	char extension[5];

	sscanf(filename, "%[^.].%4s", basename, extension);

	fsize += uint64_num_chars(suffix) + 2;
	
	filename = g_renew(char, filename, fsize+1);
	
	snprintf(filename, fsize, "%s-%llu.%s", basename, (unsigned long long) suffix, extension);
	
	return(filename);
}

uint64_t get_file_suffix(const char *path, const char* filename)
{
	uint64_t suffix = 0;
	GDir *dir =  g_dir_open(path, 0, NULL);
	
	if(dir == NULL)
	{
		fprintf(stderr, "ERROR: Couldn't open %s directory\n", path);
		return suffix;
	}
	
	int fsize=strlen(filename);
	char basename[fsize];
	char extension[5];
	
	sscanf(filename,"%[^.].%4s", basename, extension);
	fsize += 8;
	char format_str[fsize];
	g_snprintf(format_str, fsize-1, "%s-%%20s.%s", basename, extension);
			
	char* file_name = NULL;
	while ((file_name = (char *) g_dir_read_name (dir)) != NULL)
	{
		if( g_str_has_prefix (file_name, basename) &&
		    g_str_has_suffix (file_name, extension))
		{
			char sfix_str[21];
			sscanf(file_name, format_str, sfix_str);
			uint64_t sfix = g_ascii_strtoull(sfix_str, NULL, 10);
			
			if(sfix > suffix)
				suffix = sfix;
		}
	}
	
	g_dir_close(dir);
	return suffix;
}

