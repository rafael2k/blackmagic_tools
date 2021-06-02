#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#include "DeckLinkAPI.h"
#include "aux_functions.h"
#include "main.h"

extern "C" {
#include "ring_buffer.h"
#include "libwiav.h"
}

// Comment this out for production...
// #define VERBOSE 1

#define VIDEO_CHUNK_SIZE 4147200 // 1920*1080*2
#define AUDIO_CHUNK_SIZE 6400 // (16/8) * 2 * 1600 // 16 bits per sample, 2ch, 1s audio ~= 1600 samples per frame


// resources used when in non-multiplexed mode
pthread_t tid_video;
pthread_mutex_t mutex_video = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_video  = PTHREAD_COND_INITIALIZER;
struct ring_buffer buffer_video;

pthread_t tid_audio;
pthread_mutex_t mutex_audio = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_audio  = PTHREAD_COND_INITIALIZER;
struct ring_buffer buffer_audio;

// resources used when in multiplexed mode
pthread_t tid_mpx;
pthread_mutex_t mutex_mpx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_mpx  = PTHREAD_COND_INITIALIZER;
struct ring_buffer buffer_mpx;
wiav_handler *handler_mpx;

bool g_opt_planar_output = false;
bool g_opt_multiplexed_output = false;
bool g_opt_compressed_output = false;

BM_Capture bm_capture;


void *videoBufferDump(void *nothing)
{
    static uint8_t buffer[VIDEO_CHUNK_SIZE]; // 1920*1080*2
    static uint8_t buffer_planar[VIDEO_CHUNK_SIZE]; // 1920*1080*2

    void *addr;

    while (1)
    {
	
	if (ring_buffer_count_bytes (&buffer_video) >= VIDEO_CHUNK_SIZE) 
	{
	    pthread_mutex_lock( &mutex_video );
	    addr = ring_buffer_read_address (&buffer_video);
	    memcpy(buffer, addr, VIDEO_CHUNK_SIZE);
	    ring_buffer_read_advance (&buffer_video, VIDEO_CHUNK_SIZE);
	    pthread_mutex_unlock( &mutex_video );
	    if (g_opt_planar_output == true)
	    {
		packed_to_planar (buffer, buffer_planar, VIDEO_CHUNK_SIZE, 1920, 1080); // humm how good it would be in 4:2:0....
		fwrite(buffer_planar, 1, VIDEO_CHUNK_SIZE, bm_capture.video_output); // 4:2:2 chroma sub-sampling
	    }
	    else // if (g_opt_planar_output == false)
	    {  
		fwrite(buffer, 1, VIDEO_CHUNK_SIZE, bm_capture.video_output); // 4:2:2 chroma sub-sampling
	    }
	}
	else
	{
	    // we don't use usleep() anymore...
	    pthread_mutex_lock( &mutex_video );
	    pthread_cond_wait( &cond_video , &mutex_video );
	    pthread_mutex_unlock( &mutex_video );
	    // usleep (10);
	}
    }
}

void *audioBufferDump(void *nothing)
{
    static uint8_t buffer[AUDIO_CHUNK_SIZE*2];  // here we multiply by 2 to ensure will not overflow the audio buffer
    int audio_frame_size;
    int i;
    void *addr;

    while (1){
	
	if (ring_buffer_count_bytes (&buffer_audio) >= AUDIO_CHUNK_SIZE + 512) { // we make sure to make all the data we need in the buffer
	    pthread_mutex_lock( &mutex_audio );

	    // the size stuff
	    addr = ring_buffer_read_address (&buffer_audio);
	    memcpy(buffer, addr, 16); // the size will never exceed 16...
	    audio_frame_size = atoi((char *)buffer);
	    i = 0;
	    while (buffer[i] != '\n')
		i++;
	    ring_buffer_read_advance (&buffer_audio, i+1);

	    // audio payload stuff
	    addr = ring_buffer_read_address (&buffer_audio);
	    memcpy(buffer, addr, audio_frame_size);
	    ring_buffer_read_advance (&buffer_audio, audio_frame_size);
	    pthread_mutex_unlock( &mutex_audio );

	    // now writing...
	    fwrite(buffer, 1, audio_frame_size, bm_capture.audio_output); 
	}
	else{
	    // we don't use usleep() anymore...
	    pthread_mutex_lock( &mutex_audio );
	    pthread_cond_wait( &cond_audio , &mutex_audio );
	    pthread_mutex_unlock( &mutex_audio );
	    // usleep (10);
	}
    }
}

void *mpxBufferDump(void *nothing)
{
    int audio_frame_size;
    int i;
    void *addr;
    static uint8_t buffer[VIDEO_CHUNK_SIZE+(AUDIO_CHUNK_SIZE*2)]; // 1920*1080*2
    static uint8_t buffer_planar[VIDEO_CHUNK_SIZE]; // 1920*1080*2

    while (1)
    {
	
	if (ring_buffer_count_bytes (&buffer_mpx) >= VIDEO_CHUNK_SIZE + AUDIO_CHUNK_SIZE + 512) 
	{
	    // Video part
	    pthread_mutex_lock( &mutex_mpx );

	    addr = ring_buffer_read_address (&buffer_mpx);
	    memcpy(buffer, addr, VIDEO_CHUNK_SIZE);
	    ring_buffer_read_advance (&buffer_mpx, VIDEO_CHUNK_SIZE);

	    pthread_mutex_unlock( &mutex_mpx );

	    if (g_opt_planar_output == true)
	    {
		packed_to_planar (buffer, buffer_planar, VIDEO_CHUNK_SIZE, 1920, 1080); // humm how good it would be in 4:2:0....
		wiav_write_video (handler_mpx, buffer_planar);
	    }
	    else // if (g_opt_planar_output == false)
	    {  
		wiav_write_video(handler_mpx, buffer);
	    }


	    // Audio part
	    pthread_mutex_lock( &mutex_mpx );
	    
            // the size stuff
	    addr = ring_buffer_read_address (&buffer_mpx);
	    memcpy(buffer, addr, 16); // the size will never exceed 16...
	    audio_frame_size = atoi((char *)buffer);
	    i = 0;
	    while (buffer[i] != '\n')
		i++;
	    ring_buffer_read_advance (&buffer_mpx, i+1);

	    // audio payload stuff
	    addr = ring_buffer_read_address (&buffer_mpx);
	    memcpy(buffer, addr, audio_frame_size);
	    ring_buffer_read_advance (&buffer_mpx, audio_frame_size);
	    
	    pthread_mutex_unlock( &mutex_mpx );

	    wiav_write_audio(handler_mpx, buffer, &audio_frame_size);

	}
	else
	{
	    // we don't use usleep() anymore...
	    pthread_mutex_lock( &mutex_mpx );
	    pthread_cond_wait( &cond_mpx , &mutex_mpx );
	    pthread_mutex_unlock( &mutex_mpx );
	    // usleep (10);
	}
    }
    
}


HRESULT videoBufferFill(void *videoFrame, int frame_size){
    void *addr;

    if (ring_buffer_count_free_bytes (&buffer_video) >= frame_size){ // 1920*1080*2
	pthread_mutex_lock( &mutex_video );
	addr = ring_buffer_write_address (&buffer_video);
	memcpy(addr, videoFrame, frame_size);
	ring_buffer_write_advance (&buffer_video, frame_size);
	pthread_mutex_unlock( &mutex_video );

	pthread_cond_signal( &cond_video );
    }
    else{
	// do nothing...
	// busy waiting...
	fprintf(stderr, "Video Frame Dropped (Buffer full): %d bytes free, %d bytes occupied.\n", 
		ring_buffer_count_free_bytes (&buffer_video),
		ring_buffer_count_bytes (&buffer_video));
	fprintf(stderr, "Running out of sync...\n");
	return E_FAIL;
    }

    return S_OK;
}

HRESULT audioBufferFill(void *audioFrame, int frame_size){
    void *addr;

    if (ring_buffer_count_free_bytes (&buffer_audio) >= frame_size){ 
	pthread_mutex_lock( &mutex_audio );
	addr = ring_buffer_write_address (&buffer_audio);
	memcpy(addr, audioFrame, frame_size);
	ring_buffer_write_advance (&buffer_audio, frame_size);
	pthread_mutex_unlock( &mutex_audio );

	pthread_cond_signal( &cond_audio );
    }
    else{
	// do nothing...
	// busy waiting...
	fprintf(stderr, "Audio Frame Dropped (Buffer full): %d bytes free, %d bytes occupied.\n", 
		ring_buffer_count_free_bytes (&buffer_audio),
		ring_buffer_count_bytes (&buffer_audio));
	fprintf(stderr, "Running out of sync...\n");
	return E_FAIL;
    }

    return S_OK;

}

HRESULT mpxBufferFill(void *mpxFrame, int frame_size){
    void *addr;

    if (ring_buffer_count_free_bytes (&buffer_mpx) >= frame_size){ 
	pthread_mutex_lock( &mutex_mpx );
	addr = ring_buffer_write_address (&buffer_mpx);
	memcpy(addr, mpxFrame, frame_size);
	ring_buffer_write_advance (&buffer_mpx, frame_size);
	pthread_mutex_unlock( &mutex_mpx );

	pthread_cond_signal( &cond_mpx );
    }
    else{
	// do nothing...
	// busy waiting...
	fprintf(stderr, "MPX Frame Dropped (Buffer full): %d bytes free, %d bytes occupied.\n", 
		ring_buffer_count_free_bytes (&buffer_mpx),
		ring_buffer_count_bytes (&buffer_mpx));
	fprintf(stderr, "Running out of sync...\n");
	return E_FAIL;
    }

    return S_OK;

}


int main(int argc, char **argv){
    HRESULT result;
    BMDVideoConnection port_type = bmdVideoConnectionSDI;
    char *deviceNameString = NULL;
    char *multiplexed_output_filename = NULL;
    char *es_video_output_filename = NULL;
    char *es_audio_output_filename = NULL;
    int card_number = 0;
    int loop_var, opt;
    bool show_card_information_and_exit = false;
    
    if (argc < 2) {
    usage:
	fprintf(stderr, "Usage:\n %s [-p] [-z] [-c card number] [-t port_type] [-m multiplexed_output.wiav] [-v video.yuv] [-a audio.pcm]\n", argv[0]);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-p                           Output yuv422p\n");
	fprintf(stderr, "-c card number               Choose the card to use\n");
	fprintf(stderr, "-m multiplexed_output.wiav   Multiplexed output filename\n");
	fprintf(stderr, "-v video.yuv                 Elementary video stream output\n");
	fprintf(stderr, "-a audio.pcm                 Elementary audio stream output\n");
	fprintf(stderr, "-t port_type                 Possible options: SDI, HDMI\n");
	fprintf(stderr, "-z                           Compress the stream\n");
	fprintf(stderr, "or\n");
	fprintf(stderr, "-s                           Show capture cards information.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Be aware that you need to choose or multiplexed output or separate streams output\n");
	fprintf(stderr, "\n");	
	return EXIT_SUCCESS;
    }

    while ((opt = getopt(argc, argv, "spzt:c:m:v:a:")) != -1) {
	switch (opt) {
	case 't':
	    if (!strcmp("SDI", optarg))
		port_type = bmdVideoConnectionSDI;
	    if (!strcmp("HDMI", optarg))
		port_type = bmdVideoConnectionHDMI;
	    break;
	case 'p':
	    g_opt_planar_output = true;
	    break;
	case 'c':
	    card_number = atoi(optarg);
	    break;
	case 'm':
	    multiplexed_output_filename = optarg;
	    g_opt_multiplexed_output = true;
	    break;
	case 'v':
	    es_video_output_filename = optarg;
	    g_opt_multiplexed_output = false;
	    break;
	case 'a':
	    es_audio_output_filename = optarg;
	    g_opt_multiplexed_output = false;
	    break;
	case 's':
	    show_card_information_and_exit = true;
	    break;
	case 'z':
	    g_opt_compressed_output = true;
	    break;
	default: 
	    goto usage;
	}
    }
    

    if (g_opt_multiplexed_output == false)
    {
	// create out video buffer
	ring_buffer_create (&buffer_video, 28);

	// create out audio buffer
	ring_buffer_create (&buffer_audio, 24);
    }
    else
    {
	// create mpx out buffer
	ring_buffer_create (&buffer_mpx, 28);
    }


    // Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
    bm_capture.deckLinkIterator = CreateDeckLinkIteratorInstance();
    if (bm_capture.deckLinkIterator == NULL){
	fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
	return EXIT_FAILURE;
    }
    
    // Open get the pointer to the desired card
    for (loop_var = 0; loop_var <= card_number; loop_var++)
        if ((bm_capture.deckLinkIterator)->Next(&(bm_capture.deckLink)) != S_OK){
	    fprintf(stderr, "No Blackmagic Design devices were found.\n");
	    return EXIT_FAILURE;
	}
    
    if (show_card_information_and_exit == true){
	
        do{
	    // *** Print the model name of the DeckLink card
	    deviceNameString = NULL;
	    result = (bm_capture.deckLink)->GetModelName((const char **) &deviceNameString);
	    if (result == S_OK){
	        printf("\n\n");
	
		// char deviceName[64];
		printf("Card %d: =============== %s ===============\n\n", card_number, deviceNameString);
		free(deviceNameString);
	    
		print_attributes(bm_capture.deckLink);
		
		// ** List the video output display modes supported by the card
		print_output_modes(bm_capture.deckLink);
		
		// ** List the input and output capabilities of the card
		print_capabilities(bm_capture.deckLink);
		
		// Release the IDeckLink instance when we've finished with it to prevent leaks
		bm_capture.deckLink->Release();
		
	    }
	
	    printf("\n");
	    card_number++;
	
	}
	while( (bm_capture.deckLinkIterator)->Next(&(bm_capture.deckLink)) == S_OK);
	(bm_capture.deckLinkIterator)->Release();
	return EXIT_SUCCESS;
    }

    result = (bm_capture.deckLink)->GetModelName((const char **) &deviceNameString);
    if (result == S_OK)
    {
        printf("Using Card %d: %s.\n", card_number, deviceNameString);
	free(deviceNameString);
    }
    

    if (g_opt_multiplexed_output == false)
    {
	bm_capture.audio_output = fopen(es_audio_output_filename , "w");
	bm_capture.video_output = fopen(es_video_output_filename, "w");
	
	if (!bm_capture.audio_output || !bm_capture.video_output)
	{
	    if (!bm_capture.audio_output)
		fprintf(stderr, "%s could not be opened.\n", argv[2]);
	    if (!bm_capture.video_output)
		fprintf(stderr, "%s could not be opened.\n", argv[1]);
	    return EXIT_FAILURE;
	}
    }
    else // if (g_opt_multiplexed_output == true)
    {
	handler_mpx = wiav_init();

	if (g_opt_compressed_output == false)
	{
	    wiav_open_write(handler_mpx, multiplexed_output_filename, 
			    WIAV_HD_1080i_x_resolution,
			    WIAV_HD_1080i_y_resolution,
			    WIAV_YUYV,
			    WIAV_AUDIO_STEREO,
			    WIAV_AUDIO_16_BITS,
			    WIAV_VIDEO_8_BITS,
			    WIAV_INTERLACED,
			    WIAV_UNCOMPRESSED);
	}
	else // if (g_opt_compressed_output == true)
	{
	    wiav_open_write(handler_mpx, multiplexed_output_filename, 
			    WIAV_HD_1080i_x_resolution,
			    WIAV_HD_1080i_y_resolution,
			    WIAV_YUYV,
			    WIAV_AUDIO_STEREO,
			    WIAV_AUDIO_16_BITS,
			    WIAV_VIDEO_8_BITS,
			    WIAV_INTERLACED,
			    WIAV_COMPRESSED);
	}
    }

    

    // Obtain the audio/video input interface (IDeckLinkOutput)
    result = (bm_capture.deckLink)->QueryInterface(IID_IDeckLinkInput, (void**)&(bm_capture.deckLinkInput));
    check_result(result, "Error at QueryInterface/IID_IDeckLinkInput");

    result = (bm_capture.deckLinkInput)->StopStreams();
    if (result == S_OK){
      fprintf(stderr, "Streaming was stopped.\n");
    }
    
    // Obtain the audio/video configuration interface (IDeckLinkConfiguration)
    result = (bm_capture.deckLink)->QueryInterface(IID_IDeckLinkConfiguration, (void**)&(bm_capture.deckLinkConfiguration));
    check_result(result, "Error at QueryInterface/IID_IDeckLinkConfiguration");
    
    // Set video input capture interface
    // here we could use bmdVideoConnectionSDI, bmdVideoConnectionHDMI, bmdVideoConnectionOpticalSDI
    // bmdVideoConnectionComponent, bmdVideoConnectionComposite, bmdVideoConnectionSVideo 
    result = (bm_capture.deckLinkConfiguration)->SetVideoInputFormat(port_type);
    // result = (bm_capture.deckLinkConfiguration)->SetVideoInputFormat(bmdVideoConnectionComposite);
    check_result(result, "Error at SetVideoInputFormat");

    // Set audio input capture interface
    // here we could use bmdAudioConnectionEmbedded, bmdAudioConnectionAESEBU, bmdAudioConnectionAnalog
    // result = (bm_capture.deckLinkConfiguration)->SetAudioInputFormat(bmdAudioConnectionAnalog);
    result = (bm_capture.deckLinkConfiguration)->SetAudioInputFormat(bmdAudioConnectionEmbedded);
    check_result(result, "Error at SetAudioInputFormat");

    // here we could use bmdModeNTSC, bmdModeNTSC2398, bmdModePAL, bmdModeHD1080p2398, bmdModeHD1080p24,
    // bmdModeHD1080i50, bmdModeHD1080i5994, bmdModeHD720p50, bmdModeHD720p5994, bmdModeHD720p60
    // HARDCODED VALUES: bmdModeNTSC, bmdFormat8BitYUV
    result = (bm_capture.deckLinkInput)->EnableVideoInput(bmdModeHD1080i5994, bmdFormat8BitYUV, 0);
    // result = (bm_capture.deckLinkInput)->EnableVideoInput(bmdModeNTSC, bmdFormat8BitYUV, 0);
    check_result(result, "Error at EnableVideoInput");
    
    // Audio stuff
    // HARDCODED VALUES: bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2 (channels)
    result = (bm_capture.deckLinkInput)->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2);
    check_result(result, "Error at EnableAudioInput");
    
    result = (bm_capture.deckLinkInput)->SetCallback(&bm_capture);
    check_result(result, "Error at SetCallback");

    result = (bm_capture.deckLinkInput)->StartStreams();
    check_result(result, "Error at StartStreams");

    // lets sleep a little here?
    // how about 0.5s ? -- WHYYYYYYYYYYYYYYY
    //    sleep(0.3);
    
    if (g_opt_multiplexed_output == false)
    {
	// Call our threads...
	// lets start our video thread
	pthread_create(&tid_video, NULL, videoBufferDump, NULL);
    
	// lets start our audio thread
	pthread_create(&tid_audio, NULL, audioBufferDump, NULL);
    }
    else
    {
	pthread_create(&tid_mpx, NULL, mpxBufferDump, NULL);
    }
    
    if (g_opt_multiplexed_output == false)
    {
	pthread_join(tid_video, NULL);
	pthread_join(tid_audio, NULL);
    }
    else
    {
	pthread_join(tid_mpx, NULL);
    }

    result = (bm_capture.deckLinkInput)->Release();
    // check_result(result, "Error at Release");

    result = (bm_capture.deckLink)->Release();
    // check_result(result, "Error at Release");

    return 0;
}


BM_Capture::BM_Capture(){
    
}

HRESULT BM_Capture::VideoInputFrameArrived (/* in */ IDeckLinkVideoInputFrame *videoFrame, /* in */ IDeckLinkAudioInputPacket *audioPacket){
    static long long frame_count = 0;
    char buffer[16];
    long audio_sample_count;
    int audio_frame_size;
    HRESULT result;
    BMDTimeValue frameTime, frameDuration, packetTime;
    int	hours, minutes, seconds, frames;
    void *VideoFrame;
    void *AudioFrame;

    // Audio
    // GetSampleCount Get number of sample frames in packet
    audio_sample_count = audioPacket->GetSampleFrameCount();

#ifdef VERBOSE
    fprintf(stderr, "VideoInputFrameArrived %lld\n", frame_count++);
    
    // GetAudioPacketTime Get corresponding audio timestamp
    audioPacket->GetPacketTime (&packetTime, bmdAudioSampleRate48kHz);
    fprintf(stderr, "audio: %d samples, time %d\n", audio_sample_count, packetTime);

    // GetStreamTime Get corresponding video timestamp
    videoFrame->GetStreamTime(&frameTime, &frameDuration, 600);
 	
    hours = (frameTime / (600 * 60*60));
    minutes = (frameTime / (600 * 60)) % 60;
    seconds = (frameTime / 600) % 60;
    frames = (frameTime / 6) % 100;
    fprintf(stderr, "video frame timestamp: %02d:%02d:%02d:%02d ", hours, minutes, seconds, frames);
    fprintf(stderr, "\t %dx%d\n", videoFrame->GetWidth(), videoFrame->GetHeight() );

#endif

    // Get the video and audio data
    videoFrame->GetBytes(&VideoFrame);
    audioPacket->GetBytes(&AudioFrame);

    if (g_opt_multiplexed_output == false)
	result = videoBufferFill(VideoFrame, videoFrame->GetWidth() * videoFrame->GetHeight() * 2);
    else // (g_opt_multiplexed_output == true)
	result = mpxBufferFill(VideoFrame, videoFrame->GetWidth() * videoFrame->GetHeight() * 2);
    check_result(result, "Error at video/mpxBufferFill");
    
    audio_frame_size = audio_sample_count*4; // 2-byte per sample, 2 channels
    // here we store the audio_frame_size before the audio payload itself...
    sprintf(buffer,"%d\n", audio_frame_size);
    
    if (g_opt_multiplexed_output == false)
	result = audioBufferFill(buffer, strlen(buffer)); 
    else
	result = mpxBufferFill(buffer, strlen(buffer)); 
    check_result(result, "Error at audio/mpxBufferFill");

    if (g_opt_multiplexed_output == false)
	result = audioBufferFill(AudioFrame, audio_frame_size); 
    else
	result = mpxBufferFill(AudioFrame, audio_frame_size); 
    check_result(result, "Error at audio/mpxBufferFill");


    // // I left these fwrite's here to be able to return to the non-buffered version...
    // fwrite(VideoFrame, 1, videoFrame->GetWidth() * videoFrame->GetHeight() * 2, video_output); // 4:2:2 chroma sub-sampling
    // fflush(video_output);
    // fwrite(AudioFrame, 1, audio_sample_count*4, audio_output); // 2-byte per sample, 2 channels
    // fflush(audio_output);
    
    return S_OK;

}

HRESULT BM_Capture::VideoInputFormatChanged (/* in */ BMDVideoInputFormatChangedEvents notificationEvents, /* in */ IDeckLinkDisplayMode *newDisplayMode, /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags){
    static long long aux = 0;
    fprintf(stderr, "VideoInputFormatChanged %lld\n", aux++);

    return S_OK;
}

BM_Capture::~BM_Capture () {}


