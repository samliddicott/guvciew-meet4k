/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/videodev2.h>
#include <gtk/gtk.h>
#include "defs.h"
#include "profile.h"
#include "../config.h"

int
SaveControls(struct VidState *s, struct GLOBAL *global, struct vdIn *videoIn)
{
	
	FILE *fp;
	int i=0;
	int val=0;
	int sfname=strlen(global->profile_FPath[1])+strlen(global->profile_FPath[0]);
	char filename[sfname+2];
	
	sprintf(filename,"%s/%s", global->profile_FPath[1],global->profile_FPath[0]);
	
	fp=fopen(filename,"w");
	if( fp == NULL )
	{
		printf("Could not open profile data file: %s.\n",filename);
		return (-1);
	} else {
		if (s->control) {
			fprintf(fp,"#guvcview control profile\n");
			fprintf(fp,"version=%s\n",VERSION);
			fprintf(fp,"# control name +\n");
			fprintf(fp,"#control[num]:id:type=val\n");
			/*save controls by type order*/
			/* 1- Boolean controls       */
			/* 2- Menu controls          */
			/* 3- Integer controls       */
			fprintf(fp,"# 1-BOOLEAN CONTROLS\n");
			for (i = 0; i < s->num_controls; i++) {
				/*Boolean*/
				InputControl * c = s->control + i;
				if(c->type == INPUT_CONTROL_TYPE_BOOLEAN) {
					if (input_get_control (videoIn, c, &val) != 0) {
						val=c->default_val;
					}
					val = val & 0x0001;
					fprintf(fp,"# %s +\n",c->name);
					fprintf(fp,"control[%d]:0x%x:%d=%d\n",c->i,c->id,c->type, val);
				}
			}
			fprintf(fp,"# 2-MENU CONTROLS\n");
			for (i = 0; i < s->num_controls; i++) {
				/*Menu*/
				InputControl * c = s->control + i;
				if(c->type == INPUT_CONTROL_TYPE_MENU) {
					if (input_get_control (videoIn, c, &val) != 0) {
						val=c->default_val;
					}
					fprintf(fp,"# %s +\n",c->name);
					fprintf(fp,"control[%d]:0x%x:%d=%d\n",c->i,c->id,c->type, val);
				}
			}
			fprintf(fp,"# 3-INTEGER CONTROLS\n");
			for (i = 0; i < s->num_controls; i++) {
				/*Integer*/
				InputControl * c = s->control + i;
				if(c->type == INPUT_CONTROL_TYPE_INTEGER) {
					if (input_get_control (videoIn, c, &val) != 0) {
						val=c->default_val;
					}
					fprintf(fp,"# %s +\n",c->name);
					fprintf(fp,"control[%d]:0x%x:%d=%d\n",c->i,c->id,c->type, val);
				}
			}
			
		}
	}
	fclose(fp);
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
	//char contr_inf[100];
	int sfname=strlen(global->profile_FPath[1])+strlen(global->profile_FPath[0]);
	char filename[sfname+2];
	ControlInfo *base_ci = s->control_info;
	InputControl *base_c = s->control;
	ControlInfo *ci;
	InputControl *c;
	
	sprintf(filename,"%s/%s", global->profile_FPath[1],global->profile_FPath[0]);
	
	if((fp = fopen(filename,"r"))!=NULL) {
		char line[144];

		while (fgets(line, 144, fp) != NULL) {
			
			if ((line[0]=='#') || (line[0]==' ') || (line[0]=='\n')) {
				/*skip*/
			} else if ((sscanf(line,"control[%i]:0x%x:%i=%i",&i,&id,&type,&val))==4){
				/*set control*/
				if (i < s->num_controls) {
					ci=base_ci+i;
					c=base_c+i;
					printf("control[%i]:0x%x:%i=%d\n",i,id,type,val);
					if((c->id==id) && (c->type==type)) {
						if(type == INPUT_CONTROL_TYPE_INTEGER) {					
							//input_set_control (videoIn, c, val);
							gtk_range_set_value (GTK_RANGE (ci->widget), val);
						} else if (type == INPUT_CONTROL_TYPE_BOOLEAN) {
							val = val & 0x0001;
							//input_set_control (videoIn, c, val);
							gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
												val ? TRUE : FALSE);
						} else if (type == INPUT_CONTROL_TYPE_MENU) {
							//input_set_control (videoIn, c, val);
							gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget),
				        							     val);
						}
					}
					else {
						printf("wrong control id(0x%x:0x%x) or type(%i:%i) for %s\n",
							   id,c->id,type,c->type,c->name);
					}
				} else {
					printf("wrong control index: %d\n",i);
				}
			}
		}	
	} else {
		printf("Could not open profile data file: %s.\n",filename);
		return (-1);
	} 
	

	fclose(fp);
	return (0);
	
}
