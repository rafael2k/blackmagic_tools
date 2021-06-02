#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdbool.h>

#include "DeckLinkAPI.h"
#include "aux_functions.h"
#include "main.h"

extern "C"{
#include "ring_buffer.h"
#include "libwiav.h"
}


// Comment this out for production...
// #define VERBOSE 1

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

// max video chunk size
#define VIDEO_CHUNK_SIZE 4147200 // 1920*1080*2
#define AUDIO_CHUNK_SIZE 6400 // (16/8) * 2 * 1600 // 16 bits per sample, 2ch, ~1600 samples per frame


const unsigned long		kAudioWaterlevel = 48000;


pthread_t tid_video;
pthread_mutex_t mutex_video = PTHREAD_MUTEX_INITIALIZER;
struct ring_buffer buffer_video;


pthread_t tid_audio;
pthread_mutex_t mutex_audio = PTHREAD_MUTEX_INITIALIZER;
struct ring_buffer buffer_audio;

pthread_t tid_mpx;
wiav_handler *handler_mpx;

pthread_mutex_t mutex_eof = PTHREAD_MUTEX_INITIALIZER;

// lets make myself global...
BM_Play bm_play;

bool video_running;
bool audio_running;

bool g_opt_multiplexed_input = false;

int video_frame_size;

void *videoBufferFill(void *nothing){
    static uint8_t buffer[VIDEO_CHUNK_SIZE]; // 1920*1080*2
    void *addr;

    while (1){

        // When we stop filling our buffer???
        // We have to pick a value...

	if (ring_buffer_count_free_bytes (&buffer_video) >= video_frame_size){ // 1920*1080*2
	    
	    if (fread(buffer, 1, video_frame_size, bm_play.video_input) != video_frame_size){ // 1920*1080*2
		fprintf(stderr, "End of Video Stream.\n");
		video_running = false;
		pthread_exit(NULL);
	    }
	    
	    pthread_mutex_lock( &mutex_video );
	    addr = ring_buffer_write_address (&buffer_video);
	    memcpy(addr, buffer, video_frame_size);
	    ring_buffer_write_advance (&buffer_video, video_frame_size);
	    pthread_mutex_unlock( &mutex_video );
	}
	else{
	    // do nothing...
	    // busy waiting...
	    usleep(10);
	}
    }
    
}

void *audioBufferFill(void *nothing){
    static uint8_t buffer[AUDIO_CHUNK_SIZE]; // (16/8) * 2 * 1600 // 16 bits per sample, 2ch, ~1600 samples per frame
    void *addr;

    while (1){

	if (ring_buffer_count_free_bytes (&buffer_audio) >= 2 * SDI_CHANNELS)
	{ 
	    
	    if (fread(buffer, 1, 2 * bm_play.audioChannelCount, bm_play.audio_input) != 2 * bm_play.audioChannelCount){ // (16/8) * 2 * 1600
		fprintf(stderr, "End of Audio Stream.\n");
		audio_running = false;
		pthread_exit(NULL);
	    }
	    
	    pthread_mutex_lock( &mutex_audio );
	    addr = ring_buffer_write_address (&buffer_audio);
	    memcpy(addr, buffer, 2 * bm_play.audioChannelCount);
	    
	    
	    if (bm_play.audioChannelCount != 2 && bm_play.audioChannelCount < SDI_CHANNELS)
		memset(addr + (2 * bm_play.audioChannelCount), 0, 2 *  (SDI_CHANNELS - bm_play.audioChannelCount));
		       
	    if (bm_play.audioChannelCount == 2)
		ring_buffer_write_advance (&buffer_audio, 2 * bm_play.audioChannelCount);
	    else
		ring_buffer_write_advance (&buffer_audio, 2 * SDI_CHANNELS);

	    pthread_mutex_unlock( &mutex_audio );
	}
	else{
	    // do nothing...
	    // busy waiting...
	    usleep(10);
	}
    }
    
}

void *mpxBufferFill(void *nothing){
    static uint8_t buffer[VIDEO_CHUNK_SIZE+(AUDIO_CHUNK_SIZE*2)]; // Are you happy? I'm happy.
    void *addr;
    wiav_type audio_size;

    while (1){

	if ( ( ring_buffer_count_free_bytes (&buffer_audio) >= AUDIO_CHUNK_SIZE+512 ) // (16/8) * 2 * 1600 + we do not overflow
	     && ( ring_buffer_count_free_bytes (&buffer_video) >= video_frame_size ) // 1920*1080*2
	    )
	{

	    if (wiav_read_video(handler_mpx, buffer) == WIAV_FAILURE)
	    {
		fprintf(stderr, "End of Video Stream.\n");
	        video_running = false;
		pthread_exit(NULL);
	    }
	    
	    pthread_mutex_lock( &mutex_video );
	    addr = ring_buffer_write_address (&buffer_video);
	    memcpy(addr, buffer, video_frame_size);
	    ring_buffer_write_advance (&buffer_video, video_frame_size);
	    pthread_mutex_unlock( &mutex_video );

	    
	    if (wiav_read_audio(handler_mpx, buffer, &audio_size) == WIAV_FAILURE) 
	    {
		fprintf(stderr, "End of Audio Stream.\n");
		audio_running = false;
		pthread_exit(NULL);
	    }
	    
	    pthread_mutex_lock( &mutex_audio );
	    addr = ring_buffer_write_address (&buffer_audio);
	    memcpy(addr, buffer, audio_size);
	    ring_buffer_write_advance (&buffer_audio, audio_size);
	    pthread_mutex_unlock( &mutex_audio );

	}
	else{
	    // do nothing...
	    // busy waiting...
	    usleep(10);
	}
    }
    
}

HRESULT FillNextVideoFrame(IDeckLinkVideoFrame *VideoFrame){
    void *video_data;
    void *addr;
    static uint64_t frame_number = 0;


    (bm_play.deckLinkOutput)->CreateVideoFrame(bm_play.frameWidth, bm_play.frameHeight, bm_play.frameWidth*2, bmdFormat8BitYUV, bmdFrameFlagDefault, &(bm_play.VideoFrame));
    
    (bm_play.VideoFrame)->GetBytes((void**)&video_data);

again:
    if (ring_buffer_count_bytes (&buffer_video) >= video_frame_size) {
	    pthread_mutex_lock( &mutex_video );
	    addr = ring_buffer_read_address (&buffer_video);
	    memcpy(video_data, addr, video_frame_size);
	    ring_buffer_read_advance (&buffer_video, video_frame_size);
	    pthread_mutex_unlock( &mutex_video );
    }
    else{
	if (audio_running == false || video_running == false)
	    pthread_mutex_unlock ( &mutex_eof );
	else
	    usleep (10); // TODO: fix this sleep please...
	
	goto again;
    }

    fprintf(stderr,"   %09llu\r", frame_number);

    frame_number++;

    VideoFrame->Release();

    return S_OK;
}

HRESULT FillNextAudioFrame(void *audio_buffer, uint32_t size){
// octave:3> (48000/1601 + 48000/1602 + 48000/1601 + 48000/1602 +  48000/1602) / 5
// ans =  29.970

    void *addr;

again:
    if (ring_buffer_count_bytes (&buffer_audio) >= size) {
	    pthread_mutex_lock( &mutex_audio );
	    addr = ring_buffer_read_address (&buffer_audio);
	    memcpy(audio_buffer, addr, size);
	    ring_buffer_read_advance (&buffer_audio, size);
	    pthread_mutex_unlock( &mutex_audio );
    }
    else{
	if (audio_running == false || video_running == false)
	    pthread_mutex_unlock ( &mutex_eof );
	else
	    usleep (10);

	goto again;
    }
    
    return S_OK;
}

int main(int argc, char **argv){
    HRESULT result;
    BMDVideoConnection port_type = bmdVideoConnectionSDI;
    char output_type[64];
    char *deviceNameString = NULL;
    char *multiplexed_input_filename = NULL;
    char *es_audio_input_filename = NULL;
    char *es_video_input_filename = NULL;
    int card_number = 0;
    int loop_var, opt;
    bool show_card_information_and_exit = false;

    // default value
    bm_play.audioChannelCount = 2;
    strcpy(output_type, "HD 1080i 59.94");
    video_frame_size = 1920 * 1080 * 2;

    
    if (argc < 2) {
    usage:
	fprintf(stderr, "Usage:\n %s [-c card number] [-t port_type] [-m multiplexed_output.wiav] [-q audio_channels] [-v video.yuv] [-a audio.pcm]\n", argv[0]);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-c card number               Choose the card to use\n");
	fprintf(stderr, "-m multiplexed_input.wiav    Multiplexed input filename\n");
	fprintf(stderr, "-v video.yuv                 Elementary video stream input\n");
	fprintf(stderr, "-a audio.pcm                 Elementary audio stream input\n");
	fprintf(stderr, "-q audio_channels            Number of audio channels\n");
	fprintf(stderr, "-t port_type                 Possible options: SDI, HDMI\n");
	fprintf(stderr, "-r video_resolution          Possible options: 1080i, NTSC\n");
	fprintf(stderr, "or\n");
	fprintf(stderr, "-s                           Show capture cards information.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Be aware that you need to choose or multiplexed input or separate streams input\n");
	fprintf(stderr, "\n");	
	return EXIT_SUCCESS;
    }
    
    while ((opt = getopt(argc, argv, "sr:t:c:m:v:a:q:")) != -1) {
	switch (opt) {
	case 'r':
	    if (!strcmp("1080i", optarg))
	    {
		strcpy(output_type, "HD 1080i 59.94");
		video_frame_size = 1920 * 1080 * 2;
	    }
	    if (!strcmp("480i", optarg))
	    {
		strcpy(output_type, "NTSC");
		video_frame_size = 720 * 486 * 2;
	    }
	    break;
	case 't':
	    if (!strcmp("SDI", optarg))
		port_type = bmdVideoConnectionSDI;
	    if (!strcmp("HDMI", optarg))
		port_type = bmdVideoConnectionHDMI;
	    break;
	case 'c':
	    card_number = atoi(optarg);
	    break;
	case 'm':
	    multiplexed_input_filename = optarg;
	    g_opt_multiplexed_input = true;
	    break;
	case 'v':
	    es_video_input_filename = optarg;
	    g_opt_multiplexed_input = false;
	    break;
	case 'a':
	    es_audio_input_filename = optarg;
	    g_opt_multiplexed_input = false;
	    break;
	case 's':
	    show_card_information_and_exit = true;
	    break;
	case 'q':
	    bm_play.audioChannelCount = atoi(optarg);
	    break;
	default: 
	    goto usage;
	}
    }

    // create out video buffer
    ring_buffer_create (&buffer_video, 28);
    
    // create out audio buffer
    ring_buffer_create (&buffer_audio, 24);
    
    // Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
    bm_play.deckLinkIterator = CreateDeckLinkIteratorInstance();
    if (bm_play.deckLinkIterator == NULL){
	fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
	return EXIT_FAILURE;
    }
    
    // Open get the pointer to the desired card
    for (loop_var = 0; loop_var <= card_number; loop_var++)
        if ((bm_play.deckLinkIterator)->Next(&(bm_play.deckLink)) != S_OK)
	{
	    fprintf(stderr, "No Blackmagic Design devices were found.\n");
	    return EXIT_FAILURE;
	}
    
    if (show_card_information_and_exit == true)
    {
	
        do
	{
	    // *** Print the model name of the DeckLink card
	    deviceNameString = NULL;
	    result = (bm_play.deckLink)->GetModelName((const char **) &deviceNameString);
	    if (result == S_OK){
	        printf("\n\n");
	
		// char deviceName[64];
		printf("Card %d: =============== %s ===============\n\n", card_number, deviceNameString);
		free(deviceNameString);
	    
		print_attributes(bm_play.deckLink);
		
		// ** List the video output display modes supported by the card
		print_output_modes(bm_play.deckLink);
		
		// ** List the input and output capabilities of the card
		print_capabilities(bm_play.deckLink);
		
		// Release the IDeckLink instance when we've finished with it to prevent leaks
		bm_play.deckLink->Release();
		
	    }
	
	    printf("\n");
	    card_number++;
	
	}
	while( (bm_play.deckLinkIterator)->Next(&(bm_play.deckLink)) == S_OK);
	(bm_play.deckLinkIterator)->Release();
	return EXIT_SUCCESS;
    }

    result = (bm_play.deckLink)->GetModelName((const char **) &deviceNameString);
    if (result == S_OK)
    {
        printf("Using Card %d: %s.\n", card_number, deviceNameString);
	free(deviceNameString);
    }

    if (g_opt_multiplexed_input == false)
    {
	bm_play.audio_input = fopen(es_audio_input_filename, "r");
	bm_play.video_input = fopen(es_video_input_filename, "r");
    
	if (!bm_play.audio_input || !bm_play.video_input)
	{
	    if (!bm_play.audio_input)
	    fprintf(stderr, "%s could not be opened.\n", argv[2]);
	    if (!bm_play.video_input)
		fprintf(stderr, "%s could not be opened.\n", argv[1]);
	    return EXIT_FAILURE;
	}
    }
    else // if (g_opt_multiplexed_input == true)
    {
	handler_mpx = wiav_init();
	wiav_open_read(handler_mpx, multiplexed_input_filename);
    }

    // Obtain the audio/video output interface (IDeckLinkOutput)
    result = (bm_play.deckLink)->QueryInterface(IID_IDeckLinkOutput, (void**)&(bm_play.deckLinkOutput));
    check_result(result, "Error at QueryInterface/IID_IDeckLinkOutput");

    // Obtain the audio/video configuration interface (IDeckLinkConfiguration)
    result = (bm_play.deckLink)->QueryInterface(IID_IDeckLinkConfiguration, (void**)&(bm_play.deckLinkConfiguration));
    check_result(result, "Error at QueryInterface/IID_IDeckLinkConfiguration");


    // Set video input capture interface
    // here we could use bmdVideoConnectionSDI, bmdVideoConnectionHDMI, bmdVideoConnectionOpticalSDI
    // bmdVideoConnectionComponent, bmdVideoConnectionComposite, bmdVideoConnectionSVideo 
    result = (bm_play.deckLinkConfiguration)->SetVideoOutputFormat(port_type);
    check_result(result, "Error at SetVideoOutputFormat");
    
    // Set audio input capture interface
    // here we could use bmdAudioConnectionEmbedded, bmdAudioConnectionAESEBU, bmdAudioConnectionAnalog
    // result = (bm_play.deckLinkConfiguration)->SetAudioInputFormat(bmdAudioConnectionAnalog);
//    result = (bm_play.deckLinkConfiguration)->SetAudioInputFormat(bmdAudioConnectionEmbedded);
//    check_result(result, "Error at SetAudioInputFormat");

    
    // Set the video callback
    result = (bm_play.deckLinkOutput)->SetScheduledFrameCompletionCallback(&bm_play);
    check_result(result, "SetScheduledFrameCompletionCallback");

    // Set the audio callback
    result = (bm_play.deckLinkOutput)->SetAudioCallback(&bm_play);
    check_result(result, "SetAudioCallback");

       
    // Obtain an IDeckLinkDisplayModeIterator to enumerate the
    // display modes supported on input
    result = (bm_play.deckLinkOutput)->GetDisplayModeIterator (&(bm_play.displayModeIterator));
    check_result(result, "GetDisplayModeIterator");

    
    // Lets Pick the right mode...
    const char *displayModeString = NULL;
    while ((bm_play.displayModeIterator)->Next (&(bm_play.displayMode)) == S_OK){

	result = (bm_play.displayMode)->GetName (&displayModeString);
	
	if (!strcmp(output_type, displayModeString)){
	    printf("Mode: %s.\n", displayModeString);
	    break;
	}

        // Release the IDeckLinkDisplayMode object to prevent a leak
        (bm_play.displayMode)->Release ();
    }
    (bm_play.displayModeIterator)->Release();

    // video settings
    bm_play.frameWidth = (bm_play.displayMode)->GetWidth();
    bm_play.frameHeight = (bm_play.displayMode)->GetHeight();
	
    (bm_play.displayMode)->GetFrameRate(&(bm_play.frameDuration), &(bm_play.frameTimescale));
    // Calculate the number of frames per second, rounded up to the nearest integer.  For example, for NTSC (29.97 FPS), framesPerSecond == 30.
    bm_play.framesPerSecond = (bm_play.frameTimescale + (bm_play.frameDuration-1))  /  bm_play.frameDuration;
    
    // audio settings
    bm_play.audioSampleDepth = 16;
    bm_play.audioSampleRate = bmdAudioSampleRate48kHz;

    if (g_opt_multiplexed_input == false)
    {
	// lets start our video thread
	pthread_create(&tid_video, NULL, videoBufferFill, NULL);

	// lets start our audio thread
	pthread_create(&tid_audio, NULL, audioBufferFill, NULL);
    }
    else
    {
	// out mpx thread
	pthread_create(&tid_mpx, NULL, mpxBufferFill, NULL);
    }

    // here we could use bmdModeNTSC, bmdModeNTSC2398, bmdModePAL, bmdModeHD1080p2398, bmdModeHD1080p24,
    // bmdModeHD1080i50, bmdModeHD1080i5994, bmdModeHD720p50, bmdModeHD720p5994, bmdModeHD720p60
    // HARDCODED VALUES: bmdModeNTSC, bmdFormat8BitYUV
    result = (bm_play.deckLinkOutput)->EnableVideoOutput((bm_play.displayMode)->GetDisplayMode(), bmdVideoOutputFlagDefault);
    check_result(result, "Error at EnableVideoOutput");

    // Audio stuff
    // HARDCODED VALUES: bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2 (channels)
    if (bm_play.audioChannelCount == 2)
    {      
        result = (bm_play.deckLinkOutput)->EnableAudioOutput(bm_play.audioSampleRate, bm_play.audioSampleDepth, 2, bmdAudioOutputStreamContinuous/*bmdAudioOutputStreamTimestamped*/);
    }
    else
    {
        result = (bm_play.deckLinkOutput)->EnableAudioOutput(bm_play.audioSampleRate, bm_play.audioSampleDepth, SDI_CHANNELS, bmdAudioOutputStreamContinuous/*bmdAudioOutputStreamTimestamped*/);
    }
    check_result(result, "Error at EnableAudioInput");
    
    // DO THE PRE-ROLLING.
    // create 1s of row in the audio buffer
    bm_play.audioSamplesPerFrame = ((bm_play.audioSampleRate * bm_play.frameDuration) / bm_play.frameTimescale);
    bm_play.audioBufferSampleLength = (bm_play.framesPerSecond * bm_play.audioSampleRate * bm_play.frameDuration) / bm_play.frameTimescale;

    if (bm_play.audioChannelCount == 2)
      bm_play.audio_buffer = malloc(bm_play.audioBufferSampleLength * bm_play.audioChannelCount * (bm_play.audioSampleDepth / 8));
    else
      bm_play.audio_buffer = malloc(bm_play.audioBufferSampleLength * SDI_CHANNELS * (bm_play.audioSampleDepth / 8));

    bm_play.totalFramesScheduled = 0;
    for (int i = 0; i < bm_play.framesPerSecond; i++){
	result = (bm_play.deckLinkOutput)->CreateVideoFrame(bm_play.frameWidth, bm_play.frameHeight, bm_play.frameWidth*2, bmdFormat8BitYUV, bmdFrameFlagDefault, &(bm_play.VideoFrame));
	check_result(result, "Error at CreateVideoFrame");
	
	FillNextVideoFrame(bm_play.VideoFrame);

	result = (bm_play.deckLinkOutput)->ScheduleVideoFrame(bm_play.VideoFrame, (bm_play.totalFramesScheduled * bm_play.frameDuration), bm_play.frameDuration, bm_play.frameTimescale);
	check_result(result, "Error at ScheduleVideoFrame");
	
	bm_play.totalFramesScheduled++;

    }

    video_running = true;
    audio_running = true;
 
    bm_play.audioBufferOffset = 0;
    result = (bm_play.deckLinkOutput)->BeginAudioPreroll();
    check_result(result, "BeginAudioPreroll");
 
    if (g_opt_multiplexed_input == false){
	pthread_join(tid_video, NULL);
	pthread_join(tid_audio, NULL);
    }
    else
    {
	pthread_join(tid_mpx, NULL);
    }

    pthread_mutex_lock( &mutex_eof ); // when unlocked, program exits.
    
    result = (bm_play.deckLinkOutput)->Release();
    // check_result(result, "Error at Release");

    result = (bm_play.deckLink)->Release();
    // check_result(result, "Error at Release");

    return 0;
}


BM_Play::BM_Play()
{
    
}

HRESULT BM_Play::ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
    
    // treat errors??
    // if (result != bmdOutputFrameCompleted)

    FillNextVideoFrame(completedFrame);
    
    result = deckLinkOutput->ScheduleVideoFrame(VideoFrame, totalFramesScheduled * frameDuration, frameDuration, frameTimescale);
    check_result(result, "Error at ScheduleVideoFrame");

    totalFramesScheduled++;
    
    return S_OK;

}

HRESULT BM_Play::ScheduledPlaybackHasStopped ()
{

    return S_OK;
}

HRESULT BM_Play::RenderAudioSamples (bool preroll)
{
    uint32_t Audio_Frames_Buffered;

    
    // Try to maintain the number of audio samples buffered in the API at a specified waterlevel
    if ((deckLinkOutput->GetBufferedAudioSampleFrameCount(&Audio_Frames_Buffered) == S_OK) && (Audio_Frames_Buffered < kAudioWaterlevel)) {

	unsigned int		samplesToEndOfBuffer;
	unsigned int		samplesToWrite;
	unsigned int		samplesWritten;
		
	samplesToEndOfBuffer = (audioBufferSampleLength - bm_play.audioBufferOffset);
	samplesToWrite = (kAudioWaterlevel - Audio_Frames_Buffered);
	if (samplesToWrite > samplesToEndOfBuffer)
	    samplesToWrite = samplesToEndOfBuffer;
	

#ifdef ARCH32_
	if (audioChannelCount == 2)
	    FillNextAudioFrame((void*)((uint32_t)audio_buffer + (audioBufferOffset * audioChannelCount * audioSampleDepth/8)), samplesToWrite * audioChannelCount * audioSampleDepth/8);
	else
	    FillNextAudioFrame((void*)((uint32_t)audio_buffer + (audioBufferOffset * SDI_CHANNELS * audioSampleDepth/8)), samplesToWrite * SDI_CHANNELS * audioSampleDepth/8);
	
	if (audioChannelCount == 2)
	{
	    if (deckLinkOutput->ScheduleAudioSamples((void*)((uint32_t)audio_buffer + (audioBufferOffset * audioChannelCount * audioSampleDepth/8)), samplesToWrite, 0, audioSampleRate, &samplesWritten) == S_OK)
	        audioBufferOffset = ((audioBufferOffset + samplesWritten) % audioBufferSampleLength);
	}
	else
	{
	    if (deckLinkOutput->ScheduleAudioSamples((void*)((uint32_t)audio_buffer + (audioBufferOffset * SDI_CHANNELS * audioSampleDepth/8)), samplesToWrite, 0, audioSampleRate, &samplesWritten) == S_OK)
	        audioBufferOffset = ((audioBufferOffset + samplesWritten) % audioBufferSampleLength);
	}
	  
#endif

#ifdef ARCH64_
	if (audioChannelCount == 2)
	    FillNextAudioFrame((void*)((uint64_t)audio_buffer + (audioBufferOffset * audioChannelCount * audioSampleDepth/8)), samplesToWrite * audioChannelCount * audioSampleDepth/8);
	else
	    FillNextAudioFrame((void*)((uint64_t)audio_buffer + (audioBufferOffset * SDI_CHANNELS * audioSampleDepth/8)), samplesToWrite * SDI_CHANNELS * audioSampleDepth/8);


	if (audioChannelCount == 2)
	{
	    if (deckLinkOutput->ScheduleAudioSamples((void*)((uint64_t)audio_buffer + (audioBufferOffset * audioChannelCount * audioSampleDepth/8)), samplesToWrite, 0, audioSampleRate, &samplesWritten) == S_OK)
	        audioBufferOffset = ((audioBufferOffset + samplesWritten) % audioBufferSampleLength);
	}
	else
	{
	    if (deckLinkOutput->ScheduleAudioSamples((void*)((uint64_t)audio_buffer + (audioBufferOffset * SDI_CHANNELS * audioSampleDepth/8)), samplesToWrite, 0, audioSampleRate, &samplesWritten) == S_OK)
	        audioBufferOffset = ((audioBufferOffset + samplesWritten) % audioBufferSampleLength);
	}

#endif

    }

    
    if (preroll == true){
	
	deckLinkOutput->StartScheduledPlayback(0, frameTimescale, 1.0);
	
    }

    return S_OK;
}

BM_Play::~BM_Play () {}


