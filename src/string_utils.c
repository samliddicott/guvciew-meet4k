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
	int i;
	int j;
	int k;
	int l;
	int FPSize;
	int FDSize;
	int FSize;
	char *tmpstr;
	tmpstr=FullPath;
	
	FPSize=strlen(FullPath);
	tmpstr+=FPSize;
	for(i=0;i<FPSize;i++) 
	{
		if((*tmpstr--)=='/') 
		{
			FSize=i;
			tmpstr+=2;/*must increment by 2 because of '/'*/
			if((FSize>19) && (FSize>strlen(splited[0]))) 
			{
				// printf("realloc Filename to %d chars.\n",FSize);
				splited[0]=realloc(splited[0],(FSize+1)*sizeof(char));
			} 
			
			splited[0]=strncpy(splited[0],tmpstr,FSize);
			splited[0][FSize]='\0';
			/* cut spaces at begin of Filename String*/
			j=0;
			l=0;
			while((splited[0][j]==' ') && (j<100)) j++;/*count*/
			if (j>0) 
			{
				for(k=j;k<strlen(splited[0]);k++) 
				{
					splited[0][l++]=splited[0][k];
				}
				splited[0][l++]='\0';
			}
			
			FDSize=FPSize-FSize;
			if ((FDSize>99) && (FDSize>strlen(splited[1]))) 
			{
				// printf("realloc FileDir to %d chars.\n",FDSize);
				splited[1]=realloc(splited[1],(FDSize+1)*sizeof(char));
			} 
			if(FDSize>0) 
			{
				splited[1]=strncpy(splited[1],FullPath,FDSize);
				splited[1][FDSize]='\0';
				/* cut spaces at begin of Dir String*/
				j=0;
				l=0;
				while((splited[1][j]==' ') && (j<100)) j++;
				if (j>0) 
				{
					for(k=j;k<strlen(splited[1]);k++) 
					{
						splited[1][l++]=splited[1][k];
					}
					splited[1][l++]='\0';
				}
				/* check for "~" and replace with home dir*/
				//printf("FileDir[0]=%c\n",FileDir[0]);
				if(splited[1][0]=='~') 
				{
					for(k=0;k<strlen(splited[1]);k++) 
					{
						splited[1][k]=splited[1][k+1];
					}
					char path_str[100];
					char *home=getenv("HOME");
					sprintf(path_str,"%s%s",home,splited[1]);
					//printf("path is %s\n",path_str);
					FDSize=strlen(path_str);
					if (FDSize<99) 
					{
						strncpy (splited[1],path_str,FDSize);
						splited[1][FDSize]='\0';
					}
					else printf("Error: Home Path(~) too big, keeping last.\n");
				}
			}
			
			break;
		}
	}
	
	if(i>=FPSize) 
	{ /* no dir specified */
		if ((FPSize>19) && (FPSize>strlen(splited[0]))) 
		{
			// printf("realloc Filename to %d chars.\n",FPSize);
			splited[0]=realloc(splited[0],(FPSize+1)*sizeof(char));
		} 
		splited[0]=strncpy(splited[0],FullPath,FPSize);
		splited[0][FPSize]='\0';
	}
	//printf("Dir:%s File:%s\n",splited[1],splited[0]);
	tmpstr=NULL;/*clean up*/
	return (splited);
}
