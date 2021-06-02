#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#include "DeckLinkAPI.h"
#include "aux_functions.h"
#include "main.h"

// Comment this out for production...
// #define VERBOSE 1

const unsigned long		kAudioWaterlevel = 48000;

HRESULT FillNextVideoFrame(FILE *video_input, IDeckLinkVideoFrame *VideoFrame){
    void *video_data;

    VideoFrame->GetBytes((void**)&video_data);

    fread(video_data,1, VideoFrame->GetWidth() * VideoFrame->GetHeight() * 2 ,video_input);
    
    return S_OK;
}

HRESULT FillNextAudioFrame(FILE *audio_input , void *audio_buffer, uint32_t size){
// octave:3> (48000/1601 + 48000/1602 + 48000/1601 + 48000/1602 +  48000/1602) / 5
// ans =  29.970

    // audio_sample_count = 1602;

    fread(audio_buffer, 1, size, audio_input); // 2 channels, 2 bytes per sample

    return S_OK;
}

int main(int argc, char **argv){
    HRESULT result;
    char *deviceNameString = NULL;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    BM_Play bm_play;

    if (argc != 3 && argc != 2) {
    usage:
	fprintf(stderr, "Usage:\n bm_play video.yuv audio.pcm\n");
	fprintf(stderr, "or\n");
	fprintf(stderr, " bm_play -s\n");
	fprintf(stderr, "to show capture card information.\n");
	return EXIT_SUCCESS;	
    }

    // Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
    bm_play.deckLinkIterator = CreateDeckLinkIteratorInstance();
    if (bm_play.deckLinkIterator == NULL){
	fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
	return EXIT_FAILURE;
    }
    
    // Open get the pointer to the first card
    if ((bm_play.deckLinkIterator)->Next(&(bm_play.deckLink)) != S_OK){
	fprintf(stderr, "No Blackmagic Design devices were found.\n");
	return EXIT_FAILURE;
    }
    
    if (!strcmp(argv[1], "-s")){
	
	// *** Print the model name of the DeckLink card
	result = (bm_play.deckLink)->GetModelName((const char **) &deviceNameString);
	if (result == S_OK){
	    printf("\n\n");
	
	    // char deviceName[64];
	    printf("=============== %s ===============\n\n", deviceNameString);
	    free(deviceNameString);
	    
	    print_attributes(bm_play.deckLink);
		
	    // ** List the video output display modes supported by the card
	    print_output_modes(bm_play.deckLink);
		
	    // ** List the input and output capabilities of the card
	    print_capabilities(bm_play.deckLink);
		
	    // Release the IDeckLink instance when we've finished with it to prevent leaks
	    bm_play.deckLink->Release();

	    (bm_play.deckLinkIterator)->Release();
	}
	
	printf("\n");
	return EXIT_SUCCESS;
    }
    else{
	if (argc != 3)
	    goto usage;
    }

    bm_play.audio_input = fopen(argv[2], "r");
    bm_play.video_input = fopen(argv[1], "r");
    
    if (!bm_play.audio_input || !bm_play.video_input){
	if (!bm_play.audio_input)
	    fprintf(stderr, "%s could not be opened.\n", argv[2]);
	if (!bm_play.video_input)
	    fprintf(stderr, "%s could not be opened.\n", argv[1]);
	return EXIT_FAILURE;
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
    result = (bm_play.deckLinkConfiguration)->SetVideoOutputFormat(bmdVideoConnectionSDI);
    // result = (bm_play.deckLinkConfiguration)->SetVideoInputFormat(bmdVideoConnectionComposite);
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
    /*
    result = (bm_play.deckLinkOutput)->SetAudioCallback(&bm_play);
    check_result(result, "SetAudioCallback");
    */
       
    // Obtain an IDeckLinkDisplayModeIterator to enumerate the
    // display modes supported on input
    result = (bm_play.deckLinkOutput)->GetDisplayModeIterator (&(bm_play.displayModeIterator));
    check_result(result, "GetDisplayModeIterator");

    
    // Lets Pick the right mode...
    const char *displayModeString = NULL;
    while ((bm_play.displayModeIterator)->Next (&(bm_play.displayMode)) == S_OK){

	result = (bm_play.displayMode)->GetName (&displayModeString);
	
	if (!strncmp("HD 1080i 59.94", displayModeString, 14)){
	    printf("Mode %s chosed.\n", displayModeString);
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
    bm_play.audioChannelCount = 2;

    // here we could use bmdModeNTSC, bmdModeNTSC2398, bmdModePAL, bmdModeHD1080p2398, bmdModeHD1080p24,
    // bmdModeHD1080i50, bmdModeHD1080i5994, bmdModeHD720p50, bmdModeHD720p5994, bmdModeHD720p60
    // HARDCODED VALUES: bmdModeNTSC, bmdFormat8BitYUV
    result = (bm_play.deckLinkOutput)->EnableVideoOutput((bm_play.displayMode)->GetDisplayMode(), bmdVideoOutputFlagDefault);
    check_result(result, "Error at EnableVideoOutput");
    
    // Audio stuff
    // HARDCODED VALUES: bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2 (channels)

    /*
    result = (bm_play.deckLinkOutput)->EnableAudioOutput(bm_play.audioSampleRate, bm_play.audioSampleDepth, bm_play.audioChannelCount, bmdAudioOutputStreamTimestamped);
    check_result(result, "Error at EnableAudioInput");
    */

    // DO THE PRE-ROLLING.
    // create 1s of row in the audio buffer
    bm_play.audioSamplesPerFrame = ((bm_play.audioSampleRate * bm_play.frameDuration) / bm_play.frameTimescale);
    bm_play.audioBufferSampleLength = (bm_play.framesPerSecond * bm_play.audioSampleRate * bm_play.frameDuration) / bm_play.frameTimescale;
    bm_play.audio_buffer = malloc(bm_play.audioBufferSampleLength * bm_play.audioChannelCount * (bm_play.audioSampleDepth / 8));


    bm_play.totalAudioSecondsScheduled = 0;

    // 1sec prerolling
    /* 
    FillNextAudioFrame(bm_play.audio_input , bm_play.audio_buffer, bm_play.audioBufferSampleLength * bm_play.audioChannelCount * (bm_play.audioSampleDepth / 8));
    (bm_play.deckLinkOutput)->ScheduleAudioSamples(bm_play.audio_buffer, bm_play.audioSamplesPerFrame, (bm_play.totalAudioSecondsScheduled * bm_play.audioBufferSampleLength), bm_play.audioSampleRate, NULL);
    bm_play.totalAudioSecondsScheduled++;
    */


    bm_play.totalFramesScheduled = 0;
    for (int i = 0; i < bm_play.framesPerSecond; i++){
	result = (bm_play.deckLinkOutput)->CreateVideoFrame(bm_play.frameWidth, bm_play.frameHeight, bm_play.frameWidth*2, bmdFormat8BitYUV, bmdFrameFlagDefault, &(bm_play.VideoFrame));
	check_result(result, "Error at CreateVideoFrame");
	
	FillNextVideoFrame(bm_play.video_input, bm_play.VideoFrame);

	result = (bm_play.deckLinkOutput)->ScheduleVideoFrame(bm_play.VideoFrame, (bm_play.totalFramesScheduled * bm_play.frameDuration), bm_play.frameDuration, bm_play.frameTimescale);
	check_result(result, "Error at ScheduleVideoFrame");
	
	bm_play.totalFramesScheduled++;

    }
    
    bm_play.audioBufferOffset = 0;

    /*
    result = (bm_play.deckLinkOutput)->BeginAudioPreroll();
    check_result(result, "BeginAudioPreroll");
    */
    (bm_play.deckLinkOutput)->StartScheduledPlayback(0, 100, 1.0);

    pthread_cond_wait(&cond, &mutex);
    // pthread_cond_wait(&cond, &mutex);

    result = (bm_play.deckLinkOutput)->Release();
    // check_result(result, "Error at Release");

    result = (bm_play.deckLink)->Release();
    // check_result(result, "Error at Release");

    return 0;
}


BM_Play::BM_Play(){
    
}

HRESULT BM_Play::ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result){
    
    // treat errors??
    // if (result != bmdOutputFrameCompleted)

    FillNextVideoFrame(video_input, completedFrame);
    
    result = deckLinkOutput->ScheduleVideoFrame(VideoFrame, totalFramesScheduled * frameDuration, frameDuration, frameTimescale);
    check_result(result, "Error at ScheduleVideoFrame");

    totalFramesScheduled++;
    
    return S_OK;

}

HRESULT BM_Play::ScheduledPlaybackHasStopped (){

    return S_OK;
}

HRESULT BM_Play::RenderAudioSamples (bool preroll){
    uint32_t Audio_Frames_Buffered;

    
    // Try to maintain the number of audio samples buffered in the API at a specified waterlevel
    if ((deckLinkOutput->GetBufferedAudioSampleFrameCount(&Audio_Frames_Buffered) == S_OK) && (Audio_Frames_Buffered < kAudioWaterlevel))
    {
	uint32_t samplesToEndOfBuffer;
	uint32_t samplesToWrite;
	uint32_t samplesWritten;
		
	samplesToEndOfBuffer = (audioBufferSampleLength - audioBufferOffset);
	samplesToWrite = (kAudioWaterlevel - Audio_Frames_Buffered);
	if (samplesToWrite > samplesToEndOfBuffer)
	    samplesToWrite = samplesToEndOfBuffer;
	
	if (deckLinkOutput->ScheduleAudioSamples((void *)((uint32_t)audio_buffer + (audioBufferOffset * audioChannelCount * audioSampleDepth/8)), samplesToWrite, 0, 0, &samplesWritten) == S_OK){
	    audioBufferOffset = ((audioBufferOffset + samplesWritten) % audioBufferSampleLength);
	}
    }


    // deckLinkOutput->GetBufferedAudioSampleFrameCount(Audio_Frames_Buffered);
    
    // 1s of audio
    FillNextAudioFrame(audio_input , audio_buffer, audioBufferSampleLength * audioChannelCount * (audioSampleDepth / 8));
    totalAudioSecondsScheduled++;

    deckLinkOutput->ScheduleAudioSamples(audio_buffer, audioSamplesPerFrame, (totalAudioSecondsScheduled * audioBufferSampleLength), audioSampleRate, NULL);

    if (preroll == true){
	
	deckLinkOutput->StartScheduledPlayback(0, 100, 1.0);
	
    }

    return S_OK;
}

BM_Play::~BM_Play () {}


