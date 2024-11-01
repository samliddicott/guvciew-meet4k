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

#include "packet.h"
#include <errno.h>

/*create a new spacket from a AVPacket*/
SPacket_t* spacket_clone(AVPacket* pkt) {
  
  SPacket_t* spkt = malloc(sizeof(SPacket_t));
  
  if (!spkt) {
    fprintf(stderr, "ENCODER: Error spacket_clone: %s\n", strerror(errno));
    exit(1);
  }

  spkt->size = pkt->size;
  spkt->data = malloc(pkt->size);
  
  if (!spkt->data) {
    fprintf(stderr, "ENCODER: Error spacket_clone (alloc data): %s\n", strerror(errno));
    exit(1);
  }

  memcpy(spkt->data, pkt->data, pkt->size);

  spkt->pts = pkt->pts;
  spkt->dts = pkt->dts;
  spkt->flags = pkt->flags;

  return spkt;
}

/*free SPacket_t*/
void spacket_free(SPacket_t* spkt) {
  if(!spkt)
    return;

  if(spkt->data)
    free(spkt->data);

  free(spkt);
}

/*create a new spacket list*/
SPacket_list_t* spacket_list_new() {
  SPacket_list_t* spkt_list = malloc(sizeof(SPacket_list_t));
  spkt_list->size = 0;
  spkt_list->head = NULL;
  //spkt_list->tail = NULL;

  return spkt_list;
}

/*clean the packet list*/
void spacket_list_free(SPacket_list_t* spkt_list) {
  while (spkt_list->head != NULL) {
    SPacket_list_item_t* spkt_l_item = spkt_list->head;
    spkt_list->head = spkt_l_item->next;
    spacket_free(spkt_l_item->pkt);
    free(spkt_l_item);
  }
}

/*add pkt to list (order by pts)*/
int spacket_list_add(SPacket_list_t* spkt_list, SPacket_t *spkt, int order_by_dts) {
  
  SPacket_list_item_t* spkt_l_item = malloc(sizeof(SPacket_list_item_t));
  
  if (!spkt_l_item) {
    fprintf(stderr, "ENCODER: Error spacket_list_add: %s\n", strerror(errno));
    exit(1);
  }

  spkt_l_item->pkt = spkt;
  spkt_l_item->next = NULL;

  //first element
  if (spkt_list->head == NULL) {  
    spkt_list->head = spkt_l_item;
    //spkt_list->tail = spkt_l_item;
    spkt_list->size = 1;
    return spkt_list->size;
  }

  SPacket_list_item_t* p_item = NULL;
  SPacket_list_item_t* n_item = spkt_list->head;

  while (n_item) {
    if ((!order_by_dts && (spkt->pts < n_item->pkt->pts)) ||
       (order_by_dts && (spkt->dts < n_item->pkt->dts))) {
      
      spkt_l_item->next = n_item;
      
      if(!p_item) {
        spkt_list->head = spkt_l_item;
      } else {
        p_item->next = spkt_l_item;
      }
      spkt_list->size++;
      return spkt_list->size;
    }

    p_item = n_item;
    n_item = n_item->next;
  }

  //add to end of list
  p_item->next = spkt_l_item;
  spkt_list->size++;
  return spkt_list->size;
}

/*pop the first pkt from the list*/
SPacket_t* spacket_list_pop(SPacket_list_t* spkt_list) {
  if(spkt_list->head == NULL || spkt_list->size <= 0)
    return NULL;

  SPacket_list_item_t* spkt_l_item = spkt_list->head;
  SPacket_t* spkt = spkt_l_item->pkt;
  spkt_list->head = spkt_l_item->next;
  free(spkt_l_item);
  spkt_list->size--;

  return spkt;
}
