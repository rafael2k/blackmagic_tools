#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "quicklz.h"

#include "libwiav.h"

void get_int_until_0xa(FILE *fp, int *value)
{
    char buffer[32];
    int i = 0;

    do
    {
	assert(i < 32);
	
	if (fread(buffer+i, 1, 1, fp) != 1)
	{
	    *value = 0;
	    return;
	}

    }while(buffer[i++] != '\n');
    
    *value = atoi(buffer);
    
}

void get_string_until_0xa(FILE *fp, char *string)
{
    int i = 0;

    do
    {
	assert(i < 32);
	
	if (fread(string+i, 1, 1, fp) != 1)
	{
	    string = NULL;
	    return;
	}

    }while(string[i++] != '\n');

    string[i-1] = 0;
    
}

void put_string_and_0xa(FILE *fp, char *string)
{
    int i = 0;
    char c = '\n';
    
    while (string[i] != 0)
    {
	if (fwrite(string+i, 1, 1, fp) != 1)
	{
	    return;
	}
	i++;
    }
    

    fwrite(&c, 1, 1, fp);
    
}

void put_int_and_0xa(FILE *fp, int value)
{
    char buffer[32];
    int i = 0;

    sprintf(buffer, "%d\n", value);

    while (buffer[i] != 0)
    {
	if (fwrite(buffer+i, 1, 1, fp) != 1)
	{
	    return;
	}
	i++;
    }
}

wiav_type wiav_blank_parameters(wiav_handler *handler)
{
    
#ifdef DEBUG_
    handler->av_index = NULL;
#endif
    handler->av_file = NULL;
    handler->x_resolution = WIAV_NON_INITIALIZED;
    handler->y_resolution = WIAV_NON_INITIALIZED;
    handler->color_space = WIAV_NON_INITIALIZED;
    handler->audio_channels = WIAV_NON_INITIALIZED;
    handler->audio_sample_size = WIAV_NON_INITIALIZED;
    handler->video_sample_size = WIAV_NON_INITIALIZED;
    handler->interlaced = WIAV_NON_INITIALIZED;
    handler->compressed = WIAV_NON_INITIALIZED;
    handler->uncompressed_video_frame_size = WIAV_NON_INITIALIZED;
    handler->mode = WIAV_NON_INITIALIZED;
    handler->status = WIAV_NON_INITIALIZED;
    handler->state_decompress = NULL;
    handler->state_compress = NULL;

    return WIAV_SUCCESS;

}

wiav_type wiav_get_pixel_size(wiav_handler *handler, wiav_type *numerator, wiav_type *denominator)
{
    if (handler->video_sample_size == WIAV_VIDEO_10_BITS)
    {
	fprintf(stderr, "10 bits video not implemented yet!\n");
	return WIAV_FAILURE;
    }
    else /* if(handler->video_sample_size == WIAV_VIDEO_8_BITS) */
    {
	switch(handler->color_space)
	{
	case WIAV_YUYV:
	case WIAV_YUV422p:
	    *numerator = 2;
	    *denominator = 1;
	    break;
	case WIAV_YUV420p:
	    *numerator = 3;
	    *denominator = 2;
	    break;
	default:
	    fprintf(stderr,"pixel format unknown or not implemented!\n");
	    return WIAV_FAILURE;
	}
    }

    return WIAV_SUCCESS;
}

wiav_handler *wiav_init()
{
    wiav_handler *handler;
    
    handler = (wiav_handler *) malloc(sizeof(wiav_handler));
    
    wiav_blank_parameters(handler);
    
    handler->status = WIAV_STATUS_STOPPED;

    return handler;
	
}

wiav_type wiav_open_read(wiav_handler *handler, const char *filename)
{
    char buffer[128];
    int pixel_size_numerator, pixel_size_denominator;

    if (!(handler->av_file = fopen(filename, "r")))
	return WIAV_FAILURE;

#ifdef DEBUG_
    handler->av_index = fopen("av_index-read.txt", "w");
#endif

    handler->mode = WIAV_MODE_READING;

    fscanf(handler->av_file, "%s %d %d %d %d %d %d %d %d\n",buffer,
	   &handler->x_resolution,
	   &handler->y_resolution,
	   &handler->color_space,
	   &handler->audio_channels,
	   &handler->audio_sample_size,
	   &handler->video_sample_size,
	   &handler->interlaced,
	   &handler->compressed
	);

    if (strcmp(buffer,WIAV_MAGIC_NUMBER))
	return WIAV_FAILURE;

    wiav_get_pixel_size(handler, &pixel_size_numerator, &pixel_size_denominator);
    handler->uncompressed_video_frame_size = handler->x_resolution * handler->y_resolution * 
	pixel_size_numerator / pixel_size_denominator;

    
    if (handler->compressed == WIAV_COMPRESSED)
    {
	fprintf(stderr, "Compressed Video Stream mode!\n");
	handler->state_decompress = (qlz_state_decompress *)malloc(sizeof(qlz_state_decompress));
    }

    
    return WIAV_SUCCESS;
 
}

wiav_type wiav_open_write(wiav_handler *handler, const char *filename, 
			  wiav_type x_resolution,
			  wiav_type y_resolution,
			  wiav_type color_space,
			  wiav_type audio_channels,
			  wiav_type audio_sample_size,
			  wiav_type video_sample_size,
			  wiav_type interlaced,
			  wiav_type compressed)
{
    int pixel_size_numerator, pixel_size_denominator;
    
#ifdef DEBUG_
    handler->av_index = fopen("av_index-write.txt", "w");
#endif

    if (!(handler->av_file = fopen(filename, "w")))
	return WIAV_FAILURE;

    handler->mode = WIAV_MODE_WRITING;
    
    handler->x_resolution = x_resolution;
    handler->y_resolution = y_resolution;
    handler->color_space = color_space;
    handler->audio_channels = audio_channels;
    handler->audio_sample_size = audio_sample_size;
    handler->video_sample_size = video_sample_size;
    handler->interlaced = interlaced;
    handler->compressed = compressed;


    wiav_get_pixel_size(handler, &pixel_size_numerator, &pixel_size_denominator);
    handler->uncompressed_video_frame_size = handler->x_resolution * handler->y_resolution * 
	pixel_size_numerator / pixel_size_denominator;
    
    fprintf(handler->av_file, "%s %d %d %d %d %d %d %d %d\n",WIAV_MAGIC_NUMBER,
	    handler->x_resolution,
	    handler->y_resolution,
	    handler->color_space,
	    handler->audio_channels,
	    handler->audio_sample_size,
	    handler->video_sample_size,
	    handler->interlaced,
	    handler->compressed
	);

    
    if (handler->compressed == WIAV_COMPRESSED)
    {
	fprintf(stderr, "Compressed Video Stream mode!\n");
	handler->state_compress = (qlz_state_compress *)malloc(sizeof(qlz_state_compress));
    }


    return WIAV_SUCCESS;

}

wiav_type wiav_close_read(wiav_handler *handler)
{
    
    if (handler->compressed == WIAV_COMPRESSED)
    {
	free(handler->state_decompress);
    }

    
    if (!fclose(handler->av_file))
    {
	wiav_blank_parameters(handler);
	return WIAV_FAILURE;
    }
    else
    {
	wiav_blank_parameters(handler);
    }

#ifdef DEBUG_
    fclose(handler->av_index);
#endif
    
    return WIAV_SUCCESS;

}

wiav_type wiav_close_write(wiav_handler *handler)
{
    
    if (handler->compressed == WIAV_COMPRESSED)
    {
	free(handler->state_compress);
    }


    if (!fclose(handler->av_file))
    {
	wiav_blank_parameters(handler);
	return WIAV_FAILURE;
    }
    else
    {
	wiav_blank_parameters(handler);
    }

#ifdef DEBUG_
    fclose(handler->av_index);
#endif
    
    return WIAV_SUCCESS;

}


wiav_type wiav_read_video(wiav_handler *handler, void *vbuffer)
{
    static char compressed_buffer[WIAV_HD_1080i_x_resolution*WIAV_HD_1080i_y_resolution*2]; // our maximum size for now
    char buffer[16];
    wiav_type video_size;
    
    if (handler->mode != WIAV_MODE_READING)
	return WIAV_FAILURE;

    if (handler->status == WIAV_STATUS_STOPPED)
	handler->status = WIAV_STATUS_RUNNING;
    
#ifdef DEBUG_
    fprintf(handler->av_index, "New Video Frame at: %ld\n", ftell(handler->av_file));
#endif


    if (handler->compressed == WIAV_COMPRESSED)
    {
	// fscanf(handler->av_file, "%s\n",buffer);
	get_string_until_0xa(handler->av_file, buffer);
	if (strcmp(buffer, WIAV_MAGIC_NUMBER))
	{
	    fprintf(stderr, "Error on checking video Magic Number: %s\n", buffer);
	    return WIAV_FAILURE;
	}

	// the compressed video size
	//fscanf(handler->av_file, "%d\n", &video_size);
	get_int_until_0xa(handler->av_file, &video_size);

	if (fread(compressed_buffer, 1, video_size , handler->av_file) != video_size)
	{
	    fprintf(stderr, "Error reading video from file\n");
	    return WIAV_FAILURE;
	}
	
	// qlz_decompress returns the size of the uncompressed chunk, that we already know...
	video_size = qlz_decompress(compressed_buffer, vbuffer, handler->state_decompress);
	if (video_size != handler->uncompressed_video_frame_size)
	{
	    fprintf(stderr, "Error on decompressing video\n");
	    return WIAV_FAILURE;
	}

	return WIAV_SUCCESS;
	
    }
    else /* if (handler->compressed == WIAV_UNCOMPRESSED) */
    {
	// fscanf(handler->av_file, "%s\n", buffer);
	get_string_until_0xa(handler->av_file, buffer);
	if (strcmp(buffer, WIAV_MAGIC_NUMBER))
	{
	    fprintf(stderr,"Error on checking video Magic Number: %s\n", buffer);
	    return WIAV_FAILURE;
	}
	if (fread(vbuffer, 1, handler->uncompressed_video_frame_size, handler->av_file) != handler->uncompressed_video_frame_size)
	{
	    fprintf(stderr,"Error on Reading Video Payload!\n");
	    return WIAV_FAILURE;
	}
    }

#ifdef DEBUG_
    fprintf(handler->av_index, "Ending at: %ld\n", ftell(handler->av_file));
#endif

    return WIAV_SUCCESS;
    
}

wiav_type wiav_read_audio(wiav_handler *handler, void *abuffer, wiav_type *audio_size)
{
    
    if (handler->mode != WIAV_MODE_READING)
	return WIAV_FAILURE;

    if (handler->status == WIAV_STATUS_STOPPED)
	handler->status = WIAV_STATUS_RUNNING;

#ifdef DEBUG_
    fprintf(handler->av_index, "New Audio Frame at: %ld\n", ftell(handler->av_file));
#endif

    //fscanf(handler->av_file, "%d\n",audio_size);
    get_int_until_0xa(handler->av_file, audio_size);

    if (fread(abuffer, 1, *audio_size, handler->av_file) != *audio_size)
    {
	fprintf(stderr, "Error reading audio from file\n");
	return WIAV_FAILURE;
    }

#ifdef DEBUG_
    fprintf(handler->av_index, "Ending Audio Frame at: %ld\n", ftell(handler->av_file));
#endif
    
    return WIAV_SUCCESS;

}

wiav_type wiav_write_video(wiav_handler *handler, void *vbuffer)
{
    static char compressed_buffer[WIAV_HD_1080i_x_resolution*WIAV_HD_1080i_y_resolution*2 + 500000]; // our maximum size for now + compression workspace
    wiav_type video_size;

    
    if (handler->mode != WIAV_MODE_WRITING)
	return WIAV_FAILURE;
    
    if (handler->status == WIAV_STATUS_STOPPED)
	handler->status = WIAV_STATUS_RUNNING;

#ifdef DEBUG_
    fprintf(handler->av_index, "New Video Frame at: %ld\n", ftell(handler->av_file));
#endif

    if (handler->compressed == WIAV_COMPRESSED)
    {
	// fprintf(handler->av_file, "%s\n", WIAV_MAGIC_NUMBER);
	put_string_and_0xa(handler->av_file, WIAV_MAGIC_NUMBER);
	
	video_size = qlz_compress(vbuffer, compressed_buffer, handler->uncompressed_video_frame_size, handler->state_compress);
	if (video_size == 0)
	{
	    fprintf(stderr, "Error compressing file\n");
	    return WIAV_FAILURE;
	}

	// fprintf(handler->av_file, "%d\n", video_size);
	put_int_and_0xa(handler->av_file, video_size);

	// we need a thread to offload this writing time!!!!!!
	if (fwrite(compressed_buffer, 1, video_size, handler->av_file) != video_size)
	{
	    fprintf(stderr, "Error writing video to file\n");
	    return WIAV_FAILURE;
	}

    }
    else /* if (handler->compressed == WIAV_UNCOMPRESSED) */
    {
	// fprintf(handler->av_file, "%s\n", WIAV_MAGIC_NUMBER);
	put_string_and_0xa(handler->av_file, WIAV_MAGIC_NUMBER);

	if (fwrite(vbuffer, 1, handler->uncompressed_video_frame_size, handler->av_file) != handler->uncompressed_video_frame_size)
	{
	    fprintf(stderr, "Error writing video to file\n");
	    return WIAV_FAILURE;
	}
    }

#ifdef DEBUG_
    fprintf(handler->av_index, "Ending Video Frame at: %ld\n", ftell(handler->av_file));
#endif
    
    return WIAV_SUCCESS;    
}

wiav_type wiav_write_audio(wiav_handler *handler, void *abuffer, wiav_type *audio_size)
{
    
    if (handler->mode != WIAV_MODE_WRITING)
	return WIAV_FAILURE;

    if (handler->status == WIAV_STATUS_STOPPED)
	handler->status = WIAV_STATUS_RUNNING;

#ifdef DEBUG_
    fprintf(handler->av_index, "New Audio Frame at: %ld\n", ftell(handler->av_file));
#endif

    // fprintf(handler->av_file, "%d\n",*audio_size);
    put_int_and_0xa(handler->av_file, *audio_size);

    if (fwrite(abuffer, 1, *audio_size, handler->av_file) != *audio_size)
    {
	fprintf(stderr, "Error writing audio to file\n");
	return WIAV_FAILURE;
    }

#ifdef DEBUG_
    fprintf(handler->av_index, "Ending at: %ld\n", ftell(handler->av_file));
#endif
    
    return WIAV_SUCCESS;

}
