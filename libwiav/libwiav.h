#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include "quicklz.h"

#ifndef HAVE_LIBWIAV_
#define HAVE_LIBWIAV_

// number of threads the compressed algoritm will use
#define NUM_THREADS 3

// This is the string used to mark the beginning of the stream and of every frame 
#define WIAV_MAGIC_NUMBER "WIAV"

#define WIAV_HD_1080i_x_resolution 1920
#define WIAV_HD_1080i_y_resolution 1080

#define WIAV_YUYV 0
#define WIAV_YUV420p 1
#define WIAV_YUV422p 2

#define WIAV_AUDIO_16_BITS 16
#define WIAV_AUDIO_24_BITS 24
#define WIAV_AUDIO_32_BITS 32

#define WIAV_AUDIO_STEREO 2
#define WIAV_AUDIO_MONO 1

#define WIAV_VIDEO_8_BITS 8
#define WIAV_VIDEO_10_BITS 10

#define WIAV_INTERLACED 1
#define WIAV_NON_INTERLACED 0

#define WIAV_COMPRESSED 1
#define WIAV_UNCOMPRESSED 0

#define WIAV_NON_INITIALIZED -1

#define WIAV_MODE_WRITING 1
#define WIAV_MODE_READING 2

#define WIAV_STATUS_RUNNING 1
#define WIAV_STATUS_STOPPED 2

#define WIAV_SUCCESS 1
#define WIAV_FAILURE 0

typedef int wiav_type;

typedef struct wiav_handler_
{

#ifdef DEBUG_
    FILE *av_index;
#endif
    FILE *av_file;
    wiav_type x_resolution;
    wiav_type y_resolution;
    wiav_type color_space;
    wiav_type audio_channels;
    wiav_type audio_sample_size;
    wiav_type video_sample_size;
    wiav_type interlaced;
    wiav_type compressed;
    wiav_type uncompressed_video_frame_size;
    wiav_type mode;
    wiav_type status;
    qlz_state_compress *state_compress;
    qlz_state_decompress *state_decompress;
} wiav_handler;


wiav_handler *wiav_init();

wiav_type wiav_open_read(wiav_handler *handler, const char *filename);

wiav_type wiav_open_write(wiav_handler *handler, const char *filename, 
			wiav_type x_resolution,
			wiav_type y_resolution,
			wiav_type color_space,
			wiav_type audio_channels,
			wiav_type audio_sample_size,
			wiav_type video_sample_size,
			wiav_type interlaced,
			wiav_type compressed);

wiav_type wiav_close_read(wiav_handler *handler);

wiav_type wiav_close_write(wiav_handler *handler);

wiav_type wiav_read_video(wiav_handler *handler, void *vbuffer);

wiav_type wiav_read_audio(wiav_handler *handler, void *abuffer, wiav_type *audio_size);

wiav_type wiav_write_video(wiav_handler *handler, void *vbuffer);

wiav_type wiav_write_audio(wiav_handler *handler, void *abuffer, wiav_type *audio_size);


#endif // HAVE_LIBWIAV_
