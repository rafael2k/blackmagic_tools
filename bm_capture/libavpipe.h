/* (c) Copyright 2015 Rafael Diniz
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include "quicklz.h"

#ifndef HAVE_AVPIPE_
#define HAVE_AVPIPE_

// number of threads the compressed algoritm will use
#define NUM_THREADS 2

// This is the string used to mark the beginning of the stream and of every frame 
#define AVPIPE_MAGIC_NUMBER "AVPIPE"

#define AVPIPE_HD_1080i_x_resolution 1920
#define AVPIPE_HD_1080i_y_resolution 1080

#define AVPIPE_YUYV 0
#define AVPIPE_YUV420p 1
#define AVPIPE_YUV422p 2

#define AVPIPE_AUDIO_16_BITS 16
#define AVPIPE_AUDIO_24_BITS 24
#define AVPIPE_AUDIO_32_BITS 32

#define AVPIPE_AUDIO_STEREO 2
#define AVPIPE_AUDIO_MONO 1

#define AVPIPE_VIDEO_8_BITS 8
#define AVPIPE_VIDEO_10_BITS 10

#define AVPIPE_INTERLACED 1
#define AVPIPE_NON_INTERLACED 0

#define AVPIPE_COMPRESSED 1
#define AVPIPE_UNCOMPRESSED 0

#define AVPIPE_NON_INITIALIZED -1

#define AVPIPE_MODE_WRITING 1
#define AVPIPE_MODE_READING 2

#define AVPIPE_STATUS_RUNNING 1
#define AVPIPE_STATUS_STOPPED 2

#define AVPIPE_SUCCESS 1
#define AVPIPE_FAILURE 0

typedef int avpipe_type;

typedef struct avpipe_handler_
{

#ifdef DEBUG_
    FILE *av_index;
#endif
    FILE *av_file;
    avpipe_type x_resolution;
    avpipe_type y_resolution;
    avpipe_type color_space;
    avpipe_type audio_channels;
    avpipe_type audio_sample_size;
    avpipe_type video_sample_size;
    avpipe_type interlaced;
    avpipe_type compressed;
    avpipe_type uncompressed_video_frame_size;
    avpipe_type mode;
    avpipe_type status;
    qlz_state_compress *state_compress;
    qlz_state_decompress *state_decompress;
} avpipe_handler;


avpipe_handler *avpipe_init();

avpipe_type avpipe_open_read(avpipe_handler *handler, const char *filename);

avpipe_type avpipe_open_write(avpipe_handler *handler, const char *filename, 
			avpipe_type x_resolution,
			avpipe_type y_resolution,
			avpipe_type color_space,
			avpipe_type audio_channels,
			avpipe_type audio_sample_size,
			avpipe_type video_sample_size,
			avpipe_type interlaced,
			avpipe_type compressed);

avpipe_type avpipe_close_read(avpipe_handler *handler);

avpipe_type avpipe_close_write(avpipe_handler *handler);

avpipe_type avpipe_read_video(avpipe_handler *handler, void *vbuffer);

avpipe_type avpipe_read_audio(avpipe_handler *handler, void *abuffer, avpipe_type *audio_size);

avpipe_type avpipe_write_video(avpipe_handler *handler, void *vbuffer);

avpipe_type avpipe_write_audio(avpipe_handler *handler, void *abuffer, avpipe_type *audio_size);


#endif // HAVE_LIBAVPIPE_
