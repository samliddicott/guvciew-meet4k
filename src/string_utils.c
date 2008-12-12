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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <libgen.h>

#include "defs.h"


/* counts chars needed for n*/
int
num_chars (int n)
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
/* check image file extension and return image type*/
int 
check_image_type (char *filename) 
{
	int format=0;
	char str_ext[3];
	/*get the file extension*/
	sscanf(filename,"%*[^.].%3c",str_ext);
	/* change image type */
	int somExt = str_ext[0]+str_ext[1]+str_ext[2];
	switch (somExt) 
	{
		/* there are 8 variations we will check for 3*/
		case ('j'+'p'+'g'):
		case ('J'+'P'+'G'):
		case ('J'+'p'+'g'):
			format=0;
			break;
			
		case ('b'+'m'+'p'):
		case ('B'+'M'+'P'):
		case ('B'+'m'+'p'):
			format=1;
			break;
			
		case ('p'+'n'+'g'):
		case ('P'+'N'+'G'):
		case ('P'+'n'+'g'):
			format=2;
			break;
			
		case ('r'+'a'+'w'):
		case ('R'+'A'+'W'):
		case ('R'+'a'+'w'):
			format=3;
		 	break;
			
		default: /* use jpeg as default*/
			format=0;
	}

	return (format);
}

/* split fullpath in Path (splited[1]) and filename (splited[0])*/
pchar* splitPath(char *FullPath, char* splited[2]) 
{
	printf("in split path\n");
	char *basename = g_path_get_basename(FullPath);
	char *dirname  = g_path_get_dirname(FullPath);
	
	int cpysize = 0;
	int size = strlen(basename)+1; 
	
	if (size > (strlen(splited[0])+1))
	{
		/* strlen doesn't count '/0' so add 1 char*/
		printf("realloc basename to %d chars.\n",size);
		splited[0]=realloc(splited[0],(size)*sizeof(char));
	}
	
	cpysize = g_strlcpy(splited[0],basename,size*sizeof(char));
	if ( (cpysize+1) < (size*sizeof(char)) ) 
		printf("filename copy size error:(%i != %i)\n",cpysize+1,size*sizeof(char));
	
	size = strlen(dirname)+1; 
	
	if (size > (strlen(splited[1])+1))
	{
		/* strlen doesn't count '/0' so add 1 char*/
		printf("realloc dirname to %d chars.\n",size);
		splited[1]=realloc(splited[1],(size)*sizeof(char));
	}
	
	cpysize = g_strlcpy(splited[1],dirname,size*sizeof(char));
	if ( (cpysize + 1) < (size*sizeof(char)) ) 
		printf("dirname copy size error:(%i != %i)\n",cpysize+1,size*sizeof(char));
	
	if(basename != NULL) free(basename);
	if(dirname != NULL) free(dirname);
	
	printf("exiting split path\n");
	return (splited);
}
