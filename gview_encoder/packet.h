/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#                                                                              #
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

#ifndef PACKET_H
#define PACKET_H

#include <inttypes.h>
#include <sys/types.h>

#include <libavcodec/avcodec.h>

/*
 * simpler packet data struct used for storing AVPacket data 
 */
typedef struct SPacket {
  uint8_t* data;
  int size;
  int64_t pts;
  int64_t dts;
  int flags;
} SPacket_t;

/*SPacket_t list item*/
typedef struct SPacket_list_item {
  SPacket_t* pkt; /*packet*/
  struct SPacket_list_item* next; /*next item on the list*/
} SPacket_list_item_t;

/*SPacket_t list (ordered by pts)*/
typedef struct SPacket_list {
  SPacket_list_item_t* head;
  //SPacket_list_item* tail;
  int size;
} SPacket_list_t;

/*create a new spacket from a AVPacket*/
SPacket_t* spacket_clone(AVPacket* pkt);

/*free SPacket_t*/
void spacket_free(SPacket_t* spkt);

/*create a new spacket list*/
SPacket_list_t* spacket_list_new();

/*clean the packet list*/
void spacket_list_free(SPacket_list_t* spkt_list);

/*add pkt to list (order by pts)*/
int spacket_list_add(SPacket_list_t* spkt_list, SPacket_t *spkt, int order_by_dts);

/*pop the first pkt from the list*/
SPacket_t* spacket_list_pop(SPacket_list_t* spkt_list);


#endif //PACKET_H
