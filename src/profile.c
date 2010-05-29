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

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <string.h>

#include "defs.h"
#include "profile.h"
#include "../config.h"

int
SaveControls(struct VidState *s, struct GLOBAL *global, struct vdIn *videoIn)
{
	FILE *fp;
	int i=0;
	char *filename;
	filename = g_strjoin("/", global->profile_FPath[1], global->profile_FPath[0], NULL);
	
	fp = g_fopen(filename, "w");
	if( fp == NULL )
	{
		g_printerr("Could not open profile data file: %s.\n",filename);
		return (-1);
	} 
	else 
	{
		if (s->control_list) 
		{
		    Control *current = s->control_list;
		    Control *next = current->next;
            //write header
            fprintf(fp, "#V4L2/CTRL/0.0.2\n");
            fprintf(fp, "APP{\"v4l2-re-store-ctrls example app\"}\n");
            //write control data
            fprintf(fp, "# control data\n");
            for(i=0; i<s->num_controls; i++)
            {
                fprintf(fp, "#%s\n", current->control.name);
                switch(current->control.type)
                {
                    case V4L2_CTRL_TYPE_STRING :
                       fprintf(fp, "ID{0x%08x};CHK{%i:%i:%i:0}=STR{\"%s\"}\n",
                            current->control.id, 
                            current->control.minimum, 
                            current->control.maximum,
                            current->control.step,
                            current->string);
                        break;
                    case V4L2_CTRL_TYPE_INTEGER64 :
                        fprintf(fp, "ID{0x%08x};CHK{0:0:0:0}=VAL64{%" PRId64 "}\n",
                            current->control.id, 
                            current->value64);
                        break;
                    default :
                        fprintf(fp, "ID{0x%08x};CHK{%i:%i:%i:%i}=VAL{%i}\n",
                            current->control.id, 
                            current->control.minimum, 
                            current->control.maximum,
                            current->control.step,
                            current->control.default_value,
                            current->value);
                        break;
                }
            
                if(next == NULL)
                    break;
                else 
                {
                    current = next;
                    next = current->next;
                }    
            }
		}
	}
	g_free(filename);
	
	fflush(fp); //flush stream buffers to filesystem
	if(fsync(fileno(fp)) ||	fclose(fp))
	{
		perror("PROFILE ERROR - write to file failed");
		return(-1);
	}
	
	return (0);
}

int
LoadControls(struct VidState *s, struct GLOBAL *global, struct vdIn *videoIn)
{
	FILE *fp;
	int i=0, major=0, minor=0, rev=0;
	unsigned int id=0;
	
	Control *current = NULL;

	char *filename;
	filename = g_strjoin("/", global->profile_FPath[1], global->profile_FPath[0], NULL);
	
	if((fp = g_fopen(filename,"r"))!=NULL) 
	{
        char line[200];
        if(fgets(line, sizeof(line), fp) != NULL)
        {
            if(sscanf(line,"#V4L2/CTRL/%i.%i.%i", &major, &minor, &rev) == 3) 
            {
                //check standard version if needed
            }
            else
            {
                printf("no valid header found\n");
                goto finish;
            }
        }
        else
        {
            printf("no valid header found\n");
            goto finish;
        }
            
        while (fgets(line, sizeof(line), fp) != NULL) 
        {
            int id = 0; 
            int min = 0, max = 0, step = 0, def = 0;
            int32_t val = 0;
            int64_t val64 = 0;
            
            if ((line[0]!='#') && (line[0]!='\n')) 
            {
                if(sscanf(line,"ID{0x%08x};CHK{%i:%i:%i:%i}=VAL{%i}",
                    &id, &min, &max, &step, &def, &val) == 6)
                {
                    current = get_ctrl_by_id(s->control_list, id);
                    if(current)
                    {
                        //check values
                        if(current->control.minimum == min &&
                           current->control.maximum == max &&
                           current->control.step == step &&
                           current->control.default_value == def)
                        {
                            current->value = val;
                            //printf("setting %s to %i\n",
                            //    current->control.name, val);
                        }
                    }
                }
                else if(sscanf(line,"ID{0x%08x};CHK{0:0:0:0}=VAL64{%" PRId64 "}",
                    &id, &val64) == 2)
                {
                    current = get_ctrl_by_id(s->control_list, id);
                    if(current)
                    {
                        current->value64 = val64;
                    }
                }
                else if(sscanf(line,"ID{0x%08x};CHK{%i:%i:%i:0}=STR{\"%*s\"}",
                    &id, &min, &max, &step) == 5)
                {
                    current = get_ctrl_by_id(s->control_list, id);
                    if(current)
                    {
                         //check values
                        if(current->control.minimum == min &&
                           current->control.maximum == max &&
                           current->control.step == step)
                        {
                            char str[max+1];
                            sscanf(line, "ID{0x%*x};CHK{%*i:%*i:%*i:0}==STR{\"%s\"}", str);
                            if(strlen(str) > max) //FIXME: should also check (minimum +N*step)
                            {
                                printf("string bigger than maximum buffer size");
                            }
                            else
                            {
                                //string size including terminating null character
                                current->value = strlen(str) + 1;
                                strcpy(current->string, str);
                            }
                        }
                    }
                }       
            }
        }
        
        set_ctrl_values(videoIn->fd, s->control_list, s->num_controls);	
	} 
	else 
	{
		g_printerr("Could not open profile data file: %s.\n",filename);
		return (-1);
	} 
	
finish:
	fclose(fp);
	g_free(filename);
	return (0);
}
