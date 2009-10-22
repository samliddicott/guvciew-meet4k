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

#include "defs.h"
#include "profile.h"
#include "../config.h"

int
SaveControls(struct VidState *s, struct GLOBAL *global, struct vdIn *videoIn)
{
	FILE *fp;
	int i=0;
	int val=0;
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
		if (s->control) 
		{
			g_fprintf(fp,"#guvcview control profile\n");
			g_fprintf(fp,"version=%s\n",VERSION);
			g_fprintf(fp,"# control name +\n");
			g_fprintf(fp,"#control[num]:id:type=val\n");
			/*save controls by type order*/
			/* 1- Boolean controls       */
			/* 2- Menu controls          */
			/* 3- Integer controls       */
			g_fprintf(fp,"# 1-BOOLEAN CONTROLS\n");
			for (i = 0; i < s->num_controls; i++)
			{
				/*Boolean*/
				InputControl * c = s->control + i;
				if(c->type == INPUT_CONTROL_TYPE_BOOLEAN) 
				{
					if (input_get_control (videoIn->fd, c, &val) != 0) 
					{
						val=c->default_val;
					}
					val = val & 0x0001;
					g_fprintf(fp,"# %s +\n",c->name);
					g_fprintf(fp,"control[%d]:0x%x:%d=%d\n",c->i,c->id,c->type, val);
				}
			}
			g_fprintf(fp,"# 2-MENU CONTROLS\n");
			for (i = 0; i < s->num_controls; i++) 
			{
				/*Menu*/
				InputControl * c = s->control + i;
				if(c->type == INPUT_CONTROL_TYPE_MENU) 
				{
					if (input_get_control (videoIn->fd, c, &val) != 0)
					{
						val=c->default_val;
					}
					g_fprintf(fp,"# %s +\n",c->name);
					g_fprintf(fp,"control[%d]:0x%x:%d=%d\n",c->i,c->id,c->type, val);
				}
			}
			g_fprintf(fp,"# 3-INTEGER CONTROLS\n");
			for (i = 0; i < s->num_controls; i++) 
			{
				/*Integer*/
				InputControl * c = s->control + i;
				if(c->type == INPUT_CONTROL_TYPE_INTEGER) 
				{
					if (input_get_control (videoIn->fd, c, &val) != 0) 
					{
						val=c->default_val;
					}
					g_fprintf(fp,"# %s +\n",c->name);
					g_fprintf(fp,"control[%d]:0x%x:%d=%d\n",c->i,c->id,c->type, val);
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
LoadControls(struct VidState *s, struct GLOBAL *global)
{
	FILE *fp;
	int i=0;
	unsigned int id=0;
	int type=0;
	int val=0;
	
	ControlInfo *base_ci = s->control_info;
	InputControl *base_c = s->control;
	ControlInfo *ci;
	InputControl *c;
	
	char *filename;
	filename = g_strjoin("/", global->profile_FPath[1], global->profile_FPath[0], NULL);
	
	if((fp = g_fopen(filename,"r"))!=NULL) 
	{
		char line[144];

		while (fgets(line, sizeof(line), fp) != NULL) 
		{
			
			if ((line[0]=='#') || (line[0]==' ') || (line[0]=='\n')) 
			{
				/*skip*/
			} 
			else if ((sscanf(line,"control[%i]:0x%x:%i=%i",&i,&id,&type,&val))==4)
			{
				/*set control*/
				if (i < s->num_controls) 
				{
					ci=base_ci+i;
					c=base_c+i;
					g_printf("control[%i]:0x%x:%i=%d\n",i,id,type,val);
					if((c->id==id) && (c->type==type)) 
					{
						if(type == INPUT_CONTROL_TYPE_INTEGER) 
						{
							//input_set_control (videoIn, c, val);
							gtk_range_set_value (GTK_RANGE (ci->widget), val);
						} 
						else if (type == INPUT_CONTROL_TYPE_BOOLEAN) 
						{
							val = val & 0x0001;
							//input_set_control (videoIn, c, val);
							gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
												val ? TRUE : FALSE);
						} 
						else if (type == INPUT_CONTROL_TYPE_MENU) 
						{
							//input_set_control (videoIn, c, val);
							gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget), val);
						}
					}
					else 
					{
						g_printerr("wrong control id(0x%x:0x%x) or type(%i:%i) for %s\n",
							id,c->id,type,c->type,c->name);
					}
				} 
				else 
				{
					g_printerr("wrong control index: %d\n",i);
				}
			}
		}	
	} 
	else 
	{
		g_printerr("Could not open profile data file: %s.\n",filename);
		return (-1);
	} 

	fclose(fp);
	g_free(filename);
	return (0);
}
