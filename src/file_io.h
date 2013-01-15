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

#ifndef FILE_IO_H
#define FILE_IO_H

#include "../config.h"
#include "defs.h"

#define IO_BUFFER_SIZE 32768

typedef struct io_Writer
{
	FILE *fp;      /* file pointer     */

	BYTE *buffer;  /**< Start of the buffer. */
    int buffer_size;        /**< Maximum buffer size */
    BYTE *buf_ptr; /**< Current position in the buffer */
    BYTE *buf_end; /**< End of the buffer. */

	int64_t size; //file size (end of file position)
	int64_t position; //file pointer position (updates on buffer flush)
} io_Writer;

/** create a new writer:
 * params: filename - file for write to (if NULL mem only writer)
 *         max_size - mem buffer size (if 0 use default)*/
io_Writer* io_create_writer(const char *filename, int max_size);
/* destroy the writer*/
void io_destroy_writer(io_Writer* writer);
/*flush the writer buffer to disk */
int64_t io_flush_buffer(io_Writer* writer);
/* move the writer pointer to position */
int io_seek(io_Writer* writer, int64_t position);
/* move file pointer by amount*/
int io_skip(io_Writer* writer, int amount);
/* get writer offset (current position) */
int64_t io_get_offset(io_Writer* writer);
/* write 1 octet */
void io_write_w8(io_Writer* writer, BYTE b);
/* write a buffer of size*/
void io_write_buf(io_Writer* writer, BYTE *buf, int size);
/* write 2 octets le */
void io_write_wl16(io_Writer* writer, uint16_t val);
/* write 2 octets be */
void io_write_wb16(io_Writer* writer, uint16_t val);
/* write 3 octets le */
void io_write_wl24(io_Writer* writer, uint32_t val);
/* write 3 octets be */
void io_write_wb24(io_Writer* writer, uint32_t val);
/* write 4 octets le */
void io_write_wl32(io_Writer* writer, uint32_t val);
/* write 4 octets be */
void io_write_wb32(io_Writer* writer, uint32_t val);
/* write 8 octets le */
void io_write_wl64(io_Writer* writer, uint64_t val);
/* write 8 octets be */
void io_write_wb64(io_Writer* writer, uint64_t val);
/* write a 4cc (4 char word)*/
void io_write_4cc(io_Writer* writer, const char *str);
/* write a string (null terminated) returns the size writen*/
int io_write_str(io_Writer* writer, const char *str);

#if BIGENDIAN
	#define io_write_w16 io_write_wb16
	#define io_write_w24 io_write_wb24
	#define io_write_w32 io_write_wb32
	#define io_write_w64 io_write_wb64
#else
	#define io_write_w16 io_write_wl16
	#define io_write_w24 io_write_wl24
	#define io_write_w32 io_write_wl32
	#define io_write_w64 io_write_wl64
#endif

#endif
