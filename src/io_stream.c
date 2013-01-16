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

#ifndef STREAM_H
#define STREAM_H

#include "io_stream.h"
#include <stdio.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

io_Stream* add_new_stream(io_Stream** stream_list, int* list_size)
{
	
	io_Stream* stream = g_new0(io_Stream, 1);
	stream->next = NULL;
	stream->id = *list_size;

	fprintf(stderr, "STREAM: add stream %i to stream list\n", stream->id);
	if(stream->id == 0)
	{
		stream->previous = NULL;
		*stream_list = stream;
	}
	else
	{

		io_Stream* last_stream = get_last_stream(*stream_list);
		stream->previous = last_stream;
		last_stream->next = stream;
	}

	stream->indexes = NULL;

	*list_size = *list_size + 1;

	return(stream);
}

void destroy_stream_list(io_Stream* stream_list, int* list_size)
{
	io_Stream* stream = get_last_stream(stream_list);
	while(stream->previous != NULL) //from end to start
	{
		io_Stream* prev_stream = stream->previous;
		if(stream->indexes != NULL)
			g_free(stream->indexes);
		g_free(stream);
		stream = prev_stream;
		*list_size = *list_size - 1;
	}
	g_free(stream); //free the last one;
}

io_Stream* get_stream(io_Stream* stream_list, int index)
{
	io_Stream* stream = stream_list;

	if(!stream)
		return NULL;

	int j = 0;

	while(stream->next != NULL && (j < index))
	{
		stream = stream->next;
		j++;
	}

	if(j != index)
		return NULL;

	return stream;
}

io_Stream* get_first_video_stream(io_Stream* stream_list)
{
	io_Stream* stream = stream_list;

	if(!stream)
		return NULL;

	if(stream->type == STREAM_TYPE_VIDEO)
		return stream;

	while(stream->next != NULL)
	{
		stream = stream->next;

		if(stream->type == STREAM_TYPE_VIDEO)
			return stream;
	}

	return NULL;
}

io_Stream* get_first_audio_stream(io_Stream* stream_list)
{
	io_Stream* stream = stream_list;

	if(!stream)
		return NULL;

	if(stream->type == STREAM_TYPE_AUDIO)
		return stream;

	while(stream->next != NULL)
	{
		stream = stream->next;

		if(stream->type == STREAM_TYPE_AUDIO)
			return stream;
	}

	return NULL;
}

io_Stream* get_last_stream(io_Stream* stream_list)
{
	io_Stream* stream = stream_list;

	if(!stream)
		return NULL;

	while(stream->next != NULL)
		stream = stream->next;

	return stream;
}

#endif
