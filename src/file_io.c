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

#include <stdio.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "file_io.h"

/**
 *  file io base functions
 **/

static int64_t io_tell(io_Writer* writer)
{
	if(writer->fp == NULL)
	{
		fprintf(stderr, "IO: (tell) no file pointer associated with writer (mem only ?)\n");
		return -1;
	}
	//flush the file buffer
	fflush(writer->fp);

	//return the file pointer position
	return ((int64_t) ftello64(writer->fp));
}

/** flush a mem only writer(buf_writer) into a file writer */
int io_flush_buf_writer(io_Writer* file_writer, io_Writer* buf_writer)
{
	int size = (int) (buf_writer->buf_ptr - buf_writer->buffer);
	io_write_buf(file_writer, buf_writer->buffer, size);
	buf_writer->buf_ptr = buf_writer->buffer;

	return size;
}


/* create a new writer for filename*/
io_Writer* io_create_writer(const char *filename, int max_size)
{
	io_Writer* writer = g_new0(io_Writer, 1);

	if(writer == NULL)
		return NULL;

	if(max_size > 0)
		writer->buffer_size = max_size;
	else
		writer->buffer_size = IO_BUFFER_SIZE;

	writer->buffer = g_new0(BYTE, writer->buffer_size);
	writer->buf_ptr = writer->buffer;
	writer->buf_end = writer->buf_ptr + writer->buffer_size;

	if(filename != NULL)
	{
		writer->fp = fopen(filename, "wb");
		if (writer->fp == NULL)
		{
			perror("Could not open file for writing");
			g_free(writer);
			return NULL;
		}
	}
	else
		writer->fp = NULL; //mem only writer (must be flushed to a file writer)

	return writer;
}

void io_destroy_writer(io_Writer* writer)
{
	//clean up

	if(writer->fp != NULL)
	{
		// flush the buffer to file
		io_flush_buffer(writer);
		// flush the file buffer
		fflush(writer->fp);
		// close the file pointer
		fclose(writer->fp);
	}

	//clean the mem buffer
	free(writer->buffer);
}

int64_t io_flush_buffer(io_Writer* writer)
{
	if(writer->fp == NULL)
	{
		fprintf(stderr, "IO: (flush) no file pointer associated with writer (mem only ?)\n");
		fprintf(stderr, "IO: (flush) try to increase buffer size\n");
		return -1;
	}

	size_t nitems = 0;
	if (writer->buf_ptr > writer->buffer)
	{
		nitems= writer->buf_ptr - writer->buffer;
		if(fwrite(writer->buffer, 1, nitems, writer->fp) < nitems)
		{
			perror("IO: file write error");
			return -1;
		}
	}
	else if (writer->buf_ptr < writer->buffer)
	{
		fprintf(stderr, "IO: Bad buffer pointer - dropping buffer\n");
		writer->buf_ptr = writer->buffer;
		return -1;
	}

	int64_t size_inc = nitems - (writer->size - writer->position);
	if(size_inc > 0)
		writer->size += size_inc;

	writer->position = io_tell(writer); //update current file pointer position

	writer->buf_ptr = writer->buffer;

	//should never happen
	if(writer->position > writer->size)
	{
		fprintf(stderr, "IO: file pointer ( %" PRIu64 " ) above expected file size ( %" PRIu64 " )\n", writer->position, writer->size);
		writer->size = writer->position;
	}

	return writer->position;
}

int io_seek(io_Writer* writer, int64_t position)
{
	int ret = 0;

	if(position <= writer->size) //position is on the file
	{
		if(writer->fp == NULL)
		{
			fprintf(stderr, "IO: (seek) no file pointer associated with writer (mem only ?)\n");
			return -1;
		}
		//flush the memory buffer (we need an empty buffer)
		io_flush_buffer(writer);
		//try to move the file pointer to position
		int ret = fseeko(writer->fp, position, SEEK_SET);
		if(ret != 0)
			fprintf(stderr, "IO: seek to file position %" PRIu64 "failed\n", position);
		else
			writer->position = io_tell(writer); //update current file pointer position

		//we are now on position with an empty memory buffer
	}
	else // position is on the buffer
	{
		//move file pointer to EOF
		if(writer->position != writer->size)
		{
			fseeko(writer->fp, writer->size, SEEK_SET);
			writer->position = writer->size;
		}
		//move buffer pointer to position
		writer->buf_ptr = writer->buffer + (position - writer->size);
	}
	return ret;
}

int io_skip(io_Writer* writer, int amount)
{
	if(writer->fp == NULL)
	{
		fprintf(stderr, "IO: (skip) no file pointer associated with writer (mem only ?)\n");
		return -1;
	}
	//flush the memory buffer (clean buffer)
	io_flush_buffer(writer);
	//try to move the file pointer to position
	int ret = fseeko(writer->fp, amount, SEEK_CUR);
	if(ret != 0)
		fprintf(stderr, "writer: skip file pointer by 0x%x failed\n", amount);

	writer->position = io_tell(writer); //update current file pointer position

	//we are on position with an empty memory buffer
	return ret;
}

int64_t io_get_offset(io_Writer* writer)
{
	//buffer offset
	int64_t offset = writer->buf_ptr - writer->buffer;
	if(offset < 0)
	{
		fprintf(stderr, "IO: bad buf pointer\n");
		writer->buf_ptr = writer->buffer;
		offset = 0;
	}
	//add to file offset
	offset += writer->position;

	return offset;
}

void io_write_w8(io_Writer* writer, BYTE b)
{
	*writer->buf_ptr++ = b;
    if (writer->buf_ptr >= writer->buf_end)
        io_flush_buffer(writer);
}

void io_write_buf(io_Writer* writer, BYTE *buf, int size)
{
	while (size > 0)
	{
		int len = writer->buf_end - writer->buf_ptr;
		if(len < 0)
			fprintf(stderr,"IO: (write_buf) buff pointer outside buffer\n");
		if(len >= size)
			len = size;

        memcpy(writer->buf_ptr, buf, len);
        writer->buf_ptr += len;

       if (writer->buf_ptr >= writer->buf_end)
            io_flush_buffer(writer);

        buf += len;
        size -= len;
    }
}

void io_write_wl16(io_Writer* writer, uint16_t val)
{
    io_write_w8(writer, (BYTE) val);
    io_write_w8(writer, (BYTE) (val >> 8));
}

void io_write_wb16(io_Writer* writer, uint16_t val)
{
    io_write_w8(writer, (BYTE) (val >> 8));
    io_write_w8(writer, (BYTE) val);
}

void io_write_wl24(io_Writer* writer, uint32_t val)
{
    io_write_wl16(writer, (uint16_t) (val & 0xffff));
    io_write_w8(writer, (BYTE) (val >> 16));
}

void io_write_wb24(io_Writer* writer, uint32_t val)
{
    io_write_wb16(writer, (uint16_t) (val >> 8));
    io_write_w8(writer, (BYTE) val);
}

void io_write_wl32(io_Writer* writer, uint32_t val)
{
    io_write_w8(writer, (BYTE) val);
    io_write_w8(writer, (BYTE) (val >> 8));
    io_write_w8(writer, (BYTE) (val >> 16));
    io_write_w8(writer, (BYTE) (val >> 24));
}

void io_write_wb32(io_Writer* writer, uint32_t val)
{
    io_write_w8(writer, (BYTE) (val >> 24));
    io_write_w8(writer, (BYTE) (val >> 16));
    io_write_w8(writer, (BYTE) (val >> 8));
    io_write_w8(writer, (BYTE) val);
}

void io_write_wl64(io_Writer* writer, uint64_t val)
{
    io_write_wl32(writer, (uint32_t)(val & 0xffffffff));
    io_write_wl32(writer, (uint32_t)(val >> 32));
}

void io_write_wb64(io_Writer* writer, uint64_t val)
{
    io_write_wb32(writer, (uint32_t)(val >> 32));
    io_write_wb32(writer, (uint32_t)(val & 0xffffffff));
}

void io_write_4cc(io_Writer* writer, const char *str)
{
    int len = 4;
    if(strlen(str) < len )
	{
		len = strlen(str);
	}

    io_write_buf(writer, (BYTE *) str, len);

    len = 4 - len;
    //fill remaining chars with spaces
    while(len > 0)
    {
		io_write_w8(writer, ' ');
		len--;
	}
}

int io_write_str(io_Writer* writer, const char *str)
{
    int len = 1;
    if (str) {
        len += strlen(str);
        io_write_buf(writer, (BYTE *) str, len);
    } else
        io_write_w8(writer, 0);
    return len;
}
